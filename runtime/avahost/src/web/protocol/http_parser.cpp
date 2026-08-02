#include "web/protocol/http_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace avahost {

namespace {

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

long HeaderContentLength(const std::string& headerBlock) {
    // Naive case-insensitive search for "content-length:" within the
    // raw header block; good enough since IsComplete only needs the
    // number, not full header parsing.
    std::string lower = headerBlock;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    size_t pos = lower.find("content-length:");
    if (pos == std::string::npos) return 0;
    size_t valueStart = pos + strlen("content-length:");
    size_t lineEnd = headerBlock.find("\r\n", valueStart);
    if (lineEnd == std::string::npos) lineEnd = headerBlock.size();
    std::string value = Trim(headerBlock.substr(valueStart, lineEnd - valueStart));
    return std::atol(value.c_str());
}

} // namespace

bool HttpParser::IsComplete(const std::string& buffer) {
    size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;

    long contentLength = HeaderContentLength(buffer.substr(0, headerEnd));
    if (contentLength <= 0) return true;

    size_t bodyStart = headerEnd + 4;
    size_t bodyBytesReceived = buffer.size() > bodyStart ? buffer.size() - bodyStart : 0;
    return static_cast<long>(bodyBytesReceived) >= contentLength;
}

bool HttpParser::Parse(const std::string& raw, HttpRequest& outRequest, std::string& outError) {
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        outError = "incomplete request (no header terminator)";
        return false;
    }

    std::string headerBlock = raw.substr(0, headerEnd);
    outRequest.body = raw.substr(headerEnd + 4);

    std::istringstream stream(headerBlock);
    std::string requestLine;
    if (!std::getline(stream, requestLine)) {
        outError = "empty request";
        return false;
    }
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    std::istringstream lineStream(requestLine);
    std::string fullPath;
    if (!(lineStream >> outRequest.method >> fullPath >> outRequest.httpVersion)) {
        outError = "malformed request line: " + requestLine;
        return false;
    }

    size_t queryPos = fullPath.find('?');
    if (queryPos != std::string::npos) {
        outRequest.path = fullPath.substr(0, queryPos);
        outRequest.query = fullPath.substr(queryPos + 1);
    } else {
        outRequest.path = fullPath;
        outRequest.query.clear();
    }

    std::string headerLine;
    while (std::getline(stream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
        if (headerLine.empty()) continue;
        size_t colon = headerLine.find(':');
        if (colon == std::string::npos) continue;
        std::string name = Trim(headerLine.substr(0, colon));
        std::string value = Trim(headerLine.substr(colon + 1));
        outRequest.headers[name] = value;
    }

    return true;
}

} // namespace avahost
