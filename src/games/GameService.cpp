#include "games/GameService.h"

#include <algorithm>
#include <ctime>

#include "core/Logger.h"
#include "core/StringUtil.h"
#include "games/RecommendationEngine.h"

namespace rl::games {
namespace {
constexpr size_t kForYouCount = 18;
constexpr size_t kCandidateCount = 100;
}  // namespace

GameService::GameService(api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main, GameList& recents,
                         GameList& favorites, Notify notify, AliveFlag alive)
    : api_(api),
      pool_(pool),
      main_(main),
      recents_(recents),
      favorites_(favorites),
      notify_(std::move(notify)),
      alive_(std::move(alive)) {}

// --- search ------------------------------------------------------------------------

void GameService::setQuery(const std::string& query) {
    if (query == query_) return;
    query_ = query;
    lastEdit_ = std::chrono::steady_clock::now();
    const bool empty = str::trim(query_).empty();
    debouncePending_ = !empty;
    if (empty) {
        ++searchSeq_;  // drop in-flight results
        searching_ = false;
        results_.clear();
        searchError_ = ErrorCode::None;
        searchedQuery_.clear();
    }
}

void GameService::tick() {
    if (!debouncePending_ || std::chrono::steady_clock::now() - lastEdit_ < kDebounce) return;
    debouncePending_ = false;
    const std::string q(str::trim(query_));
    const int seq = ++searchSeq_;
    searching_ = true;
    searchError_ = ErrorCode::None;
    pool_.post([this, q, seq, full = query_, alive = alive_] {
        auto res = api_.search(q);
        main_.post([this, res = std::move(res), seq, full, alive] {
            if (!*alive || seq != searchSeq_) return;
            searching_ = false;
            searchedQuery_ = full;
            results_.clear();
            if (!res) {
                searchError_ = res.error;
                return;
            }
            merge(*res.value);
            for (const auto& g : *res.value) results_.push_back(g.universeId);
            requestIcons(results_);
            fetchDetails(results_);  // search entries carry no creator/genre
        });
    });
}

// --- home ----------------------------------------------------------------------------

void GameService::loadHome(bool force) {
    // Refresh details (players online etc.) of the games shown in Recent and Favorites.
    std::vector<std::int64_t> known;
    for (const auto& g : recents_.items()) known.push_back(g.universeId);
    for (const auto& g : favorites_.items())
        if (std::find(known.begin(), known.end(), g.universeId) == known.end()) known.push_back(g.universeId);
    if (!known.empty()) {
        fetchDetails(known);
        requestIcons(known);
    }

    if (forYouLoading_ || (forYouLoaded_ && !force)) return;
    forYouLoading_ = true;
    forYouError_ = ErrorCode::None;
    const TasteProfile profile = buildProfile(recents_.items(), std::time(nullptr));
    pool_.post([this, profile, alive = alive_] {
        auto popular = api_.popular();
        std::vector<api::GameInfo> candidates;
        ErrorCode error = popular.error;
        if (popular) {
            candidates = std::move(*popular.value);
            if (candidates.size() > kCandidateCount) candidates.resize(kCandidateCount);
            // Explore entries carry no genre/creator id: enrich them for the ranking.
            std::vector<std::int64_t> ids;
            for (const auto& c : candidates) ids.push_back(c.universeId);
            if (auto details = api_.details(ids)) {
                std::map<std::int64_t, api::GameInfo> byId;
                for (auto& d : *details.value) byId[d.universeId] = std::move(d);
                for (auto& c : candidates) {
                    const auto it = byId.find(c.universeId);
                    if (it != byId.end()) c.mergeFrom(it->second);
                }
            }
        }
        auto ranked = recommend(profile, candidates, kForYouCount);
        main_.post([this, alive, error, ranked = std::move(ranked), personalized = !profile.empty()] {
            if (!*alive) return;
            forYouLoading_ = false;
            forYouError_ = error;
            if (error != ErrorCode::None) return;
            forYouLoaded_ = true;
            forYouPersonalized_ = personalized;
            forYou_.clear();
            std::vector<api::GameInfo> games;
            for (const auto& s : ranked) {
                games.push_back(s.game);
                forYou_.push_back(s.game.universeId);
            }
            merge(games);
            requestIcons(forYou_);
        });
    });
}

// --- games ---------------------------------------------------------------------------

const api::GameInfo* GameService::game(std::int64_t universeId) const {
    const auto it = games_.find(universeId);
    return it == games_.end() ? nullptr : &it->second;
}

ErrorCode GameService::detailsError(std::int64_t universeId) const {
    const auto it = detailErrors_.find(universeId);
    return it == detailErrors_.end() ? ErrorCode::None : it->second;
}

std::string GameService::iconUrl(std::int64_t universeId) const {
    const auto it = icons_.find(universeId);
    return it == icons_.end() ? std::string{} : it->second;
}

std::string GameService::bannerUrl(std::int64_t universeId) const {
    const auto it = banners_.find(universeId);
    return it == banners_.end() ? std::string{} : it->second;
}

void GameService::merge(const std::vector<api::GameInfo>& games) {
    for (const auto& g : games) {
        auto& slot = games_[g.universeId];
        slot.mergeFrom(g);
    }
}

void GameService::fetchDetails(const std::vector<std::int64_t>& ids) {
    std::vector<std::int64_t> todo;
    for (const auto id : ids)
        if (pendingDetails_.insert(id).second) todo.push_back(id);
    if (todo.empty()) return;
    pool_.post([this, todo, alive = alive_] {
        auto res = api_.details(todo);
        main_.post([this, todo, res = std::move(res), alive] {
            if (!*alive) return;
            for (const auto id : todo) pendingDetails_.erase(id);
            if (!res) {
                for (const auto id : todo) detailErrors_[id] = res.error;
                return;
            }
            for (const auto id : todo) detailErrors_.erase(id);
            merge(*res.value);
        });
    });
}

void GameService::ensureDetails(std::int64_t universeId) {
    const auto* g = game(universeId);
    if ((!g || !g->hasDetails) && !detailErrors_.count(universeId)) fetchDetails({universeId});
    requestIcons({universeId});
    if (!banners_.count(universeId) && pendingBanners_.insert(universeId).second) {
        pool_.post([this, universeId, alive = alive_] {
            auto res = api_.gameBanners({universeId});
            main_.post([this, universeId, res = std::move(res), alive] {
                if (!*alive) return;
                pendingBanners_.erase(universeId);
                banners_[universeId] = res ? (res.value->count(universeId) ? res.value->at(universeId) : "") : "";
            });
        });
    }
}

void GameService::openPlace(std::int64_t placeId, std::function<void(std::int64_t)> done) {
    pool_.post([this, placeId, done, alive = alive_] {
        auto res = api_.universeForPlace(placeId);
        main_.post([this, res, done, alive] {
            if (!*alive) return;
            if (!res) {
                notify_({Notice::Level::Error, errorKey(res.error), {}});
                return;
            }
            done(*res.value);
        });
    });
}

void GameService::requestIcons(const std::vector<std::int64_t>& ids) {
    std::vector<std::int64_t> todo;
    for (const auto id : ids)
        if (!icons_.count(id) && pendingIcons_.insert(id).second) todo.push_back(id);
    if (todo.empty()) return;
    pool_.post([this, todo, alive = alive_] {
        auto res = api_.gameIcons(todo);
        main_.post([this, todo, res = std::move(res), alive] {
            if (!*alive) return;
            for (const auto id : todo) pendingIcons_.erase(id);
            if (!res) return;  // cards keep their placeholder; a later request retries
            for (const auto& [id, url] : *res.value) icons_[id] = url;
        });
    });
}

// --- history & favorites -------------------------------------------------------------

GameEntry GameService::entryFor(std::int64_t universeId) const {
    GameEntry e;
    e.universeId = universeId;
    if (const auto* g = game(universeId)) {
        e.placeId = g->rootPlaceId;
        e.name = g->name;
        e.creatorName = g->creatorName;
        e.creatorId = g->creatorId;
        e.genre = g->genre;
    } else if (const auto* r = recents_.find(universeId)) {
        e = *r;
    } else if (const auto* f = favorites_.find(universeId)) {
        e = *f;
    }
    return e;
}

void GameService::persist(GameList& list) {
    if (!list.save()) notify_({Notice::Level::Error, "error.io", {}});
}

void GameService::recordPlayed(std::int64_t universeId) {
    recents_.touch(entryFor(universeId), std::time(nullptr));
    persist(recents_);
}

void GameService::removeRecent(std::int64_t universeId) {
    if (recents_.remove(universeId)) persist(recents_);
}

void GameService::clearHistory() {
    recents_.clear();
    persist(recents_);
    forYouLoaded_ = false;
}

void GameService::toggleFavorite(std::int64_t universeId) {
    if (favorites_.contains(universeId)) {
        favorites_.remove(universeId);
    } else {
        favorites_.add(entryFor(universeId), std::time(nullptr));
    }
    persist(favorites_);
}

void GameService::setRecentMax(size_t max) {
    recents_.setMaxSize(max);
}

void GameService::clearMemoryCache() {
    api_.cache().clear();
    icons_.clear();
    banners_.clear();
    detailErrors_.clear();
    forYouLoaded_ = false;
}

}  // namespace rl::games
