#include "dap_transport.h"

#include <cerrno>
#include <csignal>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ava {
namespace dap {

namespace {

constexpr std::intptr_t kInvalidSocket = -1;

#if defined(_WIN32)
using NativeSocket = SOCKET;
#else
using NativeSocket = int;
#endif

NativeSocket ToNative(std::intptr_t handle) {
    return static_cast<NativeSocket>(handle);
}

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

}  // namespace

TcpServer::~TcpServer() {
    Close();
}

bool TcpServer::Listen(int port, std::string& error) {
#if defined(_WIN32)
    WSADATA wsa_data;
    if (::WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        error = "WSAStartup failed";
        return false;
    }
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif

    std::intptr_t handle = static_cast<std::intptr_t>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (handle == kInvalidSocket) {
        error = LastError();
        return false;
    }

    int reuse = 1;
    ::setsockopt(ToNative(handle), SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    NativeSocket native = ToNative(handle);
    if (::bind(native, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(native, 1) != 0) {
        error = LastError();
        CloseNative(handle);
        return false;
    }

    sockaddr_in bound{};
    socklen_t bound_length = sizeof(bound);
    if (::getsockname(native, reinterpret_cast<sockaddr*>(&bound), &bound_length) == 0) {
        bound_port_ = ntohs(bound.sin_port);
    } else {
        bound_port_ = port;
    }

    listen_socket_ = handle;
    return true;
}

bool TcpServer::AcceptClient(std::string& error) {
    if (listen_socket_ == kInvalidSocket) {
        error = "not listening";
        return false;
    }
    NativeSocket native = ToNative(listen_socket_);
    std::intptr_t client = static_cast<std::intptr_t>(::accept(native, nullptr, nullptr));
    if (client == kInvalidSocket) {
        error = LastError();
        return false;
    }
    client_socket_ = client;
    CloseNative(listen_socket_);
    listen_socket_ = kInvalidSocket;
    return true;
}

long TcpServer::Receive(char* buffer, size_t capacity) {
    if (client_socket_ == kInvalidSocket) return -1;
    NativeSocket native = ToNative(client_socket_);
#if defined(_WIN32)
    return ::recv(native, buffer, static_cast<int>(capacity), 0);
#else
    return static_cast<long>(::recv(native, buffer, capacity, 0));
#endif
}

bool TcpServer::Send(const std::string& data) {
    if (client_socket_ == kInvalidSocket) return false;
    NativeSocket native = ToNative(client_socket_);
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

void TcpServer::Close() {
    if (client_socket_ != kInvalidSocket) {
        CloseNative(client_socket_);
        client_socket_ = kInvalidSocket;
    }
    if (listen_socket_ != kInvalidSocket) {
        CloseNative(listen_socket_);
        listen_socket_ = kInvalidSocket;
    }
}

}  // namespace dap
}  // namespace ava
