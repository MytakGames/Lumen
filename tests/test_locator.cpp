#include <doctest.h>

#include "instances/RobloxLocator.h"

using namespace rl::instances;

TEST_CASE("executable path from shell open commands") {
    CHECK(exeFromCommand(LR"("C:\Users\x\AppData\Local\Roblox\Versions\version-1\RobloxPlayerBeta.exe" %1)") ==
          LR"(C:\Users\x\AppData\Local\Roblox\Versions\version-1\RobloxPlayerBeta.exe)");
    CHECK(exeFromCommand(LR"(  C:\Program Files\Bloxstrap\Bloxstrap.exe -player "%1")") ==
          LR"(C:\Program Files\Bloxstrap\Bloxstrap.exe)");
    CHECK(exeFromCommand(LR"(C:\Tools\handler %1)") == LR"(C:\Tools\handler)");
    CHECK(exeFromCommand(LR"("C:\unterminated)") == LR"(C:\unterminated)");
    CHECK(exeFromCommand(L"   ").empty());
}

TEST_CASE("launch URI layout") {
    const std::string uri = buildLaunchUri("TICKET", 2753915549, 1700000000123, "123456", "join-id", "it_it");
    CHECK(uri.rfind("roblox-player:1+launchmode:play+gameinfo:TICKET+launchtime:1700000000123+placelauncherurl:", 0) ==
          0);
    // The place launcher URL is percent-encoded so its '&' and '=' don't split the URI.
    CHECK(uri.find("PlaceLauncher.ashx%3Frequest%3DRequestGame") != std::string::npos);
    CHECK(uri.find("placeId%3D2753915549") != std::string::npos);
    CHECK(uri.find("joinAttemptId%3Djoin-id") != std::string::npos);
    CHECK(uri.find("+browsertrackerid:123456+robloxLocale:it_it+gameLocale:it_it+") != std::string::npos);
    CHECK(uri.find('&') == std::string::npos);
    CHECK(uri.find(' ') == std::string::npos);
}

TEST_CASE("Roblox cookie file location") {
    const auto p = robloxCookieFile();
    REQUIRE_FALSE(p.empty());
    CHECK(p.filename() == L"RobloxCookies.dat");
    CHECK(p.parent_path().filename() == L"LocalStorage");
}
