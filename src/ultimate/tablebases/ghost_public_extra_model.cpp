/*
  Ultimate Fish - exact one-Ghost plus one-public-extra information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "ghost_public_extra_model.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <tuple>

namespace Stockfish::Ultimate::GhostPublicExtra {
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
    throw std::runtime_error("one-Ghost public-extra square rank is invalid");
}

[[nodiscard]] ConcreteState horizontal_canonical(ConcreteState state) {
#ifndef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.first = horizontal_reflection(state.first);
        state.second = horizontal_reflection(state.second);
    }
#endif
    return state;
}

void validate_material(const MaterialSpec& material) {
    if (material.extraType == PieceType::King ||
        material.extraType == PieceType::Jester ||
        material.extraType == PieceType::Ghost ||
        material.extraType == PieceType::Count)
        throw std::invalid_argument(
          "public-extra model requires one concrete non-royal, non-Ghost extra");
    if (material.extraType == PieceType::Giant)
        throw std::invalid_argument(
          "Giant requires an anchor-aware public-extra geometry adapter");
    if ((material.sourceOrder == SourceOrder::ExtraPrimary &&
         material.extraColor != Color::White) ||
        (material.sourceOrder == SourceOrder::GhostPrimary &&
         material.ghostColor != Color::White))
        throw std::invalid_argument(
          "the primary source-table piece must belong to White");
}

[[nodiscard]] bool occupied_squares_distinct(const ConcreteState& state) {
    const std::array<std::uint8_t, 4> squares{
      state.whiteKing, state.blackKing, state.first, state.second};
    for (std::size_t first = 0; first < squares.size(); ++first)
        for (std::size_t second = first + 1; second < squares.size(); ++second)
            if (squares[first] == squares[second])
                return false;
    return true;
}

[[nodiscard]] bool move_auxiliary_is_square(const ActionKey& action) {
    // Pull stores the forced landing square. Swap/Link/Castle/Shoot store a
    // piece ID. Ordinary moves in the supported point-extra domains have no
    // auxiliary; fail closed rather than guessing a future piece's encoding.
    if (action.kind == MoveKind::Pull)
        return true;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    if (action.kind == MoveKind::Normal && action.auxiliary != 0)
        return true;
#endif
    if (action.kind == MoveKind::Normal && action.auxiliary != 0)
        throw std::invalid_argument(
          "nonzero ordinary-move auxiliary needs a material action adapter");
    return false;
}

struct LiveMaterial {
    int whiteKing = Position::NoSquare;
    int blackKing = Position::NoSquare;
    int extra = Position::NoSquare;
    int extraId = Position::NoPiece;
    std::uint8_t extraSubstate = 0;
    Color extraColor = Color::White;
    int ghost = Position::NoSquare;
    Color ghostColor = Color::White;
    bool visible = false;
    int live = 0;
    bool unexpected = false;
};

[[nodiscard]] LiveMaterial scan_live_material(
  const Position& position, const MaterialSpec& material) {
    LiveMaterial result;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
        if (piece.type == PieceType::CopycatClone &&
            material.extraType == PieceType::Copycat)
            continue;
#endif
        ++result.live;
        if (piece.type == PieceType::King && piece.color == Color::White &&
            result.whiteKing == Position::NoSquare)
            result.whiteKing = piece.square;
        else if (piece.type == PieceType::King && piece.color == Color::Black &&
                 result.blackKing == Position::NoSquare)
            result.blackKing = piece.square;
        else if ((piece.type == material.extraType ||
                  (material.extraType == PieceType::Checker &&
                   piece.type == PieceType::CheckerKing)) &&
                 piece.color == material.extraColor &&
                 result.extra == Position::NoSquare) {
            result.extra = piece.square;
            result.extraId = id;
            result.extraColor = piece.color;
        }
        else if (piece.type == PieceType::Ghost &&
                 result.ghost == Position::NoSquare) {
            if (piece.parasiteTracked) {
                result.unexpected = true;
                continue;
            }
            result.ghost = piece.square;
            result.ghostColor = piece.color;
            result.visible = piece.visible;
        }
        else
            result.unexpected = true;
    }
    if (result.extraId != Position::NoPiece) {
        const auto substate = position.tablebase_substate(
          result.extraId, material.extraType);
        if (!substate || *substate >= ExtraSubstateCount)
            result.unexpected = true;
        else
            result.extraSubstate = static_cast<std::uint8_t>(*substate);
    }
    return result;
}

[[nodiscard]] std::optional<ConcreteState> same_class_state(
  const Position& position, const MaterialSpec& material) {
    const LiveMaterial live = scan_live_material(position, material);
    if (live.unexpected || live.live != 4 ||
        live.whiteKing == Position::NoSquare ||
        live.blackKing == Position::NoSquare ||
        live.extra == Position::NoSquare || live.ghost == Position::NoSquare ||
        live.ghostColor != material.ghostColor)
        return std::nullopt;
    ConcreteState result;
    result.side = position.side_to_move();
    result.whiteKing = static_cast<std::uint8_t>(live.whiteKing);
    result.blackKing = static_cast<std::uint8_t>(live.blackKing);
    result.ghostVisible = live.visible;
    result.extraSubstate = live.extraSubstate;
    return with_piece_squares(result, material,
      static_cast<std::uint8_t>(live.extra),
      static_cast<std::uint8_t>(live.ghost));
}

[[nodiscard]] bool adjacent(std::uint8_t first, std::uint8_t second) {
    return std::abs(int(first % Position::BoardFiles) -
                    int(second % Position::BoardFiles)) <= 1 &&
           std::abs(int(first / Position::BoardFiles) -
                    int(second / Position::BoardFiles)) <= 1;
}

}  // namespace

MaterialSpec bishop_same() {
    return {PieceType::Bishop, Color::White, Color::White,
      SourceOrder::ExtraPrimary,
      HiddenAdjacentPolicy::ImpossibleWithoutForcedRelocation,
      "kbishopghostk"};
}

MaterialSpec bishop_reciprocal() {
    return {PieceType::Bishop, Color::White, Color::Black,
      SourceOrder::ExtraPrimary,
      HiddenAdjacentPolicy::ImpossibleWithoutForcedRelocation,
      "kbishopkghost"};
}

MaterialSpec mage_same_contract() {
    return {PieceType::Mage, Color::White, Color::White,
      SourceOrder::GhostPrimary,
      HiddenAdjacentPolicy::NeedsExactCausalAudit,
      "kghostmagek"};
}

MaterialSpec mage_reciprocal_contract() {
    return {PieceType::Mage, Color::Black, Color::White,
      SourceOrder::GhostPrimary,
      HiddenAdjacentPolicy::NeedsExactCausalAudit,
      "kghostkmage"};
}

MaterialSpec fisherman_same_contract() {
    return {PieceType::Fisherman, Color::White, Color::White,
      SourceOrder::GhostPrimary,
      HiddenAdjacentPolicy::NeedsExactCausalAudit,
      "kghostfishermank"};
}

MaterialSpec fisherman_reciprocal_contract() {
    return {PieceType::Fisherman, Color::Black, Color::White,
      SourceOrder::GhostPrimary,
      HiddenAdjacentPolicy::NeedsExactCausalAudit,
      "kghostkfisherman"};
}

std::uint8_t extra_square(const ConcreteState& state,
                          const MaterialSpec& material) {
    return material.sourceOrder == SourceOrder::ExtraPrimary
         ? state.first : state.second;
}

std::uint8_t ghost_square(const ConcreteState& state,
                          const MaterialSpec& material) {
    return material.sourceOrder == SourceOrder::GhostPrimary
         ? state.first : state.second;
}

ConcreteState with_piece_squares(ConcreteState state,
                                 const MaterialSpec& material,
                                 std::uint8_t extra,
                                 std::uint8_t ghost) {
    if (material.sourceOrder == SourceOrder::ExtraPrimary) {
        state.first = extra;
        state.second = ghost;
    }
    else {
        state.first = ghost;
        state.second = extra;
    }
    return state;
}

std::uint32_t encode_index(const ConcreteState& source,
                           const MaterialSpec& material) {
    validate_material(material);
    ConcreteState state = horizontal_canonical(source);
    if (!occupied_squares_distinct(state))
        throw std::invalid_argument(
          "one-Ghost public-extra placement overlaps a model");
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    const std::uint32_t whiteRank = state.whiteKing;
    constexpr std::uint32_t WhiteKingCount = Squares;
#else
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
      (Position::BoardFiles / 2) +
      state.whiteKing % Position::BoardFiles;
    constexpr std::uint32_t WhiteKingCount = Squares / 2;
#endif
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t firstRank = rank_excluding(
      state.first, {state.whiteKing, state.blackKing});
    const std::uint32_t secondRank = rank_excluding(
      state.second, {state.whiteKing, state.blackKing, state.first});
    const std::uint32_t placement =
      ((((static_cast<std::uint32_t>(state.side) * WhiteKingCount + whiteRank)
          * (Squares - 1) + blackRank)
         * (Squares - 2) + firstRank)
        * (Squares - 3) + secondRank);
    if (state.extraSubstate >= ExtraSubstateCount)
        throw std::invalid_argument("public-extra substate is out of range");
    return (placement * ExtraSubstateCount + state.extraSubstate) * 2 +
           (state.ghostVisible ? 1u : 0u);
}

ConcreteState decode_index(std::uint32_t index,
                           const MaterialSpec& material) {
    validate_material(material);
    if (index >= StateCount)
        throw std::out_of_range(
          "one-Ghost public-extra state index is out of range");
    const bool visible = index % 2 != 0;
    index /= 2;
    const std::uint8_t extraSubstate = static_cast<std::uint8_t>(
      index % ExtraSubstateCount);
    index /= ExtraSubstateCount;
    const std::uint32_t secondRank = index % (Squares - 3);
    index /= Squares - 3;
    const std::uint32_t firstRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    const std::uint32_t whiteRank = index % Squares;
    const Color side = static_cast<Color>(index / Squares);
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(whiteRank);
#else
    const std::uint32_t whiteRank = index % (Squares / 2);
    const Color side = static_cast<Color>(index / (Squares / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Position::BoardFiles / 2)) * Position::BoardFiles +
      whiteRank % (Position::BoardFiles / 2));
#endif
    const std::uint8_t blackKing = unrank_excluding(
      blackRank, {whiteKing});
    const std::uint8_t first = unrank_excluding(
      firstRank, {whiteKing, blackKing});
    const std::uint8_t second = unrank_excluding(
      secondRank, {whiteKing, blackKing, first});
    return {side, whiteKing, blackKing, first, second, extraSubstate, visible};
}

RoleState normalize_roles(const ConcreteState& state,
                          const MaterialSpec& material) {
    validate_material(material);
    const bool ownerIsWhite = material.ghostColor == Color::White;
    return {
      state.side == material.ghostColor ? Role::GhostOwner : Role::Observer,
      ownerIsWhite ? state.whiteKing : state.blackKing,
      ownerIsWhite ? state.blackKing : state.whiteKing,
      extra_square(state, material), ghost_square(state, material),
      state.extraSubstate, state.ghostVisible,
      material.extra_owned_by_ghost_owner()};
}

ConcreteState denormalize_roles(const RoleState& state,
                                const MaterialSpec& material) {
    validate_material(material);
    if (state.extraOwnedByOwner != material.extra_owned_by_ghost_owner())
        throw std::invalid_argument(
          "role state disagrees with public-extra ownership");
    ConcreteState result;
    result.side = state.side == Role::GhostOwner
                ? material.ghostColor : material.observer_color();
    if (material.ghostColor == Color::White) {
        result.whiteKing = state.ownerKing;
        result.blackKing = state.observerKing;
    }
    else {
        result.whiteKing = state.observerKing;
        result.blackKing = state.ownerKing;
    }
    result.ghostVisible = state.ghostVisible;
    result.extraSubstate = state.extraSubstate;
    return with_piece_squares(
      result, material, state.extra, state.ghost);
}

Position make_position(const ConcreteState& state,
                       const MaterialSpec& material) {
    validate_material(material);
    if (!valid_concrete_world(state, material))
        throw std::invalid_argument(
          "cannot construct overlapping one-Ghost public-extra world");
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, state.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, state.blackKing);
    const PieceType representedExtra =
      material.extraType == PieceType::Checker && (state.extraSubstate & 2u)
        ? PieceType::CheckerKing : material.extraType;
    const int extra = position.add_piece(
      representedExtra, material.extraColor,
      extra_square(state, material));
    const int ghost = position.add_piece(
      PieceType::Ghost, material.ghostColor,
      ghost_square(state, material));
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        extra == Position::NoPiece || ghost == Position::NoPiece)
        throw std::runtime_error(
          "cannot construct one-Ghost public-extra world");
    for (const int id : {whiteKing, blackKing, extra, ghost})
        position.piece(id).moved = true;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    if (material.extraType == PieceType::Copycat) {
        const int clone = position.piece(extra).link;
        if (clone == Position::NoPiece)
            throw std::runtime_error("Copycat public-extra clone is missing");
        position.piece(clone).moved = true;
    }
#endif
    position.piece(ghost).visible = state.ghostVisible;
    if (!position.apply_tablebase_substate(
          extra, material.extraType, state.extraSubstate))
        throw std::runtime_error("cannot apply public-extra substate");
    position.set_side_to_move(state.side);
    return position;
}

bool valid_concrete_world(const ConcreteState& state,
                          const MaterialSpec& material) {
    if (!occupied_squares_distinct(state))
        return false;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    if (material.extraType == PieceType::Copycat) {
        const std::uint8_t clone = horizontal_reflection(
          extra_square(state, material));
        if (clone == state.whiteKing || clone == state.blackKing ||
            clone == ghost_square(state, material))
            return false;
    }
#endif
    return tablebase_substate_geometrically_valid(
      material.extraType, state.whiteKing, state.blackKing,
      extra_square(state, material), ghost_square(state, material),
      state.extraSubstate);
}

bool tablebase_substate_geometrically_valid(
  PieceType type, std::uint8_t whiteKing, std::uint8_t blackKing,
  std::uint8_t extra, std::uint8_t other, std::uint32_t substate) {
    if (type != PieceType::Penguin)
        return true;
    if (substate >= 8)
        return false;
    const auto adjacent = [](std::uint8_t first, std::uint8_t second) {
        return std::abs(int(first % Position::BoardFiles) -
                        int(second % Position::BoardFiles)) <= 1 &&
               std::abs(int(first / Position::BoardFiles) -
                        int(second / Position::BoardFiles)) <= 1;
    };
    std::uint32_t available = 0;
    if (adjacent(extra, whiteKing)) available |= 1u;
    if (adjacent(extra, blackKing)) available |= 2u;
    if (adjacent(extra, other)) available |= 4u;
    return (substate & ~available) == 0;
}

Position make_position(std::uint32_t index,
                       const MaterialSpec& material) {
    return make_position(decode_index(index, material), material);
}

std::uint8_t transform_square(std::uint8_t square,
                              RectangleTransform transform) {
    if (static_cast<std::uint8_t>(transform) & 1u)
        square = horizontal_reflection(square);
    if (static_cast<std::uint8_t>(transform) & 2u)
        square = vertical_reflection(square);
    return square;
}

ConcreteState transform_state(ConcreteState state,
                              RectangleTransform transform) {
    state.whiteKing = transform_square(state.whiteKing, transform);
    state.blackKing = transform_square(state.blackKing, transform);
    state.first = transform_square(state.first, transform);
    state.second = transform_square(state.second, transform);
    return state;
}

bool operator<(const ActionKey& lhs, const ActionKey& rhs) {
    return std::tie(lhs.from, lhs.to, lhs.auxiliary, lhs.kind, lhs.promotion) <
           std::tie(rhs.from, rhs.to, rhs.auxiliary, rhs.kind, rhs.promotion);
}

ActionKey action_key(const Move& move) {
    return {move.from, move.to, move.auxiliary, move.kind, move.promotion};
}

ActionKey transform_action(const ActionKey& source,
                           RectangleTransform transform) {
    ActionKey action = source;
    if (action.kind == MoveKind::Pass)
        return action;
    if (action.from >= Squares || action.to >= Squares)
        throw std::invalid_argument("action endpoint is outside the board");
    action.from = transform_square(action.from, transform);
    action.to = transform_square(action.to, transform);
    if (move_auxiliary_is_square(action)) {
        if (action.auxiliary >= Squares)
            throw std::invalid_argument(
              "square-valued action auxiliary is outside the board");
        action.auxiliary = transform_square(action.auxiliary, transform);
    }
    return action;
}

std::vector<ActionKey> legal_action_keys(const Position& position) {
    std::vector<ActionKey> result;
    for (const Move& move : position.legal_moves())
        result.push_back(action_key(move));
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end())
        throw std::runtime_error(
          "legal move generator produced duplicate complete actions");
    return result;
}

std::vector<DecisionMarker> decision_markers(const Position& position) {
    std::vector<DecisionMarker> result;
    for (const Move& move : position.legal_moves()) {
        if (move.kind == MoveKind::Pass)
            result.push_back({-1, -1});
        else
            result.push_back({move.from, move.to});
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<DecisionMarker> transform_markers(
  std::vector<DecisionMarker> markers, RectangleTransform transform) {
    for (DecisionMarker& marker : markers) {
        if (marker.from < 0)
            continue;
        marker.from = transform_square(
          static_cast<std::uint8_t>(marker.from), transform);
        marker.to = transform_square(
          static_cast<std::uint8_t>(marker.to), transform);
    }
    std::sort(markers.begin(), markers.end());
    return markers;
}

std::uint32_t encode_lower_ghost(const LowerGhostState& state) {
    if (state.ownerKing == state.observerKing ||
        state.ownerKing == state.ghost || state.observerKing == state.ghost)
        throw std::invalid_argument("overlapping lower Ghost placement");
    const std::uint32_t observerRank = state.observerKing -
      (state.observerKing > state.ownerKing ? 1u : 0u);
    const std::uint32_t low = std::min(state.ownerKing, state.observerKing);
    const std::uint32_t high = std::max(state.ownerKing, state.observerKing);
    const std::uint32_t ghostRank = state.ghost -
      (state.ghost > low ? 1u : 0u) -
      (state.ghost > high ? 1u : 0u);
    const std::uint32_t placement =
      (((static_cast<std::uint32_t>(state.side) * Squares + state.ownerKing) *
        (Squares - 1) + observerRank) * (Squares - 2) + ghostRank);
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

ClassifiedChild classify_child(const Position& position,
                               const MaterialSpec& material) {
    validate_material(material);
    if (const auto same = same_class_state(position, material))
        return {ChildDomain::SameClass, encode_index(*same, material), {}};

    const LiveMaterial live = scan_live_material(position, material);
    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.ghost != Position::NoSquare &&
        live.extra == Position::NoSquare) {
        if (position.game_over())
            return {ChildDomain::ExactTerminal, 0, position.winner()};
        const bool ownerIsWhite = live.ghostColor == Color::White;
        const LowerGhostState lower{
          position.side_to_move() == live.ghostColor
            ? Role::GhostOwner : Role::Observer,
          static_cast<std::uint8_t>(ownerIsWhite
            ? live.whiteKing : live.blackKing),
          static_cast<std::uint8_t>(ownerIsWhite
            ? live.blackKing : live.whiteKing),
          static_cast<std::uint8_t>(live.ghost), live.visible};
        return {ChildDomain::LowerGhost, encode_lower_ghost(lower), {}};
    }

    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.extra != Position::NoSquare &&
        live.ghost == Position::NoSquare) {
        if (position.game_over() && !position.winner())
            return {ChildDomain::InsufficientExtra, 0, {}};
        if (position.game_over())
            return {ChildDomain::ExactTerminal, 0, position.winner()};
        return {ChildDomain::LowerExtraConcrete, 0, {}};
    }

    if (position.game_over())
        return {ChildDomain::ExactTerminal, 0, position.winner()};
    return {ChildDomain::Invalid, 0, {}};
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
    return static_cast<unsigned>(__builtin_popcountll(low) +
                                 __builtin_popcount(high));
}

LowerGhostImage inherited_lower_ghost_image(
  const std::vector<ClassifiedChild>& children) {
    if (children.empty())
        throw std::invalid_argument("lower Ghost image is empty");
    const LowerGhostState first = [&] {
        if (children.front().domain != ChildDomain::LowerGhost)
            throw std::invalid_argument(
              "lower Ghost image contains a non-K+Ghost child");
        return decode_lower_ghost(children.front().index);
    }();
    LowerGhostImage result{first.side, first.ownerKing,
      first.observerKing, first.visible, {}};
    for (const ClassifiedChild& child : children) {
        if (child.domain != ChildDomain::LowerGhost)
            throw std::invalid_argument(
              "lower Ghost image contains a non-K+Ghost child");
        const LowerGhostState state = decode_lower_ghost(child.index);
        if (state.side != result.side || state.ownerKing != result.ownerKing ||
            state.observerKing != result.observerKing ||
            state.visible != result.visible)
            throw std::invalid_argument(
              "one observation mixes lower K+Ghost public geometries");
        result.locations.set(state.ghost);
    }
    return result;
}

FreshAdmissionVerdict classify_fresh_root_admission(
  const Position& position, const MaterialSpec& material) {
    const auto source = same_class_state(position, material);
    if (!source)
        return FreshAdmissionVerdict::Reject;
    if (position.has_forced_action() ||
        !position.ordinary_predecessor_king_safe())
        return FreshAdmissionVerdict::Reject;
    const RoleState roles = normalize_roles(*source, material);
    if (!roles.ghostVisible && adjacent(roles.ghost, roles.observerKing))
        return material.hiddenAdjacent ==
                 HiddenAdjacentPolicy::NeedsExactCausalAudit
             ? FreshAdmissionVerdict::NeedsExactCausalAudit
             : FreshAdmissionVerdict::Reject;
    return FreshAdmissionVerdict::Admit;
}

}  // namespace Stockfish::Ultimate::GhostPublicExtra
