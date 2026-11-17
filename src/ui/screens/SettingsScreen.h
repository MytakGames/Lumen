#pragma once

#include "ui/UiContext.h"

namespace rl::ui {

class SettingsScreen final : public Screen {
public:
    void draw(UiContext& ctx) override;

private:
    bool drawGeneral(UiContext& ctx);
    bool drawStartup(UiContext& ctx);
    bool drawInstances(UiContext& ctx);
    bool drawData(UiContext& ctx);
    void drawAbout(UiContext& ctx);
    void drawResetModal(UiContext& ctx);

    bool openResetModal_ = false;
};

}  // namespace rl::ui
