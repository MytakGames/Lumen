#include "robloxapi/HttpClient.h"

#include <windows.h>
#include <winhttp.h>

#include <thread>

#include "core/Logger.h"
#include "core/StringUtil.h"

namespace rl::api {
namespace {

struct InternetHandle {
    HINTERNET h = nullptr;
    ~InternetHandle() {
        if (h) WinHttpCloseHandle(h);
    }
};

// Strips the query string so URLs with search terms are logged compactly.
std::string logUrl(const std::string& url) {
    const auto q = url.find('?');
    return q == std::string::npos ? url : url.substr(0, q) + "?...";
}

ErrorCode fromWinHttpError(DWORD err) {
    switch (err) {
        case ERROR_WINHTTP_TIMEOUT: return ErrorCode::Timeout;
        case ERROR_WINHTTP_SECURE_FAILURE:
        case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:
        case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:
        case ERROR_WINHTTP_SECURE_INVALID_CA:
        case ERROR_WINHTTP_SECURE_CERT_REV_FAILED:
        case ERROR_WINHTTP_SECURE_INVALID_CERT:
        case ERROR_WINHTTP_SECURE_CERT_REVOKED:
            return ErrorCode::Tls;
        case ERROR_WINHTTP_OPERATION_CANCELLED: return ErrorCode::Cancelled;
        default: return ErrorCode::Network;
    }
}

void parseHeaders(const std::wstring& raw, std::map<std::string, std::string>& out) {
    size_t start = 0;
    while (start < raw.size()) {
        size_t end = raw.find(L"\r\n", start);
        if (end == std::wstring::npos) end = raw.size();
        const std::wstring line = raw.substr(start, end - start);
        const size_t colon = line.find(L':');
        if (colon != std::wstring::npos) {
            const std::string name = str::toLower(str::toUtf8(line.substr(0, colon)));
            std::string value(str::trim(str::toUtf8(line.substr(colon + 1))));
            auto& slot = out[name];
            // Repeated headers (Set-Cookie) are kept, one per line.
            slot = slot.empty() ? std::move(value) : slot + "\n" + value;
        }
        start = end + 2;
    }
}

}  // namespace

HttpClient::HttpClient() {
    session_ = WinHttpOpen(L"Lumen/" RL_VERSION_W, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session_) {
        // Pre-8.1 fallback.
        session_ = WinHttpOpen(L"Lumen/" RL_VERSION_W, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (session_) {
        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00002000 /* TLS 1.3 */;
        if (!WinHttpSetOption(session_, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
            protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            WinHttpSetOption(session_, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
        }
        DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
        WinHttpSetOption(session_, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
    } else {
        log::error("http", "WinHttpOpen failed");
    }
}

HttpClient::~HttpClient() {
    if (session_) WinHttpCloseHandle(session_);
}

ErrorCode HttpClient::classify(const HttpResponse& r) {
    if (r.error != ErrorCode::None) return r.error;
    if (r.status >= 200 && r.status < 300) return ErrorCode::None;
    if (r.status == 401) return ErrorCode::Unauthorized;
    if (r.status == 429) return ErrorCode::RateLimited;
    return ErrorCode::ApiError;
}

HttpResponse HttpClient::send(const HttpRequest& request) const {
    HttpResponse r;
    for (int attempt = 0;; ++attempt) {
        r = sendOnce(request);
        const bool transient = r.error == ErrorCode::Network || r.error == ErrorCode::Timeout || r.status == 429 ||
                               (r.status >= 500 && r.status <= 599);
        if (!transient || attempt >= request.maxRetries) break;
        const int delayMs = 400 * (1 << attempt);  // 400, 800, 1600 ...
        log::warn("http", "Retry " + std::to_string(attempt + 1) + " for " + logUrl(request.url) + " in " +
                              std::to_string(delayMs) + " ms");
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return r;
}

HttpResponse HttpClient::sendOnce(const HttpRequest& request) const {
    HttpResponse r;
    if (!session_) {
        r.error = ErrorCode::Network;
        return r;
    }

    const std::wstring wurl = str::toWide(request.url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = static_cast<DWORD>(-1);
    uc.dwUrlPathLength = static_cast<DWORD>(-1);
    uc.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        r.error = ErrorCode::Network;
        return r;
    }
    const std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    const bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;

    InternetHandle connect{WinHttpConnect(session_, host.c_str(), uc.nPort, 0)};
    if (!connect.h) {
        r.error = fromWinHttpError(GetLastError());
        return r;
    }
    const std::wstring method = str::toWide(request.method);
    InternetHandle req{WinHttpOpenRequest(connect.h, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0)};
    if (!req.h) {
        r.error = fromWinHttpError(GetLastError());
        return r;
    }
    const int t = request.timeoutMs;
    WinHttpSetTimeouts(req.h, t, t, t, t);
    // No shared cookie jar: every request carries exactly the session it is
    // given, so accounts can never leak into each other's requests.
    DWORD disable = WINHTTP_DISABLE_COOKIES;
    WinHttpSetOption(req.h, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));

    std::wstring headers;
    for (const auto& [name, value] : request.headers) headers += str::toWide(name + ": " + value + "\r\n");
    if (!request.body.empty() && headers.find(L"Content-Type") == std::wstring::npos) {
        headers += L"Content-Type: application/json\r\n";
    }
    headers += L"Accept: application/json, image/*;q=0.9, */*;q=0.5\r\n";

    const BOOL sent = WinHttpSendRequest(
        req.h, headers.c_str(), static_cast<DWORD>(-1L),
        request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data()),
        static_cast<DWORD>(request.body.size()), static_cast<DWORD>(request.body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(req.h, nullptr)) {
        r.error = fromWinHttpError(GetLastError());
        log::warn("http", request.method + " " + logUrl(request.url) + " failed: " + errorName(r.error));
        return r;
    }

    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    r.status = static_cast<int>(status);

    DWORD headerBytes = 0;
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &headerBytes,
                        WINHTTP_NO_HEADER_INDEX);
    if (headerBytes > 0) {
        std::wstring raw(headerBytes / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(),
                                &headerBytes, WINHTTP_NO_HEADER_INDEX)) {
            raw.resize(headerBytes / sizeof(wchar_t));
            parseHeaders(raw, r.headers);
        }
    }

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req.h, &available)) {
            r.error = fromWinHttpError(GetLastError());
            return r;
        }
        if (available == 0) break;
        const size_t offset = r.body.size();
        r.body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(req.h, r.body.data() + offset, available, &read)) {
            r.error = fromWinHttpError(GetLastError());
            return r;
        }
        r.body.resize(offset + read);
    }
    log::debug("http", request.method + " " + logUrl(request.url) + " -> " + std::to_string(r.status));
    return r;
}

}  // namespace rl::api
