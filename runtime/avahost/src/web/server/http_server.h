#pragma once
// AvaHost.Web -- HTTP/1.1 server with configurable worker threads.
// Each connection is handled by a worker thread from the pool.
#include <functional>
#include <memory>
#include <string>

#include "core/logger.h"
#include "web/protocol/http_types.h"
#include "web/transport/socket.h"
#include "web/server/thread_pool.h"

namespace avahost {

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer(std::string host, int port, Logger& logger, int worker_threads = 0);

    // Blocks, accepting connections and dispatching to workers, until
    // Stop() is called or a signal handler triggers.
    bool Run(const RequestHandler& handler);

    void Stop();

private:
    void HandleConnection(Socket socket, const RequestHandler& handler);

    std::string host_;
    int port_;
    Logger& logger_;
    ListenSocket listenSocket_;
    std::unique_ptr<ThreadPool> pool_;
    std::function<HttpResponse(const HttpRequest&)> handler_;
    bool running_ = false;
};

} // namespace avahost
