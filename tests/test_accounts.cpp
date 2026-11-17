#include <doctest.h>

#include "TempDir.h"
#include "accounts/AccountStore.h"
#include "accounts/CookieVault.h"
#include "core/AtomicFile.h"

using namespace rl;
using namespace rl::accounts;

namespace {
Account make(const std::string& name, std::int64_t userId, const std::string& cookie = {}) {
    Account a;
    a.username = name;
    a.displayName = name + " D";
    a.userId = userId;
    a.protectedCookie = cookie;
    return a;
}
}  // namespace

TEST_CASE("DPAPI vault round-trip") {
    const std::string secret = "_|WARNING:-DO-NOT-SHARE-THIS.--test-cookie-value";
    const auto blob = vault::protect(SecureString(secret));
    REQUIRE(blob);
    CHECK(blob.value->find("test-cookie-value") == std::string::npos);  // not stored in clear
    const auto back = vault::unprotect(*blob.value);
    REQUIRE(back);
    CHECK(back.value->reveal() == secret);
}

TEST_CASE("DPAPI vault rejects tampered or invalid blobs") {
    const auto blob = vault::protect(SecureString(std::string("abc")));
    REQUIRE(blob);
    std::string tampered = *blob.value;
    tampered[tampered.size() / 2] = tampered[tampered.size() / 2] == 'A' ? 'B' : 'A';
    CHECK_FALSE(vault::unprotect(tampered));
    CHECK(vault::unprotect("%%% not base64 %%%").error == ErrorCode::CryptoFailed);
}

TEST_CASE("account store persists order, fields and the protected cookie") {
    TempDir dir;
    const auto file = dir.path() / "accounts.json";
    {
        AccountStore store(file);
        CHECK(store.load() == settings::LoadStatus::Missing);
        auto& a = store.add(make("alpha", 1, "BLOB-A"));
        a.alias = "Main";
        a.colorIndex = 3;
        store.add(make("beta", 2));
        auto& c = store.add(make("gamma", 3));
        c.enabled = false;
        CHECK(store.move(c.id, -1));
        CHECK_FALSE(store.move(store.all().front().id, -1));  // already first
        REQUIRE(store.save());
    }
    AccountStore store(file);
    REQUIRE(store.load() == settings::LoadStatus::Ok);
    const auto& all = store.all();
    REQUIRE(all.size() == 3);
    CHECK(all[0].username == "alpha");
    CHECK(all[0].label() == "Main");
    CHECK(all[0].colorIndex == 3);
    CHECK(all[0].protectedCookie == "BLOB-A");
    CHECK(all[0].addedAt != 0);
    CHECK(all[1].username == "gamma");
    CHECK_FALSE(all[1].enabled);
    CHECK(all[2].username == "beta");
    CHECK(all[2].label() == "beta D");
    CHECK(store.findByUserId(2) != nullptr);
    CHECK(store.findByUserId(0) == nullptr);
    CHECK(store.remove(all[0].id));
    CHECK(store.all().size() == 2);
}

TEST_CASE("corrupt accounts file is backed up") {
    TempDir dir;
    const auto file = dir.path() / "accounts.json";
    REQUIRE(fs::writeAtomic(file, "{ broken"));
    AccountStore store(file);
    CHECK(store.load() == settings::LoadStatus::Corrupt);
    CHECK(store.all().empty());
    bool backup = false;
    for (const auto& e : std::filesystem::directory_iterator(dir.path()))
        if (e.path().filename().string().find("corrupt") != std::string::npos) backup = true;
    CHECK(backup);
}

TEST_CASE("metadata export never contains cookies; import skips duplicates") {
    TempDir dir;
    AccountStore store(dir.path() / "a.json");
    store.add(make("alpha", 1, "SECRET-BLOB"));
    const std::string exported = store.exportMetadata();
    CHECK(exported.find("SECRET-BLOB") == std::string::npos);
    CHECK(exported.find("protectedCookie") == std::string::npos);
    CHECK(exported.find("alpha") != std::string::npos);

    AccountStore other(dir.path() / "b.json");
    other.add(make("beta", 2));
    CHECK(other.importMetadata(exported) == 1);
    CHECK(other.importMetadata(exported) == 0);  // already present
    CHECK(other.all().size() == 2);

    // Secrets inside an imported document are dropped.
    const std::string withSecret =
        R"({"version":1,"accounts":[{"username":"gamma","userId":3,"protectedCookie":"INJECTED"}]})";
    CHECK(other.importMetadata(withSecret) == 1);
    CHECK(other.findByUserId(3)->protectedCookie.empty());
    CHECK(other.importMetadata("[]") == -1);
}
