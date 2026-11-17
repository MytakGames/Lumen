#include <doctest.h>

#include "TempDir.h"
#include "core/AtomicFile.h"
#include "settings/Settings.h"

using namespace rl::settings;

TEST_CASE("settings round-trip through JSON") {
    Settings s;
    s.language = "it";
    s.theme = Theme::Light;
    s.multiInstanceEnabled = true;
    s.maxInstances = 7;
    s.instanceLaunchDelayMs = 1500;
    s.recentGamesMax = 42;
    s.lastScreen = "settings";

    const Settings back = fromJson(toJson(s));
    CHECK(back.language == "it");
    CHECK(back.theme == Theme::Light);
    CHECK(back.multiInstanceEnabled);
    CHECK(back.maxInstances == 7);
    CHECK(back.instanceLaunchDelayMs == 1500);
    CHECK(back.recentGamesMax == 42);
    CHECK(back.lastScreen == "settings");
}

TEST_CASE("out-of-range and mistyped values are sanitised") {
    const Settings s = fromJson(R"({"maxInstances": 999, "recentGamesMax": -3, "language": "de",
                                    "animations": "yes", "theme": "neon"})");
    CHECK(s.maxInstances == limits::kMaxInstancesMax);
    CHECK(s.recentGamesMax == limits::kRecentMin);
    CHECK(s.language.empty());  // unsupported -> re-detect
    CHECK(s.animations == Settings{}.animations);
    CHECK(s.theme == Theme::Dark);
}

TEST_CASE("missing file yields defaults") {
    TempDir dir;
    const LoadResult r = load(dir.path() / "settings.json");
    CHECK(r.status == LoadStatus::Missing);
    CHECK(r.settings.maxInstances == Settings{}.maxInstances);
}

TEST_CASE("save then load") {
    TempDir dir;
    const auto file = dir.path() / "settings.json";
    Settings s;
    s.language = "en";
    s.apiCacheTtlMinutes = 33;
    REQUIRE(save(file, s));
    CHECK_FALSE(std::filesystem::exists(dir.path() / "settings.json.tmp"));
    const LoadResult r = load(file);
    CHECK(r.status == LoadStatus::Ok);
    CHECK(r.settings.apiCacheTtlMinutes == 33);
}

TEST_CASE("corrupt file is backed up and replaced by defaults") {
    TempDir dir;
    const auto file = dir.path() / "settings.json";
    REQUIRE(rl::fs::writeAtomic(file, "{ this is : not json"));

    const LoadResult r = load(file);
    CHECK(r.status == LoadStatus::Corrupt);
    CHECK(r.settings.maxInstances == Settings{}.maxInstances);
    REQUIRE_FALSE(r.backupPath.empty());
    CHECK(std::filesystem::exists(r.backupPath));
    CHECK_FALSE(std::filesystem::exists(file));
    CHECK(rl::fs::readAll(r.backupPath).value() == "{ this is : not json");
}

TEST_CASE("non-object JSON counts as corrupt") {
    TempDir dir;
    const auto file = dir.path() / "settings.json";
    REQUIRE(rl::fs::writeAtomic(file, "[1, 2, 3]"));
    CHECK(load(file).status == LoadStatus::Corrupt);
}
