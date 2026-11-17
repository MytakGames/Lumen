#pragma once

#include <cstdint>
#include <string>

#include "ui/UiContext.h"

namespace rl::ui {

class GameDetailScreen final : public Screen {
public:
    void setGame(std::int64_t universeId);
    std::int64_t game() const { return universeId_; }
    void onShow(UiContext& ctx) override;
    void draw(UiContext& ctx) override;

private:
    void drawLaunchCard(UiContext& ctx);
    void drawStats(UiContext& ctx);

    std::int64_t universeId_ = 0;
    std::string selectedAccount_;
};

}  // namespace rl::ui
