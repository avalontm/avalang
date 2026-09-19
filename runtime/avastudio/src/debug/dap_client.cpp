#include "debug/dap_client.h"

#include <chrono>
#include <cstring>
#include <optional>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace studio {
namespace debug {

using ava::dap::JsonValue;

namespace {

constexpr std::intptr_t kInvalidSocket = -1;

#if defined(_WIN32)
using NativeSocket = SOCKET;
#else
using NativeSocket = int;
#endif

NativeSocket ToNative(std::intptr_t handle) { return static_cast<NativeSocket>(handle); }

void CloseNative(std::intptr_t handle) {
#if defined(_WIN32)
    ::shutdown(ToNative(handle), SD_BOTH);
    ::closesocket(ToNative(handle));
#else
    ::shutdown(ToNative(handle), SHUT_RDWR);
    ::close(ToNative(handle));
#endif
}

std::string LastError() {
#if defined(_WIN32)
    return "winsock error " + std::to_string(::WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

std::intptr_t ConnectOnce(int port, std::string& error) {
#if defined(_WIN32)
    WSADATA wsa_data;
    if (::WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        error = "WSAStartup failed";
        return kInvalidSocket;
    }
#endif

    std::intptr_t handle = static_cast<std::intptr_t>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (handle == kInvalidSocket) {
        error = LastError();
        return kInvalidSocket;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    NativeSocket native = ToNative(handle);
    if (::connect(native, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = LastError();
        CloseNative(handle);
        return kInvalidSocket;
    }
    return handle;
}

long ReceiveFrom(std::intptr_t handle, char* buffer, size_t capacity) {
    NativeSocket native = ToNative(handle);
#if defined(_WIN32)
    return ::recv(native, buffer, static_cast<int>(capacity), 0);
#else
    return static_cast<long>(::recv(native, buffer, capacity, 0));
#endif
}

bool SendAll(std::intptr_t handle, const std::string& data) {
    NativeSocket native = ToNative(handle);
    size_t sent = 0;
    while (sent < data.size()) {
#if defined(_WIN32)
        int written = ::send(native, data.data() + sent, static_cast<int>(data.size() - sent), 0);
#elif defined(MSG_NOSIGNAL)
        long written = static_cast<long>(::send(native, data.data() + sent, data.size() - sent, MSG_NOSIGNAL));
#else
        long written = static_cast<long>(::send(native, data.data() + sent, data.size() - sent, 0));
#endif
        if (written <= 0) return false;
        sent += static_cast<size_t>(written);
    }
    return true;
}

}  // namespace

DapClient::DapClient() = default;

DapClient::~DapClient() {
    Disconnect();
}

bool DapClient::Connect(int port, int timeout_ms, std::string& error) {
    if (connected_) {
        error = "already connected";
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::string last_error;
    std::intptr_t handle = kInvalidSocket;
    while (true) {
        handle = ConnectOnce(port, last_error);
        if (handle != kInvalidSocket) break;
        if (std::chrono::steady_clock::now() >= deadline) {
            error = last_error.empty() ? "connection timed out" : last_error;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    socket_ = handle;
    closed_notified_ = false;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        incoming_.clear();
        closed_pending_ = false;
    }
    connected_ = true;
    reader_thread_ = std::thread([this] { ReadLoop(); });
    return true;
}

void DapClient::Disconnect() {
    if (socket_ != kInvalidSocket) {
        CloseNative(socket_);
        socket_ = kInvalidSocket;
    }
    if (reader_thread_.joinable()) reader_thread_.join();
    connected_ = false;

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        incoming_.clear();
        closed_pending_ = false;
    }
}

void DapClient::ReadLoop() {
    std::string buffer;
    char chunk[4096];
    while (true) {
        long received = ReceiveFrom(socket_, chunk, sizeof(chunk));
        if (received <= 0) break;
        buffer.append(chunk, static_cast<size_t>(received));

        while (true) {
            std::optional<JsonValue> message;
            try {
                message = ava::dap::TryExtractMessage(buffer);
            } catch (const ava::dap::JsonParseError&) {
                break;
            }
            if (!message) break;
            std::lock_guard<std::mutex> lock(queue_mutex_);
            incoming_.push_back(std::move(*message));
        }
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    closed_pending_ = true;
}

int DapClient::SendRequest(const std::string& command, JsonValue arguments, ResponseHandler on_response) {
    if (!connected_) return 0;

    JsonValue request = ava::dap::MakeRequest(seq_, command, std::move(arguments));
    int seq = static_cast<int>(request.get("seq").as_int());

    if (on_response) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_[seq] = std::move(on_response);
    }

    if (!SendAll(socket_, ava::dap::EncodeMessage(request))) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_.erase(seq);
    }
    return seq;
}

void DapClient::Poll() {
    std::vector<JsonValue> batch;
    bool closed = false;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        batch.swap(incoming_);
        if (closed_pending_) {
            closed = true;
            closed_pending_ = false;
        }
    }

    for (JsonValue& message : batch) HandleMessage(std::move(message));

    if (closed && !closed_notified_) {
        closed_notified_ = true;
        connected_ = false;
        if (closed_handler_) closed_handler_();
    }
}

void DapClient::HandleMessage(JsonValue message) {
    if (ava::dap::IsResponse(message)) {
        int request_seq = static_cast<int>(message.get("request_seq").as_int());
        ResponseHandler handler;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_.find(request_seq);
            if (it != pending_.end()) {
                handler = std::move(it->second);
                pending_.erase(it);
            }
        }
        if (handler) handler(message);
    } else if (ava::dap::IsEvent(message)) {
        if (event_handler_) event_handler_(message.get("event").as_string(), message.get("body"));
    }
}

bool DapClient::Succeeded(const JsonValue& response) {
    return response.get("success").as_bool(false);
}

std::string DapClient::ErrorMessage(const JsonValue& response) {
    std::string message = response.get("message").as_string();
    return message.empty() ? "unknown error" : message;
}

}  // namespace debug
}  // namespace studio
