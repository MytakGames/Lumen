#include "ui/Fonts.h"

#include <windows.h>
#include <shlobj.h>

#include <filesystem>

#include "core/Logger.h"

namespace rl::ui {
namespace {

std::filesystem::path fontsDir() {
    PWSTR raw = nullptr;
    std::filesystem::path dir = L"C:\\Windows\\Fonts";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &raw))) dir = raw;
    CoTaskMemFree(raw);
    return dir;
}

// Latin + Latin-1 (Italian accents) + general punctuation (… – “ ” €).
const ImWchar kRanges[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0x2030, 0x203A, 0x20AC, 0x20AC, 0};

// Merged into every font so game names with symbols/emoji don't show "?".
// Rendered monochrome (Dear ImGui has no colour-font support without FreeType).
const ImWchar kSymbolRanges[] = {0x2190, 0x21FF, 0x2300, 0x23FF, 0x2460, 0x24FF, 0x25A0, 0x27BF, 0x2B00, 0x2BFF, 0};
const ImWchar kEmojiRanges[] = {0x200D, 0x200D, 0xFE00, 0xFE0F, 0x1F000, 0x1FAFF, 0};

ImFont* load(const std::filesystem::path& file, float px) {
    if (!std::filesystem::exists(file)) return nullptr;
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(file.string().c_str(), px, &cfg, kRanges);
    if (!font) return nullptr;

    const auto dir = file.parent_path();
    ImFontConfig merge;
    merge.MergeMode = true;
    merge.OversampleH = 1;
    merge.OversampleV = 1;
    merge.GlyphMinAdvanceX = px * 0.9f;
    if (const auto sym = dir / L"seguisym.ttf"; std::filesystem::exists(sym)) {
        ImGui::GetIO().Fonts->AddFontFromFileTTF(sym.string().c_str(), px, &merge, kSymbolRanges);
    }
    if (const auto emoji = dir / L"seguiemj.ttf"; std::filesystem::exists(emoji)) {
        ImGui::GetIO().Fonts->AddFontFromFileTTF(emoji.string().c_str(), px, &merge, kEmojiRanges);
    }
    return font;
}

}  // namespace

Fonts& fonts() {
    static Fonts f;
    return f;
}

void loadFonts(float dpiScale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const auto dir = fontsDir();
    Fonts& f = fonts();
    f.regular = load(dir / L"segoeui.ttf", 16.0f * dpiScale);
    f.semibold = load(dir / L"seguisb.ttf", 16.0f * dpiScale);
    f.small = load(dir / L"segoeui.ttf", 13.5f * dpiScale);
    f.title = load(dir / L"segoeuib.ttf", 26.0f * dpiScale);

    if (!f.regular) {
        log::warn("ui", "Segoe UI not found, using built-in font");
        ImFontConfig cfg;
        cfg.SizePixels = 13.0f * dpiScale;
        f.regular = io.Fonts->AddFontDefault(&cfg);
    }
    if (!f.semibold) f.semibold = f.regular;
    if (!f.small) f.small = f.regular;
    if (!f.title) f.title = f.semibold;
    io.FontDefault = f.regular;
    io.Fonts->Build();
}

}  // namespace rl::ui
