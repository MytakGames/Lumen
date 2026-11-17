#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/Error.h"

namespace rl::api {

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeoutMs = 10000;
    // Retries apply to network errors, 429 and 5xx, with exponential backoff.
    int maxRetries = 2;
};

struct HttpResponse {
    int status = 0;
    std::string body;
    std::map<std::string, std::string> headers;  // lower-case names
    ErrorCode error = ErrorCode::None;

    bool ok() const { return error == ErrorCode::None && status >= 200 && status < 300; }
    std::string header(const std::string& lowerName) const {
        const auto it = headers.find(lowerName);
        return it == headers.end() ? std::string{} : it->second;
    }
};

// Blocking WinHTTP client: call it from worker threads only.
// TLS certificate validation is always on (system trust store).
class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse send(const HttpRequest& request) const;

    // Maps a finished response to an ErrorCode (None for 2xx).
    static ErrorCode classify(const HttpResponse& r);

private:
    HttpResponse sendOnce(const HttpRequest& request) const;
    void* session_ = nullptr;  // HINTERNET
};

}  // namespace rl::api
