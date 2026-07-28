#include "web/server/http_server.h"

#include "web/protocol/http_parser.h"

namespace avahost {

namespace {
constexpr int kReadChunkSize = 8192;
constexpr int kListenBacklog = 32;

std::string StatusLine(const HttpResponse& response) {
    return "HTTP/1.1 " + std::to_string(response.statusCode) + " " + response.statusText + "\r\n";
}

const char* DefaultStatusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}
} // namespace

HttpServer::HttpServer(std::string host, int port, Logger& logger)
    : host_(std::move(host)), port_(port), logger_(logger) {}

bool HttpServer::Run(const RequestHandler& handler) {
    if (!SocketPlatform::Init()) {
        logger_.Error("failed to initialize socket subsystem");
        return false;
    }

    std::string error;
    if (!listenSocket_.Listen(host_, port_, kListenBacklog, error)) {
        logger_.Error("HttpServer::Run: " + error);
        SocketPlatform::Shutdown();
        return false;
    }

    running_ = true;
    logger_.Info("listening on http://" + (host_.empty() || host_ == "*" ? std::string("localhost") : host_) +
                 ":" + std::to_string(port_));

    while (running_) {
        Socket client = listenSocket_.Accept();
        if (!client.IsValid()) {
            if (!running_) break;
            continue;
        }
        HandleConnection(std::move(client), handler);
    }

    listenSocket_.Close();
    SocketPlatform::Shutdown();
    return true;
}

void HttpServer::Stop() {
    running_ = false;
    listenSocket_.Close();
}

void HttpServer::HandleConnection(Socket socket, const RequestHandler& handler) {
    std::string buffer;
    char chunk[kReadChunkSize];

    // Read until we have a complete request (headers + declared body).
    while (!HttpParser::IsComplete(buffer)) {
        int n = socket.Receive(chunk, kReadChunkSize);
        if (n <= 0) {
            if (buffer.empty()) return; // peer closed before sending anything
            break;
        }
        buffer.append(chunk, n);
    }

    HttpRequest request;
    std::string parseError;
    HttpResponse response;

    if (!HttpParser::Parse(buffer, request, parseError)) {
        response = HttpResponse::Text(400, "400 Bad Request: " + parseError);
    } else {
        try {
            response = handler(request);
        } catch (const std::exception& ex) {
            logger_.Error(std::string("unhandled exception in request handler: ") + ex.what());
            response = HttpResponse::ServerError("500 Internal Server Error");
        } catch (...) {
            logger_.Error("unhandled non-std exception in request handler");
            response = HttpResponse::ServerError("500 Internal Server Error");
        }
        if (!response.skipAccessLog) {
            logger_.Info(request.method + " " + request.path + " -> " + std::to_string(response.statusCode));
        }
    }

    if (response.statusText.empty() || (response.statusText == "OK" && response.statusCode != 200)) {
        response.statusText = DefaultStatusText(response.statusCode);
    }
    if (response.headers.find("Content-Length") == response.headers.end()) {
        response.SetHeader("Content-Length", std::to_string(response.body.size()));
    }
    response.SetHeader("Connection", "close");
    response.SetHeader("Server", "AvaHost");

    std::string out = StatusLine(response);
    for (const auto& [name, value] : response.headers) {
        out += name + ": " + value + "\r\n";
    }
    out += "\r\n";
    out += response.body;

    socket.SendAll(out.data(), out.size());
}

} // namespace avahost
