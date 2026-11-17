#pragma once

#include <array>
#include <string>

#include "ui/UiContext.h"

namespace rl::ui {

class AccountsScreen final : public Screen {
public:
    void onShow(UiContext& ctx) override;
    void draw(UiContext& ctx) override;

private:
    void drawToolbar(UiContext& ctx);
    void drawEditModal(UiContext& ctx);

    std::string editing_;  // account id being edited
    bool openEdit_ = false;
    std::array<char, 64> alias_{};
    int color_ = -1;
};

}  // namespace rl::ui
