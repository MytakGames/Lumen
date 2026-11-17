#include "robloxapi/RobloxApi.h"

#include <windows.h>

#include <algorithm>
#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/StringUtil.h"
#include "robloxapi/Parsers.h"

namespace rl::api {
namespace {

constexpr size_t kBatch = 50;

template <class T>
Result<T> failFrom(const HttpResponse& r) {
    return Result<T>::fail(HttpClient::classify(r), r.status);
}

std::vector<std::vector<std::int64_t>> batches(const std::vector<std::int64_t>& ids) {
    std::vector<std::vector<std::int64_t>> out;
    for (size_t i = 0; i < ids.size(); i += kBatch) {
        out.emplace_back(ids.begin() + static_cast<std::ptrdiff_t>(i),
                         ids.begin() + static_cast<std::ptrdiff_t>(std::min(ids.size(), i + kBatch)));
    }
    return out;
}

}  // namespace

std::string joinIds(const std::vector<std::int64_t>& ids) {
    std::string s;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) s += ',';
        s += std::to_string(ids[i]);
    }
    return s;
}

// --- ResponseCache -------------------------------------------------------------

bool ResponseCache::get(const std::string& key, std::string& out) {
    std::lock_guard lock(mutex_);
    const auto it = entries_.find(key);
    if (it == entries_.end()) return false;
    if (std::chrono::steady_clock::now() - it->second.stored > std::chrono::seconds(ttlSeconds_.load())) {
        entries_.erase(it);
        return false;
    }
    out = it->second.body;
    return true;
}

void ResponseCache::put(const std::string& key, std::string body) {
    std::lock_guard lock(mutex_);
    if (entries_.size() > 2000) entries_.clear();  // crude bound; entries are small and short-lived
    entries_[key] = {std::chrono::steady_clock::now(), std::move(body)};
}

void ResponseCache::clear() {
    std::lock_guard lock(mutex_);
    entries_.clear();
}

// --- RobloxApi -------------------------------------------------------------------

RobloxApi::RobloxApi() : sessionId_(str::uuid4()) {}

HttpResponse RobloxApi::getCached(const std::string& url, bool useCache) {
    HttpResponse r;
    if (useCache && cache_.get(url, r.body)) {
        r.status = 200;
        return r;
    }
    HttpRequest req;
    req.url = url;
    r = http_.send(req);
    if (r.ok() && useCache) cache_.put(url, r.body);
    return r;
}

HttpResponse RobloxApi::sendWithCookie(HttpRequest req, const SecureString& cookie) {
    req.headers.emplace_back("Cookie", ".ROBLOSECURITY=" + cookie.reveal());
    HttpResponse r = http_.send(req);
    // Wipe our copy of the secret from the request headers.
    for (auto& [name, value] : req.headers) {
        if (!value.empty()) SecureZeroMemory(value.data(), value.size());
    }
    return r;
}

Result<AuthenticatedUser> RobloxApi::authenticatedUser(const SecureString& cookie) {
    HttpRequest req;
    req.url = "https://users.roblox.com/v1/users/authenticated";
    const HttpResponse r = sendWithCookie(req, cookie);
    if (!r.ok()) return failFrom<AuthenticatedUser>(r);
    auto user = parse::authenticatedUser(r.body);
    if (!user) return Result<AuthenticatedUser>::fail(ErrorCode::Parse, r.status);
    return Result<AuthenticatedUser>::ok(std::move(*user));
}

Result<SecureString> RobloxApi::authTicket(const SecureString& cookie, std::int64_t placeId) {
    HttpRequest req;
    req.method = "POST";
    req.url = "https://auth.roblox.com/v1/authentication-ticket";
    req.body = "{}";
    req.maxRetries = 0;
    req.headers = {{"Referer", "https://www.roblox.com/games/" + std::to_string(placeId) + "/"},
                   {"Origin", "https://www.roblox.com"},
                   {"RBXAuthenticationNegotiation", "1"}};

    // First call is expected to fail with 403 + a CSRF token (Roblox's XSRF protocol).
    HttpResponse r = sendWithCookie(req, cookie);
    const std::string csrf = r.header("x-csrf-token");
    if (r.status == 403 && !csrf.empty()) {
        req.headers.emplace_back("x-csrf-token", csrf);
        r = sendWithCookie(req, cookie);
    }
    if (!r.ok()) return failFrom<SecureString>(r);
    std::string ticket = r.header("rbx-authentication-ticket");
    if (ticket.empty()) return Result<SecureString>::fail(ErrorCode::Parse, r.status);
    return Result<SecureString>::ok(SecureString(std::move(ticket)));
}

HttpResponse RobloxApi::postWithCsrf(HttpRequest req) {
    req.method = "POST";
    req.maxRetries = 0;
    req.headers.emplace_back("Origin", "https://www.roblox.com");
    req.headers.emplace_back("Referer", "https://www.roblox.com/");
    HttpResponse r = http_.send(req);
    const std::string csrf = r.header("x-csrf-token");
    if (r.status == 403 && !csrf.empty()) {
        req.headers.emplace_back("x-csrf-token", csrf);
        r = http_.send(req);
    }
    return r;
}

Result<QuickLoginCode> RobloxApi::quickLoginCreate() {
    HttpRequest req;
    req.url = "https://apis.roblox.com/auth-token-service/v1/login/create";
    req.body = "{}";
    const HttpResponse r = postWithCsrf(req);
    if (!r.ok()) {
        log::warn("login", "Quick login code request failed: HTTP " + std::to_string(r.status));
        return failFrom<QuickLoginCode>(r);
    }
    auto code = parse::quickLoginCreate(r.body);
    if (!code) return Result<QuickLoginCode>::fail(ErrorCode::Parse, r.status);
    return Result<QuickLoginCode>::ok(std::move(*code));
}

Result<QuickLoginState> RobloxApi::quickLoginStatus(const QuickLoginCode& code) {
    HttpRequest req;
    req.url = "https://apis.roblox.com/auth-token-service/v1/login/status";
    req.body = nlohmann::json{{"code", code.code}, {"privateKey", code.privateKey}}.dump();
    const HttpResponse r = postWithCsrf(req);
    if (r.status == 400 || r.status == 404) {
        // Unknown or expired code.
        QuickLoginState s;
        s.status = QuickLoginStatus::Invalid;
        return Result<QuickLoginState>::ok(s);
    }
    if (!r.ok()) return failFrom<QuickLoginState>(r);
    auto st = parse::quickLoginStatus(r.body);
    if (!st) return Result<QuickLoginState>::fail(ErrorCode::Parse, r.status);
    return Result<QuickLoginState>::ok(std::move(*st));
}

Result<SecureString> RobloxApi::quickLoginComplete(const QuickLoginCode& code) {
    HttpRequest req;
    req.url = "https://auth.roblox.com/v2/login";
    req.body = nlohmann::json{{"ctype", "AuthToken"}, {"cvalue", code.code}, {"password", code.privateKey}}.dump();
    HttpResponse r = postWithCsrf(req);
    SecureZeroMemory(req.body.data(), req.body.size());
    if (!r.ok()) {
        log::warn("login", "Quick login completion failed: HTTP " + std::to_string(r.status));
        if (r.status == 403 && parse::isChallenge(r.body)) return Result<SecureString>::fail(ErrorCode::LoginChallenge, 403);
        return failFrom<SecureString>(r);
    }
    std::string cookie = parse::sessionCookie(r.header("set-cookie"));
    if (r.headers.count("set-cookie")) {
        auto& raw = r.headers["set-cookie"];
        SecureZeroMemory(raw.data(), raw.size());
    }
    if (cookie.empty()) return Result<SecureString>::fail(ErrorCode::Parse, r.status);
    return Result<SecureString>::ok(SecureString(std::move(cookie)));
}

Result<std::vector<GameInfo>> RobloxApi::search(const std::string& query) {
    const std::string url = "https://apis.roblox.com/search-api/omni-search?searchQuery=" + str::urlEncode(query) +
                            "&sessionId=" + sessionId_ + "&pageType=all";
    const HttpResponse r = getCached(url);
    if (!r.ok()) return failFrom<std::vector<GameInfo>>(r);
    auto games = parse::omniSearch(r.body);
    if (!games) return Result<std::vector<GameInfo>>::fail(ErrorCode::Parse, r.status);
    return Result<std::vector<GameInfo>>::ok(std::move(*games));
}

Result<std::vector<GameInfo>> RobloxApi::details(const std::vector<std::int64_t>& universeIds) {
    std::vector<GameInfo> all;
    for (const auto& batch : batches(universeIds)) {
        const std::string ids = joinIds(batch);
        const HttpResponse r = getCached("https://games.roblox.com/v1/games?universeIds=" + ids);
        if (!r.ok()) return failFrom<std::vector<GameInfo>>(r);
        auto games = parse::gameDetails(r.body);
        if (!games) return Result<std::vector<GameInfo>>::fail(ErrorCode::Parse, r.status);

        // Votes are best-effort: details stay useful without them.
        const HttpResponse v = getCached("https://games.roblox.com/v1/games/votes?universeIds=" + ids);
        if (v.ok()) {
            if (const auto votes = parse::votes(v.body)) {
                for (auto& g : *games) {
                    const auto it = votes->find(g.universeId);
                    if (it != votes->end()) {
                        g.upVotes = it->second.first;
                        g.downVotes = it->second.second;
                    }
                }
            }
        }
        all.insert(all.end(), games->begin(), games->end());
    }
    return Result<std::vector<GameInfo>>::ok(std::move(all));
}

Result<std::vector<GameInfo>> RobloxApi::popular() {
    const HttpResponse r = getCached("https://apis.roblox.com/explore-api/v1/get-sorts?sessionId=" + sessionId_);
    if (!r.ok()) return failFrom<std::vector<GameInfo>>(r);
    auto games = parse::exploreSorts(r.body);
    if (!games) return Result<std::vector<GameInfo>>::fail(ErrorCode::Parse, r.status);
    return Result<std::vector<GameInfo>>::ok(std::move(*games));
}

Result<std::int64_t> RobloxApi::universeForPlace(std::int64_t placeId) {
    const HttpResponse r =
        getCached("https://apis.roblox.com/universes/v1/places/" + std::to_string(placeId) + "/universe");
    if (!r.ok()) return failFrom<std::int64_t>(r);
    const auto id = parse::universeId(r.body);
    if (!id) return Result<std::int64_t>::fail(ErrorCode::Parse, r.status);
    return Result<std::int64_t>::ok(*id);
}

namespace {
template <class Fn>
Result<std::map<std::int64_t, std::string>> collectThumbs(const std::vector<std::int64_t>& ids, Fn&& fetch) {
    std::map<std::int64_t, std::string> out;
    for (const auto& batch : batches(ids)) {
        auto part = fetch(batch);
        if (!part) return part;
        out.insert(part.value->begin(), part.value->end());
    }
    return Result<std::map<std::int64_t, std::string>>::ok(std::move(out));
}
}  // namespace

Result<std::map<std::int64_t, std::string>> RobloxApi::gameIcons(const std::vector<std::int64_t>& universeIds) {
    return collectThumbs(universeIds, [this](const std::vector<std::int64_t>& b) {
        const HttpResponse r = getCached("https://thumbnails.roblox.com/v1/games/icons?universeIds=" + joinIds(b) +
                                         "&returnPolicy=PlaceHolder&size=256x256&format=Png&isCircular=false");
        if (!r.ok()) return failFrom<std::map<std::int64_t, std::string>>(r);
        auto m = parse::thumbnails(r.body);
        if (!m) return Result<std::map<std::int64_t, std::string>>::fail(ErrorCode::Parse);
        return Result<std::map<std::int64_t, std::string>>::ok(std::move(*m));
    });
}

Result<std::map<std::int64_t, std::string>> RobloxApi::gameBanners(const std::vector<std::int64_t>& universeIds) {
    return collectThumbs(universeIds, [this](const std::vector<std::int64_t>& b) {
        const HttpResponse r =
            getCached("https://thumbnails.roblox.com/v1/games/multiget/thumbnails?universeIds=" + joinIds(b) +
                      "&countPerUniverse=1&defaults=true&size=768x432&format=Png&isCircular=false");
        if (!r.ok()) return failFrom<std::map<std::int64_t, std::string>>(r);
        auto m = parse::multigetThumbnails(r.body);
        if (!m) return Result<std::map<std::int64_t, std::string>>::fail(ErrorCode::Parse);
        return Result<std::map<std::int64_t, std::string>>::ok(std::move(*m));
    });
}

Result<std::map<std::int64_t, std::string>> RobloxApi::userHeadshots(const std::vector<std::int64_t>& userIds) {
    return collectThumbs(userIds, [this](const std::vector<std::int64_t>& b) {
        const HttpResponse r = getCached("https://thumbnails.roblox.com/v1/users/avatar-headshot?userIds=" +
                                         joinIds(b) + "&size=150x150&format=Png&isCircular=false");
        if (!r.ok()) return failFrom<std::map<std::int64_t, std::string>>(r);
        auto m = parse::thumbnails(r.body);
        if (!m) return Result<std::map<std::int64_t, std::string>>::fail(ErrorCode::Parse);
        return Result<std::map<std::int64_t, std::string>>::ok(std::move(*m));
    });
}

Result<std::string> RobloxApi::download(const std::string& url) {
    HttpRequest req;
    req.url = url;
    req.timeoutMs = 15000;
    HttpResponse r = http_.send(req);
    if (!r.ok()) return failFrom<std::string>(r);
    return Result<std::string>::ok(std::move(r.body));
}

}  // namespace rl::api
