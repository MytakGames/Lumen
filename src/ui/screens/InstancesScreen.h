#pragma once

#include "ui/UiContext.h"

namespace rl::ui {

class InstancesScreen final : public Screen {
public:
    void draw(UiContext& ctx) override;
};

}  // namespace rl::ui
