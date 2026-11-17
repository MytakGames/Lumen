#include <doctest.h>

#include "TempDir.h"
#include "games/GameList.h"
#include "games/RecommendationEngine.h"

using namespace rl;
using namespace rl::games;

namespace {
constexpr std::time_t kNow = 1'700'000'000;
constexpr std::time_t kDay = 86400;

GameEntry entry(std::int64_t id, const std::string& genre = {}, std::int64_t creator = 0) {
    GameEntry e;
    e.universeId = id;
    e.placeId = id * 10;
    e.name = "Game " + std::to_string(id);
    e.genre = genre;
    e.creatorId = creator;
    return e;
}

api::GameInfo game(std::int64_t id, const std::string& genre, std::int64_t creator, std::int64_t playing,
                   std::int64_t up = 0, std::int64_t down = 0) {
    api::GameInfo g;
    g.universeId = id;
    g.genre = genre;
    g.creatorId = creator;
    g.playing = playing;
    g.upVotes = up;
    g.downVotes = down;
    return g;
}
}  // namespace

TEST_CASE("recent list: most recent first, play count, bounded") {
    GameList list({}, 3);
    list.touch(entry(1), kNow);
    list.touch(entry(2), kNow + 1);
    list.touch(entry(1), kNow + 2);
    REQUIRE(list.items().size() == 2);
    CHECK(list.items()[0].universeId == 1);
    CHECK(list.items()[0].playCount == 2);
    CHECK(list.items()[0].lastPlayed == kNow + 2);
    CHECK(list.items()[0].added == kNow);

    list.touch(entry(3), kNow + 3);
    list.touch(entry(4), kNow + 4);
    REQUIRE(list.items().size() == 3);
    CHECK_FALSE(list.contains(2));  // oldest dropped
    list.setMaxSize(1);
    CHECK(list.items().size() == 1);
    CHECK(list.remove(4));
    CHECK_FALSE(list.remove(4));
}

TEST_CASE("favorites: unbounded, no duplicates, persisted") {
    TempDir dir;
    const auto file = dir.path() / "favorites.json";
    {
        GameList favs(file, 0);
        for (int i = 1; i <= 30; ++i) favs.add(entry(i, "RPG", 7), kNow + i);
        favs.add(entry(5), kNow + 100);
        CHECK(favs.items().size() == 30);
        REQUIRE(favs.save());
    }
    GameList favs(file, 0);
    REQUIRE(favs.load() == settings::LoadStatus::Ok);
    REQUIRE(favs.items().size() == 30);
    CHECK(favs.items().front().universeId == 30);
    CHECK(favs.find(5)->genre == "RPG");
    CHECK(favs.find(5)->creatorId == 7);
}

TEST_CASE("taste profile weights recent plays more") {
    auto rpg = entry(1, "RPG", 100);
    rpg.playCount = 1;
    rpg.lastPlayed = kNow;
    auto obby = entry(2, "Obby", 200);
    obby.playCount = 1;
    obby.lastPlayed = kNow - 28 * kDay;  // two half-lives: weight 0.25
    const auto p = buildProfile({rpg, obby}, kNow);
    CHECK(p.genres.at("RPG") == doctest::Approx(0.8));
    CHECK(p.genres.at("Obby") == doctest::Approx(0.2));
    CHECK(p.creators.at(100) == doctest::Approx(0.8));
    CHECK(p.played.count(1));
    CHECK_FALSE(p.empty());
}

TEST_CASE("recommendations follow the profile and exclude played games") {
    auto played = entry(1, "RPG", 100);
    played.playCount = 3;
    played.lastPlayed = kNow;
    const auto profile = buildProfile({played}, kNow);

    const std::vector<api::GameInfo> candidates = {
        game(1, "RPG", 100, 1'000'000),  // already played
        game(2, "Shooter", 300, 500'000, 90, 10),
        game(3, "RPG", 400, 1'000),
        game(4, "Obby", 100, 1'000),  // same creator
        game(3, "RPG", 400, 1'000),   // duplicate
    };
    const auto out = recommend(profile, candidates, 10);
    REQUIRE(out.size() == 3);
    CHECK(out[0].game.universeId == 3);  // genre match wins
    CHECK(out[1].game.universeId == 4);  // creator match
    CHECK(out[2].game.universeId == 2);
    CHECK(recommend(profile, candidates, 1).size() == 1);
}

TEST_CASE("empty profile falls back to popularity") {
    const std::vector<api::GameInfo> candidates = {
        game(1, "A", 1, 100),
        game(2, "B", 2, 1'000'000),
        game(3, "C", 3, 10'000),
    };
    const auto out = recommend(TasteProfile{}, candidates, 10);
    REQUIRE(out.size() == 3);
    CHECK(out[0].game.universeId == 2);
    CHECK(out[1].game.universeId == 3);
    CHECK(out[2].game.universeId == 1);
}
