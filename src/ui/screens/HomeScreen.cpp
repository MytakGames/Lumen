#include "ui/screens/HomeScreen.h"

#include <cctype>
#include <regex>
#include <string>

#include "core/Error.h"
#include "games/GameService.h"
#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/screens/GameUi.h"

namespace rl::ui {
namespace {

// "123456" or ".../games/123456/..." -> place id.
std::int64_t placeIdFromQuery(const std::string& q) {
    static const std::regex url(R"(roblox\.com/games/(\d{3,15}))", std::regex::icase);
    static const std::regex digits(R"(^\s*(\d{3,15})\s*$)");
    std::smatch m;
    if (std::regex_search(q, m, url) || std::regex_match(q, m, digits)) {
        try {
            return std::stoll(m[1].str());
        } catch (...) {
        }
    }
    return 0;
}

}  // namespace

void HomeScreen::onShow(UiContext& ctx) { ctx.games->loadHome(); }

void HomeScreen::draw(UiContext& ctx) {
    pageHeader(ctx.tr.c("home.title"), ctx.tr.c("home.subtitle"));
    drawSearchBox(ctx);
    ImGui::Dummy({0, dp(6)});
    if (!ctx.games->query().empty()) {
        drawResults(ctx);
    } else {
        drawSections(ctx);
    }
    ImGui::Dummy({0, dp(24)});
}

void HomeScreen::drawSearchBox(UiContext& ctx) {
    const Palette& p = palette();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {dp(40), dp(11)});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, dp(12));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, p.surface);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, p.surfaceHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, p.surfaceHover);
    ImGui::SetNextItemWidth(-1);
    ImGui::SetNextItemAllowOverlap();  // the clear button sits on top of the field
    if (ImGui::InputTextWithHint("##search", ctx.tr.c("home.search_hint"), query_.data(), query_.size())) {
        ctx.games->setQuery(query_.data());
    }
    const bool active = ImGui::IsItemActive();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = animate(ImGui::GetID("##search_focus"), active ? 1.0f : 0.0f);
    dl->AddRect(pos, max, toU32(mix(p.border, p.accent, h)), dp(12), 0, 1.0f + h);
    drawGlyph(dl, Glyph::Search, {pos.x + dp(21), (pos.y + max.y) * 0.5f}, dp(16),
              toU32(mix(p.textMuted, p.accent, h)));

    if (query_[0] != 0) {
        const float sz = dp(32);
        ImGui::SetCursorScreenPos({max.x - sz - dp(6), (pos.y + max.y) * 0.5f - sz * 0.5f});
        if (iconButton("##clear_search", Glyph::Close, ctx.tr.c("home.clear_search"))) {
            query_[0] = 0;
            ctx.games->setQuery("");
        }
        ImGui::SetCursorScreenPos({pos.x, max.y + ImGui::GetStyle().ItemSpacing.y});
        ImGui::Dummy({0, 0});
    }

    if (ctx.games->searchPending()) requestRedraw();  // keep the debounce timer ticking
}

void HomeScreen::drawResults(UiContext& ctx) {
    auto& games = *ctx.games;
    const std::string q = games.query();
    sectionTitle(ctx.tr.format("home.results_for", {{"query", q}}).c_str());

    if (const auto placeId = placeIdFromQuery(q)) {
        const std::string label = ctx.tr.format("home.open_place", {{"id", std::to_string(placeId)}});
        if (glyphButton(label.c_str(), "##open_place", Glyph::Play, false)) {
            auto openGame = ctx.openGame;
            games.openPlace(placeId, [openGame](std::int64_t uid) { openGame(uid); });
        }
    }

    if (games.searchPending() || games.searching()) {
        loadingRow(ctx.tr.c("home.searching"));
        return;
    }
    if (games.searchError() != ErrorCode::None) {
        wrappedText(ctx.tr.c(errorKey(games.searchError())), palette().danger);
        if (button(ctx.tr.c("common.retry"), "##retry_search")) {
            const std::string again = games.query();
            games.setQuery("");
            games.setQuery(again);
        }
        return;
    }
    if (games.results().empty()) {
        emptyState(ctx.tr.c("home.no_results"), ctx.tr.c("home.no_results_hint"));
        return;
    }
    mutedText(ctx.tr.plural("home.result_count", static_cast<long long>(games.results().size())).c_str());
    gameGrid(ctx, "results", games.results(), false);
}

void HomeScreen::drawSections(UiContext& ctx) {
    auto& games = *ctx.games;

    // Recent games
    sectionTitle(ctx.tr.c("home.recent"));
    if (games.recents().items().empty()) {
        mutedText(ctx.tr.c("home.recent_empty"));
    } else {
        std::vector<std::int64_t> ids;
        for (const auto& g : games.recents().items()) ids.push_back(g.universeId);
        gameGrid(ctx, "recent", ids, true);
    }

    // Favorites
    if (!games.favorites().items().empty()) {
        ImGui::Dummy({0, dp(8)});
        sectionTitle(ctx.tr.c("home.favorites"));
        std::vector<std::int64_t> ids;
        for (const auto& g : games.favorites().items()) ids.push_back(g.universeId);
        gameGrid(ctx, "favorites", ids, false);
    }

    // For You
    ImGui::Dummy({0, dp(8)});
    sectionTitle(ctx.tr.c("home.for_you"));
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + dp(4));
    if (!games.forYouLoading() && !games.forYou().empty()) {
        chip(ctx.tr.c(games.forYouPersonalized() ? "home.for_you_personal" : "home.for_you_popular"),
             games.forYouPersonalized() ? palette().accent : palette().success);
        ImGui::SameLine();
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - dp(8));
    if (iconButton("##refresh_foryou", Glyph::Refresh, ctx.tr.c("home.refresh"), !games.forYouLoading())) {
        games.loadHome(true);
    }

    if (games.forYouLoading()) {
        loadingRow(ctx.tr.c("home.loading"));
    } else if (games.forYouError() != ErrorCode::None) {
        wrappedText(ctx.tr.c(errorKey(games.forYouError())), palette().danger);
        if (button(ctx.tr.c("common.retry"), "##retry_home")) games.loadHome(true);
    } else if (games.forYou().empty()) {
        mutedText(ctx.tr.c("home.for_you_empty"));
    } else {
        gameGrid(ctx, "foryou", games.forYou(), false);
    }
}

}  // namespace rl::ui
