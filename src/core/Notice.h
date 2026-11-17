#pragma once

#include <functional>
#include <string>

#include "i18n/Localizer.h"

namespace rl {

// A user-facing message raised by a service. Services only know the locale
// key; the UI layer renders it as a toast in the current language.
struct Notice {
    enum class Level { Info, Success, Warning, Error };
    Level level = Level::Info;
    std::string key;
    i18n::Args args;
};

using Notify = std::function<void(Notice)>;

}  // namespace rl
