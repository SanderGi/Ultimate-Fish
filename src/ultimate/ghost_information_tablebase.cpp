/*
  Ultimate Fish - exact K+Ghost-v-K public-information tablebase
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  Standalone prototype.  It intentionally does not share TablebaseGenerator's
  private codec: the constants and ranking below are the legacy one-extra
  codec used by kghostk.uftb.  There is no belief cap, sampling, or lossy key.

  The observer's hidden-Ghost belief is an arbitrary nonempty subset B of the
  at most 78 legal Ghost squares in one fixed public geometry.  Explicit
  powerset closure is not viable: even after the exact four-way rectangle
  quotient it passed 50 million beliefs and 2.58 billion memberships before
  queue turnover.  The production path therefore solves Boolean predicates
  over all 80 membership bits with a canonical reduced ordered decision
  diagram (ROBDD); it never interns individual beliefs.

  Two monotonicity facts make the symbolic domain exact rather than an
  approximation.  BlackForce(B), for the uninformed observer, is downward
  closed: deleting possible worlds cannot invalidate a common winning Black
  action, and every observation image of a subset is a subset of the original
  image.  WhiteForce(actual, B), for the informed Ghost owner, is upward closed
  among beliefs containing actual: adding possible worlds can only remove
  Black common actions, while the same White action produces a superset of its
  prior non-signalling observation image.  These arguments include quiet
  hidden moves (one public observation can map one source square to several
  possible hidden destinations), reveal transitions to a visible singleton,
  and visible-singleton positions whose next quiet Ghost move hides again.

  For each observation, every child-membership bit is an OR of fixed source
  membership bits obtained from the exhaustive concrete CSR oracle.  ROBDD
  functional composition therefore implements the exact history-preserving
  belief update.  Black actions are guarded by B being a subset of the
  action's concrete legal-world mask.  Starting the monotone Bellman operator
  at false yields the exact reachability least fixed point, including cycles
  and soft locks.  Stable root identity, full Bellman re-evaluation,
  monotonicity checks, dense-root conservation, exhaustive concrete singleton
  comparison, and the oracle's exhaustive D2 certificate form the proof
  obligations for a completed solve.
*/

#include "information.h"
#include "information_solver.h"
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
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sys/resource.h>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t PlacementCount = 2 * Squares * (Squares - 1) * (Squares - 2);
constexpr std::uint32_t GhostSubstates = 2;
constexpr std::uint32_t ConcreteStateCount = PlacementCount * GhostSubstates;
constexpr std::uint32_t NoBelief = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t NoActual = std::numeric_limits<std::uint32_t>::max();

enum class Wdl : std::uint8_t { Unknown, Win, Loss, Draw };

[[nodiscard]] constexpr std::size_t color_index(Color color) {
    return static_cast<std::size_t>(color);
}

[[nodiscard]] constexpr int file_of(int square) {
    return square % Position::BoardFiles;
}

[[nodiscard]] constexpr int rank_of(int square) {
    return square / Position::BoardFiles;
}

struct LegacyState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t ghost = 0;
    bool visible = false;
};

[[nodiscard]] std::uint32_t encode_placement(const LegacyState& state) {
    if (state.whiteKing >= Squares || state.blackKing >= Squares ||
        state.ghost >= Squares || state.whiteKing == state.blackKing ||
        state.whiteKing == state.ghost || state.blackKing == state.ghost)
        throw std::runtime_error("invalid legacy K+Ghost-v-K placement");
    const std::uint32_t blackRank = state.blackKing -
      (state.blackKing > state.whiteKing ? 1u : 0u);
    const std::uint32_t low = std::min(state.whiteKing, state.blackKing);
    const std::uint32_t high = std::max(state.whiteKing, state.blackKing);
    const std::uint32_t ghostRank = state.ghost -
      (state.ghost > low ? 1u : 0u) - (state.ghost > high ? 1u : 0u);
    return (((static_cast<std::uint32_t>(state.side) * Squares + state.whiteKing) *
             (Squares - 1) + blackRank) * (Squares - 2) + ghostRank);
}

[[nodiscard]] LegacyState decode_placement(std::uint32_t index) {
    if (index >= PlacementCount)
        throw std::runtime_error("legacy placement index out of range");
    LegacyState state;
    std::uint32_t ghostRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    state.whiteKing = static_cast<std::uint8_t>(index % Squares);
    state.side = static_cast<Color>(index / Squares);
    state.blackKing = static_cast<std::uint8_t>(blackRank +
      (blackRank >= state.whiteKing ? 1u : 0u));
    const std::uint32_t low = std::min(state.whiteKing, state.blackKing);
    const std::uint32_t high = std::max(state.whiteKing, state.blackKing);
    std::uint32_t ghost = ghostRank;
    if (ghost >= low)
        ++ghost;
    if (ghost >= high)
        ++ghost;
    state.ghost = static_cast<std::uint8_t>(ghost);
    return state;
}

[[nodiscard]] LegacyState decode_index(std::uint32_t index) {
    if (index >= ConcreteStateCount)
        throw std::runtime_error("legacy concrete index out of range");
    LegacyState state = decode_placement(index / GhostSubstates);
    state.visible = index % GhostSubstates != 0;
    return state;
}

[[nodiscard]] std::uint32_t encode_index(const LegacyState& state) {
    return encode_placement(state) * GhostSubstates + (state.visible ? 1u : 0u);
}

// The 8x10 rectangle has four color-preserving geometric automorphisms for
// King/Ghost material: identity, file reflection, rank reflection, and 180°.
// Every transform is its own inverse.
[[nodiscard]] int transform_square(int square, std::uint8_t transform) {
    int file = file_of(square);
    int rank = rank_of(square);
    if (transform & 1)
        file = Position::BoardFiles - 1 - file;
    if (transform & 2)
        rank = Position::BoardRanks - 1 - rank;
    return rank * Position::BoardFiles + file;
}

[[nodiscard]] std::uint32_t transform_index(std::uint32_t index,
                                            std::uint8_t transform) {
    LegacyState state = decode_index(index);
    state.whiteKing = static_cast<std::uint8_t>(
      transform_square(state.whiteKing, transform));
    state.blackKing = static_cast<std::uint8_t>(
      transform_square(state.blackKing, transform));
    state.ghost = static_cast<std::uint8_t>(
      transform_square(state.ghost, transform));
    return encode_index(state);
}

[[nodiscard]] Position make_position(std::uint32_t index) {
    const LegacyState state = decode_index(index);
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(PieceType::King, Color::White,
                                             state.whiteKing);
    const int blackKing = position.add_piece(PieceType::King, Color::Black,
                                             state.blackKing);
    const int ghost = position.add_piece(PieceType::Ghost, Color::White,
                                         state.ghost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        ghost == Position::NoPiece)
        throw std::runtime_error("legacy codec produced invalid geometry");
    position.piece(whiteKing).moved = true;
    position.piece(blackKing).moved = true;
    position.piece(ghost).moved = true;
    position.piece(ghost).visible = state.visible;
    position.set_side_to_move(state.side);
    return position;
}

[[nodiscard]] bool hidden_next_to_enemy_king(const LegacyState& state) {
    return !state.visible &&
           std::abs(file_of(state.ghost) - file_of(state.blackKing)) <= 1 &&
           std::abs(rank_of(state.ghost) - rank_of(state.blackKing)) <= 1;
}

[[nodiscard]] bool admitted_root(std::uint32_t index) {
    const LegacyState state = decode_index(index);
    if (hidden_next_to_enemy_king(state))
        return false;
    const Position position = make_position(index);
    return !position.has_forced_action() &&
           position.ordinary_predecessor_king_safe();
}

[[nodiscard]] std::optional<std::uint32_t> same_class_index(
  const Position& position) {
    int whiteKing = Position::NoSquare;
    int blackKing = Position::NoSquare;
    int ghostSquare = Position::NoSquare;
    bool ghostVisible = false;
    int liveModels = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++liveModels;
        if (piece.type == PieceType::King && piece.color == Color::White)
            whiteKing = piece.square;
        else if (piece.type == PieceType::King && piece.color == Color::Black)
            blackKing = piece.square;
        else if (piece.type == PieceType::Ghost && piece.color == Color::White) {
            ghostSquare = piece.square;
            ghostVisible = piece.visible;
        }
        else
            return std::nullopt;
    }
    if (liveModels != 3 || whiteKing == Position::NoSquare ||
        blackKing == Position::NoSquare || ghostSquare == Position::NoSquare)
        return std::nullopt;
    LegacyState state;
    state.side = position.side_to_move();
    state.whiteKing = static_cast<std::uint8_t>(whiteKing);
    state.blackKing = static_cast<std::uint8_t>(blackKing);
    state.ghost = static_cast<std::uint8_t>(ghostSquare);
    state.visible = ghostVisible;
    return encode_index(state);
}

// Small self-contained SHA-256 implementation.  The overlay binds itself to
// the exact packed WDL input instead of trusting a filename or mtime.
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
        for (int i = 0; i < 8; ++i)
            block_[63 - i] = static_cast<std::uint8_t>(bitLength >> (8 * i));
        compress(block_.data());
        std::array<std::uint8_t, 32> digest{};
        for (std::size_t i = 0; i < state_.size(); ++i)
            for (int byte = 0; byte < 4; ++byte)
                digest[4 * i + byte] = static_cast<std::uint8_t>(
                  state_[i] >> (24 - 8 * byte));
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

    [[nodiscard]] static std::uint32_t rotr(std::uint32_t value, int count) {
        return (value >> count) | (value << (32 - count));
    }

    void compress(const std::uint8_t* block) {
        std::array<std::uint32_t, 64> words{};
        for (int i = 0; i < 16; ++i)
            words[i] = (std::uint32_t(block[4 * i]) << 24) |
                       (std::uint32_t(block[4 * i + 1]) << 16) |
                       (std::uint32_t(block[4 * i + 2]) << 8) |
                       std::uint32_t(block[4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(words[i - 15], 7) ^
                                     rotr(words[i - 15], 18) ^
                                     (words[i - 15] >> 3);
            const std::uint32_t s1 = rotr(words[i - 2], 17) ^
                                     rotr(words[i - 2], 19) ^
                                     (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + choose + K[i] + words[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + majority;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
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

struct PackedTable {
    std::vector<std::uint8_t> bytes;
    std::size_t planeOffset = 0;
    std::array<std::uint8_t, 32> sha{};

    [[nodiscard]] Wdl result(std::uint32_t index) const {
        const std::uint8_t packed = bytes.at(planeOffset + index / 4);
        return static_cast<Wdl>((packed >> (2 * (index % 4))) & 3);
    }
};

[[nodiscard]] std::uint32_t read_u32(const std::vector<std::uint8_t>& data,
                                     std::size_t offset) {
    if (offset + 4 > data.size())
        throw std::runtime_error("truncated integer in packed tablebase");
    std::uint32_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

[[nodiscard]] PackedTable load_table(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open concrete tablebase: " + path);
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0)
        throw std::runtime_error("cannot size concrete tablebase");
    PackedTable table;
    table.bytes.resize(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(table.bytes.data()), table.bytes.size());
    if (static_cast<std::size_t>(input.gcount()) != table.bytes.size())
        throw std::runtime_error("truncated concrete tablebase read");
    if (table.bytes.size() < 40 ||
        std::memcmp(table.bytes.data(), "UFTB1\0\0\0", 8) != 0)
        throw std::runtime_error("invalid concrete tablebase magic");
    const std::uint32_t version = read_u32(table.bytes, 8);
    const std::uint32_t wdlBytes = read_u32(table.bytes, 28);
    if (version < 4 || version > 6 ||
        read_u32(table.bytes, 12) != static_cast<std::uint32_t>(PieceType::Ghost) ||
        read_u32(table.bytes, 16) != ConcreteStateCount ||
        read_u32(table.bytes, 24) != GhostSubstates ||
        wdlBytes != (ConcreteStateCount + 3) / 4)
        throw std::runtime_error("concrete tablebase does not match legacy K+Ghost-v-K codec");
    table.planeOffset = 40 + (version >= 5 ? 8 : 0) + (version >= 6 ? 8 : 0);
    if (table.planeOffset + wdlBytes > table.bytes.size())
        throw std::runtime_error("truncated concrete WDL plane");
    Sha256 hasher;
    hasher.update(table.bytes.data(), table.bytes.size());
    table.sha = hasher.finish();
    return table;
}

[[nodiscard]] std::string hex_digest(const std::array<std::uint8_t, 32>& digest) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest)
        out << std::setw(2) << unsigned(byte);
    return out.str();
}

struct Successor {
    std::uint32_t belief = NoBelief;
    std::uint32_t actual = NoActual;
    bool exactWhite = false;
    bool exactBlack = false;
};

struct GeneratedNode {
    std::vector<std::uint32_t> worlds;
    Color mover = Color::White;
    bool terminal = false;
    std::vector<std::uint8_t> terminalWhite;
    bool terminalBlack = false;

    // White-to-move: offsets delimit each actual world's available actions.
    std::vector<std::uint32_t> informedOffsets;
    std::vector<Successor> informedEdges;

    // Black-to-move: common actions are stored action-major, with one
    // successor for every actual world.
    std::uint32_t commonActionCount = 0;
    std::vector<Successor> commonEdges;
};

// A K+Ghost-v-K public view fixes side-to-move, both King squares, and Ghost
// visibility.  Its only private dimension is the Ghost square.  Keeping that
// set in two words is lossless for all 80 squares and avoids retaining millions
// of heap vectors.  Equality compares the complete key after hashing, so hash
// collisions cannot merge beliefs.
struct BeliefKey {
    std::uint64_t ghostLow = 0;
    std::uint64_t ghostHigh = 0;
    std::uint8_t side = 0;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t visible = 0;

    friend bool operator==(const BeliefKey& lhs, const BeliefKey& rhs) {
        return lhs.ghostLow == rhs.ghostLow &&
               lhs.ghostHigh == rhs.ghostHigh && lhs.side == rhs.side &&
               lhs.whiteKing == rhs.whiteKing &&
               lhs.blackKing == rhs.blackKing && lhs.visible == rhs.visible;
    }
};

struct BeliefKeyHash {
    [[nodiscard]] std::size_t operator()(const BeliefKey& key) const {
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        const auto mix = [&](std::uint64_t value) {
            hash ^= value;
            hash *= 0x100000001b3ULL;
        };
        mix(key.ghostLow);
        mix(key.ghostHigh);
        mix(key.side);
        mix(key.whiteKing);
        mix(key.blackKing);
        mix(key.visible);
        return static_cast<std::size_t>(hash);
    }
};

[[nodiscard]] bool key_less(const BeliefKey& lhs, const BeliefKey& rhs) {
    if (lhs.side != rhs.side) return lhs.side < rhs.side;
    if (lhs.whiteKing != rhs.whiteKing) return lhs.whiteKing < rhs.whiteKing;
    if (lhs.blackKing != rhs.blackKing) return lhs.blackKing < rhs.blackKing;
    if (lhs.visible != rhs.visible) return lhs.visible < rhs.visible;
    if (lhs.ghostHigh != rhs.ghostHigh) return lhs.ghostHigh < rhs.ghostHigh;
    return lhs.ghostLow < rhs.ghostLow;
}

void add_ghost_square(BeliefKey& key, int square) {
    if (square < 0 || square >= Position::BoardSquares)
        throw std::runtime_error("Ghost belief square is outside the board");
    if (square < 64)
        key.ghostLow |= std::uint64_t(1) << square;
    else
        key.ghostHigh |= std::uint64_t(1) << (square - 64);
}

[[nodiscard]] bool has_ghost_square(const BeliefKey& key, int square) {
    return square < 64 ? (key.ghostLow >> square) & 1
                       : (key.ghostHigh >> (square - 64)) & 1;
}

[[nodiscard]] std::uint32_t belief_size(const BeliefKey& key) {
    return static_cast<std::uint32_t>(__builtin_popcountll(key.ghostLow) +
                                      __builtin_popcountll(key.ghostHigh));
}

[[nodiscard]] BeliefKey transform_key(const BeliefKey& source,
                                      std::uint8_t transform) {
    BeliefKey result;
    result.side = source.side;
    result.whiteKing = static_cast<std::uint8_t>(
      transform_square(source.whiteKing, transform));
    result.blackKing = static_cast<std::uint8_t>(
      transform_square(source.blackKing, transform));
    result.visible = source.visible;
    for (int square = 0; square < Position::BoardSquares; ++square)
        if (has_ghost_square(source, square))
            add_ghost_square(result, transform_square(square, transform));
    return result;
}

struct CanonicalBelief {
    BeliefKey key;
    std::uint8_t transform = 0;
};

[[nodiscard]] CanonicalBelief canonicalize(const BeliefKey& source) {
    CanonicalBelief result{source, 0};
    for (std::uint8_t transform = 1; transform < 4; ++transform) {
        const BeliefKey candidate = transform_key(source, transform);
        if (key_less(candidate, result.key))
            result = {candidate, transform};
    }
    return result;
}

struct RawChild {
    bool sameClass = false;
    std::uint32_t index = NoActual;
    bool whiteWins = false;
    bool blackWins = false;
};

struct TempEdge {
    std::uint32_t action = 0;
    RawChild child;
    Successor successor;
};

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

struct Mask80 {
    std::uint64_t low = 0;
    std::uint16_t high = 0;

    friend bool operator==(const Mask80& lhs, const Mask80& rhs) {
        return lhs.low == rhs.low && lhs.high == rhs.high;
    }
};

[[nodiscard]] bool mask_test(const Mask80& mask, unsigned square) {
    return square < 64 ? ((mask.low >> square) & 1u)
                       : ((mask.high >> (square - 64)) & 1u);
}

void mask_set(Mask80& mask, unsigned square) {
    if (square < 64)
        mask.low |= std::uint64_t(1) << square;
    else
        mask.high |= std::uint16_t(1) << (square - 64);
}

[[maybe_unused, nodiscard]] bool mask_empty(const Mask80& mask) {
    return !mask.low && !mask.high;
}

[[nodiscard]] Mask80 mask_and(Mask80 lhs, const Mask80& rhs) {
    lhs.low &= rhs.low;
    lhs.high &= rhs.high;
    return lhs;
}

[[nodiscard]] Mask80 mask_or(Mask80 lhs, const Mask80& rhs) {
    lhs.low |= rhs.low;
    lhs.high |= rhs.high;
    return lhs;
}

[[nodiscard]] bool mask_subset(const Mask80& lhs, const Mask80& rhs) {
    return !(lhs.low & ~rhs.low) &&
           !(lhs.high & static_cast<std::uint16_t>(~rhs.high));
}

[[maybe_unused, nodiscard]] std::uint32_t mask_count(const Mask80& mask) {
    return static_cast<std::uint32_t>(__builtin_popcountll(mask.low) +
                                     __builtin_popcount(mask.high));
}

[[nodiscard]] Mask80 singleton_mask(unsigned square) {
    Mask80 result;
    mask_set(result, square);
    return result;
}

// Collision-checked canonical reduced ordered decision diagram.  Node 0 is
// false and node 1 is true.  Variables use board-square order 0..79.  Every
// unique-table and computed-table hit compares its complete tuple/key.
class Robdd {
  public:
    using Id = std::uint32_t;
    static constexpr Id False = 0;
    static constexpr Id True = 1;

    Robdd() {
        nodes_.push_back({Squares, False, False});
        nodes_.push_back({Squares, True, True});
        for (unsigned square = 0; square < Squares; ++square)
            variables_[square] = make(square, False, True);
    }

    [[nodiscard]] Id variable(unsigned square) const {
        if (square >= Squares)
            throw std::runtime_error("ROBDD variable square is out of range");
        return variables_[square];
    }

    [[nodiscard]] Id logical_not(Id root) {
        if (root == False)
            return True;
        if (root == True)
            return False;
        if (const auto found = notCache_.find(root); found != notCache_.end())
            return found->second;
        const Node& node = nodes_.at(root);
        const Id result = make(node.variable, logical_not(node.low),
                               logical_not(node.high));
        notCache_.emplace(root, result);
        return result;
    }

    [[nodiscard]] Id logical_and(Id lhs, Id rhs) {
        return apply(Binary::And, lhs, rhs);
    }

    [[nodiscard]] Id logical_or(Id lhs, Id rhs) {
        return apply(Binary::Or, lhs, rhs);
    }

    [[nodiscard]] Id implication(Id premise, Id conclusion) {
        return logical_or(logical_not(premise), conclusion);
    }

    [[nodiscard]] Id ite(Id condition, Id whenTrue, Id whenFalse) {
        if (condition == True)
            return whenTrue;
        if (condition == False)
            return whenFalse;
        if (whenTrue == whenFalse)
            return whenTrue;
        return logical_or(logical_and(condition, whenTrue),
                          logical_and(logical_not(condition), whenFalse));
    }

    [[nodiscard]] Id any(const Mask80& mask) {
        Id result = False;
        for (unsigned square = 0; square < Squares; ++square)
            if (mask_test(mask, square))
                result = logical_or(result, variable(square));
        return result;
    }

    [[nodiscard]] Id subset_of(const Mask80& mask) {
        Id result = True;
        for (unsigned square = 0; square < Squares; ++square)
            if (!mask_test(mask, square))
                result = logical_and(result, logical_not(variable(square)));
        return result;
    }

    [[nodiscard]] bool evaluate(Id root, const Mask80& assignment) const {
        while (root > True) {
            const Node& node = nodes_.at(root);
            root = mask_test(assignment, node.variable) ? node.high : node.low;
        }
        return root == True;
    }

    [[nodiscard]] Id restrict_variable(Id root, unsigned variable,
                                       bool value) {
        if (root <= True)
            return root;
        const RestrictKey key{root, static_cast<std::uint8_t>(variable), value};
        if (const auto found = restrictCache_.find(key);
            found != restrictCache_.end())
            return found->second;
        const Node& node = nodes_.at(root);
        Id result;
        if (node.variable > variable)
            result = root;
        else if (node.variable == variable)
            result = value ? node.high : node.low;
        else
            result = make(node.variable,
                          restrict_variable(node.low, variable, value),
                          restrict_variable(node.high, variable, value));
        restrictCache_.emplace(key, result);
        return result;
    }

    [[nodiscard]] bool is_downward_closed(Id root) {
        Mask80 all;
        all.low = std::numeric_limits<std::uint64_t>::max();
        all.high = std::numeric_limits<std::uint16_t>::max();
        return is_downward_closed(root, all);
    }

    [[nodiscard]] bool is_downward_closed(Id root, const Mask80& variables) {
        for (unsigned variable = 0; variable < Squares; ++variable) {
            if (!mask_test(variables, variable))
                continue;
            const Id low = restrict_variable(root, variable, false);
            const Id high = restrict_variable(root, variable, true);
            if (logical_and(high, logical_not(low)) != False)
                return false;
        }
        return true;
    }

    [[nodiscard]] bool is_upward_closed(Id root) {
        Mask80 all;
        all.low = std::numeric_limits<std::uint64_t>::max();
        all.high = std::numeric_limits<std::uint16_t>::max();
        return is_upward_closed(root, all);
    }

    [[nodiscard]] bool is_upward_closed(Id root, const Mask80& variables) {
        for (unsigned variable = 0; variable < Squares; ++variable) {
            if (!mask_test(variables, variable))
                continue;
            const Id low = restrict_variable(root, variable, false);
            const Id high = restrict_variable(root, variable, true);
            if (logical_and(low, logical_not(high)) != False)
                return false;
        }
        return true;
    }

    // Substitute every child membership variable by the corresponding exact
    // OR-image formula over source membership variables.
    [[nodiscard]] Id compose(Id root, const std::array<Id, Squares>& image,
                             std::uint32_t relation) {
        const ComposeKey key{relation, root};
        if (const auto found = composeCache_.find(key);
            found != composeCache_.end())
            return found->second;
        const Id result = compose_uncached(root, image, relation);
        composeCache_.emplace(key, result);
        return result;
    }

    void clear_transient_caches() {
        applyCache_.clear();
        restrictCache_.clear();
        composeCache_.clear();
        // Complement is intrinsic to a node and remains valid forever.
    }

    [[nodiscard]] std::size_t node_count() const { return nodes_.size(); }

  private:
    struct Node {
        std::uint32_t variable = Squares;
        Id low = False;
        Id high = False;
    };

    struct NodeKey {
        std::uint32_t variable = 0;
        Id low = False;
        Id high = False;
        friend bool operator==(const NodeKey& lhs, const NodeKey& rhs) {
            return lhs.variable == rhs.variable && lhs.low == rhs.low &&
                   lhs.high == rhs.high;
        }
    };

    struct NodeKeyHash {
        std::size_t operator()(const NodeKey& key) const noexcept {
            std::uint64_t hash = key.variable + 0x9e3779b97f4a7c15ULL;
            hash ^= std::uint64_t(key.low) + (hash << 6) + (hash >> 2);
            hash ^= std::uint64_t(key.high) + (hash << 6) + (hash >> 2);
            return static_cast<std::size_t>(hash);
        }
    };

    enum class Binary : std::uint8_t { And, Or };

    struct ApplyKey {
        Id lhs = False;
        Id rhs = False;
        Binary operation = Binary::And;
        friend bool operator==(const ApplyKey& lhs, const ApplyKey& rhs) {
            return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs &&
                   lhs.operation == rhs.operation;
        }
    };

    struct ApplyKeyHash {
        std::size_t operator()(const ApplyKey& key) const noexcept {
            return (std::uint64_t(key.lhs) << 32 ^ key.rhs) *
                     0x9e3779b97f4a7c15ULL +
                   static_cast<std::uint8_t>(key.operation);
        }
    };

    struct RestrictKey {
        Id root = False;
        std::uint8_t variable = 0;
        bool value = false;
        friend bool operator==(const RestrictKey& lhs,
                               const RestrictKey& rhs) {
            return lhs.root == rhs.root && lhs.variable == rhs.variable &&
                   lhs.value == rhs.value;
        }
    };

    struct RestrictKeyHash {
        std::size_t operator()(const RestrictKey& key) const noexcept {
            return (std::uint64_t(key.root) << 8) ^
                   (std::uint64_t(key.variable) << 1) ^ key.value;
        }
    };

    struct ComposeKey {
        std::uint32_t relation = 0;
        Id root = False;
        friend bool operator==(const ComposeKey& lhs, const ComposeKey& rhs) {
            return lhs.relation == rhs.relation && lhs.root == rhs.root;
        }
    };

    struct ComposeKeyHash {
        std::size_t operator()(const ComposeKey& key) const noexcept {
            return (std::uint64_t(key.relation) << 32 ^ key.root) *
                   0x9e3779b97f4a7c15ULL;
        }
    };

    [[nodiscard]] Id make(std::uint32_t variable, Id low, Id high) {
        if (low == high)
            return low;
        const NodeKey key{variable, low, high};
        if (const auto found = unique_.find(key); found != unique_.end())
            return found->second;
        if (nodes_.size() >= std::numeric_limits<Id>::max())
            throw std::runtime_error("ROBDD node identifier exceeds 32 bits");
        const Id result = static_cast<Id>(nodes_.size());
        nodes_.push_back({variable, low, high});
        if (!unique_.emplace(key, result).second)
            throw std::runtime_error("ROBDD unique-table insertion failed");
        return result;
    }

    [[nodiscard]] std::uint32_t top_variable(Id root) const {
        return root <= True ? Squares : nodes_.at(root).variable;
    }

    [[nodiscard]] Id apply(Binary operation, Id lhs, Id rhs) {
        if (lhs > rhs)
            std::swap(lhs, rhs);
        if (operation == Binary::And) {
            if (lhs == False || rhs == False)
                return False;
            if (lhs == True)
                return rhs;
            if (lhs == rhs)
                return lhs;
        }
        else {
            if (rhs == True)
                return True;
            if (lhs == False)
                return rhs;
            if (lhs == rhs)
                return lhs;
        }
        const ApplyKey key{lhs, rhs, operation};
        if (const auto found = applyCache_.find(key); found != applyCache_.end())
            return found->second;
        const std::uint32_t variable = std::min(top_variable(lhs),
                                                top_variable(rhs));
        const auto branch = [&](Id root, bool high) {
            if (top_variable(root) != variable)
                return root;
            return high ? nodes_[root].high : nodes_[root].low;
        };
        const Id result = make(variable,
          apply(operation, branch(lhs, false), branch(rhs, false)),
          apply(operation, branch(lhs, true), branch(rhs, true)));
        applyCache_.emplace(key, result);
        return result;
    }

    [[nodiscard]] Id compose_uncached(
      Id root, const std::array<Id, Squares>& image, std::uint32_t relation) {
        if (root <= True)
            return root;
        const ComposeKey key{relation, root};
        if (const auto found = composeCache_.find(key);
            found != composeCache_.end())
            return found->second;
        const Node& node = nodes_.at(root);
        const Id low = compose_uncached(node.low, image, relation);
        const Id high = compose_uncached(node.high, image, relation);
        const Id result = ite(image[node.variable], high, low);
        composeCache_.emplace(key, result);
        return result;
    }

    std::vector<Node> nodes_;
    std::array<Id, Squares> variables_{};
    std::unordered_map<NodeKey, Id, NodeKeyHash> unique_;
    std::unordered_map<ApplyKey, Id, ApplyKeyHash> applyCache_;
    std::unordered_map<Id, Id> notCache_;
    std::unordered_map<RestrictKey, Id, RestrictKeyHash> restrictCache_;
    std::unordered_map<ComposeKey, Id, ComposeKeyHash> composeCache_;
};

void robdd_self_test() {
    Robdd bdd;
    const auto x0 = bdd.variable(0);
    const auto x1 = bdd.variable(1);
    const auto upward = bdd.logical_or(x0, x1);
    const auto downward = bdd.logical_not(bdd.logical_and(x0, x1));
    if (!bdd.is_upward_closed(upward) || bdd.is_downward_closed(upward) ||
        !bdd.is_downward_closed(downward) || bdd.is_upward_closed(downward))
        throw std::runtime_error("ROBDD monotonicity self-test failed");
    std::array<Robdd::Id, Squares> image{};
    image.fill(Robdd::False);
    image[0] = x1;
    image[1] = x0;
    const auto swapped = bdd.compose(upward, image, 1);
    if (swapped != upward)
        throw std::runtime_error("ROBDD composition self-test failed");
    Mask80 zero;
    Mask80 one = singleton_mask(0);
    Mask80 both = mask_or(one, singleton_mask(1));
    if (bdd.evaluate(upward, zero) || !bdd.evaluate(upward, one) ||
        !bdd.evaluate(downward, one) || bdd.evaluate(downward, both))
        throw std::runtime_error("ROBDD evaluation self-test failed");
}

// Exhaustive three-world oracle for the symbolic information-game kernel.
// H is a non-signalling hide observation: one source world can leave two
// hidden destinations possible.  R is a reveal observation: every compatible
// source collapses to one visible destination.  The mutually recursive nodes
// contain a cycle; a Black common-action guard and its negation exercise both
// observer force and informed-owner soft-lock semantics.  We solve all 2^3
// beliefs explicitly and as ROBDDs, then compare every value.
void symbolic_tiny_powerset_self_test() {
    constexpr unsigned Worlds = 3;
    const auto mask_from_bits = [](unsigned bits) {
        Mask80 mask;
        for (unsigned world = 0; world < Worlds; ++world)
            if ((bits >> world) & 1u)
                mask_set(mask, world);
        return mask;
    };
    const auto bits_from_mask = [](const Mask80& mask) {
        unsigned bits = 0;
        for (unsigned world = 0; world < Worlds; ++world)
            bits |= unsigned(mask_test(mask, world)) << world;
        return bits;
    };
    const auto hide_image = [](unsigned belief) {
        unsigned child = 0;
        if (belief & 0b011) child |= 0b001;
        if (belief & 0b101) child |= 0b010;
        return child;
    };
    const auto reveal_image = [](unsigned belief) {
        return belief ? 0b001u : 0u;
    };
    if (hide_image(0b001) != 0b011 || reveal_image(0b110) != 0b001)
        throw std::runtime_error("tiny hide/reveal image oracle is malformed");

    using Truth = std::array<std::uint8_t, 8>;
    std::array<Truth, 2> explicitBlack{};
    std::array<std::array<Truth, Worlds>, 2> explicitWhite{};
    unsigned explicitIterations = 0;
    for (;;) {
        ++explicitIterations;
        auto nextBlack = explicitBlack;
        auto nextWhite = explicitWhite;
        for (unsigned belief = 0; belief < 8; ++belief) {
            const bool common0 = (belief & ~0b011u) == 0;
            const bool common1 = (belief & ~0b101u) == 0;
            nextBlack[0][belief] =
              ((belief & ~0b001u) == 0) ||
              (common0 && explicitBlack[1][hide_image(belief)]);
            nextBlack[1][belief] =
              ((belief & ~0b010u) == 0) ||
              (common1 && explicitBlack[0][reveal_image(belief)]);
            for (unsigned actual = 0; actual < Worlds; ++actual) {
                if (!((belief >> actual) & 1u)) {
                    nextWhite[0][actual][belief] = 0;
                    nextWhite[1][actual][belief] = 0;
                    continue;
                }
                const unsigned hideActual = actual ? 1u : 0u;
                nextWhite[0][actual][belief] = !common0 ||
                  explicitWhite[1][hideActual][hide_image(belief)];
                nextWhite[1][actual][belief] = actual == 2 ||
                  explicitWhite[0][0][reveal_image(belief)];
            }
        }
        if (nextBlack == explicitBlack && nextWhite == explicitWhite)
            break;
        explicitBlack = nextBlack;
        explicitWhite = nextWhite;
    }
    if (explicitIterations < 3 || !explicitWhite[0][0][0b101])
        throw std::runtime_error(
          "tiny powerset cycle/soft-lock oracle did not exercise its fixtures");

    Robdd bdd;
    Mask80 universe = mask_from_bits(0b111);
    const Robdd::Id domain = bdd.subset_of(universe);
    std::array<Robdd::Id, Squares> hide{};
    std::array<Robdd::Id, Squares> reveal{};
    hide.fill(Robdd::False);
    reveal.fill(Robdd::False);
    hide[0] = bdd.logical_or(bdd.variable(0), bdd.variable(1));
    hide[1] = bdd.logical_or(bdd.variable(0), bdd.variable(2));
    reveal[0] = bdd.logical_or(
      bdd.variable(0), bdd.logical_or(bdd.variable(1), bdd.variable(2)));
    std::array<Robdd::Id, 2> black{{Robdd::False, Robdd::False}};
    std::array<std::array<Robdd::Id, Worlds>, 2> white{};
    for (auto& roots : white)
        roots.fill(Robdd::False);
    unsigned symbolicIterations = 0;
    for (;;) {
        ++symbolicIterations;
        const Robdd::Id common0 = bdd.logical_and(
          domain, bdd.subset_of(mask_from_bits(0b011)));
        const Robdd::Id common1 = bdd.logical_and(
          domain, bdd.subset_of(mask_from_bits(0b101)));
        std::array<Robdd::Id, 2> nextBlack;
        nextBlack[0] = bdd.logical_and(domain, bdd.logical_or(
          bdd.subset_of(mask_from_bits(0b001)),
          bdd.logical_and(common0, bdd.compose(black[1], hide, 101))));
        nextBlack[1] = bdd.logical_and(domain, bdd.logical_or(
          bdd.subset_of(mask_from_bits(0b010)),
          bdd.logical_and(common1, bdd.compose(black[0], reveal, 102))));
        auto nextWhite = white;
        for (unsigned actual = 0; actual < Worlds; ++actual) {
            const unsigned hideActual = actual ? 1u : 0u;
            nextWhite[0][actual] = bdd.logical_and(
              domain, bdd.logical_and(bdd.variable(actual), bdd.logical_or(
                bdd.logical_not(common0),
                bdd.compose(white[1][hideActual], hide, 103 + hideActual))));
            nextWhite[1][actual] = bdd.logical_and(
              domain, bdd.logical_and(bdd.variable(actual), bdd.logical_or(
                actual == 2 ? Robdd::True : Robdd::False,
                bdd.compose(white[0][0], reveal, 110))));
        }
        if (nextBlack == black && nextWhite == white)
            break;
        black = nextBlack;
        white = nextWhite;
    }
    if (symbolicIterations != explicitIterations)
        throw std::runtime_error(
          "tiny ROBDD and powerset fixed points converged at different ranks");
    for (unsigned belief = 0; belief < 8; ++belief) {
        const Mask80 assignment = mask_from_bits(belief);
        if (bits_from_mask(assignment) != belief)
            throw std::runtime_error("tiny belief mask round trip failed");
        for (unsigned node = 0; node < 2; ++node) {
            if (bdd.evaluate(black[node], assignment) !=
                bool(explicitBlack[node][belief]))
                throw std::runtime_error(
                  "tiny BlackForce ROBDD differs from exhaustive powerset");
            if (!bdd.is_downward_closed(black[node], universe))
                throw std::runtime_error(
                  "tiny BlackForce is not downward closed");
            for (unsigned actual = 0; actual < Worlds; ++actual) {
                if (bdd.evaluate(white[node][actual], assignment) !=
                    bool(explicitWhite[node][actual][belief]))
                    throw std::runtime_error(
                      "tiny WhiteForce ROBDD differs from exhaustive powerset");
                if (!bdd.is_upward_closed(white[node][actual], universe))
                    throw std::runtime_error(
                      "tiny WhiteForce is not upward closed");
            }
        }
    }
}

struct ConcreteEdge {
    std::uint32_t observation = 0;
    std::uint32_t action = 0;
    std::uint32_t child = NoActual;
    std::uint16_t moveCode = 0;
    std::uint8_t exact = 0;  // bit 0 White win; bit 1 Black win
};

[[nodiscard]] std::uint16_t move_code(const Move& move) {
    if (move.kind != MoveKind::Normal || move.from >= Position::BoardSquares ||
        move.to >= Position::BoardSquares || move.auxiliary != 0 ||
        move.promotion != PieceType::Count)
        throw std::runtime_error(
          "K+Ghost-v-K symmetry proof encountered a non-ordinary move");
    return static_cast<std::uint16_t>(move.from * Position::BoardSquares + move.to);
}

[[nodiscard]] std::uint16_t transform_move_code(std::uint16_t code,
                                                std::uint8_t transform) {
    const int from = code / Position::BoardSquares;
    const int to = code % Position::BoardSquares;
    return static_cast<std::uint16_t>(
      transform_square(from, transform) * Position::BoardSquares +
      transform_square(to, transform));
}

// Current information.cpp strings are interned with std::string equality,
// never by a digest.  Once every concrete transition has an ID, the strings
// can be discarded: equality of IDs is then exactly equality of the full
// canonical action/observation strings which assigned them.
class ConcreteTransitionOracle {
  public:
    ConcreteTransitionOracle() : offsets_(ConcreteStateCount + 1, 0),
                                 terminal_(ConcreteStateCount, 0),
                                 decision_(ConcreteStateCount, NoActual) {
        const auto started = std::chrono::steady_clock::now();
        build_decision_classes();
        std::array<std::unordered_map<std::string, std::uint32_t>, 2> observations;
        std::array<std::unordered_map<std::string, std::uint32_t>, 2> actions;
        std::uint64_t observationClasses = 0;
        std::uint64_t actionClasses = 0;
        std::uint32_t maxObservationsPerView = 0;
        std::uint32_t maxActionsPerView = 0;
        std::array<int, 3> previousGeometry{{-1, -1, -1}};
        std::vector<std::uint8_t> verifyMask(ConcreteStateCount, 0);
        for (std::uint32_t index = 0; index < 4096; ++index)
            verifyMask[index] = 1;
        for (const std::uint32_t base : {
               0u, 2u, ConcreteStateCount / 2,
               ConcreteStateCount / 2 + 2, ConcreteStateCount - 2}) {
            verifyMask[base] = 1;
            verifyMask[base + 1] = 1;
        }
        std::uint32_t sample = 0x9e3779b9u;
        for (int count = 0; count < 8192; ++count) {
            sample = sample * 1664525u + 1013904223u;
            verifyMask[sample % ConcreteStateCount] = 1;
        }
        std::uint64_t verifiedStates = 0;
        std::uint64_t verifiedEdges = 0;
        const DisclosureContext blackView{Color::Black, false};
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            const LegacyState sourceState = decode_index(index);
            const std::array<int, 3> geometry{{
              static_cast<int>(sourceState.side), sourceState.whiteKing,
              sourceState.blackKing}};
            if (geometry != previousGeometry) {
                if (previousGeometry[0] >= 0)
                    for (std::size_t visible = 0; visible < 2; ++visible) {
                        observationClasses += observations[visible].size();
                        actionClasses += actions[visible].size();
                        maxObservationsPerView = std::max<std::uint32_t>(
                          maxObservationsPerView, observations[visible].size());
                        maxActionsPerView = std::max<std::uint32_t>(
                          maxActionsPerView, actions[visible].size());
                        observations[visible].clear();
                        actions[visible].clear();
                    }
                previousGeometry = geometry;
            }
            auto& localObservations = observations[sourceState.visible ? 1 : 0];
            auto& localActions = actions[sourceState.visible ? 1 : 0];
            Position position = make_position(index);
            if (position.game_over()) {
                terminal_[index] = 4;
                const auto winner = position.winner();
                terminal_[index] |= winner && *winner == Color::White ? 1 : 0;
                terminal_[index] |= winner && *winner == Color::Black ? 2 : 0;
            }
            else {
                for (const Move& move : position.legal_moves()) {
                    Position child = position;
                    Undo undo;
                    if (!child.make_move(move, undo))
                        throw std::runtime_error(
                          "legal concrete-oracle edge failed make_move");
                    std::string observation = transition_observation_key(
                      position, move, child, blackView);
                    const std::string action = position.move_to_string(move);
                    const auto intern_string = [](auto& interner,
                                                  const std::string& text) {
                        if (const auto found = interner.find(text);
                            found != interner.end())
                            return found->second;
                        if (interner.size() >= NoActual)
                            throw std::runtime_error(
                              "concrete transition string ID exceeds 32-bit domain");
                        const std::uint32_t id =
                          static_cast<std::uint32_t>(interner.size());
                        if (!interner.emplace(text, id).second)
                            throw std::runtime_error("string interner insertion failed");
                        return id;
                    };
                    ConcreteEdge edge;
                    edge.action = intern_string(localActions, action);
                    edge.moveCode = move_code(move);
                    if (const auto sameClass = same_class_index(child)) {
                        edge.child = *sameClass;
                        const LegacyState childState = decode_index(*sameClass);
                        if (childState.side == Color::Black &&
                            !(terminal_[*sameClass] & 4)) {
                            if (decision_[*sameClass] == NoActual)
                                throw std::runtime_error(
                                  "live Black child has no decision observation");
                            observation += "|blackDecision=" +
                              std::to_string(decision_[*sameClass]);
                        }
                    }
                    else {
                        if (!child.game_over())
                            throw std::runtime_error(
                              "concrete Ghost edge escaped its closed material class");
                        const auto winner = child.winner();
                        edge.exact |= winner && *winner == Color::White ? 1 : 0;
                        edge.exact |= winner && *winner == Color::Black ? 2 : 0;
                    }
                    edge.observation = intern_string(localObservations, observation);
                    edges_.push_back(edge);
                    if (edges_.size() >= NoActual)
                        throw std::runtime_error(
                          "concrete transition oracle exceeds 32-bit edge offsets");
                }
            }
            offsets_[index + 1] = static_cast<std::uint32_t>(edges_.size());
            if (verifyMask[index]) {
                const auto moves = position.game_over() ? std::vector<Move>{}
                                                        : position.legal_moves();
                const ConcreteEdge* cached = begin(index);
                if (static_cast<std::size_t>(end(index) - cached) != moves.size())
                    throw std::runtime_error(
                      "concrete transition oracle self-test edge-count mismatch");
                for (const Move& move : moves) {
                    Position child = position;
                    Undo undo;
                    if (!child.make_move(move, undo))
                        throw std::runtime_error(
                          "concrete transition oracle self-test move failed");
                    std::string observation = transition_observation_key(
                      position, move, child, blackView);
                    const std::string action = position.move_to_string(move);
                    if (const auto childIndex = same_class_index(child)) {
                        const LegacyState childState = decode_index(*childIndex);
                        if (childState.side == Color::Black &&
                            !(terminal_[*childIndex] & 4))
                            observation += "|blackDecision=" +
                              std::to_string(decision_.at(*childIndex));
                    }
                    const auto observationId = localObservations.find(observation);
                    const auto actionId = localActions.find(action);
                    if (observationId == localObservations.end() ||
                        actionId == localActions.end() ||
                        cached->observation != observationId->second ||
                        cached->action != actionId->second)
                        throw std::runtime_error(
                          "concrete transition oracle self-test string-ID mismatch");
                    std::uint32_t childIndex = NoActual;
                    std::uint8_t exact = 0;
                    if (const auto sameClass = same_class_index(child))
                        childIndex = *sameClass;
                    else {
                        const auto winner = child.winner();
                        exact |= winner && *winner == Color::White ? 1 : 0;
                        exact |= winner && *winner == Color::Black ? 2 : 0;
                    }
                    if (cached->child != childIndex || cached->exact != exact)
                        throw std::runtime_error(
                          "concrete transition oracle self-test child mismatch");
                    ++cached;
                    ++verifiedEdges;
                }
                ++verifiedStates;
            }
            if ((index + 1) % 100'000 == 0) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - started).count();
                std::cout << "information_oracle concrete " << index + 1 << '/'
                          << ConcreteStateCount << " edges " << edges_.size()
                          << " local_actions " << localActions.size()
                          << " local_observations " << localObservations.size()
                          << " peak_rss_bytes " << peak_rss_bytes()
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        for (std::size_t visible = 0; visible < 2; ++visible) {
            observationClasses += observations[visible].size();
            actionClasses += actions[visible].size();
            maxObservationsPerView = std::max<std::uint32_t>(
              maxObservationsPerView, observations[visible].size());
            maxActionsPerView = std::max<std::uint32_t>(
              maxActionsPerView, actions[visible].size());
        }
        actionCount_ = actionClasses;
        observationCount_ = observationClasses;

        // Exhaustive D2-equivariance certificate over the complete dense
        // concrete domain and all cached edges.  Action and observation IDs
        // are local to a fixed source public geometry; the two-way maps prove
        // that each of the four transforms preserves both equivalence
        // partitions, not merely individual moves.
        std::uint64_t symmetryStates = 0;
        std::uint64_t symmetryEdges = 0;
        std::array<int, 3> priorSource{{-1, -1, -1}};
        using IdMap = std::unordered_map<std::uint32_t, std::uint32_t>;
        using TransformMaps = std::array<IdMap, 4>;
        std::array<TransformMaps, 2> actionForward, actionReverse;
        std::array<TransformMaps, 2> observationForward, observationReverse;
        std::array<TransformMaps, 2> decisionForward, decisionReverse;
        const auto bind_id = [](IdMap& forward, IdMap& reverse,
                                std::uint32_t from, std::uint32_t to,
                                const char* kind) {
            const auto [fit, inserted] = forward.emplace(from, to);
            if (!inserted && fit->second != to)
                throw std::runtime_error(std::string("D2 ") + kind +
                                         " map is not a function");
            const auto [rit, reverseInserted] = reverse.emplace(to, from);
            if (!reverseInserted && rit->second != from)
                throw std::runtime_error(std::string("D2 ") + kind +
                                         " map is not injective");
        };
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            const LegacyState source = decode_index(index);
            const std::array<int, 3> geometry{{
              static_cast<int>(source.side), source.whiteKing,
              source.blackKing}};
            if (geometry != priorSource) {
                for (std::size_t visible = 0; visible < 2; ++visible)
                    for (std::uint8_t transform = 0; transform < 4; ++transform) {
                        actionForward[visible][transform].clear();
                        actionReverse[visible][transform].clear();
                        observationForward[visible][transform].clear();
                        observationReverse[visible][transform].clear();
                        decisionForward[visible][transform].clear();
                        decisionReverse[visible][transform].clear();
                    }
                priorSource = geometry;
            }
            const std::size_t visible = source.visible ? 1 : 0;
            for (std::uint8_t transform = 0; transform < 4; ++transform) {
                const std::uint32_t transformedIndex =
                  transform_index(index, transform);
                if (terminal_[index] != terminal_[transformedIndex])
                    throw std::runtime_error(
                      "D2 symmetry changed a concrete terminal result");
                if ((decision_[index] == NoActual) !=
                    (decision_[transformedIndex] == NoActual))
                    throw std::runtime_error(
                      "D2 symmetry changed decision-observation availability");
                if (decision_[index] != NoActual)
                    bind_id(decisionForward[visible][transform],
                            decisionReverse[visible][transform], decision_[index],
                            decision_[transformedIndex], "decision observation");
                const ConcreteEdge* transformedBegin = begin(transformedIndex);
                const ConcreteEdge* transformedEnd = end(transformedIndex);
                if (end(index) - begin(index) != transformedEnd - transformedBegin)
                    throw std::runtime_error(
                      "D2 symmetry changed the legal action count");
                for (const ConcreteEdge* edge = begin(index);
                     edge != end(index); ++edge) {
                    const std::uint16_t wanted = transform_move_code(
                      edge->moveCode, transform);
                    const ConcreteEdge* paired = std::find_if(
                      transformedBegin, transformedEnd,
                      [&](const ConcreteEdge& candidate) {
                          return candidate.moveCode == wanted;
                      });
                    if (paired == transformedEnd)
                        throw std::runtime_error(
                          "D2 symmetry lost a transformed legal action");
                    if (edge->exact != paired->exact ||
                        (edge->child == NoActual) != (paired->child == NoActual) ||
                        (edge->child != NoActual &&
                         transform_index(edge->child, transform) != paired->child))
                        throw std::runtime_error(
                          "D2 symmetry changed a transformed successor");
                    bind_id(actionForward[visible][transform],
                            actionReverse[visible][transform],
                            edge->action, paired->action, "action");
                    bind_id(observationForward[visible][transform],
                            observationReverse[visible][transform], edge->observation,
                            paired->observation, "observation");
                    ++symmetryEdges;
                }
                ++symmetryStates;
            }
        }
        // observations/actions die here, before the belief closure allocates
        // its interner.  The CSR retains their exact equivalence classes.
        std::cout << "information_oracle_complete concrete " << ConcreteStateCount
                  << " edges " << edges_.size()
                  << " action_classes " << actionCount_
                  << " observation_classes " << observationCount_
                  << " max_actions_per_view " << maxActionsPerView
                  << " max_observations_per_view " << maxObservationsPerView
                  << " verified_states " << verifiedStates
                  << " verified_edges " << verifiedEdges
                  << " symmetry_states " << symmetryStates
                  << " symmetry_edges " << symmetryEdges
                  << " peak_rss_bytes " << peak_rss_bytes() << '\n' << std::flush;
    }

    [[nodiscard]] const ConcreteEdge* begin(std::uint32_t state) const {
        return edges_.data() + offsets_.at(state);
    }
    [[nodiscard]] const ConcreteEdge* end(std::uint32_t state) const {
        return edges_.data() + offsets_.at(state + 1);
    }
    [[nodiscard]] std::uint8_t terminal(std::uint32_t state) const {
        return terminal_.at(state);
    }
    [[nodiscard]] std::uint32_t decision(std::uint32_t state) const {
        return decision_.at(state);
    }

  private:
    void build_decision_classes() {
        const DisclosureContext blackView{Color::Black, false};
        std::array<std::unordered_map<std::string, std::uint32_t>, 2> local;
        std::array<int, 3> previousGeometry{{-1, -1, -1}};
        std::uint64_t classes = 0;
        std::uint32_t maximum = 0;
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            const LegacyState state = decode_index(index);
            const std::array<int, 3> geometry{{static_cast<int>(state.side),
              state.whiteKing, state.blackKing}};
            if (geometry != previousGeometry) {
                for (auto& visible : local) {
                    classes += visible.size();
                    maximum = std::max<std::uint32_t>(maximum, visible.size());
                    visible.clear();
                }
                previousGeometry = geometry;
            }
            Position position = make_position(index);
            if (position.game_over()) {
                terminal_[index] = 4;
                const auto winner = position.winner();
                terminal_[index] |= winner && *winner == Color::White ? 1 : 0;
                terminal_[index] |= winner && *winner == Color::Black ? 2 : 0;
            }
            if (state.side != Color::Black || position.game_over())
                continue;
            const std::string key = decision_observation_key(position, blackView);
            auto& visible = local[state.visible ? 1 : 0];
            const auto [found, inserted] = visible.emplace(
              key, static_cast<std::uint32_t>(visible.size()));
            decision_[index] = found->second;
            if (!inserted && found->first != key)
                throw std::runtime_error(
                  "decision-observation interner collision error");
        }
        for (const auto& visible : local) {
            classes += visible.size();
            maximum = std::max<std::uint32_t>(maximum, visible.size());
        }
        std::cout << "information_decision_oracle classes " << classes
                  << " max_per_geometry " << maximum << '\n' << std::flush;
    }

    std::vector<std::uint32_t> offsets_;
    std::vector<ConcreteEdge> edges_;
    std::vector<std::uint8_t> terminal_;
    std::vector<std::uint32_t> decision_;
    std::uint32_t actionCount_ = 0;
    std::uint32_t observationCount_ = 0;
};

struct PublicGeometry {
    std::uint8_t side = 0;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t visible = 0;

    friend bool operator==(const PublicGeometry& lhs,
                           const PublicGeometry& rhs) {
        return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
               lhs.blackKing == rhs.blackKing && lhs.visible == rhs.visible;
    }
};

[[nodiscard]] bool geometry_less(const PublicGeometry& lhs,
                                 const PublicGeometry& rhs) {
    return std::tie(lhs.side, lhs.whiteKing, lhs.blackKing, lhs.visible) <
           std::tie(rhs.side, rhs.whiteKing, rhs.blackKing, rhs.visible);
}

[[nodiscard]] std::uint32_t geometry_code(const PublicGeometry& geometry) {
    return geometry.side | (std::uint32_t(geometry.whiteKing) << 1) |
           (std::uint32_t(geometry.blackKing) << 8) |
           (std::uint32_t(geometry.visible) << 15);
}

[[nodiscard]] PublicGeometry transform_geometry(PublicGeometry geometry,
                                                std::uint8_t transform) {
    geometry.whiteKing = static_cast<std::uint8_t>(
      transform_square(geometry.whiteKing, transform));
    geometry.blackKing = static_cast<std::uint8_t>(
      transform_square(geometry.blackKing, transform));
    return geometry;
}

struct CanonicalGeometry {
    PublicGeometry geometry;
    std::uint8_t transform = 0;
};

[[nodiscard]] CanonicalGeometry canonical_geometry(
  const PublicGeometry& source) {
    CanonicalGeometry result{source, 0};
    for (std::uint8_t transform = 1; transform < 4; ++transform) {
        const PublicGeometry candidate = transform_geometry(source, transform);
        if (geometry_less(candidate, result.geometry))
            result = {candidate, transform};
    }
    return result;
}

[[nodiscard]] PublicGeometry public_geometry(const LegacyState& state) {
    return {static_cast<std::uint8_t>(state.side), state.whiteKing,
            state.blackKing, static_cast<std::uint8_t>(state.visible)};
}

[[maybe_unused, nodiscard]] Mask80 transform_mask(
  const Mask80& source, std::uint8_t transform) {
    Mask80 result;
    for (unsigned square = 0; square < Squares; ++square)
        if (mask_test(source, square))
            mask_set(result, transform_square(square, transform));
    return result;
}

struct SymbolicRelationEntry {
    Mask80 sources;
    std::uint8_t childSquare = 0;
};

struct SymbolicRelation {
    std::uint32_t observation = 0;
    std::uint32_t childGeometry = NoBelief;
    std::uint32_t childStratum = NoBelief;
    Mask80 possibleSources;
    Mask80 badBlackSources;
    std::vector<SymbolicRelationEntry> image;
    bool sameClass = false;
    bool sawExternal = false;
    bool childTerminalKnown = false;
    bool childTerminal = false;
};

struct SymbolicEdge {
    std::uint32_t relation = NoBelief;
    std::uint32_t action = 0;
    std::uint8_t childActual = 0;
    std::uint8_t exact = 0;
    bool sameClass = false;
};

struct SymbolicActionObservation {
    std::uint32_t relation = NoBelief;
    Mask80 possibleSources;
};

struct SymbolicAction {
    std::uint32_t publicId = 0;
    Mask80 legalSources;
    std::vector<SymbolicActionObservation> observations;
};

struct SymbolicGeometryModel {
    PublicGeometry geometry;
    Mask80 liveSources;
    Mask80 terminalSources;
    Mask80 terminalWhiteSources;
    Mask80 terminalBlackSources;
    std::array<std::vector<SymbolicEdge>, Squares> edges;
    std::array<std::uint32_t, Squares> actualStratum{};
    std::vector<std::uint32_t> strata;
    std::vector<std::uint32_t> relations;
    std::vector<SymbolicAction> actions;
};

struct SymbolicStratum {
    std::uint32_t geometry = NoBelief;
    Mask80 liveSources;
};

class GhostSymbolicModel {
  public:
    explicit GhostSymbolicModel(const ConcreteTransitionOracle& oracle)
      : oracle_(oracle) {
        build_geometries();
        build_transitions();
        build_fresh_roots();
    }

    [[nodiscard]] const std::vector<SymbolicGeometryModel>& geometries() const {
        return geometries_;
    }
    [[nodiscard]] const std::vector<SymbolicRelation>& relations() const {
        return relations_;
    }
    [[nodiscard]] const std::vector<SymbolicStratum>& strata() const {
        return strata_;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& dense_geometry() const {
        return denseGeometry_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& dense_actual() const {
        return denseActual_;
    }
    [[nodiscard]] const std::vector<Mask80>& dense_root_mask() const {
        return denseRootMask_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& admitted() const {
        return admitted_;
    }
    [[nodiscard]] std::pair<std::uint32_t, std::uint8_t> locate(
      std::uint32_t concrete) const {
        const LegacyState state = decode_index(concrete);
        const CanonicalGeometry canonical = canonical_geometry(
          public_geometry(state));
        return {find_geometry(canonical.geometry),
                static_cast<std::uint8_t>(
                  transform_square(state.ghost, canonical.transform))};
    }
    [[nodiscard]] std::uint8_t terminal(std::uint32_t concrete) const {
        return oracle_.terminal(concrete);
    }

    [[nodiscard]] std::array<Robdd::Id, Squares> image_formulas(
      Robdd& bdd, std::uint32_t relation) const {
        std::array<Robdd::Id, Squares> result{};
        result.fill(Robdd::False);
        for (const SymbolicRelationEntry& entry : relations_.at(relation).image)
            result[entry.childSquare] = bdd.any(entry.sources);
        return result;
    }

  private:
    [[nodiscard]] std::uint32_t find_geometry(
      const PublicGeometry& geometry) const {
        const auto found = geometryByCode_.find(geometry_code(geometry));
        if (found == geometryByCode_.end() ||
            !(geometries_[found->second].geometry == geometry))
            throw std::runtime_error("symbolic child geometry is not interned");
        return found->second;
    }

    void build_geometries() {
        for (std::uint8_t side = 0; side < 2; ++side)
            for (std::uint8_t whiteKing = 0; whiteKing < Squares; ++whiteKing)
                for (std::uint8_t blackKing = 0; blackKing < Squares; ++blackKing) {
                    if (whiteKing == blackKing)
                        continue;
                    for (std::uint8_t visible = 0; visible < 2; ++visible) {
                        const CanonicalGeometry canonical = canonical_geometry(
                          {side, whiteKing, blackKing, visible});
                        const std::uint32_t code = geometry_code(canonical.geometry);
                        if (geometryByCode_.count(code))
                            continue;
                        const std::uint32_t id =
                          static_cast<std::uint32_t>(geometries_.size());
                        geometries_.push_back({});
                        geometries_.back().geometry = canonical.geometry;
                        if (!geometryByCode_.emplace(code, id).second)
                            throw std::runtime_error(
                              "symbolic geometry interner collision error");
                    }
                }
        std::cout << "information_symbolic_geometries " << geometries_.size()
                  << " expected " << (2 * Squares * (Squares - 1) * 2 / 4)
                  << '\n' << std::flush;
    }

    void build_transitions() {
        std::uint64_t liveWorlds = 0;
        std::uint64_t terminalWorlds = 0;
        std::uint64_t symbolicEdges = 0;
        // First partition every live geometry by the mover-private observation
        // available at that decision.  White is the informed owner, so her
        // private dots do not refine Black's belief; Black-to-move geometries
        // receive one stratum per exact shared decision-observation class.
        for (std::uint32_t geometryId = 0; geometryId < geometries_.size();
             ++geometryId) {
            SymbolicGeometryModel& model = geometries_[geometryId];
            model.actualStratum.fill(NoBelief);
            std::unordered_map<std::uint32_t, Mask80> decisionBlocks;
            for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
                if (ghost == model.geometry.whiteKing ||
                    ghost == model.geometry.blackKing)
                    continue;
                LegacyState state;
                state.side = static_cast<Color>(model.geometry.side);
                state.whiteKing = model.geometry.whiteKing;
                state.blackKing = model.geometry.blackKing;
                state.ghost = ghost;
                state.visible = model.geometry.visible;
                const std::uint32_t concrete = encode_index(state);
                const std::uint8_t terminal = oracle_.terminal(concrete);
                if (terminal & 4) {
                    mask_set(model.terminalSources, ghost);
                    if (terminal & 1)
                        mask_set(model.terminalWhiteSources, ghost);
                    if (terminal & 2)
                        mask_set(model.terminalBlackSources, ghost);
                    ++terminalWorlds;
                    continue;
                }
                mask_set(model.liveSources, ghost);
                ++liveWorlds;
                if (state.visible)
                    continue;
                const std::uint32_t decision =
                  state.side == Color::Black ? oracle_.decision(concrete) : 0;
                if (decision == NoActual)
                    throw std::runtime_error(
                      "live symbolic world has no decision stratum");
                mask_set(decisionBlocks[decision], ghost);
            }
            for (const auto& [decision, sources] : decisionBlocks) {
                (void)decision;
                const std::uint32_t stratum =
                  static_cast<std::uint32_t>(strata_.size());
                strata_.push_back({geometryId, sources});
                model.strata.push_back(stratum);
                for (std::uint8_t ghost = 0; ghost < Squares; ++ghost)
                    if (mask_test(sources, ghost))
                        model.actualStratum[ghost] = stratum;
            }
        }
        for (std::uint32_t geometryId = 0; geometryId < geometries_.size();
             ++geometryId) {
            SymbolicGeometryModel& model = geometries_[geometryId];
            std::unordered_map<std::uint32_t, std::uint32_t> relationByObservation;
            std::unordered_map<std::uint32_t, std::uint32_t> actionByPublicId;
            std::vector<std::unordered_map<std::uint8_t, Mask80>> relationImage;
            std::vector<std::unordered_map<std::uint32_t, Mask80>>
              actionObservationSources;
            for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
                if (ghost == model.geometry.whiteKing ||
                    ghost == model.geometry.blackKing)
                    continue;
                LegacyState state;
                state.side = static_cast<Color>(model.geometry.side);
                state.whiteKing = model.geometry.whiteKing;
                state.blackKing = model.geometry.blackKing;
                state.ghost = ghost;
                state.visible = model.geometry.visible;
                const std::uint32_t concrete = encode_index(state);
                if (oracle_.terminal(concrete) & 4)
                    continue;
                for (const ConcreteEdge* concreteEdge = oracle_.begin(concrete);
                     concreteEdge != oracle_.end(concrete); ++concreteEdge) {
                    std::uint32_t relationLocal;
                    if (const auto found = relationByObservation.find(
                          concreteEdge->observation);
                        found != relationByObservation.end())
                        relationLocal = found->second;
                    else {
                        relationLocal = static_cast<std::uint32_t>(
                          model.relations.size());
                        SymbolicRelation relation;
                        relation.observation = concreteEdge->observation;
                        model.relations.push_back(
                          static_cast<std::uint32_t>(relations_.size()));
                        relations_.push_back(std::move(relation));
                        relationImage.emplace_back();
                        relationByObservation.emplace(concreteEdge->observation,
                                                      relationLocal);
                    }
                    const std::uint32_t relationId = model.relations[relationLocal];
                    SymbolicRelation& relation = relations_[relationId];
                    mask_set(relation.possibleSources, ghost);
                    if (!(concreteEdge->exact & 2))
                        mask_set(relation.badBlackSources, ghost);

                    SymbolicEdge edge;
                    edge.relation = relationId;
                    edge.action = concreteEdge->action;
                    edge.exact = concreteEdge->exact;
                    edge.sameClass = concreteEdge->child != NoActual;
                    if (edge.sameClass) {
                        const LegacyState rawChild = decode_index(concreteEdge->child);
                        const CanonicalGeometry canonical = canonical_geometry(
                          public_geometry(rawChild));
                        const std::uint32_t childGeometry = find_geometry(
                          canonical.geometry);
                        const std::uint8_t childActual = static_cast<std::uint8_t>(
                          transform_square(rawChild.ghost, canonical.transform));
                        const std::uint32_t childStratum =
                          geometries_[childGeometry].actualStratum[childActual];
                        const bool childTerminal =
                          oracle_.terminal(concreteEdge->child) & 4;
                        if (childStratum == NoBelief && !childTerminal &&
                            !rawChild.visible)
                            throw std::runtime_error(
                              "live symbolic child has no decision stratum");
                        if (relation.childTerminalKnown &&
                            relation.childTerminal != childTerminal)
                            throw std::runtime_error(
                              "one symbolic observation mixes live and terminal children");
                        relation.childTerminalKnown = true;
                        relation.childTerminal = childTerminal;
                        if (relation.sawExternal)
                            throw std::runtime_error(
                              "one symbolic observation mixes material classes");
                        if (relation.sameClass &&
                            relation.childGeometry != childGeometry)
                            throw std::runtime_error(
                              "one symbolic observation mixes public child geometry");
                        relation.sameClass = true;
                        relation.childGeometry = childGeometry;
                        if (childStratum != NoBelief) {
                            if (relation.childStratum != NoBelief &&
                                relation.childStratum != childStratum)
                                throw std::runtime_error(
                                  "one symbolic observation crosses a child decision stratum");
                            relation.childStratum = childStratum;
                        }
                        mask_set(relationImage[relationLocal][childActual], ghost);
                        edge.childActual = childActual;
                    }
                    else {
                        if (relation.sameClass)
                            throw std::runtime_error(
                              "one symbolic observation mixes exact and same-class children");
                        relation.sawExternal = true;
                    }
                    model.edges[ghost].push_back(edge);
                    ++symbolicEdges;

                    if (static_cast<Color>(model.geometry.side) == Color::Black) {
                        std::uint32_t actionLocal;
                        if (const auto found = actionByPublicId.find(
                              concreteEdge->action);
                            found != actionByPublicId.end())
                            actionLocal = found->second;
                        else {
                            actionLocal = static_cast<std::uint32_t>(
                              model.actions.size());
                            model.actions.push_back({});
                            model.actions.back().publicId = concreteEdge->action;
                            actionObservationSources.emplace_back();
                            actionByPublicId.emplace(concreteEdge->action,
                                                     actionLocal);
                        }
                        SymbolicAction& action = model.actions[actionLocal];
                        if (mask_test(action.legalSources, ghost))
                            throw std::runtime_error(
                              "duplicate symbolic public action in one concrete world");
                        mask_set(action.legalSources, ghost);
                        mask_set(actionObservationSources[actionLocal][relationId],
                                 ghost);
                    }
                }
            }
            for (std::size_t local = 0; local < model.relations.size(); ++local) {
                SymbolicRelation& relation = relations_[model.relations[local]];
                if (relation.sameClass) {
                    for (const auto& [childSquare, sources] : relationImage[local])
                        relation.image.push_back({sources, childSquare});
                    std::sort(relation.image.begin(), relation.image.end(),
                              [](const auto& lhs, const auto& rhs) {
                                  return lhs.childSquare < rhs.childSquare;
                              });
                }
            }
            for (std::size_t action = 0; action < model.actions.size(); ++action) {
                for (const auto& [relation, sources] :
                     actionObservationSources[action])
                    model.actions[action].observations.push_back({relation, sources});
                std::sort(model.actions[action].observations.begin(),
                          model.actions[action].observations.end(),
                          [](const auto& lhs, const auto& rhs) {
                              return lhs.relation < rhs.relation;
                          });
            }
            if ((geometryId + 1) % 500 == 0)
                std::cout << "information_symbolic_model " << geometryId + 1
                          << '/' << geometries_.size() << " relations "
                          << relations_.size() << " edges " << symbolicEdges
                          << " peak_rss_bytes " << peak_rss_bytes() << '\n'
                          << std::flush;
        }
        std::cout << "information_symbolic_model_complete geometries "
                  << geometries_.size() << " live_worlds " << liveWorlds
                  << " terminal_worlds " << terminalWorlds << " relations "
                  << relations_.size() << " strata " << strata_.size()
                  << " edges " << symbolicEdges
                  << " peak_rss_bytes " << peak_rss_bytes() << '\n' << std::flush;
    }

    void build_fresh_roots() {
        denseGeometry_.assign(ConcreteStateCount, NoBelief);
        denseActual_.assign(ConcreteStateCount, 0);
        denseRootMask_.assign(ConcreteStateCount, {});
        admitted_.assign(ConcreteStateCount, 0);
        std::unordered_map<std::uint64_t, Mask80> hiddenRoots;
        std::vector<std::uint64_t> denseRootKey(ConcreteStateCount, 0);
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            if (!admitted_root(index))
                continue;
            const LegacyState state = decode_index(index);
            const CanonicalGeometry canonical = canonical_geometry(
              public_geometry(state));
            const std::uint32_t geometry = find_geometry(canonical.geometry);
            const std::uint8_t actual = static_cast<std::uint8_t>(
              transform_square(state.ghost, canonical.transform));
            admitted_[index] = 1;
            denseGeometry_[index] = geometry;
            denseActual_[index] = actual;
            if (state.visible)
                denseRootMask_[index] = singleton_mask(actual);
            else {
                LegacyState canonicalState;
                canonicalState.side = static_cast<Color>(canonical.geometry.side);
                canonicalState.whiteKing = canonical.geometry.whiteKing;
                canonicalState.blackKing = canonical.geometry.blackKing;
                canonicalState.ghost = actual;
                canonicalState.visible = canonical.geometry.visible;
                const std::uint32_t canonicalIndex = encode_index(canonicalState);
                const std::uint32_t decision =
                  state.side == Color::Black &&
                  !(oracle_.terminal(canonicalIndex) & 4)
                  ? oracle_.decision(canonicalIndex) : NoActual;
                const std::uint64_t key =
                  std::uint64_t(geometry) << 32 | decision;
                denseRootKey[index] = key;
                mask_set(hiddenRoots[key], actual);
            }
        }
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index)
            if (admitted_[index] && !decode_index(index).visible)
                denseRootMask_[index] = hiddenRoots.at(denseRootKey[index]);
        std::uint64_t admitted = 0;
        for (const std::uint8_t value : admitted_)
            admitted += value;
        std::cout << "information_symbolic_roots admitted " << admitted
                  << " hidden_public_geometries " << hiddenRoots.size() << '\n'
                  << std::flush;
    }

    const ConcreteTransitionOracle& oracle_;
    std::vector<SymbolicGeometryModel> geometries_;
    std::unordered_map<std::uint32_t, std::uint32_t> geometryByCode_;
    std::vector<SymbolicRelation> relations_;
    std::vector<SymbolicStratum> strata_;
    std::vector<std::uint32_t> denseGeometry_;
    std::vector<std::uint8_t> denseActual_;
    std::vector<Mask80> denseRootMask_;
    std::vector<std::uint8_t> admitted_;
};

class GhostInformationGraph {
  public:
    explicit GhostInformationGraph(const ConcreteTransitionOracle& oracle)
      : oracle_(oracle), rootBelief_(ConcreteStateCount, NoBelief),
        rootActual_(ConcreteStateCount, NoActual),
        admitted_(ConcreteStateCount, 0) {}

    void build_roots() {
        const DisclosureContext blackView{Color::Black, false};
        std::unordered_map<std::string, std::vector<std::uint32_t>> hiddenGroups;
        hiddenGroups.reserve(16'384);
        std::uint64_t hiddenAdmitted = 0;
        std::uint64_t visibleAdmitted = 0;
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            if (!admitted_root(index))
                continue;
            admitted_[index] = 1;
            const LegacyState state = decode_index(index);
            if (state.visible) {
                const InternedBelief root = intern({index});
                rootBelief_[index] = root.belief;
                rootActual_[index] = transform_index(index, root.transform);
                ++visibleAdmitted;
            }
            else {
                Position position = make_position(index);
                std::string observation = view_key(position, blackView);
                if (state.side == Color::Black && !position.game_over()) {
                    const std::uint32_t decision = oracle_.decision(index);
                    if (decision == NoActual)
                        throw std::runtime_error(
                          "live Black root has no decision observation");
                    observation += "|blackDecision=" + std::to_string(decision);
                }
                hiddenGroups[observation].push_back(index);
                ++hiddenAdmitted;
            }
            if ((index + 1) % 250'000 == 0)
                std::cout << "information_roots concrete " << index + 1 << '/'
                          << ConcreteStateCount << " beliefs " << nodes_.size()
                          << '\n' << std::flush;
        }
        for (auto& [view, rootWorlds] : hiddenGroups) {
            (void)view;
            std::sort(rootWorlds.begin(), rootWorlds.end());
            const InternedBelief root = intern(rootWorlds);
            for (const std::uint32_t world : rootWorlds) {
                if (rootBelief_[world] != NoBelief)
                    throw std::runtime_error("concrete root assigned to two public views");
                rootBelief_[world] = root.belief;
                rootActual_[world] = transform_index(world, root.transform);
            }
        }
        std::cout << "information_roots hidden_concrete " << hiddenAdmitted
                  << " visible_concrete " << visibleAdmitted
                  << " hidden_views " << hiddenGroups.size()
                  << " total_beliefs " << nodes_.size() << '\n' << std::flush;
    }

    void expand_all() {
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t cursor = 0; cursor < nodes_.size(); ++cursor) {
            (void)generate(static_cast<std::uint32_t>(cursor), true, false);
            if ((cursor + 1) % 100'000 == 0) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - started).count();
                std::cout << "information_graph expanded " << cursor + 1
                          << " discovered " << nodes_.size()
                          << " memberships " << worldMemberships_
                          << " observations " << observationEdges_
                          << " max_belief " << maxBeliefSize_
                          << " peak_rss_bytes " << peak_rss_bytes()
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        std::uint32_t maxBelief = 0;
        for (const BeliefKey& key : nodes_)
            maxBelief = std::max(maxBelief, belief_size(key));
        std::cout << "information_graph_complete beliefs " << nodes_.size()
                  << " memberships " << worldMemberships_
                  << " observations " << observationEdges_
                  << " max_belief " << maxBelief
                  << " peak_rss_bytes " << peak_rss_bytes() << '\n' << std::flush;
    }

    [[nodiscard]] const std::vector<BeliefKey>& nodes() const { return nodes_; }
    [[nodiscard]] const std::vector<std::uint32_t>& roots() const { return rootBelief_; }
    [[nodiscard]] const std::vector<std::uint32_t>& root_actuals() const {
        return rootActual_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& admitted() const { return admitted_; }

    [[nodiscard]] GeneratedNode regenerate(std::uint32_t belief) {
        return generate(belief, false, true);
    }

    [[nodiscard]] std::uint8_t common_action_count(std::uint32_t belief) const {
        const BeliefKey& key = nodes_.at(belief);
        if (static_cast<Color>(key.side) != Color::Black)
            return 0;
        const std::vector<std::uint32_t> concrete = worlds(key);
        if (concrete.empty())
            throw std::runtime_error("empty compact Ghost belief");
        std::vector<std::map<std::uint32_t, bool>> actions(concrete.size());
        for (std::size_t world = 0; world < concrete.size(); ++world) {
            if (oracle_.terminal(concrete[world]) & 4)
                return 0;
            for (const ConcreteEdge* edge = oracle_.begin(concrete[world]);
                 edge != oracle_.end(concrete[world]); ++edge)
                actions[world].emplace(edge->action, true);
        }
        std::uint32_t count = 0;
        for (const auto& [action, unused] : actions.front()) {
            (void)unused;
            bool common = true;
            for (std::size_t world = 1; world < actions.size(); ++world)
                common = common && actions[world].count(action) != 0;
            count += common;
        }
        if (count > std::numeric_limits<std::uint8_t>::max())
            throw std::runtime_error("Black common action count exceeds byte storage");
        return static_cast<std::uint8_t>(count);
    }

    [[nodiscard]] std::vector<std::uint32_t> belief_worlds(
      std::uint32_t belief) const {
        return worlds(nodes_.at(belief));
    }

  private:
    struct InternedBelief {
        std::uint32_t belief = NoBelief;
        std::uint8_t transform = 0;
    };

    [[nodiscard]] static BeliefKey key_from_worlds(
      const std::vector<std::uint32_t>& concrete) {
        if (concrete.empty())
            throw std::runtime_error("cannot key an empty Ghost belief");
        const LegacyState first = decode_index(concrete.front());
        BeliefKey key;
        key.side = static_cast<std::uint8_t>(first.side);
        key.whiteKing = first.whiteKing;
        key.blackKing = first.blackKing;
        key.visible = first.visible;
        for (const std::uint32_t index : concrete) {
            const LegacyState state = decode_index(index);
            if (state.side != first.side || state.whiteKing != first.whiteKing ||
                state.blackKing != first.blackKing || state.visible != first.visible)
                throw std::runtime_error(
                  "one observation produced incompatible public Ghost geometry");
            add_ghost_square(key, state.ghost);
        }
        if (key.visible && belief_size(key) != 1)
            throw std::runtime_error("a visible Ghost public view is not singleton");
        return key;
    }

    [[nodiscard]] static std::vector<std::uint32_t> worlds(const BeliefKey& key) {
        std::vector<std::uint32_t> result;
        result.reserve(belief_size(key));
        for (int square = 0; square < Position::BoardSquares; ++square) {
            if (!has_ghost_square(key, square))
                continue;
            LegacyState state;
            state.side = static_cast<Color>(key.side);
            state.whiteKing = key.whiteKing;
            state.blackKing = key.blackKing;
            state.ghost = static_cast<std::uint8_t>(square);
            state.visible = key.visible != 0;
            result.push_back(encode_index(state));
        }
        return result;
    }

    [[nodiscard]] InternedBelief intern(
      const std::vector<std::uint32_t>& sourceWorlds) {
        if (sourceWorlds.empty())
            throw std::runtime_error("cannot intern an empty belief");
        std::vector<std::uint32_t> worlds = sourceWorlds;
        if (!std::is_sorted(worlds.begin(), worlds.end()))
            std::sort(worlds.begin(), worlds.end());
        worlds.erase(std::unique(worlds.begin(), worlds.end()), worlds.end());
        const CanonicalBelief canonical = canonicalize(key_from_worlds(worlds));
        if (const auto found = interner_.find(canonical.key); found != interner_.end())
            return {found->second, canonical.transform};
        if (nodes_.size() >= NoBelief)
            throw std::runtime_error("belief identifier exceeds 32-bit domain");
        const std::uint32_t belief = static_cast<std::uint32_t>(nodes_.size());
        const std::uint32_t size = belief_size(canonical.key);
        worldMemberships_ += size;
        maxBeliefSize_ = std::max(maxBeliefSize_, size);
        nodes_.push_back(canonical.key);
        if (!interner_.emplace(canonical.key, belief).second)
            throw std::runtime_error("compact Ghost belief interner collision error");
        return {belief, canonical.transform};
    }

    [[nodiscard]] InternedBelief find_belief(
      const std::vector<std::uint32_t>& concrete) const {
        const CanonicalBelief canonical = canonicalize(key_from_worlds(concrete));
        const auto found = interner_.find(canonical.key);
        if (found == interner_.end())
            throw std::runtime_error(
              "equation regeneration found a belief outside the closed domain");
        return {found->second, canonical.transform};
    }

    [[nodiscard]] GeneratedNode generate(std::uint32_t belief,
                                         bool allowNewBeliefs,
                                         bool retainEquations) {
        // Copy the fixed-width key because interning may reallocate nodes_.
        const BeliefKey key = nodes_.at(belief);
        GeneratedNode node;
        node.worlds = worlds(key);
        node.mover = static_cast<Color>(key.side);
        const bool firstTerminal = oracle_.terminal(node.worlds.front()) & 4;
        if (!std::all_of(node.worlds.begin(), node.worlds.end(), [&](std::uint32_t world) {
                return bool(oracle_.terminal(world) & 4) == firstTerminal;
            }))
            throw std::runtime_error("compact belief mixes terminal and live worlds");
        if (!firstTerminal && node.mover == Color::Black) {
            const std::uint32_t firstDecision = oracle_.decision(node.worlds.front());
            if (firstDecision == NoActual ||
                !std::all_of(node.worlds.begin(), node.worlds.end(),
                  [&](std::uint32_t world) {
                      return oracle_.decision(world) == firstDecision;
                  }))
                throw std::runtime_error(
                  "Black belief crosses a private legal-dot observation");
        }
        if (firstTerminal) {
            node.terminal = true;
            node.terminalWhite.reserve(node.worlds.size());
            node.terminalBlack = true;
            for (const std::uint32_t world : node.worlds) {
                const std::uint8_t terminal = oracle_.terminal(world);
                node.terminalWhite.push_back(terminal & 1);
                node.terminalBlack = node.terminalBlack && (terminal & 2);
            }
            return node;
        }

        using Member = std::pair<std::uint32_t, std::uint32_t>;
        std::vector<std::vector<TempEdge>> edges(node.worlds.size());
        std::unordered_map<std::uint32_t, std::vector<Member>> byObservation;
        for (std::size_t world = 0; world < node.worlds.size(); ++world) {
            const std::uint32_t concrete = node.worlds[world];
            for (const ConcreteEdge* cached = oracle_.begin(concrete);
                 cached != oracle_.end(concrete); ++cached) {
                TempEdge edge;
                edge.action = cached->action;
                edge.child.sameClass = cached->child != NoActual;
                edge.child.index = cached->child;
                edge.child.whiteWins = cached->exact & 1;
                edge.child.blackWins = cached->exact & 2;
                edges[world].push_back(std::move(edge));
                byObservation[cached->observation].push_back({
                  static_cast<std::uint32_t>(world),
                  static_cast<std::uint32_t>(edges[world].size() - 1)});
                ++observationEdges_;
            }
        }

        for (const auto& [observation, members] : byObservation) {
            (void)observation;
            std::vector<std::uint32_t> sameClass;
            bool hasExternal = false;
            bool allBlackWin = true;
            for (const auto [world, edge] : members) {
                const RawChild& child = edges[world][edge].child;
                if (child.sameClass)
                    sameClass.push_back(child.index);
                else {
                    hasExternal = true;
                    allBlackWin = allBlackWin && child.blackWins;
                }
            }
            if (!sameClass.empty() && hasExternal)
                throw std::runtime_error(
                  "one Black observation mixes material/terminal classes");
            InternedBelief childBelief;
            if (!sameClass.empty()) {
                std::sort(sameClass.begin(), sameClass.end());
                sameClass.erase(std::unique(sameClass.begin(), sameClass.end()),
                                sameClass.end());
                childBelief = allowNewBeliefs ? intern(sameClass)
                                              : find_belief(sameClass);
            }
            for (const auto [world, edge] : members) {
                const RawChild& child = edges[world][edge].child;
                Successor& successor = edges[world][edge].successor;
                if (child.sameClass) {
                    successor.belief = childBelief.belief;
                    successor.actual = transform_index(
                      child.index, childBelief.transform);
                }
                else {
                    successor.exactWhite = child.whiteWins;
                    successor.exactBlack = allBlackWin;
                }
            }
        }

        if (!retainEquations)
            return node;
        if (node.mover == Color::White) {
            node.informedOffsets.reserve(edges.size() + 1);
            node.informedOffsets.push_back(0);
            for (const auto& worldEdges : edges) {
                for (const TempEdge& edge : worldEdges)
                    node.informedEdges.push_back(edge.successor);
                node.informedOffsets.push_back(
                  static_cast<std::uint32_t>(node.informedEdges.size()));
            }
        }
        else {
            std::vector<std::map<std::uint32_t, Successor>> byAction(edges.size());
            for (std::size_t world = 0; world < edges.size(); ++world)
                for (const TempEdge& edge : edges[world])
                    if (!byAction[world].emplace(edge.action, edge.successor).second)
                        throw std::runtime_error(
                          "duplicate Black public action in one concrete world");
            if (!byAction.empty()) {
                for (const auto& [action, successor] : byAction.front()) {
                    (void)successor;
                    bool common = true;
                    for (std::size_t world = 1; world < byAction.size(); ++world)
                        common = common && byAction[world].count(action) != 0;
                    if (!common)
                        continue;
                    for (std::size_t world = 0; world < byAction.size(); ++world)
                        node.commonEdges.push_back(byAction[world].at(action));
                    ++node.commonActionCount;
                }
            }
        }
        return node;
    }

    std::vector<BeliefKey> nodes_;
    std::unordered_map<BeliefKey, std::uint32_t, BeliefKeyHash> interner_;
    const ConcreteTransitionOracle& oracle_;
    std::vector<std::uint32_t> rootBelief_;
    std::vector<std::uint32_t> rootActual_;
    std::vector<std::uint8_t> admitted_;
    std::uint64_t worldMemberships_ = 0;
    std::uint64_t observationEdges_ = 0;
    std::uint32_t maxBeliefSize_ = 0;
};

struct SolvedInformation {
    std::vector<std::uint8_t> flags;
    InformationSolveSummary fixedPoint;
    std::uint64_t variables = 0;
};

void verify_symbolic_roots(const GhostSymbolicModel& symbolic,
                           const GhostInformationGraph& explicitRoots) {
    std::vector<Mask80> masks(explicitRoots.nodes().size());
    std::vector<std::uint32_t> geometries(explicitRoots.nodes().size(), NoBelief);
    for (std::uint32_t belief = 0; belief < explicitRoots.nodes().size(); ++belief) {
        for (const std::uint32_t world : explicitRoots.belief_worlds(belief)) {
            const auto [geometry, actual] = symbolic.locate(world);
            if (geometries[belief] == NoBelief)
                geometries[belief] = geometry;
            else if (geometries[belief] != geometry)
                throw std::runtime_error(
                  "explicit root crosses a symbolic public geometry");
            mask_set(masks[belief], actual);
        }
    }
    std::uint64_t checked = 0;
    for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
        if (symbolic.admitted()[index] != explicitRoots.admitted()[index])
            throw std::runtime_error(
              "symbolic and explicit fresh-root admission differ");
        if (!symbolic.admitted()[index])
            continue;
        const std::uint32_t belief = explicitRoots.roots()[index];
        const auto [geometry, actual] = symbolic.locate(index);
        if (belief == NoBelief || geometries.at(belief) != geometry ||
            !(masks.at(belief) == symbolic.dense_root_mask()[index]) ||
            decode_index(explicitRoots.root_actuals()[index]).ghost != actual)
            throw std::runtime_error(
              "symbolic fresh root differs from explicit full-string partition");
        ++checked;
    }
    std::cout << "information_symbolic_root_certificate concrete " << checked
              << " explicit_sets " << explicitRoots.nodes().size()
              << " residual 0\n" << std::flush;
}

class GhostSymbolicFixedPoint {
  public:
    GhostSymbolicFixedPoint(const GhostSymbolicModel& model,
                            const PackedTable& concrete)
      : model_(model), concrete_(concrete),
        black_(model.strata().size(), Robdd::False),
        nextBlack_(model.strata().size(), Robdd::False),
        white_(model.geometries().size()),
        nextWhite_(model.geometries().size()),
        visibleWhite_(model.geometries().size()),
        nextVisibleWhite_(model.geometries().size()),
        visibleBlack_(model.geometries().size()),
        nextVisibleBlack_(model.geometries().size()),
        domain_(model.strata().size(), Robdd::False),
        relationImage_(model.relations().size()) {
        for (auto& roots : white_)
            roots.fill(Robdd::False);
        for (auto& roots : nextWhite_)
            roots.fill(Robdd::False);
        for (auto& values : visibleWhite_)
            values.fill(0);
        for (auto& values : nextVisibleWhite_)
            values.fill(0);
        for (auto& values : visibleBlack_)
            values.fill(0);
        for (auto& values : nextVisibleBlack_)
            values.fill(0);
        for (std::uint32_t stratum = 0; stratum < model_.strata().size();
             ++stratum)
            domain_[stratum] = bdd_.subset_of(
              model_.strata()[stratum].liveSources);
    }

    [[nodiscard]] SolvedInformation solve() {
        const auto started = std::chrono::steady_clock::now();
        std::uint64_t iteration = 0;
        for (;;) {
            ++iteration;
            bdd_.clear_transient_caches();
            bellman(nextBlack_, nextWhite_);
            std::uint64_t changedBlack = 0;
            std::uint64_t changedWhite = 0;
            std::uint64_t changedVisible = 0;
            for (std::size_t stratum = 0; stratum < black_.size(); ++stratum) {
                if (bdd_.logical_and(black_[stratum],
                                     bdd_.logical_not(nextBlack_[stratum])) !=
                    Robdd::False)
                    throw std::runtime_error(
                      "symbolic Black least-fixed-point iteration regressed");
                changedBlack += black_[stratum] != nextBlack_[stratum];
            }
            for (std::size_t geometry = 0; geometry < white_.size(); ++geometry)
                for (unsigned actual = 0; actual < Squares; ++actual) {
                    if (bdd_.logical_and(
                          white_[geometry][actual],
                          bdd_.logical_not(nextWhite_[geometry][actual])) !=
                        Robdd::False)
                        throw std::runtime_error(
                          "symbolic White least-fixed-point iteration regressed");
                    changedWhite += white_[geometry][actual] !=
                                    nextWhite_[geometry][actual];
                }
            for (std::size_t geometry = 0; geometry < visibleWhite_.size();
                 ++geometry)
                for (unsigned actual = 0; actual < Squares; ++actual) {
                    if ((visibleWhite_[geometry][actual] &&
                         !nextVisibleWhite_[geometry][actual]) ||
                        (visibleBlack_[geometry][actual] &&
                         !nextVisibleBlack_[geometry][actual]))
                        throw std::runtime_error(
                          "symbolic visible-singleton iteration regressed");
                    changedVisible += visibleWhite_[geometry][actual] !=
                                      nextVisibleWhite_[geometry][actual];
                    changedVisible += visibleBlack_[geometry][actual] !=
                                      nextVisibleBlack_[geometry][actual];
                }
            black_.swap(nextBlack_);
            white_.swap(nextWhite_);
            visibleWhite_.swap(nextVisibleWhite_);
            visibleBlack_.swap(nextVisibleBlack_);
            const double elapsed = std::chrono::duration<double>(
              std::chrono::steady_clock::now() - started).count();
            std::cout << "information_symbolic_iteration " << iteration
                      << " bdd_nodes " << bdd_.node_count()
                      << " changed_black " << changedBlack
                      << " changed_white " << changedWhite
                      << " changed_visible " << changedVisible
                      << " peak_rss_bytes " << peak_rss_bytes()
                      << " elapsed " << elapsed << "s\n" << std::flush;
            if (!changedBlack && !changedWhite && !changedVisible)
                break;
        }

        // An independent full Bellman application is an exact symbolic
        // residual: canonical ROBDD root equality means equality on every
        // one of the 2^80 masks, not equality on a sample.
        bdd_.clear_transient_caches();
        bellman(nextBlack_, nextWhite_);
        std::uint64_t bellmanResidual = 0;
        for (std::size_t stratum = 0; stratum < black_.size(); ++stratum)
            bellmanResidual += black_[stratum] != nextBlack_[stratum];
        for (std::size_t geometry = 0; geometry < white_.size(); ++geometry)
            for (unsigned actual = 0; actual < Squares; ++actual)
                bellmanResidual += white_[geometry][actual] !=
                                   nextWhite_[geometry][actual];
        for (std::size_t geometry = 0; geometry < visibleWhite_.size(); ++geometry)
            for (unsigned actual = 0; actual < Squares; ++actual) {
                bellmanResidual += visibleWhite_[geometry][actual] !=
                                   nextVisibleWhite_[geometry][actual];
                bellmanResidual += visibleBlack_[geometry][actual] !=
                                   nextVisibleBlack_[geometry][actual];
            }
        if (bellmanResidual)
            throw std::runtime_error("symbolic Bellman residual is nonzero");

        std::uint64_t monotonicityResidual = 0;
        for (std::uint32_t stratum = 0; stratum < black_.size(); ++stratum)
            if (!bdd_.is_downward_closed(black_[stratum]))
                ++monotonicityResidual;
        const auto& geometries = model_.geometries();
        for (std::uint32_t geometry = 0; geometry < geometries.size(); ++geometry)
            for (unsigned actual = 0; actual < Squares; ++actual) {
                const std::uint32_t stratum =
                  geometries[geometry].actualStratum[actual];
                if (stratum != NoBelief &&
                    !bdd_.is_upward_closed(
                      white_[geometry][actual],
                      model_.strata()[stratum].liveSources))
                    ++monotonicityResidual;
            }
        if (monotonicityResidual)
            throw std::runtime_error(
              "symbolic force predicate violates its exact monotonicity");

        const std::uint64_t singletonResidual = verify_singletons();
        if (singletonResidual)
            throw std::runtime_error(
              "symbolic singleton result differs from concrete tablebase");

        SolvedInformation solved;
        solved.flags.assign(ConcreteStateCount, 0);
        const auto& admitted = model_.admitted();
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            if (!admitted[index])
                continue;
            const auto [geometry, actual] = model_.locate(index);
            const std::uint8_t terminal = model_.terminal(index);
            bool white = false;
            bool black = false;
            if (terminal & 4) {
                white = terminal & 1;
                black = terminal & 2;
            }
            else {
                if (model_.geometries()[geometry].geometry.visible) {
                    if (!(model_.dense_root_mask()[index] ==
                          singleton_mask(actual)))
                        throw std::runtime_error(
                          "visible fresh root is not a public singleton");
                    white = visibleWhite_[geometry][actual];
                    black = visibleBlack_[geometry][actual];
                }
                else {
                    const std::uint32_t stratum =
                      model_.geometries()[geometry].actualStratum[actual];
                    if (stratum == NoBelief ||
                        !mask_subset(model_.dense_root_mask()[index],
                                     model_.strata()[stratum].liveSources))
                        throw std::runtime_error(
                          "fresh root crosses a symbolic decision stratum");
                    white = bdd_.evaluate(white_[geometry][actual],
                                          model_.dense_root_mask()[index]);
                    black = bdd_.evaluate(black_[stratum],
                                          model_.dense_root_mask()[index]);
                }
            }
            if (white && black)
                throw std::runtime_error(
                  "both players force a win at one symbolic root");
            solved.flags[index] = 4 | (white ? 1 : 0) | (black ? 2 : 0);
        }
        solved.fixedPoint.variables = bdd_.node_count();
        solved.fixedPoint.activated = iteration;
        solved.fixedPoint.bellmanResidual = bellmanResidual;
        solved.fixedPoint.rankResidual = monotonicityResidual + singletonResidual;
        solved.variables = bdd_.node_count();
        std::cout << "information_symbolic_certificate iterations " << iteration
                  << " bdd_nodes " << bdd_.node_count()
                  << " bellman_residual " << bellmanResidual
                  << " monotonicity_residual " << monotonicityResidual
                  << " singleton_residual " << singletonResidual
                  << " belief_cap none powerset_exact 1\n" << std::flush;
        return solved;
    }

  private:
    [[nodiscard]] const std::vector<std::pair<std::uint8_t, Robdd::Id>>&
    relation_image(std::uint32_t relation) {
        auto& cached = relationImage_.at(relation);
        if (!cached.empty())
            return cached;
        const SymbolicRelation& source = model_.relations().at(relation);
        if (!source.sameClass || source.image.empty())
            throw std::runtime_error(
              "symbolic composition requested for an empty relation");
        cached.reserve(source.image.size());
        for (const SymbolicRelationEntry& entry : source.image)
            cached.emplace_back(entry.childSquare, bdd_.any(entry.sources));
        return cached;
    }

    [[nodiscard]] Robdd::Id compose(std::uint32_t relation,
                                    Robdd::Id child) {
        if (child <= Robdd::True)
            return child;
        std::array<Robdd::Id, Squares> image{};
        image.fill(Robdd::False);
        for (const auto [square, formula] : relation_image(relation))
            image[square] = formula;
        return bdd_.compose(child, image, relation);
    }

    [[nodiscard]] Robdd::Id no_sources(const Mask80& sources) {
        return bdd_.logical_not(bdd_.any(sources));
    }

    [[nodiscard]] Robdd::Id white_successor(
      const SymbolicEdge& edge,
      const std::vector<std::array<Robdd::Id, Squares>>& white) {
        const SymbolicRelation& relation = model_.relations().at(edge.relation);
        if (!edge.sameClass)
            return edge.exact & 1 ? Robdd::True : Robdd::False;
        const SymbolicGeometryModel& child =
          model_.geometries().at(relation.childGeometry);
        if (relation.childTerminal)
            return mask_test(child.terminalWhiteSources, edge.childActual)
                 ? Robdd::True : Robdd::False;
        if (child.geometry.visible)
            return visibleWhite_.at(relation.childGeometry)[edge.childActual]
                 ? Robdd::True : Robdd::False;
        return compose(edge.relation,
                       white.at(relation.childGeometry)[edge.childActual]);
    }

    [[nodiscard]] Robdd::Id black_successor(
      std::uint32_t relationId,
      const std::vector<Robdd::Id>& black) {
        const SymbolicRelation& relation = model_.relations().at(relationId);
        if (!relation.sameClass)
            return no_sources(relation.badBlackSources);
        if (relation.childTerminal) {
            const SymbolicGeometryModel& child =
              model_.geometries().at(relation.childGeometry);
            Mask80 bad;
            for (const SymbolicRelationEntry& entry : relation.image)
                if (!mask_test(child.terminalBlackSources, entry.childSquare))
                    bad = mask_or(bad, entry.sources);
            return no_sources(bad);
        }
        const SymbolicGeometryModel& child =
          model_.geometries().at(relation.childGeometry);
        if (child.geometry.visible) {
            if (relation.image.empty())
                throw std::runtime_error(
                  "visible symbolic child has an empty image");
            const std::uint8_t actual = relation.image.front().childSquare;
            if (!std::all_of(relation.image.begin(), relation.image.end(),
                  [&](const SymbolicRelationEntry& entry) {
                      return entry.childSquare == actual;
                  }))
                throw std::runtime_error(
                  "one visible observation contains multiple Ghost squares");
            return visibleBlack_.at(relation.childGeometry)[actual]
                 ? Robdd::True : Robdd::False;
        }
        if (relation.childStratum == NoBelief)
            throw std::runtime_error(
              "live symbolic relation has no child decision stratum");
        return compose(relationId, black.at(relation.childStratum));
    }

    [[nodiscard]] const SymbolicEdge* action_edge(
      const SymbolicGeometryModel& geometry, unsigned actual,
      std::uint32_t action) const {
        const auto& edges = geometry.edges[actual];
        const auto found = std::find_if(edges.begin(), edges.end(),
          [&](const SymbolicEdge& edge) { return edge.action == action; });
        return found == edges.end() ? nullptr : &*found;
    }

    void bellman(std::vector<Robdd::Id>& blackOutput,
                 std::vector<std::array<Robdd::Id, Squares>>& whiteOutput) {
        std::fill(blackOutput.begin(), blackOutput.end(), Robdd::False);
        for (auto& roots : whiteOutput)
            roots.fill(Robdd::False);
        for (auto& values : nextVisibleWhite_)
            values.fill(0);
        for (auto& values : nextVisibleBlack_)
            values.fill(0);
        const auto& geometries = model_.geometries();
        for (std::uint32_t geometryId = 0; geometryId < geometries.size();
             ++geometryId) {
            const SymbolicGeometryModel& geometry = geometries[geometryId];
            const Color mover = static_cast<Color>(geometry.geometry.side);
            if (geometry.geometry.visible) {
                for (unsigned actual = 0; actual < Squares; ++actual) {
                    if (!mask_test(geometry.liveSources, actual))
                        continue;
                    const Mask80 singleton = singleton_mask(actual);
                    bool whiteValue = mover == Color::Black;
                    bool blackValue = mover == Color::White;
                    for (const SymbolicEdge& edge : geometry.edges[actual]) {
                        const bool whiteChild = bdd_.evaluate(
                          white_successor(edge, white_), singleton);
                        const bool blackChild = bdd_.evaluate(
                          black_successor(edge.relation, black_), singleton);
                        if (mover == Color::White) {
                            whiteValue = whiteValue || whiteChild;
                            blackValue = blackValue && blackChild;
                        }
                        else {
                            whiteValue = whiteValue && whiteChild;
                            blackValue = blackValue || blackChild;
                        }
                    }
                    nextVisibleWhite_[geometryId][actual] = whiteValue;
                    nextVisibleBlack_[geometryId][actual] = blackValue;
                }
                continue;
            }
            for (unsigned actual = 0; actual < Squares; ++actual) {
                const std::uint32_t stratum = geometry.actualStratum[actual];
                if (stratum == NoBelief)
                    continue;
                Robdd::Id value;
                if (mover == Color::White) {
                    value = Robdd::False;
                    for (const SymbolicEdge& edge : geometry.edges[actual])
                        value = bdd_.logical_or(
                          value, white_successor(edge, white_));
                }
                else {
                    value = Robdd::True;
                    for (const SymbolicAction& action : geometry.actions) {
                        const Robdd::Id common = bdd_.logical_and(
                          domain_[stratum], bdd_.subset_of(action.legalSources));
                        const SymbolicEdge* edge = action_edge(
                          geometry, actual, action.publicId);
                        const Robdd::Id successor = edge
                          ? white_successor(*edge, white_)
                          : Robdd::False;
                        value = bdd_.logical_and(
                          value, bdd_.logical_or(bdd_.logical_not(common),
                                                successor));
                    }
                }
                whiteOutput[geometryId][actual] = bdd_.logical_and(
                  domain_[stratum],
                  bdd_.logical_and(bdd_.variable(actual), value));
            }

            for (const std::uint32_t stratum : geometry.strata) {
                Robdd::Id value;
                if (mover == Color::White) {
                    value = Robdd::True;
                    const Mask80 worlds = model_.strata()[stratum].liveSources;
                    for (unsigned actual = 0; actual < Squares; ++actual) {
                        if (!mask_test(worlds, actual))
                            continue;
                        for (const SymbolicEdge& edge : geometry.edges[actual]) {
                            const Robdd::Id child = black_successor(
                              edge.relation, black_);
                            value = bdd_.logical_and(
                              value, bdd_.logical_or(
                                bdd_.logical_not(bdd_.variable(actual)), child));
                        }
                    }
                }
                else {
                    value = Robdd::False;
                    for (const SymbolicAction& action : geometry.actions) {
                        Robdd::Id gate = bdd_.logical_and(
                          domain_[stratum], bdd_.subset_of(action.legalSources));
                        for (const SymbolicActionObservation& observation :
                             action.observations) {
                            const Robdd::Id possible = bdd_.any(mask_and(
                              observation.possibleSources,
                              model_.strata()[stratum].liveSources));
                            const Robdd::Id child = black_successor(
                              observation.relation, black_);
                            gate = bdd_.logical_and(
                              gate, bdd_.logical_or(bdd_.logical_not(possible),
                                                    child));
                        }
                        value = bdd_.logical_or(value, gate);
                    }
                }
                blackOutput[stratum] = bdd_.logical_and(domain_[stratum], value);
            }
        }
    }

    [[nodiscard]] std::uint64_t verify_singletons() {
        std::uint64_t residual = 0;
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            const auto [geometry, actual] = model_.locate(index);
            const std::uint8_t terminal = model_.terminal(index);
            bool white;
            bool black;
            if (terminal & 4) {
                white = terminal & 1;
                black = terminal & 2;
            }
            else {
                if (model_.geometries()[geometry].geometry.visible) {
                    white = visibleWhite_[geometry][actual];
                    black = visibleBlack_[geometry][actual];
                }
                else {
                    const std::uint32_t stratum =
                      model_.geometries()[geometry].actualStratum[actual];
                    const Mask80 singleton = singleton_mask(actual);
                    white = bdd_.evaluate(white_[geometry][actual], singleton);
                    black = bdd_.evaluate(black_[stratum], singleton);
                }
            }
            const LegacyState state = decode_index(index);
            const Wdl exact = concrete_.result(index);
            const bool expectedWhite =
              (state.side == Color::White && exact == Wdl::Win) ||
              (state.side == Color::Black && exact == Wdl::Loss);
            const bool expectedBlack =
              (state.side == Color::Black && exact == Wdl::Win) ||
              (state.side == Color::White && exact == Wdl::Loss);
            residual += white != expectedWhite || black != expectedBlack;
        }
        return residual;
    }

    const GhostSymbolicModel& model_;
    const PackedTable& concrete_;
    Robdd bdd_;
    std::vector<Robdd::Id> black_;
    std::vector<Robdd::Id> nextBlack_;
    std::vector<std::array<Robdd::Id, Squares>> white_;
    std::vector<std::array<Robdd::Id, Squares>> nextWhite_;
    std::vector<std::array<std::uint8_t, Squares>> visibleWhite_;
    std::vector<std::array<std::uint8_t, Squares>> nextVisibleWhite_;
    std::vector<std::array<std::uint8_t, Squares>> visibleBlack_;
    std::vector<std::array<std::uint8_t, Squares>> nextVisibleBlack_;
    std::vector<Robdd::Id> domain_;
    std::vector<std::vector<std::pair<std::uint8_t, Robdd::Id>>> relationImage_;
};

[[nodiscard]] SolvedInformation solve_symbolic(
  const GhostSymbolicModel& model, const PackedTable& concrete) {
    GhostSymbolicFixedPoint solver(model, concrete);
    return solver.solve();
}

[[nodiscard]] SolvedInformation solve_graph(GhostInformationGraph& graph,
                                            const std::string& scratch) {
    const auto& nodes = graph.nodes();
    std::vector<std::uint64_t> whiteBase(nodes.size() + 1, 0);
    std::vector<std::uint64_t> auxiliaryBase(nodes.size() + 1, 0);
    std::vector<std::uint8_t> commonCounts(nodes.size(), 0);
    std::uint64_t softLocks = 0;
    std::uint32_t firstSoftLock = NoBelief;
    for (std::size_t belief = 0; belief < nodes.size(); ++belief)
        whiteBase[belief + 1] = whiteBase[belief] + belief_size(nodes[belief]);
    const std::uint64_t blackBase = whiteBase.back();
    const std::uint64_t auxiliaryStart = blackBase + nodes.size();
    for (std::size_t belief = 0; belief < nodes.size(); ++belief) {
        if (static_cast<Color>(nodes[belief].side) == Color::Black) {
            Position first = make_position(graph.belief_worlds(
              static_cast<std::uint32_t>(belief)).front());
            if (!first.game_over()) {
                commonCounts[belief] = graph.common_action_count(
                  static_cast<std::uint32_t>(belief));
                if (!commonCounts[belief]) {
                    ++softLocks;
                    if (firstSoftLock == NoBelief)
                        firstSoftLock = static_cast<std::uint32_t>(belief);
                }
            }
        }
        auxiliaryBase[belief + 1] = auxiliaryBase[belief] + commonCounts[belief];
        if ((belief + 1) % 100'000 == 0)
            std::cout << "information_action_scan " << belief + 1 << '/'
                      << nodes.size() << " common_actions "
                      << auxiliaryBase[belief + 1] << " soft_locks "
                      << softLocks << '\n' << std::flush;
    }
    std::cout << "information_soft_locks sets " << softLocks
              << " first_belief ";
    if (firstSoftLock == NoBelief)
        std::cout << "none\n";
    else {
        const auto firstWorlds = graph.belief_worlds(firstSoftLock);
        std::cout << firstSoftLock << " worlds " << firstWorlds.size()
                  << " example " << make_position(firstWorlds.front()).upn() << '\n';
    }
    const std::uint64_t variableCount = auxiliaryStart + auxiliaryBase.back();
    if (variableCount >= InformationTrue)
        throw std::runtime_error("information equation domain exceeds 32-bit tokens");

    const auto white_variable = [&](std::uint32_t belief, std::size_t ordinal) {
        return static_cast<InformationToken>(whiteBase.at(belief) + ordinal);
    };
    const auto black_variable = [&](std::uint32_t belief) {
        return static_cast<InformationToken>(blackBase + belief);
    };
    const auto auxiliary_variable = [&](std::uint32_t belief, std::size_t action) {
        return static_cast<InformationToken>(auxiliaryStart +
          auxiliaryBase.at(belief) + action);
    };
    const auto actual_ordinal = [&](const Successor& successor) {
        const auto worlds = graph.belief_worlds(successor.belief);
        const auto found = std::lower_bound(worlds.begin(), worlds.end(),
                                            successor.actual);
        if (found == worlds.end() || *found != successor.actual)
            throw std::runtime_error("successor actual missing from child belief");
        return static_cast<std::size_t>(found - worlds.begin());
    };
    const auto white_token = [&](const Successor& successor) {
        return successor.belief == NoBelief
             ? (successor.exactWhite ? InformationTrue : InformationFalse)
             : white_variable(successor.belief, actual_ordinal(successor));
    };
    const auto black_token = [&](const Successor& successor) {
        return successor.belief == NoBelief
             ? (successor.exactBlack ? InformationTrue : InformationFalse)
             : black_variable(successor.belief);
    };

    std::cout << "information_equations white_variables " << blackBase
              << " black_variables " << nodes.size()
              << " action_and_variables " << auxiliaryBase.back()
              << " total " << variableCount << '\n' << std::flush;
    InformationFixedPoint solver(static_cast<std::uint32_t>(variableCount), scratch);
    std::vector<InformationToken> tokens;
    for (std::uint32_t belief = 0; belief < nodes.size(); ++belief) {
        const GeneratedNode node = graph.regenerate(belief);
        if (node.terminal) {
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                const InformationToken value = node.terminalWhite[world]
                                             ? InformationTrue : InformationFalse;
                solver.define_or(white_variable(belief, world), &value, 1);
            }
            const InformationToken value = node.terminalBlack
                                         ? InformationTrue : InformationFalse;
            solver.define_or(black_variable(belief), &value, 1);
            continue;
        }
        if (node.mover == Color::Black &&
            node.commonActionCount != commonCounts[belief])
            throw std::runtime_error(
              "Black common action count changed during equation regeneration");
        if (node.mover == Color::White) {
            tokens.clear();
            tokens.reserve(node.informedEdges.size());
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                std::vector<InformationToken> choices;
                for (std::size_t edge = node.informedOffsets[world];
                     edge < node.informedOffsets[world + 1]; ++edge) {
                    choices.push_back(white_token(node.informedEdges[edge]));
                    tokens.push_back(black_token(node.informedEdges[edge]));
                }
                solver.define_or(white_variable(belief, world), choices);
            }
            solver.define_and(black_variable(belief), tokens);
        }
        else {
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                tokens.clear();
                tokens.reserve(node.commonActionCount);
                for (std::size_t action = 0; action < node.commonActionCount; ++action)
                    tokens.push_back(white_token(node.commonEdges[
                      action * node.worlds.size() + world]));
                solver.define_and(white_variable(belief, world), tokens);
            }
            std::vector<InformationToken> actionTokens;
            actionTokens.reserve(node.commonActionCount);
            for (std::size_t action = 0; action < node.commonActionCount; ++action) {
                tokens.clear();
                tokens.reserve(node.worlds.size());
                for (std::size_t world = 0; world < node.worlds.size(); ++world)
                    tokens.push_back(black_token(node.commonEdges[
                      action * node.worlds.size() + world]));
                const InformationToken gate = auxiliary_variable(belief, action);
                solver.define_and(gate, tokens);
                actionTokens.push_back(gate);
            }
            solver.define_or(black_variable(belief), actionTokens);
        }
    }

    const InformationSolveSummary summary = solver.solve();
    if (summary.bellmanResidual || summary.rankResidual)
        throw std::runtime_error("information fixed point has a nonzero residual");

    SolvedInformation solved;
    solved.flags.resize(ConcreteStateCount, 0);
    solved.fixedPoint = summary;
    solved.variables = variableCount;
    const auto& roots = graph.roots();
    const auto& rootActuals = graph.root_actuals();
    const auto& admitted = graph.admitted();
    for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
        if (!admitted[index])
            continue;
        const std::uint32_t belief = roots[index];
        if (belief == NoBelief)
            throw std::runtime_error("admitted concrete state has no canonical root");
        const auto worlds = graph.belief_worlds(belief);
        const std::uint32_t rootActual = rootActuals[index];
        const auto found = std::lower_bound(worlds.begin(), worlds.end(), rootActual);
        if (found == worlds.end() || *found != rootActual)
            throw std::runtime_error("canonical root does not contain actual state");
        const std::size_t ordinal = static_cast<std::size_t>(found - worlds.begin());
        const bool white = solver.value(white_variable(belief, ordinal));
        const bool black = solver.value(black_variable(belief));
        if (white && black)
            throw std::runtime_error("both players have a sure win at one root");
        solved.flags[index] = 4 | (white ? 1 : 0) | (black ? 2 : 0);
    }
    return solved;
}

void report_results(const GhostInformationGraph& graph,
                    const PackedTable& concrete,
                    const SolvedInformation& solved) {
    using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
    using Cross = std::array<std::array<std::array<std::uint64_t, 4>, 4>, 2>;
    Counts totals{};
    Counts unreachable{};
    Cross cross{};
    std::array<std::uint64_t, 2> rootSets{};
    std::array<std::vector<std::uint8_t>, 2> seen{
      std::vector<std::uint8_t>(graph.nodes().size(), 0),
      std::vector<std::uint8_t>(graph.nodes().size(), 0)};
    for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
        const LegacyState state = decode_index(index);
        const std::size_t side = color_index(state.side);
        const Wdl exact = concrete.result(index);
        if (!graph.admitted()[index]) {
            ++unreachable[side][static_cast<std::size_t>(exact)];
            continue;
        }
        const std::uint8_t flags = solved.flags[index];
        const bool white = flags & 1;
        const bool black = flags & 2;
        const bool moverWins = state.side == Color::White ? white : black;
        const bool moverLoses = state.side == Color::White ? black : white;
        const Wdl information = moverWins ? Wdl::Win
                              : moverLoses ? Wdl::Loss : Wdl::Draw;
        ++totals[side][static_cast<std::size_t>(information)];
        ++cross[side][static_cast<std::size_t>(exact)]
                     [static_cast<std::size_t>(information)];
        const std::uint32_t belief = graph.roots()[index];
        if (!seen[side][belief]) {
            seen[side][belief] = 1;
            ++rootSets[side];
        }
    }
    for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t conserved = 0;
        for (std::size_t result = 1; result < 4; ++result)
            conserved += totals[side][result] + unreachable[side][result];
        if (conserved != ConcreteStateCount / 2)
            throw std::runtime_error("information summary does not conserve concrete roots");
        std::cout << "information_summary side " << side
                  << " win " << totals[side][1]
                  << " loss " << totals[side][2]
                  << " draw " << totals[side][3]
                  << " unreachable_win " << unreachable[side][1]
                  << " unreachable_loss " << unreachable[side][2]
                  << " unreachable_draw " << unreachable[side][3]
                  << " sets " << rootSets[side]
                  << " concrete " << ConcreteStateCount / 2
                  << " bellman_residual " << solved.fixedPoint.bellmanResidual
                  << " rank_residual " << solved.fixedPoint.rankResidual
                  << " belief_cap none exhaustive 1\n";
        for (std::size_t exact = 1; exact < 4; ++exact) {
            std::cout << "information_crosstab side " << side
                      << " concrete " << exact
                      << " information_win " << cross[side][exact][1]
                      << " information_loss " << cross[side][exact][2]
                      << " information_draw " << cross[side][exact][3] << '\n';
        }
    }
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

[[nodiscard]] bool valid_sha256(const std::string& text) {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f');
    });
}

void write_overlay(const std::string& path,
                   const PackedTable& concrete,
                   const GhostInformationGraph& graph,
                   const SolvedInformation& solved,
                   const std::string& sourceSha256,
                   const std::string& modelSha256) {
    if (path.empty())
        return;
    if (!valid_sha256(sourceSha256) || !valid_sha256(modelSha256))
        throw std::runtime_error(
          "UFIW2 output requires lowercase 64-digit source and model SHA-256 values");
    if (sourceSha256 != hex_digest(concrete.sha))
        throw std::runtime_error(
          "information source SHA-256 does not match the concrete kghostk.uftb bytes");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("cannot create information overlay: " + path);
    const std::array<char, 8> magic{{'U','F','I','W','2','\0','\0','\0'}};
    output.write(magic.data(), magic.size());
    write_u32(output, 2);
    write_u32(output, static_cast<std::uint32_t>(PieceType::Ghost));
    write_u32(output, static_cast<std::uint32_t>(PieceType::Count));
    write_u32(output, static_cast<std::uint32_t>(Color::White));
    write_u32(output, ConcreteStateCount);
    write_u32(output, GhostSubstates);
    output.write(sourceSha256.data(), sourceSha256.size());
    output.write(modelSha256.data(), modelSha256.size());
    output.write(reinterpret_cast<const char*>(solved.flags.data()), solved.flags.size());
    if (!output)
        throw std::runtime_error("failed writing information overlay");
    constexpr std::uint64_t HeaderBytes = 160;
    std::cout << "information_overlay " << path
              << " bytes " << HeaderBytes + solved.flags.size()
              << " concrete_sha256 " << sourceSha256
              << " solver_model_sha256 " << modelSha256
              << " beliefs " << graph.nodes().size()
              << " variables " << solved.variables << '\n';
}

void codec_self_test() {
    robdd_self_test();
    symbolic_tiny_powerset_self_test();
    for (std::uint32_t placement = 0; placement < PlacementCount; ++placement) {
        const LegacyState state = decode_placement(placement);
        if (encode_placement(state) != placement)
            throw std::runtime_error("legacy one-extra codec round trip failed");
        for (std::uint32_t visible = 0; visible < GhostSubstates; ++visible) {
            LegacyState substate = state;
            substate.visible = visible != 0;
            const std::uint32_t index = placement * GhostSubstates + visible;
            if (encode_index(substate) != index)
                throw std::runtime_error("legacy Ghost substate round trip failed");
        }
    }
    Sha256 sha;
    static constexpr char Test[] = "abc";
    sha.update(reinterpret_cast<const std::uint8_t*>(Test), 3);
    if (hex_digest(sha.finish()) !=
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        throw std::runtime_error("SHA-256 self-test failed");
    std::cout << "codecok placements " << PlacementCount
              << " concrete " << ConcreteStateCount << " sha256 ok\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    using namespace Stockfish::Ultimate;
    try {
        std::string input = "tablebases/kghostk.uftb";
        std::string output;
        std::string scratch = "/tmp";
        std::string sourceSha256;
        std::string modelSha256;
        bool selfTestOnly = false;
        bool explicitClosure = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            const auto value = [&](const char* option) {
                if (i + 1 >= argc)
                    throw std::runtime_error(std::string("missing value for ") + option);
                return std::string(argv[++i]);
            };
            if (argument == "--input")
                input = value("--input");
            else if (argument == "--output")
                output = value("--output");
            else if (argument == "--scratch")
                scratch = value("--scratch");
            else if (argument == "--information-source-sha256")
                sourceSha256 = value("--information-source-sha256");
            else if (argument == "--information-model-sha256")
                modelSha256 = value("--information-model-sha256");
            else if (argument == "--self-test")
                selfTestOnly = true;
            else if (argument == "--explicit-closure")
                explicitClosure = true;
            else
                throw std::runtime_error("unknown argument: " + argument);
        }
        codec_self_test();
        if (selfTestOnly)
            return 0;
        const PackedTable concrete = load_table(input);
        if (sourceSha256.empty())
            sourceSha256 = hex_digest(concrete.sha);
        std::cout << "information_input " << input
                  << " bytes " << concrete.bytes.size()
                  << " sha256 " << hex_digest(concrete.sha) << '\n' << std::flush;
        const ConcreteTransitionOracle oracle;
        GhostInformationGraph graph(oracle);
        graph.build_roots();
        SolvedInformation solved;
        if (explicitClosure) {
            graph.expand_all();
            solved = solve_graph(graph, scratch);
        }
        else {
            const GhostSymbolicModel symbolic(oracle);
            verify_symbolic_roots(symbolic, graph);
            solved = solve_symbolic(symbolic, concrete);
        }
        report_results(graph, concrete, solved);
        write_overlay(output, concrete, graph, solved, sourceSha256, modelSha256);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "ghost-information-tablebase: " << error.what() << '\n';
        return 1;
    }
}
