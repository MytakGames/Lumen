#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui/UiContext.h"

namespace rl::ui {

// Shared rendering of game tiles for Home sections and search results.
std::string gameMeta(const UiContext& ctx, std::int64_t players, int ratingPercent);
// Draws a responsive grid of game cards; `recentMenu` enables the context
// menu entry "remove from recent games".
void gameGrid(UiContext& ctx, const char* id, const std::vector<std::int64_t>& universeIds, bool recentMenu,
              size_t maxItems = 0);

}  // namespace rl::ui
