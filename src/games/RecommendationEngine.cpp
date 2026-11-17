#include "games/RecommendationEngine.h"

#include <algorithm>
#include <cmath>

namespace rl::games {

TasteProfile buildProfile(const std::vector<GameEntry>& recents, std::time_t now) {
    TasteProfile p;
    double genreTotal = 0.0, creatorTotal = 0.0;
    for (const auto& g : recents) {
        p.played.insert(g.universeId);
        const double ageDays = std::max(0.0, std::difftime(now, g.lastPlayed) / 86400.0);
        const double w = std::max(1, g.playCount) * std::pow(0.5, ageDays / 14.0);
        if (!g.genre.empty()) {
            p.genres[g.genre] += w;
            genreTotal += w;
        }
        if (g.creatorId != 0) {
            p.creators[g.creatorId] += w;
            creatorTotal += w;
        }
    }
    if (genreTotal > 0)
        for (auto& [k, v] : p.genres) v /= genreTotal;
    if (creatorTotal > 0)
        for (auto& [k, v] : p.creators) v /= creatorTotal;
    return p;
}

namespace {

double popularity(const api::GameInfo& g) {
    // log scale: 10 players -> ~0.17, 1M players -> 1.0
    return std::min(1.0, std::log10(1.0 + static_cast<double>(g.playing)) / 6.0);
}

double rating(const api::GameInfo& g) {
    const auto r = g.ratingPercent();
    return r ? *r / 100.0 : 0.5;
}

}  // namespace

std::vector<Scored> recommend(const TasteProfile& profile, const std::vector<api::GameInfo>& candidates,
                              size_t limit) {
    std::vector<Scored> out;
    std::set<std::int64_t> seen;
    for (const auto& g : candidates) {
        if (g.universeId == 0 || profile.played.count(g.universeId) || !seen.insert(g.universeId).second) continue;
        Scored s{g, 0.0};
        if (profile.empty()) {
            s.score = popularity(g) + 0.25 * rating(g);
        } else {
            const auto genre = profile.genres.find(g.genre);
            const auto creator = profile.creators.find(g.creatorId);
            const double genreAffinity = genre == profile.genres.end() ? 0.0 : genre->second;
            const double creatorAffinity = creator == profile.creators.end() ? 0.0 : creator->second;
            s.score = 3.0 * genreAffinity + 2.0 * creatorAffinity + 0.6 * popularity(g) + 0.4 * rating(g);
        }
        out.push_back(std::move(s));
    }
    std::stable_sort(out.begin(), out.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });
    if (out.size() > limit) out.resize(limit);
    return out;
}

}  // namespace rl::games
