#include "ui/Components.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>

#include "core/StringUtil.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace rl::ui {
namespace {
constexpr float kPi = 3.14159265358979f;
std::optional<ConfirmRequest> g_confirm;
bool g_confirmOpen = false;
}  // namespace

// --- glyphs -----------------------------------------------------------------------

void drawGlyph(ImDrawList* dl, Glyph g, ImVec2 c, float s, ImU32 col) {
    const float t = std::max(1.4f, s * 0.1f);
    auto line = [&](float x0, float y0, float x1, float y1) {
        dl->AddLine({c.x + x0 * s, c.y + y0 * s}, {c.x + x1 * s, c.y + y1 * s}, col, t);
    };
    auto poly = [&](std::initializer_list<ImVec2> pts, bool closed) {
        for (const auto& p : pts) dl->PathLineTo({c.x + p.x * s, c.y + p.y * s});
        dl->PathStroke(col, closed ? ImDrawFlags_Closed : ImDrawFlags_None, t);
    };
    switch (g) {
        case Glyph::Search:
            dl->AddCircle({c.x - 0.08f * s, c.y - 0.08f * s}, 0.28f * s, col, 20, t);
            line(0.13f, 0.13f, 0.42f, 0.42f);
            break;
        case Glyph::Back: poly({{0.15f, -0.35f}, {-0.2f, 0.0f}, {0.15f, 0.35f}}, false); break;
        case Glyph::Up: poly({{-0.32f, 0.15f}, {0.0f, -0.18f}, {0.32f, 0.15f}}, false); break;
        case Glyph::Down: poly({{-0.32f, -0.15f}, {0.0f, 0.18f}, {0.32f, -0.15f}}, false); break;
        case Glyph::Edit:
            poly({{-0.35f, 0.35f}, {-0.35f, 0.15f}, {0.18f, -0.38f}, {0.38f, -0.18f}, {-0.15f, 0.35f}}, true);
            break;
        case Glyph::Trash:
            line(-0.38f, -0.25f, 0.38f, -0.25f);
            line(-0.12f, -0.38f, 0.12f, -0.38f);
            poly({{-0.28f, -0.2f}, {-0.22f, 0.4f}, {0.22f, 0.4f}, {0.28f, -0.2f}}, false);
            break;
        case Glyph::Star:
        case Glyph::StarFilled: {
            ImVec2 pts[10];
            for (int i = 0; i < 10; ++i) {
                const float r = (i % 2 == 0) ? 0.45f * s : 0.19f * s;
                const float a = -kPi / 2 + static_cast<float>(i) * kPi / 5;
                pts[i] = {c.x + std::cos(a) * r, c.y + 0.04f * s + std::sin(a) * r};
            }
            if (g == Glyph::StarFilled) {
                const ImVec2 center(c.x, c.y + 0.04f * s);
                for (int i = 0; i < 10; ++i) dl->AddTriangleFilled(center, pts[i], pts[(i + 1) % 10], col);
            }
            dl->AddPolyline(pts, 10, col, ImDrawFlags_Closed, t);
            break;
        }
        case Glyph::Close:
            line(-0.3f, -0.3f, 0.3f, 0.3f);
            line(-0.3f, 0.3f, 0.3f, -0.3f);
            break;
        case Glyph::Play:
            dl->AddTriangleFilled({c.x - 0.25f * s, c.y - 0.35f * s}, {c.x - 0.25f * s, c.y + 0.35f * s},
                                  {c.x + 0.35f * s, c.y}, col);
            break;
        case Glyph::Refresh:
            dl->PathArcTo(c, 0.32f * s, -kPi * 0.35f, kPi * 1.35f, 24);
            dl->PathStroke(col, ImDrawFlags_None, t);
            poly({{0.08f, -0.42f}, {0.3f, -0.3f}, {0.12f, -0.12f}}, false);
            break;
        case Glyph::Plus:
            line(0.0f, -0.35f, 0.0f, 0.35f);
            line(-0.35f, 0.0f, 0.35f, 0.0f);
            break;
        case Glyph::Import:
            line(0.0f, -0.4f, 0.0f, 0.15f);
            poly({{-0.2f, -0.05f}, {0.0f, 0.15f}, {0.2f, -0.05f}}, false);
            poly({{-0.38f, 0.1f}, {-0.38f, 0.38f}, {0.38f, 0.38f}, {0.38f, 0.1f}}, false);
            break;
        case Glyph::Export:
            line(0.0f, 0.15f, 0.0f, -0.4f);
            poly({{-0.2f, -0.2f}, {0.0f, -0.4f}, {0.2f, -0.2f}}, false);
            poly({{-0.38f, 0.1f}, {-0.38f, 0.38f}, {0.38f, 0.38f}, {0.38f, 0.1f}}, false);
            break;
        case Glyph::Login:
            poly({{0.0f, -0.4f}, {0.38f, -0.4f}, {0.38f, 0.4f}, {0.0f, 0.4f}}, false);
            line(-0.4f, 0.0f, 0.15f, 0.0f);
            poly({{-0.05f, -0.2f}, {0.15f, 0.0f}, {-0.05f, 0.2f}}, false);
            break;
    }
}

bool iconButton(const char* id, Glyph glyph, const char* tooltip, bool enabled, bool danger) {
    const Palette& p = palette();
    const float sz = dp(32);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, {sz, sz});
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    const bool held = ImGui::IsItemActive();
    ImGui::EndDisabled();
    const float h = animate(ImGui::GetItemID(), hovered && enabled ? 1.0f : 0.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 bg = held ? p.border : p.surfaceActive;
    if (h > 0.0f) dl->AddRectFilled(pos, {pos.x + sz, pos.y + sz}, toU32(bg, h * (enabled ? 1.0f : 0.0f)), dp(8));
    const ImVec4 fg = danger && h > 0.0f ? mix(p.textMuted, p.danger, h) : mix(p.textMuted, p.text, h);
    drawGlyph(dl, glyph, {pos.x + sz * 0.5f, pos.y + sz * 0.5f}, dp(16), toU32(fg, enabled ? 1.0f : 0.4f));
    if (hovered && enabled) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (tooltip && *tooltip) ImGui::SetTooltip("%s", tooltip);
    }
    return clicked && enabled;
}

bool glyphButton(const char* label, const char* id, Glyph glyph, bool primary, bool enabled) {
    const Palette& p = palette();
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 size(textSize.x + dp(56), dp(38));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    ImGui::EndDisabled();
    if (hovered && enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const float h = animate(ImGui::GetItemID(), hovered && enabled ? 1.0f : 0.0f);
    const ImVec4 bg = primary ? (held ? p.accentActive : mix(p.accent, p.accentHover, h))
                              : (held ? p.border : mix(p.surfaceHover, p.surfaceActive, h));
    const ImVec4 fg = primary ? p.onAccent : p.text;
    const float a = enabled ? 1.0f : 0.45f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, toU32(bg, a), dp(9));
    drawGlyph(dl, glyph, {pos.x + dp(22), pos.y + size.y * 0.5f}, dp(15), toU32(fg, a));
    dl->AddText({pos.x + dp(38), pos.y + (size.y - textSize.y) * 0.5f}, toU32(fg, a), label);
    return clicked && enabled;
}

// --- loading ---------------------------------------------------------------------------

void spinner(float radius, ImU32 color) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy({radius * 2, radius * 2});
    const float t = static_cast<float>(ImGui::GetTime());
    const ImVec2 c(pos.x + radius, pos.y + radius);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float start = t * 6.0f;
    dl->PathArcTo(c, radius - dp(1.5f), start, start + kPi * 1.3f, 24);
    dl->PathStroke(color, ImDrawFlags_None, dp(2.5f));
    requestRedraw();
}

void loadingRow(const char* text) {
    spinner(dp(9), toU32(palette().accent));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - dp(6));
    ImGui::TextColored(palette().textMuted, "%s", text);
}

void imageOrPlaceholder(ImTextureID tex, ImVec2 min, ImVec2 max, float rounding, ImDrawFlags corners) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const Palette& p = palette();
    if (tex) {
        dl->AddImageRounded(tex, min, max, {0, 0}, {1, 1}, IM_COL32_WHITE, rounding, corners);
        return;
    }
    // Soft pulsing placeholder.
    const float pulse = animationsEnabled() ? 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) : 0.5f;
    dl->AddRectFilled(min, max, toU32(mix(p.surfaceHover, p.surfaceActive, pulse)), rounding, corners);
    if (animationsEnabled()) requestRedraw();
}

// --- game card ---------------------------------------------------------------------------

float gameCardHeight(float width) { return width + dp(78); }

bool gameCard(const char* id, const CardInfo& info, float width) {
    const Palette& p = palette();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(width, gameCardHeight(width));
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const float h = animate(ImGui::GetItemID(), hovered ? 1.0f : 0.0f);
    const float lift = h * dp(3);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImVec2 min(pos.x, pos.y - lift);
    const ImVec2 max(pos.x + size.x, pos.y + size.y - lift);
    const float r = dp(12);
    if (h > 0.0f) dl->AddRectFilled({min.x, min.y + dp(6)}, {max.x, max.y + dp(6)}, IM_COL32(0, 0, 0, static_cast<int>(50 * h)), r);
    dl->AddRectFilled(min, max, toU32(mix(p.surface, p.surfaceHover, h)), r);
    dl->AddRect(min, max, toU32(mix(p.border, p.accent, h * 0.6f)), r);
    imageOrPlaceholder(info.image, {min.x + 1, min.y + 1}, {max.x - 1, min.y + width}, r, ImDrawFlags_RoundCornersTop);

    if (info.favorite) {
        const ImVec2 c(max.x - dp(18), min.y + dp(18));
        dl->AddCircleFilled(c, dp(13), IM_COL32(0, 0, 0, 120));
        drawGlyph(dl, Glyph::StarFilled, c, dp(14), toU32(p.warning));
    }

    const float pad = dp(12);
    const float textW = width - pad * 2;
    float y = min.y + width + dp(8);
    ImGui::PushFont(fonts().semibold);
    const std::string title = ellipsize(info.title, textW);
    dl->AddText({min.x + pad, y}, toU32(p.text), title.c_str());
    y += ImGui::GetFontSize() + dp(2);
    ImGui::PopFont();
    ImGui::PushFont(fonts().small);
    const std::string sub = ellipsize(info.subtitle, textW);
    dl->AddText({min.x + pad, y}, toU32(p.textMuted), sub.c_str());
    y += ImGui::GetFontSize() + dp(3);
    const std::string meta = ellipsize(info.meta, textW);
    dl->AddText({min.x + pad, y}, toU32(mix(p.textMuted, p.accent, 0.5f)), meta.c_str());
    ImGui::PopFont();
    return clicked;
}

Grid::Grid(float minItemWidth, float spacing) : spacing_(spacing) {
    const float avail = ImGui::GetContentRegionAvail().x;
    columns_ = std::max(1, static_cast<int>((avail + spacing) / (minItemWidth + spacing)));
    itemWidth_ = (avail - spacing * static_cast<float>(columns_ - 1)) / static_cast<float>(columns_);
    startX_ = ImGui::GetCursorPosX();
}

void Grid::next() {
    if (index_ % columns_ != 0) {
        ImGui::SameLine(0, spacing_);
    } else if (index_ > 0) {
        ImGui::SetCursorPosX(startX_);
        ImGui::Dummy({0, spacing_ - ImGui::GetStyle().ItemSpacing.y});
    }
    ++index_;
}

// --- avatar ---------------------------------------------------------------------------

int swatchCount() { return 8; }

ImVec4 accentSwatch(int index) {
    static const ImVec4 swatches[] = {
        {0.24f, 0.48f, 1.00f, 1}, {0.55f, 0.36f, 0.96f, 1}, {0.93f, 0.33f, 0.60f, 1}, {0.95f, 0.36f, 0.36f, 1},
        {0.97f, 0.58f, 0.20f, 1}, {0.93f, 0.76f, 0.18f, 1}, {0.17f, 0.77f, 0.54f, 1}, {0.13f, 0.70f, 0.84f, 1},
    };
    const int n = swatchCount();
    return swatches[((index % n) + n) % n];
}

int swatchFor(const std::string& seed) { return static_cast<int>(str::fnv1a(seed) % static_cast<std::uint64_t>(swatchCount())); }

void avatar(ImTextureID tex, const std::string& label, int colorIndex, float size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy({size, size});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec4 color = accentSwatch(colorIndex >= 0 ? colorIndex : swatchFor(label));
    const ImVec2 c(pos.x + size * 0.5f, pos.y + size * 0.5f);
    dl->AddCircleFilled(c, size * 0.5f, toU32(color), 40);
    if (tex) {
        dl->AddImageRounded(tex, {pos.x + dp(2), pos.y + dp(2)}, {pos.x + size - dp(2), pos.y + size - dp(2)}, {0, 0},
                            {1, 1}, IM_COL32_WHITE, size * 0.5f);
    } else {
        // Initial letter (ASCII only; other scripts fall back to "?").
        char initial[2] = {label.empty() ? '?' : label[0], 0};
        if (initial[0] >= 'a' && initial[0] <= 'z') initial[0] = static_cast<char>(initial[0] - 'a' + 'A');
        if (static_cast<unsigned char>(initial[0]) > 127) initial[0] = '?';
        ImGui::PushFont(fonts().title);
        const float fs = size * 0.45f;
        const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, initial);
        dl->AddText(ImGui::GetFont(), fs, {c.x - ts.x * 0.5f, c.y - ts.y * 0.5f}, IM_COL32_WHITE, initial);
        ImGui::PopFont();
    }
}

void chip(const char* text, const ImVec4& color) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushFont(fonts().small);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const ImVec2 size(ts.x + dp(16), ts.y + dp(6));
    ImGui::Dummy(size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, toU32(color, 0.16f), size.y * 0.5f);
    dl->AddText({pos.x + dp(8), pos.y + dp(3)}, toU32(color), text);
    ImGui::PopFont();
}

std::string ellipsize(const std::string& text, float maxWidth) {
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;
    static const std::string dots = "\xE2\x80\xA6";  // "…"
    std::string s = text;
    while (!s.empty() && ImGui::CalcTextSize((s + dots).c_str()).x > maxWidth) {
        s.pop_back();
        while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();  // UTF-8 boundary
        if (!s.empty() && static_cast<unsigned char>(s.back()) >= 0xC0) s.pop_back();
    }
    return s + dots;
}

std::string compactNumber(long long v, bool italian) {
    char buf[32];
    const char* sep = italian ? "," : ".";
    auto fmt = [&](double x, const char* suffix) {
        if (x >= 100) {
            std::snprintf(buf, sizeof(buf), "%.0f%s", x, suffix);
        } else {
            std::snprintf(buf, sizeof(buf), "%.1f%s", x, suffix);
            std::string s(buf);
            const auto dot = s.find('.');
            if (dot != std::string::npos) s.replace(dot, 1, sep);
            if (s.find(std::string(sep) + "0") != std::string::npos) s.erase(s.find(std::string(sep) + "0"), 2);
            return s;
        }
        return std::string(buf);
    };
    const double d = static_cast<double>(v);
    if (v >= 1000000000) return fmt(d / 1e9, italian ? " Mrd" : "B");
    if (v >= 1000000) return fmt(d / 1e6, italian ? " Mln" : "M");
    if (v >= 10000) return fmt(d / 1e3, "K");
    std::snprintf(buf, sizeof(buf), "%lld", v);
    return buf;
}

// --- confirm dialog -----------------------------------------------------------------------

void requestConfirm(ConfirmRequest req) {
    g_confirm = std::move(req);
    g_confirmOpen = false;
    requestRedraw();
}

void drawConfirm() {
    if (!g_confirm) return;
    constexpr const char* kId = "###confirm_dialog";
    if (!g_confirmOpen) {
        ImGui::OpenPopup(kId);
        g_confirmOpen = true;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({dp(440), 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {dp(22), dp(18)});
    if (ImGui::BeginPopupModal(kId, nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::PushFont(fonts().semibold);
        ImGui::TextUnformatted(g_confirm->title.c_str());
        ImGui::PopFont();
        mutedText(g_confirm->body.c_str());
        ImGui::Dummy({0, dp(6)});
        bool close = false;
        if (button(g_confirm->confirmLabel.c_str(), "##confirm_yes",
                   g_confirm->danger ? ButtonKind::Danger : ButtonKind::Primary)) {
            auto action = g_confirm->onConfirm;
            close = true;
            if (action) action();
        }
        ImGui::SameLine();
        if (button(g_confirm->cancelLabel.c_str(), "##confirm_no") || ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;
        if (close) {
            ImGui::CloseCurrentPopup();
            g_confirm.reset();
            g_confirmOpen = false;
        }
        ImGui::EndPopup();
    } else {
        g_confirm.reset();
        g_confirmOpen = false;
    }
    ImGui::PopStyleVar();
}

void emptyState(const char* title, const char* body) {
    ImGui::Dummy({0, dp(4)});
    ImGui::PushFont(fonts().semibold);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (body && *body) mutedText(body);
}

}  // namespace rl::ui
