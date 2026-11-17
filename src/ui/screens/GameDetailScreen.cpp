#include "ui/screens/GameDetailScreen.h"

#include <algorithm>
#include <ctime>

#include "accounts/AccountService.h"
#include "games/GameService.h"
#include "instances/InstanceManager.h"
#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/TextureCache.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace rl::ui {

void GameDetailScreen::setGame(std::int64_t universeId) { universeId_ = universeId; }

void GameDetailScreen::onShow(UiContext& ctx) {
    if (universeId_) ctx.games->ensureDetails(universeId_);
    const accounts::Account* a = selectedAccount_.empty() ? nullptr : ctx.accounts->find(selectedAccount_);
    if (!a || !a->enabled || !a->hasSession()) selectedAccount_ = ctx.defaultAccountId();
}

void GameDetailScreen::draw(UiContext& ctx) {
    const Palette& p = palette();
    auto& games = *ctx.games;
    if (glyphButton(ctx.tr.c("common.back"), "##back", Glyph::Back, false)) {
        ctx.goBack();
        return;
    }
    ImGui::Dummy({0, dp(2)});

    const api::GameInfo* g = games.game(universeId_);
    const games::GameEntry* saved = games.recents().find(universeId_);
    if (!saved) saved = games.favorites().find(universeId_);
    const std::string name = g && !g->name.empty() ? g->name : (saved ? saved->name : "");

    // Banner (16:9, capped height)
    const float width = ImGui::GetContentRegionAvail().x;
    const float bannerH = std::min(width * 9.0f / 16.0f, dp(340));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const std::string bannerUrl = games.bannerUrl(universeId_);
    ImTextureID banner = ctx.textures->get(bannerUrl.empty() ? games.iconUrl(universeId_) : bannerUrl);
    imageOrPlaceholder(banner, pos, {pos.x + width, pos.y + bannerH}, dp(16));
    // Bottom gradient for legibility of the title overlay.
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor({pos.x, pos.y + bannerH * 0.55f}, {pos.x + width, pos.y + bannerH},
                                                       IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                                       IM_COL32(0, 0, 0, 170), IM_COL32(0, 0, 0, 170));
    ImGui::Dummy({width, bannerH});

    // Title row
    const ImVec2 titlePos(pos.x + dp(24), pos.y + bannerH - dp(64));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushFont(fonts().title);
    const std::string title = ellipsize(name.empty() ? ctx.tr.tr("detail.loading") : name, width - dp(48));
    dl->AddText(titlePos, IM_COL32_WHITE, title.c_str());
    ImGui::PopFont();
    if (g && !g->creatorName.empty()) {
        ImGui::PushFont(fonts().regular);
        const std::string by = ctx.tr.format("games.by", {{"creator", g->creatorName}});
        dl->AddText({titlePos.x, titlePos.y + dp(34)}, IM_COL32(230, 230, 235, 230), by.c_str());
        ImGui::PopFont();
    }

    // Favorite toggle (top-right on the banner)
    {
        const bool fav = games.isFavorite(universeId_);
        const char* label = ctx.tr.c(fav ? "games.unfavorite" : "games.favorite");
        const float w = ImGui::CalcTextSize(label).x + dp(56);
        const ImVec2 back = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos({pos.x + width - w - dp(16), pos.y + dp(16)});
        if (glyphButton(label, "##fav", fav ? Glyph::StarFilled : Glyph::Star, false)) games.toggleFavorite(universeId_);
        ImGui::SetCursorScreenPos(back);
    }

    if (games.detailsLoading(universeId_) && !(g && g->hasDetails)) {
        loadingRow(ctx.tr.c("detail.loading"));
    } else if (games.detailsError(universeId_) != ErrorCode::None && !(g && g->hasDetails)) {
        wrappedText(ctx.tr.c(errorKey(games.detailsError(universeId_))), p.danger);
    }

    drawStats(ctx);
    drawLaunchCard(ctx);

    if (g && !g->description.empty()) {
        sectionTitle(ctx.tr.c("detail.description"));
        beginCard("##desc");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(g->description.c_str());
        ImGui::PopTextWrapPos();
        endCard();
    }
    ImGui::Dummy({0, dp(24)});
}

void GameDetailScreen::drawStats(UiContext& ctx) {
    const api::GameInfo* g = ctx.games->game(universeId_);
    if (!g) return;
    const bool it = ctx.tr.language() == "it";
    struct Stat {
        const char* key;
        std::string value;
    };
    std::vector<Stat> stats = {
        {"detail.playing", ctx.tr.formatNumber(g->playing)},
        {"detail.visits", compactNumber(g->visits, it)},
        {"detail.rating", g->ratingPercent() ? std::to_string(*g->ratingPercent()) + "%" : "\xE2\x80\x94"},
        {"detail.updated", g->updated ? ctx.tr.formatRelative(g->updated, std::time(nullptr)) : "\xE2\x80\x94"},
        {"detail.max_players", g->maxPlayers ? std::to_string(g->maxPlayers) : "\xE2\x80\x94"},
    };
    if (!g->genre.empty()) stats.push_back({"detail.genre", g->genre});

    const Palette& p = palette();
    Grid grid(dp(130), dp(12));
    for (const auto& s : stats) {
        grid.next();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(grid.itemWidth(), dp(66));
        ImGui::Dummy(size);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, toU32(p.surface), dp(12));
        dl->AddRect(pos, {pos.x + size.x, pos.y + size.y}, toU32(p.border), dp(12));
        ImGui::PushFont(fonts().small);
        dl->AddText({pos.x + dp(14), pos.y + dp(10)}, toU32(p.textMuted), ctx.tr.c(s.key));
        ImGui::PopFont();
        ImGui::PushFont(fonts().semibold);
        const std::string v = ellipsize(s.value, size.x - dp(28));
        dl->AddText({pos.x + dp(14), pos.y + dp(32)}, toU32(p.text), v.c_str());
        ImGui::PopFont();
    }
}

void GameDetailScreen::drawLaunchCard(UiContext& ctx) {
    sectionTitle(ctx.tr.c("detail.play_section"));
    beginCard("##launch");
    auto& accounts = *ctx.accounts;

    std::vector<const accounts::Account*> usable;
    for (const auto& a : accounts.accounts())
        if (a.enabled && a.hasSession()) usable.push_back(&a);

    if (usable.empty()) {
        mutedText(ctx.tr.c("detail.no_accounts"));
        if (glyphButton(ctx.tr.c("accounts.add"), "##add_from_detail", Glyph::Plus, true, !ctx.loginInProgress())) {
            ctx.addAccount();
        }
        endCard();
        return;
    }
    if (std::none_of(usable.begin(), usable.end(), [&](const accounts::Account* a) { return a->id == selectedAccount_; })) {
        selectedAccount_ = usable.front()->id;
    }
    const accounts::Account* current = accounts.find(selectedAccount_);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(ctx.tr.c("detail.account"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(dp(280), ImGui::GetContentRegionAvail().x - dp(10)));
    if (ImGui::BeginCombo("##account", current ? current->label().c_str() : "")) {
        for (const auto* a : usable) {
            std::string item = a->label();
            if (ctx.instances->accountActive(a->id)) item += "  \xE2\x80\xA2  " + ctx.tr.tr("detail.in_game");
            item += "###" + a->id;
            if (ImGui::Selectable(item.c_str(), a->id == selectedAccount_)) selectedAccount_ = a->id;
        }
        ImGui::EndCombo();
    }

    const bool active = ctx.instances->accountActive(selectedAccount_);
    if (active) {
        ImGui::SameLine();
        chip(ctx.tr.c("detail.in_game"), palette().warning);
    }
    const auto state = accounts.state(selectedAccount_);
    if (state == accounts::SessionState::Expired) {
        wrappedText(ctx.tr.c("error.unauthorized"), palette().danger);
        if (glyphButton(ctx.tr.c("accounts.relogin"), "##relogin_detail", Glyph::Login, false, !ctx.loginInProgress())) {
            ctx.reloginAccount(selectedAccount_);
        }
    }

    const std::string instancesText = ctx.tr.format(
        "detail.instances", {{"active", std::to_string(ctx.instances->activeCount())},
                             {"max", std::to_string(ctx.instances->maxInstances())}});
    if (glyphButton(ctx.tr.c("detail.play"), "##play", Glyph::Play, true)) {
        ctx.launchGame(selectedAccount_, universeId_);
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + dp(2));
    ImGui::TextColored(palette().textMuted, "%s", instancesText.c_str());
    endCard();
}

}  // namespace rl::ui
