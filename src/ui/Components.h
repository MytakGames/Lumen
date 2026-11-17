#pragma once

#include <functional>
#include <string>

#include "imgui.h"

// Higher-level visual components built from Widgets. Presentation only.
namespace rl::ui {

enum class Glyph { Search, Back, Up, Down, Edit, Trash, Star, StarFilled, Close, Play, Refresh, Plus, Import, Export, Login };
void drawGlyph(ImDrawList* dl, Glyph g, ImVec2 center, float size, ImU32 color);

// Small square button with a vector glyph and an optional tooltip.
bool iconButton(const char* id, Glyph glyph, const char* tooltip, bool enabled = true, bool danger = false);
// Regular button with a leading glyph.
bool glyphButton(const char* label, const char* id, Glyph glyph, bool primary, bool enabled = true);

void spinner(float radius, ImU32 color);
void loadingRow(const char* text);  // spinner + muted text

// Rounded image, or an animated placeholder when `tex` is null.
void imageOrPlaceholder(ImTextureID tex, ImVec2 min, ImVec2 max, float rounding, ImDrawFlags corners = ImDrawFlags_RoundCornersAll);

struct CardInfo {
    ImTextureID image{};
    const char* title = "";
    const char* subtitle = "";
    const char* meta = "";
    bool favorite = false;
};
// Game tile (square artwork + texts). Returns true when clicked.
bool gameCard(const char* id, const CardInfo& info, float width);
float gameCardHeight(float width);

// Responsive grid: call begin, then for each item `gridItem()` before drawing it.
class Grid {
public:
    Grid(float minItemWidth, float spacing);
    float itemWidth() const { return itemWidth_; }
    void next();  // positions the cursor for the next item

private:
    int columns_ = 1;
    int index_ = 0;
    float itemWidth_ = 0, spacing_ = 0, startX_ = 0;
};

void avatar(ImTextureID tex, const std::string& label, int colorIndex, float size);
ImVec4 accentSwatch(int index);  // 8 swatches; negative = derived from a hash of `label`
int swatchCount();
int swatchFor(const std::string& seed);

void chip(const char* text, const ImVec4& color);

// Truncates `text` with an ellipsis to fit `maxWidth` using the current font.
std::string ellipsize(const std::string& text, float maxWidth);

// Formats 1234567 as "1.2M" / "1,2 Mln" style compact numbers.
std::string compactNumber(long long value, bool italian);

// --- modal confirmation -------------------------------------------------------
struct ConfirmRequest {
    std::string title;
    std::string body;
    std::string confirmLabel;
    std::string cancelLabel;
    bool danger = false;
    std::function<void()> onConfirm;
};
void requestConfirm(ConfirmRequest req);
void drawConfirm();

// --- empty state ---------------------------------------------------------------
void emptyState(const char* title, const char* body);

}  // namespace rl::ui
