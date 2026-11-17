#include "ui/LoginDialog.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "accounts/QuickLoginFlow.h"
#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace rl::ui {
namespace {

using Stage = accounts::QuickLoginFlow::Stage;

void stepLine(int n, const char* text) {
    const Palette& p = palette();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float r = dp(11);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 c(pos.x + r, pos.y + ImGui::GetFontSize() * 0.5f + dp(1));
    dl->AddCircleFilled(c, r, toU32(p.accent, 0.18f), 24);
    char num[4];
    std::snprintf(num, sizeof(num), "%d", n);
    ImGui::PushFont(fonts().semibold);
    const ImVec2 ts = ImGui::CalcTextSize(num);
    dl->AddText({c.x - ts.x * 0.5f, c.y - ts.y * 0.5f}, toU32(p.accent), num);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos({pos.x + r * 2 + dp(10), pos.y});
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}

// The code as large separate character tiles.
void codeTiles(const std::string& code) {
    const Palette& p = palette();
    ImGui::PushFont(fonts().title);
    const float tileW = dp(44), tileH = dp(56), gap = dp(8);
    const float total = static_cast<float>(code.size()) * tileW + static_cast<float>(code.size() > 0 ? code.size() - 1 : 0) * gap;
    const float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - total) * 0.5f));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < code.size(); ++i) {
        const float x = pos.x + static_cast<float>(i) * (tileW + gap);
        dl->AddRectFilled({x, pos.y}, {x + tileW, pos.y + tileH}, toU32(p.surfaceHover), dp(10));
        dl->AddRect({x, pos.y}, {x + tileW, pos.y + tileH}, toU32(p.accent, 0.6f), dp(10), 0, dp(1.5f));
        const char ch[2] = {code[i], 0};
        const ImVec2 ts = ImGui::CalcTextSize(ch);
        dl->AddText({x + (tileW - ts.x) * 0.5f, pos.y + (tileH - ts.y) * 0.5f}, toU32(p.text), ch);
    }
    ImGui::Dummy({total, tileH});
    ImGui::PopFont();
}

}  // namespace

void drawLoginDialog(UiContext& ctx) {
    accounts::QuickLoginFlow* flow = ctx.login;
    constexpr const char* kId = "###login_dialog";
    static bool open = false;
    if (!flow || !flow->visible()) {
        open = false;
        return;
    }
    if (!open) {
        ImGui::OpenPopup(kId);
        open = true;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({dp(500), 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {dp(26), dp(22)});
    if (!ImGui::BeginPopupModal(kId, nullptr,
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::PopStyleVar();
        open = false;
        return;
    }
    const Palette& p = palette();
    const Stage stage = flow->stage();

    ImGui::PushFont(fonts().title);
    ImGui::TextUnformatted(ctx.tr.c(flow->expectedAccountId().empty() ? "login.title" : "login.title_refresh"));
    ImGui::PopFont();
    ImGui::Dummy({0, dp(2)});

    bool close = false;
    if (stage == Stage::Requesting) {
        loadingRow(ctx.tr.c("login.requesting"));
        ImGui::Dummy({0, dp(6)});
        if (button(ctx.tr.c("common.cancel"), "##login_cancel0")) {
            flow->cancel();
            close = true;
        }
    } else if (stage == Stage::Failed) {
        wrappedText(ctx.tr.c(errorKey(flow->error())), p.danger);
        ImGui::Dummy({0, dp(6)});
        if (button(ctx.tr.c("common.retry"), "##login_retry", ButtonKind::Primary)) {
            const std::string expected = flow->expectedAccountId();
            flow->dismiss();
            ctx.reloginAccount(expected);  // empty id = add a new account
        }
        ImGui::SameLine();
        if (button(ctx.tr.c("login.close"), "##login_close")) {
            flow->dismiss();
            close = true;
        }
    } else {
        stepLine(1, ctx.tr.c("login.step1"));
        ImGui::Dummy({0, dp(2)});
        stepLine(2, ctx.tr.c("login.step2"));
        ImGui::Dummy({0, dp(6)});
        codeTiles(flow->code());
        ImGui::Dummy({0, dp(2)});
        {
            const char* copyLabel = ctx.tr.c("login.copy");
            const float w = buttonWidth(copyLabel);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - w) * 0.5f));
            if (button(copyLabel, "##login_copy")) {
                ImGui::SetClipboardText(flow->code().c_str());
                pushToast(ctx.tr.tr("login.copied"), ToastKind::Info, 2.5f);
            }
        }
        ImGui::Dummy({0, dp(4)});
        stepLine(3, ctx.tr.c("login.step3"));
        ImGui::Dummy({0, dp(8)});

        // Live status
        const char* key = stage == Stage::WaitingCode      ? "login.waiting_code"
                          : stage == Stage::WaitingConfirm ? "login.waiting_confirm"
                                                           : "login.finishing";
        const std::string status = ctx.tr.format(key, {{"name", flow->accountName()}});
        loadingRow(status.c_str());

        const long long left = std::max<long long>(0, static_cast<long long>(flow->expires() - std::time(nullptr)));
        char mmss[16];
        std::snprintf(mmss, sizeof(mmss), "%lld:%02lld", left / 60, left % 60);
        ImGui::PushFont(fonts().small);
        mutedText(ctx.tr.format("login.expires", {{"time", mmss}}).c_str());
        ImGui::PopFont();
        ImGui::Dummy({0, dp(8)});

        if (glyphButton(ctx.tr.c("login.open_browser"), "##login_browser", Glyph::Export, true,
                        stage != Stage::Finishing)) {
            flow->reopenBrowser();
        }
        ImGui::SameLine();
        if (button(ctx.tr.c("common.cancel"), "##login_cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            flow->cancel();
            close = true;
        }
        ImGui::Dummy({0, dp(4)});
        ImGui::PushFont(fonts().small);
        mutedText(ctx.tr.c("login.privacy"));
        ImGui::PopFont();
        requestRedraw();  // countdown + status updates
    }

    if (close || !flow->visible()) {
        ImGui::CloseCurrentPopup();
        open = false;
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}

}  // namespace rl::ui
