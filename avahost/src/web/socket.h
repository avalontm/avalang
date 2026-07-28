#pragma once
// Cross-platform TCP socket wrapper (Winsock on Windows, BSD sockets on
// Linux) so nothing above this file needs #ifdef _WIN32. Blocking I/O
// only -- matches the simple single-request-at-a-time http_server.
#include <cstdint>
#include <string>

namespace avahost {

// Must be called once before any Socket use, and matched by
// SocketPlatform::Shutdown() at process exit. No-op on Linux; calls
// WSAStartup/WSACleanup on Windows.
class SocketPlatform {
public:
    static bool Init();
    static void Shutdown();
};

class Socket {
public:
    Socket() = default;
    explicit Socket(intptr_t nativeHandle) : handle_(nativeHandle) {}
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    bool IsValid() const;
    void Close();

    // Returns -1 on error, 0 on peer closed, else bytes read.
    int Receive(char* buffer, int bufferSize) const;
    // Returns false on any send error (short writes are retried internally).
    bool SendAll(const char* data, size_t size) const;

    intptr_t NativeHandle() const { return handle_; }

private:
    intptr_t handle_ = -1;
};

// Passive (listening) socket bound to host:port.
class ListenSocket {
public:
    ListenSocket() = default;
    ~ListenSocket();

    ListenSocket(const ListenSocket&) = delete;
    ListenSocket& operator=(const ListenSocket&) = delete;

    // Binds + listens. Returns false and fills outError on failure
    // (address in use, permission denied, etc).
    bool Listen(const std::string& host, int port, int backlog, std::string& outError);

    // Blocks until a client connects (or the socket is closed from
    // another thread, which makes this return an invalid Socket).
    Socket Accept() const;

    void Close();
    bool IsValid() const;

private:
    intptr_t handle_ = -1;
};

} // namespace avahost
