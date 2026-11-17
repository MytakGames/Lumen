#include "core/Error.h"

namespace rl {

const char* errorName(ErrorCode code) {
    switch (code) {
        case ErrorCode::None: return "none";
        case ErrorCode::Network: return "network";
        case ErrorCode::Timeout: return "timeout";
        case ErrorCode::Tls: return "tls";
        case ErrorCode::RateLimited: return "rate_limited";
        case ErrorCode::ApiError: return "api";
        case ErrorCode::Parse: return "parse";
        case ErrorCode::Unauthorized: return "unauthorized";
        case ErrorCode::CryptoFailed: return "crypto";
        case ErrorCode::RobloxNotInstalled: return "not_installed";
        case ErrorCode::InstanceLimit: return "instance_limit";
        case ErrorCode::AccountBusy: return "account_busy";
        case ErrorCode::AccountDisabled: return "account_disabled";
        case ErrorCode::LaunchFailed: return "launch_failed";
        case ErrorCode::MultiInstanceUnavailable: return "multi_unavailable";
        case ErrorCode::LoginExpired: return "login_expired";
        case ErrorCode::LoginChallenge: return "login_challenge";
        case ErrorCode::Cancelled: return "cancelled";
        case ErrorCode::Io: return "io";
    }
    return "unknown";
}

const char* errorKey(ErrorCode code) {
    switch (code) {
        case ErrorCode::None: return "error.none";
        case ErrorCode::Network: return "error.network";
        case ErrorCode::Timeout: return "error.timeout";
        case ErrorCode::Tls: return "error.tls";
        case ErrorCode::RateLimited: return "error.rate_limited";
        case ErrorCode::ApiError: return "error.api";
        case ErrorCode::Parse: return "error.parse";
        case ErrorCode::Unauthorized: return "error.unauthorized";
        case ErrorCode::CryptoFailed: return "error.crypto";
        case ErrorCode::RobloxNotInstalled: return "error.not_installed";
        case ErrorCode::InstanceLimit: return "error.instance_limit";
        case ErrorCode::AccountBusy: return "error.account_busy";
        case ErrorCode::AccountDisabled: return "error.account_disabled";
        case ErrorCode::LaunchFailed: return "error.launch_failed";
        case ErrorCode::MultiInstanceUnavailable: return "error.multi_unavailable";
        case ErrorCode::LoginExpired: return "error.login_expired";
        case ErrorCode::LoginChallenge: return "error.login_challenge";
        case ErrorCode::Cancelled: return "error.cancelled";
        case ErrorCode::Io: return "error.io";
    }
    return "error.api";
}

}  // namespace rl
