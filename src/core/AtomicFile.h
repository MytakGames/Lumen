#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace rl::fs {

// Writes to "<path>.tmp", flushes to disk, then atomically replaces <path>.
// A crash mid-write leaves the previous file intact.
bool writeAtomic(const std::filesystem::path& path, std::string_view content);

std::optional<std::string> readAll(const std::filesystem::path& path);

// Moves a damaged file aside as "<stem>.corrupt-YYYYMMDD-HHMMSS<ext>" and
// returns the new path (empty on failure).
std::filesystem::path backupCorrupt(const std::filesystem::path& path);

}  // namespace rl::fs
