#pragma once

#include <string>

namespace rl::ui {

// Decodes PNG/JPEG bytes to tightly packed RGBA8. Empty string on failure.
std::string decodeRgba(const std::string& encoded, int& width, int& height);

}  // namespace rl::ui
