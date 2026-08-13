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
#include "external_robdd.h"
#include "ghost_information_probe.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t Squares = Position::BoardSquares;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
constexpr std::uint32_t PlacementCount =
  2 * Squares * (Squares - 1) * (Squares - 2) * (Squares - 3);
constexpr bool ExtraIsCopycat = true;
#else
constexpr std::uint32_t PlacementCount =
  2 * (Squares / 2) * (Squares - 1) * (Squares - 2) * (Squares - 3);
constexpr bool ExtraIsCopycat = false;
#endif
constexpr std::uint32_t GhostSubstates = 2;
#ifdef ULTIMATE_GHOST_EXTRA_SUBSTATES
constexpr std::uint32_t ExtraSubstates = ULTIMATE_GHOST_EXTRA_SUBSTATES;
#else
constexpr std::uint32_t ExtraSubstates = 1;
#endif
constexpr std::uint32_t StateCount =
  PlacementCount * ExtraSubstates * GhostSubstates;
#ifdef ULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY
constexpr std::uint8_t GeometryTransformCount = 2;
#else
constexpr std::uint8_t GeometryTransformCount = 4;
#endif
#ifdef ULTIMATE_GHOST_EXTRA_IS_CHECKER
constexpr bool ExtraIsChecker = true;
#else
constexpr bool ExtraIsChecker = false;
#endif
constexpr std::uint32_t LowerGhostStateCount =
  2 * Squares * (Squares - 1) * (Squares - 2) * GhostSubstates;
constexpr std::uint32_t NoIndex = std::numeric_limits<std::uint32_t>::max();

struct FourState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t bishop = 0;
    std::uint8_t ghost = 0;
    std::uint8_t extraSubstate = 0;
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
#ifndef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.bishop = horizontal_reflection(state.bishop);
        state.ghost = horizontal_reflection(state.ghost);
    }
#endif
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
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
    const std::uint32_t whiteRank = state.whiteKing;
    constexpr std::uint32_t WhiteKingCount = Squares;
#else
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
      (Position::BoardFiles / 2) + state.whiteKing % Position::BoardFiles;
    constexpr std::uint32_t WhiteKingCount = Squares / 2;
#endif
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t bishopRank = rank_excluding(
      state.bishop, {state.whiteKing, state.blackKing});
    const std::uint32_t ghostRank = rank_excluding(
      state.ghost, {state.whiteKing, state.blackKing, state.bishop});
    return ((((static_cast<std::uint32_t>(state.side) * WhiteKingCount + whiteRank)
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
    const std::uint8_t blackKing = unrank_excluding(blackRank, {whiteKing});
    const std::uint8_t bishop = unrank_excluding(
      bishopRank, {whiteKing, blackKing});
    const std::uint8_t ghost = unrank_excluding(
      ghostRank, {whiteKing, blackKing, bishop});
    return {side, whiteKing, blackKing, bishop, ghost, 0, false};
}

[[nodiscard]] std::uint32_t encode_index(const FourState& state) {
    if (state.extraSubstate >= ExtraSubstates)
        throw std::runtime_error("four-model extra substate is out of range");
    return (encode_placement(state) * ExtraSubstates + state.extraSubstate) *
             GhostSubstates + (state.visible ? 1u : 0u);
}

[[nodiscard]] FourState decode_index(std::uint32_t index) {
    if (index >= StateCount)
        throw std::runtime_error("four-model state index is out of range");
    const bool visible = index % GhostSubstates != 0;
    index /= GhostSubstates;
    FourState state = decode_placement(index / ExtraSubstates);
    state.extraSubstate = static_cast<std::uint8_t>(index % ExtraSubstates);
    state.visible = visible;
    return state;
}

[[nodiscard]] bool valid_world(const FourState& state) {
    if constexpr (ExtraIsCopycat) {
        const std::uint8_t clone = horizontal_reflection(state.bishop);
        if (clone == state.whiteKing || clone == state.blackKing ||
            clone == state.ghost)
            return false;
    }
    return GhostPublicExtra::tablebase_substate_geometrically_valid(
      PieceType::Bishop, state.whiteKing, state.blackKing, state.bishop,
      state.ghost, state.extraSubstate);
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
    const PieceType representedExtra =
      ExtraIsChecker && (state.extraSubstate & 2u)
        ? PieceType::CheckerKing : PieceType::Bishop;
    const int bishop = position.add_piece(
      representedExtra, Color::White, state.bishop);
    const int ghost = position.add_piece(
      PieceType::Ghost, material.ghostColor, state.ghost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        bishop == Position::NoPiece || ghost == Position::NoPiece)
        throw std::runtime_error("four-model codec produced invalid geometry");
    for (const int id : {whiteKing, blackKing, bishop, ghost})
        position.piece(id).moved = true;
    if constexpr (ExtraIsCopycat) {
        const int clone = position.piece(bishop).link;
        if (clone == Position::NoPiece)
            throw std::runtime_error("four-model Copycat clone is missing");
        position.piece(clone).moved = true;
    }
    position.piece(ghost).visible = state.visible;
    if (!position.apply_tablebase_substate(
          bishop, PieceType::Bishop, state.extraSubstate))
        throw std::runtime_error("four-model codec rejected extra substate");
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
        if constexpr (ExtraIsCopycat)
            if (piece.type == PieceType::CopycatClone)
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
        else if ((piece.type == PieceType::Bishop ||
                  (ExtraIsChecker && piece.type == PieceType::CheckerKing)) &&
                 piece.color == Color::White &&
                 !foundBishop) {
            state.bishop = static_cast<std::uint8_t>(piece.square);
            const auto substate = position.tablebase_substate(
              id, PieceType::Bishop);
            if (!substate || *substate >= ExtraSubstates)
                return std::nullopt;
            state.extraSubstate = static_cast<std::uint8_t>(*substate);
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
        if constexpr (ExtraIsCopycat)
            if (piece.type == PieceType::CopycatClone)
                continue;
        ++live;
        if (piece.type == PieceType::King && piece.color == Color::White)
            whiteKing = piece.square;
        else if (piece.type == PieceType::King && piece.color == Color::Black)
            blackKing = piece.square;
        else if ((piece.type == PieceType::Bishop ||
                  (ExtraIsChecker &&
                   piece.type == PieceType::CheckerKing)) &&
                 piece.color == Color::White)
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

class Sha256 {
  public:
    void update(const std::uint8_t* data, std::size_t size) {
        totalBytes_ += size;
        while (size) {
            const std::size_t take = std::min(size, block_.size() - blockSize_);
            std::memcpy(block_.data() + blockSize_, data, take);
            blockSize_ += take;
            data += take;
            size -= take;
            if (blockSize_ == block_.size()) {
                compress(block_.data());
                blockSize_ = 0;
            }
        }
    }
    [[nodiscard]] std::array<std::uint8_t, 32> finish() {
        const std::uint64_t bitLength = totalBytes_ * 8;
        block_[blockSize_++] = 0x80;
        if (blockSize_ > 56) {
            std::fill(block_.begin() + blockSize_, block_.end(), 0);
            compress(block_.data());
            blockSize_ = 0;
        }
        std::fill(block_.begin() + blockSize_, block_.begin() + 56, 0);
        for (int byte = 0; byte < 8; ++byte)
            block_[63 - byte] = static_cast<std::uint8_t>(
              bitLength >> (8 * byte));
        compress(block_.data());
        std::array<std::uint8_t, 32> digest{};
        for (std::size_t word = 0; word < state_.size(); ++word)
            for (int byte = 0; byte < 4; ++byte)
                digest[4 * word + byte] = static_cast<std::uint8_t>(
                  state_[word] >> (24 - 8 * byte));
        return digest;
    }

  private:
    static constexpr std::array<std::uint32_t, 64> K{{
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2}};
    static std::uint32_t rotate(std::uint32_t value, int count) {
        return (value >> count) | (value << (32 - count));
    }
    void compress(const std::uint8_t* block) {
        std::array<std::uint32_t, 64> words{};
        for (int index = 0; index < 16; ++index)
            words[index] = (std::uint32_t(block[4 * index]) << 24) |
              (std::uint32_t(block[4 * index + 1]) << 16) |
              (std::uint32_t(block[4 * index + 2]) << 8) |
              std::uint32_t(block[4 * index + 3]);
        for (int index = 16; index < 64; ++index) {
            const std::uint32_t first = rotate(words[index - 15], 7) ^
              rotate(words[index - 15], 18) ^ (words[index - 15] >> 3);
            const std::uint32_t second = rotate(words[index - 2], 17) ^
              rotate(words[index - 2], 19) ^ (words[index - 2] >> 10);
            words[index] = words[index - 16] + first + words[index - 7] + second;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int index = 0; index < 64; ++index) {
            const std::uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t first = h + s1 + choose + K[index] + words[index];
            const std::uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t second = s0 + majority;
            h = g; g = f; f = e; e = d + first;
            d = c; c = b; b = a; a = first + second;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }
    std::array<std::uint32_t, 8> state_{{
      0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<std::uint8_t, 64> block_{};
    std::size_t blockSize_ = 0;
    std::uint64_t totalBytes_ = 0;
};

[[nodiscard]] std::string hex_digest(
  const std::array<std::uint8_t, 32>& digest) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest)
        output << std::setw(2) << unsigned(byte);
    return output.str();
}

[[nodiscard]] bool valid_sha256(const std::string& text) {
    return text.size() == 64 &&
      std::all_of(text.begin(), text.end(), [](char value) {
          return (value >= '0' && value <= '9') ||
                 (value >= 'a' && value <= 'f');
      });
}

[[nodiscard]] bool packed_four_header_matches(
  const std::vector<std::uint8_t>& bytes, const MaterialSpec& material) {
    return bytes.size() >= 48 &&
      !std::memcmp(bytes.data(), "UFTB1\0\0\0", 8) &&
      read_u32(bytes, 8) >= 5 &&
      read_u32(bytes, 12) == static_cast<std::uint32_t>(PieceType::Bishop) &&
      read_u32(bytes, 16) == StateCount &&
      read_u32(bytes, 24) == ExtraSubstates * GhostSubstates &&
      // Header word 28 is the planner's reflection-budget count, while the
      // dense codec intentionally retains both side-to-move halves.
      read_u32(bytes, 28) == PlacementCount / 2 &&
      read_u32(bytes, 32) == StateCount &&
      read_u32(bytes, 40) == static_cast<std::uint32_t>(PieceType::Ghost) &&
      read_u32(bytes, 44) == static_cast<std::uint32_t>(material.ghostColor);
}

class PackedFourTable {
  public:
    PackedFourTable(const std::string& path, const MaterialSpec& material) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open four-model table: " + path);
        bytes_ = std::vector<std::uint8_t>(
          std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (!packed_four_header_matches(bytes_, material))
            throw std::runtime_error(
              "four-model table header does not match requested Bishop/Ghost material");
        planeOffset_ = 48;
        const std::uint64_t wdlBytes = (std::uint64_t(StateCount) + 3) / 4;
        if (planeOffset_ + wdlBytes > bytes_.size())
            throw std::runtime_error("truncated four-model WDL plane");
        Sha256 hasher;
        hasher.update(bytes_.data(), bytes_.size());
        sha_ = hasher.finish();
    }

    [[nodiscard]] std::uint8_t result(std::uint32_t index) const {
        return (bytes_.at(planeOffset_ + index / 4) >> (2 * (index % 4))) & 3;
    }
    [[nodiscard]] const std::array<std::uint8_t, 32>& sha() const {
        return sha_;
    }

  private:
    std::vector<std::uint8_t> bytes_;
    std::size_t planeOffset_ = 0;
    std::array<std::uint8_t, 32> sha_{};
};

void packed_four_header_self_test() {
    std::vector<std::uint8_t> bytes(48, 0);
    std::memcpy(bytes.data(), "UFTB1\0\0\0", 8);
    const auto write32 = [&](std::size_t offset, std::uint32_t value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    };
    write32(8, 5);
    write32(12, static_cast<std::uint32_t>(PieceType::Bishop));
    write32(16, StateCount);
    write32(24, ExtraSubstates * GhostSubstates);
    write32(28, PlacementCount / 2);
    write32(32, StateCount);
    write32(40, static_cast<std::uint32_t>(PieceType::Ghost));
    MaterialSpec material;
    write32(44, static_cast<std::uint32_t>(material.ghostColor));
    if (!packed_four_header_matches(bytes, material))
        throw std::runtime_error("four-model header substate self-test failed");
    if constexpr (ExtraSubstates > 1) {
        write32(24, GhostSubstates);
        if (packed_four_header_matches(bytes, material))
            throw std::runtime_error(
              "four-model header accepted a Ghost-only substate count");
    }
    std::cout << "ghost_extra_header_contract extra_substates "
              << ExtraSubstates << " ghost_substates " << GhostSubstates
              << " combined " << ExtraSubstates * GhostSubstates
              << " residual 0\n" << std::flush;
}

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
        if constexpr (ExtraIsCopycat)
            if (ghost == horizontal_reflection(actualParent.bishop))
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
    Sha256 sha;
    const std::string abc = "abc";
    sha.update(reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size());
    if (hex_digest(sha.finish()) !=
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        throw std::runtime_error("Ghost-extra SHA-256 self-test failed");
    for (std::uint32_t placement = 0; placement < PlacementCount; ++placement) {
        FourState state = decode_placement(placement);
        if (encode_placement(state) != placement)
            throw std::runtime_error("four-model codec is not bijective");
        state.visible = true;
        if (encode_index(state) != placement * ExtraSubstates * 2 + 1)
            throw std::runtime_error("Ghost substate codec is not bijective");
    }
    FourState sample{Color::Black, 17, 62, 28, 43, true};
    FourState reflected = sample;
    reflected.whiteKing = horizontal_reflection(reflected.whiteKing);
    reflected.blackKing = horizontal_reflection(reflected.blackKing);
    reflected.bishop = horizontal_reflection(reflected.bishop);
    reflected.ghost = horizontal_reflection(reflected.ghost);
    if constexpr (!ExtraIsCopycat)
        if (encode_index(sample) != encode_index(reflected))
            throw std::runtime_error("horizontal codec orbit mismatch");

    // Vertical reflection is the remaining exact rectangle quotient after the
    // file reflection already embedded in the dense codec.
    for (std::uint32_t sampleIndex = 0; sampleIndex < 100'000; ++sampleIndex) {
        const std::uint32_t index = static_cast<std::uint32_t>(
          std::uint64_t(StateCount) * sampleIndex / 100'000);
        FourState state = decode_index(index);
        if (!valid_world(state))
            continue;
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

struct PublicExtraGeometry {
    std::uint8_t side = 0;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t bishop = 0;
    std::uint8_t visible = 0;
    std::uint8_t extraSubstate = 0;

    friend bool operator==(const PublicExtraGeometry& lhs,
                           const PublicExtraGeometry& rhs) {
        return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
               lhs.blackKing == rhs.blackKing && lhs.bishop == rhs.bishop &&
               lhs.visible == rhs.visible &&
               lhs.extraSubstate == rhs.extraSubstate;
    }
};

[[nodiscard]] bool valid_geometry_world(
  const PublicExtraGeometry& geometry, std::uint8_t ghost) {
    if (ghost == geometry.whiteKing || ghost == geometry.blackKing ||
        ghost == geometry.bishop)
        return false;
    if constexpr (ExtraIsCopycat) {
        const std::uint8_t clone = horizontal_reflection(geometry.bishop);
        if (clone == geometry.whiteKing || clone == geometry.blackKing ||
            clone == ghost)
            return false;
    }
    return GhostPublicExtra::tablebase_substate_geometrically_valid(
      PieceType::Bishop, geometry.whiteKing, geometry.blackKing,
      geometry.bishop, ghost, geometry.extraSubstate);
}

[[nodiscard]] std::uint8_t rectangle_transform_square(
  std::uint8_t square, std::uint8_t transform) {
    if (transform & 1)
        square = horizontal_reflection(square);
    if (transform & 2)
        square = vertical_reflection(square);
    return square;
}

[[nodiscard]] PublicExtraGeometry transform_geometry(
  PublicExtraGeometry geometry, std::uint8_t transform) {
    geometry.whiteKing = rectangle_transform_square(
      geometry.whiteKing, transform);
    geometry.blackKing = rectangle_transform_square(
      geometry.blackKing, transform);
    geometry.bishop = rectangle_transform_square(geometry.bishop, transform);
    return geometry;
}

[[nodiscard]] bool geometry_less(const PublicExtraGeometry& lhs,
                                 const PublicExtraGeometry& rhs) {
    return std::tie(lhs.side, lhs.whiteKing, lhs.blackKing, lhs.bishop,
                    lhs.visible, lhs.extraSubstate) <
           std::tie(rhs.side, rhs.whiteKing, rhs.blackKing, rhs.bishop,
                    rhs.visible, rhs.extraSubstate);
}

struct CanonicalExtraGeometry {
    PublicExtraGeometry geometry;
    std::uint8_t transform = 0;
};

[[nodiscard]] CanonicalExtraGeometry canonical_geometry(
  const PublicExtraGeometry& source) {
    CanonicalExtraGeometry result{source, 0};
    for (std::uint8_t transform = 1;
         transform < GeometryTransformCount; ++transform) {
        const PublicExtraGeometry candidate = transform_geometry(
          source, transform);
        if (geometry_less(candidate, result.geometry))
            result = {candidate, transform};
    }
    return result;
}

[[nodiscard]] std::uint32_t geometry_code(
  const PublicExtraGeometry& geometry) {
    return geometry.side | (std::uint32_t(geometry.whiteKing) << 1) |
           (std::uint32_t(geometry.blackKing) << 8) |
           (std::uint32_t(geometry.bishop) << 15) |
           (std::uint32_t(geometry.visible) << 22) |
           (std::uint32_t(geometry.extraSubstate) << 23);
}

class ExtraGeometryDomain {
  public:
    ExtraGeometryDomain() {
        constexpr std::uint32_t Expected =
          2 * Squares * (Squares - 1) * (Squares - 2) * 2 *
          ExtraSubstates / GeometryTransformCount;
        geometries_.reserve(Expected);
        byCode_.reserve(Expected * 2);
        for (std::uint8_t side = 0; side < 2; ++side)
            for (std::uint8_t whiteKing = 0; whiteKing < Squares; ++whiteKing)
                for (std::uint8_t blackKing = 0; blackKing < Squares;
                     ++blackKing) {
                    if (blackKing == whiteKing)
                        continue;
                    for (std::uint8_t bishop = 0; bishop < Squares; ++bishop) {
                        if (bishop == whiteKing || bishop == blackKing)
                            continue;
                        for (std::uint8_t visible = 0; visible < 2; ++visible)
                          for (std::uint8_t extraSubstate = 0;
                               extraSubstate < ExtraSubstates;
                               ++extraSubstate) {
                            const PublicExtraGeometry raw{
                              side, whiteKing, blackKing, bishop, visible,
                              extraSubstate};
                            const CanonicalExtraGeometry canonical =
                              canonical_geometry(raw);
                            if (!(canonical.geometry == raw))
                                continue;
                            const std::uint32_t id =
                              static_cast<std::uint32_t>(geometries_.size());
                            geometries_.push_back(raw);
                            if (!byCode_.emplace(geometry_code(raw), id).second)
                                throw std::runtime_error(
                                  "duplicate canonical extra public geometry");
                        }
                    }
                }
        if (geometries_.size() != Expected)
            throw std::runtime_error(
              "extra public-geometry quotient has the wrong size");
    }

    [[nodiscard]] std::size_t size() const { return geometries_.size(); }
    [[nodiscard]] const PublicExtraGeometry& operator[](
      std::uint32_t id) const { return geometries_.at(id); }
    [[nodiscard]] std::pair<std::uint32_t, std::uint8_t> locate(
      const PublicExtraGeometry& raw) const {
        const CanonicalExtraGeometry canonical = canonical_geometry(raw);
        const auto found = byCode_.find(geometry_code(canonical.geometry));
        if (found == byCode_.end() ||
            !(geometries_.at(found->second) == canonical.geometry))
            throw std::runtime_error(
              "canonical extra public geometry is not interned");
        return {found->second, canonical.transform};
    }

  private:
    std::vector<PublicExtraGeometry> geometries_;
    std::unordered_map<std::uint32_t, std::uint32_t> byCode_;
};

[[nodiscard]] Position make_geometry_position(
  const PublicExtraGeometry& geometry, std::uint8_t ghost,
  const MaterialSpec& material) {
    if (!valid_geometry_world(geometry, ghost)) {
        std::ostringstream message;
        message << "extra symbolic geometry overlaps its Ghost: wk="
                << unsigned(geometry.whiteKing) << " bk="
                << unsigned(geometry.blackKing) << " extra="
                << unsigned(geometry.bishop) << " ghost=" << unsigned(ghost);
        if constexpr (ExtraIsCopycat)
            message << " clone="
                    << unsigned(horizontal_reflection(geometry.bishop));
        throw std::runtime_error(message.str());
    }
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, geometry.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, geometry.blackKing);
    const PieceType representedExtra =
      ExtraIsChecker && (geometry.extraSubstate & 2u)
        ? PieceType::CheckerKing : PieceType::Bishop;
    const int bishop = position.add_piece(
      representedExtra, Color::White, geometry.bishop);
    const int ghostId = position.add_piece(
      PieceType::Ghost, material.ghostColor, ghost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        bishop == Position::NoPiece || ghostId == Position::NoPiece)
        throw std::runtime_error("cannot construct extra symbolic world");
    for (const int id : {whiteKing, blackKing, bishop, ghostId})
        position.piece(id).moved = true;
    if constexpr (ExtraIsCopycat) {
        const int clone = position.piece(bishop).link;
        if (clone == Position::NoPiece)
            throw std::runtime_error("symbolic Copycat clone is missing");
        position.piece(clone).moved = true;
    }
    position.piece(ghostId).visible = geometry.visible != 0;
    if (!position.apply_tablebase_substate(
          bishop, PieceType::Bishop, geometry.extraSubstate))
        throw std::runtime_error("cannot apply symbolic extra substate");
    position.set_side_to_move(static_cast<Color>(geometry.side));
    return position;
}

struct alignas(8) ExternalMask {
    std::uint64_t low = 0;
    std::uint16_t high = 0;
    std::uint16_t reserved16 = 0;
    std::uint32_t reserved32 = 0;
};
static_assert(sizeof(ExternalMask) == 16);

void external_mask_set(ExternalMask& mask, unsigned square) {
    if (square < 64)
        mask.low |= std::uint64_t(1) << square;
    else
        mask.high |= std::uint16_t(1) << (square - 64);
}

[[nodiscard]] bool external_mask_test(const ExternalMask& mask,
                                      unsigned square) {
    return square < 64 ? (mask.low >> square) & 1u
                       : (mask.high >> (square - 64)) & 1u;
}

[[nodiscard]] ExternalMask external_mask_and(
  const ExternalMask& lhs, const ExternalMask& rhs) {
    ExternalMask result;
    result.low = lhs.low & rhs.low;
    result.high = static_cast<std::uint16_t>(lhs.high & rhs.high);
    return result;
}

[[nodiscard]] ExternalMask external_singleton_mask(unsigned square) {
    ExternalMask result;
    external_mask_set(result, square);
    return result;
}

struct ExternalGeometryMeta {
    ExternalMask live;
    ExternalMask terminal;
    ExternalMask terminalOwner;
    ExternalMask terminalObserver;
    std::array<std::uint32_t, Squares> actualStratum{};
    std::uint32_t stratumBase = 0;
    std::uint32_t stratumCount = 0;
};
static_assert(sizeof(ExternalGeometryMeta) == 392);

enum class ExternalChildDomain : std::uint8_t {
  SameClass,
  LowerGhost,
  Exact,
};

struct ExternalCompiledEdge {
    std::uint32_t relation = 0;
    std::uint32_t action = 0;
    std::uint32_t child = NoIndex;
    std::uint8_t childActual = 0;
    ExternalChildDomain domain = ExternalChildDomain::Exact;
    std::uint8_t exact = 0;  // bit 0 Ghost owner; bit 1 observer
    std::uint8_t reserved = 0;
};
static_assert(sizeof(ExternalCompiledEdge) == 16);

struct ExternalTransitionHeader {
    std::array<char, 8> magic{{'U','F','G','X','1','\0','\0','\0'}};
    std::uint32_t version = 1;
    std::uint32_t material = 0;
    std::uint32_t geometryCount = 0;
    std::uint32_t completedGeometries = 0;
    std::uint64_t edges = 0;
    std::uint64_t strata = 0;
    std::uint64_t blockBytes = 0;
    std::uint64_t reserved = 0;
};
static_assert(sizeof(ExternalTransitionHeader) == 56);

// Read-only, collision-checked view of the certified K+Ghost-v-K UFGM1
// sidecar.  The public GhostInformationProbe intentionally exposes only a
// Boolean answer.  The external Bellman kernel instead needs the certified
// arbitrary-mask predicate itself so that a Bishop-capture observation can
// inherit the complete image belief rather than resetting it to a singleton.
// Keeping this reader local avoids changing the already SHA-bound base-Ghost
// source/model domain.
class LowerGhostSymbolicSidecar {
  public:
    struct Node {
        std::uint8_t variable = Squares;
        std::uint32_t low = 0;
        std::uint32_t high = 0;
    };
    struct Geometry {
        std::uint8_t side = 0;
        std::uint8_t ownerKing = 0;
        std::uint8_t observerKing = 0;
        std::uint8_t visible = 0;
        ExternalMask live;
        ExternalMask terminal;
        ExternalMask terminalOwner;
        ExternalMask terminalObserver;
        std::array<std::uint32_t, Squares> actualStratum{};
        std::array<std::uint32_t, Squares> ownerRoot{};
        std::array<std::uint8_t, Squares> visibleOwner{};
        std::array<std::uint8_t, Squares> visibleObserver{};
    };
    struct Stratum {
        std::uint32_t geometry = NoIndex;
        ExternalMask live;
        std::uint32_t observerRoot = 0;
    };
    struct Located {
        std::uint32_t geometry = NoIndex;
        std::uint8_t actual = 0;
    };

    LowerGhostSymbolicSidecar(
      const std::string& path, const std::string& concreteSha256,
      const std::string& modelSha256, const std::string& observationSha256) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error(
              "cannot open lower Ghost symbolic sidecar: " + path);
        std::array<char, 8> magic{};
        input.read(magic.data(), magic.size());
        const std::uint32_t version = read_u32(input);
        const std::uint32_t headerBytes = read_u32(input);
        const std::uint32_t piece = read_u32(input);
        const std::uint32_t ownerColor = read_u32(input);
        const std::uint32_t files = read_u32(input);
        const std::uint32_t ranks = read_u32(input);
        const std::uint32_t squares = read_u32(input);
        const std::uint32_t concreteCount = read_u32(input);
        const std::uint32_t substates = read_u32(input);
        const std::uint32_t geometryCount = read_u32(input);
        const std::uint32_t stratumCount = read_u32(input);
        const std::uint32_t nodeCount = read_u32(input);
        const std::uint32_t nodeBytes = read_u32(input);
        const std::uint32_t geometryBytes = read_u32(input);
        const std::uint32_t stratumBytes = read_u32(input);
        (void)read_u32(input);
        const std::uint64_t nodeOffset = read_u64(input);
        const std::uint64_t geometryOffset = read_u64(input);
        const std::uint64_t stratumOffset = read_u64(input);
        const std::string storedConcrete = read_text(input, 64);
        const std::string storedModel = read_text(input, 64);
        const std::string storedObservation = read_text(input, 64);
        const std::string semantics = read_text(input, 32);
        if (!input || magic !=
              std::array<char, 8>{{'U','F','G','M','1','\0','\0','\0'}} ||
            version != 1 || headerBytes != 320 ||
            piece != static_cast<std::uint32_t>(PieceType::Ghost) ||
            ownerColor != static_cast<std::uint32_t>(Color::White) ||
            files != Position::BoardFiles || ranks != Position::BoardRanks ||
            squares != Squares || concreteCount != LowerGhostStateCount ||
            substates != 2 || nodeBytes != 9 || geometryBytes != 844 ||
            stratumBytes != 18 || nodeCount < 2 ||
            semantics.c_str() != std::string("history-mask-public-view-v2") ||
            storedConcrete != concreteSha256 || storedModel != modelSha256 ||
            storedObservation != observationSha256 || nodeOffset != 320 ||
            geometryOffset != nodeOffset + std::uint64_t(nodeCount) * 9 ||
            stratumOffset != geometryOffset +
                               std::uint64_t(geometryCount) * 844)
            throw std::runtime_error(
              "lower Ghost symbolic sidecar metadata/hash mismatch");

        input.seekg(static_cast<std::streamoff>(nodeOffset));
        nodes_.resize(nodeCount);
        std::map<std::tuple<std::uint8_t, std::uint32_t, std::uint32_t>,
                 std::uint32_t> unique;
        for (std::uint32_t id = 0; id < nodeCount; ++id) {
            Node& node = nodes_[id];
            node.variable = read_u8(input);
            node.low = read_u32(input);
            node.high = read_u32(input);
            if (id < 2) {
                if (node.variable != Squares || node.low != id || node.high != id)
                    throw std::runtime_error(
                      "lower Ghost sidecar has invalid terminal nodes");
            }
            else {
                if (node.variable >= Squares || node.low >= id ||
                    node.high >= id || node.low == node.high)
                    throw std::runtime_error(
                      "lower Ghost sidecar has invalid ROBDD tuple");
                const auto child_variable = [&](std::uint32_t child) {
                    return child < 2 ? Squares : nodes_.at(child).variable;
                };
                if (child_variable(node.low) <= node.variable ||
                    child_variable(node.high) <= node.variable ||
                    !unique.emplace(
                      std::make_tuple(node.variable, node.low, node.high), id)
                       .second)
                    throw std::runtime_error(
                      "lower Ghost sidecar ROBDD is unordered or duplicated");
            }
        }

        input.seekg(static_cast<std::streamoff>(geometryOffset));
        geometries_.resize(geometryCount);
        geometryIndex_.reserve(geometryCount * 2);
        for (std::uint32_t id = 0; id < geometryCount; ++id) {
            Geometry& geometry = geometries_[id];
            geometry.side = read_u8(input);
            geometry.ownerKing = read_u8(input);
            geometry.observerKing = read_u8(input);
            geometry.visible = read_u8(input);
            geometry.live = read_mask(input);
            geometry.terminal = read_mask(input);
            geometry.terminalOwner = read_mask(input);
            geometry.terminalObserver = read_mask(input);
            for (std::uint32_t& value : geometry.actualStratum)
                value = read_u32(input);
            for (std::uint32_t& value : geometry.ownerRoot) {
                value = read_u32(input);
                if (value >= nodes_.size())
                    throw std::runtime_error(
                      "lower Ghost owner root is out of range");
            }
            input.read(reinterpret_cast<char*>(geometry.visibleOwner.data()),
                       geometry.visibleOwner.size());
            input.read(reinterpret_cast<char*>(geometry.visibleObserver.data()),
                       geometry.visibleObserver.size());
            const std::uint32_t code = lower_geometry_code(
              geometry.side, geometry.ownerKing, geometry.observerKing,
              geometry.visible != 0);
            if (!geometryIndex_.emplace(code, id).second)
                throw std::runtime_error(
                  "lower Ghost sidecar duplicates a public geometry");
        }
        input.seekg(static_cast<std::streamoff>(stratumOffset));
        strata_.resize(stratumCount);
        for (Stratum& stratum : strata_) {
            stratum.geometry = read_u32(input);
            stratum.live = read_mask(input);
            stratum.observerRoot = read_u32(input);
            if (stratum.geometry >= geometries_.size() ||
                stratum.observerRoot >= nodes_.size())
                throw std::runtime_error(
                  "lower Ghost sidecar stratum is out of range");
        }
        input.peek();
        if (!input.eof())
            throw std::runtime_error(
              "lower Ghost symbolic sidecar has trailing bytes");
        verify_reverse_maps();
    }

    [[nodiscard]] const std::vector<Node>& nodes() const { return nodes_; }
    [[nodiscard]] const Geometry& geometry(std::uint32_t id) const {
        return geometries_.at(id);
    }
    [[nodiscard]] const Stratum& stratum(std::uint32_t id) const {
        return strata_.at(id);
    }
    [[nodiscard]] Located locate(
      std::uint8_t side, std::uint8_t ownerKing, std::uint8_t observerKing,
      bool visible, std::uint8_t actual) const {
        const std::uint32_t code = lower_geometry_code(
          side, ownerKing, observerKing, visible);
        const auto found = geometryIndex_.find(code);
        if (found == geometryIndex_.end())
            throw std::runtime_error(
              "lower Ghost public geometry is absent from sidecar");
        return {found->second, actual};
    }
    [[nodiscard]] bool evaluate(
      std::uint32_t root, const ExternalMask& belief) const {
        while (root > 1) {
            const Node& node = nodes_.at(root);
            root = external_mask_test(belief, node.variable)
                 ? node.high : node.low;
        }
        return root == 1;
    }

  private:
    static std::uint8_t read_u8(std::istream& input) {
        const int value = input.get();
        if (value == std::char_traits<char>::eof())
            throw std::runtime_error("truncated lower Ghost sidecar");
        return static_cast<std::uint8_t>(value);
    }
    static std::uint32_t read_u32(std::istream& input) {
        std::array<std::uint8_t, 4> bytes{};
        input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        if (!input)
            throw std::runtime_error("truncated lower Ghost sidecar");
        return bytes[0] | (std::uint32_t(bytes[1]) << 8) |
               (std::uint32_t(bytes[2]) << 16) |
               (std::uint32_t(bytes[3]) << 24);
    }
    static std::uint64_t read_u64(std::istream& input) {
        const std::uint64_t low = read_u32(input);
        return low | (std::uint64_t(read_u32(input)) << 32);
    }
    static std::string read_text(std::istream& input, std::size_t size) {
        std::string value(size, '\0');
        input.read(value.data(), static_cast<std::streamsize>(size));
        if (!input)
            throw std::runtime_error("truncated lower Ghost sidecar");
        return value;
    }
    static ExternalMask read_mask(std::istream& input) {
        ExternalMask result;
        result.low = read_u64(input);
        std::array<std::uint8_t, 2> bytes{};
        input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        if (!input)
            throw std::runtime_error("truncated lower Ghost sidecar");
        result.high = static_cast<std::uint16_t>(
          bytes[0] | (std::uint16_t(bytes[1]) << 8));
        return result;
    }
    static std::uint32_t lower_geometry_code(
      std::uint8_t side, std::uint8_t ownerKing,
      std::uint8_t observerKing, bool visible) {
        return side | (std::uint32_t(ownerKing) << 1) |
               (std::uint32_t(observerKing) << 8) |
               (std::uint32_t(visible) << 15);
    }
    void verify_reverse_maps() const {
        for (std::uint32_t geometryId = 0; geometryId < geometries_.size();
             ++geometryId) {
            const Geometry& geometry = geometries_[geometryId];
            if (geometry.side > 1 || geometry.ownerKing >= Squares ||
                geometry.observerKing >= Squares ||
                geometry.ownerKing == geometry.observerKing ||
                geometry.visible > 1)
                throw std::runtime_error(
                  "lower Ghost geometry record is invalid");
            for (unsigned actual = 0; actual < Squares; ++actual) {
                const std::uint32_t stratumId = geometry.actualStratum[actual];
                const bool live = external_mask_test(geometry.live, actual);
                const bool terminal = external_mask_test(
                  geometry.terminal, actual);
                if ((!live || geometry.visible) && stratumId != NoIndex)
                    throw std::runtime_error(
                      "lower Ghost non-hidden world has a stratum");
                if (live && !geometry.visible && stratumId == NoIndex)
                    throw std::runtime_error(
                      "lower Ghost live hidden world lacks a stratum");
                if (stratumId != NoIndex &&
                    (stratumId >= strata_.size() ||
                     strata_[stratumId].geometry != geometryId ||
                     !external_mask_test(strata_[stratumId].live, actual)))
                    throw std::runtime_error(
                      "lower Ghost stratum reverse map is invalid");
                if (live && terminal)
                    throw std::runtime_error(
                      "lower Ghost live/terminal masks overlap");
            }
        }
    }

    std::vector<Node> nodes_;
    std::vector<Geometry> geometries_;
    std::vector<Stratum> strata_;
    std::unordered_map<std::uint32_t, std::uint32_t> geometryIndex_;
};

class LowerGhostRobddImport {
  public:
    LowerGhostRobddImport(const LowerGhostSymbolicSidecar& sidecar,
                          ExternalRobdd& target) {
        const auto& nodes = sidecar.nodes();
        imported_.resize(nodes.size(), ExternalRobdd::False);
        imported_[0] = ExternalRobdd::False;
        imported_[1] = ExternalRobdd::True;
        std::uint64_t structuralResidual = 0;
        for (std::uint32_t id = 2; id < nodes.size(); ++id) {
            const auto& source = nodes[id];
            const ExternalRobdd::Id result = target.make(
              source.variable, imported_.at(source.low),
              imported_.at(source.high));
            imported_[id] = result;
            const ExternalRobdd::Node copied = target.node(result);
            structuralResidual += copied.variable != source.variable ||
              copied.low != imported_.at(source.low) ||
              copied.high != imported_.at(source.high);
        }
        if (structuralResidual)
            throw std::runtime_error(
              "lower Ghost ROBDD import structural residual is nonzero");
        std::cout << "ghost_extra_lower_robdd_import source_nodes "
                  << nodes.size() << " target_nodes " << target.node_count()
                  << " structural_residual 0\n" << std::flush;
    }

    [[nodiscard]] ExternalRobdd::Id root(std::uint32_t source) const {
        return imported_.at(source);
    }
    [[nodiscard]] std::vector<ExternalRobdd::Id>& roots() {
        return imported_;
    }

  private:
    std::vector<ExternalRobdd::Id> imported_;
};

[[maybe_unused, noreturn]] void external_system_error(
  const std::string& operation, const std::string& path) {
    throw std::runtime_error(operation + " " + path + ": " +
                             std::strerror(errno));
}

template<typename Value>
class ExternalArray {
  public:
    ExternalArray() = default;
    ExternalArray(const std::string& path, std::uint64_t count, bool create)
      : path_(path), count_(count) {
        if (!count || count > std::numeric_limits<std::size_t>::max() /
                              sizeof(Value))
            throw std::runtime_error("external root-array size is invalid");
        bytes_ = count * sizeof(Value);
        int flags = O_RDWR;
        if (create)
            flags |= O_CREAT | O_TRUNC;
        descriptor_ = ::open(path.c_str(), flags, 0600);
        if (descriptor_ < 0)
            external_system_error("cannot open", path);
        if (create && ::ftruncate(descriptor_, static_cast<off_t>(bytes_)))
            external_system_error("cannot size", path);
        struct stat status{};
        if (::fstat(descriptor_, &status) ||
            static_cast<std::uint64_t>(status.st_size) != bytes_)
            external_system_error("cannot verify size of", path);
        data_ = static_cast<Value*>(::mmap(
          nullptr, static_cast<std::size_t>(bytes_), PROT_READ | PROT_WRITE,
          MAP_SHARED, descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            external_system_error("cannot mmap", path);
        }
    }
    ~ExternalArray() { close(); }
    ExternalArray(ExternalArray&& other) noexcept { swap(other); }
    ExternalArray& operator=(ExternalArray&& other) noexcept {
        if (this != &other) {
            close();
            swap(other);
        }
        return *this;
    }
    ExternalArray(const ExternalArray&) = delete;
    ExternalArray& operator=(const ExternalArray&) = delete;
    [[nodiscard]] Value& operator[](std::uint64_t index) {
        if (index >= count_)
            throw std::runtime_error("external root-array index is invalid");
        return data_[index];
    }
    [[nodiscard]] const Value& operator[](std::uint64_t index) const {
        if (index >= count_)
            throw std::runtime_error("external root-array index is invalid");
        return data_[index];
    }
    [[nodiscard]] std::uint64_t size() const { return count_; }
    [[nodiscard]] Value* begin() { return data_; }
    [[nodiscard]] Value* end() { return data_ + count_; }
    void fill(Value value) { std::fill(begin(), end(), value); }
    void flush() {
        if (::msync(data_, static_cast<std::size_t>(bytes_), MS_SYNC))
            external_system_error("cannot flush", path_);
    }

  private:
    void close() noexcept {
        if (data_)
            ::munmap(data_, static_cast<std::size_t>(bytes_));
        if (descriptor_ >= 0)
            ::close(descriptor_);
        data_ = nullptr;
        descriptor_ = -1;
        bytes_ = 0;
        count_ = 0;
    }
    void swap(ExternalArray& other) noexcept {
        std::swap(path_, other.path_);
        std::swap(descriptor_, other.descriptor_);
        std::swap(data_, other.data_);
        std::swap(bytes_, other.bytes_);
        std::swap(count_, other.count_);
    }
    std::string path_;
    int descriptor_ = -1;
    Value* data_ = nullptr;
    std::uint64_t bytes_ = 0;
    std::uint64_t count_ = 0;
};

template<typename Value>
[[nodiscard]] std::vector<Value> read_external_vector(
  const std::string& path, std::uint64_t expectedCount) {
    if (expectedCount > std::numeric_limits<std::size_t>::max() /
                          sizeof(Value))
        throw std::runtime_error("external certificate vector is too large");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open external certificate: " + path);
    input.seekg(0, std::ios::end);
    const std::uint64_t expectedBytes = expectedCount * sizeof(Value);
    if (input.tellg() < 0 ||
        static_cast<std::uint64_t>(input.tellg()) != expectedBytes)
        throw std::runtime_error("external certificate file size mismatch: " + path);
    std::vector<Value> result(static_cast<std::size_t>(expectedCount));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(result.data()), expectedBytes);
    if (!input)
        throw std::runtime_error("truncated external certificate: " + path);
    return result;
}

[[nodiscard]] std::uint8_t terminal_force_flags(
  const Position& position, const MaterialSpec& material) {
    if (!position.game_over())
        throw std::runtime_error("force flags requested for a live position");
    const std::optional<Color> winner = position.winner();
    std::uint8_t result = 0;
    if (winner && *winner == material.ghostColor)
        result |= 1;
    if (winner && *winner == material.observer())
        result |= 2;
    return result;
}

struct ExternalCompileSummary {
    std::uint64_t geometries = 0;
    std::uint64_t worlds = 0;
    std::uint64_t terminalWorlds = 0;
    std::uint64_t strata = 0;
    std::uint64_t edges = 0;
    std::uint64_t sameClass = 0;
    std::uint64_t lowerGhost = 0;
    std::uint64_t exact = 0;
    std::uint64_t observationClasses = 0;
    std::uint64_t actionClasses = 0;
    std::uint64_t symmetryWorlds = 0;
    std::uint64_t symmetryEdges = 0;
};

void verify_external_transition_certificate(
  const std::string& prefix, const MaterialSpec& material);

[[nodiscard]] std::uint64_t external_move_key(
  const Position& position, const Move& move, std::uint8_t transform,
  std::uint8_t copycatSquare) {
    if (move.from >= Squares || move.to >= Squares)
        throw std::runtime_error(
          "Bishop/Ghost rectangle certificate encountered an invalid move");
    const int actor = position.piece_on(move.from);
    if (actor == Position::NoPiece)
        throw std::runtime_error(
          "rectangle certificate move has no source actor");
    const PieceType actorType = position.piece(actor).type;
    const bool copycatMove = ExtraIsCopycat &&
      (move.from == copycatSquare ||
       move.from == horizontal_reflection(copycatSquare));
    const bool checkerMove =
      actorType == PieceType::Checker || actorType == PieceType::CheckerKing;
    std::uint8_t transformedAuxiliary = move.auxiliary;
    if (move.kind == MoveKind::Normal &&
        (copycatMove || (checkerMove && transformedAuxiliary != 0)) &&
        transformedAuxiliary < Squares)
        transformedAuxiliary = rectangle_transform_square(
          transformedAuxiliary, transform);
    else if (move.kind == MoveKind::Normal && transformedAuxiliary != 0)
        throw std::runtime_error(
          "ordinary extra move has a semantic auxiliary");
    else if (move.kind != MoveKind::Normal &&
             move.kind != MoveKind::Shoot)
        throw std::runtime_error(
          "rectangle certificate encountered an unsupported move kind");
    const std::uint64_t from = rectangle_transform_square(move.from, transform);
    const std::uint64_t to = rectangle_transform_square(move.to, transform);
    return from | (to << 7) |
           (std::uint64_t(transformedAuxiliary) << 14) |
           (std::uint64_t(move.kind) << 22) |
           (std::uint64_t(move.promotion) << 26);
}

class ExternalStringBijection {
  public:
    void bind(std::uint32_t sourceClass, const std::string& target,
              const char* label) {
        if (sourceClass >= forward_.size())
            forward_.resize(std::size_t(sourceClass) + 1);
        std::optional<std::string>& mapped = forward_[sourceClass];
        if (mapped && *mapped != target)
            throw std::runtime_error(
              std::string("D2 symmetry split an ") + label + " class");
        if (!mapped)
            mapped = target;
        const auto [reverse, inserted] = reverse_.emplace(target, sourceClass);
        if (!inserted && reverse->second != sourceClass)
            throw std::runtime_error(
              std::string("D2 symmetry merged two ") + label + " classes");
    }

  private:
    std::vector<std::optional<std::string>> forward_;
    std::unordered_map<std::string, std::uint32_t> reverse_;
};

struct CanonicalLowerSignature {
    std::uint8_t side = 0;
    std::uint8_t ownerKing = 0;
    std::uint8_t observerKing = 0;
    std::uint8_t visible = 0;
    std::uint8_t actual = 0;

    friend bool operator==(const CanonicalLowerSignature& lhs,
                           const CanonicalLowerSignature& rhs) {
        return lhs.side == rhs.side && lhs.ownerKing == rhs.ownerKing &&
               lhs.observerKing == rhs.observerKing &&
               lhs.visible == rhs.visible && lhs.actual == rhs.actual;
    }
};

[[nodiscard]] CanonicalLowerSignature canonical_lower_signature(
  std::uint32_t lowerIndex) {
    const FourState raw = decode_lower_ghost(lowerIndex);
    struct LowerPublic {
        std::uint8_t side;
        std::uint8_t ownerKing;
        std::uint8_t observerKing;
        std::uint8_t visible;
    } best{static_cast<std::uint8_t>(raw.side), raw.whiteKing,
           raw.blackKing, static_cast<std::uint8_t>(raw.visible)};
    std::uint8_t bestTransform = 0;
    for (std::uint8_t transform = 1; transform < 4; ++transform) {
        const LowerPublic candidate{best.side,
          rectangle_transform_square(raw.whiteKing, transform),
          rectangle_transform_square(raw.blackKing, transform), best.visible};
        if (std::tie(candidate.side, candidate.ownerKing,
                     candidate.observerKing, candidate.visible) <
            std::tie(best.side, best.ownerKing,
                     best.observerKing, best.visible)) {
            best = candidate;
            bestTransform = transform;
        }
    }
    return {best.side, best.ownerKing, best.observerKing, best.visible,
      rectangle_transform_square(raw.ghost, bestTransform)};
}

[[nodiscard]] ExternalCompiledEdge encode_external_child(
  const Position& child, const MaterialSpec& material,
  const ExtraGeometryDomain& domain) {
    ExternalCompiledEdge edge;
    const ClassifiedChild classified = classify_child(child, material);
    if (classified.domain == ChildDomain::SameClass) {
        const FourState raw = decode_index(classified.index);
        const auto [childGeometry, transform] = domain.locate({
          static_cast<std::uint8_t>(raw.side), raw.whiteKing,
          raw.blackKing, raw.bishop, static_cast<std::uint8_t>(raw.visible)});
        edge.domain = ExternalChildDomain::SameClass;
        edge.child = childGeometry;
        edge.childActual = rectangle_transform_square(raw.ghost, transform);
    }
    else if (classified.domain == ChildDomain::LowerGhost) {
        edge.domain = ExternalChildDomain::LowerGhost;
        edge.child = classified.index;
        edge.childActual = decode_lower_ghost(classified.index).ghost;
    }
    else if (classified.domain == ChildDomain::InsufficientBishop) {
        edge.domain = ExternalChildDomain::Exact;
        edge.exact = 0;
    }
    else if (classified.domain == ChildDomain::ExactTerminal) {
        edge.domain = ExternalChildDomain::Exact;
        edge.exact = terminal_force_flags(child, material);
    }
    else
        throw std::runtime_error(
          "external transition escaped all exact domains");
    return edge;
}

void verify_external_child_symmetry(
  const ExternalCompiledEdge& source, const ExternalCompiledEdge& transformed) {
    if (source.domain != transformed.domain || source.exact != transformed.exact)
        throw std::runtime_error("D2 symmetry changed a child domain/result");
    if (source.domain == ExternalChildDomain::SameClass) {
        if (source.child != transformed.child ||
            source.childActual != transformed.childActual)
            throw std::runtime_error(
              "D2 symmetry changed a canonical same-class child");
    }
    else if (source.domain == ExternalChildDomain::LowerGhost &&
             !(canonical_lower_signature(source.child) ==
               canonical_lower_signature(transformed.child)))
        throw std::runtime_error(
          "D2 symmetry changed a canonical lower-Ghost child");
}

void compile_external_transitions(
  const std::string& prefix, const MaterialSpec& material,
  std::uint32_t geometryStart, std::uint32_t geometryLimit) {
    if (material.ghostColor != Color::White)
        throw std::runtime_error(
          "the first external solve is restricted to same-side Bishop+Ghost");
    const auto started = std::chrono::steady_clock::now();
    const ExtraGeometryDomain domain;
    if (geometryStart >= domain.size())
        throw std::runtime_error(
          "external transition shard starts outside the geometry domain");
    const std::uint32_t remaining = static_cast<std::uint32_t>(
      domain.size() - geometryStart);
    const std::uint32_t count = geometryLimit
      ? std::min(geometryLimit, remaining) : remaining;
    std::ofstream headerFile(prefix + ".header",
      std::ios::binary | std::ios::trunc);
    std::ofstream metaFile(prefix + ".meta",
      std::ios::binary | std::ios::trunc);
    std::ofstream strataFile(prefix + ".strata",
      std::ios::binary | std::ios::trunc);
    std::ofstream indexFile(prefix + ".index",
      std::ios::binary | std::ios::trunc);
    std::ofstream blockFile(prefix + ".blocks",
      std::ios::binary | std::ios::trunc);
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile)
        throw std::runtime_error(
          "cannot create external Ghost-extra transition scratch");
    ExternalTransitionHeader header;
    header.material = static_cast<std::uint32_t>(material.ghostColor);
    header.geometryCount = count;
    header.reserved = geometryStart;
    headerFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
    ExternalCompileSummary summary;
    const DisclosureContext observer{material.observer(), false};
    for (std::uint32_t localGeometry = 0; localGeometry < count;
         ++localGeometry) {
        const std::uint32_t geometryId = geometryStart + localGeometry;
        const PublicExtraGeometry& geometry = domain[geometryId];
        ExternalGeometryMeta meta;
        meta.actualStratum.fill(NoIndex);
        std::map<std::string, ExternalMask> decisionBlocks;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (!valid_geometry_world(geometry, ghost))
                continue;
            Position position = make_geometry_position(geometry, ghost, material);
            ++summary.worlds;
            if (position.game_over()) {
                external_mask_set(meta.terminal, ghost);
                const std::uint8_t flags = terminal_force_flags(position, material);
                if (flags & 1) external_mask_set(meta.terminalOwner, ghost);
                if (flags & 2) external_mask_set(meta.terminalObserver, ghost);
                ++summary.terminalWorlds;
                continue;
            }
            external_mask_set(meta.live, ghost);
            if (geometry.visible)
                continue;
            const std::string decision =
              static_cast<Color>(geometry.side) == material.observer()
              ? decision_observation_key(position, observer) : std::string();
            external_mask_set(decisionBlocks[decision], ghost);
        }
        meta.stratumBase = static_cast<std::uint32_t>(summary.strata);
        meta.stratumCount = static_cast<std::uint32_t>(decisionBlocks.size());
        for (const auto& [decision, mask] : decisionBlocks) {
            (void)decision;
            const std::uint32_t stratum =
              static_cast<std::uint32_t>(summary.strata++);
            strataFile.write(reinterpret_cast<const char*>(&mask), sizeof(mask));
            for (unsigned ghost = 0; ghost < Squares; ++ghost)
                if (external_mask_test(mask, ghost))
                    meta.actualStratum[ghost] = stratum;
        }

        std::array<std::vector<ExternalCompiledEdge>, Squares> perSource;
        std::unordered_map<std::string, std::uint32_t> observations;
        std::unordered_map<std::string, std::uint32_t> actions;
        std::array<ExternalStringBijection, 4>
          actionBijection, observationBijection;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (!external_mask_test(meta.live, ghost))
                continue;
            Position position = make_geometry_position(geometry, ghost, material);
            const std::vector<Move> moves = position.legal_moves();
            for (const Move& move : moves) {
                Position child = position;
                Undo undo;
                if (!child.make_move(move, undo))
                    throw std::runtime_error(
                      "external transition compiler failed a legal move");
                const std::string observation = complete_transition_observation(
                  position, move, child, observer);
                const std::string action = position.move_to_string(move);
                const auto intern = [](auto& map, const std::string& text) {
                    if (const auto found = map.find(text); found != map.end())
                        return found->second;
                    const std::uint32_t id =
                      static_cast<std::uint32_t>(map.size());
                    if (!map.emplace(text, id).second)
                        throw std::runtime_error(
                          "external transition interner insertion failed");
                    return id;
                };
                ExternalCompiledEdge edge;
                edge.relation = intern(observations, observation);
                edge.action = intern(actions, action);
                const ExternalCompiledEdge childEdge = encode_external_child(
                  child, material, domain);
                edge.child = childEdge.child;
                edge.childActual = childEdge.childActual;
                edge.domain = childEdge.domain;
                edge.exact = childEdge.exact;
                if (edge.domain == ExternalChildDomain::SameClass) {
                    ++summary.sameClass;
                }
                else if (edge.domain == ExternalChildDomain::LowerGhost) {
                    ++summary.lowerGhost;
                }
                else ++summary.exact;
                perSource[ghost].push_back(edge);
                ++summary.edges;
            }

            // Exhaustive certificate over all three non-identity D2 images.
            // Full strings are bound bijectively within this public geometry,
            // so equality classes—not hashes—must transform consistently.
            for (std::uint8_t transform = 1;
                 transform < GeometryTransformCount; ++transform) {
                const PublicExtraGeometry transformedGeometry =
                  transform_geometry(geometry, transform);
                const std::uint8_t transformedGhost =
                  rectangle_transform_square(ghost, transform);
                Position pairedPosition = make_geometry_position(
                  transformedGeometry, transformedGhost, material);
                const std::vector<Move> pairedMoves =
                  pairedPosition.legal_moves();
                if (pairedMoves.size() != moves.size())
                    throw std::runtime_error(
                      "D2 symmetry changed the legal-action count");
                std::unordered_map<std::uint64_t, std::size_t> pairedByMove;
                pairedByMove.reserve(pairedMoves.size() * 2);
                for (std::size_t pairedIndex = 0;
                     pairedIndex < pairedMoves.size(); ++pairedIndex) {
                    const std::uint64_t key = external_move_key(
                      pairedPosition, pairedMoves[pairedIndex], 0,
                      transformedGeometry.bishop);
                    if (!pairedByMove.emplace(key, pairedIndex).second)
                        throw std::runtime_error(
                          "D2 target duplicates a rendered legal action");
                }
                for (std::size_t moveIndex = 0; moveIndex < moves.size();
                     ++moveIndex) {
                    const std::uint64_t wanted = external_move_key(
                      position, moves[moveIndex], transform, geometry.bishop);
                    const auto paired = pairedByMove.find(wanted);
                    if (paired == pairedByMove.end())
                        throw std::runtime_error(
                          "D2 symmetry lost a transformed legal action");
                    const std::size_t pairedIndex = paired->second;
                    Position pairedChild = pairedPosition;
                    Undo pairedUndo;
                    if (!pairedChild.make_move(
                          pairedMoves[pairedIndex], pairedUndo))
                        throw std::runtime_error(
                          "D2 certificate failed to apply paired moves");
                    verify_external_child_symmetry(
                      perSource[ghost][moveIndex], encode_external_child(
                        pairedChild, material, domain));
                    actionBijection[transform].bind(
                      perSource[ghost][moveIndex].action,
                      pairedPosition.move_to_string(pairedMoves[pairedIndex]),
                      "action");
                    observationBijection[transform].bind(
                      perSource[ghost][moveIndex].relation,
                      complete_transition_observation(
                        pairedPosition, pairedMoves[pairedIndex], pairedChild,
                        observer), "observation");
                    ++summary.symmetryEdges;
                }
                ++summary.symmetryWorlds;
            }
        }
        summary.observationClasses += observations.size();
        summary.actionClasses += actions.size();
        const std::uint64_t blockOffset =
          static_cast<std::uint64_t>(blockFile.tellp());
        indexFile.write(reinterpret_cast<const char*>(&blockOffset),
                        sizeof(blockOffset));
        std::array<std::uint32_t, Squares + 1> offsets{};
        std::uint32_t edgeCount = 0;
        for (unsigned ghost = 0; ghost < Squares; ++ghost) {
            offsets[ghost] = edgeCount;
            edgeCount += static_cast<std::uint32_t>(perSource[ghost].size());
        }
        offsets[Squares] = edgeCount;
        blockFile.write(reinterpret_cast<const char*>(offsets.data()),
                        sizeof(offsets));
        for (const auto& edges : perSource)
            blockFile.write(reinterpret_cast<const char*>(edges.data()),
                            edges.size() * sizeof(ExternalCompiledEdge));
        metaFile.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
        ++summary.geometries;
        header.completedGeometries = localGeometry + 1;
        header.edges = summary.edges;
        header.strata = summary.strata;
        header.blockBytes = static_cast<std::uint64_t>(blockFile.tellp());
        if ((localGeometry + 1) % 1'000 == 0 ||
            localGeometry + 1 == count) {
            headerFile.seekp(0);
            headerFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
            headerFile.flush();
            metaFile.flush();
            strataFile.flush();
            indexFile.flush();
            blockFile.flush();
            const double elapsed = std::chrono::duration<double>(
              std::chrono::steady_clock::now() - started).count();
            std::cout << "ghost_extra_external_compile "
                      << localGeometry + 1 << '/' << count
                      << " global_geometry " << geometryId + 1
                      << " worlds " << summary.worlds
                      << " edges " << summary.edges
                      << " strata " << summary.strata
                      << " block_bytes " << header.blockBytes
                      << " peak_rss_bytes " << peak_rss_bytes()
                      << " elapsed " << elapsed << "s\n" << std::flush;
        }
    }
    const std::uint64_t finalOffset =
      static_cast<std::uint64_t>(blockFile.tellp());
    indexFile.write(reinterpret_cast<const char*>(&finalOffset),
                    sizeof(finalOffset));
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile)
        throw std::runtime_error(
          "failed writing external Ghost-extra transition certificate");
    std::cout << "ghost_extra_external_compile_complete geometries "
              << summary.geometries << " worlds " << summary.worlds
              << " terminal_worlds " << summary.terminalWorlds
              << " strata " << summary.strata << " edges " << summary.edges
              << " same_class " << summary.sameClass
              << " lower_ghost " << summary.lowerGhost
              << " exact " << summary.exact
              << " action_classes " << summary.actionClasses
              << " observation_classes " << summary.observationClasses
              << " symmetry_worlds " << summary.symmetryWorlds
              << " symmetry_edges " << summary.symmetryEdges
              << " block_bytes " << finalOffset
              << " geometry_start " << geometryStart
              << " exhaustive "
              << (geometryStart == 0 && count == domain.size())
              << " solve_launched 0\n" << std::flush;
    headerFile.close();
    metaFile.close();
    strataFile.close();
    indexFile.close();
    blockFile.close();
    verify_external_transition_certificate(prefix, material);
}

void verify_external_transition_certificate(
  const std::string& prefix, const MaterialSpec& material) {
    const auto started = std::chrono::steady_clock::now();
    std::ifstream headerFile(prefix + ".header", std::ios::binary);
    if (!headerFile)
        throw std::runtime_error("cannot open external transition header");
    ExternalTransitionHeader header;
    headerFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!headerFile || header.magic !=
          std::array<char, 8>{{'U','F','G','X','1','\0','\0','\0'}} ||
        header.version != 1 ||
        header.material != static_cast<std::uint32_t>(material.ghostColor) ||
        header.completedGeometries != header.geometryCount)
        throw std::runtime_error("external transition header is incompatible");
    const ExtraGeometryDomain domain;
    const std::uint64_t geometryStart = header.reserved;
    if (!header.geometryCount || geometryStart + header.geometryCount >
          domain.size())
        throw std::runtime_error("external transition geometry count is invalid");
    const std::vector<ExternalGeometryMeta> metas =
      read_external_vector<ExternalGeometryMeta>(
        prefix + ".meta", header.geometryCount);
    const std::vector<ExternalMask> strata = read_external_vector<ExternalMask>(
      prefix + ".strata", header.strata);
    const std::vector<std::uint64_t> index = read_external_vector<std::uint64_t>(
      prefix + ".index", std::uint64_t(header.geometryCount) + 1);
    std::ifstream blocks(prefix + ".blocks", std::ios::binary);
    if (!blocks)
        throw std::runtime_error("cannot open external transition blocks");
    blocks.seekg(0, std::ios::end);
    if (blocks.tellg() < 0 ||
        static_cast<std::uint64_t>(blocks.tellg()) != header.blockBytes ||
        index.front() != 0 || index.back() != header.blockBytes)
        throw std::runtime_error("external transition block extent mismatch");
    const DisclosureContext observer{material.observer(), false};
    std::uint64_t verifiedWorlds = 0;
    std::uint64_t verifiedEdges = 0;
    std::uint64_t verifiedStrata = 0;
    for (std::uint32_t localGeometry = 0;
         localGeometry < header.geometryCount; ++localGeometry) {
        const std::uint32_t geometryId = static_cast<std::uint32_t>(
          geometryStart + localGeometry);
        if (index[localGeometry] > index[localGeometry + 1])
            throw std::runtime_error("external transition index is not monotone");
        const ExternalGeometryMeta& stored = metas[localGeometry];
        if (stored.stratumBase != verifiedStrata ||
            std::uint64_t(stored.stratumBase) + stored.stratumCount >
              strata.size())
            throw std::runtime_error("external stratum prefix is inconsistent");
        ExternalMask stratumUnion;
        for (std::uint32_t local = 0; local < stored.stratumCount; ++local) {
            const ExternalMask& mask = strata[stored.stratumBase + local];
            if ((stratumUnion.low & mask.low) ||
                (stratumUnion.high & mask.high))
                throw std::runtime_error("external decision strata overlap");
            stratumUnion.low |= mask.low;
            stratumUnion.high |= mask.high;
        }
        verifiedStrata += stored.stratumCount;
        const PublicExtraGeometry& geometry = domain[geometryId];
        ExternalGeometryMeta regenerated;
        regenerated.actualStratum.fill(NoIndex);
        std::map<std::string, ExternalMask> decisionBlocks;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (!valid_geometry_world(geometry, ghost))
                continue;
            ++verifiedWorlds;
            Position position = make_geometry_position(geometry, ghost, material);
            if (position.game_over()) {
                external_mask_set(regenerated.terminal, ghost);
                const std::uint8_t flags = terminal_force_flags(position, material);
                if (flags & 1)
                    external_mask_set(regenerated.terminalOwner, ghost);
                if (flags & 2)
                    external_mask_set(regenerated.terminalObserver, ghost);
            }
            else {
                external_mask_set(regenerated.live, ghost);
                if (!geometry.visible) {
                    const std::string decision =
                      static_cast<Color>(geometry.side) == material.observer()
                      ? decision_observation_key(position, observer)
                      : std::string();
                    external_mask_set(decisionBlocks[decision], ghost);
                }
            }
        }
        regenerated.stratumBase = stored.stratumBase;
        regenerated.stratumCount =
          static_cast<std::uint32_t>(decisionBlocks.size());
        std::uint32_t local = 0;
        for (const auto& [decision, mask] : decisionBlocks) {
            (void)decision;
            if (std::memcmp(&mask, &strata[stored.stratumBase + local],
                            sizeof(mask)))
                throw std::runtime_error(
                  "external decision stratum changed after reload");
            for (unsigned ghost = 0; ghost < Squares; ++ghost)
                if (external_mask_test(mask, ghost))
                    regenerated.actualStratum[ghost] =
                      stored.stratumBase + local;
            ++local;
        }
        if (std::memcmp(&stored, &regenerated, sizeof(stored)))
            throw std::runtime_error(
              "external geometry metadata reload residual is nonzero");

        const std::uint64_t blockBytes =
          index[localGeometry + 1] - index[localGeometry];
        if (blockBytes < sizeof(std::array<std::uint32_t, Squares + 1>) ||
            (blockBytes - sizeof(std::array<std::uint32_t, Squares + 1>)) %
              sizeof(ExternalCompiledEdge))
            throw std::runtime_error("external transition block is malformed");
        blocks.seekg(static_cast<std::streamoff>(index[localGeometry]));
        std::array<std::uint32_t, Squares + 1> offsets{};
        blocks.read(reinterpret_cast<char*>(offsets.data()), sizeof(offsets));
        const std::uint64_t edgeCount =
          (blockBytes - sizeof(offsets)) / sizeof(ExternalCompiledEdge);
        std::vector<ExternalCompiledEdge> edges(edgeCount);
        blocks.read(reinterpret_cast<char*>(edges.data()),
                    edges.size() * sizeof(ExternalCompiledEdge));
        if (!blocks || offsets.front() != 0 || offsets.back() != edgeCount)
            throw std::runtime_error("external source-edge offsets are invalid");
        std::unordered_map<std::string, std::uint32_t> observations;
        std::unordered_map<std::string, std::uint32_t> actions;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (offsets[ghost] > offsets[ghost + 1] ||
                offsets[ghost + 1] > edges.size())
                throw std::runtime_error(
                  "external source-edge range is not monotone");
            if (!external_mask_test(stored.live, ghost)) {
                if (offsets[ghost] != offsets[ghost + 1])
                    throw std::runtime_error(
                      "terminal/occupied source has compiled edges");
                continue;
            }
            Position position = make_geometry_position(geometry, ghost, material);
            const std::vector<Move> moves = position.legal_moves();
            if (moves.size() != offsets[ghost + 1] - offsets[ghost])
                throw std::runtime_error(
                  "external transition edge count changed after reload");
            for (std::size_t ordinal = 0; ordinal < moves.size(); ++ordinal) {
                Position child = position;
                Undo undo;
                if (!child.make_move(moves[ordinal], undo))
                    throw std::runtime_error(
                      "external reload verifier failed a legal move");
                const auto intern = [](auto& map, const std::string& text) {
                    if (const auto found = map.find(text); found != map.end())
                        return found->second;
                    const std::uint32_t id =
                      static_cast<std::uint32_t>(map.size());
                    map.emplace(text, id);
                    return id;
                };
                ExternalCompiledEdge expected = encode_external_child(
                  child, material, domain);
                expected.relation = intern(observations,
                  complete_transition_observation(
                    position, moves[ordinal], child, observer));
                expected.action = intern(
                  actions, position.move_to_string(moves[ordinal]));
                const ExternalCompiledEdge& actual =
                  edges[offsets[ghost] + ordinal];
                if (std::memcmp(&actual, &expected, sizeof(actual)))
                    throw std::runtime_error(
                      "external transition reload residual is nonzero");
                ++verifiedEdges;
            }
        }
        if ((localGeometry + 1) % 5'000 == 0 ||
            localGeometry + 1 == header.geometryCount) {
            const double elapsed = std::chrono::duration<double>(
              std::chrono::steady_clock::now() - started).count();
            std::cout << "ghost_extra_external_reload " << localGeometry + 1
                      << '/' << header.geometryCount
                      << " global_geometry " << geometryId + 1
                      << " worlds " << verifiedWorlds
                      << " edges " << verifiedEdges
                      << " elapsed " << elapsed << "s\n" << std::flush;
        }
    }
    if (verifiedStrata != header.strata || verifiedEdges != header.edges)
        throw std::runtime_error(
          "external transition reload conservation residual is nonzero");
    std::cout << "ghost_extra_external_transition_certificate geometries "
              << header.geometryCount << " worlds " << verifiedWorlds
              << " edges " << verifiedEdges << " strata " << verifiedStrata
              << " metadata_residual 0 transition_residual 0"
              << " geometry_start " << geometryStart
              << " conservation_residual 0\n" << std::flush;
    std::ofstream verified(prefix + ".verified",
      std::ios::binary | std::ios::trunc);
    verified.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!verified)
        throw std::runtime_error(
          "cannot persist external transition verification stamp");
}

void copy_external_bytes(std::ifstream& input, std::ofstream& output,
                         std::uint64_t bytes) {
    std::vector<char> buffer(8 << 20);
    while (bytes) {
        const std::size_t amount = static_cast<std::size_t>(
          std::min<std::uint64_t>(bytes, buffer.size()));
        input.read(buffer.data(), static_cast<std::streamsize>(amount));
        output.write(buffer.data(), static_cast<std::streamsize>(amount));
        if (!input || !output)
            throw std::runtime_error(
              "failed copying an external transition shard");
        bytes -= amount;
    }
}

void merge_external_transition_shards(
  const std::string& outputPrefix, std::vector<std::string> shardPrefixes,
  const MaterialSpec& material, std::uint32_t expectedGeometries) {
    if (shardPrefixes.empty())
        throw std::runtime_error(
          "external transition merge requires at least one shard");
    struct Shard {
        std::string prefix;
        ExternalTransitionHeader header;
    };
    std::vector<Shard> shards;
    shards.reserve(shardPrefixes.size());
    for (const std::string& prefix : shardPrefixes) {
        std::ifstream headerFile(prefix + ".header", std::ios::binary);
        std::ifstream verifiedFile(prefix + ".verified", std::ios::binary);
        Shard shard{prefix, {}};
        ExternalTransitionHeader verifiedHeader;
        headerFile.read(reinterpret_cast<char*>(&shard.header),
                        sizeof(shard.header));
        verifiedFile.read(reinterpret_cast<char*>(&verifiedHeader),
                          sizeof(verifiedHeader));
        if (!headerFile || !verifiedFile ||
            std::memcmp(&shard.header, &verifiedHeader,
                        sizeof(shard.header)) ||
            shard.header.magic !=
              std::array<char, 8>{{'U','F','G','X','1','\0','\0','\0'}} ||
            shard.header.version != 1 ||
            shard.header.material !=
              static_cast<std::uint32_t>(material.ghostColor) ||
            !shard.header.geometryCount ||
            shard.header.completedGeometries != shard.header.geometryCount)
            throw std::runtime_error(
              "external transition shard lacks a matching regeneration certificate");
        shards.push_back(std::move(shard));
    }
    std::sort(shards.begin(), shards.end(), [](const Shard& lhs,
                                               const Shard& rhs) {
        return lhs.header.reserved < rhs.header.reserved;
    });
    const ExtraGeometryDomain domain;
    std::uint64_t expectedStart = 0;
    ExternalTransitionHeader merged;
    merged.material = static_cast<std::uint32_t>(material.ghostColor);
    for (const Shard& shard : shards) {
        if (shard.header.reserved != expectedStart)
            throw std::runtime_error(
              "external transition shards have a gap or overlap");
        expectedStart += shard.header.geometryCount;
        merged.edges += shard.header.edges;
        merged.strata += shard.header.strata;
        merged.blockBytes += shard.header.blockBytes;
    }
    const std::uint64_t requiredGeometries = expectedGeometries
      ? expectedGeometries : domain.size();
    if (expectedStart != requiredGeometries ||
        expectedStart > std::numeric_limits<std::uint32_t>::max() ||
        merged.strata > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error(
          "external transition shards do not cover the complete domain");
    merged.geometryCount = static_cast<std::uint32_t>(expectedStart);
    merged.completedGeometries = merged.geometryCount;

    std::ofstream headerFile(outputPrefix + ".header",
      std::ios::binary | std::ios::trunc);
    std::ofstream metaFile(outputPrefix + ".meta",
      std::ios::binary | std::ios::trunc);
    std::ofstream strataFile(outputPrefix + ".strata",
      std::ios::binary | std::ios::trunc);
    std::ofstream indexFile(outputPrefix + ".index",
      std::ios::binary | std::ios::trunc);
    std::ofstream blockFile(outputPrefix + ".blocks",
      std::ios::binary | std::ios::trunc);
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile)
        throw std::runtime_error(
          "cannot create merged external transition database");
    headerFile.write(reinterpret_cast<const char*>(&merged), sizeof(merged));
    std::uint64_t stratumBase = 0;
    std::uint64_t blockBase = 0;
    std::uint64_t geometryBase = 0;
    std::uint64_t copiedEdges = 0;
    for (const Shard& shard : shards) {
        std::vector<ExternalGeometryMeta> metas =
          read_external_vector<ExternalGeometryMeta>(
            shard.prefix + ".meta", shard.header.geometryCount);
        for (ExternalGeometryMeta& meta : metas) {
            if (std::uint64_t(meta.stratumBase) + stratumBase >
                  std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error(
                  "merged external stratum base exceeds 32-bit metadata");
            meta.stratumBase += static_cast<std::uint32_t>(stratumBase);
            for (std::uint32_t& stratum : meta.actualStratum)
                if (stratum != NoIndex)
                    stratum += static_cast<std::uint32_t>(stratumBase);
        }
        metaFile.write(reinterpret_cast<const char*>(metas.data()),
                       static_cast<std::streamsize>(
                         metas.size() * sizeof(ExternalGeometryMeta)));
        std::ifstream shardStrata(shard.prefix + ".strata", std::ios::binary);
        copy_external_bytes(shardStrata, strataFile,
          shard.header.strata * sizeof(ExternalMask));
        const std::vector<std::uint64_t> shardIndex =
          read_external_vector<std::uint64_t>(shard.prefix + ".index",
            std::uint64_t(shard.header.geometryCount) + 1);
        if (shardIndex.front() ||
            shardIndex.back() != shard.header.blockBytes)
            throw std::runtime_error(
              "external transition shard index extent is invalid");
        for (std::uint32_t geometry = 0;
             geometry < shard.header.geometryCount; ++geometry) {
            const std::uint64_t offset = blockBase + shardIndex[geometry];
            indexFile.write(reinterpret_cast<const char*>(&offset),
                            sizeof(offset));
            if (shardIndex[geometry] > shardIndex[geometry + 1])
                throw std::runtime_error(
                  "external transition shard index is not monotone");
            copiedEdges += (shardIndex[geometry + 1] - shardIndex[geometry] -
              sizeof(std::array<std::uint32_t, Squares + 1>)) /
              sizeof(ExternalCompiledEdge);
        }
        std::ifstream shardBlocks(shard.prefix + ".blocks", std::ios::binary);
        copy_external_bytes(shardBlocks, blockFile, shard.header.blockBytes);
        stratumBase += shard.header.strata;
        blockBase += shard.header.blockBytes;
        geometryBase += shard.header.geometryCount;
    }
    indexFile.write(reinterpret_cast<const char*>(&blockBase),
                    sizeof(blockBase));
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile ||
        geometryBase != merged.geometryCount || stratumBase != merged.strata ||
        blockBase != merged.blockBytes || copiedEdges != merged.edges)
        throw std::runtime_error(
          "external transition shard merge conservation residual is nonzero");
    std::cout << "ghost_extra_external_merge_certificate shards "
              << shards.size() << " geometries " << geometryBase
              << " edges " << copiedEdges << " strata " << stratumBase
              << " block_bytes " << blockBase
              << " shard_regeneration_certificates " << shards.size()
              << " gap_residual 0 offset_residual 0 byte_residual 0"
              << " conservation_residual 0\n" << std::flush;
}

struct ExternalSolverEdge {
    std::uint32_t relation = 0;
    std::uint32_t action = 0;
    std::uint32_t childGeometry = NoIndex;
    std::uint8_t childActual = 0;
    ExternalChildDomain domain = ExternalChildDomain::Exact;
    std::uint8_t exact = 0;
};

struct ExternalSolverRelation {
    ExternalChildDomain domain = ExternalChildDomain::Exact;
    std::uint32_t childGeometry = NoIndex;
    std::uint32_t childStratum = NoIndex;
    ExternalMask possibleSources;
    ExternalMask badObserverSources;
    std::vector<std::pair<std::uint8_t, ExternalMask>> image;
    bool initialized = false;
    bool childTerminal = false;
    bool childVisible = false;
};

struct ExternalSolverObservation {
    std::uint32_t relation = 0;
    ExternalMask possibleSources;
};

struct ExternalSolverAction {
    ExternalMask legalSources;
    std::vector<ExternalSolverObservation> observations;
};

struct ExternalSolverBlock {
    std::array<std::vector<ExternalSolverEdge>, Squares> edges;
    std::vector<ExternalSolverRelation> relations;
    std::vector<ExternalSolverAction> actions;
};

void validate_lower_inherited_mask_coverage(
  std::size_t fixtureSources, std::uint64_t lowerGhostEdges) {
    if (fixtureSources >= 2 || !lowerGhostEdges)
        return;
    throw std::runtime_error(
      "lower inherited-mask fixture is absent despite live lower-Ghost edges");
}

class ExternalTransitionDatabase {
  public:
    ExternalTransitionDatabase(const std::string& prefix,
                               const MaterialSpec& material)
      : prefix_(prefix) {
        std::ifstream input(prefix + ".header", std::ios::binary);
        input.read(reinterpret_cast<char*>(&header_), sizeof(header_));
        if (!input || header_.magic !=
              std::array<char, 8>{{'U','F','G','X','1','\0','\0','\0'}} ||
            header_.version != 1 || header_.material !=
              static_cast<std::uint32_t>(material.ghostColor) ||
            !header_.geometryCount ||
            header_.reserved != 0 ||
            header_.completedGeometries != header_.geometryCount)
            throw std::runtime_error(
              "external transition database header is incompatible");
        metas_ = read_external_vector<ExternalGeometryMeta>(
          prefix + ".meta", header_.geometryCount);
        strata_ = read_external_vector<ExternalMask>(
          prefix + ".strata", header_.strata);
        index_ = read_external_vector<std::uint64_t>(
          prefix + ".index", std::uint64_t(header_.geometryCount) + 1);
        blocks_.open(prefix + ".blocks", std::ios::binary);
        if (!blocks_ || index_.front() || index_.back() != header_.blockBytes)
            throw std::runtime_error(
              "external transition database extent is invalid");
    }

    [[nodiscard]] std::uint32_t geometry_count() const {
        return header_.geometryCount;
    }
    [[nodiscard]] std::uint64_t edge_count() const { return header_.edges; }
    [[nodiscard]] const ExternalGeometryMeta& meta(std::uint32_t id) const {
        return metas_.at(id);
    }
    [[nodiscard]] const ExternalMask& stratum(std::uint32_t id) const {
        return strata_.at(id);
    }
    [[nodiscard]] std::uint64_t stratum_count() const { return strata_.size(); }

    [[nodiscard]] std::pair<std::array<std::uint32_t, Squares + 1>,
                            std::vector<ExternalCompiledEdge>>
    raw_block(std::uint32_t geometry) {
        if (geometry >= header_.geometryCount)
            throw std::runtime_error(
              "external transition geometry is out of range");
        const std::uint64_t begin = index_[geometry];
        const std::uint64_t end = index_[geometry + 1];
        if (begin > end || end - begin <
              sizeof(std::array<std::uint32_t, Squares + 1>) ||
            (end - begin - sizeof(std::array<std::uint32_t, Squares + 1>)) %
              sizeof(ExternalCompiledEdge))
            throw std::runtime_error(
              "external transition block extent is malformed");
        std::array<std::uint32_t, Squares + 1> offsets{};
        const std::uint64_t edgeCount =
          (end - begin - sizeof(offsets)) / sizeof(ExternalCompiledEdge);
        std::vector<ExternalCompiledEdge> edges(edgeCount);
        blocks_.clear();
        blocks_.seekg(static_cast<std::streamoff>(begin));
        blocks_.read(reinterpret_cast<char*>(offsets.data()), sizeof(offsets));
        blocks_.read(reinterpret_cast<char*>(edges.data()),
                     static_cast<std::streamsize>(
                       edges.size() * sizeof(ExternalCompiledEdge)));
        if (!blocks_ || offsets.front() || offsets.back() != edges.size())
            throw std::runtime_error(
              "external transition block failed to load");
        return {offsets, std::move(edges)};
    }

  private:
    std::string prefix_;
    ExternalTransitionHeader header_;
    std::vector<ExternalGeometryMeta> metas_;
    std::vector<ExternalMask> strata_;
    std::vector<std::uint64_t> index_;
    std::ifstream blocks_;
};

[[nodiscard]] bool external_child_force(
  const ExternalSolverRelation& relation, unsigned actual, bool owner,
  const ExternalTransitionDatabase& database,
  const LowerGhostSymbolicSidecar& lower) {
    if (!relation.childTerminal)
        throw std::runtime_error(
          "terminal child force requested for a live relation");
    const ExternalMask* force = nullptr;
    if (relation.domain == ExternalChildDomain::SameClass) {
        const ExternalGeometryMeta& child = database.meta(
          relation.childGeometry);
        force = owner ? &child.terminalOwner : &child.terminalObserver;
    }
    else if (relation.domain == ExternalChildDomain::LowerGhost) {
        const auto& child = lower.geometry(relation.childGeometry);
        force = owner ? &child.terminalOwner : &child.terminalObserver;
    }
    else
        throw std::runtime_error(
          "terminal force requested for an exact relation");
    return external_mask_test(*force, actual);
}

[[nodiscard]] ExternalSolverBlock build_external_solver_block(
  std::uint32_t geometryId, ExternalTransitionDatabase& database,
  const LowerGhostSymbolicSidecar& lower, const ExtraGeometryDomain& domain,
  const MaterialSpec& material) {
    const auto [offsets, compiled] = database.raw_block(geometryId);
    std::uint32_t relationCount = 0;
    std::uint32_t actionCount = 0;
    for (const ExternalCompiledEdge& edge : compiled) {
        relationCount = std::max(relationCount, edge.relation + 1);
        actionCount = std::max(actionCount, edge.action + 1);
    }
    ExternalSolverBlock result;
    result.relations.resize(relationCount);
    if (static_cast<Color>(domain[geometryId].side) == material.observer())
        result.actions.resize(actionCount);
    std::vector<std::map<std::uint8_t, ExternalMask>> images(relationCount);
    std::vector<std::map<std::uint32_t, ExternalMask>> actionObservations(
      result.actions.size());
    for (std::uint8_t source = 0; source < Squares; ++source) {
        if (offsets[source] > offsets[source + 1] ||
            offsets[source + 1] > compiled.size())
            throw std::runtime_error(
              "external solver source range is invalid");
        std::vector<std::uint8_t> seenActions(result.actions.size(), 0);
        for (std::uint32_t ordinal = offsets[source];
             ordinal < offsets[source + 1]; ++ordinal) {
            const ExternalCompiledEdge& stored = compiled[ordinal];
            ExternalSolverEdge edge;
            edge.relation = stored.relation;
            edge.action = stored.action;
            edge.domain = stored.domain;
            edge.exact = stored.exact;
            edge.childGeometry = stored.child;
            edge.childActual = stored.childActual;
            ExternalSolverRelation& relation =
              result.relations.at(edge.relation);
            external_mask_set(relation.possibleSources, source);

            bool childTerminal = false;
            bool childVisible = false;
            std::uint32_t childStratum = NoIndex;
            if (edge.domain == ExternalChildDomain::SameClass) {
                if (edge.childGeometry >= database.geometry_count())
                    throw std::runtime_error(
                      "partial transition certificate is not closed under moves");
                const ExternalGeometryMeta& child = database.meta(
                  edge.childGeometry);
                childTerminal = external_mask_test(
                  child.terminal, edge.childActual);
                childVisible = domain[edge.childGeometry].visible != 0;
                childStratum = child.actualStratum[edge.childActual];
            }
            else if (edge.domain == ExternalChildDomain::LowerGhost) {
                const CanonicalLowerSignature signature =
                  canonical_lower_signature(edge.childGeometry);
                const auto located = lower.locate(
                  signature.side, signature.ownerKing,
                  signature.observerKing, signature.visible != 0,
                  signature.actual);
                edge.childGeometry = located.geometry;
                edge.childActual = located.actual;
                const auto& child = lower.geometry(edge.childGeometry);
                childTerminal = external_mask_test(
                  child.terminal, edge.childActual);
                childVisible = child.visible != 0;
                childStratum = child.actualStratum[edge.childActual];
            }

            if (!relation.initialized) {
                relation.initialized = true;
                relation.domain = edge.domain;
                relation.childGeometry = edge.childGeometry;
                relation.childStratum = childStratum;
                relation.childTerminal = childTerminal;
                relation.childVisible = childVisible;
            }
            else if (relation.domain != edge.domain ||
                     (edge.domain != ExternalChildDomain::Exact &&
                      (relation.childGeometry != edge.childGeometry ||
                       relation.childStratum != childStratum ||
                       relation.childTerminal != childTerminal ||
                       relation.childVisible != childVisible)))
                throw std::runtime_error(
                  "one observation relation mixes child public domains");

            if (edge.domain == ExternalChildDomain::Exact) {
                if (!(edge.exact & 2))
                    external_mask_set(relation.badObserverSources, source);
            }
            else {
                external_mask_set(images[edge.relation][edge.childActual], source);
                if (childTerminal && !external_child_force(
                      relation, edge.childActual, false, database, lower))
                    external_mask_set(relation.badObserverSources, source);
            }
            result.edges[source].push_back(edge);

            if (!result.actions.empty()) {
                if (seenActions.at(edge.action))
                    throw std::runtime_error(
                      "one concrete world duplicates a public action");
                seenActions[edge.action] = 1;
                external_mask_set(result.actions[edge.action].legalSources,
                                  source);
                external_mask_set(
                  actionObservations[edge.action][edge.relation], source);
            }
        }
    }
    for (std::uint32_t relation = 0; relation < relationCount; ++relation) {
        if (!result.relations[relation].initialized)
            throw std::runtime_error(
              "external transition relation ID is not dense");
        for (const auto& [child, sources] : images[relation])
            result.relations[relation].image.emplace_back(child, sources);
        if (result.relations[relation].domain != ExternalChildDomain::Exact &&
            result.relations[relation].image.empty())
            throw std::runtime_error(
              "live external relation has an empty image");
        if (result.relations[relation].childVisible) {
            const std::uint8_t actual =
              result.relations[relation].image.front().first;
            if (!std::all_of(result.relations[relation].image.begin(),
                  result.relations[relation].image.end(),
                  [&](const auto& item) { return item.first == actual; }))
                throw std::runtime_error(
                  "one visible observation contains multiple Ghost squares");
        }
    }
    for (std::uint32_t action = 0; action < result.actions.size(); ++action)
        for (const auto& [relation, sources] : actionObservations[action])
            result.actions[action].observations.push_back({relation, sources});
    return result;
}

struct ExternalGhostExtraSolveOptions {
    std::string scratch;
    std::string output;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    ExternalRobdd::Limits bddLimits;
    std::uint32_t compactEvery = 4;
    // Nonzero is a resource-measurement mode, never a proof/result mode.  It
    // runs complete deterministic Bellman sweeps and exits without verification
    // or an overlay after the requested sweep, so a representative full-domain
    // RSS measurement can gate the much longer fixed point.
    std::uint32_t measureIterations = 0;
};

[[nodiscard]] std::uint64_t external_file_bytes(const std::string& path) {
    struct stat status{};
    if (::stat(path.c_str(), &status))
        external_system_error("cannot stat", path);
    return static_cast<std::uint64_t>(status.st_size);
}

[[nodiscard]] std::uint64_t physical_memory_bytes() {
#ifdef __APPLE__
    std::uint64_t bytes = 0;
    std::size_t length = sizeof(bytes);
    if (::sysctlbyname("hw.memsize", &bytes, &length, nullptr, 0) == 0 &&
        length == sizeof(bytes) && bytes)
        return bytes;
#endif
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const long pageBytes = ::sysconf(_SC_PAGESIZE);
    if (pages > 0 && pageBytes > 0 &&
        std::uint64_t(pages) <= std::numeric_limits<std::uint64_t>::max() /
                                  std::uint64_t(pageBytes))
        return std::uint64_t(pages) * std::uint64_t(pageBytes);
#endif
    throw std::runtime_error(
      "cannot determine physical memory for exact external solve gate");
}

void gate_external_ghost_extra_solve(
  const std::string& transitionPrefix,
  const ExternalTransitionDatabase& database,
  const ExternalGhostExtraSolveOptions& options) {
    constexpr std::uint64_t Budget = 97ULL << 30;
    const std::uint64_t geometryCount = database.geometry_count();
    const std::uint64_t strataCount = database.stratum_count();
    const std::uint64_t ownerRoots = geometryCount * Squares * sizeof(
      ExternalRobdd::Id) * 2;
    const std::uint64_t observerRoots = strataCount * sizeof(
      ExternalRobdd::Id) * 3;
    const std::uint64_t visibleRoots = geometryCount * Squares * 4;
    const std::uint64_t transitionBytes =
      external_file_bytes(transitionPrefix + ".header") +
      external_file_bytes(transitionPrefix + ".meta") +
      external_file_bytes(transitionPrefix + ".strata") +
      external_file_bytes(transitionPrefix + ".index") +
      external_file_bytes(transitionPrefix + ".blocks");
    const std::uint64_t bddBytes = ExternalRobdd::required_bytes(
      options.bddLimits);
    const std::uint64_t compactionBytes =
      std::uint64_t(options.bddLimits.maxNodes) * sizeof(std::uint32_t) +
      (std::uint64_t(options.bddLimits.maxNodes) + 7) / 8;
    const std::uint64_t scratchBytes = transitionBytes + bddBytes * 2 +
      compactionBytes + ownerRoots + observerRoots + visibleRoots;
    if (bddBytes > Budget || scratchBytes > Budget)
        throw std::runtime_error(
          "exact external Ghost-extra solve exceeds the 97 GiB gate");
    const std::size_t slash = options.scratch.rfind('/');
    const std::string scratchDirectory = slash == std::string::npos
      ? "." : slash == 0 ? "/" : options.scratch.substr(0, slash);
    struct statvfs freeSpace{};
    if (::statvfs(scratchDirectory.c_str(), &freeSpace))
        external_system_error(
          "cannot query free space for", scratchDirectory);
    const std::uint64_t available =
      std::uint64_t(freeSpace.f_bavail) * freeSpace.f_frsize;
    if (available < bddBytes * 2 + compactionBytes + ownerRoots +
                    observerRoots + visibleRoots)
        throw std::runtime_error(
          "insufficient free disk for exact external Ghost-extra solve");

    // Disk-backed does not mean memory-free: the unique index is randomly
    // accessed and the current node arena may become resident.  Bound the
    // ordinary fixed point and both streaming-compaction phases separately.
    // Compaction unmaps the source unique index before opening the replacement
    // one and evicts old node pages in 64 MiB chunks, so the two full arenas do
    // not appear in the same resident estimate.
    constexpr std::uint64_t NodeBytes = 9;
    constexpr std::uint64_t UniqueSlotBytes = sizeof(std::uint32_t);
    constexpr std::uint64_t StreamWindowBytes = 256ULL << 20;
    constexpr std::uint64_t FixedOverheadBytes = 128ULL << 20;
    const std::uint64_t nodeArenaBytes =
      std::uint64_t(options.bddLimits.maxNodes) * NodeBytes;
    const std::uint64_t uniqueIndexBytes =
      options.bddLimits.uniqueSlots * UniqueSlotBytes;
    const std::uint64_t forceRootBytes =
      ownerRoots + observerRoots + visibleRoots;
    const std::uint64_t cacheBytes = 64ULL *
      (options.bddLimits.applyCacheEntries +
       options.bddLimits.unaryCacheEntries +
       options.bddLimits.composeCacheEntries);
    const std::uint64_t ordinaryResident = nodeArenaBytes + uniqueIndexBytes +
      forceRootBytes + cacheBytes + FixedOverheadBytes;
    const std::uint64_t markResident = nodeArenaBytes + forceRootBytes +
      (std::uint64_t(options.bddLimits.maxNodes) + 7) / 8 +
      FixedOverheadBytes;
    const std::uint64_t copyResident = uniqueIndexBytes + forceRootBytes +
      std::uint64_t(options.bddLimits.maxNodes) * sizeof(std::uint32_t) +
      (std::uint64_t(options.bddLimits.maxNodes) + 7) / 8 + cacheBytes +
      StreamWindowBytes + FixedOverheadBytes;
    const std::uint64_t residentBytes = std::max(
      ordinaryResident, std::max(markResident, copyResident));
    const std::uint64_t physicalBytes = physical_memory_bytes();
    const std::uint64_t residentLimit = std::min<std::uint64_t>(
      12ULL << 30, physicalBytes * 7 / 10);
    if (residentBytes > residentLimit)
        throw std::runtime_error(
          "exact external Ghost-extra solve exceeds the physical-memory gate");
    std::cout << "ghost_extra_external_allocation_gate geometry_count "
              << geometryCount << " strata_count " << strataCount
              << " transition_bytes " << transitionBytes
              << " bdd_pair_bytes " << bddBytes * 2
              << " compaction_bytes " << compactionBytes
              << " force_root_bytes "
              << ownerRoots + observerRoots + visibleRoots
              << " total_scratch_bytes " << scratchBytes
              << " available_bytes " << available
              << " budget_bytes " << Budget << " admitted 1\n" << std::flush;
    std::cout << "ghost_extra_external_memory_gate physical_bytes "
              << physicalBytes << " resident_limit_bytes " << residentLimit
              << " ordinary_resident_bytes " << ordinaryResident
              << " compaction_mark_resident_bytes " << markResident
              << " compaction_copy_resident_bytes " << copyResident
              << " admitted 1\n" << std::flush;
}

class ExternalGhostExtraFixedPoint {
  public:
    ExternalGhostExtraFixedPoint(
      ExternalTransitionDatabase& database,
      const LowerGhostSymbolicSidecar& lower, const PackedFourTable& concrete,
      const ExtraGeometryDomain& domain, const MaterialSpec& material,
      ExternalGhostExtraSolveOptions options)
      : database_(database), lower_(lower), concrete_(concrete), domain_(domain),
        material_(material), options_(std::move(options)),
        bdd_(std::make_unique<ExternalRobdd>(
          options_.scratch + ".bdd-a", options_.bddLimits, true)),
        lowerImport_(lower_, *bdd_),
        ownerCurrent_(options_.scratch + ".owner-current",
          std::uint64_t(database_.geometry_count()) * Squares, true),
        ownerNext_(options_.scratch + ".owner-next",
          std::uint64_t(database_.geometry_count()) * Squares, true),
        observerCurrent_(options_.scratch + ".observer-current",
          database_.stratum_count(), true),
        observerNext_(options_.scratch + ".observer-next",
          database_.stratum_count(), true),
        domainRoots_(options_.scratch + ".domains",
          database_.stratum_count(), true),
        visibleOwnerCurrent_(options_.scratch + ".visible-owner-current",
          std::uint64_t(database_.geometry_count()) * Squares, true),
        visibleOwnerNext_(options_.scratch + ".visible-owner-next",
          std::uint64_t(database_.geometry_count()) * Squares, true),
        visibleObserverCurrent_(
          options_.scratch + ".visible-observer-current",
          std::uint64_t(database_.geometry_count()) * Squares, true),
        visibleObserverNext_(options_.scratch + ".visible-observer-next",
          std::uint64_t(database_.geometry_count()) * Squares, true) {
        if (material_.ghostColor != Color::White)
            throw std::runtime_error(
              "external exact solver currently supports same-side Bishop+Ghost only");
        if (options_.output.empty() ||
            !valid_sha256(options_.sourceSha256) ||
            !valid_sha256(options_.modelSha256) ||
            !valid_sha256(options_.observationSha256) ||
            options_.sourceSha256 != hex_digest(concrete_.sha()))
            throw std::runtime_error(
              "external exact solve requires matching source/model/observation SHA-256 values");
        ownerCurrent_.fill(ExternalRobdd::False);
        ownerNext_.fill(ExternalRobdd::False);
        observerCurrent_.fill(ExternalRobdd::False);
        observerNext_.fill(ExternalRobdd::False);
        visibleOwnerCurrent_.fill(0);
        visibleOwnerNext_.fill(0);
        visibleObserverCurrent_.fill(0);
        visibleObserverNext_.fill(0);
        for (std::uint64_t stratum = 0;
             stratum < database_.stratum_count(); ++stratum) {
            const ExternalMask& mask = database_.stratum(
              static_cast<std::uint32_t>(stratum));
            domainRoots_[stratum] = bdd_->subset_of(mask.low, mask.high);
        }
        inherited_lower_mask_self_test();
        fresh_root_public_grouping_self_test();
    }

    void solve() {
        const auto started = std::chrono::steady_clock::now();
        for (;;) {
            ++iteration_;
            std::uint64_t changedOwner = 0;
            std::uint64_t changedObserver = 0;
            std::uint64_t changedVisible = 0;
            for (std::uint32_t geometry = 0;
                 geometry < database_.geometry_count(); ++geometry) {
                bdd_->clear_computed_caches();
                const ExternalSolverBlock block = build_external_solver_block(
                  geometry, database_, lower_, domain_, material_);
                bellman_geometry(geometry, block);
                const ExternalGeometryMeta& meta = database_.meta(geometry);
                for (unsigned actual = 0; actual < Squares; ++actual) {
                    const std::uint64_t index = owner_index(geometry, actual);
                    const ExternalRobdd::Id oldOwner = ownerCurrent_[index];
                    const ExternalRobdd::Id newOwner = ownerNext_[index];
                    if (bdd_->logical_and(oldOwner,
                          bdd_->logical_not(newOwner)) != ExternalRobdd::False)
                        throw std::runtime_error(
                          "external owner least fixed point regressed");
                    changedOwner += oldOwner != newOwner;
                    if ((visibleOwnerCurrent_[index] &&
                         !visibleOwnerNext_[index]) ||
                        (visibleObserverCurrent_[index] &&
                         !visibleObserverNext_[index]))
                        throw std::runtime_error(
                          "external visible least fixed point regressed");
                    changedVisible += visibleOwnerCurrent_[index] !=
                                      visibleOwnerNext_[index];
                    changedVisible += visibleObserverCurrent_[index] !=
                                      visibleObserverNext_[index];
                }
                for (std::uint32_t local = 0; local < meta.stratumCount;
                     ++local) {
                    const std::uint32_t stratum = meta.stratumBase + local;
                    const ExternalRobdd::Id oldObserver =
                      observerCurrent_[stratum];
                    const ExternalRobdd::Id newObserver = observerNext_[stratum];
                    if (bdd_->logical_and(oldObserver,
                          bdd_->logical_not(newObserver)) !=
                        ExternalRobdd::False)
                        throw std::runtime_error(
                          "external observer least fixed point regressed");
                    changedObserver += oldObserver != newObserver;
                }
                if ((geometry + 1) % 5'000 == 0 ||
                    geometry + 1 == database_.geometry_count()) {
                    const double elapsed = std::chrono::duration<double>(
                      std::chrono::steady_clock::now() - started).count();
                    std::cout << "ghost_extra_external_bellman iteration "
                              << iteration_ << " geometry " << geometry + 1
                              << '/' << database_.geometry_count()
                              << " bdd_nodes " << bdd_->node_count()
                              << " peak_rss_bytes " << peak_rss_bytes()
                              << " elapsed " << elapsed << "s\n" << std::flush;
                }
            }
            swap_force_arrays();
            const double elapsed = std::chrono::duration<double>(
              std::chrono::steady_clock::now() - started).count();
            std::cout << "ghost_extra_external_iteration " << iteration_
                      << " bdd_nodes " << bdd_->node_count()
                      << " changed_owner " << changedOwner
                      << " changed_observer " << changedObserver
                      << " changed_visible " << changedVisible
                      << " peak_rss_bytes " << peak_rss_bytes()
                      << " elapsed " << elapsed << "s\n" << std::flush;
            if (options_.measureIterations &&
                iteration_ >= options_.measureIterations) {
                std::cout << "ghost_extra_external_measurement iterations "
                          << iteration_ << " bdd_nodes " << bdd_->node_count()
                          << " peak_rss_bytes " << peak_rss_bytes()
                          << " proof_complete 0 overlay_written 0\n"
                          << std::flush;
                return;
            }
            if (!changedOwner && !changedObserver && !changedVisible)
                break;
            if (options_.compactEvery &&
                iteration_ % options_.compactEvery == 0)
                compact();
        }
        verify();
    }

  private:
    void fresh_root_public_grouping_self_test() {
        const DisclosureContext observer{material_.observer(), false};
        std::unordered_map<std::string, std::string> fullToCompact;
        std::unordered_map<std::string, std::string> compactToFull;
        std::uint32_t random = 0x6a09e667u;
        std::uint64_t live = 0;
        std::uint64_t terminal = 0;
        std::uint64_t admitted = 0;
        constexpr std::uint32_t Samples = 200'000;
        for (std::uint32_t sample = 0; sample < Samples; ++sample) {
            random = random * 1664525u + 1013904223u;
            const std::uint32_t index = static_cast<std::uint32_t>(
              (std::uint64_t(random) * StateCount) >> 32);
            const FourState state = decode_index(index);
            if (!valid_world(state))
                continue;
            const bool hiddenAdjacent = !state.visible &&
              std::abs(int(state.ghost % Position::BoardFiles) -
                       int(state.blackKing % Position::BoardFiles)) <= 1 &&
              std::abs(int(state.ghost / Position::BoardFiles) -
                       int(state.blackKing / Position::BoardFiles)) <= 1;
            Position raw = make_position(index, material_);
            if (hiddenAdjacent || raw.has_forced_action() ||
                !raw.ordinary_predecessor_king_safe())
                continue;
            const PublicExtraGeometry source{
              static_cast<std::uint8_t>(state.side), state.whiteKing,
              state.blackKing, state.bishop,
              static_cast<std::uint8_t>(state.visible), state.extraSubstate};
            const CanonicalExtraGeometry canonical = canonical_geometry(source);
            const std::uint8_t actual = rectangle_transform_square(
              state.ghost, canonical.transform);
            Position position = make_geometry_position(
              canonical.geometry, actual, material_);
            const bool isTerminal = position.game_over();
            terminal += isTerminal;
            live += !isTerminal;
            ++admitted;

            std::string full = view_key(position, observer);
            if (!isTerminal && position.side_to_move() == observer.observer)
                full += "|decision=" + decision_observation_key(
                  position, observer);

            std::ostringstream compact;
            compact << int(canonical.geometry.side) << ','
                    << int(canonical.geometry.whiteKing) << ','
                    << int(canonical.geometry.blackKing) << ','
                    << int(canonical.geometry.bishop) << ','
                    << int(canonical.geometry.visible) << '|';
            if (canonical.geometry.visible)
                compact << "visible=" << int(actual);
            else if (isTerminal) {
                const std::optional<Color> winner = position.winner();
                compact << "terminal="
                        << (winner ? int(*winner) : 2);
            }
            else if (position.side_to_move() == observer.observer) {
                std::vector<std::pair<int, int>> markers;
                for (const Move& move : position.legal_moves())
                    markers.emplace_back(
                      move.kind == MoveKind::Pass ? -1 : move.from,
                      move.kind == MoveKind::Pass ? -1 : move.to);
                std::sort(markers.begin(), markers.end());
                markers.erase(std::unique(markers.begin(), markers.end()),
                              markers.end());
                compact << "dots=" << markers.size();
                for (const auto& [from, to] : markers)
                    compact << ',' << from << '>' << to;
            }
            else compact << "owner-turn";
            const std::string compactKey = compact.str();
            const auto [forward, insertedForward] = fullToCompact.emplace(
              full, compactKey);
            const auto [reverse, insertedReverse] = compactToFull.emplace(
              compactKey, full);
            if ((!insertedForward && forward->second != compactKey) ||
                (!insertedReverse && reverse->second != full))
                throw std::runtime_error(
                  "fresh-root compact/public observation grouping differs");
        }
        if (!live || !terminal || !admitted ||
            fullToCompact.size() != compactToFull.size())
            throw std::runtime_error(
              "fresh-root public grouping regression lacks live/terminal coverage");
        std::cout << "ghost_extra_fresh_root_grouping samples " << Samples
                  << " admitted " << admitted << " live " << live
                  << " terminal " << terminal << " public_sets "
                  << fullToCompact.size()
                  << " split_residual 0 merge_residual 0"
                  << " realization_residual 0\n" << std::flush;
    }

    void inherited_lower_mask_self_test() {
        struct World {
            std::uint8_t parentActual = 0;
            std::uint8_t childActual = 0;
            std::uint32_t childGeometry = NoIndex;
            std::uint32_t childStratum = NoIndex;
        };
        const PublicExtraGeometry raw{
          static_cast<std::uint8_t>(Color::Black), 0, 79, 78, 0};
        const auto [unusedGeometry, parentTransform] = domain_.locate(raw);
        (void)unusedGeometry;
        const DisclosureContext observer{material_.observer(), false};
        std::map<std::string, std::vector<World>> groups;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (ghost == raw.whiteKing || ghost == raw.blackKing ||
                ghost == raw.bishop)
                continue;
            if constexpr (ExtraIsCopycat)
                if (ghost == horizontal_reflection(raw.bishop))
                    continue;
            Position position = make_geometry_position(raw, ghost, material_);
            if (position.game_over())
                continue;
            // Legal-dot observations belong only to the side to move.  This
            // fixture deliberately uses the Black king to capture the extra
            // piece.  In the opposed material orientation Black owns the
            // Ghost, so the White observer sees no pre-move decision dots.
            const std::string decision =
              position.side_to_move() == observer.observer
              ? decision_observation_key(position, observer)
              : std::string();
            for (const Move& move : position.legal_moves()) {
                if (move.from != raw.blackKing || move.to != raw.bishop)
                    continue;
                Position child = position;
                Undo undo;
                if (!child.make_move(move, undo))
                    throw std::runtime_error(
                      "lower inherited-mask fixture move failed");
                const ClassifiedChild classified = classify_child(
                  child, material_);
                if (classified.domain != ChildDomain::LowerGhost)
                    throw std::runtime_error(
                      "lower inherited-mask fixture escaped KGhost");
                const CanonicalLowerSignature signature =
                  canonical_lower_signature(classified.index);
                const auto located = lower_.locate(
                  signature.side, signature.ownerKing,
                  signature.observerKing, signature.visible != 0,
                  signature.actual);
                const auto& lowerGeometry = lower_.geometry(located.geometry);
                if (lowerGeometry.visible ||
                    lowerGeometry.actualStratum[located.actual] == NoIndex)
                    continue;
                const std::string observation =
                  complete_transition_observation(
                    position, move, child, observer);
                std::ostringstream key;
                key << decision.size() << ':' << decision << '|'
                    << observation.size() << ':' << observation << '|'
                    << located.geometry << '|'
                    << lowerGeometry.actualStratum[located.actual];
                groups[key.str()].push_back({
                  rectangle_transform_square(ghost, parentTransform),
                  located.actual, located.geometry,
                  lowerGeometry.actualStratum[located.actual]});
            }
        }
        const auto selected = std::max_element(groups.begin(), groups.end(),
          [](const auto& lhs, const auto& rhs) {
              return lhs.second.size() < rhs.second.size();
          });
        if (selected == groups.end() || selected->second.size() < 2) {
            // Some material kernels cannot leave a live K+Ghost child. Bomb
            // captures are the important case: the explosion resolves the
            // move to an exact terminal result.  Absence of the inherited
            // Bishop fixture is valid only after exhaustively certifying the
            // stored transition graph has no live lower-Ghost edge.  This
            // keeps the shared solver fail-closed for every material that
            // really does inherit a lower information mask.
            std::uint64_t edges = 0;
            std::uint64_t lowerGhostEdges = 0;
            for (std::uint32_t geometry = 0;
                 geometry < database_.geometry_count(); ++geometry) {
                const auto [offsets, compiled] = database_.raw_block(geometry);
                (void)offsets;
                edges += compiled.size();
                lowerGhostEdges += std::count_if(
                  compiled.begin(), compiled.end(),
                  [](const ExternalCompiledEdge& edge) {
                      return edge.domain == ExternalChildDomain::LowerGhost;
                  });
            }
            validate_lower_inherited_mask_coverage(0, lowerGhostEdges);
            std::cout << "ghost_extra_lower_inherited_mask sources 0"
                      << " child_memberships 0 owner_roots 0 observer_roots 0"
                      << " stored_edges " << edges
                      << " lower_ghost_edges 0 residual 0\n" << std::flush;
            return;
        }
        const std::vector<World>& worlds = selected->second;
        ExternalMask parentMask;
        ExternalMask childMask;
        std::map<std::uint8_t, ExternalMask> imageMasks;
        for (const World& world : worlds) {
            external_mask_set(parentMask, world.parentActual);
            external_mask_set(childMask, world.childActual);
            external_mask_set(
              imageMasks[world.childActual], world.parentActual);
            if (world.childGeometry != worlds.front().childGeometry ||
                world.childStratum != worlds.front().childStratum)
                throw std::runtime_error(
                  "lower inherited-mask observation mixes public children");
        }
        std::vector<ExternalRobdd::Id> image(Squares, ExternalRobdd::False);
        for (const auto& [child, sources] : imageMasks)
            image[child] = mask_any(sources);
        std::uint64_t residual = 0;
        const auto& lowerGeometry = lower_.geometry(
          worlds.front().childGeometry);
        for (const World& world : worlds) {
            const std::uint32_t sourceRoot =
              lowerGeometry.ownerRoot[world.childActual];
            const bool expected = lower_.evaluate(sourceRoot, childMask);
            const ExternalRobdd::Id composed = bdd_->compose(
              lowerImport_.root(sourceRoot), image, 0xf000000000000001ULL);
            residual += bdd_->evaluate(
              composed, parentMask.low, parentMask.high) != expected;
        }
        const std::uint32_t observerSource = lower_.stratum(
          worlds.front().childStratum).observerRoot;
        const bool observerExpected = lower_.evaluate(
          observerSource, childMask);
        const ExternalRobdd::Id observerComposed = bdd_->compose(
          lowerImport_.root(observerSource), image, 0xf000000000000002ULL);
        residual += bdd_->evaluate(observerComposed, parentMask.low,
                                   parentMask.high) != observerExpected;
        if (residual)
            throw std::runtime_error(
              "lower inherited-mask symbolic composition residual is nonzero");
        std::cout << "ghost_extra_lower_inherited_mask sources "
                  << worlds.size() << " child_memberships "
                  << __builtin_popcountll(childMask.low) +
                       __builtin_popcount(childMask.high)
                  << " owner_roots " << worlds.size()
                  << " observer_roots 1 residual 0\n" << std::flush;
    }

    [[nodiscard]] std::uint64_t owner_index(
      std::uint32_t geometry, unsigned actual) const {
        return std::uint64_t(geometry) * Squares + actual;
    }

    [[nodiscard]] ExternalRobdd::Id mask_any(
      const ExternalMask& mask) {
        return bdd_->any(mask.low, mask.high);
    }
    [[nodiscard]] ExternalRobdd::Id no_sources(
      const ExternalMask& mask) {
        return bdd_->logical_not(mask_any(mask));
    }

    [[nodiscard]] std::vector<ExternalRobdd::Id> relation_image(
      const ExternalSolverRelation& relation) {
        std::vector<ExternalRobdd::Id> image(Squares, ExternalRobdd::False);
        for (const auto& [child, sources] : relation.image)
            image[child] = mask_any(sources);
        return image;
    }

    [[nodiscard]] ExternalRobdd::Id compose(
      std::uint32_t geometry, std::uint32_t relation,
      ExternalRobdd::Id child, const ExternalSolverRelation& model) {
        if (child <= ExternalRobdd::True)
            return child;
        const std::uint64_t key =
          (std::uint64_t(geometry) << 32) | relation;
        return bdd_->compose(child, relation_image(model), key);
    }

    [[nodiscard]] ExternalRobdd::Id owner_successor(
      std::uint32_t geometry, const ExternalSolverEdge& edge,
      const ExternalSolverRelation& relation) {
        if (edge.domain == ExternalChildDomain::Exact)
            return edge.exact & 1 ? ExternalRobdd::True
                                  : ExternalRobdd::False;
        if (relation.childTerminal)
            return external_child_force(
              relation, edge.childActual, true, database_, lower_)
              ? ExternalRobdd::True : ExternalRobdd::False;
        ExternalRobdd::Id child = ExternalRobdd::False;
        if (edge.domain == ExternalChildDomain::SameClass) {
            if (relation.childVisible)
                child = visibleOwnerCurrent_[owner_index(
                  relation.childGeometry, edge.childActual)]
                      ? ExternalRobdd::True : ExternalRobdd::False;
            else
                child = ownerCurrent_[owner_index(
                  relation.childGeometry, edge.childActual)];
        }
        else {
            const auto& lowerGeometry = lower_.geometry(
              relation.childGeometry);
            if (relation.childVisible)
                child = lowerGeometry.visibleOwner[edge.childActual]
                      ? ExternalRobdd::True : ExternalRobdd::False;
            else
                child = lowerImport_.root(
                  lowerGeometry.ownerRoot[edge.childActual]);
        }
        return compose(geometry, edge.relation, child, relation);
    }

    [[nodiscard]] ExternalRobdd::Id observer_successor(
      std::uint32_t geometry, std::uint32_t relationId,
      const ExternalSolverRelation& relation) {
        if (relation.domain == ExternalChildDomain::Exact ||
            relation.childTerminal)
            return no_sources(relation.badObserverSources);
        ExternalRobdd::Id child = ExternalRobdd::False;
        if (relation.domain == ExternalChildDomain::SameClass) {
            if (relation.childVisible) {
                const std::uint8_t actual = relation.image.front().first;
                child = visibleObserverCurrent_[owner_index(
                  relation.childGeometry, actual)]
                      ? ExternalRobdd::True : ExternalRobdd::False;
            }
            else {
                if (relation.childStratum == NoIndex)
                    throw std::runtime_error(
                      "same-class hidden relation lacks a child stratum");
                child = observerCurrent_[relation.childStratum];
            }
        }
        else {
            const auto& lowerGeometry = lower_.geometry(
              relation.childGeometry);
            if (relation.childVisible) {
                const std::uint8_t actual = relation.image.front().first;
                child = lowerGeometry.visibleObserver[actual]
                      ? ExternalRobdd::True : ExternalRobdd::False;
            }
            else {
                if (relation.childStratum == NoIndex)
                    throw std::runtime_error(
                      "lower hidden relation lacks a child stratum");
                child = lowerImport_.root(
                  lower_.stratum(relation.childStratum).observerRoot);
            }
        }
        return compose(geometry, relationId, child, relation);
    }

    [[nodiscard]] const ExternalSolverEdge* action_edge(
      const ExternalSolverBlock& block, unsigned actual,
      std::uint32_t action) const {
        const auto& edges = block.edges[actual];
        const auto found = std::find_if(edges.begin(), edges.end(),
          [&](const ExternalSolverEdge& edge) {
              return edge.action == action;
          });
        return found == edges.end() ? nullptr : &*found;
    }

    void bellman_geometry(std::uint32_t geometryId,
                          const ExternalSolverBlock& block) {
        const ExternalGeometryMeta& meta = database_.meta(geometryId);
        const PublicExtraGeometry& publicGeometry = domain_[geometryId];
        const Color mover = static_cast<Color>(publicGeometry.side);
        for (unsigned actual = 0; actual < Squares; ++actual) {
            const std::uint64_t index = owner_index(geometryId, actual);
            ownerNext_[index] = ExternalRobdd::False;
            visibleOwnerNext_[index] = 0;
            visibleObserverNext_[index] = 0;
            if (!external_mask_test(meta.live, actual))
                continue;
            if (publicGeometry.visible) {
                bool ownerValue = mover == material_.observer();
                bool observerValue = mover == material_.ghostColor;
                const ExternalMask singleton = external_singleton_mask(actual);
                for (const ExternalSolverEdge& edge : block.edges[actual]) {
                    const ExternalSolverRelation& relation =
                      block.relations.at(edge.relation);
                    const bool ownerChild = bdd_->evaluate(
                      owner_successor(geometryId, edge, relation),
                      singleton.low, singleton.high);
                    const bool observerChild = bdd_->evaluate(
                      observer_successor(
                        geometryId, edge.relation, relation),
                      singleton.low, singleton.high);
                    if (mover == material_.ghostColor) {
                        ownerValue = ownerValue || ownerChild;
                        observerValue = observerValue && observerChild;
                    }
                    else {
                        ownerValue = ownerValue && ownerChild;
                        observerValue = observerValue || observerChild;
                    }
                }
                visibleOwnerNext_[index] = ownerValue;
                visibleObserverNext_[index] = observerValue;
                continue;
            }
            const std::uint32_t stratum = meta.actualStratum[actual];
            if (stratum == NoIndex)
                throw std::runtime_error(
                  "live hidden parent lacks a decision stratum");
            ExternalRobdd::Id value;
            if (mover == material_.ghostColor) {
                value = ExternalRobdd::False;
                for (const ExternalSolverEdge& edge : block.edges[actual])
                    value = bdd_->logical_or(value, owner_successor(
                      geometryId, edge, block.relations.at(edge.relation)));
            }
            else {
                value = ExternalRobdd::True;
                for (std::uint32_t action = 0;
                     action < block.actions.size(); ++action) {
                    const ExternalRobdd::Id common = bdd_->logical_and(
                      domainRoots_[stratum], bdd_->subset_of(
                        block.actions[action].legalSources.low,
                        block.actions[action].legalSources.high));
                    const ExternalSolverEdge* edge = action_edge(
                      block, actual, action);
                    const ExternalRobdd::Id successor = edge
                      ? owner_successor(geometryId, *edge,
                          block.relations.at(edge->relation))
                      : ExternalRobdd::False;
                    value = bdd_->logical_and(value, bdd_->logical_or(
                      bdd_->logical_not(common), successor));
                }
            }
            ownerNext_[index] = bdd_->logical_and(domainRoots_[stratum],
              bdd_->logical_and(bdd_->variable(actual), value));
        }

        for (std::uint32_t local = 0; local < meta.stratumCount; ++local) {
            const std::uint32_t stratum = meta.stratumBase + local;
            const ExternalMask& worlds = database_.stratum(stratum);
            ExternalRobdd::Id value;
            if (mover == material_.ghostColor) {
                value = ExternalRobdd::True;
                for (unsigned actual = 0; actual < Squares; ++actual) {
                    if (!external_mask_test(worlds, actual))
                        continue;
                    for (const ExternalSolverEdge& edge : block.edges[actual]) {
                        const ExternalRobdd::Id child = observer_successor(
                          geometryId, edge.relation,
                          block.relations.at(edge.relation));
                        value = bdd_->logical_and(value, bdd_->logical_or(
                          bdd_->logical_not(bdd_->variable(actual)), child));
                    }
                }
            }
            else {
                value = ExternalRobdd::False;
                for (const ExternalSolverAction& action : block.actions) {
                    ExternalRobdd::Id gate = bdd_->logical_and(
                      domainRoots_[stratum], bdd_->subset_of(
                        action.legalSources.low, action.legalSources.high));
                    for (const ExternalSolverObservation& observation :
                         action.observations) {
                        const ExternalMask possibleSources = external_mask_and(
                          observation.possibleSources, worlds);
                        const ExternalRobdd::Id possible = mask_any(
                          possibleSources);
                        const ExternalRobdd::Id child = observer_successor(
                          geometryId, observation.relation,
                          block.relations.at(observation.relation));
                        gate = bdd_->logical_and(gate, bdd_->logical_or(
                          bdd_->logical_not(possible), child));
                    }
                    value = bdd_->logical_or(value, gate);
                }
            }
            observerNext_[stratum] = bdd_->logical_and(
              domainRoots_[stratum], value);
        }
    }

    void swap_force_arrays() {
        std::swap(ownerCurrent_, ownerNext_);
        std::swap(observerCurrent_, observerNext_);
        std::swap(visibleOwnerCurrent_, visibleOwnerNext_);
        std::swap(visibleObserverCurrent_, visibleObserverNext_);
    }

    void compact() {
        std::vector<ExternalRobdd::Id> roots;
        roots.reserve(lowerImport_.roots().size() + ownerCurrent_.size() +
                      observerCurrent_.size() + domainRoots_.size());
        roots.insert(roots.end(), lowerImport_.roots().begin(),
                     lowerImport_.roots().end());
        for (std::uint64_t index = 0; index < ownerCurrent_.size(); ++index)
            roots.push_back(ownerCurrent_[index]);
        for (std::uint64_t index = 0; index < observerCurrent_.size(); ++index)
            roots.push_back(observerCurrent_[index]);
        for (std::uint64_t index = 0; index < domainRoots_.size(); ++index)
            roots.push_back(domainRoots_[index]);
        const std::string replacement = options_.scratch +
          (compactToB_ ? ".bdd-b" : ".bdd-a");
        compactToB_ = !compactToB_;
        auto [fresh, certificate] = bdd_->compact(
          replacement, options_.scratch + ".bdd-remap", roots);
        std::size_t cursor = 0;
        for (ExternalRobdd::Id& root : lowerImport_.roots())
            root = roots.at(cursor++);
        for (std::uint64_t index = 0; index < ownerCurrent_.size(); ++index)
            ownerCurrent_[index] = roots.at(cursor++);
        for (std::uint64_t index = 0; index < observerCurrent_.size(); ++index)
            observerCurrent_[index] = roots.at(cursor++);
        for (std::uint64_t index = 0; index < domainRoots_.size(); ++index)
            domainRoots_[index] = roots.at(cursor++);
        if (cursor != roots.size() || certificate.structuralResidual ||
            certificate.rootResidual)
            throw std::runtime_error(
              "external Ghost-extra compaction remap residual is nonzero");
        bdd_ = std::move(fresh);
        ownerNext_.fill(ExternalRobdd::False);
        observerNext_.fill(ExternalRobdd::False);
        std::cout << "ghost_extra_external_compaction iteration " << iteration_
                  << " roots " << certificate.roots
                  << " marked_nodes " << certificate.markedNodes
                  << " copied_nodes " << certificate.copiedNodes
                  << " structural_residual 0 root_residual 0\n" << std::flush;
    }

    void verify() {
        // One independent full Bellman pass proves equality of every force
        // predicate over all 2^80 masks because canonical ROBDD identity is
        // exact, not sampled.
        for (std::uint32_t geometry = 0;
             geometry < database_.geometry_count(); ++geometry) {
            bdd_->clear_computed_caches();
            bellman_geometry(geometry, build_external_solver_block(
              geometry, database_, lower_, domain_, material_));
        }
        std::uint64_t bellmanResidual = 0;
        std::uint64_t monotonicityResidual = 0;
        for (std::uint64_t index = 0; index < ownerCurrent_.size(); ++index) {
            bellmanResidual += ownerCurrent_[index] != ownerNext_[index];
            const std::uint32_t geometry = static_cast<std::uint32_t>(
              index / Squares);
            const unsigned actual = static_cast<unsigned>(index % Squares);
            const std::uint32_t stratum =
              database_.meta(geometry).actualStratum[actual];
            if (stratum != NoIndex) {
                const ExternalMask& domain = database_.stratum(stratum);
                monotonicityResidual += !bdd_->is_upward_closed(
                  ownerCurrent_[index], domain.low, domain.high);
            }
            bellmanResidual += visibleOwnerCurrent_[index] !=
                               visibleOwnerNext_[index];
            bellmanResidual += visibleObserverCurrent_[index] !=
                               visibleObserverNext_[index];
        }
        for (std::uint64_t stratum = 0;
             stratum < observerCurrent_.size(); ++stratum) {
            bellmanResidual += observerCurrent_[stratum] !=
                               observerNext_[stratum];
            const ExternalMask& domain = database_.stratum(
              static_cast<std::uint32_t>(stratum));
            monotonicityResidual += !bdd_->is_downward_closed(
              observerCurrent_[stratum], domain.low, domain.high);
        }
        if (bellmanResidual || monotonicityResidual)
            throw std::runtime_error(
              "external Ghost-extra symbolic certificate has a residual");
        const std::uint64_t singletonResidual = verify_singletons();
        if (singletonResidual)
            throw std::runtime_error(
              "external Ghost-extra singleton result differs from concrete WDL");
        std::cout << "information_symbolic_certificate iterations "
                  << iteration_ << " bdd_nodes " << bdd_->node_count()
                  << " bellman_residual 0 monotonicity_residual 0"
                  << " singleton_residual 0 compaction_root_residual 0"
              << " belief_cap none powerset_exact 1\n" << std::flush;
        if (database_.geometry_count() == domain_.size())
            report_fresh_roots();
    }

    [[nodiscard]] std::uint64_t verify_singletons() {
        std::uint64_t residual = 0;
        for (std::uint32_t placement = 0; placement < PlacementCount;
             ++placement) {
          for (std::uint8_t extraSubstate = 0;
               extraSubstate < ExtraSubstates; ++extraSubstate)
            for (std::uint32_t substate = 0; substate < 2; ++substate) {
                FourState state = decode_placement(placement);
                state.extraSubstate = extraSubstate;
                state.visible = substate != 0;
                if (!valid_world(state))
                    continue;
                const std::uint32_t concreteIndex =
                  (placement * ExtraSubstates + extraSubstate) * 2 + substate;
                const PublicExtraGeometry raw{
                  static_cast<std::uint8_t>(state.side), state.whiteKing,
                  state.blackKing, state.bishop,
                  static_cast<std::uint8_t>(state.visible),
                  state.extraSubstate};
                const auto [geometry, transform] = domain_.locate(raw);
                if (geometry >= database_.geometry_count())
                    continue;
                const unsigned actual = rectangle_transform_square(
                  state.ghost, transform);
                const ExternalGeometryMeta& meta = database_.meta(geometry);
                bool owner = false;
                bool observer = false;
                if (external_mask_test(meta.terminal, actual)) {
                    owner = external_mask_test(meta.terminalOwner, actual);
                    observer = external_mask_test(
                      meta.terminalObserver, actual);
                }
                else if (state.visible) {
                    owner = visibleOwnerCurrent_[owner_index(geometry, actual)];
                    observer = visibleObserverCurrent_[owner_index(
                      geometry, actual)];
                }
                else {
                    const std::uint32_t stratum = meta.actualStratum[actual];
                    if (stratum == NoIndex)
                        throw std::runtime_error(
                          "singleton verification lacks a decision stratum");
                    const ExternalMask singleton = external_singleton_mask(actual);
                    owner = bdd_->evaluate(
                      ownerCurrent_[owner_index(geometry, actual)],
                      singleton.low, singleton.high);
                    observer = bdd_->evaluate(observerCurrent_[stratum],
                      singleton.low, singleton.high);
                }
                const std::uint8_t exact = concrete_.result(concreteIndex);
                const bool ownerExpected =
                  (state.side == material_.ghostColor && exact == 1) ||
                  (state.side == material_.observer() && exact == 2);
                const bool observerExpected =
                  (state.side == material_.observer() && exact == 1) ||
                  (state.side == material_.ghostColor && exact == 2);
                residual += owner != ownerExpected ||
                            observer != observerExpected;
            }
        }
        return residual;
    }

    void report_fresh_roots() {
        const auto started = std::chrono::steady_clock::now();
        std::vector<std::uint8_t> admitted((StateCount + 7) / 8, 0);
        std::vector<ExternalMask> freshMasks(database_.stratum_count());
        std::array<std::array<std::uint64_t, 4>, 2> unreachable{};
        std::uint64_t admittedCount = 0;
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            const FourState state = decode_index(index);
            if (!valid_world(state)) {
                ++unreachable[static_cast<std::size_t>(state.side)]
                              [concrete_.result(index)];
                continue;
            }
            const bool hiddenAdjacent = !state.visible &&
              std::abs(int(state.ghost % Position::BoardFiles) -
                       int(state.blackKing % Position::BoardFiles)) <= 1 &&
              std::abs(int(state.ghost / Position::BoardFiles) -
                       int(state.blackKing / Position::BoardFiles)) <= 1;
            const Position position = make_position(index, material_);
            const bool allowed = !hiddenAdjacent &&
              !position.has_forced_action() &&
              position.ordinary_predecessor_king_safe();
            if (!allowed) {
                ++unreachable[static_cast<std::size_t>(state.side)]
                              [concrete_.result(index)];
                continue;
            }
            admitted[index / 8] |= static_cast<std::uint8_t>(
              1u << (index % 8));
            ++admittedCount;
            if (!state.visible) {
                const PublicExtraGeometry raw{
                  static_cast<std::uint8_t>(state.side), state.whiteKing,
                  state.blackKing, state.bishop, 0, state.extraSubstate};
                const auto [geometry, transform] = domain_.locate(raw);
                const unsigned actual = rectangle_transform_square(
                  state.ghost, transform);
                const std::uint32_t stratum =
                  database_.meta(geometry).actualStratum[actual];
                if (stratum != NoIndex)
                    external_mask_set(freshMasks[stratum], actual);
            }
            if ((index + 1) % 5'000'000 == 0)
                std::cout << "ghost_extra_external_fresh_admission "
                          << index + 1 << '/' << StateCount
                          << " admitted " << admittedCount
                          << " elapsed " << std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - started).count()
                          << "s\n" << std::flush;
        }

        std::array<std::array<std::uint64_t, 4>, 2> totals{};
        std::vector<std::uint8_t> flags(StateCount, 0);
        std::array<std::uint64_t, 2> rootSets{};
        std::vector<std::uint8_t> seenVisible(
          std::uint64_t(database_.geometry_count()) * Squares, 0);
        std::vector<std::uint8_t> seenStratum(database_.stratum_count(), 0);
        std::vector<std::uint8_t> seenTerminal(
          std::uint64_t(database_.geometry_count()) * 3, 0);
        std::vector<std::uint8_t> independentlySeenVisible(
          std::uint64_t(database_.geometry_count()) * Squares, 0);
        std::vector<std::uint8_t> independentlySeenStratum(
          database_.stratum_count(), 0);
        std::vector<std::uint8_t> independentlySeenTerminal(
          std::uint64_t(database_.geometry_count()) * 3, 0);
        std::array<std::uint64_t, 2> independentSets{};
        std::array<std::uint64_t, 2> realizationCounts{};
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            if (!((admitted[index / 8] >> (index % 8)) & 1u))
                continue;
            const FourState state = decode_index(index);
            const std::size_t side = static_cast<std::size_t>(state.side);
            const PublicExtraGeometry raw{
              static_cast<std::uint8_t>(state.side), state.whiteKing,
              state.blackKing, state.bishop,
              static_cast<std::uint8_t>(state.visible), state.extraSubstate};
            const auto [geometry, transform] = domain_.locate(raw);
            const unsigned actual = rectangle_transform_square(
              state.ghost, transform);
            const ExternalGeometryMeta& meta = database_.meta(geometry);
            ++realizationCounts[side];
            // Independent public-root enumeration.  This does not consult the
            // force-result branches below: visible views key the disclosed
            // square, live hidden views key the exact legal-dot stratum, and
            // terminal hidden views key the publicly observed W/L/D outcome.
            if (state.visible) {
                const std::uint64_t key = owner_index(geometry, actual);
                if (!independentlySeenVisible[key]) {
                    independentlySeenVisible[key] = 1;
                    ++independentSets[side];
                }
            }
            else if (external_mask_test(meta.terminal, actual)) {
                const std::size_t outcome =
                  external_mask_test(meta.terminalOwner, actual) ? 0 :
                  external_mask_test(meta.terminalObserver, actual) ? 1 : 2;
                const std::uint64_t key =
                  std::uint64_t(geometry) * 3 + outcome;
                if (!independentlySeenTerminal[key]) {
                    independentlySeenTerminal[key] = 1;
                    ++independentSets[side];
                }
            }
            else {
                const std::uint32_t key = meta.actualStratum[actual];
                if (key == NoIndex)
                    throw std::runtime_error(
                      "independent live root lacks a decision observation");
                if (!independentlySeenStratum[key]) {
                    independentlySeenStratum[key] = 1;
                    ++independentSets[side];
                }
            }
            bool owner = false;
            bool observer = false;
            if (external_mask_test(meta.terminal, actual)) {
                owner = external_mask_test(meta.terminalOwner, actual);
                observer = external_mask_test(meta.terminalObserver, actual);
                if (state.visible) {
                    const std::uint64_t root = owner_index(geometry, actual);
                    if (!seenVisible[root]) {
                        seenVisible[root] = 1;
                        ++rootSets[side];
                    }
                }
                else {
                    const std::size_t outcome = owner ? 0 : observer ? 1 : 2;
                    const std::uint64_t root =
                      std::uint64_t(geometry) * 3 + outcome;
                    if (!seenTerminal[root]) {
                        seenTerminal[root] = 1;
                        ++rootSets[side];
                    }
                }
            }
            else if (state.visible) {
                const std::uint64_t root = owner_index(geometry, actual);
                owner = visibleOwnerCurrent_[root];
                observer = visibleObserverCurrent_[root];
                if (!seenVisible[root]) {
                    seenVisible[root] = 1;
                    ++rootSets[side];
                }
            }
            else {
                const std::uint32_t stratum = meta.actualStratum[actual];
                if (stratum == NoIndex ||
                    !external_mask_test(freshMasks[stratum], actual))
                    throw std::runtime_error(
                      "admitted hidden root crosses its decision stratum");
                owner = bdd_->evaluate(
                  ownerCurrent_[owner_index(geometry, actual)],
                  freshMasks[stratum].low, freshMasks[stratum].high);
                observer = bdd_->evaluate(observerCurrent_[stratum],
                  freshMasks[stratum].low, freshMasks[stratum].high);
                if (!seenStratum[stratum]) {
                    seenStratum[stratum] = 1;
                    ++rootSets[side];
                }
            }
            if (owner && observer)
                throw std::runtime_error(
                  "both players force a win at one fresh Ghost-extra root");
            flags[index] = static_cast<std::uint8_t>(
              4 | (owner ? 1 : 0) | (observer ? 2 : 0));
            const bool moverWins = state.side == material_.ghostColor
                                 ? owner : observer;
            const bool moverLoses = state.side == material_.ghostColor
                                  ? observer : owner;
            const std::size_t result = moverWins ? 1 : moverLoses ? 2 : 3;
            ++totals[side][result];
        }
        for (std::size_t side = 0; side < 2; ++side) {
            std::uint64_t conserved = 0;
            for (std::size_t result = 1; result < 4; ++result)
                conserved += totals[side][result] + unreachable[side][result];
            if (conserved != StateCount / 2)
                throw std::runtime_error(
                  "Ghost-extra fresh-root summary does not conserve states");
            const std::uint64_t realized = totals[side][1] + totals[side][2] +
                                           totals[side][3];
            if (rootSets[side] != independentSets[side] ||
                realized != realizationCounts[side])
                throw std::runtime_error(
                  "Ghost-extra independent public-root grouping residual");
            std::cout << "information_summary side " << side
                      << " win " << totals[side][1]
                      << " loss " << totals[side][2]
                      << " draw " << totals[side][3]
                      << " unreachable_win " << unreachable[side][1]
                      << " unreachable_loss " << unreachable[side][2]
                      << " unreachable_draw " << unreachable[side][3]
                      << " sets " << rootSets[side]
                      << " concrete " << StateCount / 2
                      << " bellman_residual 0 rank_residual 0"
                      << " belief_cap none exhaustive 1\n";
        }
        std::cout << "ghost_extra_external_root_conservation admitted "
                  << admittedCount << " total " << StateCount
                  << " independent_grouping_residual 0"
                  << " realization_residual 0 conservation_residual 0\n"
                  << std::flush;
        std::ofstream output(options_.output,
          std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error(
              "cannot create Ghost-extra information overlay");
        const auto writeU32 = [&](std::uint32_t value) {
            output.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };
        output.write("UFIW2\0\0\0", 8);
        writeU32(2);
        writeU32(static_cast<std::uint32_t>(PieceType::Bishop));
        writeU32(static_cast<std::uint32_t>(PieceType::Ghost));
        writeU32(static_cast<std::uint32_t>(material_.ghostColor));
        writeU32(StateCount);
        writeU32(GhostSubstates);
        output.write(options_.sourceSha256.data(),
                     static_cast<std::streamsize>(options_.sourceSha256.size()));
        output.write(options_.modelSha256.data(),
                     static_cast<std::streamsize>(options_.modelSha256.size()));
        output.write(reinterpret_cast<const char*>(flags.data()),
                     static_cast<std::streamsize>(flags.size()));
        if (!output)
            throw std::runtime_error(
              "failed writing Ghost-extra information overlay");
        std::cout << "information_overlay " << options_.output
                  << " bytes " << flags.size() + 160
                  << " source_sha256 " << options_.sourceSha256
                  << " solver_model_sha256 " << options_.modelSha256
                  << " observation_model_sha256 "
                  << options_.observationSha256
                  << " root_grouping_residual 0 conservation_residual 0\n"
                  << std::flush;
    }

    ExternalTransitionDatabase& database_;
    const LowerGhostSymbolicSidecar& lower_;
    const PackedFourTable& concrete_;
    const ExtraGeometryDomain& domain_;
    MaterialSpec material_;
    ExternalGhostExtraSolveOptions options_;
    std::unique_ptr<ExternalRobdd> bdd_;
    LowerGhostRobddImport lowerImport_;
    ExternalArray<ExternalRobdd::Id> ownerCurrent_;
    ExternalArray<ExternalRobdd::Id> ownerNext_;
    ExternalArray<ExternalRobdd::Id> observerCurrent_;
    ExternalArray<ExternalRobdd::Id> observerNext_;
    ExternalArray<ExternalRobdd::Id> domainRoots_;
    ExternalArray<std::uint8_t> visibleOwnerCurrent_;
    ExternalArray<std::uint8_t> visibleOwnerNext_;
    ExternalArray<std::uint8_t> visibleObserverCurrent_;
    ExternalArray<std::uint8_t> visibleObserverNext_;
    std::uint64_t iteration_ = 0;
    bool compactToB_ = true;
};

void solve_external_ghost_extra(
  const std::string& transitionPrefix, const MaterialSpec& material,
  const PackedFourTable& concrete, const std::string& lowerSidecar,
  const std::string& lowerSourceSha256,
  const std::string& lowerModelSha256,
  const std::string& lowerObservationSha256,
  ExternalGhostExtraSolveOptions options) {
    ExternalTransitionDatabase database(transitionPrefix, material);
    gate_external_ghost_extra_solve(transitionPrefix, database, options);
    const LowerGhostSymbolicSidecar lower(
      lowerSidecar, lowerSourceSha256, lowerModelSha256,
      lowerObservationSha256);
    const ExtraGeometryDomain domain;
    ExternalGhostExtraFixedPoint solver(
      database, lower, concrete, domain, material, std::move(options));
    solver.solve();
}

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
        if (!valid_world(decode_index(index)))
            continue;
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
        std::string compileExternalPrefix;
        std::string mergeExternalPrefix;
        std::vector<std::string> mergeShards;
        std::uint32_t mergeExpectedGeometries = 0;
        std::string solveExternalPrefix;
        ExternalGhostExtraSolveOptions solveOptions;
        solveOptions.scratch = "/tmp/kbishopghostk-exact";
        std::uint32_t compileGeometries = 0;
        std::uint32_t compileStart = 0;
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
            else if (argument == "--compile-external")
                compileExternalPrefix = value("--compile-external");
            else if (argument == "--merge-external")
                mergeExternalPrefix = value("--merge-external");
            else if (argument == "--merge-shard")
                mergeShards.push_back(value("--merge-shard"));
            else if (argument == "--merge-expected-geometries")
                mergeExpectedGeometries = static_cast<std::uint32_t>(
                  std::stoul(value("--merge-expected-geometries")));
            else if (argument == "--compile-geometries")
                compileGeometries = static_cast<std::uint32_t>(
                  std::stoul(value("--compile-geometries")));
            else if (argument == "--compile-start")
                compileStart = static_cast<std::uint32_t>(
                  std::stoul(value("--compile-start")));
            else if (argument == "--solve-external")
                solveExternalPrefix = value("--solve-external");
            else if (argument == "--solve-scratch")
                solveOptions.scratch = value("--solve-scratch");
            else if (argument == "--information-output")
                solveOptions.output = value("--information-output");
            else if (argument == "--information-source-sha256")
                solveOptions.sourceSha256 =
                  value("--information-source-sha256");
            else if (argument == "--information-model-sha256")
                solveOptions.modelSha256 =
                  value("--information-model-sha256");
            else if (argument == "--information-observation-sha256")
                solveOptions.observationSha256 =
                  value("--information-observation-sha256");
            else if (argument == "--solve-max-nodes")
                solveOptions.bddLimits.maxNodes =
                  static_cast<std::uint32_t>(std::stoul(
                    value("--solve-max-nodes")));
            else if (argument == "--solve-unique-slots")
                solveOptions.bddLimits.uniqueSlots = std::stoull(
                  value("--solve-unique-slots"));
            else if (argument == "--solve-apply-cache")
                solveOptions.bddLimits.applyCacheEntries =
                  static_cast<std::size_t>(std::stoull(
                    value("--solve-apply-cache")));
            else if (argument == "--solve-unary-cache")
                solveOptions.bddLimits.unaryCacheEntries =
                  static_cast<std::size_t>(std::stoull(
                    value("--solve-unary-cache")));
            else if (argument == "--solve-compose-cache")
                solveOptions.bddLimits.composeCacheEntries =
                  static_cast<std::size_t>(std::stoull(
                    value("--solve-compose-cache")));
            else if (argument == "--solve-compact-every")
                solveOptions.compactEvery = static_cast<std::uint32_t>(
                  std::stoul(value("--solve-compact-every")));
            else if (argument == "--measure-iterations")
                solveOptions.measureIterations = static_cast<std::uint32_t>(
                  std::stoul(value("--measure-iterations")));
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
        if (!mergeExternalPrefix.empty()) {
            merge_external_transition_shards(
              mergeExternalPrefix, std::move(mergeShards), material,
              mergeExpectedGeometries);
            return 0;
        }
        if (!compileExternalPrefix.empty()) {
            compile_external_transitions(
              compileExternalPrefix, material, compileStart,
              compileGeometries);
            return 0;
        }
        if (!solveExternalPrefix.empty()) {
            solve_external_ghost_extra(
              solveExternalPrefix, material, concrete, lowerSidecar,
              lowerSourceSha256, lowerModelSha256, lowerObservationSha256,
              std::move(solveOptions));
            return 0;
        }
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
