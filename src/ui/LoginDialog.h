#pragma once

#include "ui/UiContext.h"

namespace rl::ui {

// Modal shown while a quick-login (code + browser) sign-in is in progress.
void drawLoginDialog(UiContext& ctx);

}  // namespace rl::ui
