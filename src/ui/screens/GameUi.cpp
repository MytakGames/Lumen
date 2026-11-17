#include "ui/screens/GameUi.h"

#include "games/GameService.h"
#include "ui/Components.h"
#include "ui/TextureCache.h"
#include "ui/Widgets.h"

namespace rl::ui {

std::string gameMeta(const UiContext& ctx, std::int64_t players, int ratingPercent) {
    std::string meta = ctx.tr.format(players == 1 ? "games.players_one" : "games.players_other",
                                     {{"count", compactNumber(players, ctx.tr.language() == "it")}});
    if (ratingPercent >= 0) meta += "  \xC2\xB7  " + ctx.tr.format("games.rating_short", {{"value", std::to_string(ratingPercent)}});
    return meta;
}

void gameGrid(UiContext& ctx, const char* id, const std::vector<std::int64_t>& universeIds, bool recentMenu,
              size_t maxItems) {
    auto& games = *ctx.games;
    ImGui::PushID(id);
    Grid grid(dp(172), dp(16));
    size_t shown = 0;
    for (const auto uid : universeIds) {
        if (maxItems && shown >= maxItems) break;
        ++shown;
        const api::GameInfo* g = games.game(uid);
        const games::GameEntry* saved = games.recents().find(uid);
        if (!saved) saved = games.favorites().find(uid);

        const std::string name = g && !g->name.empty() ? g->name : (saved ? saved->name : std::to_string(uid));
        const std::string creator = g && !g->creatorName.empty() ? g->creatorName : (saved ? saved->creatorName : "");
        std::string subtitle = creator.empty() ? "" : ctx.tr.format("games.by", {{"creator", creator}});
        std::string meta;
        if (g) {
            const auto rating = g->ratingPercent();
            meta = gameMeta(ctx, g->playing, rating ? *rating : -1);
        } else if (saved && saved->lastPlayed) {
            meta = ctx.tr.formatRelative(saved->lastPlayed, std::time(nullptr));
        }
        if (recentMenu && saved && saved->lastPlayed) {
            subtitle = ctx.tr.formatRelative(saved->lastPlayed, std::time(nullptr));
            if (!creator.empty()) subtitle = creator + "  \xC2\xB7  " + subtitle;
        }

        CardInfo info;
        info.image = ctx.textures->get(games.iconUrl(uid));
        info.title = name.c_str();
        info.subtitle = subtitle.c_str();
        info.meta = meta.c_str();
        info.favorite = games.isFavorite(uid);

        grid.next();
        const std::string cardId = "##g" + std::to_string(uid);
        if (gameCard(cardId.c_str(), info, grid.itemWidth())) ctx.openGame(uid);

        if (ImGui::BeginPopupContextItem(cardId.c_str())) {
            if (ImGui::MenuItem(ctx.tr.c("games.open"))) ctx.openGame(uid);
            if (ImGui::MenuItem(ctx.tr.c(info.favorite ? "games.unfavorite" : "games.favorite"))) games.toggleFavorite(uid);
            if (recentMenu && ImGui::MenuItem(ctx.tr.c("games.remove_recent"))) games.removeRecent(uid);
            ImGui::EndPopup();
        }
    }
    ImGui::PopID();
}

}  // namespace rl::ui
