#include "core/AtomicFile.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

#include "core/Logger.h"

namespace rl::fs {
namespace {

struct HandleCloser {
    HANDLE h;
    ~HandleCloser() {
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
};

}  // namespace

bool writeAtomic(const std::filesystem::path& path, std::string_view content) {
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);

    auto tmp = path;
    tmp += L".tmp";
    {
        HandleCloser file{CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.h == INVALID_HANDLE_VALUE) {
            log::error("fs", "Cannot create temp file " + tmp.string());
            return false;
        }
        size_t offset = 0;
        while (offset < content.size()) {
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(content.size() - offset, 1u << 30));
            DWORD done = 0;
            if (!WriteFile(file.h, content.data() + offset, chunk, &done, nullptr)) {
                log::error("fs", "Write failed for " + tmp.string());
                return false;
            }
            offset += done;
        }
        if (!FlushFileBuffers(file.h)) {
            log::warn("fs", "FlushFileBuffers failed for " + tmp.string());
        }
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        log::error("fs", "Atomic rename failed for " + path.string() + " (error " +
                             std::to_string(GetLastError()) + ")");
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

std::optional<std::string> readAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::filesystem::path backupCorrupt(const std::filesystem::path& path) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), ".corrupt-%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay,
                  t.wHour, t.wMinute, t.wSecond);
    auto target = path.parent_path() / path.stem();
    target += stamp;
    target += path.extension();
    std::error_code ec;
    std::filesystem::rename(path, target, ec);
    if (ec) {
        log::error("fs", "Cannot back up corrupt file " + path.string());
        return {};
    }
    log::warn("fs", "Corrupt file moved to " + target.string());
    return target;
}

}  // namespace rl::fs
