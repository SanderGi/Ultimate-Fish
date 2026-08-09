/*
  Ultimate Fish - exact K+Jester+Ghost versus K information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "jester_ghost_information_model.h"

#include "information.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace Stockfish::Ultimate::JesterGhostInformation {
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
    throw std::runtime_error("Jester/Ghost square rank is invalid");
}

[[nodiscard]] ConcreteState horizontal_canonical(ConcreteState state) {
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.jester = horizontal_reflection(state.jester);
        state.ghost = horizontal_reflection(state.ghost);
    }
    return state;
}

[[nodiscard]] bool distinct(std::initializer_list<std::uint8_t> squares) {
    for (auto first = squares.begin(); first != squares.end(); ++first)
        for (auto second = first + 1; second != squares.end(); ++second)
            if (*first == *second)
                return false;
    return true;
}

void validate_side(Color side) {
    if (side != Color::White && side != Color::Black)
        throw std::invalid_argument("invalid Jester/Ghost side to move");
}

void validate_square(std::uint8_t square, const char* field) {
    if (square >= Squares)
        throw std::invalid_argument(std::string(field) +
                                    " is outside the Ultimate board");
}

void validate_source(const ConcreteState& state) {
    validate_side(state.side);
    validate_square(state.whiteKing, "White King square");
    validate_square(state.blackKing, "Black King square");
    validate_square(state.jester, "Jester square");
    validate_square(state.ghost, "Ghost square");
    if (!distinct(
          {state.whiteKing, state.blackKing, state.jester, state.ghost}))
        throw std::invalid_argument("overlapping Jester/Ghost source state");
}

void validate_frame(const PublicFrame& frame) {
    validate_side(frame.side);
    validate_square(frame.blackKing, "Black King square");
    validate_square(frame.royalFirst, "first royal silhouette");
    validate_square(frame.royalSecond, "second royal silhouette");
    if (frame.visibleGhost)
        validate_square(*frame.visibleGhost, "visible Ghost square");
    if (frame.royalFirst >= frame.royalSecond ||
        !distinct({frame.blackKing, frame.royalFirst, frame.royalSecond}))
        throw std::invalid_argument("invalid Jester/Ghost public frame");
    if (frame.visibleGhost &&
        (!distinct({frame.blackKing, frame.royalFirst, frame.royalSecond,
                    *frame.visibleGhost})))
        throw std::invalid_argument(
          "visible Ghost overlaps a public Jester/Ghost model");
}

void validate_transform(RectangleTransform transform) {
    if (static_cast<std::uint8_t>(transform) >
        static_cast<std::uint8_t>(RectangleTransform::Both))
        throw std::invalid_argument("invalid Ultimate rectangle transform");
}

void validate_world(const PublicFrame& frame, const ProductWorld& world) {
    validate_frame(frame);
    if (world.ghost >= Squares ||
        !distinct({frame.blackKing, frame.royalFirst, frame.royalSecond,
                   world.ghost}))
        throw std::invalid_argument("invalid Jester/Ghost product world");
    if (frame.visibleGhost && *frame.visibleGhost != world.ghost)
        throw std::invalid_argument(
          "product world disagrees with visible Ghost square");
}

[[nodiscard]] std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned,
                         unsigned>
frame_key(const PublicFrame& frame) {
    return {static_cast<unsigned>(frame.side), frame.blackKing,
      frame.royalFirst, frame.royalSecond,
      frame.visibleGhost ? 1u : 0u,
      frame.visibleGhost ? *frame.visibleGhost : Squares};
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
    int jester = Position::NoSquare;
    int ghost = Position::NoSquare;
    bool visible = false;
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
        else if (piece.type == PieceType::Jester &&
                 piece.color == Color::White &&
                 result.jester == Position::NoSquare)
            result.jester = piece.square;
        else if (piece.type == PieceType::Ghost &&
                 piece.color == Color::White &&
                 result.ghost == Position::NoSquare) {
            result.ghost = piece.square;
            result.visible = piece.visible;
        }
        else
            result.unexpected = true;
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

std::uint32_t encode_source(const ConcreteState& source) {
    validate_source(source);
    ConcreteState state = horizontal_canonical(source);
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
      (Position::BoardFiles / 2) +
      state.whiteKing % (Position::BoardFiles / 2);
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t jesterRank = rank_excluding(
      state.jester, {state.whiteKing, state.blackKing});
    const std::uint32_t ghostRank = rank_excluding(
      state.ghost, {state.whiteKing, state.blackKing, state.jester});
    const std::uint32_t placement =
      ((((static_cast<std::uint32_t>(state.side) * (Squares / 2) + whiteRank)
          * (Squares - 1) + blackRank)
         * (Squares - 2) + jesterRank)
        * (Squares - 3) + ghostRank);
    return placement * 2 + (state.ghostVisible ? 1u : 0u);
}

ConcreteState decode_source(std::uint32_t index) {
    if (index >= StateCount)
        throw std::out_of_range("Jester/Ghost source index is out of range");
    const bool visible = index % 2 != 0;
    index /= 2;
    const std::uint32_t ghostRank = index % (Squares - 3);
    index /= Squares - 3;
    const std::uint32_t jesterRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    const Color side = static_cast<Color>(index / (Squares / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Position::BoardFiles / 2)) * Position::BoardFiles +
      whiteRank % (Position::BoardFiles / 2));
    const std::uint8_t blackKing = unrank_excluding(
      blackRank, {whiteKing});
    const std::uint8_t jester = unrank_excluding(
      jesterRank, {whiteKing, blackKing});
    const std::uint8_t ghost = unrank_excluding(
      ghostRank, {whiteKing, blackKing, jester});
    return {side, whiteKing, blackKing, jester, ghost, visible};
}

FramedWorld source_to_product(const ConcreteState& state) {
    validate_source(state);
    const std::uint8_t first = std::min(state.whiteKing, state.jester);
    const std::uint8_t second = std::max(state.whiteKing, state.jester);
    PublicFrame frame{state.side, state.blackKing, first, second, {}};
    if (state.ghostVisible)
        frame.visibleGhost = state.ghost;
    ProductWorld world{state.whiteKing == first, state.ghost};
    validate_world(frame, world);
    return {frame, world};
}

ConcreteState product_to_source(const PublicFrame& frame,
                                const ProductWorld& world) {
    validate_world(frame, world);
    const std::uint8_t whiteKing = world.kingAtFirst
                                 ? frame.royalFirst : frame.royalSecond;
    const std::uint8_t jester = world.kingAtFirst
                              ? frame.royalSecond : frame.royalFirst;
    return {frame.side, whiteKing, frame.blackKing, jester, world.ghost,
            frame.visibleGhost.has_value()};
}

Position make_position(const PublicFrame& frame, const ProductWorld& world) {
    const ConcreteState state = product_to_source(frame, world);
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, state.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, state.blackKing);
    const int jester = position.add_piece(
      PieceType::Jester, Color::White, state.jester);
    const int ghost = position.add_piece(
      PieceType::Ghost, Color::White, state.ghost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        jester == Position::NoPiece || ghost == Position::NoPiece)
        throw std::runtime_error("cannot construct Jester/Ghost product world");
    for (const int id : {whiteKing, blackKing, jester, ghost})
        position.piece(id).moved = true;
    position.piece(ghost).visible = state.ghostVisible;
    position.set_side_to_move(state.side);
    return position;
}

void ProductMask::set(unsigned variable) {
    if (variable >= ProductVariables)
        throw std::out_of_range("Jester/Ghost product variable is invalid");
    words[variable / 64] |= std::uint64_t(1) << (variable % 64);
}

bool ProductMask::test(unsigned variable) const {
    return variable < ProductVariables &&
           ((words[variable / 64] >> (variable % 64)) & 1u);
}

unsigned ProductMask::count() const {
    return static_cast<unsigned>(
      __builtin_popcountll(words[0]) + __builtin_popcountll(words[1]) +
      __builtin_popcountll(words[2]));
}

unsigned product_variable(const PublicFrame& frame,
                          const ProductWorld& world) {
    validate_world(frame, world);
    return (world.kingAtFirst ? 0u : Squares) + world.ghost;
}

ProductWorld decode_product_variable(const PublicFrame& frame,
                                     unsigned variable) {
    if (variable >= ProductVariables)
        throw std::out_of_range("Jester/Ghost product variable is invalid");
    const ProductWorld result{variable < Squares,
      static_cast<std::uint8_t>(variable % Squares)};
    validate_world(frame, result);
    return result;
}

std::vector<ProductWorld> geometric_worlds(const PublicFrame& frame) {
    validate_frame(frame);
    std::vector<ProductWorld> result;
    if (frame.visibleGhost) {
        result.push_back({true, *frame.visibleGhost});
        result.push_back({false, *frame.visibleGhost});
        return result;
    }
    result.reserve(2 * (Squares - 3));
    for (const bool kingAtFirst : {true, false})
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            ProductWorld world{kingAtFirst, ghost};
            try {
                validate_world(frame, world);
                result.push_back(world);
            }
            catch (const std::invalid_argument&) {
            }
        }
    if (result.size() != 2 * (Squares - 3))
        throw std::runtime_error(
          "hidden Jester/Ghost frame has the wrong product cardinality");
    return result;
}

std::vector<ProductWorld> decode_product_mask(const PublicFrame& frame,
                                              const ProductMask& mask) {
    validate_frame(frame);
    // Only 160 variables are defined. Reject stray payload bits rather than
    // permitting two byte-distinct masks to denote one information set.
    if (mask.words[2] >> (ProductVariables - 128))
        throw std::invalid_argument(
          "Jester/Ghost product mask contains undefined variables");
    std::vector<ProductWorld> result;
    result.reserve(mask.count());
    for (unsigned variable = 0; variable < ProductVariables; ++variable)
        if (mask.test(variable))
            result.push_back(decode_product_variable(frame, variable));
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
                            const ProductWorld& world,
                            RectangleTransform transform) {
    validate_world(frame, world);
    const std::uint8_t physicalKing = world.kingAtFirst
                                    ? frame.royalFirst : frame.royalSecond;
    const std::uint8_t transformedFirst =
      transform_square(frame.royalFirst, transform);
    const std::uint8_t transformedSecond =
      transform_square(frame.royalSecond, transform);
    PublicFrame resultFrame;
    resultFrame.side = frame.side;
    resultFrame.blackKing = transform_square(frame.blackKing, transform);
    resultFrame.royalFirst = std::min(transformedFirst, transformedSecond);
    resultFrame.royalSecond = std::max(transformedFirst, transformedSecond);
    if (frame.visibleGhost)
        resultFrame.visibleGhost =
          transform_square(*frame.visibleGhost, transform);
    const std::uint8_t resultKing =
      transform_square(physicalKing, transform);
    ProductWorld resultWorld{
      resultKing == resultFrame.royalFirst,
      transform_square(world.ghost, transform)};
    validate_world(resultFrame, resultWorld);
    return {resultFrame, resultWorld};
}

ProductSet transform_set(const PublicFrame& frame,
                         const ProductMask& worlds,
                         RectangleTransform transform) {
    const std::vector<ProductWorld> decoded =
      decode_product_mask(frame, worlds);
    if (decoded.empty())
        throw std::invalid_argument(
          "cannot transform an empty Jester/Ghost information set");
    ProductSet result;
    for (const ProductWorld& world : decoded) {
        const FramedWorld mapped = transform_world(frame, world, transform);
        if (result.worlds.count() == 0)
            result.frame = mapped.frame;
        else if (!(result.frame == mapped.frame))
            throw std::runtime_error(
              "one product set transformed into multiple public frames");
        result.worlds.set(product_variable(result.frame, mapped.world));
    }
    if (result.worlds.count() != worlds.count())
        throw std::runtime_error(
          "rectangle transform did not conserve product worlds");
    return result;
}

CanonicalWorld canonicalize_world(const PublicFrame& frame,
                                  const ProductWorld& world) {
    validate_world(frame, world);
    CanonicalWorld result{frame, world, RectangleTransform::Identity};
    for (std::uint8_t raw = 1; raw < 4; ++raw) {
        const auto transform = static_cast<RectangleTransform>(raw);
        const FramedWorld candidate = transform_world(frame, world, transform);
        // The chosen frame transform must not depend on the private world.
        // A public-frame stabilizer keeps the first (smallest raw) transform;
        // this is exact even though it forgoes an additional stabilizer fold.
        if (frame_key(candidate.frame) < frame_key(result.frame))
            result = {candidate.frame, candidate.world, transform};
    }
    return result;
}

CanonicalSet canonicalize_set(const PublicFrame& frame,
                              const ProductMask& worlds) {
    CanonicalSet result{
      transform_set(frame, worlds, RectangleTransform::Identity),
      RectangleTransform::Identity};
    for (std::uint8_t raw = 1; raw < 4; ++raw) {
        const auto transform = static_cast<RectangleTransform>(raw);
        ProductSet candidate = transform_set(frame, worlds, transform);
        // All private masks in a public frame use this same transform. A
        // stabilizer intentionally keeps the first transform because folding
        // the mask inside a stabilizer orbit would need a second set codec.
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
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds) {
    validate_frame(frame);
    std::vector<DecisionBucket> result;
    std::map<std::string, ProductMask> observerBuckets;
    std::set<unsigned> seen;
    for (const ProductWorld& world : worlds) {
        const unsigned variable = product_variable(frame, world);
        if (!seen.insert(variable).second)
            throw std::invalid_argument(
              "decision partition contains a duplicate product world");
        const Position position = make_position(frame, world);
        const std::string observation = decision_observation_key(
          position, {frame.side, false});
        if (frame.side == Color::White) {
            ProductMask singleton;
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
          "mover-private decision partition does not conserve worlds");
    return result;
}

std::uint32_t encode_lower_jester(const LowerJesterState& state) {
    validate_side(state.side);
    validate_square(state.whiteKing, "lower White King square");
    validate_square(state.blackKing, "lower Black King square");
    validate_square(state.jester, "lower Jester square");
    if (!distinct({state.whiteKing, state.blackKing, state.jester}))
        throw std::invalid_argument("overlapping lower Jester placement");
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t jesterRank = rank_excluding(
      state.jester, {state.whiteKing, state.blackKing});
    return ((static_cast<std::uint32_t>(state.side) * Squares +
             state.whiteKing) * (Squares - 1) + blackRank) *
             (Squares - 2) + jesterRank;
}

LowerJesterState decode_lower_jester(std::uint32_t index) {
    if (index >= LowerJesterStateCount)
        throw std::out_of_range("lower Jester index is out of range");
    const std::uint32_t jesterRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(index % Squares);
    const Color side = static_cast<Color>(index / Squares);
    const std::uint8_t blackKing = unrank_excluding(
      blackRank, {whiteKing});
    const std::uint8_t jester = unrank_excluding(
      jesterRank, {whiteKing, blackKing});
    return {side, whiteKing, blackKing, jester};
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
    if (!live.unexpected && live.live == 4 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester != Position::NoSquare &&
        live.ghost != Position::NoSquare) {
        const ConcreteState state{
          position.side_to_move(),
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.jester),
          static_cast<std::uint8_t>(live.ghost), live.visible};
        return {ChildDomain::SameClass, encode_source(state), {}};
    }
    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester != Position::NoSquare &&
        live.ghost == Position::NoSquare) {
        const LowerJesterState state{
          position.side_to_move(),
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.jester)};
        return {ChildDomain::LowerJester, encode_lower_jester(state), {}};
    }
    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester == Position::NoSquare &&
        live.ghost != Position::NoSquare) {
        const LowerGhostState state{
          position.side_to_move() == Color::White
            ? Role::GhostOwner : Role::Observer,
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.ghost), live.visible};
        return {ChildDomain::LowerGhost, encode_lower_ghost(state), {}};
    }
    if (position.game_over())
        return {ChildDomain::ExactTerminal, 0, position.winner()};
    return {ChildDomain::Invalid, 0, {}};
}

std::optional<FramedWorld> same_class_product(const Position& position) {
    const LiveMaterial live = scan_live(position);
    if (live.unexpected || live.live != 4 ||
        live.whiteKing == Position::NoSquare ||
        live.blackKing == Position::NoSquare ||
        live.jester == Position::NoSquare ||
        live.ghost == Position::NoSquare)
        return std::nullopt;
    const ConcreteState physical{
      position.side_to_move(),
      static_cast<std::uint8_t>(live.whiteKing),
      static_cast<std::uint8_t>(live.blackKing),
      static_cast<std::uint8_t>(live.jester),
      static_cast<std::uint8_t>(live.ghost), live.visible};
    return source_to_product(physical);
}

LowerJesterSet inherited_lower_jester_set(
  const std::vector<ClassifiedChild>& children) {
    if (children.empty() || children.size() > 2)
        throw std::invalid_argument(
          "lower Jester set must contain one or two assignments");
    std::set<std::uint32_t> unique;
    std::optional<LowerJesterSet> result;
    for (const ClassifiedChild& child : children) {
        if (child.domain != ChildDomain::LowerJester ||
            !unique.insert(child.index).second)
            throw std::invalid_argument(
              "lower Jester set contains an invalid or duplicate child");
        const LowerJesterState state = decode_lower_jester(child.index);
        const std::uint8_t first = std::min(state.whiteKing, state.jester);
        const std::uint8_t second = std::max(state.whiteKing, state.jester);
        if (!result)
            result = LowerJesterSet{
              0, state.side, state.blackKing, first, second, {}};
        else if (result->side != state.side ||
                 result->blackKing != state.blackKing ||
                 result->royalFirst != first ||
                 result->royalSecond != second)
            throw std::invalid_argument(
              "one observation mixes lower Jester public frames");
    }
    result->cardinality = static_cast<std::uint8_t>(unique.size());
    std::copy(unique.begin(), unique.end(), result->concrete.begin());
    if (result->cardinality == 2) {
        const LowerJesterState first =
          decode_lower_jester(result->concrete[0]);
        const LowerJesterState second =
          decode_lower_jester(result->concrete[1]);
        if (first.whiteKing != second.jester ||
            first.jester != second.whiteKing)
            throw std::invalid_argument(
              "two lower Jester children are not swapped assignments");
    }
    return *result;
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
    if (children.empty())
        throw std::invalid_argument("lower Ghost image is empty");
    if (children.front().domain != ChildDomain::LowerGhost)
        throw std::invalid_argument(
          "lower Ghost image contains a non-K+Ghost child");
    const LowerGhostState first =
      decode_lower_ghost(children.front().index);
    LowerGhostImage result{
      first.side, first.ownerKing, first.observerKing, first.visible, {}};
    for (const ClassifiedChild& child : children) {
        if (child.domain != ChildDomain::LowerGhost)
            throw std::invalid_argument(
              "lower Ghost image contains a non-K+Ghost child");
        const LowerGhostState state = decode_lower_ghost(child.index);
        if (state.side != result.side || state.ownerKing != result.ownerKing ||
            state.observerKing != result.observerKing ||
            state.visible != result.visible)
            throw std::invalid_argument(
              "one observation mixes lower Ghost public frames");
        result.locations.set(state.ghost);
    }
    return result;
}

std::vector<std::vector<TransitionWorld>> transition_partition(
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds,
  const ActionKey& action, Color observer) {
    validate_frame(frame);
    std::map<std::string, std::vector<TransitionWorld>> grouped;
    std::set<unsigned> seen;
    for (const ProductWorld& world : worlds) {
        const unsigned variable = product_variable(frame, world);
        if (!seen.insert(variable).second)
            throw std::invalid_argument(
              "transition partition contains a duplicate product world");
        const Position position = make_position(frame, world);
        const std::optional<Move> move = locate_action(position, action);
        if (!move)
            continue;
        Position child = position;
        Undo undo;
        if (!child.make_move(*move, undo))
            throw std::runtime_error(
              "complete legal Jester/Ghost action failed to apply");
        const std::string observation = transition_observation_key(
          position, *move, child, {observer, false});
        const ClassifiedChild classified = classify_child(child);
        std::optional<FramedWorld> physical;
        if (classified.domain == ChildDomain::SameClass) {
            physical = same_class_product(child);
            if (!physical)
                throw std::runtime_error(
                  "same-class child lost its physical product coordinate");
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
                                       const ProductWorld& world) {
    const Position position = make_position(frame, world);
    if (position.has_forced_action() ||
        !position.ordinary_predecessor_king_safe())
        return AdmissionVerdict::Reject;
    if (!frame.visibleGhost && adjacent(world.ghost, frame.blackKing))
        return AdmissionVerdict::Reject;
    return AdmissionVerdict::Admit;
}

std::vector<ProductWorld> admitted_fresh_worlds(const PublicFrame& frame) {
    std::vector<ProductWorld> result;
    for (const ProductWorld& world : geometric_worlds(frame))
        if (fresh_world_admission(frame, world) == AdmissionVerdict::Admit)
            result.push_back(world);
    return result;
}

}  // namespace Stockfish::Ultimate::JesterGhostInformation
