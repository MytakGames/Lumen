#pragma once

#include <optional>
#include <string>
#include <utility>

namespace rl {

// Every failure the user can see maps to one of these; the UI turns it into
// a localized message via errorKey().
enum class ErrorCode {
    None,
    Network,          // no connectivity / DNS / connection refused
    Timeout,
    Tls,              // certificate validation failed
    RateLimited,      // HTTP 429
    ApiError,         // unexpected HTTP status
    Parse,            // malformed response
    Unauthorized,     // session cookie invalid or expired
    CryptoFailed,     // DPAPI protect/unprotect failed
    RobloxNotInstalled,
    InstanceLimit,
    AccountBusy,      // account already has a running instance
    AccountDisabled,
    LaunchFailed,
    MultiInstanceUnavailable,  // Roblox already running before the mutex was taken
    LoginExpired,     // the quick-login code expired or was cancelled
    LoginChallenge,   // Roblox asked for an extra verification (captcha)
    Cancelled,
    Io,
};

const char* errorKey(ErrorCode code);  // e.g. "error.network"
const char* errorName(ErrorCode code);  // stable identifier for logs

template <class T>
struct Result {
    std::optional<T> value;
    ErrorCode error = ErrorCode::None;
    int httpStatus = 0;

    static Result ok(T v) { return Result{std::move(v), ErrorCode::None, 0}; }
    static Result fail(ErrorCode e, int status = 0) { return Result{std::nullopt, e, status}; }
    explicit operator bool() const { return value.has_value(); }
};

}  // namespace rl
