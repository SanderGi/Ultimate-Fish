/*
  Ultimate Fish - reproducible public-belief strength benchmark
  GPLv3 or later

  The production engine has one belief-search implementation: exact,
  history-preserving PublicBeliefState search. This benchmark keeps a
  deliberately weak single-determinization player entirely out of production
  code so optimizations can be compared in color-balanced games without
  reintroducing a sampled search API.
*/

#include "information.h"
#include "position.h"
#include "search.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace Stockfish::Ultimate;

namespace {

enum class AgentMode { Exact, Determinize };

struct BenchmarkOptions {
    int depth = 3;
    std::uint64_t nodes = 3'000;
    int maxPlies = 24;
};

struct GameResult {
    AgentMode mode = AgentMode::Exact;
    Color focalColor = Color::White;
    std::string actual;
    double points = 0.5;
    int plies = 0;
    std::uint64_t focalNodes = 0;
    std::uint64_t opponentNodes = 0;
    std::string firstAction;
    std::string termination;
};

struct Summary {
    int wins = 0;
    int draws = 0;
    int losses = 0;
    double points = 0.0;
    std::uint64_t nodes = 0;
};

bool parse_positive(std::string_view text, std::uint64_t& value) {
    const auto [end, error] = std::from_chars(
      text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() && value > 0;
}

int mirror_square(int square) {
    const int file = square % Position::BoardFiles;
    const int rank = square / Position::BoardFiles;
    return (Position::BoardRanks - 1 - rank) * Position::BoardFiles + file;
}

Position trap_world(Color focal, int enemyGhostSquare) {
    const bool white = focal == Color::White;
    const auto square = [white](std::string_view name) {
        const int original = Position::square_from_name(name);
        return white ? original : mirror_square(original);
    };
    const Color enemy = ~focal;
    Position position;
    position.add_piece(PieceType::King, focal, square("a1"));
    position.add_piece(PieceType::Rook, focal, square("b2"));
    position.add_piece(PieceType::Queen, focal, square("d4"));
    position.add_piece(PieceType::King, enemy, square("e10"));
    position.add_piece(PieceType::Rook, enemy, square("h8"));
    position.add_piece(PieceType::Pawn, enemy, square("e5"));
    const int ghost = position.add_piece(
      PieceType::Ghost, enemy,
      white ? enemyGhostSquare : mirror_square(enemyGhostSquare));
    position.piece(ghost).visible = false;
    position.set_side_to_move(focal);
    return position;
}

PublicBeliefState trap_belief(Color focal) {
    PublicBeliefState belief({focal, false});
    std::string error;
    const Position safe = trap_world(
      focal, Position::square_from_name("a9"));
    const Position unsafe = trap_world(
      focal, Position::square_from_name("f6"));
    if (!belief.add(safe, &error) || !belief.add(unsafe, &error))
        throw std::runtime_error("could not construct trap belief: " + error);
    return belief;
}

bool contains_actual(const std::vector<Position>& worlds,
                     const std::string& actualUpn) {
    return std::any_of(worlds.begin(), worlds.end(), [&](const Position& world) {
        return world.upn() == actualUpn;
    });
}

PublicBeliefState belief_from_worlds(
  const DisclosureContext& disclosure,
  const std::vector<Position>& worlds) {
    PublicBeliefState result(disclosure);
    std::string error;
    for (const Position& world : worlds)
        if (!result.add(world, &error))
            throw std::runtime_error("invalid successor belief: " + error);
    if (result.empty())
        throw std::runtime_error("successor observation is empty");
    return result;
}

PublicBeliefState condition_actual_decision(
  const PublicBeliefState& belief, const Position& actual) {
    const std::string actualUpn = actual.upn();
    const std::vector<BeliefDecisionBucket> cells = belief.decision_cells();
    const BeliefDecisionBucket* selected = nullptr;
    for (const BeliefDecisionBucket& cell : cells) {
        if (!contains_actual(cell.worlds, actualUpn))
            continue;
        if (selected)
            throw std::runtime_error("actual world appears in multiple decision cells");
        selected = &cell;
    }
    if (!selected)
        throw std::runtime_error("actual world is absent from decision cells");
    return belief_from_worlds(belief.disclosure(), selected->worlds);
}

PublicBeliefState observe_successor(const PublicBeliefState& belief,
                                    std::string_view action,
                                    const Position& actualAfter) {
    const BeliefSuccessorPartitions partitions = belief.successor_partitions(action);
    const std::string actualUpn = actualAfter.upn();
    const BeliefSuccessorBucket* selected = nullptr;
    for (const BeliefSuccessorBucket& bucket : partitions.buckets) {
        if (!contains_actual(bucket.worlds, actualUpn))
            continue;
        if (selected)
            throw std::runtime_error("actual successor appears in multiple observations");
        selected = &bucket;
    }
    if (!selected)
        throw std::runtime_error("actual successor is absent from observation buckets");
    return belief_from_worlds(belief.disclosure(), selected->worlds);
}

struct Choice {
    std::optional<std::string> action;
    std::uint64_t nodes = 0;
};

SearchLimits limits_for(const BenchmarkOptions& options) {
    SearchLimits limits;
    limits.depth = options.depth;
    limits.nodes = options.nodes;
    return limits;
}

Choice choose_focal(AgentMode mode, const PublicBeliefState& belief,
                    const BenchmarkOptions& options) {
    Search search(16);
    if (mode == AgentMode::Exact) {
        const BeliefSearchResult result = search.think_beliefs(
          belief, limits_for(options));
        if (!result.validInformationCell)
            throw std::runtime_error("benchmark failed to condition legal-dot cell");
        return {result.bestMove, result.nodes};
    }

    // Benchmark-only legacy approximation: search one canonical concrete
    // world and ignore every other compatible hidden state.
    Position representative = belief.concrete_worlds().begin()->second;
    const SearchResult result = search.think(representative, limits_for(options));
    return {result.bestMove
              ? std::optional<std::string>(
                  representative.move_to_string(*result.bestMove))
              : std::nullopt,
            result.nodes};
}

Choice choose_opponent(const Position& actual,
                       const BenchmarkOptions& options) {
    Position position = actual;
    Search search(16);
    const SearchResult result = search.think(position, limits_for(options));
    return {result.bestMove
              ? std::optional<std::string>(position.move_to_string(*result.bestMove))
              : std::nullopt,
            result.nodes};
}

double terminal_points(const Position& position, Color focal) {
    const std::optional<Color> winner = position.winner();
    if (!winner)
        return 0.5;
    return *winner == focal ? 1.0 : 0.0;
}

double adjudicate(Position position, Color focal,
                  const BenchmarkOptions& options) {
    SearchLimits limits;
    limits.depth = std::max(4, options.depth);
    limits.nodes = std::max<std::uint64_t>(4'000, options.nodes);
    Search search(16);
    const SearchResult result = search.think(position, limits);
    const int focalScore = position.side_to_move() == focal
                         ? result.score : -result.score;
    if (focalScore > 100)
        return 1.0;
    if (focalScore < -100)
        return 0.0;
    return 0.5;
}

GameResult play_game(AgentMode mode, Color focal, bool unsafeActual,
                     const BenchmarkOptions& options) {
    const int actualGhost = Position::square_from_name(
      unsafeActual ? "f6" : "a9");
    Position actual = trap_world(focal, actualGhost);
    PublicBeliefState belief = trap_belief(focal);
    GameResult result;
    result.mode = mode;
    result.focalColor = focal;
    result.actual = unsafeActual ? "unsafe" : "safe";

    for (int ply = 0; ply < options.maxPlies; ++ply) {
        if (actual.game_over()) {
            result.points = terminal_points(actual, focal);
            result.plies = ply;
            result.termination = "terminal";
            return result;
        }

        const bool focalTurn = actual.side_to_move() == focal;
        PublicBeliefState before = belief;
        Choice choice;
        if (focalTurn) {
            before = condition_actual_decision(belief, actual);
            choice = choose_focal(mode, before, options);
            result.focalNodes += choice.nodes;
            if (result.firstAction.empty() && choice.action)
                result.firstAction = *choice.action;
        }
        else {
            choice = choose_opponent(actual, options);
            result.opponentNodes += choice.nodes;
        }
        if (!choice.action) {
            result.points = actual.game_over()
                          ? terminal_points(actual, focal) : 0.5;
            result.plies = ply;
            result.termination = actual.game_over() ? "terminal" : "no-move";
            return result;
        }

        const std::optional<Move> move = actual.move_from_string(*choice.action);
        if (!move) {
            result.points = focalTurn ? 0.0 : 1.0;
            result.plies = ply;
            result.termination = "illegal-determinization";
            return result;
        }
        Undo undo;
        if (!actual.make_move(*move, undo))
            throw std::runtime_error("chosen action failed to apply");
        belief = observe_successor(before, *choice.action, actual);
    }

    result.points = adjudicate(actual, focal, options);
    result.plies = options.maxPlies;
    result.termination = "adjudicated";
    return result;
}

Summary summarize(const std::vector<GameResult>& games, AgentMode mode) {
    Summary result;
    for (const GameResult& game : games) {
        if (game.mode != mode)
            continue;
        result.points += game.points;
        result.nodes += game.focalNodes;
        if (game.points > 0.75)
            ++result.wins;
        else if (game.points < 0.25)
            ++result.losses;
        else
            ++result.draws;
    }
    return result;
}

std::string_view mode_name(AgentMode mode) {
    return mode == AgentMode::Exact ? "exact" : "single-determinization";
}

std::string_view color_name(Color color) {
    return color == Color::White ? "white" : "black";
}

void print_summary(std::string_view name, const Summary& summary) {
    std::cout << "\"" << name << "\":{";
    std::cout << "\"wins\":" << summary.wins << ',';
    std::cout << "\"draws\":" << summary.draws << ',';
    std::cout << "\"losses\":" << summary.losses << ',';
    std::cout << "\"points\":" << summary.points << ',';
    std::cout << "\"nodes\":" << summary.nodes << '}';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        BenchmarkOptions options;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (index + 1 >= argc)
                throw std::runtime_error("benchmark option requires a value");
            std::uint64_t value = 0;
            if (!parse_positive(argv[++index], value))
                throw std::runtime_error("benchmark values must be positive integers");
            if (argument == "--depth")
                options.depth = static_cast<int>(value);
            else if (argument == "--nodes")
                options.nodes = value;
            else if (argument == "--plies")
                options.maxPlies = static_cast<int>(value);
            else
                throw std::runtime_error("unknown benchmark option");
        }

        std::vector<GameResult> games;
        for (const AgentMode mode : {AgentMode::Exact, AgentMode::Determinize})
            for (const Color color : {Color::White, Color::Black})
                for (const bool unsafe : {false, true})
                    games.push_back(play_game(mode, color, unsafe, options));

        const Summary exact = summarize(games, AgentMode::Exact);
        const Summary determinized = summarize(games, AgentMode::Determinize);
        const bool exactRobust = std::all_of(
          games.begin(), games.end(), [](const GameResult& game) {
              return game.mode != AgentMode::Exact ||
                     (game.firstAction != "d4-e5" &&
                      game.firstAction != "d7-e6");
          });
        const bool determinizationExposed = std::all_of(
          games.begin(), games.end(), [](const GameResult& game) {
              return game.mode != AgentMode::Determinize ||
                     game.firstAction == "d4-e5" ||
                     game.firstAction == "d7-e6";
          });
        std::cout << '{';
        std::cout << "\"schema\":\"ultimate-public-belief-match-v1\",";
        std::cout << "\"depth\":" << options.depth << ',';
        std::cout << "\"nodes_per_move\":" << options.nodes << ',';
        std::cout << "\"max_plies\":" << options.maxPlies << ',';
        std::cout << "\"games\":[";
        for (std::size_t index = 0; index < games.size(); ++index) {
            if (index)
                std::cout << ',';
            const GameResult& game = games[index];
            std::cout << '{'
                      << "\"mode\":\"" << mode_name(game.mode) << "\"," 
                      << "\"color\":\"" << color_name(game.focalColor) << "\"," 
                      << "\"actual\":\"" << game.actual << "\"," 
                      << "\"points\":" << game.points << ','
                      << "\"plies\":" << game.plies << ','
                      << "\"focal_nodes\":" << game.focalNodes << ','
                      << "\"opponent_nodes\":" << game.opponentNodes << ','
                      << "\"first_action\":\"" << game.firstAction << "\"," 
                      << "\"termination\":\"" << game.termination << "\"}";
        }
        std::cout << "],\"summary\":{";
        print_summary("exact", exact);
        std::cout << ',';
        print_summary("single_determinization", determinized);
        std::cout << "},\"color_balanced\":true,";
        std::cout << "\"exact_robust_root\":"
                  << (exactRobust ? "true" : "false") << ',';
        std::cout << "\"determinization_exposed\":"
                  << (determinizationExposed ? "true" : "false") << ',';
        std::cout << "\"exact_stronger\":"
                  << (exact.points > determinized.points ? "true" : "false")
                  << "}\n";
        return exactRobust && determinizationExposed
             ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    catch (const std::exception& error) {
        std::cerr << "Ultimate belief benchmark error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
