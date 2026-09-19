#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "dap/dap_protocol.h"

namespace studio {
namespace debug {

class DapClient {
public:
    using EventHandler = std::function<void(const std::string&, const ava::dap::JsonValue&)>;
    using ClosedHandler = std::function<void()>;
    using ResponseHandler = std::function<void(const ava::dap::JsonValue&)>;

    DapClient();
    DapClient(const DapClient&) = delete;
    DapClient& operator=(const DapClient&) = delete;
    ~DapClient();

    bool Connect(int port, int timeout_ms, std::string& error);
    void Disconnect();
    bool IsConnected() const { return connected_; }

    int SendRequest(const std::string& command, ava::dap::JsonValue arguments = ava::dap::JsonValue(),
                     ResponseHandler on_response = nullptr);

    void Poll();

    void SetEventHandler(EventHandler handler) { event_handler_ = std::move(handler); }
    void SetClosedHandler(ClosedHandler handler) { closed_handler_ = std::move(handler); }

    static bool Succeeded(const ava::dap::JsonValue& response);
    static std::string ErrorMessage(const ava::dap::JsonValue& response);

private:
    void ReadLoop();
    void HandleMessage(ava::dap::JsonValue message);

    std::intptr_t socket_ = -1;
    std::atomic<bool> connected_{false};

    std::thread reader_thread_;

    std::mutex queue_mutex_;
    std::vector<ava::dap::JsonValue> incoming_;
    bool closed_pending_ = false;
    bool closed_notified_ = false;

    ava::dap::SeqCounter seq_;
    std::mutex pending_mutex_;
    std::unordered_map<int, ResponseHandler> pending_;

    EventHandler event_handler_;
    ClosedHandler closed_handler_;
};

}  // namespace debug
}  // namespace studio
