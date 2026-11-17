#include "robloxapi/Parsers.h"

#include <cstdio>
#include <nlohmann/json.hpp>
#include <set>

namespace rl::api {

void GameInfo::mergeFrom(const GameInfo& o) {
    if (o.universeId) universeId = o.universeId;
    if (o.rootPlaceId) rootPlaceId = o.rootPlaceId;
    if (!o.name.empty()) name = o.name;
    if (!o.description.empty()) description = o.description;
    if (!o.creatorName.empty()) creatorName = o.creatorName;
    if (!o.creatorType.empty()) creatorType = o.creatorType;
    if (o.creatorId) creatorId = o.creatorId;
    if (!o.genre.empty()) genre = o.genre;
    if (o.playing || o.hasDetails) playing = o.playing;
    if (o.visits) visits = o.visits;
    if (o.upVotes || o.downVotes) {
        upVotes = o.upVotes;
        downVotes = o.downVotes;
    }
    if (o.favorites) favorites = o.favorites;
    if (o.maxPlayers) maxPlayers = o.maxPlayers;
    if (o.updated) updated = o.updated;
    hasDetails = hasDetails || o.hasDetails;
}

namespace parse {
namespace {

using json = nlohmann::json;

std::optional<json> load(const std::string& body) {
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return std::nullopt;
    return j;
}

std::int64_t i64(const json& j, const char* key) {
    const auto it = j.find(key);
    if (it == j.end()) return 0;
    if (it->is_number_integer()) return it->get<std::int64_t>();
    if (it->is_number_float()) return static_cast<std::int64_t>(it->get<double>());
    if (it->is_string()) {
        try {
            return std::stoll(it->get<std::string>());
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

std::string text(const json& j, const char* key) {
    const auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string{};
}

// Shared by omni-search and explore entries, which use the same game shape.
GameInfo gameFromListEntry(const json& e) {
    GameInfo g;
    g.universeId = i64(e, "universeId");
    if (!g.universeId) g.universeId = i64(e, "contentId");
    g.rootPlaceId = i64(e, "rootPlaceId");
    if (!g.rootPlaceId) g.rootPlaceId = i64(e, "placeId");
    g.name = text(e, "name");
    g.description = text(e, "description");
    g.creatorName = text(e, "creatorName");
    g.creatorId = i64(e, "creatorId");
    g.creatorType = text(e, "creatorType");
    g.playing = i64(e, "playerCount");
    g.upVotes = i64(e, "totalUpVotes");
    g.downVotes = i64(e, "totalDownVotes");
    g.genre = text(e, "genre");
    return g;
}

}  // namespace

std::time_t iso8601(const std::string& s) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) < 3) return 0;
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = sec;
    return _mkgmtime(&tm);
}

std::optional<std::vector<GameInfo>> gameDetails(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->contains("data") || !(*j)["data"].is_array()) return std::nullopt;
    std::vector<GameInfo> out;
    for (const auto& e : (*j)["data"]) {
        if (!e.is_object()) continue;
        GameInfo g;
        g.universeId = i64(e, "id");
        g.rootPlaceId = i64(e, "rootPlaceId");
        g.name = text(e, "name");
        g.description = text(e, "description");
        if (const auto c = e.find("creator"); c != e.end() && c->is_object()) {
            g.creatorName = text(*c, "name");
            g.creatorId = i64(*c, "id");
            g.creatorType = text(*c, "type");
        }
        g.genre = text(e, "genre_l1");
        if (g.genre.empty()) g.genre = text(e, "genre");
        if (g.genre == "All") g.genre.clear();  // legacy placeholder, carries no signal
        g.playing = i64(e, "playing");
        g.visits = i64(e, "visits");
        g.favorites = i64(e, "favoritedCount");
        g.maxPlayers = static_cast<int>(i64(e, "maxPlayers"));
        g.updated = iso8601(text(e, "updated"));
        g.hasDetails = true;
        if (g.universeId) out.push_back(std::move(g));
    }
    return out;
}

std::optional<std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>>> votes(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->contains("data") || !(*j)["data"].is_array()) return std::nullopt;
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> out;
    for (const auto& e : (*j)["data"]) {
        if (!e.is_object()) continue;
        const auto id = i64(e, "id");
        if (id) out[id] = {i64(e, "upVotes"), i64(e, "downVotes")};
    }
    return out;
}

std::optional<std::vector<GameInfo>> omniSearch(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->is_object()) return std::nullopt;
    const auto groups = j->find("searchResults");
    if (groups == j->end() || !groups->is_array()) return std::nullopt;
    std::vector<GameInfo> out;
    std::set<std::int64_t> seen;
    for (const auto& group : *groups) {
        if (!group.is_object()) continue;
        const std::string type = text(group, "contentGroupType");
        if (!type.empty() && type != "Game") continue;
        const auto contents = group.find("contents");
        if (contents == group.end() || !contents->is_array()) continue;
        for (const auto& e : *contents) {
            if (!e.is_object()) continue;
            const std::string ct = text(e, "contentType");
            if (!ct.empty() && ct != "Game") continue;
            GameInfo g = gameFromListEntry(e);
            if (g.universeId && seen.insert(g.universeId).second) out.push_back(std::move(g));
        }
    }
    return out;
}

std::optional<std::vector<GameInfo>> exploreSorts(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->is_object()) return std::nullopt;
    std::vector<GameInfo> out;
    std::set<std::int64_t> seen;
    auto addGames = [&](const json& games) {
        if (!games.is_array()) return;
        for (const auto& e : games) {
            if (!e.is_object()) continue;
            GameInfo g = gameFromListEntry(e);
            if (g.universeId && seen.insert(g.universeId).second) out.push_back(std::move(g));
        }
    };
    if (const auto sorts = j->find("sorts"); sorts != j->end() && sorts->is_array()) {
        for (const auto& sort : *sorts) {
            if (!sort.is_object()) continue;
            if (const auto games = sort.find("games"); games != sort.end()) addGames(*games);
        }
    } else if (const auto games = j->find("games"); games != j->end()) {
        addGames(*games);
    } else {
        return std::nullopt;
    }
    return out;
}

std::optional<std::map<std::int64_t, std::string>> thumbnails(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->contains("data") || !(*j)["data"].is_array()) return std::nullopt;
    std::map<std::int64_t, std::string> out;
    for (const auto& e : (*j)["data"]) {
        if (!e.is_object()) continue;
        const auto id = i64(e, "targetId");
        const std::string url = text(e, "imageUrl");
        if (id && !url.empty() && text(e, "state") == "Completed") out[id] = url;
    }
    return out;
}

std::optional<std::map<std::int64_t, std::string>> multigetThumbnails(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->contains("data") || !(*j)["data"].is_array()) return std::nullopt;
    std::map<std::int64_t, std::string> out;
    for (const auto& e : (*j)["data"]) {
        if (!e.is_object()) continue;
        const auto id = i64(e, "universeId");
        const auto thumbs = e.find("thumbnails");
        if (!id || thumbs == e.end() || !thumbs->is_array()) continue;
        for (const auto& t : *thumbs) {
            if (t.is_object() && text(t, "state") == "Completed" && !text(t, "imageUrl").empty()) {
                out[id] = text(t, "imageUrl");
                break;
            }
        }
    }
    return out;
}

std::optional<std::int64_t> universeId(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->is_object()) return std::nullopt;
    const auto id = i64(*j, "universeId");
    if (!id) return std::nullopt;
    return id;
}

std::optional<QuickLoginCode> quickLoginCreate(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->is_object()) return std::nullopt;
    QuickLoginCode c;
    c.code = text(*j, "code");
    c.privateKey = text(*j, "privateKey");
    c.expires = iso8601(text(*j, "expirationTime"));
    if (c.code.empty() || c.privateKey.empty()) return std::nullopt;
    return c;
}

std::optional<QuickLoginState> quickLoginStatus(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->is_object()) return std::nullopt;
    const std::string s = text(*j, "status");
    QuickLoginState st;
    if (s == "Created") {
        st.status = QuickLoginStatus::Created;
    } else if (s == "UserLinked") {
        st.status = QuickLoginStatus::UserLinked;
    } else if (s == "Validated") {
        st.status = QuickLoginStatus::Validated;
    } else if (s == "Cancelled") {
        st.status = QuickLoginStatus::Cancelled;
    } else {
        return std::nullopt;
    }
    st.accountName = text(*j, "accountName");
    return st;
}

std::string sessionCookie(const std::string& setCookieHeaders) {
    static const std::string kName = ".ROBLOSECURITY=";
    size_t pos = 0;
    while ((pos = setCookieHeaders.find(kName, pos)) != std::string::npos) {
        const size_t start = pos + kName.size();
        const size_t end = setCookieHeaders.find_first_of(";\r\n", start);
        std::string value = setCookieHeaders.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!value.empty()) return value;  // an empty value is a deletion, keep looking
        pos = start;
    }
    return {};
}

bool isChallenge(const std::string& body) {
    return body.find("Challenge") != std::string::npos || body.find("captcha") != std::string::npos;
}

std::optional<AuthenticatedUser> authenticatedUser(const std::string& body) {
    const auto j = load(body);
    if (!j || !j->is_object()) return std::nullopt;
    AuthenticatedUser u;
    u.id = i64(*j, "id");
    u.name = text(*j, "name");
    u.displayName = text(*j, "displayName");
    if (!u.id || u.name.empty()) return std::nullopt;
    if (u.displayName.empty()) u.displayName = u.name;
    return u;
}

}  // namespace parse
}  // namespace rl::api
