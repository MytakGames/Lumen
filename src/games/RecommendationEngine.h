#pragma once

#include <cstdint>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "games/GameList.h"
#include "robloxapi/Types.h"

// "For You" suggestions. Pure functions over local history and candidate
// games, so the ranking is deterministic and unit-testable.
//
// Local data used: the Recent games list only (universe id, genre, creator,
// play count, last played). Nothing is sent anywhere; clearing the recent
// history resets the profile.
namespace rl::games {

struct TasteProfile {
    std::map<std::string, double> genres;          // normalized to sum 1
    std::map<std::int64_t, double> creators;       // normalized to sum 1
    std::set<std::int64_t> played;
    bool empty() const { return played.empty(); }
};

// Each play counts more when recent: weight = playCount * 0.5^(ageDays / 14).
TasteProfile buildProfile(const std::vector<GameEntry>& recents, std::time_t now);

struct Scored {
    api::GameInfo game;
    double score = 0.0;
};

// Ranks candidates. Already-played games are excluded. With an empty profile
// the result is the popularity order (players online, then rating).
std::vector<Scored> recommend(const TasteProfile& profile, const std::vector<api::GameInfo>& candidates,
                              size_t limit);

}  // namespace rl::games
