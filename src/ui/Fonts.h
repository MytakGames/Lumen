#pragma once

#include "imgui.h"

namespace rl::ui {

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* semibold = nullptr;
    ImFont* small = nullptr;
    ImFont* title = nullptr;
};

Fonts& fonts();

// (Re)builds the font atlas at the given DPI scale. Uses Segoe UI from the
// Windows fonts folder, falling back to ImGui's built-in font.
void loadFonts(float dpiScale);

}  // namespace rl::ui
