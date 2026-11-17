#pragma once

#include <cstdint>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "robloxapi/Types.h"

// Pure, tolerant parsers for Roblox web API responses. They never throw:
// malformed documents yield nullopt, unknown/missing fields are skipped.
// Several of these endpoints are public but not formally versioned, so the
// parsers accept the field-name variants seen in the wild.
namespace rl::api::parse {

// games.roblox.com/v1/games?universeIds=...
std::optional<std::vector<GameInfo>> gameDetails(const std::string& body);
// games.roblox.com/v1/games/votes?universeIds=...  -> universeId -> (up, down)
std::optional<std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>>> votes(const std::string& body);
// apis.roblox.com/search-api/omni-search
std::optional<std::vector<GameInfo>> omniSearch(const std::string& body);
// apis.roblox.com/explore-api/v1/get-sorts (and get-sort-content)
std::optional<std::vector<GameInfo>> exploreSorts(const std::string& body);
// thumbnails.roblox.com/v1/games/icons, /v1/users/avatar-headshot -> targetId -> imageUrl
std::optional<std::map<std::int64_t, std::string>> thumbnails(const std::string& body);
// thumbnails.roblox.com/v1/games/multiget/thumbnails -> universeId -> first imageUrl
std::optional<std::map<std::int64_t, std::string>> multigetThumbnails(const std::string& body);
// apis.roblox.com/universes/v1/places/{placeId}/universe
std::optional<std::int64_t> universeId(const std::string& body);
// users.roblox.com/v1/users/authenticated
std::optional<AuthenticatedUser> authenticatedUser(const std::string& body);

// apis.roblox.com/auth-token-service/v1/login/create
std::optional<QuickLoginCode> quickLoginCreate(const std::string& body);
// apis.roblox.com/auth-token-service/v1/login/status
std::optional<QuickLoginState> quickLoginStatus(const std::string& body);
// Extracts the .ROBLOSECURITY value from (possibly several, newline-joined) Set-Cookie headers.
std::string sessionCookie(const std::string& setCookieHeaders);
// True when an error body says Roblox wants an extra challenge (captcha etc.).
bool isChallenge(const std::string& body);

// "2024-05-01T12:34:56.789Z" -> UTC time_t (0 on failure).
std::time_t iso8601(const std::string& s);

}  // namespace rl::api::parse
