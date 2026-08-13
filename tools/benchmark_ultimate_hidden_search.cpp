/*
  Ultimate Fish - deterministic hidden-information search microbenchmarks
  GPLv3 or later
*/

#include "position.h"
#include "search.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace Stockfish::Ultimate;

namespace {

struct Fixture {
    std::string name;
    PublicBeliefState beliefs;
    int depth = 1;
};

Position parse(std::string_view upn) {
    Position position;
    std::string error;
    if (!position.set_upn(upn, &error))
        throw std::runtime_error("invalid benchmark UPN: " + error);
    return position;
}

PublicBeliefState history_beliefs(
  Position actual, DisclosureContext disclosure,
  const std::vector<int>& royalCandidates = {}) {
    PublicHistoryState history;
    std::string error;
    const bool started = royalCandidates.empty()
      ? history.start(std::move(actual), disclosure, false, &error)
      : history.start(std::move(actual), disclosure, false,
                      royalCandidates, &error);
    if (!started)
        throw std::runtime_error("could not build benchmark belief: " + error);
    return history.beliefs();
}

std::vector<Fixture> fixtures() {
    const DisclosureContext onyx{Color::Black, false};
    std::vector<Fixture> result;
    result.push_back({
      "royal-two-world-endgame",
      history_beliefs(parse(
        "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
        "king,w,c1,0,0,0,0,0,1,-1,1,-1,0;"
        "jester,w,e1,0,0,0,0,0,1,-1,1,-1,0;"
        "king,b,d10,0,0,0,0,0,1,-1,1,-1,0"), onyx),
      16});

    const std::string mixed =
      "b;hm=41;fm=19;ep=-;cont=0;forced=-1;epv=-1;"
      "king,w,h1,0,0,0,0,1,1,-1,1,-1,0;"
      "jester,w,a1,0,0,0,0,0,1,-1,1,-1,0;"
      "rook,w,e1,0,0,0,0,1,1,-1,1,-1,0;"
      "berserker,w,e4,0,0,0,0,1,1,-1,1,-1,0;"
      "pawn,w,a3,0,0,0,0,0,1,-1,1,-1,0;"
      "pawn,w,b3,0,0,0,0,0,1,-1,1,-1,0;"
      "pawn,w,c3,0,0,0,0,0,1,-1,1,-1,0;"
      "sniper,w,g2,0,0,0,0,1,1,-1,1,-1,0;"
      "pawn,w,f3,0,0,0,0,0,1,-1,1,-1,0;"
      "king,b,d10,0,0,0,0,0,1,-1,1,-1,0;"
      "prince,b,f5,0,0,0,0,1,1,-1,1,-1,0;"
      "prince,b,d9,0,0,0,0,1,1,-1,1,-1,0";
    result.push_back({
      "royal-two-world-mixed-midgame",
      history_beliefs(parse(mixed), onyx,
        {Position::square_from_name("h1"),
         Position::square_from_name("a1")}),
      4});

    result.push_back({
      "ghost-75-world-endgame",
      history_beliefs(parse(
        "b;king,w,a1;"
        "ghost,w,c3,0,0,0,0,0,0,-1,1,-1,0;king,b,h10"), onyx),
      4});
    result.push_back({
      "ghost-73-world-conditioned-midgame",
      history_beliefs(parse(
        "b;king,w,d8;king,b,d10;"
        "ghost,w,b10,0,0,0,0,0,0,-1,1,-1,0"), onyx),
      2});
    result.push_back({
      "ghost-pair-2775-world-endgame",
      history_beliefs(parse(
        "b;king,w,a1;"
        "ghost,w,c2,0,0,0,0,0,0,-1,1,-1,0;"
        "ghost,w,d2,0,0,0,0,0,0,-1,1,-1,0;king,b,h10"), onyx),
      3});

    result.push_back({
      "royal-plus-ghost-148-world-endgame",
      history_beliefs(parse(
        "b;king,w,a1;jester,w,b1;"
        "ghost,w,c3,0,0,0,0,0,0,-1,1,-1,0;king,b,h10"), onyx),
      4});

    Position reexpansion = parse(
      "w;king,w,a1;"
      "ghost,w,c3,0,0,0,0,0,1,-1,1,-1,0;king,b,h10");
    PublicBeliefState singleton(onyx);
    std::string error;
    if (!singleton.add(std::move(reexpansion), &error))
        throw std::runtime_error("could not build singleton fixture: " + error);
    result.push_back({"ghost-visible-singleton-reexpands", singleton, 2});
    return result;
}

void print_result(const Fixture& fixture, const BeliefSearchResult& result,
                  std::int64_t wallMilliseconds, bool factored, bool comma) {
    if (comma)
        std::cout << ',';
    std::cout << '{'
              << "\"name\":\"" << fixture.name << "\","
              << "\"depth\":" << fixture.depth << ','
              << "\"mode\":\"" << (factored ? "factored" : "enumerated-oracle")
              << "\","
              << "\"root_beliefs\":" << fixture.beliefs.size() << ','
              << "\"score\":" << result.score << ','
              << "\"nodes\":" << result.nodes << ','
              << "\"engine_ms\":" << result.elapsed.count() << ','
              << "\"wall_ms\":" << wallMilliseconds << ','
              << "\"belief_nodes\":" << result.beliefNodes << ','
              << "\"belief_tt_hits\":" << result.beliefTtHits << ','
              << "\"singleton_handoffs\":" << result.singletonHandoffs << ','
              << "\"observation_buckets\":" << result.observationBuckets << ','
              << "\"peak_beliefs\":" << result.peakBeliefs << ','
              << "\"bestmove\":\""
              << (result.bestMove ? *result.bestMove : "(none)") << "\"}";
}

}  // namespace

int main() {
    try {
        const std::vector<Fixture> suite = fixtures();
        std::cout << "{\"schema\":\"ultimate-hidden-search-benchmark-v2\","
                     "\"deterministic\":true,\"seed\":0,\"fixtures\":[";
        bool comma = false;
        for (const Fixture& fixture : suite) {
            for (const bool factored : {false, true}) {
                Search search(16);
                SearchLimits limits;
                limits.depth = fixture.depth;
                limits.factoredBeliefs = factored;
                const auto start = std::chrono::steady_clock::now();
                const BeliefSearchResult result = search.think_beliefs(
                  fixture.beliefs, limits, true);
                const auto elapsed = std::chrono::duration_cast<
                  std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                print_result(fixture, result, elapsed, factored, comma);
                comma = true;
            }
        }
        std::cout << "]}\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "hidden-search benchmark failed: "
                  << exception.what() << '\n';
        return 1;
    }
}
