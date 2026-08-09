/*
  Ultimate Fish - exact one-Ghost-plus-one-extra information-tablebase preflight
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  This standalone domain preflight deliberately precedes the full symbolic
  solve.  It implements the shipping four-model horizontal-reflection codec,
  exact concrete move/observation oracle semantics, mover-private legal-dot
  observations, and closed/capture-domain classification for:

      K+Bishop+Ghost v K       (--material same)
      K+Bishop v K+Ghost       (--material opposing)

  The Bishop is public and fixed in the public geometry.  The invisible Ghost
  remains an arbitrary exact mask.  When the Bishop is captured the child is
  K+Ghost-v-K with the history-preserving parent image, so this domain consumes
  the certified arbitrary-mask UFGM1 ROBDD probe rather than incorrectly
  resetting to a dense fresh-root UFIW2 entry.
  Capturing the Ghost leaves lone Bishop material and is an exact native
  insufficient-material draw, so no nonexistent K+Bishop-v-K file is needed.

  No cap, sampling, or extrapolation is permitted in a final solve.  Sampling
  below is only an explicitly labelled scale preflight; codec verification is
  exhaustive over all 37,957,920 placements and exact move/observation/lower-
  domain semantics are checked for every sampled edge.  --exhaustive-oracle
  streams the entire 75,915,840-state oracle without retaining its billion-edge
  CSR, but is deliberately not implied by the sampled scale preflight.
*/

#include "information.h"
#include "ghost_information_probe.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/resource.h>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t PlacementCount =
  2 * (Squares / 2) * (Squares - 1) * (Squares - 2) * (Squares - 3);
constexpr std::uint32_t GhostSubstates = 2;
constexpr std::uint32_t StateCount = PlacementCount * GhostSubstates;
constexpr std::uint32_t LowerGhostStateCount =
  2 * Squares * (Squares - 1) * (Squares - 2) * GhostSubstates;
constexpr std::uint32_t NoIndex = std::numeric_limits<std::uint32_t>::max();

struct FourState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t bishop = 0;
    std::uint8_t ghost = 0;
    bool visible = false;
};

struct MaterialSpec {
    Color ghostColor = Color::White;

    [[nodiscard]] Color observer() const {
        return ghostColor == Color::White ? Color::Black : Color::White;
    }
    [[nodiscard]] const char* name() const {
        return ghostColor == Color::White ? "kbishopghostk"
                                          : "kbishopkghost";
    }
};

[[nodiscard]] std::uint8_t horizontal_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>((square / Position::BoardFiles) *
      Position::BoardFiles + Position::BoardFiles - 1 -
      square % Position::BoardFiles);
}

[[nodiscard]] std::uint8_t vertical_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>(
      (Position::BoardRanks - 1 - square / Position::BoardFiles) *
      Position::BoardFiles + square % Position::BoardFiles);
}

[[nodiscard]] FourState horizontal_canonical(FourState state) {
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.bishop = horizontal_reflection(state.bishop);
        state.ghost = horizontal_reflection(state.ghost);
    }
    return state;
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
    throw std::runtime_error("four-model square rank is invalid");
}

[[nodiscard]] std::uint32_t encode_placement(FourState state) {
    state = horizontal_canonical(state);
    if (state.whiteKing == state.blackKing ||
        state.whiteKing == state.bishop || state.whiteKing == state.ghost ||
        state.blackKing == state.bishop || state.blackKing == state.ghost ||
        state.bishop == state.ghost)
        throw std::runtime_error("overlapping four-model placement");
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
      (Position::BoardFiles / 2) + state.whiteKing % Position::BoardFiles;
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t bishopRank = rank_excluding(
      state.bishop, {state.whiteKing, state.blackKing});
    const std::uint32_t ghostRank = rank_excluding(
      state.ghost, {state.whiteKing, state.blackKing, state.bishop});
    return ((((static_cast<std::uint32_t>(state.side) * (Squares / 2) + whiteRank)
               * (Squares - 1) + blackRank)
              * (Squares - 2) + bishopRank)
             * (Squares - 3) + ghostRank);
}

[[nodiscard]] FourState decode_placement(std::uint32_t index) {
    if (index >= PlacementCount)
        throw std::runtime_error("four-model placement index is out of range");
    const std::uint32_t ghostRank = index % (Squares - 3);
    index /= Squares - 3;
    const std::uint32_t bishopRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    const Color side = static_cast<Color>(index / (Squares / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Position::BoardFiles / 2)) * Position::BoardFiles +
      whiteRank % (Position::BoardFiles / 2));
    const std::uint8_t blackKing = unrank_excluding(blackRank, {whiteKing});
    const std::uint8_t bishop = unrank_excluding(
      bishopRank, {whiteKing, blackKing});
    const std::uint8_t ghost = unrank_excluding(
      ghostRank, {whiteKing, blackKing, bishop});
    return {side, whiteKing, blackKing, bishop, ghost, false};
}

[[nodiscard]] std::uint32_t encode_index(const FourState& state) {
    return encode_placement(state) * GhostSubstates + (state.visible ? 1u : 0u);
}

[[nodiscard]] FourState decode_index(std::uint32_t index) {
    if (index >= StateCount)
        throw std::runtime_error("four-model state index is out of range");
    FourState state = decode_placement(index / GhostSubstates);
    state.visible = index % GhostSubstates != 0;
    return state;
}

[[nodiscard]] Position make_position(std::uint32_t index,
                                     const MaterialSpec& material) {
    const FourState state = decode_index(index);
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, state.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, state.blackKing);
    const int bishop = position.add_piece(
      PieceType::Bishop, Color::White, state.bishop);
    const int ghost = position.add_piece(
      PieceType::Ghost, material.ghostColor, state.ghost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        bishop == Position::NoPiece || ghost == Position::NoPiece)
        throw std::runtime_error("four-model codec produced invalid geometry");
    for (const int id : {whiteKing, blackKing, bishop, ghost})
        position.piece(id).moved = true;
    position.piece(ghost).visible = state.visible;
    position.set_side_to_move(state.side);
    return position;
}

[[nodiscard]] std::optional<std::uint32_t> same_class_index(
  const Position& position, const MaterialSpec& material) {
    FourState state;
    state.side = position.side_to_move();
    int live = 0;
    bool foundWhiteKing = false;
    bool foundBlackKing = false;
    bool foundBishop = false;
    bool foundGhost = false;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++live;
        if (piece.type == PieceType::King && piece.color == Color::White &&
            !foundWhiteKing) {
            state.whiteKing = static_cast<std::uint8_t>(piece.square);
            foundWhiteKing = true;
        }
        else if (piece.type == PieceType::King && piece.color == Color::Black &&
                 !foundBlackKing) {
            state.blackKing = static_cast<std::uint8_t>(piece.square);
            foundBlackKing = true;
        }
        else if (piece.type == PieceType::Bishop && piece.color == Color::White &&
                 !foundBishop) {
            state.bishop = static_cast<std::uint8_t>(piece.square);
            foundBishop = true;
        }
        else if (piece.type == PieceType::Ghost &&
                 piece.color == material.ghostColor && !foundGhost) {
            state.ghost = static_cast<std::uint8_t>(piece.square);
            state.visible = piece.visible;
            foundGhost = true;
        }
        else
            return std::nullopt;
    }
    if (live != 4 || !foundWhiteKing || !foundBlackKing || !foundBishop ||
        !foundGhost)
        return std::nullopt;
    return encode_index(state);
}

enum class ChildDomain : std::uint8_t {
  SameClass,
  LowerGhost,
  InsufficientBishop,
  ExactTerminal,
  Invalid,
};

struct ClassifiedChild {
    ChildDomain domain = ChildDomain::Invalid;
    std::uint32_t index = NoIndex;
};

[[nodiscard]] FourState decode_lower_ghost(std::uint32_t index);

[[nodiscard]] std::uint32_t lower_ghost_index(
  Color side, std::uint8_t ghostOwnerKing, std::uint8_t opponentKing,
  std::uint8_t ghost, bool visible) {
    if (ghostOwnerKing == opponentKing || ghostOwnerKing == ghost ||
        opponentKing == ghost)
        throw std::runtime_error("overlapping lower Ghost placement");
    const std::uint32_t blackRank = opponentKing -
      (opponentKing > ghostOwnerKing ? 1u : 0u);
    const std::uint32_t low = std::min(ghostOwnerKing, opponentKing);
    const std::uint32_t high = std::max(ghostOwnerKing, opponentKing);
    const std::uint32_t ghostRank = ghost - (ghost > low ? 1u : 0u) -
      (ghost > high ? 1u : 0u);
    const std::uint32_t placement =
      (((static_cast<std::uint32_t>(side) * Squares + ghostOwnerKing) *
        (Squares - 1) + blackRank) * (Squares - 2) + ghostRank);
    return placement * GhostSubstates + (visible ? 1u : 0u);
}

[[nodiscard]] ClassifiedChild classify_child(
  const Position& position, const MaterialSpec& material) {
    if (const auto same = same_class_index(position, material))
        return {ChildDomain::SameClass, *same};

    int whiteKing = Position::NoSquare;
    int blackKing = Position::NoSquare;
    int bishop = Position::NoSquare;
    int ghost = Position::NoSquare;
    bool visible = false;
    int live = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++live;
        if (piece.type == PieceType::King && piece.color == Color::White)
            whiteKing = piece.square;
        else if (piece.type == PieceType::King && piece.color == Color::Black)
            blackKing = piece.square;
        else if (piece.type == PieceType::Bishop && piece.color == Color::White)
            bishop = piece.square;
        else if (piece.type == PieceType::Ghost &&
                 piece.color == material.ghostColor) {
            ghost = piece.square;
            visible = piece.visible;
        }
        else
            return {ChildDomain::Invalid, NoIndex};
    }
    if (live == 3 && whiteKing != Position::NoSquare &&
        blackKing != Position::NoSquare && ghost != Position::NoSquare &&
        bishop == Position::NoSquare) {
        if (position.game_over())
            return {ChildDomain::ExactTerminal, NoIndex};
        const bool swapColors = material.ghostColor == Color::Black;
        const Color lowerSide = swapColors
          ? (position.side_to_move() == Color::White ? Color::Black
                                                     : Color::White)
          : position.side_to_move();
        const std::uint8_t ownerKing = static_cast<std::uint8_t>(
          swapColors ? blackKing : whiteKing);
        const std::uint8_t enemyKing = static_cast<std::uint8_t>(
          swapColors ? whiteKing : blackKing);
        return {ChildDomain::LowerGhost,
                lower_ghost_index(lowerSide, ownerKing, enemyKing,
                  static_cast<std::uint8_t>(ghost), visible)};
    }
    if (live == 3 && whiteKing != Position::NoSquare &&
        blackKing != Position::NoSquare && bishop != Position::NoSquare &&
        ghost == Position::NoSquare) {
        if (!position.game_over() || position.winner())
            throw std::runtime_error(
              "lone Bishop child is not a native insufficient-material draw");
        return {ChildDomain::InsufficientBishop, NoIndex};
    }
    if (position.game_over())
        return {ChildDomain::ExactTerminal, NoIndex};
    return {ChildDomain::Invalid, NoIndex};
}

void lower_color_normalization_self_test() {
    for (const Color ghostColor : {Color::White, Color::Black}) {
        const MaterialSpec material{ghostColor};
        Position child;
        child.clear();
        const int whiteKing = child.add_piece(
          PieceType::King, Color::White, 0);
        const int blackKing = child.add_piece(
          PieceType::King, Color::Black, 79);
        const int ghost = child.add_piece(PieceType::Ghost, ghostColor, 35);
        for (const int id : {whiteKing, blackKing, ghost})
            child.piece(id).moved = true;
        child.piece(ghost).visible = false;
        const Color childSide = ghostColor == Color::White
                              ? Color::Black : Color::White;
        child.set_side_to_move(childSide);
        const ClassifiedChild classified = classify_child(child, material);
        if (classified.domain != ChildDomain::LowerGhost)
            throw std::runtime_error(
              "lower Ghost color-normalization fixture did not classify");
        const FourState normalized = decode_lower_ghost(classified.index);
        const bool swap = ghostColor == Color::Black;
        const Color expectedSide = swap
          ? (childSide == Color::White ? Color::Black : Color::White)
          : childSide;
        if (normalized.side != expectedSide ||
            normalized.whiteKing != (swap ? 79 : 0) ||
            normalized.blackKing != (swap ? 0 : 79) ||
            normalized.ghost != 35 || normalized.visible)
            throw std::runtime_error(
              "lower Ghost same/opposing color normalization failed");
    }
    std::cout << "ghost_extra_lower_color_normalization same 1 opposing 1"
              << " residual 0\n";
}

[[nodiscard]] std::uint64_t peak_rss_bytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        return 0;
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
}

[[nodiscard]] std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes,
                                     std::size_t offset) {
    if (offset + 4 > bytes.size())
        throw std::runtime_error("truncated tablebase header");
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

class PackedFourTable {
  public:
    PackedFourTable(const std::string& path, const MaterialSpec& material) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open four-model table: " + path);
        bytes_ = std::vector<std::uint8_t>(
          std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (bytes_.size() < 48 || std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
            read_u32(bytes_, 8) < 5 ||
            read_u32(bytes_, 12) != static_cast<std::uint32_t>(PieceType::Bishop) ||
            read_u32(bytes_, 16) != StateCount ||
            read_u32(bytes_, 24) != GhostSubstates ||
            // Header word 28 is the planner's reflection-budget count, while
            // the dense codec intentionally retains both side-to-move halves.
            read_u32(bytes_, 28) != PlacementCount / 2 ||
            read_u32(bytes_, 32) != StateCount ||
            read_u32(bytes_, 40) != static_cast<std::uint32_t>(PieceType::Ghost) ||
            read_u32(bytes_, 44) != static_cast<std::uint32_t>(material.ghostColor))
            throw std::runtime_error(
              "four-model table header does not match requested Bishop/Ghost material");
        planeOffset_ = 48;
        const std::uint64_t wdlBytes = (std::uint64_t(StateCount) + 3) / 4;
        if (planeOffset_ + wdlBytes > bytes_.size())
            throw std::runtime_error("truncated four-model WDL plane");
    }

    [[nodiscard]] std::uint8_t result(std::uint32_t index) const {
        return (bytes_.at(planeOffset_ + index / 4) >> (2 * (index % 4))) & 3;
    }

  private:
    std::vector<std::uint8_t> bytes_;
    std::size_t planeOffset_ = 0;
};

[[nodiscard]] FourState decode_lower_ghost(std::uint32_t index) {
    if (index >= LowerGhostStateCount)
        throw std::runtime_error("lower Ghost state index is out of range");
    FourState result;
    result.visible = index % GhostSubstates != 0;
    std::uint32_t placement = index / GhostSubstates;
    const std::uint32_t ghostRank = placement % (Squares - 2);
    placement /= Squares - 2;
    const std::uint32_t blackRank = placement % (Squares - 1);
    placement /= Squares - 1;
    result.whiteKing = static_cast<std::uint8_t>(placement % Squares);
    result.side = static_cast<Color>(placement / Squares);
    result.blackKing = static_cast<std::uint8_t>(blackRank +
      (blackRank >= result.whiteKing ? 1u : 0u));
    const std::uint32_t low = std::min(result.whiteKing, result.blackKing);
    const std::uint32_t high = std::max(result.whiteKing, result.blackKing);
    std::uint32_t ghost = ghostRank;
    if (ghost >= low)
        ++ghost;
    if (ghost >= high)
        ++ghost;
    result.ghost = static_cast<std::uint8_t>(ghost);
    return result;
}

void ghost_mask_set(GhostInformationMask& mask, unsigned square) {
    if (square < 64)
        mask.low |= std::uint64_t(1) << square;
    else
        mask.high |= std::uint16_t(1) << (square - 64);
}

[[nodiscard]] bool ghost_mask_test(const GhostInformationMask& mask,
                                   unsigned square) {
    return square < 64 ? (mask.low >> square) & 1u
                       : (mask.high >> (square - 64)) & 1u;
}

[[nodiscard]] std::uint32_t ghost_mask_count(
  const GhostInformationMask& mask) {
    return static_cast<std::uint32_t>(__builtin_popcountll(mask.low) +
                                     __builtin_popcount(mask.high));
}

[[nodiscard]] std::string complete_transition_observation(
  const Position& before, const Move& move, const Position& child,
  const DisclosureContext& observer) {
    std::string observation = transition_observation_key(
      before, move, child, observer);
    if (!child.game_over() && child.side_to_move() == observer.observer) {
        const std::string decision = decision_observation_key(child, observer);
        observation += "|nextDecision=" + std::to_string(decision.size()) +
                       ':' + decision;
    }
    return observation;
}

struct LowerObservationBucket {
    GhostInformationMask childMask;
    GhostInformationProbeResult result;
    std::uint32_t sourceWorlds = 0;
    bool observerActionCommon = true;
};

[[nodiscard]] LowerObservationBucket build_lower_observation_bucket(
  std::uint32_t parentIndex, const Move& actualMove,
  const std::string& actualObservation, const MaterialSpec& material,
  const GhostInformationProbe& lower) {
    const FourState actualParent = decode_index(parentIndex);
    const DisclosureContext observer{material.observer(), false};
    const Position actualPosition = make_position(parentIndex, material);
    const std::string action = actualPosition.move_to_string(actualMove);
    const bool observerMoves = actualParent.side == material.observer();
    const std::string decision = observerMoves
      ? decision_observation_key(actualPosition, observer) : std::string();

    Position actualChild = actualPosition;
    Undo actualUndo;
    if (!actualChild.make_move(actualMove, actualUndo))
        throw std::runtime_error(
          "lower observation-bucket actual move failed");
    const ClassifiedChild actualClass = classify_child(actualChild, material);
    if (actualClass.domain != ChildDomain::LowerGhost)
        throw std::runtime_error(
          "lower observation-bucket actual child changed domain");
    const FourState actualLower = decode_lower_ghost(actualClass.index);

    LowerObservationBucket bucket;
    for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
        if (ghost == actualParent.whiteKing || ghost == actualParent.blackKing ||
            ghost == actualParent.bishop)
            continue;
        if (actualParent.visible && ghost != actualParent.ghost)
            continue;
        FourState candidate = actualParent;
        candidate.ghost = ghost;
        const std::uint32_t candidateIndex = encode_index(candidate);
        Position source = make_position(candidateIndex, material);
        if (source.game_over())
            continue;
        if (observerMoves &&
            decision_observation_key(source, observer) != decision)
            continue;
        ++bucket.sourceWorlds;

        bool foundAction = false;
        for (const Move& move : source.legal_moves()) {
            if (source.move_to_string(move) != action)
                continue;
            if (foundAction)
                throw std::runtime_error(
                  "one public action names multiple concrete moves");
            foundAction = true;
            Position child = source;
            Undo undo;
            if (!child.make_move(move, undo))
                throw std::runtime_error(
                  "lower observation-bucket legal move failed");
            if (complete_transition_observation(
                  source, move, child, observer) != actualObservation)
                continue;
            const ClassifiedChild classified = classify_child(child, material);
            if (classified.domain != ChildDomain::LowerGhost)
                throw std::runtime_error(
                  "one public observation mixes lower material domains");
            const FourState normalized = decode_lower_ghost(classified.index);
            if (normalized.side != actualLower.side ||
                normalized.whiteKing != actualLower.whiteKing ||
                normalized.blackKing != actualLower.blackKing ||
                normalized.visible != actualLower.visible)
                throw std::runtime_error(
                  "one lower observation mixes public child geometry");
            ghost_mask_set(bucket.childMask, normalized.ghost);
        }
        if (observerMoves && !foundAction)
            bucket.observerActionCommon = false;
    }
    if (!ghost_mask_test(bucket.childMask, actualLower.ghost))
        throw std::runtime_error(
          "lower observation-bucket image omitted its actual world");
    bucket.result = lower.probe(
      static_cast<std::uint8_t>(actualLower.side), actualLower.whiteKing,
      actualLower.blackKing, actualLower.ghost, actualLower.visible,
      bucket.childMask);
    return bucket;
}

void codec_self_test(const MaterialSpec& material) {
    const auto started = std::chrono::steady_clock::now();
    for (std::uint32_t placement = 0; placement < PlacementCount; ++placement) {
        FourState state = decode_placement(placement);
        if (encode_placement(state) != placement)
            throw std::runtime_error("four-model codec is not bijective");
        state.visible = true;
        if (encode_index(state) != placement * 2 + 1)
            throw std::runtime_error("Ghost substate codec is not bijective");
    }
    FourState sample{Color::Black, 17, 62, 28, 43, true};
    FourState reflected = sample;
    reflected.whiteKing = horizontal_reflection(reflected.whiteKing);
    reflected.blackKing = horizontal_reflection(reflected.blackKing);
    reflected.bishop = horizontal_reflection(reflected.bishop);
    reflected.ghost = horizontal_reflection(reflected.ghost);
    if (encode_index(sample) != encode_index(reflected))
        throw std::runtime_error("horizontal codec orbit mismatch");

    // Vertical reflection is the remaining exact rectangle quotient after the
    // file reflection already embedded in the dense codec.
    for (std::uint32_t sampleIndex = 0; sampleIndex < 100'000; ++sampleIndex) {
        const std::uint32_t index = static_cast<std::uint32_t>(
          std::uint64_t(StateCount) * sampleIndex / 100'000);
        FourState state = decode_index(index);
        FourState vertical = state;
        vertical.whiteKing = vertical_reflection(vertical.whiteKing);
        vertical.blackKing = vertical_reflection(vertical.blackKing);
        vertical.bishop = vertical_reflection(vertical.bishop);
        vertical.ghost = vertical_reflection(vertical.ghost);
        Position direct = make_position(index, material);
        Position transformed = make_position(encode_index(vertical), material);
        if (direct.game_over() != transformed.game_over() ||
            direct.legal_moves().size() != transformed.legal_moves().size())
            throw std::runtime_error("vertical rectangle quotient is not equivariant");
    }
    const double elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();
    std::cout << "ghost_extra_codec exhaustive_placements " << PlacementCount
              << " states " << StateCount << " vertical_samples 100000"
              << " elapsed " << elapsed << "s\n" << std::flush;
}

// Exhaust all beliefs of a three-square toy domain.  The public Bishop square
// is fixed; a quiet Ghost observation maps one source to two possible hidden
// destinations, a reveal maps every source to one singleton, and a private
// legal-dot partition splits {0,1} from {2}.  This guards the extra public
// geometry dimension without depending on an 8x10 sample.
void tiny_public_geometry_self_test() {
    constexpr unsigned Full = 0b111;
    const auto hidden_image = [](unsigned belief) {
        unsigned child = 0;
        if (belief & 0b011) child |= 0b001;
        if (belief & 0b101) child |= 0b010;
        return child;
    };
    const auto reveal_image = [](unsigned belief) {
        return belief ? 0b100u : 0u;
    };
    std::array<std::uint8_t, 8> black{};
    std::array<std::array<std::uint8_t, 8>, 3> white{};
    unsigned iterations = 0;
    for (;;) {
        ++iterations;
        auto nextBlack = black;
        auto nextWhite = white;
        for (unsigned belief = 0; belief <= Full; ++belief) {
            const bool sameDots = (belief & 0b100) == 0 ||
                                  (belief & 0b011) == 0;
            nextBlack[belief] = (belief & ~0b001u) == 0 ||
              (sameDots && black[hidden_image(belief)]);
            for (unsigned actual = 0; actual < 3; ++actual) {
                if (!((belief >> actual) & 1u)) {
                    nextWhite[actual][belief] = 0;
                    continue;
                }
                nextWhite[actual][belief] = !sameDots || actual == 2 ||
                  white[2][reveal_image(belief)];
            }
        }
        if (nextBlack == black && nextWhite == white)
            break;
        black = nextBlack;
        white = nextWhite;
    }
    if (iterations < 2 || hidden_image(1) != 3 || reveal_image(3) != 4 ||
        !white[0][0b101])
        throw std::runtime_error("tiny Bishop/Ghost powerset regression failed");
    for (unsigned first = 0; first <= Full; ++first)
        for (unsigned second = 0; second <= Full; ++second) {
            if ((first & ~second) == 0 && black[second] && !black[first])
                throw std::runtime_error("tiny BlackForce is not downward closed");
            for (unsigned actual = 0; actual < 3; ++actual)
                if ((first & ~second) == 0 &&
                    ((first >> actual) & 1u) && white[actual][first] &&
                    !white[actual][second])
                    throw std::runtime_error("tiny WhiteForce is not upward closed");
        }
    std::cout << "ghost_extra_tiny_powerset beliefs 8 iterations " << iterations
              << " residual 0\n";
}

struct PreflightCounts {
    std::uint64_t sampledStates = 0;
    std::uint64_t terminalStates = 0;
    std::uint64_t edges = 0;
    std::uint64_t sameClass = 0;
    std::uint64_t lowerGhost = 0;
    std::uint64_t lowerGhostOwnerForce = 0;
    std::uint64_t lowerGhostObserverForce = 0;
    std::uint64_t lowerGhostDraw = 0;
    std::uint64_t lowerBucketProbes = 0;
    std::uint64_t lowerBucketMemberships = 0;
    std::uint64_t lowerBucketNonSingleton = 0;
    std::uint64_t lowerBucketNonCommonObserverActions = 0;
    std::uint32_t lowerBucketMax = 0;
    std::uint64_t insufficientBishop = 0;
    std::uint64_t exactTerminal = 0;
    std::uint64_t invalid = 0;
    std::uint64_t decisionBytes = 0;
    std::uint64_t observationBytes = 0;
    std::uint32_t maxEdges = 0;
};

void run_preflight(const MaterialSpec& material, const PackedFourTable& concrete,
                   const GhostInformationProbe& lower,
                   std::uint32_t samples, bool exhaustiveOracle) {
    const DisclosureContext observer{material.observer(), false};
    PreflightCounts counts;
    std::unordered_map<std::string, LowerObservationBucket> lowerBuckets;
    std::uint32_t random = 0x9e3779b9u;
    for (std::uint32_t sample = 0; sample < samples; ++sample) {
        random = random * 1664525u + 1013904223u;
        std::uint32_t index = exhaustiveOracle ? sample
          : static_cast<std::uint32_t>(
              (std::uint64_t(random) * StateCount) >> 32);
        if (!exhaustiveOracle && sample < 4)
            index = sample == 0 ? 0 : sample == 1 ? StateCount / 2
                  : sample == 2 ? StateCount - 2 : StateCount - 1;
        Position position = make_position(index, material);
        if (const auto roundTrip = same_class_index(position, material);
            !roundTrip || *roundTrip != index)
            throw std::runtime_error("sampled four-model Position round trip failed");
        ++counts.sampledStates;
        (void)concrete.result(index);
        if (position.game_over()) {
            ++counts.terminalStates;
            continue;
        }
        if (position.side_to_move() == material.observer())
            counts.decisionBytes +=
              decision_observation_key(position, observer).size();
        const std::vector<Move> moves = position.legal_moves();
        counts.maxEdges = std::max<std::uint32_t>(counts.maxEdges, moves.size());
        counts.edges += moves.size();
        for (const Move& move : moves) {
            Position child = position;
            Undo undo;
            if (!child.make_move(move, undo))
                throw std::runtime_error("sampled legal edge failed make_move");
            const std::string observation = complete_transition_observation(
              position, move, child, observer);
            counts.observationBytes += observation.size();
            const ClassifiedChild classified = classify_child(child, material);
            switch (classified.domain) {
            case ChildDomain::SameClass:
                ++counts.sameClass;
                if (classified.index >= StateCount)
                    throw std::runtime_error("same-class child index is out of range");
                break;
            case ChildDomain::LowerGhost:
                ++counts.lowerGhost;
                if (classified.index >= LowerGhostStateCount)
                    throw std::runtime_error(
                      "lower Ghost child index is out of range");
                {
                    const FourState parent = decode_index(index);
                    const std::string action = position.move_to_string(move);
                    const std::string currentDecision =
                      position.side_to_move() == material.observer()
                      ? decision_observation_key(position, observer)
                      : std::string();
                    std::ostringstream key;
                    key << int(parent.side) << ',' << int(parent.whiteKing)
                        << ',' << int(parent.blackKing) << ','
                        << int(parent.bishop) << ',' << int(parent.visible);
                    if (parent.visible)
                        key << ',' << int(parent.ghost);
                    key << '|' << currentDecision.size() << ':' << currentDecision
                        << '|' << action.size() << ':' << action
                        << '|' << observation.size() << ':' << observation;
                    auto found = lowerBuckets.find(key.str());
                    if (found == lowerBuckets.end()) {
                        LowerObservationBucket bucket =
                          build_lower_observation_bucket(
                            index, move, observation, material, lower);
                        const std::uint32_t size =
                          ghost_mask_count(bucket.childMask);
                        ++counts.lowerBucketProbes;
                        counts.lowerBucketMemberships += size;
                        counts.lowerBucketNonSingleton += size > 1;
                        counts.lowerBucketNonCommonObserverActions +=
                          !bucket.observerActionCommon;
                        counts.lowerBucketMax = std::max(
                          counts.lowerBucketMax, size);
                        found = lowerBuckets.emplace(
                          key.str(), std::move(bucket)).first;
                    }
                    const GhostInformationProbeResult& result =
                      found->second.result;
                    if (result.ownerForce)
                        ++counts.lowerGhostOwnerForce;
                    else if (result.observerForce)
                        ++counts.lowerGhostObserverForce;
                    else
                        ++counts.lowerGhostDraw;
                }
                break;
            case ChildDomain::InsufficientBishop:
                ++counts.insufficientBishop;
                break;
            case ChildDomain::ExactTerminal: ++counts.exactTerminal; break;
            case ChildDomain::Invalid: ++counts.invalid; break;
            }
        }
        if ((sample + 1) % 100'000 == 0)
            std::cout << "ghost_extra_preflight sampled " << sample + 1 << '/'
                      << samples << " edges " << counts.edges
                      << " peak_rss_bytes " << peak_rss_bytes() << '\n'
                      << std::flush;
    }
    if (counts.invalid)
        throw std::runtime_error("sampled edge escaped all exact lower domains");
    if (samples >= 5'000 &&
        (!counts.lowerGhost || !counts.insufficientBishop))
        throw std::runtime_error(
          "sampled preflight did not exercise both capture-lower domains");

    const long double edgeRatio = counts.sampledStates
      ? static_cast<long double>(counts.edges) / counts.sampledStates : 0;
    const std::uint64_t estimatedEdges = static_cast<std::uint64_t>(
      edgeRatio * StateCount + 0.5L);
    const std::uint64_t csrBytes =
      (std::uint64_t(StateCount) + 1) * sizeof(std::uint64_t) +
      estimatedEdges * 16 + std::uint64_t(StateCount) * 5;
    constexpr std::uint64_t HiddenGeometriesAfterVertical =
      std::uint64_t(2) * (Squares / 2) * (Squares - 1) * (Squares - 2) / 2;
    constexpr std::uint64_t HiddenMembershipFunctions =
      HiddenGeometriesAfterVertical * (Squares - 3);
    constexpr std::uint64_t VisibleSingletonsAfterVertical = StateCount / 4;
    const std::uint64_t minimumSymbolicRoots =
      HiddenMembershipFunctions * sizeof(std::uint32_t) +
      HiddenGeometriesAfterVertical * sizeof(std::uint32_t) +
      VisibleSingletonsAfterVertical * 2;
    // Conservative preflight against the now-certified KGhost symbolic run.
    // The extra public Bishop multiplies hidden owner-root functions by 77.
    // Linear extrapolation is not a proof of final size, but it is sufficient
    // to gate a likely unsafe launch: KGhost peaked at 2,532,687,872 bytes and
    // ended with 5,477,899 permanent nodes for 246,480 hidden owner roots.
    constexpr std::uint64_t LowerHiddenOwnerFunctions = 246'480;
    constexpr std::uint64_t LowerBddNodes = 5'477'899;
    constexpr std::uint64_t LowerPeakRssBytes = 2'532'687'872;
    const long double symbolicScale = static_cast<long double>(
      HiddenMembershipFunctions) / LowerHiddenOwnerFunctions;
    const std::uint64_t linearBddNodes = static_cast<std::uint64_t>(
      symbolicScale * LowerBddNodes + 0.5L);
    const std::uint64_t linearPeakRss = static_cast<std::uint64_t>(
      symbolicScale * LowerPeakRssBytes + 0.5L);
    constexpr std::uint64_t Budget97GiB = 97ULL << 30;
    // Exact external-memory plan.  Permanent ROBDD nodes use the certified
    // 9-byte UFGM1 tuple.  A collision-checked open-address unique table is
    // budgeted at a deliberately conservative 24 bytes per node.  Bellman
    // transitions are D2-quotiented 16-byte disk records and streamed one
    // public source geometry at a time.  Apply/compose caches are capped and
    // discarded after each geometry; eviction affects speed, never results.
    // Between iterations, mark/copy compaction retains only nodes reachable
    // from mmap root arrays and verifies all remapped roots before deleting
    // the old arena.
    const std::uint64_t externalNodeBytes = linearBddNodes * 9;
    const std::uint64_t externalHashBytes = linearBddNodes * 24;
    const std::uint64_t externalRemapBytes = linearBddNodes * 4;
    const std::uint64_t quotientEdges = (estimatedEdges + 1) / 2;
    const std::uint64_t externalTransitionBytes = quotientEdges * 16;
    constexpr std::uint64_t ApplyCacheBudget = 8ULL << 30;
    constexpr std::uint64_t BlockBufferBudget = 1ULL << 30;
    const std::uint64_t externalPeakRss =
      externalNodeBytes * 2 + externalHashBytes + externalRemapBytes +
      ApplyCacheBudget + BlockBufferBudget + minimumSymbolicRoots * 2;
    const std::uint64_t externalScratchBytes =
      externalNodeBytes * 2 + externalHashBytes + externalRemapBytes +
      externalTransitionBytes + minimumSymbolicRoots * 2 +
      (StateCount + 3) / 4;
    std::cout << "ghost_extra_preflight_complete material " << material.name()
              << " sampled_states " << counts.sampledStates
              << " terminal " << counts.terminalStates
              << " sampled_edges " << counts.edges
              << " average_edges " << std::fixed << std::setprecision(6)
              << static_cast<double>(edgeRatio)
              << " max_edges " << counts.maxEdges
              << " same_class " << counts.sameClass
              << " lower_ghost " << counts.lowerGhost
              << " lower_ghost_owner_force "
              << counts.lowerGhostOwnerForce
              << " lower_ghost_observer_force "
              << counts.lowerGhostObserverForce
              << " lower_ghost_draw " << counts.lowerGhostDraw
              << " lower_bucket_probes " << counts.lowerBucketProbes
              << " lower_bucket_memberships "
              << counts.lowerBucketMemberships
              << " lower_bucket_non_singleton "
              << counts.lowerBucketNonSingleton
              << " lower_bucket_max " << counts.lowerBucketMax
              << " lower_bucket_non_common_observer_actions "
              << counts.lowerBucketNonCommonObserverActions
              << " insufficient_bishop " << counts.insufficientBishop
              << " exact_terminal " << counts.exactTerminal
              << " invalid " << counts.invalid
              << " estimated_full_edges " << estimatedEdges
              << " estimated_csr_bytes " << csrBytes
              << " oracle_exhaustive " << exhaustiveOracle
              << " hidden_geometries_after_vertical "
              << HiddenGeometriesAfterVertical
              << " hidden_owner_functions " << HiddenMembershipFunctions
              << " visible_singletons_after_vertical "
              << VisibleSingletonsAfterVertical
              << " minimum_force_root_bytes " << minimumSymbolicRoots
              << " linear_lower_bdd_nodes " << linearBddNodes
              << " linear_lower_peak_rss_bytes " << linearPeakRss
              << " likely_exceeds_97gib " << (linearPeakRss > Budget97GiB)
              << " external_node_bytes " << externalNodeBytes
              << " external_hash_bytes " << externalHashBytes
              << " external_transition_bytes " << externalTransitionBytes
              << " external_peak_rss_bytes " << externalPeakRss
              << " external_scratch_bytes " << externalScratchBytes
              << " external_plan_under_97gib "
              << (externalPeakRss < Budget97GiB &&
                  externalScratchBytes < Budget97GiB)
              << " lower_arbitrary_mask_probe certified"
              << " final_solve_launched 0\n" << std::flush;
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    using namespace Stockfish::Ultimate;
    try {
        MaterialSpec material;
        std::string input = "tablebases/kbishopghostk.uftb";
        std::string lowerSidecar = "tablebases/kghostk.ufgm";
        std::string lowerSourceSha256 =
          "3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31";
        std::string lowerModelSha256 =
          "4a2d9d7b503b29204cf9af08985345771b9046c07bd2116e592fab40ee12e430";
        std::string lowerObservationSha256 =
          "af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf";
        std::uint32_t samples = 200'000;
        bool selfTestOnly = false;
        bool exhaustiveOracle = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            const auto value = [&](const char* option) {
                if (i + 1 >= argc)
                    throw std::runtime_error(std::string("missing value for ") + option);
                return std::string(argv[++i]);
            };
            if (argument == "--material") {
                const std::string selected = value("--material");
                if (selected == "same")
                    material.ghostColor = Color::White;
                else if (selected == "opposing")
                    material.ghostColor = Color::Black;
                else
                    throw std::runtime_error("material must be same or opposing");
            }
            else if (argument == "--input") input = value("--input");
            else if (argument == "--lower-ghost-sidecar")
                lowerSidecar = value("--lower-ghost-sidecar");
            else if (argument == "--lower-information-source-sha256")
                lowerSourceSha256 = value("--lower-information-source-sha256");
            else if (argument == "--lower-information-model-sha256")
                lowerModelSha256 = value("--lower-information-model-sha256");
            else if (argument == "--lower-information-observation-sha256")
                lowerObservationSha256 =
                  value("--lower-information-observation-sha256");
            else if (argument == "--samples")
                samples = static_cast<std::uint32_t>(std::stoul(value("--samples")));
            else if (argument == "--self-test") selfTestOnly = true;
            else if (argument == "--exhaustive-oracle")
                exhaustiveOracle = true;
            else throw std::runtime_error("unknown argument: " + argument);
        }
        codec_self_test(material);
        tiny_public_geometry_self_test();
        lower_color_normalization_self_test();
        if (selfTestOnly)
            return 0;
        if (!samples)
            throw std::runtime_error("preflight sample count must be positive");
        if (exhaustiveOracle)
            samples = StateCount;
        const PackedFourTable concrete(input, material);
        const GhostInformationProbe lower(
          lowerSidecar, lowerSourceSha256, lowerModelSha256,
          lowerObservationSha256);
        run_preflight(material, concrete, lower, samples, exhaustiveOracle);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "ghost-extra-information-tablebase: " << error.what() << '\n';
        return 1;
    }
}
