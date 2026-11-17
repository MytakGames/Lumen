#include <doctest.h>

#include <atomic>
#include <string>

#include "TempDir.h"
#include "core/AtomicFile.h"
#include "core/Logger.h"
#include "core/StringUtil.h"
#include "core/ThreadPool.h"

TEST_CASE("atomic write replaces content and leaves no temp file") {
    TempDir dir;
    const auto file = dir.path() / "sub" / "data.json";
    REQUIRE(rl::fs::writeAtomic(file, "first"));
    REQUIRE(rl::fs::writeAtomic(file, "second"));
    CHECK(rl::fs::readAll(file).value() == "second");
    CHECK_FALSE(std::filesystem::exists(dir.path() / "sub" / "data.json.tmp"));
}

TEST_CASE("logger redacts session secrets") {
    const std::string cookie = "_|WARNING:-DO-NOT-SHARE-THIS.--Sharing-this-will-allow-someone|_ABCDEF0123456789";
    const std::string out = rl::log::redact("cookie=" + cookie + " done");
    CHECK(out.find("ABCDEF0123456789") == std::string::npos);
    CHECK(out.find("[REDACTED]") != std::string::npos);
    CHECK(out.find("done") != std::string::npos);

    const std::string named = rl::log::redact(R"({".ROBLOSECURITY": "secretvalue", "x-csrf-token": "tok123"})");
    CHECK(named.find("secretvalue") == std::string::npos);
    CHECK(named.find("tok123") == std::string::npos);

    CHECK(rl::log::redact("plain message") == "plain message");
}

TEST_CASE("logger rotates files by size") {
    TempDir dir;
    rl::log::init(dir.path(), 512, 2);
    for (int i = 0; i < 50; ++i) rl::log::info("test", "line number " + std::to_string(i) + " with some padding text");
    rl::log::shutdown();
    CHECK(std::filesystem::exists(dir.path() / "launcher.log"));
    CHECK(std::filesystem::exists(dir.path() / "launcher.1.log"));
    CHECK(std::filesystem::exists(dir.path() / "launcher.2.log"));
    CHECK_FALSE(std::filesystem::exists(dir.path() / "launcher.3.log"));
    CHECK(std::filesystem::file_size(dir.path() / "launcher.log") <= 512);
}

TEST_CASE("utf8 <-> utf16 round trip") {
    const std::string s = "perch\xC3\xA9 citt\xC3\xA0";  // "perché città"
    CHECK(rl::str::toUtf8(rl::str::toWide(s)) == s);
}

TEST_CASE("thread pool runs tasks and returns results") {
    rl::ThreadPool pool(3);
    std::atomic<int> sum{0};
    std::vector<std::future<int>> futures;
    for (int i = 1; i <= 10; ++i) futures.push_back(pool.submit([i, &sum] { sum += i; return i * 2; }));
    int total = 0;
    for (auto& f : futures) total += f.get();
    CHECK(total == 110);
    CHECK(sum == 55);
}

TEST_CASE("main thread queue runs posted work on drain") {
    rl::MainThreadQueue q;
    int value = 0;
    q.post([&] { value = 7; });
    CHECK(value == 0);
    q.drain();
    CHECK(value == 7);
}
