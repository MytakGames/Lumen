#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

// Thread-safe file logger with size-based rotation.
// Every message passes through redact() so session cookies and auth tickets
// can never end up on disk, even if a caller logs them by mistake.
namespace rl::log {

enum class Level { Debug, Info, Warn, Error };

void init(const std::filesystem::path& dir, std::size_t maxBytes = 1024 * 1024, int keepFiles = 3);
void shutdown();

void write(Level level, std::string_view module, std::string_view message);

// Masks values that look like Roblox session secrets.
std::string redact(std::string_view text);

inline void debug(std::string_view m, std::string_view msg) { write(Level::Debug, m, msg); }
inline void info(std::string_view m, std::string_view msg) { write(Level::Info, m, msg); }
inline void warn(std::string_view m, std::string_view msg) { write(Level::Warn, m, msg); }
inline void error(std::string_view m, std::string_view msg) { write(Level::Error, m, msg); }

}  // namespace rl::log
