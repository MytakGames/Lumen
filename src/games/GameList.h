#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "settings/Settings.h"  // LoadStatus

namespace rl::games {

struct GameEntry {
    std::int64_t universeId = 0;
    std::int64_t placeId = 0;
    std::string name;
    std::string creatorName;
    std::int64_t creatorId = 0;
    std::string genre;
    std::time_t lastPlayed = 0;
    std::time_t added = 0;
    int playCount = 0;
};

// A persisted, ordered list of games (most recent first). Used both for the
// "Recent games" history (bounded) and for favorites (unbounded).
class GameList {
public:
    GameList(std::filesystem::path file, size_t maxSize) : file_(std::move(file)), max_(maxSize) {}

    settings::LoadStatus load();
    bool save() const;

    const std::vector<GameEntry>& items() const { return items_; }
    bool contains(std::int64_t universeId) const;
    const GameEntry* find(std::int64_t universeId) const;

    // Recent history: moves the game to the front, bumps playCount and lastPlayed.
    void touch(const GameEntry& entry, std::time_t now);
    // Favorites: inserts at the front if missing.
    void add(GameEntry entry, std::time_t now);
    bool remove(std::int64_t universeId);
    void clear() { items_.clear(); }
    void setMaxSize(size_t max);

    static std::string toJson(const std::vector<GameEntry>& items);
    static std::vector<GameEntry> fromJson(const std::string& text);  // throws on malformed input

private:
    void trim();

    std::filesystem::path file_;
    size_t max_;  // 0 = unbounded
    std::vector<GameEntry> items_;
};

}  // namespace rl::games
