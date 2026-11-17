#pragma once

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>

namespace rl::api {

struct GameInfo {
    std::int64_t universeId = 0;
    std::int64_t rootPlaceId = 0;
    std::string name;
    std::string description;
    std::string creatorName;
    std::string creatorType;  // "User" / "Group"
    std::int64_t creatorId = 0;
    std::string genre;        // best available genre label (genre_l1, else legacy genre)
    std::int64_t playing = 0;
    std::int64_t visits = 0;
    std::int64_t upVotes = 0;
    std::int64_t downVotes = 0;
    std::int64_t favorites = 0;
    int maxPlayers = 0;
    std::time_t updated = 0;
    bool hasDetails = false;  // filled from games/v1/games

    // 0..100, or nullopt when there are no votes.
    std::optional<int> ratingPercent() const {
        const auto total = upVotes + downVotes;
        if (total <= 0) return std::nullopt;
        return static_cast<int>((upVotes * 100 + total / 2) / total);
    }

    // Merges fields known by `other` (non-empty / non-zero) into this.
    void mergeFrom(const GameInfo& other);
};

// "Log in with another device" (cross-device / quick login).
struct QuickLoginCode {
    std::string code;        // shown to the user, e.g. "A1B2C3"
    std::string privateKey;  // secret paired with the code; never shown or logged
    std::time_t expires = 0;
};

enum class QuickLoginStatus { Created, UserLinked, Validated, Cancelled, Invalid };

struct QuickLoginState {
    QuickLoginStatus status = QuickLoginStatus::Created;
    std::string accountName;  // known once the user entered the code
};

struct AuthenticatedUser {
    std::int64_t id = 0;
    std::string name;
    std::string displayName;
};

}  // namespace rl::api
