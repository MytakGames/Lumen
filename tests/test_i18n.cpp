#include <doctest.h>

#include <algorithm>
#include <iterator>

#include "i18n/Localizer.h"

using rl::i18n::Localizer;

namespace {

Localizer loadShipped() {
    Localizer l;
    REQUIRE(l.loadDirectory(RL_LOCALES_DIR));
    return l;
}

const char* kEn = R"({
  "_meta": {"name": "English"},
  "hello": "Hello {name}",
  "nested": {"key": "Nested"},
  "only_en": "Fallback",
  "items": {"one": "{count} item", "other": "{count} items"},
  "time": {
    "just_now": "just now",
    "minutes_ago": {"one": "{count} minute ago", "other": "{count} minutes ago"},
    "hours_ago": {"one": "{count} hour ago", "other": "{count} hours ago"},
    "days_ago": {"one": "{count} day ago", "other": "{count} days ago"}
  }
})";
const char* kIt = R"({
  "_meta": {"name": "Italiano"},
  "hello": "Ciao {name}",
  "nested": {"key": "Annidato"},
  "items": {"one": "{count} elemento", "other": "{count} elementi"}
})";

Localizer sample() {
    Localizer l;
    REQUIRE(l.loadFromString("en", kEn));
    REQUIRE(l.loadFromString("it", kIt));
    return l;
}

}  // namespace

TEST_CASE("shipped locales have exactly the same keys") {
    const Localizer l = loadShipped();
    const auto en = l.keys("en");
    const auto it = l.keys("it");
    REQUIRE_FALSE(en.empty());

    std::vector<std::string> missingInIt, missingInEn;
    std::set_difference(en.begin(), en.end(), it.begin(), it.end(), std::back_inserter(missingInIt));
    std::set_difference(it.begin(), it.end(), en.begin(), en.end(), std::back_inserter(missingInEn));
    for (const auto& k : missingInIt) MESSAGE("missing in it.json: " << k);
    for (const auto& k : missingInEn) MESSAGE("missing in en.json: " << k);
    CHECK(missingInIt.empty());
    CHECK(missingInEn.empty());
}

TEST_CASE("shipped locales keep placeholders consistent") {
    Localizer l = loadShipped();
    for (const auto& key : l.keys("en")) {
        l.setLanguage("en");
        const std::string en = l.tr(key);
        l.setLanguage("it");
        const std::string it = l.tr(key);
        for (const char* ph : {"{name}", "{count}", "{phase}", "{file}", "{version}", "%d"}) {
            INFO("key: " << key << " placeholder: " << ph);
            CHECK((en.find(ph) == std::string::npos) == (it.find(ph) == std::string::npos));
        }
    }
}

TEST_CASE("lookup, nesting and language switch") {
    Localizer l = sample();
    l.setLanguage("en");
    CHECK(l.tr("nested.key") == "Nested");
    l.setLanguage("it");
    CHECK(l.tr("nested.key") == "Annidato");
    CHECK(l.languageDisplayName("it") == "Italiano");
}

TEST_CASE("fallback to English, then to the key") {
    Localizer l = sample();
    l.setLanguage("it");
    CHECK(l.tr("only_en") == "Fallback");
    CHECK(l.tr("does.not.exist") == "does.not.exist");
    l.setLanguage("fr");  // unknown -> English
    CHECK(l.language() == "en");
}

TEST_CASE("placeholders") {
    Localizer l = sample();
    l.setLanguage("it");
    CHECK(l.format("hello", {{"name", "Mario"}}) == "Ciao Mario");
}

TEST_CASE("plurals in both languages") {
    Localizer l = sample();
    l.setLanguage("it");
    CHECK(l.plural("items", 1) == "1 elemento");
    CHECK(l.plural("items", 0) == "0 elementi");
    CHECK(l.plural("items", 2500) == "2.500 elementi");
    l.setLanguage("en");
    CHECK(l.plural("items", 1) == "1 item");
    CHECK(l.plural("items", 2500) == "2,500 items");
}

TEST_CASE("number formatting") {
    Localizer l = sample();
    l.setLanguage("it");
    CHECK(l.formatNumber(0) == "0");
    CHECK(l.formatNumber(999) == "999");
    CHECK(l.formatNumber(1000) == "1.000");
    CHECK(l.formatNumber(1234567) == "1.234.567");
    CHECK(l.formatNumber(-45000) == "-45.000");
    l.setLanguage("en");
    CHECK(l.formatNumber(1234567) == "1,234,567");
    CHECK(l.formatNumber(100000) == "100,000");
}

TEST_CASE("date formatting order differs per language") {
    Localizer l = sample();
    std::tm tm{};
    tm.tm_year = 2026 - 1900;
    tm.tm_mon = 10;  // November
    tm.tm_mday = 3;
    tm.tm_hour = 14;
    tm.tm_min = 5;
    tm.tm_isdst = -1;
    const std::time_t t = std::mktime(&tm);
    l.setLanguage("it");
    CHECK(l.formatDate(t) == "03/11/2026");
    CHECK(l.formatDateTime(t) == "03/11/2026 14:05");
    l.setLanguage("en");
    CHECK(l.formatDate(t) == "11/03/2026");
    CHECK(l.formatDateTime(t) == "11/03/2026 2:05 PM");
}

TEST_CASE("relative time uses plural forms") {
    Localizer l = sample();
    l.setLanguage("en");
    const std::time_t now = 1'800'000'000;
    CHECK(l.formatRelative(now - 10, now) == "just now");
    CHECK(l.formatRelative(now - 60, now) == "1 minute ago");
    CHECK(l.formatRelative(now - 3 * 3600, now) == "3 hours ago");
    CHECK(l.formatRelative(now - 86400, now) == "1 day ago");
}

TEST_CASE("invalid locale JSON is rejected") {
    Localizer l;
    CHECK_FALSE(l.loadFromString("xx", "{ not json"));
    CHECK_FALSE(l.hasLanguage("xx"));
}
