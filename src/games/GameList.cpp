#include "games/GameList.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "core/AtomicFile.h"
#include "core/Logger.h"

namespace rl::games {
namespace {

using json = nlohmann::json;

template <class T>
T value(const json& j, const char* key, T fallback) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    try {
        return it->get<T>();
    } catch (...) {
        return fallback;
    }
}

}  // namespace

std::string GameList::toJson(const std::vector<GameEntry>& items) {
    json arr = json::array();
    for (const auto& g : items) {
        arr.push_back({{"universeId", g.universeId},
                       {"placeId", g.placeId},
                       {"name", g.name},
                       {"creatorName", g.creatorName},
                       {"creatorId", g.creatorId},
                       {"genre", g.genre},
                       {"lastPlayed", static_cast<std::int64_t>(g.lastPlayed)},
                       {"added", static_cast<std::int64_t>(g.added)},
                       {"playCount", g.playCount}});
    }
    return json{{"version", 1}, {"games", std::move(arr)}}.dump(2);
}

std::vector<GameEntry> GameList::fromJson(const std::string& text) {
    const json j = json::parse(text);
    if (!j.is_object() || !j.contains("games") || !j["games"].is_array()) {
        throw std::runtime_error("game list has no 'games' array");
    }
    std::vector<GameEntry> out;
    for (const auto& e : j["games"]) {
        if (!e.is_object()) continue;
        GameEntry g;
        g.universeId = value<std::int64_t>(e, "universeId", 0);
        g.placeId = value<std::int64_t>(e, "placeId", 0);
        g.name = value<std::string>(e, "name", "");
        g.creatorName = value<std::string>(e, "creatorName", "");
        g.creatorId = value<std::int64_t>(e, "creatorId", 0);
        g.genre = value<std::string>(e, "genre", "");
        g.lastPlayed = static_cast<std::time_t>(value<std::int64_t>(e, "lastPlayed", 0));
        g.added = static_cast<std::time_t>(value<std::int64_t>(e, "added", 0));
        g.playCount = value<int>(e, "playCount", 0);
        if (g.universeId > 0) out.push_back(std::move(g));
    }
    return out;
}

settings::LoadStatus GameList::load() {
    items_.clear();
    const auto text = fs::readAll(file_);
    if (!text) return settings::LoadStatus::Missing;
    try {
        items_ = fromJson(*text);
        trim();
        return settings::LoadStatus::Ok;
    } catch (const std::exception& e) {
        log::error("games", "Corrupt list " + file_.filename().string() + ": " + e.what());
        fs::backupCorrupt(file_);
        return settings::LoadStatus::Corrupt;
    }
}

bool GameList::save() const { return fs::writeAtomic(file_, toJson(items_)); }

bool GameList::contains(std::int64_t universeId) const { return find(universeId) != nullptr; }

const GameEntry* GameList::find(std::int64_t universeId) const {
    const auto it = std::find_if(items_.begin(), items_.end(), [&](const GameEntry& g) { return g.universeId == universeId; });
    return it == items_.end() ? nullptr : &*it;
}

void GameList::touch(const GameEntry& entry, std::time_t now) {
    GameEntry updated = entry;
    const auto it =
        std::find_if(items_.begin(), items_.end(), [&](const GameEntry& g) { return g.universeId == entry.universeId; });
    if (it != items_.end()) {
        updated.playCount = it->playCount;
        updated.added = it->added;
        if (updated.genre.empty()) updated.genre = it->genre;
        if (updated.name.empty()) updated.name = it->name;
        items_.erase(it);
    }
    updated.playCount += 1;
    updated.lastPlayed = now;
    if (updated.added == 0) updated.added = now;
    items_.insert(items_.begin(), std::move(updated));
    trim();
}

void GameList::add(GameEntry entry, std::time_t now) {
    if (contains(entry.universeId)) return;
    entry.added = now;
    items_.insert(items_.begin(), std::move(entry));
    trim();
}

bool GameList::remove(std::int64_t universeId) {
    const auto before = items_.size();
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [&](const GameEntry& g) { return g.universeId == universeId; }),
                 items_.end());
    return items_.size() != before;
}

void GameList::setMaxSize(size_t max) {
    max_ = max;
    trim();
}

void GameList::trim() {
    if (max_ > 0 && items_.size() > max_) items_.resize(max_);
}

}  // namespace rl::games
