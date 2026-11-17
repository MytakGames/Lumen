#include <doctest.h>

#include "robloxapi/Parsers.h"

using namespace rl::api;
namespace parse = rl::api::parse;

TEST_CASE("games/v1/games details") {
    const auto games = parse::gameDetails(R"({"data":[
        {"id":994732206,"rootPlaceId":2753915549,"name":"Blox Fruits","description":"d",
         "creator":{"id":4372130,"name":"Gamer Robot Inc","type":"Group"},
         "playing":312000,"visits":"41000000000","maxPlayers":12,"favoritedCount":5000,
         "genre":"All","genre_l1":"RPG","updated":"2024-05-01T12:34:56.789Z"},
        {"id":0,"name":"no id, skipped"},
        "garbage"
    ]})");
    REQUIRE(games);
    REQUIRE(games->size() == 1);
    const GameInfo& g = games->front();
    CHECK(g.universeId == 994732206);
    CHECK(g.rootPlaceId == 2753915549);
    CHECK(g.creatorName == "Gamer Robot Inc");
    CHECK(g.creatorType == "Group");
    CHECK(g.genre == "RPG");
    CHECK(g.visits == 41000000000LL);  // numeric strings are accepted
    CHECK(g.maxPlayers == 12);
    CHECK(g.updated == 1714566896);
    CHECK(g.hasDetails);
}

TEST_CASE("legacy 'All' genre carries no signal") {
    const auto games = parse::gameDetails(R"({"data":[{"id":1,"genre":"All"}]})");
    REQUIRE(games);
    CHECK(games->front().genre.empty());
}

TEST_CASE("malformed documents yield nullopt") {
    CHECK_FALSE(parse::gameDetails("not json"));
    CHECK_FALSE(parse::gameDetails(R"({"data":{}})"));
    CHECK_FALSE(parse::votes("[]"));
    CHECK_FALSE(parse::omniSearch(R"({"nothing":1})"));
    CHECK_FALSE(parse::exploreSorts(R"({"x":[]})"));
    CHECK_FALSE(parse::universeId(R"({"universeId":null})"));
    CHECK_FALSE(parse::authenticatedUser(R"({"id":1})"));
}

TEST_CASE("votes and rating") {
    const auto v = parse::votes(R"({"data":[{"id":5,"upVotes":90,"downVotes":10},{"id":6}]})");
    REQUIRE(v);
    CHECK(v->at(5) == std::make_pair<std::int64_t, std::int64_t>(90, 10));
    GameInfo g;
    CHECK_FALSE(g.ratingPercent());
    g.upVotes = 90;
    g.downVotes = 10;
    CHECK(g.ratingPercent() == 90);
}

TEST_CASE("omni-search keeps only games, deduplicated") {
    const auto r = parse::omniSearch(R"({"searchResults":[
        {"contentGroupType":"Game","contents":[
            {"universeId":1,"rootPlaceId":11,"name":"A","playerCount":5,"totalUpVotes":3,"totalDownVotes":1},
            {"universeId":1,"rootPlaceId":11,"name":"A dup"},
            {"contentType":"User","universeId":9}]},
        {"contentGroupType":"User","contents":[{"universeId":2}]},
        {"contentGroupType":"Game","contents":[{"contentId":3,"placeId":33,"name":"C"}]}
    ]})");
    REQUIRE(r);
    REQUIRE(r->size() == 2);
    CHECK((*r)[0].name == "A");
    CHECK((*r)[0].playing == 5);
    CHECK((*r)[1].universeId == 3);  // contentId / placeId fallbacks
    CHECK((*r)[1].rootPlaceId == 33);
}

TEST_CASE("explore sorts and single sort content") {
    const auto sorts = parse::exploreSorts(R"({"sorts":[
        {"games":[{"universeId":1,"name":"A"},{"universeId":2,"name":"B"}]},
        {"topics":[]},
        {"games":[{"universeId":2,"name":"B again"},{"universeId":3,"name":"C"}]}]})");
    REQUIRE(sorts);
    CHECK(sorts->size() == 3);
    const auto single = parse::exploreSorts(R"({"games":[{"universeId":7}]})");
    REQUIRE(single);
    CHECK(single->front().universeId == 7);
}

TEST_CASE("thumbnails ignore pending images") {
    const auto t = parse::thumbnails(R"({"data":[
        {"targetId":1,"state":"Completed","imageUrl":"https://tr.rbxcdn.com/a"},
        {"targetId":2,"state":"Pending","imageUrl":""}]})");
    REQUIRE(t);
    CHECK(t->size() == 1);
    CHECK(t->at(1) == "https://tr.rbxcdn.com/a");

    const auto m = parse::multigetThumbnails(R"({"data":[{"universeId":4,"thumbnails":[
        {"state":"Blocked","imageUrl":"x"},{"state":"Completed","imageUrl":"https://tr.rbxcdn.com/b"}]}]})");
    REQUIRE(m);
    CHECK(m->at(4) == "https://tr.rbxcdn.com/b");
}

TEST_CASE("universe id and authenticated user") {
    CHECK(parse::universeId(R"({"universeId":994732206})") == 994732206);
    const auto u = parse::authenticatedUser(R"({"id":1234567890,"name":"someone"})");
    REQUIRE(u);
    CHECK(u->id == 1234567890LL);
    CHECK(u->displayName == "someone");  // falls back to the user name
}

TEST_CASE("quick login create and status") {
    const auto c = parse::quickLoginCreate(
        R"({"code":"A1B2C3","privateKey":"secret","expirationTime":"2024-05-01T12:39:56Z","imagePath":"x"})");
    REQUIRE(c);
    CHECK(c->code == "A1B2C3");
    CHECK(c->privateKey == "secret");
    CHECK(c->expires == 1714567196);
    CHECK_FALSE(parse::quickLoginCreate(R"({"code":"A1B2C3"})"));

    const auto linked = parse::quickLoginStatus(R"({"status":"UserLinked","accountName":"someone"})");
    REQUIRE(linked);
    CHECK(linked->status == QuickLoginStatus::UserLinked);
    CHECK(linked->accountName == "someone");
    CHECK(parse::quickLoginStatus(R"({"status":"Validated"})")->status == QuickLoginStatus::Validated);
    CHECK(parse::quickLoginStatus(R"({"status":"Cancelled"})")->status == QuickLoginStatus::Cancelled);
    CHECK_FALSE(parse::quickLoginStatus(R"({"status":"Weird"})"));
}

TEST_CASE("session cookie from Set-Cookie headers") {
    const std::string headers =
        "RBXEventTrackerV2=CreateDate=1; path=/\n"
        ".ROBLOSECURITY=; domain=.roblox.com; expires=Thu, 01 Jan 1970 00:00:00 GMT\n"
        ".ROBLOSECURITY=_|WARNING:-DO-NOT-SHARE-THIS.--abc123; domain=.roblox.com; secure; HttpOnly";
    CHECK(parse::sessionCookie(headers) == "_|WARNING:-DO-NOT-SHARE-THIS.--abc123");
    CHECK(parse::sessionCookie("other=1; path=/").empty());
    CHECK(parse::sessionCookie(".ROBLOSECURITY=; expires=0").empty());
}

TEST_CASE("challenge detection and ISO 8601") {
    CHECK(parse::isChallenge(R"({"errors":[{"code":0,"message":"Challenge is required"}]})"));
    CHECK_FALSE(parse::isChallenge(R"({"errors":[{"code":1,"message":"Incorrect"}]})"));
    CHECK(parse::iso8601("1970-01-02T00:00:00Z") == 86400);
    CHECK(parse::iso8601("garbage") == 0);
}

TEST_CASE("GameInfo merge keeps known fields") {
    GameInfo a;
    a.universeId = 1;
    a.name = "Name";
    a.creatorName = "Creator";
    GameInfo b;
    b.universeId = 1;
    b.playing = 50;
    b.hasDetails = true;
    a.mergeFrom(b);
    CHECK(a.name == "Name");
    CHECK(a.creatorName == "Creator");
    CHECK(a.playing == 50);
    CHECK(a.hasDetails);
}
