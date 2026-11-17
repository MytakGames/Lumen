#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Error.h"
#include "core/SecureString.h"
#include "robloxapi/HttpClient.h"
#include "robloxapi/Types.h"

namespace rl::api {

// Thread-safe in-memory cache of GET response bodies with a configurable TTL.
class ResponseCache {
public:
    void setTtl(std::chrono::seconds ttl) { ttlSeconds_ = ttl.count(); }
    bool get(const std::string& key, std::string& out);
    void put(const std::string& key, std::string body);
    void clear();

private:
    struct Entry {
        std::chrono::steady_clock::time_point stored;
        std::string body;
    };
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
    std::atomic<long long> ttlSeconds_{600};
};

// Typed access to the public Roblox web endpoints used by the launcher.
// All methods block: call them from worker threads.
class RobloxApi {
public:
    RobloxApi();

    ResponseCache& cache() { return cache_; }

    // --- authenticated (cookie) -------------------------------------------
    Result<AuthenticatedUser> authenticatedUser(const SecureString& cookie);
    // One-time ticket used by roblox-player: to join as this account.
    Result<SecureString> authTicket(const SecureString& cookie, std::int64_t placeId);

    // --- "log in with another device" (user confirms in their own browser) ---
    Result<QuickLoginCode> quickLoginCreate();
    Result<QuickLoginState> quickLoginStatus(const QuickLoginCode& code);
    // Exchanges a validated code for the session cookie.
    Result<SecureString> quickLoginComplete(const QuickLoginCode& code);

    // --- public ------------------------------------------------------------
    Result<std::vector<GameInfo>> search(const std::string& query);
    Result<std::vector<GameInfo>> details(const std::vector<std::int64_t>& universeIds);  // + votes
    Result<std::vector<GameInfo>> popular();
    Result<std::int64_t> universeForPlace(std::int64_t placeId);
    Result<std::map<std::int64_t, std::string>> gameIcons(const std::vector<std::int64_t>& universeIds);
    Result<std::map<std::int64_t, std::string>> gameBanners(const std::vector<std::int64_t>& universeIds);
    Result<std::map<std::int64_t, std::string>> userHeadshots(const std::vector<std::int64_t>& userIds);
    // Raw bytes (images). Not cached here: ThumbnailCache owns image caching.
    Result<std::string> download(const std::string& url);

private:
    HttpResponse getCached(const std::string& url, bool useCache = true);
    HttpResponse sendWithCookie(HttpRequest req, const SecureString& cookie);
    // POST honouring Roblox's XSRF protocol (403 + x-csrf-token, then retry).
    HttpResponse postWithCsrf(HttpRequest req);

    HttpClient http_;
    ResponseCache cache_;
    std::string sessionId_;
};

std::string joinIds(const std::vector<std::int64_t>& ids);

}  // namespace rl::api
