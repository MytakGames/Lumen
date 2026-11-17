#pragma once

#include "imgui.h"
#include "settings/Settings.h"

namespace rl::ui {

struct Palette {
    bool dark = true;
    ImVec4 bg, sidebar, surface, surfaceHover, surfaceActive, border;
    ImVec4 text, textMuted;
    ImVec4 accent, accentHover, accentActive, onAccent;
    ImVec4 success, warning, danger;
};

const Palette& palette();

// Resolves Theme::System, rebuilds the ImGui style and scales it for `dpiScale`.
void applyTheme(settings::Theme theme, float dpiScale);

bool systemPrefersDark();

ImU32 toU32(const ImVec4& c, float alphaMul = 1.0f);
ImVec4 mix(const ImVec4& a, const ImVec4& b, float t);

}  // namespace rl::ui
