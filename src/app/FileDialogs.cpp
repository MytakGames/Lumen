#include "app/FileDialogs.h"

#include <commdlg.h>

#include <vector>

namespace rl::app {
namespace {
// Filter strings are file-type patterns, not user-facing prose.
constexpr wchar_t kFilter[] = L"JSON (*.json)\0*.json\0*.*\0*.*\0";
}  // namespace

std::optional<std::filesystem::path> saveJsonDialog(HWND owner, const std::wstring& title, const std::wstring& defaultName) {
    std::vector<wchar_t> buf(MAX_PATH * 4, L'\0');
    wcsncpy_s(buf.data(), buf.size(), defaultName.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return std::nullopt;
    return std::filesystem::path(buf.data());
}

std::optional<std::filesystem::path> openJsonDialog(HWND owner, const std::wstring& title) {
    std::vector<wchar_t> buf(MAX_PATH * 4, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return std::filesystem::path(buf.data());
}

}  // namespace rl::app
