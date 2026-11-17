#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ui/UiContext.h"

namespace rl::ui {

class GameDetailScreen;

enum class ScreenId { Home, Accounts, Instances, Settings, GameDetail, Count };

const char* screenToString(ScreenId id);
ScreenId screenFromString(const std::string& s);

// Sidebar navigation + animated content area hosting the active screen.
class MainLayout {
public:
    MainLayout();
    void draw(UiContext& ctx);

    ScreenId current() const { return current_; }
    void navigate(ScreenId id);
    void openGame(std::int64_t universeId);
    void back();

private:
    void drawSidebar(UiContext& ctx);
    void drawContent(UiContext& ctx);

    std::array<std::unique_ptr<Screen>, static_cast<size_t>(ScreenId::Count)> screens_;
    GameDetailScreen* detail_ = nullptr;
    ScreenId current_ = ScreenId::Home;
    std::vector<ScreenId> history_;
    bool showPending_ = true;   // call onShow() on the next frame
    bool scrollTop_ = false;
    float transition_ = 1.0f;  // 0 -> 1 after a screen change
};

}  // namespace rl::ui
