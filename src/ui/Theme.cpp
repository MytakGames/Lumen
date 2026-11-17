#include "ui/Theme.h"

#include <windows.h>

namespace rl::ui {
namespace {

constexpr ImVec4 rgb(int r, int g, int b, float a = 1.0f) {
    return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, a);
}

Palette makeDark() {
    Palette p;
    p.dark = true;
    p.bg = rgb(15, 17, 23);
    p.sidebar = rgb(11, 13, 18);
    p.surface = rgb(23, 26, 35);
    p.surfaceHover = rgb(31, 35, 48);
    p.surfaceActive = rgb(38, 43, 58);
    p.border = rgb(38, 42, 54);
    p.text = rgb(232, 234, 240);
    p.textMuted = rgb(139, 145, 163);
    p.accent = rgb(61, 123, 255);
    p.accentHover = rgb(90, 143, 255);
    p.accentActive = rgb(47, 102, 224);
    p.onAccent = rgb(255, 255, 255);
    p.success = rgb(43, 196, 138);
    p.warning = rgb(242, 179, 61);
    p.danger = rgb(242, 92, 92);
    return p;
}

Palette makeLight() {
    Palette p;
    p.dark = false;
    p.bg = rgb(245, 246, 250);
    p.sidebar = rgb(236, 238, 244);
    p.surface = rgb(255, 255, 255);
    p.surfaceHover = rgb(240, 242, 247);
    p.surfaceActive = rgb(230, 233, 241);
    p.border = rgb(225, 228, 236);
    p.text = rgb(22, 26, 35);
    p.textMuted = rgb(100, 107, 125);
    p.accent = rgb(47, 102, 224);
    p.accentHover = rgb(61, 123, 255);
    p.accentActive = rgb(37, 84, 192);
    p.onAccent = rgb(255, 255, 255);
    p.success = rgb(22, 160, 108);
    p.warning = rgb(196, 132, 18);
    p.danger = rgb(214, 60, 60);
    return p;
}

Palette& current() {
    static Palette p = makeDark();
    return p;
}

}  // namespace

const Palette& palette() { return current(); }

ImU32 toU32(const ImVec4& c, float alphaMul) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alphaMul)); }

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

bool systemPrefersDark() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                    L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return rc != ERROR_SUCCESS || value == 0;
}

void applyTheme(settings::Theme theme, float dpiScale) {
    const bool dark = theme == settings::Theme::Dark || (theme == settings::Theme::System && systemPrefersDark());
    current() = dark ? makeDark() : makeLight();
    const Palette& p = current();

    ImGuiStyle style;  // start from defaults so rescaling never compounds
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(12, 8);
    style.ItemSpacing = ImVec2(10, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 12;
    style.GrabMinSize = 14;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 0;
    style.WindowRounding = 12;
    style.ChildRounding = 12;
    style.FrameRounding = 8;
    style.PopupRounding = 10;
    style.ScrollbarRounding = 12;
    style.GrabRounding = 8;
    style.TabRounding = 8;
    style.SeparatorTextBorderSize = 1;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = p.text;
    c[ImGuiCol_TextDisabled] = p.textMuted;
    c[ImGuiCol_WindowBg] = p.bg;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = p.surface;
    c[ImGuiCol_Border] = p.border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = p.surfaceHover;
    c[ImGuiCol_FrameBgHovered] = p.surfaceActive;
    c[ImGuiCol_FrameBgActive] = p.surfaceActive;
    c[ImGuiCol_TitleBg] = p.sidebar;
    c[ImGuiCol_TitleBgActive] = p.sidebar;
    c[ImGuiCol_TitleBgCollapsed] = p.sidebar;
    c[ImGuiCol_MenuBarBg] = p.sidebar;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = p.surfaceActive;
    c[ImGuiCol_ScrollbarGrabHovered] = p.border;
    c[ImGuiCol_ScrollbarGrabActive] = p.textMuted;
    c[ImGuiCol_CheckMark] = p.accent;
    c[ImGuiCol_SliderGrab] = p.accent;
    c[ImGuiCol_SliderGrabActive] = p.accentActive;
    c[ImGuiCol_Button] = p.surfaceHover;
    c[ImGuiCol_ButtonHovered] = p.surfaceActive;
    c[ImGuiCol_ButtonActive] = p.border;
    c[ImGuiCol_Header] = p.surfaceHover;
    c[ImGuiCol_HeaderHovered] = p.surfaceActive;
    c[ImGuiCol_HeaderActive] = p.surfaceActive;
    c[ImGuiCol_Separator] = p.border;
    c[ImGuiCol_SeparatorHovered] = p.accent;
    c[ImGuiCol_SeparatorActive] = p.accent;
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered] = p.accent;
    c[ImGuiCol_ResizeGripActive] = p.accent;
    c[ImGuiCol_Tab] = p.surface;
    c[ImGuiCol_TabHovered] = p.surfaceActive;
    c[ImGuiCol_TabSelected] = p.surfaceHover;
    c[ImGuiCol_TextSelectedBg] = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.35f);
    c[ImGuiCol_NavCursor] = p.accent;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, dark ? 0.6f : 0.35f);

    style.ScaleAllSizes(dpiScale);
    ImGui::GetStyle() = style;
}

}  // namespace rl::ui
