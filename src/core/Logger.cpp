#include "core/Logger.h"

#include <windows.h>

#include <cstdio>
#include <fstream>
#include <mutex>
#include <regex>
#include <system_error>

namespace rl::log {
namespace {

struct State {
    std::mutex mutex;
    std::ofstream file;
    std::filesystem::path dir;
    std::size_t maxBytes = 0;
    int keepFiles = 0;
    std::size_t written = 0;
    bool ready = false;
};

State& state() {
    static State s;
    return s;
}

std::filesystem::path logPath(const std::filesystem::path& dir, int index) {
    if (index == 0) return dir / "launcher.log";
    return dir / ("launcher." + std::to_string(index) + ".log");
}

// Shifts launcher.log -> launcher.1.log -> ... and drops the oldest. Caller holds the lock.
void rotate(State& s) {
    s.file.close();
    std::error_code ec;
    std::filesystem::remove(logPath(s.dir, s.keepFiles), ec);
    for (int i = s.keepFiles - 1; i >= 0; --i) {
        const auto from = logPath(s.dir, i);
        if (std::filesystem::exists(from, ec)) std::filesystem::rename(from, logPath(s.dir, i + 1), ec);
    }
    s.file.open(logPath(s.dir, 0), std::ios::binary | std::ios::trunc);
    s.written = 0;
}

const char* levelName(Level l) {
    switch (l) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO ";
        case Level::Warn: return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "?";
}

}  // namespace

void init(const std::filesystem::path& dir, std::size_t maxBytes, int keepFiles) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    s.dir = dir;
    s.maxBytes = maxBytes;
    s.keepFiles = keepFiles < 1 ? 1 : keepFiles;
    const auto path = logPath(dir, 0);
    s.written = std::filesystem::exists(path, ec) ? static_cast<std::size_t>(std::filesystem::file_size(path, ec)) : 0;
    s.file.open(path, std::ios::binary | std::ios::app);
    s.ready = s.file.is_open();
    if (s.ready && s.written >= s.maxBytes) rotate(s);
}

void shutdown() {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.file.close();
    s.ready = false;
}

std::string redact(std::string_view text) {
    // .ROBLOSECURITY values start with this well-known warning prefix.
    static const std::regex cookieValue(R"(_\|WARNING:[^\s;"',}]+)");
    // name=value / "name": "value" forms for secrets we handle.
    static const std::regex namedSecret(
        R"((\.?ROBLOSECURITY|RBX-Authentication-Ticket|x-csrf-token|authenticationTicket)(["']?\s*[:=]\s*["']?)[^\s;"',}]+)",
        std::regex::icase);
    std::string out = std::regex_replace(std::string(text), cookieValue, "[REDACTED]");
    out = std::regex_replace(out, namedSecret, "$1$2[REDACTED]");
    return out;
}

void write(Level level, std::string_view module, std::string_view message) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%04u-%02u-%02u %02u:%02u:%02u.%03u", t.wYear, t.wMonth, t.wDay,
                  t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

    std::string line;
    line.reserve(message.size() + 64);
    line.append(stamp).append(" [").append(levelName(level)).append("] [");
    line.append(module).append("] ").append(redact(message)).append("\n");

#ifndef NDEBUG
    OutputDebugStringA(line.c_str());
#endif

    auto& s = state();
    std::lock_guard lock(s.mutex);
    if (!s.ready) return;
    if (s.written + line.size() > s.maxBytes) rotate(s);
    s.file << line;
    s.file.flush();
    s.written += line.size();
}

}  // namespace rl::log
