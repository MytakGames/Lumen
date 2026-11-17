#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace rl::str {

// UTF-16 (Win32) <-> UTF-8 (everything else in the app).
std::string toUtf8(std::wstring_view wide);
std::wstring toWide(std::string_view utf8);

std::string_view trim(std::string_view s);
std::string toLower(std::string_view s);

// Replaces every occurrence of `from` with `to`.
std::string replaceAll(std::string s, std::string_view from, std::string_view to);

// RFC 3986 percent-encoding (unreserved characters kept).
std::string urlEncode(std::string_view s);

// Random lowercase hex string of `bytes * 2` characters.
std::string randomHex(size_t bytes);
// Random RFC 4122 v4 UUID.
std::string uuid4();

// 64-bit FNV-1a, used for cache file names.
std::uint64_t fnv1a(std::string_view s);

}  // namespace rl::str
