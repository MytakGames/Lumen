#include "ui/screens/AccountsScreen.h"

#include <cstring>
#include <ctime>

#include "accounts/AccountService.h"
#include "instances/InstanceManager.h"
#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/TextureCache.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace rl::ui {
namespace {

using accounts::SessionState;

void stateChip(const UiContext& ctx, const accounts::Account& a, SessionState s, bool inGame) {
    const Palette& p = palette();
    if (!a.enabled) return chip(ctx.tr.c("accounts.state.disabled"), p.textMuted);
    if (inGame) return chip(ctx.tr.c("accounts.state.in_game"), p.accent);
    switch (s) {
        case SessionState::Valid: return chip(ctx.tr.c("accounts.state.valid"), p.success);
        case SessionState::Checking: return chip(ctx.tr.c("accounts.state.checking"), p.textMuted);
        case SessionState::Expired: return chip(ctx.tr.c("accounts.state.expired"), p.danger);
        case SessionState::NeedsLogin: return chip(ctx.tr.c("accounts.state.needs_login"), p.warning);
        case SessionState::Error: return chip(ctx.tr.c("accounts.state.error"), p.warning);
        case SessionState::Unknown: return chip(ctx.tr.c("accounts.state.unknown"), p.textMuted);
    }
}

}  // namespace

void AccountsScreen::onShow(UiContext& ctx) { ctx.accounts->refreshAvatars(); }

void AccountsScreen::draw(UiContext& ctx) {
    pageHeader(ctx.tr.c("accounts.title"), ctx.tr.c("accounts.subtitle"));
    drawToolbar(ctx);
    ImGui::Dummy({0, dp(4)});

    auto& svc = *ctx.accounts;
    const auto& list = svc.accounts();
    if (list.empty()) {
        beginCard("##empty");
        emptyState(ctx.tr.c("accounts.empty_title"), ctx.tr.c("accounts.empty_body"));
        endCard();
    }

    // Copy ids first: actions below may reorder or remove entries.
    std::vector<std::string> ids;
    for (const auto& a : list) ids.push_back(a.id);
    const Palette& p = palette();

    for (size_t index = 0; index < ids.size(); ++index) {
        const accounts::Account* a = svc.find(ids[index]);
        if (!a) continue;
        const std::string id = a->id;
        ImGui::PushID(id.c_str());
        beginCard("##acc");

        const float rowStart = ImGui::GetCursorPosY();
        const float rowRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        const std::string avatarUrl = svc.avatarUrl(id);
        if (!a->enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
        avatar(ctx.textures->get(avatarUrl), a->label(), a->colorIndex, dp(46));
        if (!a->enabled) ImGui::PopStyleVar();

        ImGui::SameLine(0, dp(14));
        ImGui::BeginGroup();
        ImGui::PushFont(fonts().semibold);
        ImGui::TextUnformatted(a->label().c_str());
        ImGui::PopFont();
        ImGui::SameLine();
        const bool inGame = ctx.instances->accountActive(id);
        stateChip(ctx, *a, svc.state(id), inGame);

        ImGui::PushFont(fonts().small);
        std::string sub = a->username.empty() ? "" : "@" + a->username;
        if (!a->alias.empty() && !a->displayName.empty()) sub = a->displayName + "  \xC2\xB7  " + sub;
        if (a->lastUsedAt) {
            sub += (sub.empty() ? "" : "  \xC2\xB7  ") +
                   ctx.tr.format("accounts.last_used", {{"when", ctx.tr.formatRelative(a->lastUsedAt, std::time(nullptr))}});
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - dp(6));
        ImGui::TextColored(p.textMuted, "%s", sub.c_str());
        ImGui::PopFont();
        ImGui::EndGroup();

        // Right-aligned actions
        const float actionsW = dp(42) + dp(10) + 5 * (dp(32) + dp(4));
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), rowRight - actionsW));
        ImGui::SetCursorPosY(rowStart + dp(11));
        bool enabled = a->enabled;
        if (toggle("##enabled", &enabled)) svc.setEnabled(id, enabled);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ctx.tr.c(enabled ? "accounts.disable" : "accounts.enable"));
        ImGui::SameLine(0, dp(10));
        ImGui::SetCursorPosY(rowStart + dp(7));
        if (iconButton("##relogin", Glyph::Login, ctx.tr.c("accounts.relogin"), !ctx.loginInProgress())) {
            ctx.reloginAccount(id);
        }
        ImGui::SameLine(0, dp(4));
        if (iconButton("##edit", Glyph::Edit, ctx.tr.c("accounts.edit"))) {
            editing_ = id;
            std::strncpy(alias_.data(), a->alias.c_str(), alias_.size() - 1);
            alias_[alias_.size() - 1] = 0;
            color_ = a->colorIndex;
            openEdit_ = true;
        }
        ImGui::SameLine(0, dp(4));
        if (iconButton("##up", Glyph::Up, ctx.tr.c("accounts.move_up"), index > 0)) svc.move(id, -1);
        ImGui::SameLine(0, dp(4));
        if (iconButton("##down", Glyph::Down, ctx.tr.c("accounts.move_down"), index + 1 < ids.size())) svc.move(id, +1);
        ImGui::SameLine(0, dp(4));
        if (iconButton("##delete", Glyph::Trash, ctx.tr.c("accounts.delete"), true, true)) {
            auto* service = ctx.accounts;
            requestConfirm({ctx.tr.format("accounts.delete_confirm.title", {{"name", a->label()}}),
                            ctx.tr.tr("accounts.delete_confirm.body"), ctx.tr.tr("accounts.delete"),
                            ctx.tr.tr("common.cancel"), true, [service, id] { service->remove(id); }});
        }
        endCard();
        ImGui::PopID();
    }

    drawEditModal(ctx);
    ImGui::Dummy({0, dp(8)});
    ImGui::PushFont(fonts().small);
    mutedText(ctx.tr.c("accounts.security_note"));
    ImGui::PopFont();
    ImGui::Dummy({0, dp(24)});
}

void AccountsScreen::drawToolbar(UiContext& ctx) {
    const bool busy = ctx.loginInProgress();
    if (glyphButton(ctx.tr.c(busy ? "accounts.login_in_progress" : "accounts.add"), "##add", Glyph::Plus, true, !busy)) {
        ctx.addAccount();
    }
    ImGui::SameLine();
    if (glyphButton(ctx.tr.c("accounts.verify_all"), "##verify", Glyph::Refresh, false,
                    !ctx.accounts->accounts().empty())) {
        ctx.accounts->validateAll();
    }
    flowNext(ImGui::CalcTextSize(ctx.tr.c("accounts.import")).x + dp(56));
    if (glyphButton(ctx.tr.c("accounts.import"), "##import", Glyph::Import, false)) ctx.importAccounts();
    flowNext(ImGui::CalcTextSize(ctx.tr.c("accounts.export")).x + dp(56));
    if (glyphButton(ctx.tr.c("accounts.export"), "##export", Glyph::Export, false, !ctx.accounts->accounts().empty())) {
        ctx.exportAccounts();
    }
}

void AccountsScreen::drawEditModal(UiContext& ctx) {
    const std::string title = ctx.tr.tr("accounts.edit") + "###edit_account";
    if (openEdit_) {
        ImGui::OpenPopup(title.c_str());
        openEdit_ = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({dp(440), 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {dp(22), dp(18)});
    if (ImGui::BeginPopupModal(title.c_str(), nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        const accounts::Account* a = ctx.accounts->find(editing_);
        if (!a) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            ImGui::PopStyleVar();
            return;
        }
        ImGui::PushFont(fonts().semibold);
        ImGui::TextUnformatted(ctx.tr.c("accounts.edit"));
        ImGui::PopFont();
        mutedText(("@" + a->username).c_str());

        ImGui::TextUnformatted(ctx.tr.c("accounts.alias"));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##alias", a->displayName.c_str(), alias_.data(), alias_.size());

        ImGui::TextUnformatted(ctx.tr.c("accounts.color"));
        const Palette& p = palette();
        for (int i = -1; i < swatchCount(); ++i) {
            if (i > -1) ImGui::SameLine(0, dp(8));
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float s = dp(28);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##sw", {s, s})) color_ = i;
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 c(pos.x + s * 0.5f, pos.y + s * 0.5f);
            const ImVec4 col = i < 0 ? accentSwatch(swatchFor(a->label())) : accentSwatch(i);
            dl->AddCircleFilled(c, s * 0.42f, toU32(col), 24);
            if (i < 0) dl->AddText(fonts().small, fonts().small->FontSize, {c.x - dp(4), c.y - dp(8)}, IM_COL32_WHITE, "A");
            if (color_ == i || hovered) dl->AddCircle(c, s * 0.5f, toU32(color_ == i ? p.text : p.textMuted), 24, dp(2));
            if (hovered && i < 0) ImGui::SetTooltip("%s", ctx.tr.c("accounts.color_auto"));
        }
        ImGui::Dummy({0, dp(6)});
        if (button(ctx.tr.c("common.save"), "##save", ButtonKind::Primary)) {
            ctx.accounts->setAlias(editing_, alias_.data());
            ctx.accounts->setColor(editing_, color_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (button(ctx.tr.c("common.cancel"), "##cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

}  // namespace rl::ui
