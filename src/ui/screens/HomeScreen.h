#pragma once

#include <array>

#include "ui/UiContext.h"

namespace rl::ui {

class HomeScreen final : public Screen {
public:
    void onShow(UiContext& ctx) override;
    void draw(UiContext& ctx) override;

private:
    void drawSearchBox(UiContext& ctx);
    void drawResults(UiContext& ctx);
    void drawSections(UiContext& ctx);

    std::array<char, 256> query_{};
};

}  // namespace rl::ui
