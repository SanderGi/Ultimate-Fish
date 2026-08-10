/* Ultimate Fish exact one-Ghost plus Giant information model. GPLv3+. */

#include "ghost_giant_information_model.h"

#include "information.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace Stockfish::Ultimate::GhostGiant {
namespace {

constexpr std::uint8_t Squares = Position::BoardSquares;

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
    throw std::runtime_error("Giant/Ghost square rank is invalid");
}

[[nodiscard]] bool point_distinct(const SourceState& state) {
    const std::array<std::uint8_t, 4> squares{
      state.whiteKing, state.blackKing, state.ghost, state.giant};
    for (std::size_t first = 0; first < squares.size(); ++first)
        for (std::size_t second = first + 1; second < squares.size(); ++second)
            if (squares[first] == squares[second])
                return false;
    return true;
}

[[nodiscard]] bool adjacent(std::uint8_t lhs, std::uint8_t rhs) {
    return std::abs(int(lhs % Position::BoardFiles) -
                    int(rhs % Position::BoardFiles)) <= 1 &&
           std::abs(int(lhs / Position::BoardFiles) -
                    int(rhs / Position::BoardFiles)) <= 1;
}

[[nodiscard]] SourceState horizontal_canonical(SourceState state) {
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = transform_square(
          state.whiteKing, RectangleTransform::Horizontal);
        state.blackKing = transform_square(
          state.blackKing, RectangleTransform::Horizontal);
        state.ghost = transform_square(
          state.ghost, RectangleTransform::Horizontal);
        state.giant = transform_giant_anchor(
          state.giant, RectangleTransform::Horizontal);
    }
    return state;
}

}  // namespace

bool operator<(const PublicGeometry& lhs, const PublicGeometry& rhs) {
    return std::tie(lhs.side, lhs.whiteKing, lhs.blackKing, lhs.giant,
                    lhs.visible) <
           std::tie(rhs.side, rhs.whiteKing, rhs.blackKing, rhs.giant,
                    rhs.visible);
}

bool valid_giant_anchor(std::uint8_t anchor) {
    return anchor < Squares &&
           anchor % Position::BoardFiles < Position::BoardFiles - 1 &&
           anchor / Position::BoardFiles < Position::BoardRanks - 1;
}

Bitboard giant_footprint(std::uint8_t anchor) {
    if (!valid_giant_anchor(anchor))
        return 0;
    const Bitboard origin = Bitboard(1) << anchor;
    return origin | (origin << 1) | (origin << Position::BoardFiles) |
           (origin << (Position::BoardFiles + 1));
}

bool valid_physical_geometry(const NormalizedState& state) {
    const Bitboard footprint = giant_footprint(state.giant);
    if (!footprint || state.whiteKing == state.blackKing ||
        (footprint & (Bitboard(1) << state.whiteKing)) ||
        (footprint & (Bitboard(1) << state.blackKing)) ||
        (footprint & (Bitboard(1) << state.ghost)) ||
        state.ghost == state.whiteKing || state.ghost == state.blackKing)
        return false;
    return true;
}

std::uint8_t transform_square(std::uint8_t square,
                              RectangleTransform transform) {
    if (square >= Squares)
        throw std::out_of_range("Giant/Ghost point square is outside board");
    int file = square % Position::BoardFiles;
    int rank = square / Position::BoardFiles;
    if (static_cast<std::uint8_t>(transform) & 1u)
        file = Position::BoardFiles - 1 - file;
    if (static_cast<std::uint8_t>(transform) & 2u)
        rank = Position::BoardRanks - 1 - rank;
    return static_cast<std::uint8_t>(rank * Position::BoardFiles + file);
}

std::uint8_t transform_giant_anchor(std::uint8_t anchor,
                                    RectangleTransform transform) {
    // Keep invalid file-h/rank-10 dense records total and involutive. They are
    // never admitted as physical worlds, but the source codec must round-trip.
    int file = anchor % Position::BoardFiles;
    int rank = anchor / Position::BoardFiles;
    if (static_cast<std::uint8_t>(transform) & 1u)
        file = file == Position::BoardFiles - 1
             ? file : Position::BoardFiles - 2 - file;
    if (static_cast<std::uint8_t>(transform) & 2u)
        rank = rank == Position::BoardRanks - 1
             ? rank : Position::BoardRanks - 2 - rank;
    return static_cast<std::uint8_t>(rank * Position::BoardFiles + file);
}

SourceState transform_source(SourceState state, RectangleTransform transform) {
    state.whiteKing = transform_square(state.whiteKing, transform);
    state.blackKing = transform_square(state.blackKing, transform);
    state.ghost = transform_square(state.ghost, transform);
    state.giant = transform_giant_anchor(state.giant, transform);
    return state;
}

NormalizedState transform_normalized(NormalizedState state,
                                     RectangleTransform transform) {
    state.whiteKing = transform_square(state.whiteKing, transform);
    state.blackKing = transform_square(state.blackKing, transform);
    state.ghost = transform_square(state.ghost, transform);
    state.giant = transform_giant_anchor(state.giant, transform);
    return state;
}

PublicGeometry transform_geometry(PublicGeometry geometry,
                                  RectangleTransform transform) {
    geometry.whiteKing = transform_square(geometry.whiteKing, transform);
    geometry.blackKing = transform_square(geometry.blackKing, transform);
    geometry.giant = transform_giant_anchor(geometry.giant, transform);
    return geometry;
}

CanonicalGeometry canonical_geometry(const PublicGeometry& source) {
    CanonicalGeometry result{source, RectangleTransform::Identity};
    for (std::uint8_t value = 1; value < 4; ++value) {
        const auto transform = static_cast<RectangleTransform>(value);
        const PublicGeometry candidate = transform_geometry(source, transform);
        if (candidate < result.geometry)
            result = {candidate, transform};
    }
    return result;
}

std::uint32_t encode_source(const SourceState& source) {
    SourceState state = horizontal_canonical(source);
    if (!point_distinct(state))
        throw std::invalid_argument("Giant/Ghost dense codec overlaps anchors");
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
        (Position::BoardFiles / 2) +
      state.whiteKing % Position::BoardFiles;
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t ghostRank = rank_excluding(
      state.ghost, {state.whiteKing, state.blackKing});
    const std::uint32_t giantRank = rank_excluding(
      state.giant, {state.whiteKing, state.blackKing, state.ghost});
    const std::uint32_t placement =
      ((((static_cast<std::uint32_t>(state.side) * (Squares / 2) + whiteRank)
          * (Squares - 1) + blackRank) * (Squares - 2) + ghostRank)
        * (Squares - 3) + giantRank);
    return placement * 2 + (state.ghostVisible ? 1u : 0u);
}

SourceState decode_source(std::uint32_t index) {
    if (index >= StateCount)
        throw std::out_of_range("Giant/Ghost dense index is outside domain");
    SourceState state;
    state.ghostVisible = index % 2 != 0;
    index /= 2;
    const std::uint32_t giantRank = index % (Squares - 3);
    index /= Squares - 3;
    const std::uint32_t ghostRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    state.side = static_cast<Color>(index / (Squares / 2));
    state.whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Position::BoardFiles / 2)) * Position::BoardFiles +
      whiteRank % (Position::BoardFiles / 2));
    state.blackKing = unrank_excluding(blackRank, {state.whiteKing});
    state.ghost = unrank_excluding(
      ghostRank, {state.whiteKing, state.blackKing});
    state.giant = unrank_excluding(
      giantRank, {state.whiteKing, state.blackKing, state.ghost});
    return state;
}

NormalizedState normalize(const SourceState& state, Orientation orientation) {
    if (orientation == Orientation::Same)
        return {state.side, state.whiteKing, state.blackKing, state.giant,
                state.ghost, state.ghostVisible};
    return {~state.side, state.blackKing, state.whiteKing, state.giant,
            state.ghost, state.ghostVisible};
}

SourceState denormalize(const NormalizedState& state,
                        Orientation orientation) {
    if (orientation == Orientation::Same)
        return {state.side, state.whiteKing, state.blackKing, state.ghost,
                state.giant, state.ghostVisible};
    return {~state.side, state.blackKing, state.whiteKing, state.ghost,
            state.giant, state.ghostVisible};
}

Position make_position(const NormalizedState& state, Orientation orientation) {
    (void)orientation;
    if (!valid_physical_geometry(state))
        throw std::invalid_argument("invalid physical Giant/Ghost geometry");
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, state.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, state.blackKing);
    const int giant = position.add_piece(
      PieceType::Giant, Color::White, state.giant);
    const Color ghostColor = orientation == Orientation::Same
                           ? Color::White : Color::Black;
    const int ghost = position.add_piece(
      PieceType::Ghost, ghostColor, state.ghost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        giant == Position::NoPiece || ghost == Position::NoPiece)
        throw std::runtime_error("failed constructing Giant/Ghost world");
    for (const int id : {whiteKing, blackKing, giant, ghost})
        position.piece(id).moved = true;
    position.piece(ghost).visible = state.ghostVisible;
    position.set_side_to_move(state.side);
    return position;
}

std::optional<NormalizedState> scan_same_class(
  const Position& position, Orientation orientation) {
    NormalizedState state;
    state.side = position.side_to_move();
    int live = 0;
    bool wk = false, bk = false, giant = false, ghost = false;
    const Color ghostColor = orientation == Orientation::Same
                           ? Color::White : Color::Black;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++live;
        if (piece.type == PieceType::King && piece.color == Color::White && !wk) {
            state.whiteKing = static_cast<std::uint8_t>(piece.square); wk = true;
        }
        else if (piece.type == PieceType::King && piece.color == Color::Black && !bk) {
            state.blackKing = static_cast<std::uint8_t>(piece.square); bk = true;
        }
        else if (piece.type == PieceType::Giant && piece.color == Color::White && !giant) {
            state.giant = static_cast<std::uint8_t>(piece.square); giant = true;
        }
        else if (piece.type == PieceType::Ghost && piece.color == ghostColor && !ghost) {
            state.ghost = static_cast<std::uint8_t>(piece.square);
            state.ghostVisible = piece.visible; ghost = true;
        }
        else
            return std::nullopt;
    }
    if (live != 4 || !wk || !bk || !giant || !ghost ||
        !valid_physical_geometry(state))
        return std::nullopt;
    return state;
}

ActionKey action_key(const Move& move) {
    return GhostPublicExtra::action_key(move);
}

ActionKey transform_action(const Position& before, const Move& move,
                           RectangleTransform transform) {
    ActionKey result = action_key(move);
    if (move.kind == MoveKind::Pass)
        return result;
    const int actor = before.piece_on(move.from);
    if (actor == Position::NoPiece)
        throw std::runtime_error("Giant D2 action has no native actor");
    const bool giant = before.piece(actor).type == PieceType::Giant;
    result.from = giant ? transform_giant_anchor(result.from, transform)
                        : transform_square(result.from, transform);
    result.to = giant ? transform_giant_anchor(result.to, transform)
                      : transform_square(result.to, transform);
    if (move.kind == MoveKind::Pull)
        result.auxiliary = transform_square(result.auxiliary, transform);
    else if (move.kind == MoveKind::Normal && result.auxiliary != 0)
        throw std::runtime_error("unknown Giant ordinary-action auxiliary");
    return result;
}

std::vector<ActionKey> legal_action_keys(const Position& position) {
    std::vector<ActionKey> result;
    for (const Move& move : position.legal_moves())
        result.push_back(action_key(move));
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end())
        throw std::runtime_error("Giant world duplicates a complete action");
    return result;
}

std::vector<DecisionMarker> decision_markers(const Position& position) {
    return GhostPublicExtra::decision_markers(position);
}

std::vector<DecisionMarker> transform_markers(
  const Position& before, const std::vector<Move>& moves,
  RectangleTransform transform) {
    std::vector<DecisionMarker> result;
    for (const Move& move : moves) {
        if (move.kind == MoveKind::Pass)
            result.push_back({-1, -1});
        else {
            const ActionKey action = transform_action(before, move, transform);
            result.push_back({action.from, action.to});
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool fresh_root_admitted(const NormalizedState& state,
                         Orientation orientation) {
    if (!valid_physical_geometry(state))
        return false;
    const Position position = make_position(state, orientation);
    if (position.has_forced_action() || !position.ordinary_predecessor_king_safe())
        return false;
    const std::uint8_t observerKing = orientation == Orientation::Same
      ? state.blackKing : state.whiteKing;
    return state.ghostVisible || !adjacent(state.ghost, observerKing);
}

void exact_model_self_test() {
    std::uint64_t validDense = 0;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const SourceState state = decode_source(index);
        if (encode_source(state) != index)
            throw std::runtime_error("Giant/Ghost dense codec is not bijective");
        const SourceState reflected = transform_source(
          state, RectangleTransform::Horizontal);
        const SourceState restored = transform_source(
          reflected, RectangleTransform::Horizontal);
        if (encode_source(reflected) != index ||
            restored.side != state.side ||
            restored.whiteKing != state.whiteKing ||
            restored.blackKing != state.blackKing ||
            restored.ghost != state.ghost || restored.giant != state.giant ||
            restored.ghostVisible != state.ghostVisible)
            throw std::runtime_error("Giant anchor horizontal codec residual");
        for (const Orientation orientation : {Orientation::Same,
                                               Orientation::Opposing})
            if (encode_source(denormalize(normalize(state, orientation),
                                          orientation)) != index)
                throw std::runtime_error("Giant role normalization residual");
        validDense += valid_physical_geometry(
          normalize(state, Orientation::Same));
    }
    if (validDense != 53'146'800)
        throw std::runtime_error("Giant physical dense-state count residual");

    std::uint64_t physical = 0;
    for (std::uint8_t giant = 0; giant < Squares; ++giant) {
        if (!valid_giant_anchor(giant))
            continue;
        const Bitboard footprint = giant_footprint(giant);
        for (std::uint8_t wk = 0; wk < Squares; ++wk) {
            if (footprint & (Bitboard(1) << wk)) continue;
            for (std::uint8_t bk = 0; bk < Squares; ++bk) {
                if (bk == wk || (footprint & (Bitboard(1) << bk))) continue;
                const PublicGeometry raw{0, wk, bk, giant, 0};
                const CanonicalGeometry canonical = canonical_geometry(raw);
                for (std::uint8_t t = 0; t < 4; ++t) {
                    const auto transform = static_cast<RectangleTransform>(t);
                    if (!(transform_geometry(transform_geometry(raw, transform),
                                             transform) == raw))
                        throw std::runtime_error("Giant public D2 involution residual");
                }
                physical += canonical.geometry == raw;
            }
        }
    }
    // 63 anchors * 76 non-footprint White-King squares * 75 remaining Black
    // squares, divided by the free four-element rectangle action.
    if (physical != 89'775)
        throw std::runtime_error("Giant public geometry quotient count residual");

    NormalizedState witness;
    witness.side = Color::White;
    witness.whiteKing = 0;   // a1
    witness.blackKing = 79;  // h10
    witness.giant = 36;      // e5
    witness.ghostVisible = false;
    witness.ghost = 78;      // g10, adjacent to Black's King only.
    if (fresh_root_admitted(witness, Orientation::Same))
        throw std::runtime_error(
          "same-side Giant admission exposed a Ghost by the observer King");
    witness.ghost = 1;  // b1, adjacent to the owning White King only.
    if (!fresh_root_admitted(witness, Orientation::Same))
        throw std::runtime_error(
          "same-side Giant admission rejected a Ghost by its owner King");
    witness.ghost = 1;
    if (fresh_root_admitted(witness, Orientation::Opposing))
        throw std::runtime_error(
          "opposing Giant admission exposed a Ghost by the observer King");
    witness.ghost = 78;
    if (!fresh_root_admitted(witness, Orientation::Opposing))
        throw std::runtime_error(
          "opposing Giant admission rejected a Ghost by its owner King");
    std::cout << "ghost_giant_admission same_observer_reject 1"
              << " same_owner_admit 1 opposing_observer_reject 1"
              << " opposing_owner_admit 1 residual 0\n";
}

}  // namespace Stockfish::Ultimate::GhostGiant
