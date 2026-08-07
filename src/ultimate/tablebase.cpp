/*
  Ultimate Fish exact retrograde tablebase generator
  GPLv3 or later
*/

#include "position.h"
#include "tablebase_probe.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t SquareCount = Position::BoardSquares;
constexpr std::uint32_t PlacementStateCount =
  2 * SquareCount * (SquareCount - 1) * (SquareCount - 2);

enum class Wdl : std::uint8_t { Unknown, Win, Loss, Draw };

struct State {
    Color side;
    std::uint8_t whiteKing;
    std::uint8_t blackKing;
    std::uint8_t attacker;
    std::uint8_t substate = 0;
};

struct FourState {
    Color side;
    std::uint8_t whiteKing;
    std::uint8_t blackKing;
    std::uint8_t first;
    std::uint8_t second;
};

constexpr std::uint32_t FourPlacementStateCount =
  2 * (SquareCount / 2) * (SquareCount - 1) * (SquareCount - 2) * (SquareCount - 3);
constexpr std::uint32_t IdenticalFourStateCount = FourPlacementStateCount / 2;

std::uint8_t horizontal_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>((square / 8) * 8 + 7 - square % 8);
}

FourState canonicalize(FourState state) {
    if (state.whiteKing % 8 >= 4) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.first = horizontal_reflection(state.first);
        state.second = horizontal_reflection(state.second);
    }
    return state;
}

std::uint32_t rank_excluding(std::uint8_t square,
                             std::initializer_list<std::uint8_t> used) {
    std::uint32_t rank = square;
    for (const std::uint8_t occupied : used)
        rank -= occupied < square;
    return rank;
}

std::uint8_t unrank_excluding(std::uint32_t rank,
                              std::initializer_list<std::uint8_t> used) {
    for (std::uint8_t square = 0; square < SquareCount; ++square) {
        bool occupied = false;
        for (const std::uint8_t item : used)
            occupied = occupied || item == square;
        if (!occupied && rank-- == 0)
            return square;
    }
    throw std::runtime_error("four-model square rank is invalid");
}

std::uint32_t encode_four(FourState state) {
    state = canonicalize(state);
    const std::uint32_t whiteRank = (state.whiteKing / 8) * 4 + state.whiteKing % 8;
    const std::uint32_t blackRank = rank_excluding(state.blackKing, {state.whiteKing});
    const std::uint32_t firstRank = rank_excluding(
      state.first, {state.whiteKing, state.blackKing});
    const std::uint32_t secondRank = rank_excluding(
      state.second, {state.whiteKing, state.blackKing, state.first});
    return ((((static_cast<std::uint32_t>(state.side) * (SquareCount / 2) + whiteRank)
               * (SquareCount - 1) + blackRank)
              * (SquareCount - 2) + firstRank)
             * (SquareCount - 3) + secondRank);
}

FourState decode_four(std::uint32_t index) {
    const std::uint32_t secondRank = index % (SquareCount - 3);
    index /= SquareCount - 3;
    const std::uint32_t firstRank = index % (SquareCount - 2);
    index /= SquareCount - 2;
    const std::uint32_t blackRank = index % (SquareCount - 1);
    index /= SquareCount - 1;
    const std::uint32_t whiteRank = index % (SquareCount / 2);
    const Color side = static_cast<Color>(index / (SquareCount / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / 4) * 8 + whiteRank % 4);
    const std::uint8_t blackKing = unrank_excluding(blackRank, {whiteKing});
    const std::uint8_t first = unrank_excluding(firstRank, {whiteKing, blackKing});
    const std::uint8_t second = unrank_excluding(
      secondRank, {whiteKing, blackKing, first});
    return {side, whiteKing, blackKing, first, second};
}

std::uint32_t encode_identical_four(FourState state) {
    state = canonicalize(state);
    const std::uint32_t whiteRank = (state.whiteKing / 8) * 4 + state.whiteKing % 8;
    const std::uint32_t blackRank = rank_excluding(state.blackKing, {state.whiteKing});
    std::uint32_t firstRank = rank_excluding(
      state.first, {state.whiteKing, state.blackKing});
    std::uint32_t secondRank = rank_excluding(
      state.second, {state.whiteKing, state.blackKing});
    if (firstRank > secondRank)
        std::swap(firstRank, secondRank);
    constexpr std::uint32_t remaining = SquareCount - 2;
    const std::uint32_t pairRank =
      firstRank * (2 * remaining - firstRank - 1) / 2 + secondRank - firstRank - 1;
    constexpr std::uint32_t pairs = remaining * (remaining - 1) / 2;
    return ((static_cast<std::uint32_t>(state.side) * (SquareCount / 2) + whiteRank)
             * (SquareCount - 1) + blackRank) * pairs + pairRank;
}

FourState decode_identical_four(std::uint32_t index) {
    constexpr std::uint32_t remaining = SquareCount - 2;
    constexpr std::uint32_t pairs = remaining * (remaining - 1) / 2;
    const std::uint32_t pairRank = index % pairs;
    index /= pairs;
    const std::uint32_t blackRank = index % (SquareCount - 1);
    index /= SquareCount - 1;
    const std::uint32_t whiteRank = index % (SquareCount / 2);
    const Color side = static_cast<Color>(index / (SquareCount / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / 4) * 8 + whiteRank % 4);
    const std::uint8_t blackKing = unrank_excluding(blackRank, {whiteKing});
    std::uint32_t firstRank = 0;
    std::uint32_t residual = pairRank;
    for (; firstRank + 1 < remaining; ++firstRank) {
        const std::uint32_t row = remaining - firstRank - 1;
        if (residual < row)
            break;
        residual -= row;
    }
    const std::uint32_t secondRank = firstRank + 1 + residual;
    const std::uint8_t first = unrank_excluding(
      firstRank, {whiteKing, blackKing});
    const std::uint8_t second = unrank_excluding(
      secondRank, {whiteKing, blackKing});
    return {side, whiteKing, blackKing, first, second};
}

void self_test_four_codec() {
    for (std::uint32_t index = 0; index < FourPlacementStateCount; ++index) {
        const FourState state = decode_four(index);
        if (state.whiteKing % 8 >= 4 || state.whiteKing == state.blackKing ||
            state.whiteKing == state.first || state.whiteKing == state.second ||
            state.blackKing == state.first || state.blackKing == state.second ||
            state.first == state.second || encode_four(state) != index)
            throw std::runtime_error("four-model symmetry codec is not bijective");
    }
    // Probing may arrive in either horizontal orientation.
    FourState sample{Color::Black, 73, 4, 17, 62};
    FourState reflected = sample;
    reflected.whiteKing = horizontal_reflection(reflected.whiteKing);
    reflected.blackKing = horizontal_reflection(reflected.blackKing);
    reflected.first = horizontal_reflection(reflected.first);
    reflected.second = horizontal_reflection(reflected.second);
    if (encode_four(sample) != encode_four(reflected))
        throw std::runtime_error("four-model horizontal orbit mismatch");
    for (std::uint32_t index = 0; index < IdenticalFourStateCount; ++index) {
        const FourState state = decode_identical_four(index);
        FourState swapped = state;
        std::swap(swapped.first, swapped.second);
        if (encode_identical_four(state) != index ||
            encode_identical_four(swapped) != index)
            throw std::runtime_error("identical four-model codec is not bijective");
    }
    std::cout << "fourcodecok states " << FourPlacementStateCount << '\n';
    std::cout << "identicalfourcodecok states " << IdenticalFourStateCount << '\n';
}

struct Node {
    Wdl wdl = Wdl::Unknown;
    std::uint16_t dtw = 0;
    std::uint16_t remaining = 0;
    std::uint16_t longestWinChild = 0;
};

std::uint32_t encode_placement(const State& state) {
    const std::uint32_t blackRank = state.blackKing - (state.blackKing > state.whiteKing);
    const std::uint8_t low = std::min(state.whiteKing, state.blackKing);
    const std::uint8_t high = std::max(state.whiteKing, state.blackKing);
    const std::uint32_t attackerRank = state.attacker - (state.attacker > low)
                                                    - (state.attacker > high);
    return (((static_cast<std::uint32_t>(state.side) * SquareCount + state.whiteKing)
             * (SquareCount - 1) + blackRank)
            * (SquareCount - 2) + attackerRank);
}

State decode_placement(std::uint32_t index) {
    const std::uint32_t attackerRank = index % (SquareCount - 2);
    index /= SquareCount - 2;
    const std::uint32_t blackRank = index % (SquareCount - 1);
    index /= SquareCount - 1;
    const std::uint8_t whiteKing = index % SquareCount;
    const Color side = static_cast<Color>(index / SquareCount);
    const std::uint8_t blackKing = blackRank + (blackRank >= whiteKing);
    const std::uint8_t low = std::min(whiteKing, blackKing);
    const std::uint8_t high = std::max(whiteKing, blackKing);
    std::uint8_t attacker = attackerRank;
    if (attacker >= low)
        ++attacker;
    if (attacker >= high)
        ++attacker;
    return {side, whiteKing, blackKing, attacker};
}

const char* wdl_name(Wdl wdl) {
    switch (wdl) {
    case Wdl::Win: return "win";
    case Wdl::Loss: return "loss";
    case Wdl::Draw: return "draw";
    default: return "unknown";
    }
}

bool closed_position_only_attacker(PieceType type) {
    switch (type) {
    case PieceType::Jester:
    case PieceType::Queen:
    case PieceType::Rook:
    case PieceType::Bomb:
    case PieceType::Ninja:
    case PieceType::Parasite:
    case PieceType::Giant:
    case PieceType::Dragon:
    case PieceType::Berserker:
    case PieceType::Ghost:
    case PieceType::Sniper:
    case PieceType::Prince:
    case PieceType::Pawn:
    case PieceType::Penguin:
    case PieceType::Copycat:
        return true;
    default:
        return false;
    }
}

bool stateless_four_piece(PieceType type) {
    switch (type) {
    case PieceType::Jester:
    case PieceType::Knight:
    case PieceType::Queen:
    case PieceType::Rook:
    case PieceType::Bishop:
    case PieceType::Bomb:
    case PieceType::Ninja:
    case PieceType::Turtle:
    case PieceType::Parasite:
    case PieceType::Mage:
    case PieceType::Giant:
    case PieceType::Fisherman:
    case PieceType::Dragon: return true;
    default: return false;
    }
}

bool closed_four_piece(PieceType type) {
    if (stateless_four_piece(type))
        return true;
    switch (type) {
    case PieceType::Pawn:
    case PieceType::Berserker:
    case PieceType::Ghost:
    case PieceType::Penguin:
    case PieceType::Sniper:
    case PieceType::Prince:
    case PieceType::Checker: return true;
    default: return false;
    }
}

std::uint32_t substate_count(PieceType type) {
    switch (type) {
    case PieceType::Berserker: return 10;  // power 0..8, then board-saturating 9+
    case PieceType::Ghost: return 2;
    case PieceType::Sniper: return 4;
    case PieceType::Prince: return 2;
    case PieceType::Checker: return 4;  // ordinary/promoted x normal/forced jump
    case PieceType::Pawn: return 2;
    case PieceType::Penguin: return 12;
    default: return 1;
    }
}

}  // namespace

class TablebaseGenerator {
   public:
    TablebaseGenerator(PieceType attackerType, PieceType secondaryType,
                       Color secondaryColor, std::string output,
                       std::string checkpoint, std::uint32_t checkpointEvery) :
        attackerType_(attackerType), output_(std::move(output)),
        checkpoint_(std::move(checkpoint)), checkpointEvery_(checkpointEvery),
        secondaryType_(attackerType == PieceType::Copycat
                         ? PieceType::CopycatClone : secondaryType),
        secondaryColor_(secondaryColor),
        fourModels_(secondaryType_ != PieceType::Count),
        identicalExtras_(secondaryType == attackerType && secondaryColor == Color::White),
        primarySubstates_(substate_count(attackerType)),
        secondarySubstates_(fourModels_ ? substate_count(secondaryType_) : 1),
        substates_(primarySubstates_ * secondarySubstates_),
        stateCount_(attackerType_ == PieceType::Copycat ? PlacementStateCount
                    : identicalExtras_ ? IdenticalFourStateCount * substates_
                    : fourModels_ ? FourPlacementStateCount * substates_
                               : PlacementStateCount * substates_),
        nodes_(stateCount_), predecessorCounts_(stateCount_) {}

    std::uint32_t encode(const State& state) const {
        return encode_placement(state) * substates_ + state.substate;
    }

    State decode(std::uint32_t index) const {
        const std::uint8_t substate = index % substates_;
        State result = decode_placement(index / substates_);
        result.substate = substate;
        return result;
    }

    void self_test() const {
        if (fourModels_ && attackerType_ != PieceType::Copycat) {
            self_test_four_codec();
            constexpr std::uint32_t samples = 20'000;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const std::uint32_t index = static_cast<std::uint32_t>(
                  std::uint64_t(stateCount_) * sample / samples);
                Position position;
                if (make_position_at(index, position) && child_index(position) != index)
                    throw std::runtime_error("four-model substate codec is not bijective");
            }
            std::cout << "foursubstatecodecok samples " << samples << '\n';
            return;
        }
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            const State state = decode(index);
            if (state.whiteKing == state.blackKing || state.whiteKing == state.attacker ||
                state.blackKing == state.attacker || encode(state) != index)
                throw std::runtime_error("tablebase state codec is not bijective");
        }
        std::cout << "codecok states " << stateCount_ << '\n';
    }

    void dry_run(std::uint32_t count) const {
        count = std::min(count, stateCount_);
        std::uint64_t edges = 0;
        for (std::uint32_t index = 0; index < count; ++index) {
            Position position;
            if (!make_position_at(index, position))
                continue;
            for (const Move& move : position.legal_moves()) {
                Position child = position;
                if (!child.apply_move_unchecked(move))
                    throw std::runtime_error("legal tablebase move failed trusted application");
                ++edges;
                if (in_class(child))
                    (void) child_index(child);
            }
        }
        std::cout << "dryrun states " << count << " edges " << edges << '\n';
    }

    void inspect(std::uint32_t index) const {
        if (index >= stateCount_)
            throw std::runtime_error("inspect index is out of range");
        Position position;
        if (!make_position_at(index, position)) {
            std::cout << "invalid geometry\n";
            return;
        }
        std::cout << "state " << index << ' ' << position.upn() << '\n';
        for (const Move& move : position.legal_moves()) {
            Position child = position;
            child.apply_move_unchecked(move);
            std::cout << position.move_to_string(move) << " -> " << child.upn();
            if (const auto result = TablebaseProbe::probe(child))
                std::cout << " tb " << static_cast<int>(result->wdl) << '/' << result->dtw;
            std::cout << '\n';
        }
    }

    void generate() {
        const auto start = std::chrono::steady_clock::now();
        std::uint32_t begin = load_checkpoint();
        for (std::uint32_t index = begin; index < stateCount_; ++index) {
            analyze_node(index, true, [](std::uint32_t) {});
            if (checkpointEvery_ && (index + 1) % checkpointEvery_ == 0) {
                save_checkpoint(index + 1);
                progress("frontier", index + 1, start);
            }
        }
        save_checkpoint(stateCount_);

        std::uint64_t edgeCount = 0;
        for (const std::uint32_t count : predecessorCounts_)
            edgeCount += count;
        const auto solve = [&](auto offsetZero) {
            using Offset = decltype(offsetZero);
            std::vector<Offset> offsets(stateCount_ + 1, 0);
            for (std::uint32_t index = 0; index < stateCount_; ++index)
                offsets[index + 1] = static_cast<Offset>(offsets[index] +
                                                         predecessorCounts_[index]);
            std::vector<std::uint32_t> predecessors(edgeCount);
            std::vector<Offset> cursor(offsets.begin(), offsets.end() - 1);
            for (std::uint32_t index = 0; index < stateCount_; ++index) {
                analyze_node(index, false, [&](std::uint32_t child) {
                    predecessors[cursor[child]++] = index;
                });
                if (checkpointEvery_ && (index + 1) % checkpointEvery_ == 0)
                    progress("reverse", index + 1, start);
            }

            // DTW edges have unit cost. A Dial-style bucket queue preserves the
            // distance ordering required for shortest wins/longest losses without
            // paying O(log N) heap cost for tens of millions of solved states.
            std::vector<std::vector<std::uint32_t>> buckets(
              std::numeric_limits<std::uint16_t>::max() + 1ULL);
            for (std::uint32_t index = 0; index < stateCount_; ++index)
                if (nodes_[index].wdl == Wdl::Win || nodes_[index].wdl == Wdl::Loss)
                    buckets[nodes_[index].dtw].push_back(index);
            for (std::uint32_t distance = 0; distance < buckets.size(); ++distance)
              for (std::size_t queued = 0; queued < buckets[distance].size(); ++queued) {
                const std::uint32_t child = buckets[distance][queued];
                const Node childNode = nodes_[child];
                if (distance != childNode.dtw)
                    continue;
                for (Offset edge = offsets[child]; edge < offsets[child + 1]; ++edge) {
                    Node& parent = nodes_[predecessors[edge]];
                    if (parent.wdl == Wdl::Win && childNode.wdl == Wdl::Loss) {
                        const std::uint16_t distance = static_cast<std::uint16_t>(
                          std::min<int>(std::numeric_limits<std::uint16_t>::max(),
                                        childNode.dtw + 1));
                        if (distance < parent.dtw) {
                            parent.dtw = distance;
                            buckets[parent.dtw].push_back(predecessors[edge]);
                        }
                        continue;
                    }
                    if (parent.wdl != Wdl::Unknown)
                        continue;
                    if (childNode.wdl == Wdl::Loss) {
                        parent.wdl = Wdl::Win;
                        parent.dtw = static_cast<std::uint16_t>(std::min<int>(
                          std::numeric_limits<std::uint16_t>::max(), childNode.dtw + 1));
                        buckets[parent.dtw].push_back(predecessors[edge]);
                    }
                    else if (childNode.wdl == Wdl::Win) {
                        if (parent.remaining)
                            --parent.remaining;
                        parent.longestWinChild = std::max(parent.longestWinChild, childNode.dtw);
                        if (!parent.remaining) {
                            parent.wdl = Wdl::Loss;
                            parent.dtw = static_cast<std::uint16_t>(std::min<int>(
                              std::numeric_limits<std::uint16_t>::max(),
                              parent.longestWinChild + 1));
                            buckets[parent.dtw].push_back(predecessors[edge]);
                        }
                    }
                }
              }
            for (Node& node : nodes_)
                if (node.wdl == Wdl::Unknown)
                    node.wdl = Wdl::Draw;
        };
        if (edgeCount <= std::numeric_limits<std::uint32_t>::max())
            solve(std::uint32_t{});
        else
            solve(std::uint64_t{});
        verify_solution();
        write_output(edgeCount);
        progress("complete", stateCount_, start);
    }

   private:
    static PieceType represented_type(PieceType type, std::uint32_t substate) {
        return type == PieceType::Checker && (substate & 2)
          ? PieceType::CheckerKing : type;
    }

    static bool type_matches(PieceType represented, PieceType actual) {
        return represented == PieceType::Checker
          ? actual == PieceType::Checker || actual == PieceType::CheckerKing
          : actual == represented;
    }

    bool apply_substate(Position& position, int id, PieceType type,
                        std::uint32_t substate) const {
        switch (type) {
        case PieceType::Berserker: position.piece(id).power = substate; break;
        case PieceType::Ghost: position.piece(id).visible = substate != 0; break;
        case PieceType::Sniper: position.piece(id).cooldown = substate; break;
        case PieceType::Prince:
            if (substate) {
                if (position.continuation_ != Continuation::None)
                    return false;
                position.continuation_ = Continuation::PrinceSecondMove;
                position.forcedPiece_ = id;
            }
            break;
        case PieceType::Checker:
            if (substate & 1) {
                if (position.continuation_ != Continuation::None)
                    return false;
                position.continuation_ = Continuation::CheckerJump;
                position.forcedPiece_ = id;
            }
            break;
        case PieceType::Pawn: position.piece(id).moved = substate != 0; break;
        case PieceType::Penguin:
            position.piece(id).cooldown = substate / 2;
            break;
        default: break;
        }
        return true;
    }

    std::uint32_t piece_substate(const Position& position, int id,
                                 PieceType type) const {
        switch (type) {
        case PieceType::Berserker:
            return std::min<std::uint32_t>(position.piece(id).power, 9);
        case PieceType::Ghost: return position.piece(id).visible ? 1 : 0;
        case PieceType::Sniper: return position.piece(id).cooldown;
        case PieceType::Prince:
            return position.continuation_ == Continuation::PrinceSecondMove &&
                   position.forcedPiece_ == id;
        case PieceType::Checker:
            return (position.piece(id).type == PieceType::CheckerKing ? 2u : 0u) +
                   (position.continuation_ == Continuation::CheckerJump &&
                    position.forcedPiece_ == id ? 1u : 0u);
        case PieceType::Pawn: return position.piece(id).moved ? 1 : 0;
        case PieceType::Penguin:
            return position.piece(id).cooldown * 2 +
                   (position.piece(id).action ? 1u : 0u);
        default: return 0;
        }
    }

    bool make_position_at(std::uint32_t index, Position& position) const {
        if (!fourModels_)
            return make_position(decode(index), position);
        if (attackerType_ == PieceType::Copycat) {
            const State state = decode_placement(index);
            position.clear();
            const int whiteKing = position.add_piece(
              PieceType::King, Color::White, state.whiteKing);
            const int blackKing = position.add_piece(
              PieceType::King, Color::Black, state.blackKing);
            const int first = position.add_piece(
              PieceType::Copycat, Color::White, state.attacker);
            if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
                first == Position::NoPiece)
                return false;
            const int second = position.piece(first).link;
            for (int id : {whiteKing, blackKing, first, second})
                position.piece(id).moved = true;
            position.set_side_to_move(state.side);
            return true;
        }
        const std::uint32_t combinedSubstate = index % substates_;
        const std::uint32_t primarySubstate = combinedSubstate / secondarySubstates_;
        const std::uint32_t secondarySubstate = combinedSubstate % secondarySubstates_;
        const std::uint32_t placement = index / substates_;
        const FourState state = identicalExtras_ ? decode_identical_four(placement)
                                                 : decode_four(placement);
        position.clear();
        const int whiteKing = position.add_piece(PieceType::King, Color::White,
                                                  state.whiteKing);
        const int blackKing = position.add_piece(PieceType::King, Color::Black,
                                                  state.blackKing);
        const int first = position.add_piece(
          represented_type(attackerType_, primarySubstate), Color::White, state.first);
        int second = Position::NoPiece;
        second = position.add_piece(
          represented_type(secondaryType_, secondarySubstate), secondaryColor_, state.second);
        if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
            first == Position::NoPiece || second == Position::NoPiece)
            return false;
        for (int id : {whiteKing, blackKing, first, second})
            position.piece(id).moved = true;
        if (!apply_substate(position, first, attackerType_, primarySubstate) ||
            !apply_substate(position, second, secondaryType_, secondarySubstate))
            return false;
        for (const auto [id, type, substate] : {
               std::tuple<int, PieceType, std::uint32_t>{first, attackerType_, primarySubstate},
               {second, secondaryType_, secondarySubstate}}) {
            if (type == PieceType::Penguin && (substate & 1)) {
                position.apply_penguin_freeze(id);
                if (!position.piece(id).action)
                    return false;
            }
        }
        position.set_side_to_move(state.side);
        return true;
    }

    bool make_position(const State& state, Position& position) const {
        position.clear();
        const int whiteKing = position.add_piece(PieceType::King, Color::White,
                                                  state.whiteKing);
        const int blackKing = position.add_piece(PieceType::King, Color::Black,
                                                  state.blackKing);
        const int attacker = position.add_piece(attackerType_, Color::White,
                                                 state.attacker);
        if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
            attacker == Position::NoPiece)
            return false;
        // This first tablebase is the closed no-castling state class. Once a
        // King/Rook has moved the flag cannot become false, so every quiet
        // successor remains probeable in this same class.
        position.piece(whiteKing).moved = true;
        position.piece(blackKing).moved = true;
        position.piece(attacker).moved = true;
        switch (attackerType_) {
        case PieceType::Berserker:
            position.piece(attacker).power = state.substate;
            break;
        case PieceType::Ghost:
            position.piece(attacker).visible = state.substate != 0;
            break;
        case PieceType::Sniper:
            position.piece(attacker).cooldown = state.substate;
            break;
        case PieceType::Prince:
            if (state.substate) {
                position.continuation_ = Continuation::PrinceSecondMove;
                position.forcedPiece_ = attacker;
            }
            break;
        case PieceType::Pawn:
            position.piece(attacker).moved = state.substate != 0;
            break;
        case PieceType::Penguin:
            position.piece(attacker).cooldown = state.substate / 2;
            if (state.substate & 1) {
                position.apply_penguin_freeze(attacker);
                if (!position.piece(attacker).action)
                    return false;
            }
            break;
        default: break;
        }
        position.set_side_to_move(state.side);
        return true;
    }

    bool in_class(const Position& position) const {
        const bool primary = position.has_real_king(Color::White) &&
               position.has_real_king(Color::Black) &&
               position.piece(2).alive && position.piece(2).onBoard &&
               type_matches(attackerType_, position.piece(2).type);
        if (!primary || !fourModels_)
            return primary;
        return position.piece(3).alive && position.piece(3).onBoard &&
               type_matches(secondaryType_, position.piece(3).type);
    }

    std::uint32_t child_index(const Position& position) const {
        if (attackerType_ == PieceType::Copycat)
            return encode_placement({position.side_to_move(), position.piece(0).square,
                                     position.piece(1).square, position.piece(2).square});
        if (fourModels_) {
            std::uint32_t primarySubstate = piece_substate(position, 2, attackerType_);
            std::uint32_t secondarySubstate = piece_substate(position, 3, secondaryType_);
            const FourState state{position.side_to_move(), position.piece(0).square,
                                  position.piece(1).square, position.piece(2).square,
                                  position.piece(3).square};
            std::uint32_t placement = 0;
            if (identicalExtras_) {
                FourState canonical = canonicalize(state);
                const std::uint32_t firstRank = rank_excluding(
                  canonical.first, {canonical.whiteKing, canonical.blackKing});
                const std::uint32_t secondRank = rank_excluding(
                  canonical.second, {canonical.whiteKing, canonical.blackKing});
                if (firstRank > secondRank)
                    std::swap(primarySubstate, secondarySubstate);
                placement = encode_identical_four(state);
            }
            else placement = encode_four(state);
            return placement * substates_ +
                   primarySubstate * secondarySubstates_ + secondarySubstate;
        }
        std::uint8_t substate = 0;
        switch (attackerType_) {
        case PieceType::Berserker:
            substate = std::min<std::uint8_t>(position.piece(2).power, 9);
            break;
        case PieceType::Ghost: substate = position.piece(2).visible ? 1 : 0; break;
        case PieceType::Sniper: substate = position.piece(2).cooldown; break;
        case PieceType::Prince:
            substate = position.continuation_ == Continuation::PrinceSecondMove ? 1 : 0;
            break;
        case PieceType::Pawn: substate = position.piece(2).moved ? 1 : 0; break;
        case PieceType::Penguin:
            substate = static_cast<std::uint8_t>(position.piece(2).cooldown * 2 +
                                                 (position.piece(2).action ? 1 : 0));
            break;
        default: break;
        }
        return encode({position.side_to_move(), position.piece(0).square,
                       position.piece(1).square, position.piece(2).square, substate});
    }

    template<typename EdgeConsumer>
    void analyze_node(std::uint32_t index, bool initialize, EdgeConsumer&& consume) {
        Position position;
        if (!make_position_at(index, position)) {
            if (initialize)
                nodes_[index].wdl = Wdl::Draw;
            return;
        }
        if (!position.is_checkmate_possible()) {
            if (initialize)
                nodes_[index].wdl = Wdl::Draw;
            return;
        }
        const auto moves = position.legal_moves();
        if (initialize) {
            Node& node = nodes_[index];
            node = {};
            node.remaining = static_cast<std::uint16_t>(moves.size());
            if (moves.empty()) {
                const auto winner = position.winner();
                node.wdl = winner && *winner != position.side_to_move() ? Wdl::Loss : Wdl::Draw;
            }
        }
        for (const Move& move : moves) {
            Position child = position;
            if (!child.apply_move_unchecked(move))
                throw std::runtime_error("legal tablebase move failed trusted application");
            if (child.forced_timeout_winner() ||
                !child.has_real_king(Color::White) ||
                !child.has_real_king(Color::Black) ||
                !child.is_checkmate_possible()) {
                const auto winner = child.winner();
                if (initialize) {
                    if (winner && *winner == position.side_to_move()) {
                        nodes_[index].wdl = Wdl::Win;
                        nodes_[index].dtw = nodes_[index].dtw
                          ? std::min<std::uint16_t>(nodes_[index].dtw, 1) : 1;
                    }
                    else if (winner) {
                        if (nodes_[index].remaining)
                            --nodes_[index].remaining;
                        nodes_[index].longestWinChild = std::max<std::uint16_t>(
                          nodes_[index].longestWinChild, 0);
                        if (!nodes_[index].remaining && nodes_[index].wdl == Wdl::Unknown) {
                            nodes_[index].wdl = Wdl::Loss;
                            nodes_[index].dtw = static_cast<std::uint16_t>(
                              nodes_[index].longestWinChild + 1);
                        }
                    }
                }
                continue;
            }
            if (!in_class(child)) {
                if (initialize) {
                    const auto external = TablebaseProbe::probe(child);
                    if (external && external->wdl == TablebaseWdl::Loss) {
                        nodes_[index].wdl = Wdl::Win;
                        const std::uint16_t distance = static_cast<std::uint16_t>(external->dtw + 1);
                        nodes_[index].dtw = nodes_[index].dtw
                          ? std::min(nodes_[index].dtw, distance) : distance;
                    }
                    else if (external && external->wdl == TablebaseWdl::Win) {
                        if (nodes_[index].remaining)
                            --nodes_[index].remaining;
                        nodes_[index].longestWinChild = std::max<std::uint16_t>(
                          nodes_[index].longestWinChild, external->dtw);
                        if (!nodes_[index].remaining && nodes_[index].wdl == Wdl::Unknown) {
                            nodes_[index].wdl = Wdl::Loss;
                            nodes_[index].dtw = static_cast<std::uint16_t>(
                              nodes_[index].longestWinChild + 1);
                        }
                    }
                }
                continue;  // Captures enter K-v-K; promotions use a lower table.
            }
            const std::uint32_t successor = child_index(child);
            if (initialize)
                ++predecessorCounts_[successor];
            else
                consume(successor);
        }
    }

    void save_checkpoint(std::uint32_t processed) const {
        const std::string temporary = checkpoint_ + ".tmp";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("cannot write tablebase checkpoint");
        const std::array<char, 8> magic{{'U','F','T','B','C','P','2','\0'}};
        const std::uint32_t piece = static_cast<std::uint32_t>(attackerType_);
        stream.write(magic.data(), magic.size());
        stream.write(reinterpret_cast<const char*>(&piece), sizeof(piece));
        stream.write(reinterpret_cast<const char*>(&processed), sizeof(processed));
        stream.write(reinterpret_cast<const char*>(nodes_.data()),
                     nodes_.size() * sizeof(Node));
        stream.write(reinterpret_cast<const char*>(predecessorCounts_.data()),
                     predecessorCounts_.size() * sizeof(std::uint32_t));
        stream.close();
        if (std::rename(temporary.c_str(), checkpoint_.c_str()) != 0)
            throw std::runtime_error("cannot install tablebase checkpoint");
    }

    std::uint32_t load_checkpoint() {
        std::ifstream stream(checkpoint_, std::ios::binary);
        if (!stream)
            return 0;
        std::array<char, 8> magic{};
        std::uint32_t piece = 0, processed = 0;
        stream.read(magic.data(), magic.size());
        stream.read(reinterpret_cast<char*>(&piece), sizeof(piece));
        stream.read(reinterpret_cast<char*>(&processed), sizeof(processed));
        const std::array<char, 8> expected{{'U','F','T','B','C','P','2','\0'}};
        if (magic != expected || piece != static_cast<std::uint32_t>(attackerType_) ||
            processed > stateCount_)
            throw std::runtime_error("invalid tablebase checkpoint");
        stream.read(reinterpret_cast<char*>(nodes_.data()), nodes_.size() * sizeof(Node));
        stream.read(reinterpret_cast<char*>(predecessorCounts_.data()),
                    predecessorCounts_.size() * sizeof(std::uint32_t));
        if (!stream)
            throw std::runtime_error("truncated tablebase checkpoint");
        std::cout << "resume states " << processed << '\n';
        return processed;
    }

    void write_output(std::uint64_t edges) const {
        std::ofstream stream(output_, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("cannot write tablebase output");
        const std::array<char, 8> magic{{'U','F','T','B','1','\0','\0','\0'}};
        const std::uint32_t version = edges > std::numeric_limits<std::uint32_t>::max()
          ? 6 : fourModels_ ? 5 : 4;
        const std::uint32_t piece = static_cast<std::uint32_t>(attackerType_);
        const std::uint32_t legacyEdges = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(edges, std::numeric_limits<std::uint32_t>::max()));
        stream.write(magic.data(), magic.size());
        stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
        stream.write(reinterpret_cast<const char*>(&piece), sizeof(piece));
        stream.write(reinterpret_cast<const char*>(&stateCount_), sizeof(stateCount_));
        stream.write(reinterpret_cast<const char*>(&legacyEdges), sizeof(legacyEdges));
        stream.write(reinterpret_cast<const char*>(&substates_), sizeof(substates_));
        const std::uint32_t wdlBytes = (stateCount_ + 3) / 4;
        const std::uint32_t dtwBytes = stateCount_;
        std::vector<std::pair<std::uint32_t, std::uint16_t>> exceptions;
        for (std::uint32_t index = 0; index < stateCount_; ++index)
            if (nodes_[index].dtw >= 255)
                exceptions.emplace_back(index, nodes_[index].dtw);
        const std::uint32_t exceptionCount = exceptions.size();
        stream.write(reinterpret_cast<const char*>(&wdlBytes), sizeof(wdlBytes));
        stream.write(reinterpret_cast<const char*>(&dtwBytes), sizeof(dtwBytes));
        stream.write(reinterpret_cast<const char*>(&exceptionCount), sizeof(exceptionCount));
        if (version >= 5) {
            const std::uint32_t secondary = static_cast<std::uint32_t>(secondaryType_);
            const std::uint32_t secondaryColor = static_cast<std::uint32_t>(secondaryColor_);
            stream.write(reinterpret_cast<const char*>(&secondary), sizeof(secondary));
            stream.write(reinterpret_cast<const char*>(&secondaryColor), sizeof(secondaryColor));
        }
        if (version >= 6)
            stream.write(reinterpret_cast<const char*>(&edges), sizeof(edges));
        std::vector<std::uint8_t> wdlPlane(wdlBytes, 0);
        for (std::uint32_t index = 0; index < stateCount_; ++index)
            wdlPlane[index / 4] |= static_cast<std::uint8_t>(nodes_[index].wdl)
                                 << ((index % 4) * 2);
        stream.write(reinterpret_cast<const char*>(wdlPlane.data()), wdlPlane.size());
        for (const Node& node : nodes_) {
            const std::uint8_t distance = static_cast<std::uint8_t>(std::min<int>(node.dtw, 255));
            stream.write(reinterpret_cast<const char*>(&distance), sizeof(distance));
        }
        for (const auto [index, distance] : exceptions) {
            stream.write(reinterpret_cast<const char*>(&index), sizeof(index));
            stream.write(reinterpret_cast<const char*>(&distance), sizeof(distance));
        }
        std::array<std::uint64_t, 4> totals{};
        for (const Node& node : nodes_)
            ++totals[static_cast<std::size_t>(node.wdl)];
        std::cout << "output " << output_ << " edges " << edges;
        for (Wdl wdl : {Wdl::Win, Wdl::Loss, Wdl::Draw})
            std::cout << ' ' << wdl_name(wdl) << ' '
                      << totals[static_cast<std::size_t>(wdl)];
        std::cout << '\n';
    }

    void verify_range(std::uint32_t begin, std::uint32_t end) const {
        for (std::uint32_t index = begin; index < end; ++index) {
            const Node node = nodes_[index];
            Position position;
            if (!make_position_at(index, position)) {
                if (node.wdl != Wdl::Draw)
                    throw std::runtime_error("invalid geometry is not a draw sentinel");
                continue;
            }
            if (!position.is_checkmate_possible()) {
                if (node.wdl != Wdl::Draw || node.dtw != 0)
                    throw std::runtime_error("terminal tablebase state is misclassified");
                continue;
            }
            const auto moves = position.legal_moves();
            bool hasLoss = false;
            bool hasDraw = false;
            bool allWin = !moves.empty();
            std::uint16_t shortestLoss = std::numeric_limits<std::uint16_t>::max();
            std::uint16_t longestWin = 0;
            for (const Move& move : moves) {
                Position child = position;
                if (!child.apply_move_unchecked(move))
                    throw std::runtime_error("verification move failed trusted application");
                if (child.forced_timeout_winner() ||
                    !child.has_real_king(Color::White) ||
                    !child.has_real_king(Color::Black) ||
                    !child.is_checkmate_possible()) {
                    const auto winner = child.winner();
                    if (winner && *winner == position.side_to_move()) {
                        hasLoss = true;
                        shortestLoss = 0;
                        allWin = false;
                    }
                    else if (!winner) {
                        hasDraw = true;
                        allWin = false;
                    }
                    continue;
                }
                if (!in_class(child)) {
                    const auto external = TablebaseProbe::probe(child);
                    if (external && external->wdl == TablebaseWdl::Loss) {
                        hasLoss = true;
                        shortestLoss = std::min(shortestLoss, external->dtw);
                        allWin = false;
                    }
                    else if (external && external->wdl == TablebaseWdl::Win)
                        longestWin = std::max(longestWin, external->dtw);
                    else {
                        hasDraw = true;
                        allWin = false;
                    }
                    continue;
                }
                const Node successor = nodes_[child_index(child)];
                if (successor.wdl == Wdl::Loss) {
                    hasLoss = true;
                    shortestLoss = std::min(shortestLoss, successor.dtw);
                    allWin = false;
                }
                else if (successor.wdl == Wdl::Draw) {
                    hasDraw = true;
                    allWin = false;
                }
                else if (successor.wdl == Wdl::Win)
                    longestWin = std::max(longestWin, successor.dtw);
                else
                    throw std::runtime_error("unknown state remains after retrograde");
            }
            bool valid = false;
            if (node.wdl == Wdl::Win)
                valid = hasLoss && node.dtw == shortestLoss + 1;
            else if (node.wdl == Wdl::Loss)
                valid = (moves.empty() && node.dtw == 0) ||
                        (allWin && node.dtw == longestWin + 1);
            else if (node.wdl == Wdl::Draw)
                valid = !hasLoss && (moves.empty() || hasDraw);
            if (!valid)
                throw std::runtime_error(
                  "retrograde Bellman verification failed at state " +
                  std::to_string(index) + " node=" +
                  std::to_string(static_cast<int>(node.wdl)) + "/" +
                  std::to_string(node.dtw) + " hasLoss=" +
                  std::to_string(hasLoss) + " hasDraw=" + std::to_string(hasDraw) +
                  " allWin=" + std::to_string(allWin) + " shortestLoss=" +
                  std::to_string(shortestLoss) + " longestWin=" +
                  std::to_string(longestWin));
        }
    }

    void verify_solution() const {
        const std::uint32_t workers = std::min<std::uint32_t>(
          4, std::max(1u, std::thread::hardware_concurrency()));
        std::vector<std::future<void>> tasks;
        for (std::uint32_t worker = 0; worker < workers; ++worker) {
            const std::uint32_t begin = stateCount_ * worker / workers;
            const std::uint32_t end = stateCount_ * (worker + 1) / workers;
            tasks.push_back(std::async(std::launch::async, [this, begin, end] {
                verify_range(begin, end);
            }));
        }
        for (auto& task : tasks)
            task.get();
        std::cout << "verifyok states " << stateCount_ << '\n';
    }

    void progress(const char* phase, std::uint32_t states,
                  std::chrono::steady_clock::time_point start) const {
        const double elapsed = std::chrono::duration<double>(
          std::chrono::steady_clock::now() - start).count();
        std::cout << phase << " states " << states << '/' << stateCount_
                  << " elapsed " << elapsed << "s\n" << std::flush;
    }

    PieceType attackerType_;
    std::string output_;
    std::string checkpoint_;
    std::uint32_t checkpointEvery_;
    PieceType secondaryType_;
    Color secondaryColor_;
    bool fourModels_;
    bool identicalExtras_;
    std::uint32_t primarySubstates_;
    std::uint32_t secondarySubstates_;
    std::uint32_t substates_;
    std::uint32_t stateCount_;
    std::vector<Node> nodes_;
    std::vector<std::uint32_t> predecessorCounts_;
};

}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    using namespace Stockfish::Ultimate;
    PieceType attackerType = PieceType::Rook;
    PieceType secondaryType = PieceType::Count;
    Color secondaryColor = Color::White;
    std::string output = "/tmp/ultimatefish-krk.uftb";
    std::string checkpoint = "/tmp/ultimatefish-krk.checkpoint";
    std::uint32_t checkpointEvery = 50'000;
    bool selfTest = false;
    bool fourCodecSelfTest = false;
    std::uint32_t dryRun = 0;
    std::uint32_t inspect = std::numeric_limits<std::uint32_t>::max();
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&](const char* option) -> std::string {
            if (index + 1 >= argc)
                throw std::runtime_error(std::string("missing value for ") + option);
            return argv[++index];
        };
        if (argument == "--output") output = value("--output");
        else if (argument == "--checkpoint") checkpoint = value("--checkpoint");
        else if (argument == "--piece") {
            const std::string name = value("--piece");
            const auto parsed = Position::type_from_name(name);
            if (!parsed)
                throw std::runtime_error("unknown primary tablebase piece");
            attackerType = *parsed;
        }
        else if (argument == "--piece2") {
            const auto parsed = Position::type_from_name(value("--piece2"));
            if (!parsed)
                throw std::runtime_error("unknown secondary tablebase piece");
            secondaryType = *parsed;
        }
        else if (argument == "--opposing") secondaryColor = Color::Black;
        else if (argument == "--checkpoint-every")
            checkpointEvery = static_cast<std::uint32_t>(std::stoul(value("--checkpoint-every")));
        else if (argument == "--dry-run")
            dryRun = static_cast<std::uint32_t>(std::stoul(value("--dry-run")));
        else if (argument == "--inspect")
            inspect = static_cast<std::uint32_t>(std::stoul(value("--inspect")));
        else if (argument == "--self-test") selfTest = true;
        else if (argument == "--four-codec-self-test") fourCodecSelfTest = true;
        else throw std::runtime_error("unknown argument: " + argument);
    }
    try {
        if (fourCodecSelfTest) {
            self_test_four_codec();
            return 0;
        }
        if (secondaryType == PieceType::Count) {
            if (!closed_position_only_attacker(attackerType))
                throw std::runtime_error("piece is not a closed K+K+1 tablebase class");
        }
        else if (!closed_four_piece(attackerType) ||
                 !closed_four_piece(secondaryType))
            throw std::runtime_error("K+K+2 piece requires a larger non-closed model");
        TablebaseGenerator generator(attackerType, secondaryType, secondaryColor,
                                     output, checkpoint, checkpointEvery);
        if (selfTest)
            generator.self_test();
        if (dryRun)
            generator.dry_run(dryRun);
        if (inspect != std::numeric_limits<std::uint32_t>::max())
            generator.inspect(inspect);
        if (!selfTest && !dryRun && inspect == std::numeric_limits<std::uint32_t>::max())
            generator.generate();
    }
    catch (const std::exception& error) {
        std::cerr << "tablebase error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
