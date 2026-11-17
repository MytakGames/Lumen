#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/Error.h"
#include "core/Notice.h"
#include "core/ThreadPool.h"
#include "games/GameList.h"
#include "robloxapi/RobloxApi.h"

namespace rl::games {

// Game data for the UI: search (debounced), Home sections, details, icons,
// recent history and favorites. UI-thread API; network on the pool.
class GameService {
public:
    static constexpr std::chrono::milliseconds kDebounce{300};

    GameService(api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main, GameList& recents, GameList& favorites,
                Notify notify, AliveFlag alive);

    // --- search ---
    void setQuery(const std::string& query);
    void tick();  // call every frame; fires the debounced search
    const std::string& query() const { return query_; }
    bool searchPending() const { return debouncePending_; }
    bool searching() const { return searching_; }
    bool hasSearched() const { return searchedQuery_ == query_ && !query_.empty(); }
    ErrorCode searchError() const { return searchError_; }
    const std::vector<std::int64_t>& results() const { return results_; }

    // --- home ---
    void loadHome(bool force = false);
    bool forYouLoading() const { return forYouLoading_; }
    bool forYouPersonalized() const { return forYouPersonalized_; }
    ErrorCode forYouError() const { return forYouError_; }
    const std::vector<std::int64_t>& forYou() const { return forYou_; }

    // --- games ---
    const api::GameInfo* game(std::int64_t universeId) const;
    void ensureDetails(std::int64_t universeId);
    bool detailsLoading(std::int64_t universeId) const { return pendingDetails_.count(universeId) != 0; }
    ErrorCode detailsError(std::int64_t universeId) const;
    std::string iconUrl(std::int64_t universeId) const;
    std::string bannerUrl(std::int64_t universeId) const;
    // Resolves a place id typed by the user ("roblox.com/games/<id>") to a universe.
    void openPlace(std::int64_t placeId, std::function<void(std::int64_t universeId)> done);

    // --- history & favorites ---
    const GameList& recents() const { return recents_; }
    const GameList& favorites() const { return favorites_; }
    void recordPlayed(std::int64_t universeId);
    void removeRecent(std::int64_t universeId);
    void clearHistory();
    bool isFavorite(std::int64_t universeId) const { return favorites_.contains(universeId); }
    void toggleFavorite(std::int64_t universeId);
    void setRecentMax(size_t max);

    void clearMemoryCache();

private:
    void merge(const std::vector<api::GameInfo>& games);
    void requestIcons(const std::vector<std::int64_t>& ids);
    void fetchDetails(const std::vector<std::int64_t>& ids);
    GameEntry entryFor(std::int64_t universeId) const;
    void persist(GameList& list);

    api::RobloxApi& api_;
    ThreadPool& pool_;
    MainThreadQueue& main_;
    GameList& recents_;
    GameList& favorites_;
    Notify notify_;
    AliveFlag alive_;

    std::map<std::int64_t, api::GameInfo> games_;
    std::map<std::int64_t, std::string> icons_;
    std::map<std::int64_t, std::string> banners_;
    std::set<std::int64_t> pendingIcons_;
    std::set<std::int64_t> pendingDetails_;
    std::set<std::int64_t> pendingBanners_;
    std::map<std::int64_t, ErrorCode> detailErrors_;

    std::string query_;
    std::string searchedQuery_;
    std::chrono::steady_clock::time_point lastEdit_{};
    bool debouncePending_ = false;
    bool searching_ = false;
    int searchSeq_ = 0;
    ErrorCode searchError_ = ErrorCode::None;
    std::vector<std::int64_t> results_;

    bool forYouLoading_ = false;
    bool forYouLoaded_ = false;
    bool forYouPersonalized_ = false;
    ErrorCode forYouError_ = ErrorCode::None;
    std::vector<std::int64_t> forYou_;
};

}  // namespace rl::games
