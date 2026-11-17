#include "ui/screens/InstancesScreen.h"

#include <algorithm>
#include <ctime>

#include "core/Error.h"
#include "instances/InstanceManager.h"
#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace rl::ui {
namespace {

using instances::InstanceStatus;

void statusChip(const UiContext& ctx, const instances::Instance& i) {
    const Palette& p = palette();
    switch (i.status) {
        case InstanceStatus::Launching: return chip(ctx.tr.c("instances.status.launching"), p.warning);
        case InstanceStatus::Running: return chip(ctx.tr.c("instances.status.running"), p.success);
        case InstanceStatus::Ended: return chip(ctx.tr.c("instances.status.ended"), p.textMuted);
        case InstanceStatus::Failed: return chip(ctx.tr.c("instances.status.failed"), p.danger);
    }
}

}  // namespace

void InstancesScreen::draw(UiContext& ctx) {
    auto& mgr = *ctx.instances;
    pageHeader(ctx.tr.c("instances.title"), ctx.tr.c("instances.subtitle"));

    const std::string summary = ctx.tr.format(
        "instances.summary", {{"active", std::to_string(mgr.activeCount())}, {"max", std::to_string(mgr.maxInstances())}});
    ImGui::PushFont(fonts().semibold);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(summary.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    if (!ctx.settings.multiInstanceEnabled) chip(ctx.tr.c("instances.single_mode"), palette().textMuted);

    const bool anyFinished = std::any_of(mgr.instances().begin(), mgr.instances().end(), [](const instances::Instance& i) {
        return i.status == InstanceStatus::Ended || i.status == InstanceStatus::Failed;
    });
    if (anyFinished) {
        ImGui::SameLine();
        if (button(ctx.tr.c("instances.clear_finished"), "##clear")) mgr.clearFinished();
    }
    ImGui::Dummy({0, dp(4)});

    if (mgr.instances().empty()) {
        beginCard("##empty");
        emptyState(ctx.tr.c("instances.empty_title"), ctx.tr.c("instances.empty_body"));
        endCard();
        return;
    }

    beginCard("##table");
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX;
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, palette().surfaceHover);
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0, 0, 0, 0));
    if (ImGui::BeginTable("##instances", 6, flags)) {
        ImGui::TableSetupColumn(ctx.tr.c("instances.col.account"), ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn(ctx.tr.c("instances.col.game"), ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn(ctx.tr.c("instances.col.status"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(ctx.tr.c("instances.col.started"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(ctx.tr.c("instances.col.pid"), ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, dp(40));
        ImGui::PushFont(fonts().small);
        ImGui::TableHeadersRow();
        ImGui::PopFont();

        const std::time_t now = std::time(nullptr);
        for (const auto& i : mgr.instances()) {
            ImGui::PushID(i.id);
            ImGui::TableNextRow(0, dp(44));
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(i.accountLabel.c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(ellipsize(i.gameName.empty() ? std::to_string(i.placeId) : i.gameName,
                                             ImGui::GetContentRegionAvail().x)
                                       .c_str());
            ImGui::TableNextColumn();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + dp(4));
            statusChip(ctx, i);
            if (i.status == InstanceStatus::Failed && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", ctx.tr.c(errorKey(i.error)));
            }
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(palette().textMuted, "%s", ctx.tr.formatRelative(i.startedAt, now).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(palette().textMuted, "%s", i.pid ? std::to_string(i.pid).c_str() : "\xE2\x80\x94");
            ImGui::TableNextColumn();
            if (i.status == InstanceStatus::Running) {
                const int id = i.id;
                auto* manager = ctx.instances;
                if (iconButton("##close", Glyph::Close, ctx.tr.c("instances.close"), true, true)) {
                    requestConfirm({ctx.tr.tr("instances.close_confirm.title"),
                                    ctx.tr.format("instances.close_confirm.body", {{"account", i.accountLabel}}),
                                    ctx.tr.tr("instances.close"), ctx.tr.tr("common.cancel"), true,
                                    [manager, id] { manager->close(id); }});
                }
            } else if (i.status == InstanceStatus::Launching) {
                spinner(dp(9), toU32(palette().accent));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(3);
    endCard();
    ImGui::Dummy({0, dp(24)});
}

}  // namespace rl::ui
