#pragma once
// AvaHost.Web -- minimal blocking HTTP/1.1 server. One request per
// accepted connection, no keep-alive in v0.1 (Connection: close always
// sent). Good enough for a dev server / simple app host; a
// thread-per-connection or async model is future work once the
// single-VM-per-process constraint (see runtime/runtime_host.h) is
// revisited.
#include <functional>
#include <string>

#include "core/logger.h"
#include "web/http_types.h"
#include "web/socket.h"

namespace avahost {

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer(std::string host, int port, Logger& logger);

    // Blocks, accepting and handling connections one at a time, until
    // Stop() is called from the handler or a signal handler.
    bool Run(const RequestHandler& handler);

    void Stop();

private:
    void HandleConnection(Socket socket, const RequestHandler& handler);

    std::string host_;
    int port_;
    Logger& logger_;
    ListenSocket listenSocket_;
    bool running_ = false;
};

} // namespace avahost
