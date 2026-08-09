/*
  Ultimate Fish - exact K+Ghost+Ghost versus K information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "ghost_pair_information_model.h"

#include "information.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace Stockfish::Ultimate::GhostPairInformation {
namespace {

constexpr std::uint8_t Squares = Position::BoardSquares;

[[nodiscard]] std::uint8_t horizontal_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>(
      (square / Position::BoardFiles) * Position::BoardFiles +
      Position::BoardFiles - 1 - square % Position::BoardFiles);
}

[[nodiscard]] std::uint8_t vertical_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>(
      (Position::BoardRanks - 1 - square / Position::BoardFiles) *
      Position::BoardFiles + square % Position::BoardFiles);
}

void validate_side(Color side) {
    if (side != Color::White && side != Color::Black)
        throw std::invalid_argument("invalid Ghost-pair side to move");
}

void validate_square(std::uint8_t square, const char* field) {
    if (square >= Squares)
        throw std::invalid_argument(std::string(field) +
                                    " is outside the Ultimate board");
}

[[nodiscard]] bool distinct(std::initializer_list<std::uint8_t> squares) {
    for (auto first = squares.begin(); first != squares.end(); ++first)
        for (auto second = first + 1; second != squares.end(); ++second)
            if (*first == *second)
                return false;
    return true;
}

[[nodiscard]] std::uint32_t rank_excluding(
  std::uint8_t square, std::initializer_list<std::uint8_t> occupied) {
    std::uint32_t rank = square;
    for (const std::uint8_t used : occupied)
        rank -= used < square;
    return rank;
}

[[nodiscard]] std::uint8_t unrank_excluding(
  std::uint32_t rank, std::initializer_list<std::uint8_t> occupied) {
    for (std::uint8_t square = 0; square < Squares; ++square) {
        bool used = false;
        for (const std::uint8_t item : occupied)
            used = used || item == square;
        if (!used && rank-- == 0)
            return square;
    }
    throw std::runtime_error("Ghost-pair square rank is invalid");
}

[[nodiscard]] std::uint32_t pair_rank(unsigned first, unsigned second,
                                      unsigned count) {
    if (first >= second || second >= count)
        throw std::invalid_argument("invalid unordered Ghost-pair ranks");
    return first * (2 * count - first - 1) / 2 + second - first - 1;
}

[[nodiscard]] std::pair<unsigned, unsigned> unrank_pair(
  std::uint32_t rank, unsigned count) {
    const std::uint32_t pairs = count * (count - 1) / 2;
    if (rank >= pairs)
        throw std::out_of_range("unordered Ghost-pair rank is out of range");
    unsigned low = 0;
    unsigned high = count - 1;
    while (low + 1 < high) {
        const unsigned middle = (low + high) / 2;
        const std::uint32_t start =
          middle * (2 * count - middle - 1) / 2;
        if (start <= rank)
            low = middle;
        else
            high = middle;
    }
    const std::uint32_t start = low * (2 * count - low - 1) / 2;
    const unsigned second = low + 1 + (rank - start);
    if (second >= count)
        throw std::runtime_error("unordered Ghost-pair rank failed to decode");
    return {low, second};
}

void sort_ghost_records(ConcreteState& state) {
    if (state.firstGhost > state.secondGhost) {
        std::swap(state.firstGhost, state.secondGhost);
        std::swap(state.firstVisible, state.secondVisible);
    }
}

void validate_source(const ConcreteState& source) {
    validate_side(source.side);
    validate_square(source.whiteKing, "White King square");
    validate_square(source.blackKing, "Black King square");
    validate_square(source.firstGhost, "first Ghost square");
    validate_square(source.secondGhost, "second Ghost square");
    if (!distinct({source.whiteKing, source.blackKing, source.firstGhost,
                   source.secondGhost}))
        throw std::invalid_argument("overlapping Ghost-pair source state");
}

[[nodiscard]] ConcreteState normalize_physical(ConcreteState state) {
    validate_source(state);
    sort_ghost_records(state);
    return state;
}

[[nodiscard]] ConcreteState horizontal_canonical(ConcreteState state) {
    state = normalize_physical(state);
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.firstGhost = horizontal_reflection(state.firstGhost);
        state.secondGhost = horizontal_reflection(state.secondGhost);
        sort_ghost_records(state);
    }
    return state;
}

void validate_frame(const PublicFrame& frame) {
    validate_side(frame.side);
    validate_square(frame.whiteKing, "White King square");
    validate_square(frame.blackKing, "Black King square");
    if (frame.whiteKing == frame.blackKing)
        throw std::invalid_argument("overlapping Ghost-pair Kings");
    if (frame.visibleCount > 2)
        throw std::invalid_argument("invalid visible Ghost count");
    for (std::uint8_t index = 0; index < frame.visibleCount; ++index) {
        validate_square(frame.visibleGhosts[index], "visible Ghost square");
        if (frame.visibleGhosts[index] == frame.whiteKing ||
            frame.visibleGhosts[index] == frame.blackKing ||
            (index && frame.visibleGhosts[index - 1] >=
                        frame.visibleGhosts[index]))
            throw std::invalid_argument("invalid public Ghost-pair geometry");
    }
    for (std::uint8_t index = frame.visibleCount; index < 2; ++index)
        if (frame.visibleGhosts[index] != 0)
            throw std::invalid_argument(
              "unused visible Ghost slots must be canonical zeroes");
}

[[nodiscard]] bool is_visible(const PublicFrame& frame,
                              std::uint8_t square) {
    for (std::uint8_t index = 0; index < frame.visibleCount; ++index)
        if (frame.visibleGhosts[index] == square)
            return true;
    return false;
}

void validate_world(const PublicFrame& frame, const PairWorld& world) {
    validate_frame(frame);
    validate_square(world.first, "first product Ghost square");
    validate_square(world.second, "second product Ghost square");
    if (world.first >= world.second ||
        !distinct({frame.whiteKing, frame.blackKing, world.first,
                   world.second}))
        throw std::invalid_argument("invalid unordered Ghost-pair world");
    unsigned visible = 0;
    visible += is_visible(frame, world.first);
    visible += is_visible(frame, world.second);
    if (visible != frame.visibleCount)
        throw std::invalid_argument(
          "Ghost-pair world disagrees with public visibility");
}

[[nodiscard]] std::vector<std::uint8_t> private_available(
  const PublicFrame& frame) {
    validate_frame(frame);
    std::vector<std::uint8_t> result;
    result.reserve(Squares - 2 - frame.visibleCount);
    for (std::uint8_t square = 0; square < Squares; ++square)
        if (square != frame.whiteKing && square != frame.blackKing &&
            !is_visible(frame, square))
            result.push_back(square);
    return result;
}

[[nodiscard]] unsigned available_rank(
  const std::vector<std::uint8_t>& available, std::uint8_t square) {
    const auto found = std::lower_bound(
      available.begin(), available.end(), square);
    if (found == available.end() || *found != square)
        throw std::invalid_argument(
          "private Ghost square is outside its public universe");
    return static_cast<unsigned>(found - available.begin());
}

void validate_transform(RectangleTransform transform) {
    if (static_cast<std::uint8_t>(transform) >
        static_cast<std::uint8_t>(RectangleTransform::Both))
        throw std::invalid_argument("invalid Ultimate rectangle transform");
}

[[nodiscard]] std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned,
                         unsigned>
frame_key(const PublicFrame& frame) {
    return {static_cast<unsigned>(frame.side), frame.whiteKing,
      frame.blackKing, frame.visibleCount,
      frame.visibleCount ? frame.visibleGhosts[0] : Squares,
      frame.visibleCount > 1 ? frame.visibleGhosts[1] : Squares};
}

[[nodiscard]] bool action_auxiliary_is_square(const ActionKey& action) {
    if (action.kind == MoveKind::Pull)
        return true;
    if (action.kind == MoveKind::Normal && action.auxiliary != 0)
        throw std::invalid_argument(
          "nonzero ordinary auxiliary needs a material action adapter");
    return false;
}

[[nodiscard]] std::optional<Move> locate_action(
  const Position& position, const ActionKey& action) {
    std::optional<Move> result;
    for (const Move& move : position.legal_moves())
        if (action_key(move) == action) {
            if (result)
                throw std::runtime_error(
                  "one complete action maps to multiple legal moves");
            result = move;
        }
    return result;
}

struct LiveMaterial {
    int whiteKing = Position::NoSquare;
    int blackKing = Position::NoSquare;
    std::array<int, 2> ghosts{Position::NoSquare, Position::NoSquare};
    std::array<bool, 2> visible{};
    int ghostCount = 0;
    int live = 0;
    bool unexpected = false;
};

[[nodiscard]] LiveMaterial scan_live(const Position& position) {
    LiveMaterial result;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++result.live;
        if (piece.type == PieceType::King && piece.color == Color::White &&
            result.whiteKing == Position::NoSquare)
            result.whiteKing = piece.square;
        else if (piece.type == PieceType::King &&
                 piece.color == Color::Black &&
                 result.blackKing == Position::NoSquare)
            result.blackKing = piece.square;
        else if (piece.type == PieceType::Ghost &&
                 piece.color == Color::White && result.ghostCount < 2) {
            result.ghosts[result.ghostCount] = piece.square;
            result.visible[result.ghostCount] = piece.visible;
            ++result.ghostCount;
        }
        else
            result.unexpected = true;
    }
    if (result.ghostCount == 2 && result.ghosts[0] > result.ghosts[1]) {
        std::swap(result.ghosts[0], result.ghosts[1]);
        std::swap(result.visible[0], result.visible[1]);
    }
    return result;
}

[[nodiscard]] bool adjacent(std::uint8_t first, std::uint8_t second) {
    return std::abs(int(first % Position::BoardFiles) -
                    int(second % Position::BoardFiles)) <= 1 &&
           std::abs(int(first / Position::BoardFiles) -
                    int(second / Position::BoardFiles)) <= 1;
}

}  // namespace

VisibilityClass PublicFrame::visibility_class() const {
    validate_frame(*this);
    if (!visibleCount)
        return VisibilityClass::HiddenHidden;
    return visibleCount == 1 ? VisibilityClass::HiddenVisible
                             : VisibilityClass::VisibleVisible;
}

std::uint32_t encode_source(const ConcreteState& source) {
    const ConcreteState state = horizontal_canonical(source);
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
      (Position::BoardFiles / 2) +
      state.whiteKing % (Position::BoardFiles / 2);
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t firstRank = rank_excluding(
      state.firstGhost, {state.whiteKing, state.blackKing});
    const std::uint32_t secondRank = rank_excluding(
      state.secondGhost, {state.whiteKing, state.blackKing});
    const std::uint32_t pair = pair_rank(
      firstRank, secondRank, Squares - 2);
    constexpr std::uint32_t Pairs =
      (Squares - 2) * (Squares - 3) / 2;
    const std::uint32_t placement =
      ((static_cast<std::uint32_t>(state.side) * (Squares / 2) + whiteRank) *
       (Squares - 1) + blackRank) * Pairs + pair;
    const std::uint32_t visibility =
      (state.firstVisible ? 2u : 0u) + (state.secondVisible ? 1u : 0u);
    return placement * 4 + visibility;
}

ConcreteState decode_source(std::uint32_t index) {
    if (index >= StateCount)
        throw std::out_of_range("Ghost-pair source index is out of range");
    const std::uint32_t visibility = index % 4;
    index /= 4;
    constexpr std::uint32_t Pairs =
      (Squares - 2) * (Squares - 3) / 2;
    const std::uint32_t pair = index % Pairs;
    index /= Pairs;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    const Color side = static_cast<Color>(index / (Squares / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Position::BoardFiles / 2)) * Position::BoardFiles +
      whiteRank % (Position::BoardFiles / 2));
    const std::uint8_t blackKing = unrank_excluding(
      blackRank, {whiteKing});
    const auto [firstRank, secondRank] = unrank_pair(pair, Squares - 2);
    const std::uint8_t firstGhost = unrank_excluding(
      firstRank, {whiteKing, blackKing});
    const std::uint8_t secondGhost = unrank_excluding(
      secondRank, {whiteKing, blackKing});
    return {side, whiteKing, blackKing, firstGhost, secondGhost,
            bool(visibility & 2), bool(visibility & 1)};
}

FramedWorld source_to_product(const ConcreteState& source) {
    const ConcreteState state = normalize_physical(source);
    PublicFrame frame;
    frame.side = state.side;
    frame.whiteKing = state.whiteKing;
    frame.blackKing = state.blackKing;
    if (state.firstVisible)
        frame.visibleGhosts[frame.visibleCount++] = state.firstGhost;
    if (state.secondVisible)
        frame.visibleGhosts[frame.visibleCount++] = state.secondGhost;
    validate_frame(frame);
    const PairWorld world{state.firstGhost, state.secondGhost};
    validate_world(frame, world);
    return {frame, world};
}

ConcreteState product_to_source(const PublicFrame& frame,
                                const PairWorld& world) {
    validate_world(frame, world);
    return {frame.side, frame.whiteKing, frame.blackKing,
            world.first, world.second,
            is_visible(frame, world.first),
            is_visible(frame, world.second)};
}

Position make_position(const PublicFrame& frame, const PairWorld& world) {
    const ConcreteState state = product_to_source(frame, world);
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, state.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, state.blackKing);
    const int firstGhost = position.add_piece(
      PieceType::Ghost, Color::White, state.firstGhost);
    const int secondGhost = position.add_piece(
      PieceType::Ghost, Color::White, state.secondGhost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        firstGhost == Position::NoPiece || secondGhost == Position::NoPiece)
        throw std::runtime_error("cannot construct Ghost-pair product world");
    for (const int id : {whiteKing, blackKing, firstGhost, secondGhost})
        position.piece(id).moved = true;
    position.piece(firstGhost).visible = state.firstVisible;
    position.piece(secondGhost).visible = state.secondVisible;
    position.set_side_to_move(state.side);
    return position;
}

void PairMask::set(unsigned variable) {
    if (variable >= MaximumWorlds)
        throw std::out_of_range("Ghost-pair product variable is invalid");
    words[variable / 64] |= std::uint64_t(1) << (variable % 64);
}

bool PairMask::test(unsigned variable) const {
    return variable < MaximumWorlds &&
           ((words[variable / 64] >> (variable % 64)) & 1u);
}

unsigned PairMask::count() const {
    unsigned result = 0;
    for (const std::uint64_t word : words)
        result += static_cast<unsigned>(__builtin_popcountll(word));
    return result;
}

unsigned variable_count(const PublicFrame& frame) {
    switch (frame.visibility_class()) {
    case VisibilityClass::HiddenHidden: return MaximumWorlds;
    case VisibilityClass::HiddenVisible: return Squares - 3;
    case VisibilityClass::VisibleVisible: return 1;
    }
    throw std::runtime_error("invalid Ghost-pair visibility class");
}

unsigned pair_variable(const PublicFrame& frame, const PairWorld& world) {
    validate_world(frame, world);
    const std::vector<std::uint8_t> available = private_available(frame);
    switch (frame.visibility_class()) {
    case VisibilityClass::HiddenHidden:
        return pair_rank(available_rank(available, world.first),
                         available_rank(available, world.second),
                         available.size());
    case VisibilityClass::HiddenVisible: {
        const std::uint8_t hidden = is_visible(frame, world.first)
                                  ? world.second : world.first;
        return available_rank(available, hidden);
    }
    case VisibilityClass::VisibleVisible: return 0;
    }
    throw std::runtime_error("invalid Ghost-pair visibility class");
}

PairWorld decode_pair_variable(const PublicFrame& frame, unsigned variable) {
    const unsigned variables = variable_count(frame);
    if (variable >= variables)
        throw std::out_of_range("Ghost-pair product variable is out of range");
    const std::vector<std::uint8_t> available = private_available(frame);
    PairWorld result;
    switch (frame.visibility_class()) {
    case VisibilityClass::HiddenHidden: {
        const auto [first, second] = unrank_pair(variable, available.size());
        result = {available[first], available[second]};
        break;
    }
    case VisibilityClass::HiddenVisible:
        result = {std::min(frame.visibleGhosts[0], available[variable]),
                  std::max(frame.visibleGhosts[0], available[variable])};
        break;
    case VisibilityClass::VisibleVisible:
        result = {frame.visibleGhosts[0], frame.visibleGhosts[1]};
        break;
    }
    validate_world(frame, result);
    return result;
}

std::vector<PairWorld> geometric_worlds(const PublicFrame& frame) {
    std::vector<PairWorld> result;
    result.reserve(variable_count(frame));
    for (unsigned variable = 0; variable < variable_count(frame); ++variable)
        result.push_back(decode_pair_variable(frame, variable));
    return result;
}

std::vector<PairWorld> decode_pair_mask(const PublicFrame& frame,
                                        const PairMask& mask) {
    const unsigned variables = variable_count(frame);
    for (unsigned variable = variables; variable < MaskWords * 64; ++variable)
        if ((mask.words[variable / 64] >> (variable % 64)) & 1u)
            throw std::invalid_argument(
              "Ghost-pair mask contains undefined variables");
    std::vector<PairWorld> result;
    result.reserve(mask.count());
    for (unsigned variable = 0; variable < variables; ++variable)
        if (mask.test(variable))
            result.push_back(decode_pair_variable(frame, variable));
    return result;
}

std::uint8_t transform_square(std::uint8_t square,
                              RectangleTransform transform) {
    validate_square(square, "rectangle-transform square");
    validate_transform(transform);
    if (static_cast<std::uint8_t>(transform) & 1u)
        square = horizontal_reflection(square);
    if (static_cast<std::uint8_t>(transform) & 2u)
        square = vertical_reflection(square);
    return square;
}

FramedWorld transform_world(const PublicFrame& frame,
                            const PairWorld& world,
                            RectangleTransform transform) {
    validate_world(frame, world);
    PublicFrame mapped;
    mapped.side = frame.side;
    mapped.whiteKing = transform_square(frame.whiteKing, transform);
    mapped.blackKing = transform_square(frame.blackKing, transform);
    mapped.visibleCount = frame.visibleCount;
    for (std::uint8_t index = 0; index < frame.visibleCount; ++index)
        mapped.visibleGhosts[index] = transform_square(
          frame.visibleGhosts[index], transform);
    if (mapped.visibleCount == 2 &&
        mapped.visibleGhosts[0] > mapped.visibleGhosts[1])
        std::swap(mapped.visibleGhosts[0], mapped.visibleGhosts[1]);
    PairWorld mappedWorld{
      transform_square(world.first, transform),
      transform_square(world.second, transform)};
    if (mappedWorld.first > mappedWorld.second)
        std::swap(mappedWorld.first, mappedWorld.second);
    validate_world(mapped, mappedWorld);
    return {mapped, mappedWorld};
}

PairSet transform_set(const PublicFrame& frame, const PairMask& worlds,
                      RectangleTransform transform) {
    const std::vector<PairWorld> decoded = decode_pair_mask(frame, worlds);
    if (decoded.empty())
        throw std::invalid_argument(
          "cannot transform an empty Ghost-pair information set");
    PairSet result;
    for (const PairWorld& world : decoded) {
        const FramedWorld mapped = transform_world(frame, world, transform);
        if (!result.worlds.count())
            result.frame = mapped.frame;
        else if (!(result.frame == mapped.frame))
            throw std::runtime_error(
              "one Ghost-pair set transformed into multiple public frames");
        result.worlds.set(pair_variable(result.frame, mapped.world));
    }
    if (result.worlds.count() != worlds.count())
        throw std::runtime_error(
          "Ghost-pair rectangle transform did not conserve correlation");
    return result;
}

CanonicalSet canonicalize_set(const PublicFrame& frame,
                              const PairMask& worlds) {
    CanonicalSet result{
      transform_set(frame, worlds, RectangleTransform::Identity),
      RectangleTransform::Identity};
    for (std::uint8_t raw = 1; raw < 4; ++raw) {
        const auto transform = static_cast<RectangleTransform>(raw);
        PairSet candidate = transform_set(frame, worlds, transform);
        if (frame_key(candidate.frame) < frame_key(result.value.frame))
            result = {std::move(candidate), transform};
    }
    return result;
}

bool operator<(const ActionKey& lhs, const ActionKey& rhs) {
    return std::tie(lhs.from, lhs.to, lhs.auxiliary, lhs.kind, lhs.promotion) <
           std::tie(rhs.from, rhs.to, rhs.auxiliary, rhs.kind, rhs.promotion);
}

ActionKey action_key(const Move& move) {
    return {move.from, move.to, move.auxiliary, move.kind, move.promotion};
}

ActionKey transform_action(ActionKey action, RectangleTransform transform) {
    validate_transform(transform);
    if (action.kind == MoveKind::Pass)
        return action;
    if (action.from >= Squares || action.to >= Squares)
        throw std::invalid_argument("action endpoint is outside the board");
    action.from = transform_square(action.from, transform);
    action.to = transform_square(action.to, transform);
    if (action_auxiliary_is_square(action)) {
        if (action.auxiliary >= Squares)
            throw std::invalid_argument(
              "square-valued action auxiliary is outside the board");
        action.auxiliary = transform_square(action.auxiliary, transform);
    }
    return action;
}

std::vector<ActionKey> legal_actions(const Position& position) {
    std::vector<ActionKey> result;
    for (const Move& move : position.legal_moves())
        result.push_back(action_key(move));
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end())
        throw std::runtime_error(
          "legal move generator produced duplicate complete actions");
    return result;
}

std::vector<DecisionBucket> decision_partition(
  const PublicFrame& frame, const std::vector<PairWorld>& worlds) {
    validate_frame(frame);
    std::vector<DecisionBucket> result;
    std::map<std::string, PairMask> observerBuckets;
    std::set<unsigned> seen;
    for (const PairWorld& world : worlds) {
        const unsigned variable = pair_variable(frame, world);
        if (!seen.insert(variable).second)
            throw std::invalid_argument(
              "decision partition contains a duplicate Ghost-pair world");
        const Position position = make_position(frame, world);
        const std::string observation = decision_observation_key(
          position, {frame.side, false});
        if (frame.side == Color::White) {
            PairMask singleton;
            singleton.set(variable);
            result.push_back({observation, singleton});
        }
        else
            observerBuckets[observation].set(variable);
    }
    if (frame.side == Color::Black)
        for (auto& [observation, mask] : observerBuckets)
            result.push_back({std::move(observation), mask});
    unsigned conserved = 0;
    for (const DecisionBucket& bucket : result)
        conserved += bucket.worlds.count();
    if (conserved != worlds.size())
        throw std::runtime_error(
          "mover-private Ghost-pair partition does not conserve worlds");
    return result;
}

std::uint32_t encode_lower_ghost(const LowerGhostState& state) {
    if (state.side != Role::GhostOwner && state.side != Role::Observer)
        throw std::invalid_argument("invalid lower Ghost side to move");
    validate_square(state.ownerKing, "lower Ghost-owner King square");
    validate_square(state.observerKing, "lower observer King square");
    validate_square(state.ghost, "lower Ghost square");
    if (!distinct({state.ownerKing, state.observerKing, state.ghost}))
        throw std::invalid_argument("overlapping lower Ghost placement");
    const std::uint32_t observerRank = rank_excluding(
      state.observerKing, {state.ownerKing});
    const std::uint32_t ghostRank = rank_excluding(
      state.ghost, {state.ownerKing, state.observerKing});
    const std::uint32_t placement =
      ((static_cast<std::uint32_t>(state.side) * Squares + state.ownerKing) *
       (Squares - 1) + observerRank) * (Squares - 2) + ghostRank;
    return placement * 2 + (state.visible ? 1u : 0u);
}

LowerGhostState decode_lower_ghost(std::uint32_t index) {
    if (index >= LowerGhostStateCount)
        throw std::out_of_range("lower Ghost index is out of range");
    const bool visible = index % 2 != 0;
    index /= 2;
    const std::uint32_t ghostRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t observerRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint8_t ownerKing = static_cast<std::uint8_t>(index % Squares);
    const Role side = static_cast<Role>(index / Squares);
    const std::uint8_t observerKing = unrank_excluding(
      observerRank, {ownerKing});
    const std::uint8_t ghost = unrank_excluding(
      ghostRank, {ownerKing, observerKing});
    return {side, ownerKing, observerKing, ghost, visible};
}

ClassifiedChild classify_child(const Position& position) {
    const LiveMaterial live = scan_live(position);
    if (!live.unexpected && live.live == 4 && live.ghostCount == 2 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare) {
        const ConcreteState state{
          position.side_to_move(),
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.ghosts[0]),
          static_cast<std::uint8_t>(live.ghosts[1]),
          live.visible[0], live.visible[1]};
        return {ChildDomain::SameClass, encode_source(state), {}};
    }
    if (!live.unexpected && live.live == 3 && live.ghostCount == 1 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare) {
        const LowerGhostState state{
          position.side_to_move() == Color::White
            ? Role::GhostOwner : Role::Observer,
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.ghosts[0]), live.visible[0]};
        return {ChildDomain::LowerGhost, encode_lower_ghost(state), {}};
    }
    if (position.game_over())
        return {ChildDomain::ExactTerminal, 0, position.winner()};
    return {ChildDomain::Invalid, 0, {}};
}

std::optional<FramedWorld> same_class_product(const Position& position) {
    const LiveMaterial live = scan_live(position);
    if (live.unexpected || live.live != 4 || live.ghostCount != 2 ||
        live.whiteKing == Position::NoSquare ||
        live.blackKing == Position::NoSquare)
        return std::nullopt;
    const ConcreteState physical{
      position.side_to_move(),
      static_cast<std::uint8_t>(live.whiteKing),
      static_cast<std::uint8_t>(live.blackKing),
      static_cast<std::uint8_t>(live.ghosts[0]),
      static_cast<std::uint8_t>(live.ghosts[1]),
      live.visible[0], live.visible[1]};
    return source_to_product(physical);
}

void GhostMask::set(unsigned square) {
    if (square >= Squares)
        throw std::out_of_range("Ghost mask square is outside the board");
    if (square < 64)
        low |= std::uint64_t(1) << square;
    else
        high |= std::uint16_t(1) << (square - 64);
}

bool GhostMask::test(unsigned square) const {
    if (square >= Squares)
        return false;
    return square < 64 ? (low >> square) & 1u
                       : (high >> (square - 64)) & 1u;
}

unsigned GhostMask::count() const {
    return static_cast<unsigned>(
      __builtin_popcountll(low) + __builtin_popcount(high));
}

LowerGhostImage inherited_lower_ghost_image(
  const std::vector<ClassifiedChild>& children) {
    if (children.empty() ||
        children.front().domain != ChildDomain::LowerGhost)
        throw std::invalid_argument("lower Ghost image is empty or invalid");
    const LowerGhostState first = decode_lower_ghost(
      children.front().index);
    LowerGhostImage result{
      first.side, first.ownerKing, first.observerKing, first.visible, {}};
    for (const ClassifiedChild& child : children) {
        if (child.domain != ChildDomain::LowerGhost)
            throw std::invalid_argument(
              "lower Ghost image contains a non-K+Ghost child");
        const LowerGhostState state = decode_lower_ghost(child.index);
        if (state.side != result.side ||
            state.ownerKing != result.ownerKing ||
            state.observerKing != result.observerKing ||
            state.visible != result.visible)
            throw std::invalid_argument(
              "one observation mixes lower Ghost public frames");
        result.locations.set(state.ghost);
    }
    return result;
}

std::vector<std::vector<TransitionWorld>> transition_partition(
  const PublicFrame& frame, const std::vector<PairWorld>& worlds,
  const ActionKey& action, Color observer) {
    validate_frame(frame);
    validate_side(observer);
    std::map<std::string, std::vector<TransitionWorld>> grouped;
    std::set<unsigned> seen;
    for (const PairWorld& world : worlds) {
        const unsigned variable = pair_variable(frame, world);
        if (!seen.insert(variable).second)
            throw std::invalid_argument(
              "transition partition contains a duplicate Ghost-pair world");
        const Position position = make_position(frame, world);
        const std::optional<Move> move = locate_action(position, action);
        if (!move)
            continue;
        Position child = position;
        Undo undo;
        if (!child.make_move(*move, undo))
            throw std::runtime_error(
              "complete legal Ghost-pair action failed to apply");
        const std::string observation = transition_observation_key(
          position, *move, child, {observer, false});
        const ClassifiedChild classified = classify_child(child);
        std::optional<FramedWorld> physical;
        if (classified.domain == ChildDomain::SameClass) {
            physical = same_class_product(child);
            if (!physical)
                throw std::runtime_error(
                  "same-class child lost its physical pair coordinate");
        }
        grouped[observation].push_back(
          {world, observation, classified, physical});
    }
    std::vector<std::vector<TransitionWorld>> result;
    result.reserve(grouped.size());
    for (auto& [observation, bucket] : grouped) {
        (void)observation;
        result.push_back(std::move(bucket));
    }
    return result;
}

AdmissionVerdict fresh_world_admission(const PublicFrame& frame,
                                       const PairWorld& world) {
    const Position position = make_position(frame, world);
    if (position.has_forced_action() ||
        !position.ordinary_predecessor_king_safe())
        return AdmissionVerdict::Reject;
    for (const std::uint8_t square : {world.first, world.second})
        if (!is_visible(frame, square) &&
            adjacent(square, frame.blackKing))
            return AdmissionVerdict::Reject;
    return AdmissionVerdict::Admit;
}

std::vector<PairWorld> admitted_fresh_worlds(const PublicFrame& frame) {
    std::vector<PairWorld> result;
    for (const PairWorld& world : geometric_worlds(frame))
        if (fresh_world_admission(frame, world) == AdmissionVerdict::Admit)
            result.push_back(world);
    return result;
}

}  // namespace Stockfish::Ultimate::GhostPairInformation
