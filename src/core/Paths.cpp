#include "core/Paths.h"

#include <windows.h>
#include <shlobj.h>

#include <system_error>

namespace rl::paths {
namespace {

std::filesystem::path& overrideDir() {
    static std::filesystem::path dir;
    return dir;
}

std::filesystem::path ensure(std::filesystem::path p) {
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

}  // namespace

void setDataDirOverride(const std::filesystem::path& dir) { overrideDir() = dir; }

std::filesystem::path dataDir() {
    if (!overrideDir().empty()) return ensure(overrideDir());
    PWSTR raw = nullptr;
    std::filesystem::path base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        base = raw;
    } else {
        base = std::filesystem::temp_directory_path();
    }
    CoTaskMemFree(raw);
    const auto current = base / L"Lumen";
    // One-time migration from the pre-rename folder. If the move fails (e.g. a
    // file is still open), keep using the old folder so no data is lost.
    static const std::filesystem::path resolved = [&] {
        const auto legacy = base / L"RobloxLauncher";
        std::error_code ec;
        if (!std::filesystem::exists(current, ec) && std::filesystem::is_directory(legacy, ec)) {
            if (!MoveFileExW(legacy.c_str(), current.c_str(), MOVEFILE_WRITE_THROUGH)) return legacy;
        }
        return current;
    }();
    return ensure(resolved);
}

std::filesystem::path executablePath() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);
    }
    return std::filesystem::path(buf);
}

std::filesystem::path logsDir() { return ensure(dataDir() / L"logs"); }
std::filesystem::path cacheDir() { return ensure(dataDir() / L"cache"); }
std::filesystem::path settingsFile() { return dataDir() / L"settings.json"; }

std::filesystem::path executableDir() { return executablePath().parent_path(); }

}  // namespace rl::paths
