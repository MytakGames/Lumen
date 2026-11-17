#include "ui/screens/SettingsScreen.h"

#include <string>

#include "ui/Components.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace rl::ui {
namespace {

using settings::Theme;
namespace lim = settings::limits;

constexpr float kComboWidth = 220.0f;
constexpr float kSliderWidth = 260.0f;

const char* themeKey(Theme t) {
    switch (t) {
        case Theme::Dark: return "settings.theme.dark";
        case Theme::Light: return "settings.theme.light";
        case Theme::System: return "settings.theme.system";
    }
    return "settings.theme.dark";
}

bool sliderRow(UiContext& ctx, const char* key, const char* id, int* value, int min, int max) {
    bool changed = false;
    const std::string base(key);
    settingRow(ctx.tr.c(base + ".label"), ctx.tr.c(base + ".desc"), dp(kSliderWidth), [&] {
        changed = ImGui::SliderInt(id, value, min, max, ctx.tr.c(base + ".format"), ImGuiSliderFlags_AlwaysClamp);
    });
    return changed;
}

bool toggleRow(UiContext& ctx, const char* key, const char* id, bool* value) {
    bool changed = false;
    const std::string base(key);
    settingRow(ctx.tr.c(base + ".label"), ctx.tr.c(base + ".desc"), dp(42), [&] { changed = toggle(id, value); });
    return changed;
}

}  // namespace

void SettingsScreen::draw(UiContext& ctx) {
    pageHeader(ctx.tr.c("settings.title"), ctx.tr.c("settings.subtitle"));

    bool changed = false;
    changed |= drawGeneral(ctx);
    changed |= drawStartup(ctx);
    changed |= drawInstances(ctx);
    changed |= drawData(ctx);
    drawAbout(ctx);
    drawResetModal(ctx);
    ImGui::Dummy({0, dp(24)});

    if (changed) {
        ctx.settings.clamp();
        ctx.settingsChanged();
    }
}

bool SettingsScreen::drawGeneral(UiContext& ctx) {
    auto& s = ctx.settings;
    bool changed = false;

    sectionTitle(ctx.tr.c("settings.section.general"));
    beginCard("##general");
    settingRow(ctx.tr.c("settings.language.label"), ctx.tr.c("settings.language.desc"), dp(kComboWidth), [&] {
        const std::string currentName = ctx.tr.languageDisplayName(ctx.tr.language());
        if (ImGui::BeginCombo("##language", currentName.c_str())) {
            for (const auto& code : ctx.tr.availableLanguages()) {
                const bool selected = code == ctx.tr.language();
                if (ImGui::Selectable(ctx.tr.languageDisplayName(code).c_str(), selected) && !selected) {
                    s.language = code;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
    });
    endCard();

    sectionTitle(ctx.tr.c("settings.section.appearance"));
    beginCard("##appearance");
    settingRow(ctx.tr.c("settings.theme.label"), ctx.tr.c("settings.theme.desc"), dp(kComboWidth), [&] {
        if (ImGui::BeginCombo("##theme", ctx.tr.c(themeKey(s.theme)))) {
            for (Theme t : {Theme::Dark, Theme::Light, Theme::System}) {
                if (ImGui::Selectable(ctx.tr.c(themeKey(t)), s.theme == t) && s.theme != t) {
                    s.theme = t;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
    });
    changed |= toggleRow(ctx, "settings.animations", "##animations", &s.animations);
    endCard();
    return changed;
}

bool SettingsScreen::drawStartup(UiContext& ctx) {
    auto& s = ctx.settings;
    bool changed = false;
    sectionTitle(ctx.tr.c("settings.section.startup"));
    beginCard("##startup");
    changed |= toggleRow(ctx, "settings.startup.restore", "##restore", &s.restoreLastScreen);
    changed |= toggleRow(ctx, "settings.startup.autostart", "##autostart", &s.launchWithWindows);
    changed |= toggleRow(ctx, "settings.startup.minimized", "##minimized", &s.startMinimizedToTray);
    changed |= toggleRow(ctx, "settings.startup.close_to_tray", "##closetray", &s.closeToTray);
    endCard();
    return changed;
}

bool SettingsScreen::drawInstances(UiContext& ctx) {
    auto& s = ctx.settings;
    bool changed = false;
    sectionTitle(ctx.tr.c("settings.section.instances"));
    beginCard("##instances");
    changed |= toggleRow(ctx, "settings.instances.multi", "##multi", &s.multiInstanceEnabled);
    if (s.multiInstanceEnabled) {
        ImGui::PushFont(fonts().small);
        wrappedText(ctx.tr.c("settings.instances.multi.warning"), palette().warning);
        ImGui::PopFont();
    }
    ImGui::BeginDisabled(!s.multiInstanceEnabled);
    changed |= sliderRow(ctx, "settings.instances.max", "##maxinst", &s.maxInstances, lim::kMaxInstancesMin,
                         lim::kMaxInstancesMax);
    ImGui::EndDisabled();
    changed |= sliderRow(ctx, "settings.instances.delay", "##delay", &s.instanceLaunchDelayMs, lim::kLaunchDelayMin,
                         lim::kLaunchDelayMax);
    endCard();
    return changed;
}

bool SettingsScreen::drawData(UiContext& ctx) {
    auto& s = ctx.settings;
    bool changed = false;
    sectionTitle(ctx.tr.c("settings.section.data"));
    beginCard("##data");
    changed |= sliderRow(ctx, "settings.data.thumbs", "##thumbs", &s.thumbnailCacheMaxMb, lim::kThumbCacheMin,
                         lim::kThumbCacheMax);
    changed |= sliderRow(ctx, "settings.data.recent", "##recent", &s.recentGamesMax, lim::kRecentMin, lim::kRecentMax);
    changed |= sliderRow(ctx, "settings.data.api_ttl", "##apittl", &s.apiCacheTtlMinutes, lim::kApiTtlMin,
                         lim::kApiTtlMax);

    if (button(ctx.tr.c("settings.data.open_folder"), "##openfolder")) ctx.openDataFolder();
    flowNext(buttonWidth(ctx.tr.c("settings.data.clear_cache")));
    if (button(ctx.tr.c("settings.data.clear_cache"), "##clearcache")) ctx.clearCache();
    flowNext(buttonWidth(ctx.tr.c("settings.data.clear_history")));
    if (button(ctx.tr.c("settings.data.clear_history"), "##clearhistory")) {
        requestConfirm({ctx.tr.tr("settings.data.history_confirm.title"), ctx.tr.tr("settings.data.history_confirm.body"),
                        ctx.tr.tr("settings.data.clear_history"), ctx.tr.tr("common.cancel"), true, ctx.clearHistory});
    }
    flowNext(buttonWidth(ctx.tr.c("settings.data.reset")));
    if (button(ctx.tr.c("settings.data.reset"), "##reset", ButtonKind::Danger)) openResetModal_ = true;
    endCard();
    return changed;
}

void SettingsScreen::drawAbout(UiContext& ctx) {
    sectionTitle(ctx.tr.c("settings.section.about"));
    beginCard("##about");
    settingRow(ctx.tr.c("settings.about.version"), nullptr, dp(kComboWidth), [] {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(RL_VERSION);
    });
    settingRow(ctx.tr.c("settings.about.license"), nullptr, dp(kComboWidth), [&] {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(ctx.tr.c("settings.about.license_value"));
    });
    if (ctx.openReadme && button(ctx.tr.c("settings.about.readme"), "##readme")) ctx.openReadme();
    ImGui::PushFont(fonts().small);
    mutedText(ctx.tr.c("settings.about.disclaimer"));
    ImGui::PopFont();
    endCard();
}

void SettingsScreen::drawResetModal(UiContext& ctx) {
    if (!openResetModal_) return;
    openResetModal_ = false;
    requestConfirm({ctx.tr.tr("settings.data.reset_confirm.title"), ctx.tr.tr("settings.data.reset_confirm.body"),
                    ctx.tr.tr("settings.data.reset"), ctx.tr.tr("common.cancel"), true, ctx.resetAll});
}

}  // namespace rl::ui
