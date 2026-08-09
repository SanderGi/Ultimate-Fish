/*
  Ultimate Fish - exact K+2 Jesters-v-K public-information tablebase
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  Standalone proof kernel for fresh-maximal-public-view-v2.  The concrete
  identical-extra codec folds horizontal reflections around the real Ivory
  King.  This solver reconstructs all three royal assignments in a common
  physical frame. At every bare-King decision boundary, its private inspectable
  legal-dot frontier first refines the public set; that refinement is retained
  through later transition observations. Every reached two/three-world private
  set is interned by complete sorted concrete membership. Singletons cross-
  probe the exact concrete WDL directly. No set is hashed without equality
  comparison and no belief, action, edge, or iteration is capped or sampled.
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
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/resource.h>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t Remaining = Squares - 2;
constexpr std::uint32_t JesterPairs = Remaining * (Remaining - 1) / 2;
constexpr std::uint32_t ConcreteStateCount =
  2 * (Squares / 2) * (Squares - 1) * JesterPairs;
constexpr std::uint32_t PhysicalFrameCount = ConcreteStateCount / 3;
constexpr std::uint32_t OrientedWorldCount = ConcreteStateCount * 2;
constexpr std::uint32_t LowerStateCount =
  2 * Squares * (Squares - 1) * (Squares - 2);
constexpr std::uint32_t NoBelief = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t NoActual = std::numeric_limits<std::uint32_t>::max();

static_assert(ConcreteStateCount == 18'978'960);
static_assert(PhysicalFrameCount == 6'326'320);
static_assert(LowerStateCount == 985'920);

enum class Wdl : std::uint8_t { Unknown, Win, Loss, Draw };

// A public action is the player's chosen Move, not its protocol spelling.
// Keep all five identity fields from Move::operator== in disjoint bytes so
// unusual moves with a shared from/to spelling can never be conflated.
struct ActionKey {
    std::uint64_t value = 0;

    [[nodiscard]] static constexpr ActionKey from_move(const Move& move) {
        return {std::uint64_t(move.from) |
                (std::uint64_t(move.to) << 8) |
                (std::uint64_t(move.auxiliary) << 16) |
                (std::uint64_t(move.kind) << 24) |
                (std::uint64_t(move.promotion) << 32)};
    }

    [[nodiscard]] constexpr Move decode() const {
        Move move;
        move.from = static_cast<std::uint8_t>(value);
        move.to = static_cast<std::uint8_t>(value >> 8);
        move.auxiliary = static_cast<std::uint8_t>(value >> 16);
        move.kind = static_cast<MoveKind>(static_cast<std::uint8_t>(value >> 24));
        move.promotion = static_cast<PieceType>(
          static_cast<std::uint8_t>(value >> 32));
        return move;
    }

    friend constexpr bool operator<(ActionKey lhs, ActionKey rhs) {
        return lhs.value < rhs.value;
    }
    friend constexpr bool operator==(ActionKey lhs, ActionKey rhs) {
        return lhs.value == rhs.value;
    }
};

[[nodiscard]] constexpr std::size_t color_index(Color color) {
    return static_cast<std::size_t>(color);
}

struct FourState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t first = 0;
    std::uint8_t second = 0;
};

[[nodiscard]] std::uint8_t horizontal_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>((square / Position::BoardFiles) *
      Position::BoardFiles + Position::BoardFiles - 1 -
      square % Position::BoardFiles);
}

[[nodiscard]] FourState canonicalize(FourState state) {
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.first = horizontal_reflection(state.first);
        state.second = horizontal_reflection(state.second);
    }
    return state;
}

[[nodiscard]] std::uint32_t rank_excluding(
  std::uint8_t square, std::initializer_list<std::uint8_t> used) {
    std::uint32_t rank = square;
    for (const std::uint8_t occupied : used)
        rank -= occupied < square;
    return rank;
}

[[nodiscard]] std::uint8_t unrank_excluding(
  std::uint32_t rank, std::initializer_list<std::uint8_t> used) {
    for (std::uint8_t square = 0; square < Squares; ++square) {
        bool occupied = false;
        for (const std::uint8_t item : used)
            occupied = occupied || item == square;
        if (!occupied && rank-- == 0)
            return square;
    }
    throw std::runtime_error("double-Jester square rank is invalid");
}

[[nodiscard]] std::uint32_t encode_index(FourState state) {
    state = canonicalize(state);
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
        (Position::BoardFiles / 2) +
      state.whiteKing % Position::BoardFiles;
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    std::uint32_t firstRank = rank_excluding(
      state.first, {state.whiteKing, state.blackKing});
    std::uint32_t secondRank = rank_excluding(
      state.second, {state.whiteKing, state.blackKing});
    if (firstRank > secondRank)
        std::swap(firstRank, secondRank);
    if (firstRank == secondRank)
        throw std::runtime_error("double-Jester codec received one square twice");
    const std::uint32_t pairRank =
      firstRank * (2 * Remaining - firstRank - 1) / 2 +
      secondRank - firstRank - 1;
    return ((static_cast<std::uint32_t>(state.side) * (Squares / 2) + whiteRank)
             * (Squares - 1) + blackRank) * JesterPairs + pairRank;
}

[[nodiscard]] FourState decode_index(std::uint32_t index) {
    if (index >= ConcreteStateCount)
        throw std::runtime_error("double-Jester concrete index is out of range");
    const std::uint32_t pairRank = index % JesterPairs;
    index /= JesterPairs;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    const Color side = static_cast<Color>(index / (Squares / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Position::BoardFiles / 2)) * Position::BoardFiles +
      whiteRank % (Position::BoardFiles / 2));
    const std::uint8_t blackKing = unrank_excluding(blackRank, {whiteKing});
    std::uint32_t firstRank = 0;
    std::uint32_t residual = pairRank;
    for (; firstRank + 1 < Remaining; ++firstRank) {
        const std::uint32_t row = Remaining - firstRank - 1;
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

[[nodiscard]] Position make_position(FourState state) {
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, state.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, state.blackKing);
    const int first = position.add_piece(
      PieceType::Jester, Color::White, state.first);
    const int second = position.add_piece(
      PieceType::Jester, Color::White, state.second);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        first == Position::NoPiece || second == Position::NoPiece)
        throw std::runtime_error("double-Jester codec produced invalid geometry");
    for (const int id : {whiteKing, blackKing, first, second})
        position.piece(id).moved = true;
    position.set_side_to_move(state.side);
    return position;
}

[[nodiscard]] Position make_position(std::uint32_t index) {
    return make_position(decode_index(index));
}

[[nodiscard]] constexpr std::uint32_t base_index(std::uint32_t oriented) {
    return oriented / 2;
}

[[nodiscard]] Position make_oriented_position(std::uint32_t oriented) {
    if (oriented >= OrientedWorldCount)
        throw std::runtime_error("oriented double-Jester world is out of range");
    FourState state = decode_index(base_index(oriented));
    if (oriented & 1) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.first = horizontal_reflection(state.first);
        state.second = horizontal_reflection(state.second);
    }
    return make_position(state);
}

[[nodiscard]] std::uint32_t oriented_index(FourState physical) {
    const bool reflected =
      physical.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2;
    return encode_index(physical) * 2 + reflected;
}

[[nodiscard]] std::array<std::uint32_t, 3> frame_worlds(
  std::uint32_t index) {
    FourState decoded = decode_index(index);
    // Concrete horizontal folding is King-relative, but public royal sets must
    // share one physical board.  Canonicalize the public geometry independently
    // of which silhouette is the King, then retain one orientation bit on each
    // concrete world so decoding reconstructs that same physical frame.
    std::array<std::uint8_t, 3> publicRoyals{{
      decoded.whiteKing, decoded.first, decoded.second}};
    std::sort(publicRoyals.begin(), publicRoyals.end());
    std::array<std::uint8_t, 3> reflectedRoyals = publicRoyals;
    for (std::uint8_t& square : reflectedRoyals)
        square = horizontal_reflection(square);
    std::sort(reflectedRoyals.begin(), reflectedRoyals.end());
    const auto geometry = std::tie(decoded.blackKing, publicRoyals);
    const std::uint8_t reflectedBlack = horizontal_reflection(decoded.blackKing);
    const auto reflectedGeometry = std::tie(reflectedBlack, reflectedRoyals);
    if (reflectedGeometry < geometry) {
        decoded.blackKing = reflectedBlack;
        decoded.whiteKing = horizontal_reflection(decoded.whiteKing);
        decoded.first = horizontal_reflection(decoded.first);
        decoded.second = horizontal_reflection(decoded.second);
    }
    std::array<std::uint8_t, 3> royals{{
      decoded.whiteKing, decoded.first, decoded.second}};
    std::array<std::uint32_t, 3> worlds{};
    for (std::size_t king = 0; king < royals.size(); ++king) {
        const std::uint8_t first = royals[(king + 1) % royals.size()];
        const std::uint8_t second = royals[(king + 2) % royals.size()];
        worlds[king] = oriented_index({decoded.side, royals[king],
                                       decoded.blackKing, first, second});
    }
    std::sort(worlds.begin(), worlds.end());
    if (std::adjacent_find(worlds.begin(), worlds.end()) != worlds.end())
        throw std::runtime_error("royal-assignment codec is not three-to-one");
    return worlds;
}

[[nodiscard]] std::optional<std::uint32_t> same_class_index(
  const Position& position) {
    int whiteKing = Position::NoSquare;
    int blackKing = Position::NoSquare;
    std::array<int, 2> jesters{{Position::NoSquare, Position::NoSquare}};
    int jesterCount = 0;
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
        else if (piece.type == PieceType::Jester &&
                 piece.color == Color::White && jesterCount < 2)
            jesters[jesterCount++] = piece.square;
        else
            return std::nullopt;
    }
    if (liveModels != 4 || jesterCount != 2 ||
        whiteKing == Position::NoSquare || blackKing == Position::NoSquare ||
        position.has_forced_action() ||
        position.en_passant_square() != Position::NoSquare)
        return std::nullopt;
    return oriented_index({position.side_to_move(),
                           static_cast<std::uint8_t>(whiteKing),
                           static_cast<std::uint8_t>(blackKing),
                           static_cast<std::uint8_t>(jesters[0]),
                           static_cast<std::uint8_t>(jesters[1])});
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

    [[nodiscard]] static std::uint32_t rotate(std::uint32_t value, int count) {
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
            const std::uint32_t s0 = rotate(words[index - 15], 7) ^
                                     rotate(words[index - 15], 18) ^
                                     (words[index - 15] >> 3);
            const std::uint32_t s1 = rotate(words[index - 2], 17) ^
                                     rotate(words[index - 2], 19) ^
                                     (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int index = 0; index < 64; ++index) {
            const std::uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + choose + K[index] + words[index];
            const std::uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
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

[[nodiscard]] std::string hex_digest(
  const std::array<std::uint8_t, 32>& digest) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest)
        output << std::setw(2) << unsigned(byte);
    return output.str();
}

[[nodiscard]] std::uint32_t read_u32(const std::uint8_t* bytes) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

struct PackedTable {
    std::vector<std::uint8_t> wdl;
    std::array<std::uint8_t, 32> sha{};

    [[nodiscard]] Wdl result(std::uint32_t index) const {
        return static_cast<Wdl>((wdl.at(index / 4) >>
          (2 * (index % 4))) & 3);
    }
};

[[nodiscard]] PackedTable load_table(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open double-Jester tablebase: " + path);
    std::array<std::uint8_t, 56> header{};
    input.read(reinterpret_cast<char*>(header.data()), header.size());
    if (static_cast<std::size_t>(input.gcount()) != header.size() ||
        std::memcmp(header.data(), "UFTB1\0\0\0", 8) != 0)
        throw std::runtime_error("invalid double-Jester tablebase header");
    const std::uint32_t version = read_u32(header.data() + 8);
    const std::uint32_t wdlBytes = read_u32(header.data() + 28);
    if (version < 5 || version > 6 ||
        read_u32(header.data() + 12) != static_cast<std::uint32_t>(PieceType::Jester) ||
        read_u32(header.data() + 16) != ConcreteStateCount ||
        read_u32(header.data() + 24) != 1 ||
        wdlBytes != (ConcreteStateCount + 3) / 4 ||
        read_u32(header.data() + 40) != static_cast<std::uint32_t>(PieceType::Jester) ||
        read_u32(header.data() + 44) != static_cast<std::uint32_t>(Color::White))
        throw std::runtime_error("tablebase does not match K+2 Jesters-v-K codec");
    const std::size_t planeOffset = 40 + 8 + (version >= 6 ? 8 : 0);
    PackedTable table;
    table.wdl.resize(wdlBytes);
    input.seekg(static_cast<std::streamoff>(planeOffset));
    input.read(reinterpret_cast<char*>(table.wdl.data()), table.wdl.size());
    if (static_cast<std::size_t>(input.gcount()) != table.wdl.size())
        throw std::runtime_error("truncated double-Jester WDL plane");

    input.clear();
    input.seekg(0);
    Sha256 hasher;
    std::array<std::uint8_t, 1 << 20> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const std::streamsize count = input.gcount();
        if (count > 0)
            hasher.update(buffer.data(), static_cast<std::size_t>(count));
    }
    table.sha = hasher.finish();
    return table;
}

[[nodiscard]] std::uint32_t encode_lower(Color side,
                                         std::uint8_t ownerKing,
                                         std::uint8_t enemyKing,
                                         std::uint8_t jester) {
    const std::uint32_t blackRank = enemyKing - (enemyKing > ownerKing ? 1u : 0u);
    const std::uint8_t low = std::min(ownerKing, enemyKing);
    const std::uint8_t high = std::max(ownerKing, enemyKing);
    const std::uint32_t jesterRank = jester - (jester > low ? 1u : 0u) -
                                     (jester > high ? 1u : 0u);
    return (((static_cast<std::uint32_t>(side) * Squares + ownerKing) *
             (Squares - 1) + blackRank) * (Squares - 2) + jesterRank);
}

struct LowerState {
    Color side = Color::White;
    std::uint8_t ownerKing = 0;
    std::uint8_t enemyKing = 0;
    std::uint8_t jester = 0;
};

[[nodiscard]] LowerState decode_lower(std::uint32_t index) {
    if (index >= LowerStateCount)
        throw std::runtime_error("lower Jester index is out of range");
    const std::uint32_t jesterRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t enemyRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint8_t ownerKing = static_cast<std::uint8_t>(index % Squares);
    const Color side = static_cast<Color>(index / Squares);
    const std::uint8_t enemyKing = static_cast<std::uint8_t>(
      enemyRank + (enemyRank >= ownerKing));
    const std::uint8_t low = std::min(ownerKing, enemyKing);
    const std::uint8_t high = std::max(ownerKing, enemyKing);
    std::uint8_t jester = static_cast<std::uint8_t>(jesterRank);
    if (jester >= low)
        ++jester;
    if (jester >= high)
        ++jester;
    return {side, ownerKing, enemyKing, jester};
}

class LowerJesterOverlay {
  public:
    LowerJesterOverlay(const std::string& path,
                       const std::string& concretePath,
                       const std::string& expectedSourceSha256,
                       const std::string& expectedModelSha256) {
        if (path.empty())
            throw std::runtime_error(
              "K+2 Jesters-v-K requires --lower-information-overlay");
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open lower Jester overlay: " + path);
        std::array<char, 160> header{};
        input.read(header.data(), header.size());
        if (static_cast<std::size_t>(input.gcount()) != header.size() ||
            std::memcmp(header.data(), "UFIW2\0\0\0", 8) != 0)
            throw std::runtime_error("invalid lower Jester overlay header");
        const auto word = [&](std::size_t offset) {
            return read_u32(reinterpret_cast<const std::uint8_t*>(
              header.data() + offset));
        };
        if (word(8) != 2 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Jester) ||
            word(16) != static_cast<std::uint32_t>(PieceType::Count) ||
            word(20) != static_cast<std::uint32_t>(Color::White) ||
            word(24) != LowerStateCount || word(28) != 1)
            throw std::runtime_error("lower overlay is not K+Jester-v-K");
        const std::string source(header.data() + 32, 64);
        const std::string model(header.data() + 96, 64);
        if (expectedSourceSha256.size() != 64 || source != expectedSourceSha256)
            throw std::runtime_error(
              "lower overlay concrete-source SHA-256 mismatch");
        if (expectedModelSha256.size() != 64 || model != expectedModelSha256)
            throw std::runtime_error(
              "lower overlay information-model SHA-256 mismatch");
        flags_.resize(LowerStateCount);
        input.read(reinterpret_cast<char*>(flags_.data()), flags_.size());
        if (static_cast<std::size_t>(input.gcount()) != flags_.size())
            throw std::runtime_error("truncated lower Jester overlay flags");
        char trailing = 0;
        if (input.read(&trailing, 1))
            throw std::runtime_error("lower Jester overlay has trailing bytes");

        std::ifstream concrete(concretePath, std::ios::binary);
        if (!concrete)
            throw std::runtime_error(
              "cannot open lower concrete Jester tablebase: " + concretePath);
        std::array<std::uint8_t, 56> concreteHeader{};
        concrete.read(reinterpret_cast<char*>(concreteHeader.data()),
                      concreteHeader.size());
        if (static_cast<std::size_t>(concrete.gcount()) != concreteHeader.size() ||
            std::memcmp(concreteHeader.data(), "UFTB1\0\0\0", 8) != 0)
            throw std::runtime_error("invalid lower concrete Jester header");
        const std::uint32_t version = read_u32(concreteHeader.data() + 8);
        const std::uint32_t wdlBytes = read_u32(concreteHeader.data() + 28);
        if (version < 4 || version > 6 ||
            read_u32(concreteHeader.data() + 12) !=
              static_cast<std::uint32_t>(PieceType::Jester) ||
            read_u32(concreteHeader.data() + 16) != LowerStateCount ||
            read_u32(concreteHeader.data() + 24) != 1 ||
            wdlBytes != (LowerStateCount + 3) / 4 ||
            (version >= 5 && read_u32(concreteHeader.data() + 40) !=
              static_cast<std::uint32_t>(PieceType::Count)))
            throw std::runtime_error(
              "lower concrete tablebase is not K+Jester-v-K");
        const std::size_t planeOffset = 40 + (version >= 5 ? 8 : 0) +
                                        (version >= 6 ? 8 : 0);
        concreteWdl_.resize(wdlBytes);
        concrete.seekg(static_cast<std::streamoff>(planeOffset));
        concrete.read(reinterpret_cast<char*>(concreteWdl_.data()),
                      concreteWdl_.size());
        if (static_cast<std::size_t>(concrete.gcount()) != concreteWdl_.size())
            throw std::runtime_error("truncated lower concrete Jester WDL plane");
        concrete.clear();
        concrete.seekg(0);
        Sha256 hasher;
        std::array<std::uint8_t, 1 << 20> buffer{};
        while (concrete) {
            concrete.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            const std::streamsize count = concrete.gcount();
            if (count > 0)
                hasher.update(buffer.data(), static_cast<std::size_t>(count));
        }
        if (hex_digest(hasher.finish()) != expectedSourceSha256)
            throw std::runtime_error(
              "lower concrete table SHA-256 does not match overlay binding");
    }

    struct Probe {
        std::uint32_t index = 0;
        Color owner = Color::White;
    };

    [[nodiscard]] Probe probe(const Position& position) const {
        std::array<int, 2> kings{{Position::NoPiece, Position::NoPiece}};
        int jester = Position::NoPiece;
        int live = 0;
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive || !piece.onBoard)
                continue;
            ++live;
            if (piece.type == PieceType::King)
                kings[color_index(piece.color)] = id;
            else if (piece.type == PieceType::Jester && jester == Position::NoPiece)
                jester = id;
            else
                throw std::runtime_error(
                  "non-Jester lower material left the closed double-Jester class");
        }
        if (live != 3 || jester == Position::NoPiece ||
            kings[0] == Position::NoPiece || kings[1] == Position::NoPiece)
            throw std::runtime_error(
              "nonterminal successor is not K+Jester-v-K");
        const Color owner = position.piece(jester).color;
        const Color mappedSide = owner == Color::White
                               ? position.side_to_move() : ~position.side_to_move();
        const std::uint32_t index = encode_lower(
          mappedSide,
          position.piece(kings[color_index(owner)]).square,
          position.piece(kings[color_index(~owner)]).square,
          position.piece(jester).square);
        return {index, owner};
    }

    [[nodiscard]] bool information_forces(const Probe& probe,
                                          Color target) const {
        const std::uint8_t flags = flags_.at(probe.index);
        if (!(flags & 4))
            throw std::runtime_error(
              "lower Jester successor is outside its admitted overlay domain");
        return (flags & (target == probe.owner ? 1 : 2)) != 0;
    }

    [[nodiscard]] bool exact_forces(const Probe& probe, Color target) const {
        const Wdl result = static_cast<Wdl>(
          (concreteWdl_.at(probe.index / 4) >> (2 * (probe.index % 4))) & 3);
        const Color side = decode_lower(probe.index).side;
        const Color mappedTarget = target == probe.owner
                                 ? Color::White : Color::Black;
        return (result == Wdl::Win && mappedTarget == side) ||
               (result == Wdl::Loss && mappedTarget != side);
    }

    [[nodiscard]] std::uint32_t alternative(std::uint32_t index) const {
        const LowerState state = decode_lower(index);
        return encode_lower(state.side, state.jester,
                            state.enemyKing, state.ownerKing);
    }

    void require_canonical_pair(const Probe& first,
                                const Probe& second) const {
        if (first.owner != second.owner ||
            alternative(first.index) != second.index ||
            alternative(second.index) != first.index)
            throw std::runtime_error(
              "lower Jester observation is not the exact canonical royal pair");
        (void)information_forces(first, first.owner);
        (void)information_forces(second, second.owner);
    }

  private:
    std::vector<std::uint8_t> flags_;
    std::vector<std::uint8_t> concreteWdl_;
};

void append_word(std::string& output, std::int32_t value) {
    const std::uint32_t word = static_cast<std::uint32_t>(value);
    for (unsigned shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<char>((word >> shift) & 0xff));
}

[[nodiscard]] std::int32_t public_type(const PieceState& piece) {
    if (piece.color == Color::White &&
        (piece.type == PieceType::King || piece.type == PieceType::Jester))
        return static_cast<std::int32_t>(PieceType::Count) + 1;
    return static_cast<std::int32_t>(piece.type);
}

// Fixed-width specialization of view_key() for this stateless closed class.
// codec_self_test() checks its equivalence relation against the general public
// model.  Terminal result remains part of the observation, which is what
// separates a captured-King world from a continuing captured-Jester world.
[[nodiscard]] std::string compact_view_key(const Position& position) {
    using Record = std::array<std::int32_t, 13>;
    std::vector<Record> records;
    records.reserve(position.piece_count());
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive)
            continue;
        if (piece.link != Position::NoPiece || piece.host != Position::NoPiece)
            throw std::runtime_error(
              "double-Jester projection encountered a relationship piece");
        records.push_back({
          public_type(piece), static_cast<std::int32_t>(piece.color),
          piece.square, piece.onBoard, piece.action, piece.cooldown,
          piece.freezeCount, piece.power, piece.moved, piece.visible,
          piece.attachmentOrder, piece.link, piece.host});
    }
    std::sort(records.begin(), records.end());
    std::string output;
    output.reserve((12 + records.size() * 13) * sizeof(std::uint32_t));
    append_word(output, 2);
    append_word(output, static_cast<std::int32_t>(position.side_to_move()));
    append_word(output, static_cast<std::int32_t>(position.continuation()));
    const int forced = position.forced_piece();
    append_word(output, forced == Position::NoPiece ? -1
                                                    : public_type(position.piece(forced)));
    append_word(output, forced == Position::NoPiece ? -1
                                                    : position.piece(forced).square);
    append_word(output, position.en_passant_square());
    const int victim = position.en_passant_victim();
    append_word(output, victim == Position::NoPiece ? -1
                                                    : public_type(position.piece(victim)));
    append_word(output, victim == Position::NoPiece ? -1
                                                    : position.piece(victim).square);
    const auto timeout = position.forced_timeout_winner();
    append_word(output, timeout ? static_cast<std::int32_t>(*timeout) : -1);
    std::int32_t terminal = 0;
    if (position.game_over()) {
        const auto winner = position.winner();
        terminal = winner ? 2 + static_cast<std::int32_t>(*winner) : 1;
    }
    append_word(output, terminal);
    append_word(output, static_cast<std::int32_t>(records.size()));
    for (const Record& record : records)
        for (const std::int32_t field : record)
            append_word(output, field);
    return output;
}

[[nodiscard]] std::string compact_transition_key(
  const Position& before, const Move& move, const Position& after) {
    std::string output;
    output.reserve(32 + 13 * 4 * 4);
    append_word(output, 2);
    const int actor = move.kind == MoveKind::Pass
                    ? Position::NoPiece : before.piece_on(move.from);
    append_word(output, actor == Position::NoPiece ? -1
                                                  : public_type(before.piece(actor)));
    append_word(output, static_cast<std::int32_t>(move.kind));
    append_word(output, move.kind == MoveKind::Pass ? -1 : move.from);
    append_word(output, move.kind == MoveKind::Pass ? -1 : move.to);
    append_word(output, static_cast<std::int32_t>(move.promotion));
    output += compact_view_key(after);
    return output;
}

struct BeliefKey {
    std::array<std::uint32_t, 3> worlds{{NoActual, NoActual, NoActual}};
    std::uint32_t count = 0;

    friend bool operator==(const BeliefKey& lhs, const BeliefKey& rhs) {
        return lhs.count == rhs.count && lhs.worlds == rhs.worlds;
    }
};

static_assert(sizeof(BeliefKey) == 16);

struct BeliefKeyHash {
    [[nodiscard]] std::size_t operator()(const BeliefKey& key) const {
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        const auto mix = [&](std::uint64_t value) {
            hash ^= value;
            hash *= 0x100000001b3ULL;
        };
        mix(key.count);
        for (const std::uint32_t world : key.worlds)
            mix(world);
        return static_cast<std::size_t>(hash);
    }
};

[[nodiscard]] std::uint32_t belief_size(const BeliefKey& key) {
    return key.count;
}

[[nodiscard]] std::vector<std::uint32_t> belief_worlds(const BeliefKey& key) {
    return {key.worlds.begin(), key.worlds.begin() + key.count};
}

// Refine only the bare-King mover's current information before it chooses an
// action. Ivory owns every ambiguous royal and already knows the actual world;
// its dots are private too, but cannot further refine Onyx's belief. Terminal
// positions have no decision boundary and therefore no dot observation.
[[nodiscard]] std::vector<std::vector<std::uint32_t>> decision_groups(
  std::vector<std::uint32_t> worlds) {
    if (worlds.empty())
        throw std::runtime_error("cannot refine an empty double-Jester belief");
    std::sort(worlds.begin(), worlds.end());
    worlds.erase(std::unique(worlds.begin(), worlds.end()), worlds.end());
    const Position first = make_oriented_position(worlds.front());
    if (first.side_to_move() != Color::Black || first.game_over())
        return {std::move(worlds)};

    const DisclosureContext blackView{Color::Black, false};
    std::map<std::string, std::vector<std::uint32_t>> groups;
    for (const std::uint32_t world : worlds) {
        const Position position = make_oriented_position(world);
        if (position.side_to_move() != Color::Black || position.game_over())
            throw std::runtime_error(
              "one pre-decision belief mixes mover or terminal state");
        groups[decision_observation_key(position, blackView)].push_back(world);
    }
    std::vector<std::vector<std::uint32_t>> result;
    result.reserve(groups.size());
    for (auto& [observation, members] : groups) {
        (void)observation;
        result.push_back(std::move(members));
    }
    return result;
}

void require_one_decision_group(const std::vector<std::uint32_t>& worlds) {
    if (decision_groups(worlds).size() != 1)
        throw std::runtime_error(
          "one double-Jester node crosses a private legal-dot key");
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
    std::vector<std::uint32_t> informedOffsets;
    std::vector<Successor> informedEdges;
    std::uint32_t commonActionCount = 0;
    std::vector<Successor> commonEdges;
};

struct RawChild {
    enum class Kind : std::uint8_t { SameClass, Terminal, LowerJester };

    Kind kind = Kind::Terminal;
    std::uint32_t index = NoActual;
    LowerJesterOverlay::Probe lower;
    bool whiteWins = false;
    bool blackWins = false;
};

struct TempEdge {
    ActionKey action;
    RawChild child;
    Successor successor;
};

class DoubleJesterGraph {
  public:
    DoubleJesterGraph(const PackedTable& concrete,
                      const LowerJesterOverlay& lower)
      : concrete_(concrete), lower_(lower),
        rootBelief_(ConcreteStateCount, NoBelief),
        admitted_(ConcreteStateCount, 0) {
        nodes_.reserve(PhysicalFrameCount);
        interner_.reserve(PhysicalFrameCount);
    }

    void build_roots() {
        const auto started = std::chrono::steady_clock::now();
        std::uint64_t frames = 0;
        std::uint64_t admittedWorlds = 0;
        std::array<std::uint64_t, 4> setSizes{};
        for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
            const auto frame = frame_worlds(index);
            const std::uint32_t representative = std::min({
              base_index(frame[0]), base_index(frame[1]), base_index(frame[2])});
            if (index != representative)
                continue;
            ++frames;
            std::map<std::string, std::vector<std::uint32_t>> groups;
            for (const std::uint32_t world : frame) {
                Position position = make_oriented_position(world);
                const bool accepted = !position.has_forced_action() &&
                  position.ordinary_predecessor_king_safe();
                admitted_[base_index(world)] = accepted ? 1 : 0;
                if (accepted) {
                    groups[compact_view_key(position)].push_back(world);
                    ++admittedWorlds;
                }
            }
            for (auto& [view, worlds] : groups) {
                (void)view;
                for (std::vector<std::uint32_t>& privateWorlds :
                     decision_groups(std::move(worlds))) {
                    const std::size_t size = privateWorlds.size();
                    if (!size || size > 3)
                        throw std::runtime_error(
                          "invalid double-Jester root private-set size");
                    ++setSizes[size];
                    ++rootSetCount_[color_index(decode_index(
                      base_index(privateWorlds.front())).side)];
                    if (size == 1)
                        continue;
                    const std::uint32_t belief = intern(std::move(privateWorlds));
                    for (const std::uint32_t world : belief_worlds(nodes_[belief])) {
                        const std::uint32_t concrete = base_index(world);
                        if (rootBelief_[concrete] != NoBelief)
                            throw std::runtime_error(
                              "double-Jester concrete root assigned twice");
                        rootBelief_[concrete] = belief;
                    }
                }
            }
            if (frames % 250'000 == 0) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - started).count();
                std::cout << "information_roots frames " << frames << '/'
                          << PhysicalFrameCount << " beliefs " << nodes_.size()
                          << " admitted " << admittedWorlds
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        if (frames != PhysicalFrameCount)
            throw std::runtime_error("physical-frame codec does not conserve domain");
        std::cout << "information_roots_complete frames " << frames
                  << " admitted " << admittedWorlds
                  << " singleton_sets " << setSizes[1]
                  << " pair_sets " << setSizes[2]
                  << " triple_sets " << setSizes[3]
                  << " ambiguous_beliefs " << nodes_.size()
                  << " peak_rss_bytes " << peak_rss_bytes() << '\n' << std::flush;
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
                          << " peak_rss_bytes " << peak_rss_bytes()
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        std::array<std::uint64_t, 4> sizes{};
        for (const BeliefKey& key : nodes_)
            ++sizes[key.count];
        std::cout << "information_graph_complete beliefs " << nodes_.size()
                  << " pairs " << sizes[2] << " triples " << sizes[3]
                  << " memberships " << worldMemberships_
                  << " observations " << observationEdges_
                  << " action_frontiers " << actionFrontiers_
                  << " legal_actions " << legalActions_
                  << " private_legal_dot_splits " << decisionSplits_
                  << " lower_pair_observations " << lowerPairObservations_
                  << " lower_pair_world_probes " << lowerPairWorldProbes_
                  << " lower_singleton_observations "
                  << lowerSingletonObservations_
                  << " lower_singleton_concrete_probes "
                  << lowerSingletonConcreteProbes_
                  << " peak_rss_bytes " << peak_rss_bytes() << '\n' << std::flush;
    }

    [[nodiscard]] const std::vector<BeliefKey>& nodes() const { return nodes_; }
    [[nodiscard]] const std::vector<std::uint32_t>& roots() const {
        return rootBelief_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& admitted() const {
        return admitted_;
    }
    [[nodiscard]] const std::array<std::uint64_t, 2>& root_set_counts() const {
        return rootSetCount_;
    }

    [[nodiscard]] GeneratedNode regenerate(std::uint32_t belief) {
        return generate(belief, false, true);
    }

    [[nodiscard]] std::vector<std::uint32_t> worlds(std::uint32_t belief) const {
        return belief_worlds(nodes_.at(belief));
    }

    [[nodiscard]] std::uint8_t common_action_count(std::uint32_t belief) const {
        const auto concrete = worlds(belief);
        if (decode_index(base_index(concrete.front())).side != Color::Black)
            return 0;
        std::vector<std::map<ActionKey, bool>> actions(concrete.size());
        for (std::size_t world = 0; world < concrete.size(); ++world) {
            const Position position = make_oriented_position(concrete[world]);
            if (position.game_over())
                return 0;
            for (const Move& move : position.legal_moves())
                if (!actions[world].emplace(ActionKey::from_move(move), true).second)
                    throw std::runtime_error(
                      "duplicate full action identity in legal frontier scan");
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
            throw std::runtime_error("uniform action count exceeds byte storage");
        return static_cast<std::uint8_t>(count);
    }

    [[nodiscard]] bool exact_index_forces(std::uint32_t index,
                                          Color target) const {
        const Wdl result = concrete_.result(index);
        const Color side = decode_index(index).side;
        return (result == Wdl::Win && target == side) ||
               (result == Wdl::Loss && target != side);
    }

  private:
    [[nodiscard]] static std::uint64_t peak_rss_bytes() {
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0)
            return 0;
#if defined(__APPLE__)
        return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
        return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
    }

    [[nodiscard]] static BeliefKey key_from_worlds(
      std::vector<std::uint32_t> concrete) {
        if (concrete.size() < 2 || concrete.size() > 3)
            throw std::runtime_error(
              "ambiguous double-Jester belief must contain two or three worlds");
        std::sort(concrete.begin(), concrete.end());
        concrete.erase(std::unique(concrete.begin(), concrete.end()), concrete.end());
        if (concrete.size() < 2 || concrete.size() > 3)
            throw std::runtime_error("double-Jester belief membership collapsed");
        require_one_decision_group(concrete);
        std::string commonCompact;
        BeliefKey key;
        key.count = static_cast<std::uint32_t>(concrete.size());
        for (std::size_t ordinal = 0; ordinal < concrete.size(); ++ordinal) {
            if (concrete[ordinal] >= OrientedWorldCount)
                throw std::runtime_error("belief contains an out-of-range world");
            key.worlds[ordinal] = concrete[ordinal];
            const Position position = make_oriented_position(concrete[ordinal]);
            const std::string compact = compact_view_key(position);
            if (commonCompact.empty()) {
                commonCompact = compact;
            }
            else if (compact != commonCompact)
                throw std::runtime_error(
                  "one double-Jester belief mixes public views");
        }
        return key;
    }

    [[nodiscard]] std::uint32_t intern(std::vector<std::uint32_t> worlds) {
        const BeliefKey key = key_from_worlds(std::move(worlds));
        if (const auto found = interner_.find(key); found != interner_.end())
            return found->second;
        if (nodes_.size() >= NoBelief)
            throw std::runtime_error("double-Jester belief id exceeds 32 bits");
        const std::uint32_t id = static_cast<std::uint32_t>(nodes_.size());
        worldMemberships_ += key.count;
        nodes_.push_back(key);
        if (!interner_.emplace(key, id).second)
            throw std::runtime_error("collision-checked belief interning failed");
        return id;
    }

    [[nodiscard]] std::uint32_t find_belief(
      std::vector<std::uint32_t> worlds) const {
        const BeliefKey key = key_from_worlds(std::move(worlds));
        const auto found = interner_.find(key);
        if (found == interner_.end())
            throw std::runtime_error(
              "equation regeneration found an undiscovered belief");
        return found->second;
    }

    [[nodiscard]] RawChild classify_child(const Position& child) const {
        RawChild result;
        if (const auto index = same_class_index(child)) {
            result.kind = RawChild::Kind::SameClass;
            result.index = *index;
            return result;
        }
        if (child.game_over()) {
            result.kind = RawChild::Kind::Terminal;
            const auto winner = child.winner();
            result.whiteWins = winner && *winner == Color::White;
            result.blackWins = winner && *winner == Color::Black;
            return result;
        }
        result.kind = RawChild::Kind::LowerJester;
        result.lower = lower_.probe(child);
        return result;
    }

    [[nodiscard]] GeneratedNode generate(std::uint32_t belief,
                                         bool allowNewBeliefs,
                                         bool retainEquations) {
        // Copy because interning observations may reallocate nodes_.
        const BeliefKey key = nodes_.at(belief);
        GeneratedNode node;
        node.worlds = belief_worlds(key);
        std::vector<Position> positions;
        positions.reserve(node.worlds.size());
        std::string commonView;
        for (const std::uint32_t world : node.worlds) {
            positions.push_back(make_oriented_position(world));
            const std::string current = compact_view_key(positions.back());
            if (commonView.empty())
                commonView = current;
            else if (current != commonView)
                throw std::runtime_error("interned double-Jester view diverged");
        }
        node.mover = positions.front().side_to_move();
        if (!std::all_of(positions.begin(), positions.end(), [&](const Position& position) {
                return position.side_to_move() == node.mover;
            }))
            throw std::runtime_error("belief mixes sides to move");
        const bool terminal = positions.front().game_over();
        if (!std::all_of(positions.begin(), positions.end(), [&](const Position& position) {
                return position.game_over() == terminal;
            }))
            throw std::runtime_error("belief mixes terminal and live worlds");
        if (terminal) {
            node.terminal = true;
            node.terminalBlack = true;
            for (const Position& position : positions) {
                const auto winner = position.winner();
                node.terminalWhite.push_back(winner && *winner == Color::White);
                node.terminalBlack = node.terminalBlack &&
                                     winner && *winner == Color::Black;
            }
            return node;
        }

        using Member = std::pair<std::uint32_t, std::uint32_t>;
        std::vector<std::vector<TempEdge>> edges(node.worlds.size());
        std::unordered_map<std::string, std::vector<Member>> byObservation;
        for (std::size_t world = 0; world < positions.size(); ++world) {
            const auto moves = positions[world].legal_moves();
            if (allowNewBeliefs) {
                ++actionFrontiers_;
                legalActions_ += moves.size();
            }
            edges[world].reserve(moves.size());
            for (const Move& move : moves) {
                Position child = positions[world];
                Undo undo;
                if (!child.make_move(move, undo))
                    throw std::runtime_error("legal double-Jester edge failed");
                TempEdge edge;
                edge.action = ActionKey::from_move(move);
                edge.child = classify_child(child);
                const std::string observation = compact_transition_key(
                  positions[world], move, child);
                edges[world].push_back(std::move(edge));
                byObservation[observation].push_back({
                  static_cast<std::uint32_t>(world),
                  static_cast<std::uint32_t>(edges[world].size() - 1)});
                ++observationEdges_;
            }
        }

        for (const auto& [observation, members] : byObservation) {
            (void)observation;
            std::vector<std::uint32_t> sameClass;
            std::vector<LowerJesterOverlay::Probe> lower;
            bool terminal = false;
            bool allTerminalBlackWin = true;
            for (const auto [world, edge] : members) {
                const RawChild& child = edges[world][edge].child;
                if (child.kind == RawChild::Kind::SameClass)
                    sameClass.push_back(child.index);
                else if (child.kind == RawChild::Kind::LowerJester)
                    lower.push_back(child.lower);
                else {
                    terminal = true;
                    allTerminalBlackWin = allTerminalBlackWin && child.blackWins;
                }
            }
            std::sort(sameClass.begin(), sameClass.end());
            sameClass.erase(std::unique(sameClass.begin(), sameClass.end()),
                            sameClass.end());
            std::sort(lower.begin(), lower.end(),
                      [](const auto& lhs, const auto& rhs) {
                          return std::tie(lhs.owner, lhs.index) <
                                 std::tie(rhs.owner, rhs.index);
                      });
            lower.erase(std::unique(lower.begin(), lower.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.owner == rhs.owner && lhs.index == rhs.index;
              }), lower.end());
            const int materialKinds = int(!sameClass.empty()) + int(!lower.empty()) +
                                      int(terminal);
            if (materialKinds != 1)
                throw std::runtime_error(
                  "one observation is empty or mixes material/terminal classes");
            std::map<std::uint32_t, std::uint32_t> childBeliefs;
            if (!sameClass.empty()) {
                std::vector<std::vector<std::uint32_t>> privateGroups =
                  decision_groups(sameClass);
                if (allowNewBeliefs && privateGroups.size() > 1)
                    ++decisionSplits_;
                for (std::vector<std::uint32_t>& privateWorlds : privateGroups) {
                    if (privateWorlds.size() < 2)
                        continue;
                    const std::uint32_t childBelief = allowNewBeliefs
                      ? intern(privateWorlds) : find_belief(privateWorlds);
                    for (const std::uint32_t child : privateWorlds)
                        if (!childBeliefs.emplace(child, childBelief).second)
                            throw std::runtime_error(
                              "one child entered two private legal-dot groups");
                }
            }
            bool lowerPair = false;
            if (!lower.empty()) {
                if (lower.size() == 2) {
                    lower_.require_canonical_pair(lower[0], lower[1]);
                    lowerPair = true;
                    if (allowNewBeliefs) {
                        ++lowerPairObservations_;
                        lowerPairWorldProbes_ += 2;
                    }
                }
                else if (lower.size() == 1) {
                    if (allowNewBeliefs) {
                        ++lowerSingletonObservations_;
                        ++lowerSingletonConcreteProbes_;
                    }
                }
                else
                    throw std::runtime_error(
                      "lower Jester observation is neither singleton nor pair");
            }
            for (const auto [world, edge] : members) {
                const RawChild& child = edges[world][edge].child;
                Successor& successor = edges[world][edge].successor;
                const auto childBelief = child.kind == RawChild::Kind::SameClass
                  ? childBeliefs.find(child.index) : childBeliefs.end();
                if (child.kind == RawChild::Kind::SameClass &&
                    childBelief != childBeliefs.end()) {
                    successor.belief = childBelief->second;
                    successor.actual = child.index;
                }
                else if (child.kind == RawChild::Kind::SameClass) {
                    successor.exactWhite = exact_index_forces(
                      base_index(child.index), Color::White);
                    successor.exactBlack = exact_index_forces(
                      base_index(child.index), Color::Black);
                }
                else if (child.kind == RawChild::Kind::LowerJester && lowerPair) {
                    successor.exactWhite = lower_.information_forces(
                      child.lower, Color::White);
                    // A corrected v2 lower overlay already incorporates the
                    // bare-King mover's private dot refinement. Its two actual
                    // worlds need not have equal flags after that observation.
                    successor.exactBlack = lower_.information_forces(
                      child.lower, Color::Black);
                }
                else if (child.kind == RawChild::Kind::LowerJester) {
                    successor.exactWhite = lower_.exact_forces(
                      child.lower, Color::White);
                    successor.exactBlack = lower_.exact_forces(
                      child.lower, Color::Black);
                }
                else {
                    successor.exactWhite = child.whiteWins;
                    successor.exactBlack = allTerminalBlackWin;
                }
                if (successor.exactWhite && successor.exactBlack)
                    throw std::runtime_error(
                      "one exact/lower successor gives both sides a forced win");
            }
        }

        if (!retainEquations)
            return node;
        if (node.mover == Color::White) {
            node.informedOffsets.push_back(0);
            for (const auto& worldEdges : edges) {
                for (const TempEdge& edge : worldEdges)
                    node.informedEdges.push_back(edge.successor);
                node.informedOffsets.push_back(
                  static_cast<std::uint32_t>(node.informedEdges.size()));
            }
        }
        else {
            std::vector<std::map<ActionKey, Successor>> byAction(edges.size());
            for (std::size_t world = 0; world < edges.size(); ++world)
                for (const TempEdge& edge : edges[world])
                    if (!byAction[world].emplace(edge.action, edge.successor).second)
                        throw std::runtime_error(
                          "duplicate public action in one concrete world");
            for (const auto& [action, first] : byAction.front()) {
                (void)first;
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
        return node;
    }

    const PackedTable& concrete_;
    const LowerJesterOverlay& lower_;
    std::vector<BeliefKey> nodes_;
    std::unordered_map<BeliefKey, std::uint32_t, BeliefKeyHash> interner_;
    std::vector<std::uint32_t> rootBelief_;
    std::vector<std::uint8_t> admitted_;
    std::array<std::uint64_t, 2> rootSetCount_{};
    std::uint64_t worldMemberships_ = 0;
    std::uint64_t observationEdges_ = 0;
    std::uint64_t actionFrontiers_ = 0;
    std::uint64_t legalActions_ = 0;
    std::uint64_t decisionSplits_ = 0;
    std::uint64_t lowerPairObservations_ = 0;
    std::uint64_t lowerPairWorldProbes_ = 0;
    std::uint64_t lowerSingletonObservations_ = 0;
    std::uint64_t lowerSingletonConcreteProbes_ = 0;
};

struct SolvedInformation {
    std::vector<std::uint8_t> flags;
    InformationSolveSummary fixedPoint;
    std::uint64_t variables = 0;
    std::uint64_t softLocks = 0;
};

[[nodiscard]] SolvedInformation solve_graph(DoubleJesterGraph& graph,
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
        const auto worlds = graph.worlds(static_cast<std::uint32_t>(belief));
        if (decode_index(base_index(worlds.front())).side == Color::Black) {
            const Position first = make_oriented_position(worlds.front());
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
        if ((belief + 1) % 500'000 == 0)
            std::cout << "information_action_scan " << belief + 1 << '/'
                      << nodes.size() << " common_actions "
                      << auxiliaryBase[belief + 1] << " soft_locks "
                      << softLocks << '\n' << std::flush;
    }
    std::cout << "information_soft_locks sets " << softLocks
              << " first_belief ";
    if (firstSoftLock == NoBelief)
        std::cout << "none\n";
    else
        std::cout << firstSoftLock << " example "
                  << make_oriented_position(
                       graph.worlds(firstSoftLock).front()).upn()
                  << '\n';

    const std::uint64_t variableCount = auxiliaryStart + auxiliaryBase.back();
    if (!variableCount || variableCount >= InformationTrue)
        throw std::runtime_error("double-Jester information variables exceed token domain");
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
        const auto worlds = graph.worlds(successor.belief);
        const auto found = std::lower_bound(
          worlds.begin(), worlds.end(), successor.actual);
        if (found == worlds.end() || *found != successor.actual)
            throw std::runtime_error("actual world absent from successor belief");
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
              "uniform action count changed during equation regeneration");
        if (node.mover == Color::White) {
            tokens.clear();
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                std::vector<InformationToken> choices;
                for (std::size_t edge = node.informedOffsets[world];
                     edge < node.informedOffsets[world + 1]; ++edge) {
                    choices.push_back(white_token(node.informedEdges[edge]));
                    tokens.push_back(black_token(node.informedEdges[edge]));
                }
                solver.define_or(white_variable(belief, world), choices);
            }
            // The informed Ivory owner may choose any action in each actual
            // world.  Onyx forces a win only if every such action in every
            // world remains winning under the resulting observation.
            solver.define_and(black_variable(belief), tokens);
        }
        else {
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                tokens.clear();
                for (std::size_t action = 0;
                     action < node.commonActionCount; ++action)
                    tokens.push_back(white_token(node.commonEdges[
                      action * node.worlds.size() + world]));
                solver.define_and(white_variable(belief, world), tokens);
            }
            std::vector<InformationToken> actionGates;
            for (std::size_t action = 0; action < node.commonActionCount; ++action) {
                tokens.clear();
                for (std::size_t world = 0; world < node.worlds.size(); ++world)
                    tokens.push_back(black_token(node.commonEdges[
                      action * node.worlds.size() + world]));
                const InformationToken gate = auxiliary_variable(belief, action);
                solver.define_and(gate, tokens);
                actionGates.push_back(gate);
            }
            solver.define_or(black_variable(belief), actionGates);
        }
        if ((belief + 1) % 500'000 == 0)
            std::cout << "information_equations_defined " << belief + 1 << '/'
                      << nodes.size() << '\n' << std::flush;
    }

    const InformationSolveSummary summary = solver.solve();
    if (summary.bellmanResidual || summary.rankResidual)
        throw std::runtime_error("double-Jester fixed point has a residual");
    std::cout << "information_fixed_point variables " << summary.variables
              << " reverse_edges " << summary.reverseEdges
              << " activated " << summary.activated
              << " bellman_residual " << summary.bellmanResidual
              << " rank_residual " << summary.rankResidual << '\n';

    SolvedInformation solved;
    solved.flags.resize(ConcreteStateCount, 0);
    solved.fixedPoint = summary;
    solved.variables = variableCount;
    solved.softLocks = softLocks;
    for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
        if (!graph.admitted()[index])
            continue;
        const std::uint32_t belief = graph.roots()[index];
        bool white = false;
        bool black = false;
        if (belief == NoBelief) {
            white = graph.exact_index_forces(index, Color::White);
            black = graph.exact_index_forces(index, Color::Black);
        }
        else {
            const auto worlds = graph.worlds(belief);
            const auto found = std::find_if(worlds.begin(), worlds.end(),
              [&](std::uint32_t world) { return base_index(world) == index; });
            if (found == worlds.end())
                throw std::runtime_error("root belief omits actual world");
            const std::size_t ordinal = static_cast<std::size_t>(found - worlds.begin());
            white = solver.value(white_variable(belief, ordinal));
            black = solver.value(black_variable(belief));
        }
        if (white && black)
            throw std::runtime_error("both teams force a win in one actual root");
        solved.flags[index] = 4 | (white ? 1 : 0) | (black ? 2 : 0);
    }
    return solved;
}

void report_results(const DoubleJesterGraph& graph,
                    const PackedTable& concrete,
                    const SolvedInformation& solved) {
    using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
    using Cross = std::array<std::array<std::array<std::uint64_t, 4>, 4>, 2>;
    Counts totals{};
    Counts unreachable{};
    Cross cross{};
    for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
        const FourState state = decode_index(index);
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
    }
    for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t conserved = 0;
        for (std::size_t result = 1; result < 4; ++result)
            conserved += totals[side][result] + unreachable[side][result];
        if (conserved != ConcreteStateCount / 2)
            throw std::runtime_error("double-Jester summary does not conserve roots");
        std::cout << "information_summary side " << side
                  << " win " << totals[side][1]
                  << " loss " << totals[side][2]
                  << " draw " << totals[side][3]
                  << " unreachable_win " << unreachable[side][1]
                  << " unreachable_loss " << unreachable[side][2]
                  << " unreachable_draw " << unreachable[side][3]
                  << " sets " << graph.root_set_counts()[side]
                  << " concrete " << ConcreteStateCount / 2
                  << " bellman_residual " << solved.fixedPoint.bellmanResidual
                  << " rank_residual " << solved.fixedPoint.rankResidual
                  << " belief_cap none exhaustive 1\n";
        for (std::size_t exact = 1; exact < 4; ++exact)
            std::cout << "information_crosstab side " << side
                      << " concrete " << exact
                      << " information_win " << cross[side][exact][1]
                      << " information_loss " << cross[side][exact][2]
                      << " information_draw " << cross[side][exact][3] << '\n';
    }
}

[[nodiscard]] bool valid_sha256(const std::string& text) {
    return text.size() == 64 &&
      std::all_of(text.begin(), text.end(), [](char character) {
          return (character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'f');
      });
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_overlay(const std::string& path,
                   const PackedTable& concrete,
                   const SolvedInformation& solved,
                   const std::string& sourceSha256,
                   const std::string& modelSha256) {
    if (path.empty())
        return;
    if (!valid_sha256(sourceSha256) || !valid_sha256(modelSha256))
        throw std::runtime_error(
          "UFIW2 requires lowercase 64-digit source/model SHA-256 values");
    if (sourceSha256 != hex_digest(concrete.sha))
        throw std::runtime_error("double-Jester source SHA-256 mismatch");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("cannot create double-Jester overlay: " + path);
    const std::array<char, 8> magic{{'U','F','I','W','2','\0','\0','\0'}};
    output.write(magic.data(), magic.size());
    for (const std::uint32_t value : {
           2u, static_cast<std::uint32_t>(PieceType::Jester),
           static_cast<std::uint32_t>(PieceType::Jester),
           static_cast<std::uint32_t>(Color::White), ConcreteStateCount, 1u})
        write_u32(output, value);
    output.write(sourceSha256.data(), sourceSha256.size());
    output.write(modelSha256.data(), modelSha256.size());
    output.write(reinterpret_cast<const char*>(solved.flags.data()),
                 solved.flags.size());
    if (!output)
        throw std::runtime_error("failed writing double-Jester overlay");
    std::cout << "information_overlay " << path
              << " bytes " << 160 + solved.flags.size()
              << " concrete_sha256 " << sourceSha256
              << " solver_model_sha256 " << modelSha256
              << " variables " << solved.variables << '\n';
}

void codec_self_test() {
    // Exhaust every stored byte of the collision-free action codec.  The five
    // byte lanes are disjoint, and decode is checked against Move::operator==;
    // the full solve additionally rejects duplicate ActionKeys in every legal
    // frontier it visits.
    for (int field = 0; field < 5; ++field) {
        std::array<ActionKey, 256> keys{};
        for (int value = 0; value < 256; ++value) {
            Move move;
            if (field == 0)
                move.from = static_cast<std::uint8_t>(value);
            else if (field == 1)
                move.to = static_cast<std::uint8_t>(value);
            else if (field == 2)
                move.auxiliary = static_cast<std::uint8_t>(value);
            else if (field == 3)
                move.kind = static_cast<MoveKind>(value);
            else
                move.promotion = static_cast<PieceType>(value);
            keys[value] = ActionKey::from_move(move);
            if (!(keys[value].decode() == move))
                throw std::runtime_error(
                  "full Move action codec is not an involution");
        }
        std::sort(keys.begin(), keys.end());
        if (std::adjacent_find(keys.begin(), keys.end()) != keys.end())
            throw std::runtime_error(
              "full Move action codec aliases one byte field");
    }

    for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
        const FourState state = decode_index(index);
        if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2 ||
            state.whiteKing == state.blackKing ||
            state.whiteKing == state.first || state.whiteKing == state.second ||
            state.blackKing == state.first || state.blackKing == state.second ||
            state.first == state.second || encode_index(state) != index)
            throw std::runtime_error("double-Jester concrete codec is not bijective");
    }
    for (std::uint32_t index = 0; index < LowerStateCount; ++index) {
        const LowerState state = decode_lower(index);
        if (encode_lower(state.side, state.ownerKing,
                         state.enemyKing, state.jester) != index)
            throw std::runtime_error("lower Jester codec is not bijective");
        const std::uint32_t other = encode_lower(
          state.side, state.jester, state.enemyKing, state.ownerKing);
        const LowerState alternativeState = decode_lower(other);
        if (encode_lower(alternativeState.side, alternativeState.jester,
                         alternativeState.enemyKing,
                         alternativeState.ownerKing) != index)
            throw std::runtime_error(
              "lower Jester royal-pair codec is not an involution");
    }
    constexpr std::uint32_t FrameSamples = 100'000;
    for (std::uint32_t sample = 0; sample < FrameSamples; ++sample) {
        const std::uint32_t index = static_cast<std::uint32_t>(
          std::uint64_t(ConcreteStateCount) * sample / FrameSamples);
        const auto frame = frame_worlds(index);
        for (const std::uint32_t world : frame)
            if (frame_worlds(base_index(world)) != frame)
                throw std::runtime_error(
                  "three-world royal-assignment codec is not an involution");
        const auto roundTrip = same_class_index(make_position(index));
        if (!roundTrip || *roundTrip != index * 2)
            throw std::runtime_error(
              "double-Jester Position/codec round trip failed");
    }
    FourState reflectionSample{Color::Black, 73, 4, 17, 62};
    FourState reflected = reflectionSample;
    reflected.whiteKing = horizontal_reflection(reflected.whiteKing);
    reflected.blackKing = horizontal_reflection(reflected.blackKing);
    reflected.first = horizontal_reflection(reflected.first);
    reflected.second = horizontal_reflection(reflected.second);
    if (encode_index(reflectionSample) != encode_index(reflected))
        throw std::runtime_error("double-Jester horizontal orbit mismatch");

    const DisclosureContext blackView{Color::Black, false};

    // Minimal native legal-dot witness, lifted into this exact material class:
    // Onyx owns the e7 King and sees identical Ivory royal silhouettes on
    // e6/e5/h10. Capturing e6 is a displayed dot when it is the real King, but
    // is absent when it is a Jester because e7-e6 would finish adjacent to the
    // real King on e5. The pre-decision private observation must split these
    // worlds, and the node codec must reject their unrefined union.
    const auto square = [](const char* name) {
        return static_cast<std::uint8_t>(Position::square_from_name(name));
    };
    const FourState dotKingE6{
      Color::Black, square("e6"), square("e7"),
      square("e5"), square("h10")};
    const FourState dotJesterE6{
      Color::Black, square("e5"), square("e7"),
      square("e6"), square("h10")};
    const Position kingE6 = make_position(dotKingE6);
    const Position jesterE6 = make_position(dotJesterE6);
    if (view_key(kingE6, blackView) != view_key(jesterE6, blackView) ||
        !kingE6.move_from_string("e7-e6") ||
        jesterE6.move_from_string("e7-e6") ||
        decision_observation_key(kingE6, blackView) ==
          decision_observation_key(jesterE6, blackView))
        throw std::runtime_error(
          "double-Jester e7/e6/e5 legal-dot witness residual");
    const std::vector<std::uint32_t> differingDots{{
      oriented_index(dotKingE6), oriented_index(dotJesterE6)}};
    if (decision_groups(differingDots).size() != 2)
        throw std::runtime_error(
          "different double-Jester legal dots did not split");
    bool rejectedCrossingNode = false;
    try {
        require_one_decision_group(differingDots);
    }
    catch (const std::runtime_error&) {
        rejectedCrossingNode = true;
    }
    if (!rejectedCrossingNode)
        throw std::runtime_error(
          "double-Jester node admitted two private legal-dot keys");

    // Equal frontiers must not over-refine. These remote royal assignments do
    // not change any legal marker available to the a1 Onyx King.
    const FourState equalFirst{
      Color::Black, square("e8"), square("a1"),
      square("f8"), square("g8")};
    const FourState equalSecond{
      Color::Black, square("f8"), square("a1"),
      square("e8"), square("g8")};
    const std::vector<std::uint32_t> equalDots{{
      oriented_index(equalFirst), oriented_index(equalSecond)}};
    if (decision_observation_key(make_position(equalFirst), blackView) !=
          decision_observation_key(make_position(equalSecond), blackView) ||
        decision_groups(equalDots).size() != 1)
        throw std::runtime_error(
          "equal double-Jester legal dots were over-refined");
    require_one_decision_group(equalDots);

    constexpr std::uint32_t ProjectionSamples = 2'000;
    for (std::uint32_t sample = 0; sample < ProjectionSamples; ++sample) {
        const std::uint32_t index = static_cast<std::uint32_t>(
          std::uint64_t(ConcreteStateCount) * sample / ProjectionSamples);
        const auto frame = frame_worlds(index);
        std::array<Position, 3> positions{{
          make_oriented_position(frame[0]), make_oriented_position(frame[1]),
          make_oriented_position(frame[2])}};
        for (std::size_t lhs = 0; lhs < positions.size(); ++lhs)
            for (std::size_t rhs = 0; rhs < positions.size(); ++rhs) {
                const bool compact = compact_view_key(positions[lhs]) ==
                                     compact_view_key(positions[rhs]);
                const bool general = view_key(positions[lhs], blackView) ==
                                     view_key(positions[rhs], blackView);
                if (compact != general)
                    throw std::runtime_error(
                      "compact double-Jester view diverges from public model");
            }
    }

    // Verify transition partitions rather than byte identity: the specialized
    // binary format and the general text format intentionally differ.
    constexpr std::uint32_t TransitionSamples = 10'000;
    for (std::uint32_t sample = 0; sample < TransitionSamples; ++sample) {
        const std::uint32_t index = static_cast<std::uint32_t>(
          std::uint64_t(ConcreteStateCount) * sample / TransitionSamples);
        const auto frame = frame_worlds(index);
        struct Observation {
            std::string compact;
            std::string general;
        };
        std::vector<Observation> observations;
        for (const std::uint32_t world : frame) {
            const Position before = make_oriented_position(world);
            for (const Move& move : before.legal_moves()) {
                Position after = before;
                Undo undo;
                if (!after.make_move(move, undo))
                    throw std::runtime_error(
                      "transition oracle fixture failed legal move");
                if (const auto child = same_class_index(after)) {
                    const Position reconstructed = make_oriented_position(*child);
                    const std::string afterKey = compact_view_key(after);
                    const std::string reconstructedKey =
                      compact_view_key(reconstructed);
                    if (afterKey != reconstructedKey) {
                        std::size_t difference = 0;
                        while (difference < afterKey.size() &&
                               difference < reconstructedKey.size() &&
                               afterKey[difference] == reconstructedKey[difference])
                            ++difference;
                        throw std::runtime_error(
                          "oriented successor codec does not preserve public view: " +
                          std::to_string(world) + " " +
                          before.move_to_string(move) + " child " +
                          std::to_string(*child) + " after " + after.upn() +
                          " reconstructed " + reconstructed.upn() + " diff " +
                          std::to_string(difference) + " after_word " +
                          std::to_string(read_u32(reinterpret_cast<const std::uint8_t*>(
                            afterKey.data() + difference - difference % 4))) +
                          " reconstructed_word " +
                          std::to_string(read_u32(reinterpret_cast<const std::uint8_t*>(
                            reconstructedKey.data() + difference - difference % 4))));
                    }
                }
                observations.push_back({
                  compact_transition_key(before, move, after),
                  transition_observation_key(before, move, after, blackView)});
            }
        }
        for (std::size_t lhs = 0; lhs < observations.size(); ++lhs)
            for (std::size_t rhs = 0; rhs < observations.size(); ++rhs)
                if ((observations[lhs].compact == observations[rhs].compact) !=
                    (observations[lhs].general == observations[rhs].general))
                    throw std::runtime_error(
                      "compact transition partition diverges from public model");
    }

    Sha256 sha;
    static constexpr char Test[] = "abc";
    sha.update(reinterpret_cast<const std::uint8_t*>(Test), 3);
    if (hex_digest(sha.finish()) !=
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        throw std::runtime_error("SHA-256 self-test failed");
    std::cout << "codecok concrete " << ConcreteStateCount
              << " frames " << PhysicalFrameCount
              << " involution_samples " << FrameSamples
              << " projection_samples " << ProjectionSamples
              << " transition_samples " << TransitionSamples
              << " action_fields 5 action_byte_values 256"
              << " legal_dot_split_residual 0"
              << " legal_dot_group_residual 0"
              << " legal_dot_crossing_residual 0"
              << " lower_codec " << LowerStateCount
              << " sha256 ok\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    using namespace Stockfish::Ultimate;
    try {
        std::string input = "tablebases/kjesterjesterk.uftb";
        std::string lowerOverlay;
        std::string lowerConcrete;
        std::string lowerSourceSha256;
        std::string lowerModelSha256;
        std::string output;
        std::string scratch = "/tmp";
        std::string sourceSha256;
        std::string modelSha256;
        bool selfTestOnly = false;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            const auto value = [&](const char* option) {
                if (index + 1 >= argc)
                    throw std::runtime_error(
                      std::string("missing value for ") + option);
                return std::string(argv[++index]);
            };
            if (argument == "--input")
                input = value("--input");
            else if (argument == "--lower-information-overlay")
                lowerOverlay = value("--lower-information-overlay");
            else if (argument == "--lower-concrete")
                lowerConcrete = value("--lower-concrete");
            else if (argument == "--lower-information-source-sha256")
                lowerSourceSha256 = value("--lower-information-source-sha256");
            else if (argument == "--lower-information-model-sha256")
                lowerModelSha256 = value("--lower-information-model-sha256");
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
            else
                throw std::runtime_error("unknown argument: " + argument);
        }
        codec_self_test();
        if (selfTestOnly)
            return 0;
        if (!valid_sha256(modelSha256))
            throw std::runtime_error(
              "solve requires --information-model-sha256");
        if (!selfTestOnly && !valid_sha256(lowerModelSha256))
            throw std::runtime_error(
              "solve requires --lower-information-model-sha256");
        const PackedTable concrete = load_table(input);
        if (sourceSha256.empty())
            sourceSha256 = hex_digest(concrete.sha);
        if (sourceSha256 != hex_digest(concrete.sha))
            throw std::runtime_error(
              "provided concrete source SHA-256 does not match input");
        std::cout << "information_input " << input
                  << " concrete " << ConcreteStateCount
                  << " wdl_bytes " << concrete.wdl.size()
                  << " sha256 " << sourceSha256 << '\n' << std::flush;
        const LowerJesterOverlay lower(
          lowerOverlay, lowerConcrete, lowerSourceSha256, lowerModelSha256);
        DoubleJesterGraph graph(concrete, lower);
        graph.build_roots();
        graph.expand_all();
        const SolvedInformation solved = solve_graph(graph, scratch);
        report_results(graph, concrete, solved);
        write_overlay(output, concrete, solved, sourceSha256, modelSha256);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "double-jester-information-tablebase: "
                  << error.what() << '\n';
        return 1;
    }
}
