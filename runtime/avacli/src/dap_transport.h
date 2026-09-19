#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ava {
namespace dap {

class TcpServer {
public:
    TcpServer() = default;
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    ~TcpServer();

    bool Listen(int port, std::string& error);
    int BoundPort() const { return bound_port_; }
    bool AcceptClient(std::string& error);
    long Receive(char* buffer, size_t capacity);
    bool Send(const std::string& data);
    void Close();

private:
    std::intptr_t listen_socket_ = -1;
    std::intptr_t client_socket_ = -1;
    int bound_port_ = 0;
};

}  // namespace dap
}  // namespace ava
