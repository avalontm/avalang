#include "web/transport/socket.h"

#include <cstring>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socklen_t_compat = int;
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using socklen_t_compat = socklen_t;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR (-1)
  static void closesocket(int s) { close(s); }
#endif

namespace avahost {

// ---------------------------------------------------------------------
// SocketPlatform
// ---------------------------------------------------------------------

bool SocketPlatform::Init() {
#if defined(_WIN32)
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

void SocketPlatform::Shutdown() {
#if defined(_WIN32)
    WSACleanup();
#endif
}

// ---------------------------------------------------------------------
// Socket
// ---------------------------------------------------------------------

Socket::~Socket() { Close(); }

Socket::Socket(Socket&& other) noexcept : handle_(other.handle_) {
    other.handle_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this == &other) return *this;
    Close();
    handle_ = other.handle_;
    other.handle_ = -1;
    return *this;
}

bool Socket::IsValid() const { return handle_ != -1; }

void Socket::Close() {
    if (handle_ != -1) {
        closesocket(static_cast<int>(handle_));
        handle_ = -1;
    }
}

int Socket::Receive(char* buffer, int bufferSize) const {
    if (handle_ == -1) return -1;
    return static_cast<int>(recv(static_cast<int>(handle_), buffer, bufferSize, 0));
}

bool Socket::SendAll(const char* data, size_t size) const {
    if (handle_ == -1) return false;
    size_t sent = 0;
    while (sent < size) {
        int n = static_cast<int>(send(static_cast<int>(handle_), data + sent,
                                       static_cast<int>(size - sent), 0));
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

// ---------------------------------------------------------------------
// ListenSocket
// ---------------------------------------------------------------------

ListenSocket::~ListenSocket() { Close(); }

bool ListenSocket::Listen(const std::string& host, int port, int backlog, std::string& outError) {
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    const char* node = (host.empty() || host == "*") ? nullptr : host.c_str();

    int rc = getaddrinfo(node, portStr.c_str(), &hints, &result);
    if (rc != 0 || !result) {
        outError = "getaddrinfo failed for " + host + ":" + portStr;
        return false;
    }

    intptr_t sock = -1;
    for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == -1) continue;

        int reuse = 1;
        setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        if (bind(static_cast<int>(sock), p->ai_addr, static_cast<socklen_t_compat>(p->ai_addrlen)) == 0) {
            break;
        }
        closesocket(static_cast<int>(sock));
        sock = -1;
    }
    freeaddrinfo(result);

    if (sock == -1) {
        outError = "bind failed on " + host + ":" + portStr + " (port already in use?)";
        return false;
    }

    if (listen(static_cast<int>(sock), backlog) == SOCKET_ERROR) {
        closesocket(static_cast<int>(sock));
        outError = "listen failed on " + host + ":" + portStr;
        return false;
    }

    handle_ = sock;
    return true;
}

Socket ListenSocket::Accept() const {
    if (handle_ == -1) return Socket();
    struct sockaddr_storage clientAddr{};
    socklen_t_compat len = sizeof(clientAddr);
    intptr_t client = accept(static_cast<int>(handle_),
                              reinterpret_cast<struct sockaddr*>(&clientAddr), &len);
    if (client == -1) return Socket();
    return Socket(client);
}

void ListenSocket::Close() {
    if (handle_ != -1) {
        closesocket(static_cast<int>(handle_));
        handle_ = -1;
    }
}

bool ListenSocket::IsValid() const { return handle_ != -1; }

} // namespace avahost
