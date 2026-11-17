#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ui/Fonts.h"
#include "ui/Theme.h"

namespace rl::ui {
namespace {

bool g_animations = true;
bool g_redraw = false;
float g_scale = 1.0f;

struct Toast {
    std::string text;
    ToastKind kind;
    double created;
    float seconds;
};
std::vector<Toast> g_toasts;

constexpr float kPi = 3.14159265358979f;

ImVec4 toastColor(ToastKind k) {
    const Palette& p = palette();
    switch (k) {
        case ToastKind::Success: return p.success;
        case ToastKind::Warning: return p.warning;
        case ToastKind::Error: return p.danger;
        case ToastKind::Info: break;
    }
    return p.accent;
}

}  // namespace

// --- animation -------------------------------------------------------------

void setAnimationsEnabled(bool enabled) { g_animations = enabled; }
bool animationsEnabled() { return g_animations; }

float easeOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float animate(ImGuiID id, float target, float speed) {
    float* v = ImGui::GetStateStorage()->GetFloatRef(id, target);
    if (!g_animations) {
        *v = target;
        return target;
    }
    const float dt = ImGui::GetIO().DeltaTime;
    *v += (target - *v) * (1.0f - std::exp(-speed * dt));
    if (std::fabs(target - *v) < 0.002f) {
        *v = target;
    } else {
        g_redraw = true;
    }
    return *v;
}

void requestRedraw() { g_redraw = true; }

bool consumeRedrawRequest() {
    const bool r = g_redraw;
    g_redraw = false;
    return r;
}

// --- sizing ----------------------------------------------------------------

void setDpiScale(float scale) { g_scale = scale; }
float dp(float v) { return v * g_scale; }

// --- icons -----------------------------------------------------------------

void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float s, ImU32 col) {
    const float t = std::max(1.5f, s * 0.09f);
    switch (icon) {
        case Icon::Home: {
            const ImVec2 roof[] = {{c.x - s * 0.46f, c.y - s * 0.02f}, {c.x, c.y - s * 0.44f}, {c.x + s * 0.46f, c.y - s * 0.02f}};
            dl->AddPolyline(roof, 3, col, ImDrawFlags_None, t);
            const ImVec2 body[] = {{c.x - s * 0.32f, c.y - s * 0.14f}, {c.x - s * 0.32f, c.y + s * 0.42f},
                                   {c.x + s * 0.32f, c.y + s * 0.42f}, {c.x + s * 0.32f, c.y - s * 0.14f}};
            dl->AddPolyline(body, 4, col, ImDrawFlags_None, t);
            dl->AddRectFilled({c.x - s * 0.08f, c.y + s * 0.12f}, {c.x + s * 0.08f, c.y + s * 0.42f}, col, s * 0.03f);
            break;
        }
        case Icon::Accounts:
            dl->AddCircle({c.x, c.y - s * 0.17f}, s * 0.2f, col, 20, t);
            dl->PathArcTo({c.x, c.y + s * 0.48f}, s * 0.38f, kPi * 1.08f, kPi * 1.92f, 20);
            dl->PathStroke(col, ImDrawFlags_None, t);
            break;
        case Icon::Instances:
            dl->AddRect({c.x - s * 0.42f, c.y - s * 0.22f}, {c.x + s * 0.22f, c.y + s * 0.38f}, col, s * 0.1f, 0, t);
            dl->PathLineTo({c.x - s * 0.22f, c.y - s * 0.28f});
            dl->PathLineTo({c.x - s * 0.22f, c.y - s * 0.42f});
            dl->PathLineTo({c.x + s * 0.42f, c.y - s * 0.42f});
            dl->PathLineTo({c.x + s * 0.42f, c.y + s * 0.18f});
            dl->PathLineTo({c.x + s * 0.28f, c.y + s * 0.18f});
            dl->PathStroke(col, ImDrawFlags_None, t);
            break;
        case Icon::Settings: {
            dl->AddCircle(c, s * 0.29f, col, 24, t);
            dl->AddCircle(c, s * 0.1f, col, 16, t);
            for (int i = 0; i < 8; ++i) {
                const float a = static_cast<float>(i) * kPi / 4.0f;
                const ImVec2 dir(std::cos(a), std::sin(a));
                dl->AddLine({c.x + dir.x * s * 0.29f, c.y + dir.y * s * 0.29f},
                            {c.x + dir.x * s * 0.45f, c.y + dir.y * s * 0.45f}, col, t * 1.6f);
            }
            break;
        }
        case Icon::Logo: {
            const Palette& p = palette();
            dl->AddRectFilled({c.x - s * 0.5f, c.y - s * 0.5f}, {c.x + s * 0.5f, c.y + s * 0.5f}, toU32(p.accent),
                              s * 0.28f);
            dl->AddRectFilled({c.x - s * 0.5f, c.y - s * 0.5f}, {c.x + s * 0.5f, c.y}, toU32(p.accentHover, 0.35f),
                              s * 0.28f, ImDrawFlags_RoundCornersTop);
            dl->AddTriangleFilled({c.x - s * 0.13f, c.y - s * 0.2f}, {c.x - s * 0.13f, c.y + s * 0.2f},
                                  {c.x + s * 0.2f, c.y}, col);
            break;
        }
    }
}

// --- building blocks -------------------------------------------------------

bool navItem(const char* id, const char* label, Icon icon, bool selected) {
    const Palette& p = palette();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, dp(42));

    ImGui::PushID(id);
    const bool clicked = ImGui::InvisibleButton("##nav", size);
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiID uid = ImGui::GetItemID();
    ImGui::PopID();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    const float h = animate(uid + 1, hovered ? 1.0f : 0.0f);
    const float sel = animate(uid + 2, selected ? 1.0f : 0.0f);

    ImVec4 bg = p.surface;
    bg.w = std::max(h * 0.7f, sel);
    if (bg.w > 0.0f) dl->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, toU32(bg), dp(10));
    if (sel > 0.0f) {
        const float barH = size.y * 0.5f * sel;
        const float cy = pos.y + size.y * 0.5f;
        dl->AddRectFilled({pos.x, cy - barH * 0.5f}, {pos.x + dp(3), cy + barH * 0.5f}, toU32(p.accent), dp(2));
    }
    const ImVec4 fg = mix(p.textMuted, p.text, std::max(sel, h));
    const ImVec4 iconCol = mix(fg, p.accent, sel);
    drawIcon(dl, icon, {pos.x + dp(24), pos.y + size.y * 0.5f}, dp(18), toU32(iconCol));

    ImGui::PushFont(selected ? fonts().semibold : fonts().regular);
    const float textH = ImGui::GetFontSize();
    dl->AddText({pos.x + dp(46), pos.y + (size.y - textH) * 0.5f}, toU32(fg), label);
    ImGui::PopFont();
    return clicked;
}

void pageHeader(const char* title, const char* subtitle) {
    ImGui::PushFont(fonts().title);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (subtitle && *subtitle) mutedText(subtitle);
    ImGui::Dummy({0, dp(8)});
}

void sectionTitle(const char* text) {
    ImGui::Dummy({0, dp(6)});
    ImGui::PushFont(fonts().semibold);
    ImGui::PushStyleColor(ImGuiCol_Text, palette().textMuted);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void beginCard(const char* id) {
    const Palette& p = palette();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, p.border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {dp(20), dp(16)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {dp(10), dp(14)});
    ImGui::BeginChild(id, {0, 0},
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
}

void endCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

bool toggle(const char* id, bool* value) {
    const Palette& p = palette();
    const ImVec2 size(dp(42), dp(24));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    if (clicked) *value = !*value;
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const float t = animate(ImGui::GetItemID(), *value ? 1.0f : 0.0f, 18.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 track = mix(p.surfaceActive, hovered ? p.accentHover : p.accent, t);
    dl->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, toU32(track), size.y * 0.5f);
    const float r = size.y * 0.5f - dp(3);
    const float x = pos.x + size.y * 0.5f + t * (size.x - size.y);
    dl->AddCircleFilled({x, pos.y + size.y * 0.5f}, r, toU32(t > 0.5f ? p.onAccent : p.text), 24);
    return clicked;
}

float buttonWidth(const char* label) { return ImGui::CalcTextSize(label).x + dp(32); }

void flowNext(float nextWidth) {
    const float lastRight = ImGui::GetItemRectMax().x;
    // After an item the cursor sits at the start of the next line, so this is the row's right edge.
    const float limit = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (lastRight + ImGui::GetStyle().ItemSpacing.x + nextWidth <= limit) ImGui::SameLine();
}

bool button(const char* label, const char* id, ButtonKind kind, bool enabled) {
    const Palette& p = palette();
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 size(buttonWidth(label), dp(36));
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    ImGui::EndDisabled();
    if (hovered && enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const float h = animate(ImGui::GetItemID(), hovered && enabled ? 1.0f : 0.0f);

    ImVec4 base, hover, active, fg;
    switch (kind) {
        case ButtonKind::Primary:
            base = p.accent, hover = p.accentHover, active = p.accentActive, fg = p.onAccent;
            break;
        case ButtonKind::Danger:
            base = p.surfaceHover, hover = p.danger, active = p.danger, fg = h > 0.5f ? p.onAccent : p.danger;
            break;
        case ButtonKind::Secondary:
            base = p.surfaceHover, hover = p.surfaceActive, active = p.border, fg = p.text;
            break;
    }
    ImVec4 bg = held ? active : mix(base, hover, h);
    const float alpha = enabled ? 1.0f : 0.45f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float press = held ? dp(1) : 0.0f;  // tiny "pressed" inset
    dl->AddRectFilled({pos.x + press, pos.y + press}, {pos.x + size.x - press, pos.y + size.y - press},
                      toU32(bg, alpha), dp(8));
    dl->AddText({pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f}, toU32(fg, alpha),
                label);
    return clicked && enabled;
}

void settingRow(const char* label, const char* description, float controlWidth,
                const std::function<void()>& control) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = dp(24);
    const float textWidth = std::max(dp(120), avail - controlWidth - gap);
    const ImVec2 start = ImGui::GetCursorPos();

    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(start.x + textWidth);
    ImGui::PushFont(fonts().semibold);
    ImGui::TextUnformatted(label);
    ImGui::PopFont();
    if (description && *description) {
        ImGui::PushFont(fonts().small);
        ImGui::PushStyleColor(ImGuiCol_Text, palette().textMuted);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - dp(8));
        ImGui::TextUnformatted(description);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    const float textHeight = ImGui::GetItemRectSize().y;
    const float afterText = ImGui::GetCursorPosY();

    const float frameH = ImGui::GetFrameHeight();
    ImGui::SetCursorPos({start.x + avail - controlWidth, start.y + std::max(0.0f, (textHeight - frameH) * 0.5f)});
    ImGui::PushItemWidth(controlWidth);
    control();
    ImGui::PopItemWidth();
    const float afterControl = ImGui::GetCursorPosY();

    ImGui::SetCursorPos({start.x, std::max(afterText, afterControl)});
    ImGui::Dummy({0, 0});
}

void mutedText(const char* text) { wrappedText(text, palette().textMuted); }

void wrappedText(const char* text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

// --- toasts ----------------------------------------------------------------

void pushToast(std::string text, ToastKind kind, float seconds) {
    g_toasts.push_back({std::move(text), kind, ImGui::GetTime(), seconds});
    g_redraw = true;
}

void drawToasts() {
    const double now = ImGui::GetTime();
    g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
                                  [now](const Toast& t) { return now - t.created > t.seconds; }),
                   g_toasts.end());
    if (g_toasts.empty()) return;
    g_redraw = true;

    const Palette& p = palette();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = std::min(dp(380), display.x - dp(48));
    const float pad = dp(14);
    ImFont* font = fonts().regular;
    const float fontSize = font->FontSize;
    float y = display.y - dp(24);

    for (auto it = g_toasts.rbegin(); it != g_toasts.rend(); ++it) {
        const float age = static_cast<float>(now - it->created);
        const float fadeIn = easeOutCubic(age / 0.25f);
        const float fadeOut = std::clamp((it->seconds - age) / 0.4f, 0.0f, 1.0f);
        const float a = g_animations ? std::min(fadeIn, fadeOut) : 1.0f;
        const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, width - pad * 2 - dp(10), it->text.c_str());
        const float h = textSize.y + pad * 2;
        const float x = display.x - dp(24) - width + (1.0f - a) * dp(24);
        y -= h;
        dl->AddRectFilled({x + dp(2), y + dp(4)}, {x + width + dp(2), y + h + dp(4)}, IM_COL32(0, 0, 0, static_cast<int>(60 * a)), dp(12));
        dl->AddRectFilled({x, y}, {x + width, y + h}, toU32(p.surface, a), dp(12));
        dl->AddRect({x, y}, {x + width, y + h}, toU32(p.border, a), dp(12));
        dl->AddRectFilled({x, y}, {x + dp(4), y + h}, toU32(toastColor(it->kind), a), dp(12), ImDrawFlags_RoundCornersLeft);
        dl->AddText(font, fontSize, {x + pad + dp(6), y + pad}, toU32(p.text, a), it->text.c_str(), nullptr,
                    width - pad * 2 - dp(10));
        y -= dp(10);
    }
}

}  // namespace rl::ui
