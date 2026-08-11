/*
  Ultimate Fish - exact K+Jester-v-K+Jester information tablebase
  Copyright (C) 2026 Ultimate Fish contributors

  Standalone proof generator.  It deliberately has its own main() and is not
  part of the normal engine target.  The concrete public geometry contains two
  royal silhouettes per color.  A four-bit relation records which pair of
  private King assignments remains possible, while one six-bit equivalence
  partition per player preserves that player's exact private-observation
  history.  Before an action, only the mover's partition is refined by the
  legal dots they can inspect; after it, both partitions are refined by their
  respective complete transition observations.  This is the complete
  two-agent epistemic frame for this closed material class, not a Cartesian
  product of independently updated marginal beliefs.
*/

#include "information_solver.h"
#include "information.h"
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
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/resource.h>
#include <sys/statvfs.h>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t ConcreteCount =
  2 * (Squares / 2) * (Squares - 1) * (Squares - 2) * (Squares - 3);
constexpr std::uint32_t LowerCount =
  2 * Squares * (Squares - 1) * (Squares - 2);
constexpr std::uint32_t NoNode = std::numeric_limits<std::uint32_t>::max();
constexpr const char* SemanticsId = "fresh-maximal-public-view-v2";

enum class Wdl : std::uint8_t { Unknown, Win, Loss, Draw };

struct FourState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t whiteJester = 0;
    std::uint8_t blackJester = 0;
};

[[nodiscard]] constexpr std::uint8_t reflect_square(std::uint8_t square) {
    return static_cast<std::uint8_t>((square / 8) * 8 + 7 - square % 8);
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
            occupied = occupied || square == item;
        if (!occupied && rank-- == 0)
            return square;
    }
    throw std::runtime_error("four-model square rank is invalid");
}

[[nodiscard]] FourState canonicalize_concrete(FourState state) {
    if (state.whiteKing % 8 >= 4) {
        state.whiteKing = reflect_square(state.whiteKing);
        state.blackKing = reflect_square(state.blackKing);
        state.whiteJester = reflect_square(state.whiteJester);
        state.blackJester = reflect_square(state.blackJester);
    }
    return state;
}

[[nodiscard]] std::uint32_t encode_four(FourState state) {
    state = canonicalize_concrete(state);
    const std::uint32_t whiteRank =
      (state.whiteKing / 8) * 4 + state.whiteKing % 8;
    const std::uint32_t blackRank =
      rank_excluding(state.blackKing, {state.whiteKing});
    const std::uint32_t firstRank = rank_excluding(
      state.whiteJester, {state.whiteKing, state.blackKing});
    const std::uint32_t secondRank = rank_excluding(
      state.blackJester,
      {state.whiteKing, state.blackKing, state.whiteJester});
    return ((((static_cast<std::uint32_t>(state.side) * (Squares / 2) + whiteRank)
                * (Squares - 1) + blackRank)
               * (Squares - 2) + firstRank)
              * (Squares - 3) + secondRank);
}

[[nodiscard]] FourState decode_four(std::uint32_t index) {
    if (index >= ConcreteCount)
        throw std::runtime_error("four-model index is out of range");
    const std::uint32_t secondRank = index % (Squares - 3);
    index /= Squares - 3;
    const std::uint32_t firstRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    const Color side = static_cast<Color>(index / (Squares / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / 4) * 8 + whiteRank % 4);
    const std::uint8_t blackKing = unrank_excluding(blackRank, {whiteKing});
    const std::uint8_t whiteJester = unrank_excluding(
      firstRank, {whiteKing, blackKing});
    const std::uint8_t blackJester = unrank_excluding(
      secondRank, {whiteKing, blackKing, whiteJester});
    return {side, whiteKing, blackKing, whiteJester, blackJester};
}

[[nodiscard]] Position make_position(const FourState& state) {
    Position position;
    position.clear();
    const std::array<int, 4> pieces{{
      position.add_piece(PieceType::King, Color::White, state.whiteKing),
      position.add_piece(PieceType::King, Color::Black, state.blackKing),
      position.add_piece(PieceType::Jester, Color::White, state.whiteJester),
      position.add_piece(PieceType::Jester, Color::Black, state.blackJester)}};
    if (std::any_of(pieces.begin(), pieces.end(), [](int id) {
            return id == Position::NoPiece;
        }))
        throw std::runtime_error("four-model geometry failed Position construction");
    for (const int id : pieces)
        position.piece(id).moved = true;
    position.set_side_to_move(state.side);
    return position;
}

[[nodiscard]] std::uint8_t terminal_code(const Position& position) {
    if (!position.game_over())
        return 0;
    const auto winner = position.winner();
    if (!winner)
        return 1;
    return *winner == Color::White ? 2 : 3;
}

struct Geometry {
    Color side = Color::White;
    std::array<std::uint8_t, 2> white{{0, 0}};
    std::array<std::uint8_t, 2> black{{0, 0}};
    std::uint8_t terminal = 0;

    friend bool operator==(const Geometry& lhs, const Geometry& rhs) {
        return lhs.side == rhs.side && lhs.white == rhs.white &&
               lhs.black == rhs.black && lhs.terminal == rhs.terminal;
    }
};

[[nodiscard]] auto geometry_squares(const Geometry& geometry) {
    return std::tie(geometry.white[0], geometry.white[1],
                    geometry.black[0], geometry.black[1]);
}

[[nodiscard]] Geometry canonicalize_public(Geometry geometry) {
    Geometry reflected = geometry;
    for (std::uint8_t& square : reflected.white)
        square = reflect_square(square);
    for (std::uint8_t& square : reflected.black)
        square = reflect_square(square);
    std::sort(reflected.white.begin(), reflected.white.end());
    std::sort(reflected.black.begin(), reflected.black.end());
    return geometry_squares(reflected) < geometry_squares(geometry)
         ? reflected : geometry;
}

[[nodiscard]] std::uint64_t pack_geometry(const Geometry& geometry) {
    std::uint64_t key = static_cast<std::uint64_t>(geometry.white[0]);
    key |= static_cast<std::uint64_t>(geometry.white[1]) << 7;
    key |= static_cast<std::uint64_t>(geometry.black[0]) << 14;
    key |= static_cast<std::uint64_t>(geometry.black[1]) << 21;
    key |= static_cast<std::uint64_t>(geometry.side) << 28;
    key |= static_cast<std::uint64_t>(geometry.terminal) << 29;
    return key;
}

[[nodiscard]] Geometry unpack_geometry(std::uint64_t key) {
    Geometry geometry;
    geometry.white[0] = static_cast<std::uint8_t>(key & 0x7f);
    geometry.white[1] = static_cast<std::uint8_t>((key >> 7) & 0x7f);
    geometry.black[0] = static_cast<std::uint8_t>((key >> 14) & 0x7f);
    geometry.black[1] = static_cast<std::uint8_t>((key >> 21) & 0x7f);
    geometry.side = static_cast<Color>((key >> 28) & 1);
    geometry.terminal = static_cast<std::uint8_t>((key >> 29) & 3);
    return geometry;
}

[[nodiscard]] constexpr unsigned pair_index(unsigned first, unsigned second) {
    if (first > second) {
        const unsigned temporary = first;
        first = second;
        second = temporary;
    }
    if (first == second || second >= 4)
        return 6;
    constexpr unsigned indices[4][4] = {
      {6, 0, 1, 2}, {0, 6, 3, 4}, {1, 3, 6, 5}, {2, 4, 5, 6}};
    return indices[first][second];
}

[[nodiscard]] constexpr bool partition_equivalent(
  std::uint8_t partition, unsigned first, unsigned second) {
    return first == second ||
      (pair_index(first, second) < 6 &&
       (partition & (1u << pair_index(first, second))));
}

[[nodiscard]] std::uint8_t restrict_partition(std::uint8_t partition,
                                              std::uint8_t relation) {
    std::uint8_t result = 0;
    for (unsigned first = 0; first < 4; ++first)
      for (unsigned second = first + 1; second < 4; ++second)
        if ((relation & (1u << first)) && (relation & (1u << second)) &&
            partition_equivalent(partition, first, second))
            result |= static_cast<std::uint8_t>(1u << pair_index(first, second));
    return result;
}

[[nodiscard]] bool valid_partition(std::uint8_t partition,
                                   std::uint8_t relation) {
    if (partition != restrict_partition(partition, relation))
        return false;
    for (unsigned first = 0; first < 4; ++first)
      for (unsigned second = 0; second < 4; ++second)
        for (unsigned third = 0; third < 4; ++third)
            if ((relation & (1u << first)) && (relation & (1u << second)) &&
                (relation & (1u << third)) &&
                partition_equivalent(partition, first, second) &&
                partition_equivalent(partition, second, third) &&
                !partition_equivalent(partition, first, third))
                return false;
    return true;
}

[[nodiscard]] constexpr std::uint8_t root_white_partition() {
    return static_cast<std::uint8_t>((1u << 0) | (1u << 5));  // 0~1, 2~3
}

[[nodiscard]] constexpr std::uint8_t root_black_partition() {
    return static_cast<std::uint8_t>((1u << 1) | (1u << 4));  // 0~2, 1~3
}

[[nodiscard]] std::uint64_t node_key(const Geometry& geometry,
                                     std::uint8_t relation,
                                     std::uint8_t whitePartition,
                                     std::uint8_t blackPartition) {
    if (!relation || !valid_partition(whitePartition, relation) ||
        !valid_partition(blackPartition, relation))
        throw std::runtime_error("invalid joint-information partition key");
    return pack_geometry(geometry) |
           (static_cast<std::uint64_t>(relation) << 31) |
           (static_cast<std::uint64_t>(whitePartition) << 35) |
           (static_cast<std::uint64_t>(blackPartition) << 41);
}

[[nodiscard]] Geometry node_geometry(std::uint64_t key) {
    return unpack_geometry(key & ((std::uint64_t{1} << 31) - 1));
}

[[nodiscard]] std::uint8_t node_relation(std::uint64_t key) {
    return static_cast<std::uint8_t>((key >> 31) & 0xf);
}

[[nodiscard]] std::uint8_t node_partition(std::uint64_t key, Color color) {
    return static_cast<std::uint8_t>(
      (key >> (color == Color::White ? 35 : 41)) & 0x3f);
}

[[nodiscard]] unsigned partition_representative(
  std::uint8_t relation, std::uint8_t partition, unsigned bit) {
    if (!(relation & (1u << bit)))
        return 4;
    for (unsigned candidate = 0; candidate < bit; ++candidate)
        if ((relation & (1u << candidate)) &&
            partition_equivalent(partition, candidate, bit))
            return candidate;
    return bit;
}

template<typename Key>
[[nodiscard]] std::uint8_t refine_partition(
  std::uint8_t relation, std::uint8_t partition, const Key& keys) {
    std::uint8_t result = 0;
    for (unsigned first = 0; first < 4; ++first)
      for (unsigned second = first + 1; second < 4; ++second)
        if ((relation & (1u << first)) && (relation & (1u << second)) &&
            partition_equivalent(partition, first, second) &&
            keys[first] == keys[second])
            result |= static_cast<std::uint8_t>(1u << pair_index(first, second));
    if (!valid_partition(result, relation))
        throw std::runtime_error("private observation refinement is not a partition");
    return result;
}

template<typename Key>
[[nodiscard]] std::array<std::uint8_t, 2> refine_mover_partition(
  std::uint8_t relation,
  std::array<std::uint8_t, 2> partitions,
  Color mover,
  const Key& decisionKeys) {
    const std::size_t moverIndex = static_cast<std::size_t>(mover);
    partitions[moverIndex] = refine_partition(
      relation, partitions[moverIndex], decisionKeys);
    return partitions;
}

[[nodiscard]] constexpr unsigned white_class(unsigned bitIndex) {
    return bitIndex / 2;
}

[[nodiscard]] constexpr unsigned black_class(unsigned bitIndex) {
    return bitIndex % 2;
}

[[nodiscard]] Position position_for(std::uint64_t key, unsigned bitIndex) {
    const Geometry geometry = node_geometry(key);
    const unsigned whiteKingSlot = white_class(bitIndex);
    const unsigned blackKingSlot = black_class(bitIndex);
    const FourState state{
      geometry.side,
      geometry.white[whiteKingSlot],
      geometry.black[blackKingSlot],
      geometry.white[1 - whiteKingSlot],
      geometry.black[1 - blackKingSlot]};
    Position position = make_position(state);
    if (terminal_code(position) != geometry.terminal)
        throw std::runtime_error("epistemic frame terminal code is inconsistent");
    return position;
}

struct PhysicalFrame {
    Geometry geometry;
    unsigned bitIndex = 0;
};

[[nodiscard]] PhysicalFrame physical_frame(const Position& position) {
    std::array<std::vector<int>, 2> royalSquares;
    std::array<int, 2> kingSquares{{Position::NoSquare, Position::NoSquare}};
    int alive = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++alive;
        if (piece.type != PieceType::King && piece.type != PieceType::Jester)
            throw std::runtime_error("joint Jester frame contains a non-royal piece");
        royalSquares[static_cast<std::size_t>(piece.color)].push_back(piece.square);
        if (piece.type == PieceType::King)
            kingSquares[static_cast<std::size_t>(piece.color)] = piece.square;
    }
    if (alive != 4 || royalSquares[0].size() != 2 ||
        royalSquares[1].size() != 2 || kingSquares[0] == Position::NoSquare ||
        kingSquares[1] == Position::NoSquare)
        throw std::runtime_error("position is outside K+Jester-v-K+Jester");
    for (auto& squares : royalSquares)
        std::sort(squares.begin(), squares.end());
    Geometry geometry{
      position.side_to_move(),
      {static_cast<std::uint8_t>(royalSquares[0][0]),
       static_cast<std::uint8_t>(royalSquares[0][1])},
      {static_cast<std::uint8_t>(royalSquares[1][0]),
       static_cast<std::uint8_t>(royalSquares[1][1])},
      terminal_code(position)};
    Geometry canonical = canonicalize_public(geometry);
    int whiteKing = kingSquares[0];
    int blackKing = kingSquares[1];
    if (!(canonical == geometry)) {
        whiteKing = reflect_square(static_cast<std::uint8_t>(whiteKing));
        blackKing = reflect_square(static_cast<std::uint8_t>(blackKing));
    }
    const auto whiteFound = std::find(canonical.white.begin(), canonical.white.end(),
                                      whiteKing);
    const auto blackFound = std::find(canonical.black.begin(), canonical.black.end(),
                                      blackKing);
    if (whiteFound == canonical.white.end() || blackFound == canonical.black.end())
        throw std::runtime_error("canonical public frame lost a King square");
    const unsigned whiteSlot = static_cast<unsigned>(whiteFound - canonical.white.begin());
    const unsigned blackSlot = static_cast<unsigned>(blackFound - canonical.black.begin());
    return {canonical, 2 * whiteSlot + blackSlot};
}

struct ActionKey {
    std::uint8_t from = 0;
    std::uint8_t to = 0;
    std::uint8_t auxiliary = 0;
    MoveKind kind = MoveKind::Normal;
    PieceType promotion = PieceType::Count;

    friend bool operator<(const ActionKey& lhs, const ActionKey& rhs) {
        return std::tie(lhs.from, lhs.to, lhs.auxiliary, lhs.kind, lhs.promotion) <
               std::tie(rhs.from, rhs.to, rhs.auxiliary, rhs.kind, rhs.promotion);
    }
    friend bool operator==(const ActionKey& lhs, const ActionKey& rhs) {
        return lhs.from == rhs.from && lhs.to == rhs.to &&
               lhs.auxiliary == rhs.auxiliary && lhs.kind == rhs.kind &&
               lhs.promotion == rhs.promotion;
    }
};

[[nodiscard]] ActionKey action_key(const Move& move) {
    return {move.from, move.to, move.auxiliary, move.kind, move.promotion};
}

void append_word(std::string& output, std::int32_t value) {
    const std::uint32_t word = static_cast<std::uint32_t>(value);
    for (unsigned shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<char>((word >> shift) & 0xff));
}

[[nodiscard]] std::int32_t common_type(const PieceState& piece,
                                       const std::array<bool, 2>& ambiguous) {
    const std::size_t color = static_cast<std::size_t>(piece.color);
    if (ambiguous[color] &&
        (piece.type == PieceType::King || piece.type == PieceType::Jester))
        return static_cast<std::int32_t>(PieceType::Count) + 1;
    return static_cast<std::int32_t>(piece.type);
}

// Local observer-neutral adapter.  information.cpp currently projects one
// observer and therefore reveals that observer's own royal identity.  The
// joint solver needs a common transcript which redacts both teams.
[[nodiscard]] std::string common_view_key(const Position& position) {
    using Record = std::array<std::int32_t, 11>;
    std::array<bool, 2> hasKing{{false, false}};
    std::array<bool, 2> hasJester{{false, false}};
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        if (piece.type == PieceType::King)
            hasKing[static_cast<std::size_t>(piece.color)] = true;
        else if (piece.type == PieceType::Jester)
            hasJester[static_cast<std::size_t>(piece.color)] = true;
    }
    const std::array<bool, 2> ambiguous{{
      hasKing[0] && hasJester[0], hasKing[1] && hasJester[1]}};
    std::vector<Record> records;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive)
            continue;
        records.push_back({
          common_type(piece, ambiguous), static_cast<std::int32_t>(piece.color),
          piece.square, piece.onBoard, piece.action, piece.cooldown,
          piece.freezeCount, piece.power, piece.moved, piece.visible,
          piece.attachmentOrder});
    }
    std::sort(records.begin(), records.end());
    std::string output;
    append_word(output, 2);
    append_word(output, static_cast<std::int32_t>(position.side_to_move()));
    append_word(output, static_cast<std::int32_t>(position.continuation()));
    append_word(output, position.en_passant_square());
    append_word(output, terminal_code(position));
    append_word(output, static_cast<std::int32_t>(records.size()));
    for (const Record& record : records)
        for (const std::int32_t field : record)
            append_word(output, field);
    return output;
}

[[nodiscard]] std::string common_transition_key(
  const Position& before, const Move& move, const Position& after) {
    std::array<bool, 2> hasKing{{false, false}};
    std::array<bool, 2> hasJester{{false, false}};
    for (int id = 0; id < before.piece_count(); ++id) {
        const PieceState& piece = before.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        if (piece.type == PieceType::King)
            hasKing[static_cast<std::size_t>(piece.color)] = true;
        else if (piece.type == PieceType::Jester)
            hasJester[static_cast<std::size_t>(piece.color)] = true;
    }
    const std::array<bool, 2> ambiguous{{
      hasKing[0] && hasJester[0], hasKing[1] && hasJester[1]}};
    const int actor = move.kind == MoveKind::Pass
                    ? Position::NoPiece : before.piece_on(move.from);
    std::string output;
    append_word(output, 2);
    append_word(output, actor == Position::NoPiece ? -1
      : common_type(before.piece(actor), ambiguous));
    append_word(output, move.from);
    append_word(output, move.to);
    append_word(output, move.auxiliary);
    append_word(output, static_cast<std::int32_t>(move.kind));
    append_word(output, static_cast<std::int32_t>(move.promotion));
    output += common_view_key(after);
    return output;
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
            std::fill(block_.begin() + static_cast<std::ptrdiff_t>(blockSize_),
                      block_.end(), 0);
            compress(block_.data());
            blockSize_ = 0;
        }
        std::fill(block_.begin() + static_cast<std::ptrdiff_t>(blockSize_),
                  block_.begin() + 56, 0);
        for (int index = 0; index < 8; ++index)
            block_[63 - index] = static_cast<std::uint8_t>(bitLength >> (8 * index));
        compress(block_.data());
        std::array<std::uint8_t, 32> digest{};
        for (std::size_t index = 0; index < state_.size(); ++index)
            for (int byte = 0; byte < 4; ++byte)
                digest[4 * index + byte] = static_cast<std::uint8_t>(
                  state_[index] >> (24 - 8 * byte));
        return digest;
    }

  private:
    static constexpr std::array<std::uint32_t, 64> K{{
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
      0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
      0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
      0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
      0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
      0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
      0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
      0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
      0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2}};

    [[nodiscard]] static std::uint32_t rotate(std::uint32_t value, int bits) {
        return (value >> bits) | (value << (32 - bits));
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

[[nodiscard]] std::string hex_digest(const std::array<std::uint8_t, 32>& digest) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest)
        output << std::setw(2) << unsigned(byte);
    return output.str();
}

struct PackedWdl {
    std::vector<std::uint8_t> bytes;
    std::size_t planeOffset = 0;
    std::uint32_t count = 0;
    std::array<std::uint8_t, 32> sha{};

    [[nodiscard]] Wdl result(std::uint32_t index) const {
        if (index >= count)
            throw std::runtime_error("packed WDL index is out of range");
        const std::uint8_t packed = bytes.at(planeOffset + index / 4);
        return static_cast<Wdl>((packed >> (2 * (index % 4))) & 3);
    }
};

[[nodiscard]] std::uint32_t read_u32(const std::vector<std::uint8_t>& data,
                                     std::size_t offset) {
    if (offset + 4 > data.size())
        throw std::runtime_error("truncated packed table header");
    std::uint32_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

[[nodiscard]] PackedWdl load_table(const std::string& path,
                                   std::uint32_t expectedCount,
                                   PieceType secondary,
                                   Color secondaryColor) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open concrete table: " + path);
    input.seekg(0, std::ios::end);
    const std::streamoff end = input.tellg();
    if (end < 0)
        throw std::runtime_error("cannot size concrete table");
    PackedWdl table;
    table.bytes.resize(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(table.bytes.data()),
               static_cast<std::streamsize>(table.bytes.size()));
    if (static_cast<std::size_t>(input.gcount()) != table.bytes.size())
        throw std::runtime_error("truncated concrete table");
    if (table.bytes.size() < 40 ||
        std::memcmp(table.bytes.data(), "UFTB1\0\0\0", 8) != 0)
        throw std::runtime_error("invalid concrete table magic");
    const std::uint32_t version = read_u32(table.bytes, 8);
    table.count = read_u32(table.bytes, 16);
    const std::uint32_t wdlBytes = read_u32(table.bytes, 28);
    if (version < 4 || version > 6 ||
        read_u32(table.bytes, 12) != static_cast<std::uint32_t>(PieceType::Jester) ||
        table.count != expectedCount || read_u32(table.bytes, 24) != 1 ||
        wdlBytes != (table.count + 3) / 4)
        throw std::runtime_error("concrete table has the wrong Jester codec");
    if (version >= 5 &&
        (read_u32(table.bytes, 40) != static_cast<std::uint32_t>(secondary) ||
         read_u32(table.bytes, 44) != static_cast<std::uint32_t>(secondaryColor)))
        throw std::runtime_error("concrete table has the wrong secondary material");
    table.planeOffset = 40 + (version >= 5 ? 8 : 0) + (version >= 6 ? 8 : 0);
    if (table.planeOffset + wdlBytes > table.bytes.size())
        throw std::runtime_error("truncated concrete WDL plane");
    Sha256 sha;
    sha.update(table.bytes.data(), table.bytes.size());
    table.sha = sha.finish();
    return table;
}

[[nodiscard]] std::uint32_t encode_lower(Color side, std::uint8_t ownerKing,
                                         std::uint8_t enemyKing,
                                         std::uint8_t jester) {
    if (ownerKing == enemyKing || ownerKing == jester || enemyKing == jester)
        throw std::runtime_error("invalid lower Jester placement");
    const std::uint32_t enemyRank = enemyKing - (enemyKing > ownerKing ? 1u : 0u);
    const std::uint8_t low = std::min(ownerKing, enemyKing);
    const std::uint8_t high = std::max(ownerKing, enemyKing);
    const std::uint32_t jesterRank = jester - (jester > low ? 1u : 0u) -
                                    (jester > high ? 1u : 0u);
    return ((static_cast<std::uint32_t>(side) * Squares + ownerKing) *
             (Squares - 1) + enemyRank) * (Squares - 2) + jesterRank;
}

class LowerOracle {
  public:
    LowerOracle(PackedWdl concrete, const std::string& overlayPath,
                const std::string& expectedModel)
      : concrete_(std::move(concrete)) {
        std::ifstream input(overlayPath, std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open lower Jester UFIW2 overlay");
        std::array<char, 160> header{};
        input.read(header.data(), static_cast<std::streamsize>(header.size()));
        if (static_cast<std::size_t>(input.gcount()) != header.size() ||
            std::memcmp(header.data(), "UFIW2\0\0\0", 8) != 0)
            throw std::runtime_error("invalid lower Jester UFIW2 overlay");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        if (word(8) != 2 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Jester) ||
            word(16) != static_cast<std::uint32_t>(PieceType::Count) ||
            word(20) != static_cast<std::uint32_t>(Color::White) ||
            word(24) != LowerCount || word(28) != 1)
            throw std::runtime_error("lower overlay has the wrong material domain");
        const std::string source(header.data() + 32, 64);
        const std::string model(header.data() + 96, 64);
        if (source != hex_digest(concrete_.sha) || model != expectedModel)
            throw std::runtime_error("lower overlay SHA/model binding is stale");
        flags_.resize(LowerCount);
        input.read(reinterpret_cast<char*>(flags_.data()),
                   static_cast<std::streamsize>(flags_.size()));
        if (static_cast<std::size_t>(input.gcount()) != flags_.size())
            throw std::runtime_error("truncated lower overlay flags");
    }

    struct Index {
        std::uint32_t value = 0;
        Color owner = Color::White;
    };

    [[nodiscard]] Index index(const Position& position) const {
        std::array<int, 2> king{{Position::NoPiece, Position::NoPiece}};
        int jester = Position::NoPiece;
        int alive = 0;
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive || !piece.onBoard)
                continue;
            ++alive;
            if (piece.type == PieceType::King)
                king[static_cast<std::size_t>(piece.color)] = id;
            else if (piece.type == PieceType::Jester)
                jester = id;
            else
                throw std::runtime_error("lower child contains non-Jester material");
        }
        if (alive != 3 || jester == Position::NoPiece ||
            king[0] == Position::NoPiece || king[1] == Position::NoPiece)
            throw std::runtime_error("lower child is not K+Jester-v-K");
        const Color owner = position.piece(jester).color;
        const Color mappedSide = owner == Color::White
                               ? position.side_to_move() : ~position.side_to_move();
        return {encode_lower(
                  mappedSide,
                  position.piece(king[static_cast<std::size_t>(owner)]).square,
                  position.piece(king[static_cast<std::size_t>(~owner)]).square,
                  position.piece(jester).square), owner};
    }

    [[nodiscard]] bool exact_forces(const Position& position, Color target) const {
        const Index mapped = index(position);
        const Wdl result = concrete_.result(mapped.value);
        return (result == Wdl::Win && target == position.side_to_move()) ||
               (result == Wdl::Loss && target != position.side_to_move());
    }

    [[nodiscard]] bool pair_forces(const Position& position, Color target) const {
        const Index mapped = index(position);
        const std::uint8_t flags = flags_.at(mapped.value);
        if (!(flags & 4))
            throw std::runtime_error("lower pair is outside overlay admission domain");
        return (flags & (target == mapped.owner ? 1 : 2)) != 0;
    }

  private:
    PackedWdl concrete_;
    std::vector<std::uint8_t> flags_;
};

struct Successor {
    std::uint32_t node = NoNode;
    std::uint8_t bitIndex = 0;
    std::array<bool, 2> exact{{false, false}};
};

struct Candidate {
    std::uint8_t parentBit = 0;
    std::uint8_t actorBlock = 0;
    ActionKey action;
    Position child;
    std::array<std::string, 2> privateObservation;
    Successor successor;
};

struct UniformAction {
    std::uint8_t actorBlock = 0;
    ActionKey action;
    std::vector<std::size_t> candidates;
};

struct GeneratedNode {
    Geometry geometry;
    std::uint8_t relation = 0;
    std::array<std::uint8_t, 2> effectivePartitions{{0, 0}};
    std::vector<UniformAction> actions;
    std::vector<Candidate> candidates;
};

class JointArena {
  public:
    explicit JointArena(const LowerOracle& lower) : lower_(lower) {
        interner_.reserve(12'000'000);
        nodes_.reserve(12'000'000);
    }

    [[nodiscard]] std::uint32_t intern(std::uint64_t key) {
        const auto found = interner_.find(key);
        if (found != interner_.end())
            return found->second;
        if (nodes_.size() >= NoNode)
            throw std::runtime_error("joint information graph exceeds 32-bit node IDs");
        const std::uint32_t id = static_cast<std::uint32_t>(nodes_.size());
        nodes_.push_back(key);
        interner_.emplace(key, id);
        return id;
    }

    [[nodiscard]] std::uint32_t find(std::uint64_t key) const {
        const auto found = interner_.find(key);
        if (found == interner_.end())
            throw std::runtime_error("regenerated successor is absent from graph");
        return found->second;
    }

    void build_roots() {
        const auto started = std::chrono::steady_clock::now();
        std::uint64_t geometries = 0;
        std::uint64_t admitted = 0;
        for (int whiteFirst = 0; whiteFirst < static_cast<int>(Squares); ++whiteFirst)
          for (int whiteSecond = whiteFirst + 1;
               whiteSecond < static_cast<int>(Squares); ++whiteSecond)
            for (int blackFirst = 0; blackFirst < static_cast<int>(Squares); ++blackFirst) {
              if (blackFirst == whiteFirst || blackFirst == whiteSecond)
                continue;
              for (int blackSecond = blackFirst + 1;
                   blackSecond < static_cast<int>(Squares); ++blackSecond) {
                if (blackSecond == whiteFirst || blackSecond == whiteSecond)
                    continue;
                Geometry base{Color::White,
                              {static_cast<std::uint8_t>(whiteFirst),
                               static_cast<std::uint8_t>(whiteSecond)},
                              {static_cast<std::uint8_t>(blackFirst),
                               static_cast<std::uint8_t>(blackSecond)}, 0};
                if (!(canonicalize_public(base) == base))
                    continue;
                ++geometries;
                for (const Color side : {Color::White, Color::Black}) {
                    std::array<std::uint8_t, 4> masks{{0, 0, 0, 0}};
                    for (unsigned bit = 0; bit < 4; ++bit) {
                        FourState state{
                          side, base.white[white_class(bit)],
                          base.black[black_class(bit)],
                          base.white[1 - white_class(bit)],
                          base.black[1 - black_class(bit)]};
                        Position position = make_position(state);
                        if (position.has_forced_action() ||
                            !position.ordinary_predecessor_king_safe())
                            continue;
                        masks[terminal_code(position)] |=
                          static_cast<std::uint8_t>(1u << bit);
                        ++admitted;
                    }
                    for (std::uint8_t terminal = 0; terminal < masks.size(); ++terminal)
                        if (masks[terminal]) {
                            Geometry geometry = base;
                            geometry.side = side;
                            geometry.terminal = terminal;
                            (void)intern(node_key(
                              geometry, masks[terminal],
                              restrict_partition(root_white_partition(),
                                                 masks[terminal]),
                              restrict_partition(root_black_partition(),
                                                 masks[terminal])));
                        }
                    // With one live Jester per side, ordinary check is
                    // suspended and neither side is terminal before a
                    // capture.  Thus every collision-free public frame has
                    // the complete four-assignment relation.  This invariant
                    // is the closed-frontier proof used below; it is checked
                    // exhaustively here rather than assumed by the solver.
                    if (masks[0] != 0xf || masks[1] || masks[2] || masks[3])
                        throw std::runtime_error(
                          "fresh four-world root partition is not maximal");
                }
                if (geometries % 250'000 == 0) {
                    const double elapsed = std::chrono::duration<double>(
                      std::chrono::steady_clock::now() - started).count();
                    std::cout << "joint_roots geometries " << geometries
                              << " nodes " << nodes_.size()
                              << " admitted " << admitted
                              << " elapsed " << elapsed << "s\n" << std::flush;
                }
              }
            }
        rootNodeCount_ = nodes_.size();
        admittedRoots_ = admitted;
        std::cout << "joint_roots complete geometries " << geometries
                  << " nodes " << rootNodeCount_
                  << " admitted " << admittedRoots_ << '\n' << std::flush;
    }

    void expand_all() {
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t cursor = 0; cursor < nodes_.size(); ++cursor) {
            (void)generate(static_cast<std::uint32_t>(cursor), true);
            if ((cursor + 1) % 100'000 == 0) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - started).count();
                std::cout << "joint_graph expanded " << cursor + 1 << '/'
                          << nodes_.size() << " observations "
                          << observationBuckets_ << " elapsed " << elapsed
                          << "s\n" << std::flush;
            }
        }
        std::cout << "joint_graph complete nodes " << nodes_.size()
                  << " root_nodes " << rootNodeCount_
                  << " observations " << observationBuckets_
                  << " transition_residual 0 observation_residual 0\n"
                  << std::flush;
    }

    [[nodiscard]] GeneratedNode regenerate(std::uint32_t node) const {
        return const_cast<JointArena*>(this)->generate(node, false);
    }

    [[nodiscard]] const std::vector<std::uint64_t>& nodes() const { return nodes_; }
    [[nodiscard]] std::uint64_t admitted_roots() const { return admittedRoots_; }

  private:
    [[nodiscard]] static bool in_same_class(const Position& position) {
        int whiteKings = 0, blackKings = 0, whiteJesters = 0, blackJesters = 0;
        int alive = 0;
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive || !piece.onBoard)
                continue;
            ++alive;
            whiteKings += piece.type == PieceType::King && piece.color == Color::White;
            blackKings += piece.type == PieceType::King && piece.color == Color::Black;
            whiteJesters += piece.type == PieceType::Jester && piece.color == Color::White;
            blackJesters += piece.type == PieceType::Jester && piece.color == Color::Black;
        }
        return alive == 4 && whiteKings == 1 && blackKings == 1 &&
               whiteJesters == 1 && blackJesters == 1;
    }

    [[nodiscard]] static std::map<ActionKey, Move> legal_actions(
      const Position& position) {
        std::map<ActionKey, Move> result;
        for (const Move& move : position.legal_moves())
            if (!result.emplace(action_key(move), move).second)
                throw std::runtime_error("full Move identity is not unique");
        return result;
    }

    [[nodiscard]] static bool canonical_lower_pair(
      const std::set<std::uint32_t>& unique) {
        if (unique.size() != 2)
            return false;
        auto iterator = unique.begin();
        const std::uint32_t first = *iterator++;
        const std::uint32_t second = *iterator;
        const auto swap_index = [](std::uint32_t index) {
            std::uint32_t jesterRank = index % (Squares - 2);
            index /= Squares - 2;
            const std::uint32_t enemyRank = index % (Squares - 1);
            index /= Squares - 1;
            const std::uint8_t ownerKing =
              static_cast<std::uint8_t>(index % Squares);
            const Color side = static_cast<Color>(index / Squares);
            const std::uint8_t enemyKing = static_cast<std::uint8_t>(
              enemyRank + (enemyRank >= ownerKing));
            const std::uint8_t low = std::min(ownerKing, enemyKing);
            const std::uint8_t high = std::max(ownerKing, enemyKing);
            std::uint8_t jester = static_cast<std::uint8_t>(jesterRank);
            if (jester >= low) ++jester;
            if (jester >= high) ++jester;
            return encode_lower(side, jester, enemyKing, ownerKing);
        };
        return swap_index(first) == second && swap_index(second) == first;
    }

    void classify_external(const std::vector<std::size_t>& members,
                           GeneratedNode& node) const {
        bool terminal = true;
        std::optional<Color> winner;
        for (const std::size_t index : members) {
            const Position& child = node.candidates[index].child;
            terminal = terminal && child.game_over();
            if (!winner)
                winner = child.winner();
            else if (winner != child.winner())
                throw std::runtime_error("one public terminal observation has two winners");
        }
        if (terminal) {
            for (const std::size_t index : members)
                for (const Color target : {Color::White, Color::Black})
                    node.candidates[index].successor.exact[
                      static_cast<std::size_t>(target)] =
                        winner && *winner == target;
            return;
        }
        if (std::any_of(members.begin(), members.end(), [&](std::size_t index) {
                return node.candidates[index].child.game_over();
            }))
            throw std::runtime_error("one common observation mixes terminal and live worlds");

        for (const std::size_t index : members)
          for (const Color target : {Color::White, Color::Black}) {
            const std::size_t side = static_cast<std::size_t>(target);
            const Candidate& actual = node.candidates[index];
            std::set<std::uint32_t> unique;
            for (const std::size_t otherIndex : members) {
                const Candidate& other = node.candidates[otherIndex];
                if (partition_equivalent(node.effectivePartitions[side],
                                         actual.parentBit, other.parentBit) &&
                    actual.privateObservation[side] ==
                      other.privateObservation[side])
                    unique.insert(lower_.index(other.child).value);
            }
            if (unique.empty() || unique.size() > 2)
                throw std::runtime_error(
                  "private lower royal belief is neither singleton nor pair");
            const bool pair = unique.size() == 2;
            if (pair && !canonical_lower_pair(unique))
                throw std::runtime_error(
                  "private lower observation is not a canonical royal pair");
            node.candidates[index].successor.exact[side] = pair
              ? lower_.pair_forces(actual.child, target)
              : lower_.exact_forces(actual.child, target);
          }
    }

    [[nodiscard]] GeneratedNode generate(std::uint32_t nodeId, bool allowNew) {
        const std::uint64_t key = nodes_.at(nodeId);
        GeneratedNode generated;
        generated.geometry = node_geometry(key);
        generated.relation = node_relation(key);
        generated.effectivePartitions = {{
          node_partition(key, Color::White),
          node_partition(key, Color::Black)}};
        if (generated.geometry.terminal)
            return generated;

        std::array<std::optional<Position>, 4> positions;
        std::array<std::map<ActionKey, Move>, 4> actions;
        for (unsigned bit = 0; bit < 4; ++bit)
            if (generated.relation & (1u << bit)) {
                positions[bit] = position_for(key, bit);
                actions[bit] = legal_actions(*positions[bit]);
            }

        const Color mover = generated.geometry.side;
        const std::size_t moverIndex = static_cast<std::size_t>(mover);
        std::array<std::string, 4> decisionKeys;
        for (unsigned bit = 0; bit < 4; ++bit)
            if (generated.relation & (1u << bit))
                decisionKeys[bit] = decision_observation_key(
                  *positions[bit], DisclosureContext{mover, false});
        generated.effectivePartitions = refine_mover_partition(
          generated.relation, generated.effectivePartitions, mover,
          decisionKeys);

        for (unsigned actorBlock = 0; actorBlock < 4; ++actorBlock) {
            std::vector<unsigned> members;
            for (unsigned bit = 0; bit < 4; ++bit)
                if ((generated.relation & (1u << bit)) &&
                    partition_representative(
                      generated.relation,
                      generated.effectivePartitions[moverIndex], bit) == actorBlock)
                    members.push_back(bit);
            if (members.empty())
                continue;
            std::vector<ActionKey> common;
            for (const auto& entry : actions[members.front()]) {
                const ActionKey& action = entry.first;
                if (std::all_of(members.begin() + 1, members.end(),
                    [&](unsigned bit) { return actions[bit].count(action) != 0; }))
                    common.push_back(action);
            }
            for (const ActionKey& action : common) {
                UniformAction uniform;
                uniform.actorBlock = static_cast<std::uint8_t>(actorBlock);
                uniform.action = action;
                for (const unsigned bit : members) {
                    Position child = *positions[bit];
                    Undo undo;
                    if (!child.make_move(actions[bit].at(action), undo))
                        throw std::runtime_error("uniform legal move failed make_move");
                    Candidate candidate;
                    candidate.parentBit = static_cast<std::uint8_t>(bit);
                    candidate.actorBlock = static_cast<std::uint8_t>(actorBlock);
                    candidate.action = action;
                    candidate.child = std::move(child);
                    for (const Color observer : {Color::White, Color::Black})
                        candidate.privateObservation[
                          static_cast<std::size_t>(observer)] =
                            transition_observation_key(
                              *positions[bit], actions[bit].at(action),
                              candidate.child,
                              DisclosureContext{observer, false});
                    uniform.candidates.push_back(generated.candidates.size());
                    generated.candidates.push_back(std::move(candidate));
                }
                generated.actions.push_back(std::move(uniform));
            }
        }

        std::map<std::string, std::vector<std::size_t>> byObservation;
        for (std::size_t index = 0; index < generated.candidates.size(); ++index) {
            const Candidate& candidate = generated.candidates[index];
            const Position& before = *positions[candidate.parentBit];
            const Move& move = actions[candidate.parentBit].at(candidate.action);
            byObservation[common_transition_key(before, move, candidate.child)]
              .push_back(index);
        }
        observationBuckets_ += allowNew ? byObservation.size() : 0;

        for (const auto& [observation, members] : byObservation) {
            (void)observation;
            const bool sameClass = in_same_class(
              generated.candidates[members.front()].child);
            if (std::any_of(members.begin(), members.end(), [&](std::size_t index) {
                    return in_same_class(generated.candidates[index].child) != sameClass;
                }))
                throw std::runtime_error("one observation mixes material classes");
            if (!sameClass) {
                classify_external(members, generated);
                continue;
            }
            std::optional<Geometry> childGeometry;
            std::uint8_t childRelation = 0;
            std::vector<unsigned> childBits;
            childBits.reserve(members.size());
            for (const std::size_t index : members) {
                const PhysicalFrame child = physical_frame(
                  generated.candidates[index].child);
                if (childGeometry && !(*childGeometry == child.geometry))
                    throw std::runtime_error("one observation has two public geometries");
                childGeometry = child.geometry;
                childRelation |= static_cast<std::uint8_t>(1u << child.bitIndex);
                childBits.push_back(child.bitIndex);
            }
            if (!childGeometry || !childRelation)
                throw std::runtime_error("empty same-class observation bucket");
            std::array<std::uint8_t, 2> childPartitions{{0, 0}};
            for (const Color observer : {Color::White, Color::Black}) {
                const std::size_t side = static_cast<std::size_t>(observer);
                for (std::size_t first = 0; first < members.size(); ++first)
                  for (std::size_t second = first + 1;
                       second < members.size(); ++second) {
                    const Candidate& firstCandidate =
                      generated.candidates[members[first]];
                    const Candidate& secondCandidate =
                      generated.candidates[members[second]];
                    const unsigned firstBit = childBits[first];
                    const unsigned secondBit = childBits[second];
                    const bool remainsEquivalent = partition_equivalent(
                        generated.effectivePartitions[side],
                        firstCandidate.parentBit, secondCandidate.parentBit) &&
                      firstCandidate.privateObservation[side] ==
                        secondCandidate.privateObservation[side];
                    if (firstBit == secondBit) {
                        if (!remainsEquivalent)
                            throw std::runtime_error(
                              "physical-frame collision loses private history");
                        continue;
                    }
                    if (remainsEquivalent)
                        childPartitions[side] |= static_cast<std::uint8_t>(
                          1u << pair_index(firstBit, secondBit));
                  }
                childPartitions[side] = restrict_partition(
                  childPartitions[side], childRelation);
                if (!valid_partition(childPartitions[side], childRelation))
                    throw std::runtime_error(
                      "successor private observation is not a partition");
            }
            const std::uint64_t childKey = node_key(
              *childGeometry, childRelation,
              childPartitions[static_cast<std::size_t>(Color::White)],
              childPartitions[static_cast<std::size_t>(Color::Black)]);
            const std::uint32_t childNode = allowNew ? intern(childKey) : find(childKey);
            for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal) {
                Successor& successor =
                  generated.candidates[members[ordinal]].successor;
                successor.node = childNode;
                successor.bitIndex = static_cast<std::uint8_t>(childBits[ordinal]);
            }
        }
        return generated;
    }

    const LowerOracle& lower_;
    std::vector<std::uint64_t> nodes_;
    std::unordered_map<std::uint64_t, std::uint32_t> interner_;
    std::uint64_t rootNodeCount_ = 0;
    std::uint64_t admittedRoots_ = 0;
    std::uint64_t observationBuckets_ = 0;
};

struct TargetSolution {
    Color target = Color::White;
    std::vector<std::uint8_t> values;
    InformationSolveSummary fixedPoint;
    std::uint64_t variables = 0;
    std::uint64_t actionGates = 0;
};

[[nodiscard]] std::array<TargetSolution, 2> solve_targets(
    const JointArena& arena, const std::string& scratch) {
    const auto& nodes = arena.nodes();
    const std::uint64_t baseVariables = 4 * nodes.size();
    std::array<std::vector<std::uint64_t>, 2> gateBase;
    for (auto& bases : gateBase)
        bases.resize(nodes.size() + 1, 0);
    for (std::size_t node = 0; node < nodes.size(); ++node) {
        const Geometry geometry = node_geometry(nodes[node]);
        std::uint64_t gates = 0;
        if (!geometry.terminal)
            gates = arena.regenerate(static_cast<std::uint32_t>(node)).actions.size();
        for (const Color target : {Color::White, Color::Black}) {
            const std::size_t side = static_cast<std::size_t>(target);
            gateBase[side][node + 1] = gateBase[side][node] +
              (geometry.side == target ? gates : 0);
        }
    }

    std::array<std::uint64_t, 2> variableCounts{};
    for (const Color target : {Color::White, Color::Black}) {
        const std::size_t side = static_cast<std::size_t>(target);
        variableCounts[side] = baseVariables + gateBase[side].back();
        if (!variableCounts[side] || variableCounts[side] >= InformationTrue)
            throw std::runtime_error(
              "joint fixed-point variable count exceeds 32-bit tokens");
        std::cout << "joint_equation_estimate target " << side
                  << " private_variables " << baseVariables
                  << " action_gates " << gateBase[side].back()
                  << " variables " << variableCounts[side] << '\n';
    }
    std::cout << std::flush;

    std::array<std::unique_ptr<InformationFixedPoint>, 2> solvers;
    const auto available_bytes = [&]() {
        struct statvfs status {};
        if (::statvfs(scratch.c_str(), &status) != 0)
            throw std::runtime_error("cannot inspect information scratch filesystem");
        return static_cast<std::uint64_t>(status.f_bavail) * status.f_frsize;
    };
    const std::uint64_t scratchAvailableStart = available_bytes();
    std::uint64_t scratchAvailableMinimum = scratchAvailableStart;
    const auto sample_scratch = [&]() {
        scratchAvailableMinimum = std::min(scratchAvailableMinimum,
                                           available_bytes());
    };
    for (std::size_t side = 0; side < 2; ++side)
        solvers[side] = std::make_unique<InformationFixedPoint>(
          static_cast<std::uint32_t>(variableCounts[side]), scratch);
    sample_scratch();

    const auto class_variable = [](std::uint32_t node, unsigned bitIndex) {
        return static_cast<InformationToken>(4ULL * node + bitIndex);
    };
    std::array<std::vector<InformationToken>, 2> tokenBuffers;
    for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
        const std::uint32_t node = static_cast<std::uint32_t>(nodeIndex);
        const std::uint64_t key = nodes[nodeIndex];
        const Geometry geometry = node_geometry(key);
        const std::uint8_t relation = node_relation(key);
        const GeneratedNode generated = geometry.terminal
                                      ? GeneratedNode{} : arena.regenerate(node);

        for (const Color target : {Color::White, Color::Black}) {
            const std::size_t side = static_cast<std::size_t>(target);
            InformationFixedPoint& solver = *solvers[side];
            std::vector<InformationToken>& tokens = tokenBuffers[side];
            const auto gate_variable = [&](std::size_t action) {
                return static_cast<InformationToken>(
                  baseVariables + gateBase[side][nodeIndex] + action);
            };
            const auto child_token = [&](const Successor& successor) {
                if (successor.node == NoNode)
                    return successor.exact[side]
                         ? InformationTrue : InformationFalse;
                return class_variable(successor.node, successor.bitIndex);
            };

            if (geometry.terminal) {
                for (unsigned bit = 0; bit < 4; ++bit) {
                    const std::uint8_t winner = target == Color::White ? 2 : 3;
                    const InformationToken value =
                      (relation & (1u << bit)) && geometry.terminal == winner
                        ? InformationTrue : InformationFalse;
                    solver.define_or(class_variable(node, bit), &value, 1);
                }
                continue;
            }

            if (geometry.side == target) {
                if (generated.actions.size() !=
                    gateBase[side][nodeIndex + 1] - gateBase[side][nodeIndex])
                    throw std::runtime_error(
                      "private legal-dot action count has a residual");
                std::array<std::vector<InformationToken>, 4> choices;
                for (std::size_t actionIndex = 0;
                     actionIndex < generated.actions.size(); ++actionIndex) {
                    const UniformAction& action = generated.actions[actionIndex];
                    tokens.clear();
                    for (const std::size_t candidate : action.candidates)
                        tokens.push_back(child_token(
                          generated.candidates[candidate].successor));
                    const InformationToken gate = gate_variable(actionIndex);
                    solver.define_and(gate, tokens);
                    choices[action.actorBlock].push_back(gate);
                }
                for (unsigned bit = 0; bit < 4; ++bit) {
                    if (relation & (1u << bit)) {
                        const unsigned block = partition_representative(
                          relation, generated.effectivePartitions[side], bit);
                        solver.define_or(class_variable(node, bit), choices[block]);
                    }
                    else {
                        const InformationToken value = InformationFalse;
                        solver.define_or(class_variable(node, bit), &value, 1);
                    }
                }
            }
            else {
                for (unsigned bit = 0; bit < 4; ++bit) {
                    tokens.clear();
                    if (relation & (1u << bit)) {
                        for (const UniformAction& action : generated.actions)
                          for (const std::size_t candidate : action.candidates) {
                            const Candidate& edge =
                              generated.candidates[candidate];
                            if (partition_equivalent(
                                  generated.effectivePartitions[side], bit,
                                  edge.parentBit))
                                tokens.push_back(child_token(edge.successor));
                          }
                        // Empty AND is the exact timeout win for the target
                        // when the opponent has no uniform action in its own
                        // private legal-dot-refined class.
                        solver.define_and(class_variable(node, bit), tokens);
                    }
                    else {
                        const InformationToken value = InformationFalse;
                        solver.define_or(class_variable(node, bit), &value, 1);
                    }
                }
            }
        }
        if ((nodeIndex + 1) % 100'000 == 0) {
            sample_scratch();
            std::cout << "joint_equations both_targets defined "
                      << nodeIndex + 1 << '/' << nodes.size()
                      << '\n' << std::flush;
        }
    }

    std::array<TargetSolution, 2> results;
    for (const Color target : {Color::White, Color::Black}) {
        const std::size_t side = static_cast<std::size_t>(target);
        const InformationSolveSummary summary = solvers[side]->solve();
        sample_scratch();
        if (summary.bellmanResidual || summary.rankResidual)
            throw std::runtime_error("joint information fixed point has a residual");
        TargetSolution& result = results[side];
        result.target = target;
        result.values.resize(baseVariables);
        for (std::size_t variable = 0; variable < baseVariables; ++variable)
            result.values[variable] = solvers[side]->value(
              static_cast<InformationToken>(variable));
        result.fixedPoint = summary;
        result.variables = variableCounts[side];
        result.actionGates = gateBase[side].back();
        std::cout << "joint_fixed_point target " << side
                  << " variables " << summary.variables
                  << " reverse_edges " << summary.reverseEdges
                  << " activated " << summary.activated
                  << " bellman_residual " << summary.bellmanResidual
                  << " rank_residual " << summary.rankResidual << '\n'
                  << std::flush;
        // Release this target's reverse CSR and solution scratch before
        // constructing the other target's, while retaining the other's
        // already-written forward equation stream.
        solvers[side].reset();
        sample_scratch();
    }
    std::cout << "joint_resource_certificate scratch_peak_bytes "
              << scratchAvailableStart - scratchAvailableMinimum
              << " scratch_available_start " << scratchAvailableStart
              << " scratch_available_minimum " << scratchAvailableMinimum
              << '\n' << std::flush;
    return results;
}

[[nodiscard]] bool target_value(const TargetSolution& solution,
                                std::uint32_t node, unsigned bitIndex) {
    return solution.values.at(4ULL * node + bitIndex) != 0;
}

struct RootLookup {
    std::uint32_t node = NoNode;
    unsigned bitIndex = 0;
    bool admitted = false;
};

[[nodiscard]] RootLookup root_lookup(const JointArena& arena,
                                     const FourState& concrete) {
    Position actual = make_position(concrete);
    if (actual.has_forced_action() || !actual.ordinary_predecessor_king_safe())
        return {};
    const PhysicalFrame physical = physical_frame(actual);
    std::uint8_t relation = 0;
    for (unsigned bit = 0; bit < 4; ++bit) {
        const FourState alternative{
          physical.geometry.side,
          physical.geometry.white[white_class(bit)],
          physical.geometry.black[black_class(bit)],
          physical.geometry.white[1 - white_class(bit)],
          physical.geometry.black[1 - black_class(bit)]};
        Position position = make_position(alternative);
        if (!position.has_forced_action() && position.ordinary_predecessor_king_safe() &&
            terminal_code(position) == physical.geometry.terminal)
            relation |= static_cast<std::uint8_t>(1u << bit);
    }
    if (!(relation & (1u << physical.bitIndex)))
        throw std::runtime_error("admitted actual world absent from maximal root relation");
    return {arena.find(node_key(
              physical.geometry, relation,
              restrict_partition(root_white_partition(), relation),
              restrict_partition(root_black_partition(), relation))),
            physical.bitIndex, true};
}

void self_test_private_legal_dots() {
    const auto witness = [](Color mover, bool jesterOnE6,
                            bool moverOwnsJester) {
        Position position;
        position.clear();
        const Color enemy = ~mover;
        std::vector<int> pieces;
        pieces.push_back(position.add_piece(
          PieceType::King, mover, Position::square_from_name("e7")));
        if (moverOwnsJester)
            pieces.push_back(position.add_piece(
              PieceType::Jester, mover, Position::square_from_name("h10")));
        pieces.push_back(position.add_piece(
          jesterOnE6 ? PieceType::Jester : PieceType::King,
          enemy, Position::square_from_name("e6")));
        pieces.push_back(position.add_piece(
          jesterOnE6 ? PieceType::King : PieceType::Jester,
          enemy, Position::square_from_name("e5")));
        if (std::any_of(pieces.begin(), pieces.end(), [](int id) {
                return id == Position::NoPiece;
            }))
            throw std::runtime_error("legal-dot witness construction failed");
        for (const int id : pieces)
            position.piece(id).moved = true;
        position.set_side_to_move(mover);
        return position;
    };

    for (const Color mover : {Color::White, Color::Black}) {
        const DisclosureContext disclosure{mover, false};
        const Position kingE6 = witness(mover, false, false);
        const Position jesterE6 = witness(mover, true, false);
        if (view_key(kingE6, disclosure) != view_key(jesterE6, disclosure) ||
            !kingE6.move_from_string("e7-e6") ||
            jesterE6.move_from_string("e7-e6") ||
            decision_observation_key(kingE6, disclosure) ==
              decision_observation_key(jesterE6, disclosure))
            throw std::runtime_error(
              "e7/e6/e5 mover-private legal-dot split residual");

        const unsigned secondHypothesis = mover == Color::White ? 1 : 2;
        const std::uint8_t hypothesisEdge = static_cast<std::uint8_t>(
          1u << pair_index(0, secondHypothesis));
        const std::array<std::uint8_t, 2> initialPartitions{{
          root_white_partition(), root_black_partition()}};
        std::array<std::string, 4> differing{};
        differing[0] = decision_observation_key(kingE6, disclosure);
        differing[secondHypothesis] =
          decision_observation_key(jesterE6, disclosure);
        const std::array<std::uint8_t, 2> splitPartitions =
          refine_mover_partition(0xf, initialPartitions, mover, differing);
        if ((splitPartitions[static_cast<std::size_t>(mover)] & hypothesisEdge) ||
            splitPartitions[static_cast<std::size_t>(~mover)] !=
              initialPartitions[static_cast<std::size_t>(~mover)])
            throw std::runtime_error(
              "differing legal dots did not split only the mover's block");

        // In the actual K+Jester-v-K+Jester stratum, the mover's own Jester
        // suspends ordinary King-safety filtering. Both e7-e6 dots are then
        // present, so the same private block must remain grouped.
        const Position protectedKingE6 = witness(mover, false, true);
        const Position protectedJesterE6 = witness(mover, true, true);
        const std::string first =
          decision_observation_key(protectedKingE6, disclosure);
        const std::string second =
          decision_observation_key(protectedJesterE6, disclosure);
        if (first != second ||
            !protectedKingE6.move_from_string("e7-e6") ||
            !protectedJesterE6.move_from_string("e7-e6"))
            throw std::runtime_error("equal legal-dot witness unexpectedly split");
        std::array<std::string, 4> equal{};
        equal[0] = first;
        equal[secondHypothesis] = second;
        const std::array<std::uint8_t, 2> groupedPartitions =
          refine_mover_partition(0xf, initialPartitions, mover, equal);
        if (!(groupedPartitions[static_cast<std::size_t>(mover)] &
              hypothesisEdge) ||
            groupedPartitions[static_cast<std::size_t>(~mover)] !=
              initialPartitions[static_cast<std::size_t>(~mover)])
            throw std::runtime_error(
              "equal legal dots did not preserve only the mover's block");

        bool rejectedNonMover = false;
        try {
            (void)decision_observation_key(
              kingE6, DisclosureContext{~mover, false});
        }
        catch (const std::invalid_argument&) {
            rejectedNonMover = true;
        }
        if (!rejectedNonMover)
            throw std::runtime_error(
              "mover-private legal dots leaked to the non-mover");
    }
}

void self_test_codec(const std::string& scratch) {
    self_test_private_legal_dots();
    for (std::uint32_t index = 0; index < ConcreteCount; ++index) {
        const FourState state = decode_four(index);
        if (state.whiteKing % 8 >= 4 || encode_four(state) != index)
            throw std::runtime_error("exhaustive four-model codec residual");
    }
    Geometry geometry{Color::White, {0, 17}, {62, 79}, 0};
    Geometry mirror = geometry;
    for (std::uint8_t& square : mirror.white) square = reflect_square(square);
    for (std::uint8_t& square : mirror.black) square = reflect_square(square);
    std::sort(mirror.white.begin(), mirror.white.end());
    std::sort(mirror.black.begin(), mirror.black.end());
    if (!(canonicalize_public(geometry) == canonicalize_public(mirror)))
        throw std::runtime_error("shared public reflection residual");

    // Every valid equivalence relation on every non-empty actual-world subset
    // must round-trip independently for both players.  Invalid bit patterns
    // must never enter the node interner: otherwise two different private
    // histories could alias in the exact graph.
    std::uint64_t partitionCodecStates = 0;
    for (unsigned relation = 1; relation < 16; ++relation)
      for (unsigned whitePartition = 0; whitePartition < 64; ++whitePartition)
        if (valid_partition(static_cast<std::uint8_t>(whitePartition),
                            static_cast<std::uint8_t>(relation)))
          for (unsigned blackPartition = 0; blackPartition < 64;
               ++blackPartition)
            if (valid_partition(static_cast<std::uint8_t>(blackPartition),
                                static_cast<std::uint8_t>(relation))) {
                const std::uint64_t key = node_key(
                  geometry, static_cast<std::uint8_t>(relation),
                  static_cast<std::uint8_t>(whitePartition),
                  static_cast<std::uint8_t>(blackPartition));
                if (!(node_geometry(key) == geometry) ||
                    node_relation(key) != relation ||
                    node_partition(key, Color::White) != whitePartition ||
                    node_partition(key, Color::Black) != blackPartition)
                    throw std::runtime_error(
                      "joint relation/private-partition codec residual");
                ++partitionCodecStates;
            }
    if (!partitionCodecStates)
        throw std::runtime_error("partition codec test admitted no states");

    const Geometry asymmetric{Color::White, {0, 17}, {62, 79}, 0};
    for (unsigned bit = 0; bit < 4; ++bit) {
        const FourState state{
          asymmetric.side,
          asymmetric.white[white_class(bit)],
          asymmetric.black[black_class(bit)],
          asymmetric.white[1 - white_class(bit)],
          asymmetric.black[1 - black_class(bit)]};
        const PhysicalFrame frame = physical_frame(make_position(state));
        if (!(frame.geometry == asymmetric) || frame.bitIndex != bit)
            throw std::runtime_error("public-frame assignment partition residual");
    }
    const Geometry symmetric{Color::White, {0, 7}, {72, 79}, 0};
    for (unsigned bit = 0; bit < 4; ++bit) {
        const FourState state{
          symmetric.side,
          symmetric.white[white_class(bit)],
          symmetric.black[black_class(bit)],
          symmetric.white[1 - white_class(bit)],
          symmetric.black[1 - black_class(bit)]};
        const FourState reflected{
          state.side, reflect_square(state.whiteKing),
          reflect_square(state.blackKing), reflect_square(state.whiteJester),
          reflect_square(state.blackJester)};
        const PhysicalFrame firstFrame = physical_frame(make_position(state));
        const PhysicalFrame reflectedFrame = physical_frame(make_position(reflected));
        if (!(firstFrame.geometry == symmetric) ||
            !(reflectedFrame.geometry == symmetric) ||
            firstFrame.bitIndex != bit || reflectedFrame.bitIndex != (bit ^ 3u))
            throw std::runtime_error("symmetric public-frame collision residual");
    }

    ActionKey first{1, 2, 3, MoveKind::Normal, PieceType::Count};
    ActionKey second = first;
    second.auxiliary = 4;
    if (first == second || !(first < second))
        throw std::runtime_error("full Move action identity residual");

    // Exhaust every non-empty 2x2 relation, both observers, and both private
    // classes against a direct one-ply strategy oracle.  Each actor chooses a
    // column/row guess uniformly over its information class; a gate succeeds
    // iff that guess is correct in every retained actual world.
    constexpr unsigned RelationCases = 15 * 2 * 2;
    InformationFixedPoint solver(3 * RelationCases, scratch);
    std::array<bool, RelationCases> expected{};
    unsigned testCase = 0;
    for (unsigned perspective = 0; perspective < 2; ++perspective)
      for (unsigned relation = 1; relation < 16; ++relation)
        for (unsigned ownClass = 0; ownClass < 2; ++ownClass, ++testCase) {
            const InformationToken root = 3 * testCase;
            std::array<InformationToken, 2> gates{{root + 1, root + 2}};
            bool classExists = false;
            bool canGuess = false;
            for (unsigned guess = 0; guess < 2; ++guess) {
                std::vector<InformationToken> outcomes;
                for (unsigned bit = 0; bit < 4; ++bit) {
                    if (!(relation & (1u << bit)))
                        continue;
                    const unsigned own = perspective == 0
                                       ? white_class(bit) : black_class(bit);
                    if (own != ownClass)
                        continue;
                    classExists = true;
                    const unsigned other = perspective == 0
                                         ? black_class(bit) : white_class(bit);
                    outcomes.push_back(other == guess
                      ? InformationTrue : InformationFalse);
                }
                if (outcomes.empty()) {
                    const InformationToken value = InformationFalse;
                    solver.define_or(gates[guess], &value, 1);
                }
                else {
                    solver.define_and(gates[guess], outcomes);
                    canGuess = canGuess || std::all_of(
                      outcomes.begin(), outcomes.end(), [](InformationToken token) {
                          return token == InformationTrue;
                      });
                }
            }
            if (classExists)
                solver.define_or(root, gates.data(), gates.size());
            else {
                const InformationToken value = InformationFalse;
                solver.define_or(root, &value, 1);
            }
            expected[testCase] = classExists && canGuess;
        }
    const InformationSolveSummary summary = solver.solve();
    for (unsigned item = 0; item < RelationCases; ++item)
        if ((solver.value(3 * item) != 0) != expected[item])
            throw std::runtime_error("exhaustive small-arena oracle residual");
    if (summary.bellmanResidual || summary.rankResidual)
        throw std::runtime_error("small-arena/brute-force fixed-point residual");
    std::cout << "joint_self_test codec_states " << ConcreteCount
              << " codec_residual 0 small_arena_residual 0"
              << " partition_residual 0 legal_dot_split_residual 0"
              << " legal_dot_group_residual 0 nonmover_dot_leak_residual 0"
              << " partition_codec_states " << partitionCodecStates
              << " semantics " << SemanticsId
              << "\n";
}

[[nodiscard]] bool valid_sha(const std::string& value) {
    return value.size() == 64 &&
      std::all_of(value.begin(), value.end(), [](char character) {
          return (character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'f');
      });
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

struct Arguments {
    std::string input;
    std::string lowerTable;
    std::string lowerOverlay;
    std::string sourceSha;
    std::string modelSha;
    std::string lowerModelSha;
    std::string semanticsId;
    std::string output;
    std::string scratch = "/tmp";
    bool selfTestOnly = false;
};

[[nodiscard]] Arguments parse_arguments(int argc, char** argv) {
    Arguments result;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&]() {
            if (++index >= argc)
                throw std::runtime_error(argument + " requires a value");
            return std::string(argv[index]);
        };
        if (argument == "--input") result.input = value();
        else if (argument == "--lower-table") result.lowerTable = value();
        else if (argument == "--lower-overlay") result.lowerOverlay = value();
        else if (argument == "--source-sha256") result.sourceSha = value();
        else if (argument == "--model-sha256") result.modelSha = value();
        else if (argument == "--lower-model-sha256")
            result.lowerModelSha = value();
        else if (argument == "--semantics-id") result.semanticsId = value();
        else if (argument == "--output") result.output = value();
        else if (argument == "--scratch") result.scratch = value();
        else if (argument == "--self-test-only") result.selfTestOnly = true;
        else throw std::runtime_error("unknown argument: " + argument);
    }
    if (!result.selfTestOnly &&
        (result.input.empty() || result.lowerTable.empty() ||
         result.lowerOverlay.empty() || result.output.empty() ||
         !valid_sha(result.sourceSha) || !valid_sha(result.modelSha) ||
         !valid_sha(result.lowerModelSha) ||
         result.semanticsId != SemanticsId))
        throw std::runtime_error(
          "full solve requires v2 semantics, input/lower table/lower overlay/output, "
          "and SHA bindings");
    return result;
}

int run(int argc, char** argv) {
    const Arguments arguments = parse_arguments(argc, argv);
    self_test_codec(arguments.scratch);
    if (arguments.selfTestOnly)
        return 0;

    PackedWdl concrete = load_table(arguments.input, ConcreteCount,
                                    PieceType::Jester, Color::Black);
    if (hex_digest(concrete.sha) != arguments.sourceSha)
        throw std::runtime_error("source SHA-256 does not match concrete input");
    PackedWdl lowerConcrete = load_table(arguments.lowerTable, LowerCount,
                                         PieceType::Count, Color::White);
    LowerOracle lower(std::move(lowerConcrete), arguments.lowerOverlay,
                      arguments.lowerModelSha);
    JointArena arena(lower);
    arena.build_roots();
    arena.expand_all();
    const std::array<TargetSolution, 2> solutions =
      solve_targets(arena, arguments.scratch);
    const TargetSolution& white = solutions[static_cast<std::size_t>(Color::White)];
    const TargetSolution& black = solutions[static_cast<std::size_t>(Color::Black)];

    using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
    Counts totals{};
    Counts unreachable{};
    std::vector<std::uint8_t> rootSeen(arena.nodes().size(), 0);
    std::array<std::uint64_t, 2> rootSets{{0, 0}};
    std::vector<std::uint8_t> flags(ConcreteCount, 0);
    std::uint64_t overlap = 0;
    for (std::uint32_t index = 0; index < ConcreteCount; ++index) {
        const FourState state = decode_four(index);
        const std::size_t side = static_cast<std::size_t>(state.side);
        const Wdl exact = concrete.result(index);
        const RootLookup root = root_lookup(arena, state);
        if (!root.admitted) {
            ++unreachable[side][static_cast<std::size_t>(exact)];
            continue;
        }
        const std::uint8_t sideBit = static_cast<std::uint8_t>(1u << side);
        if (!(rootSeen[root.node] & sideBit)) {
            rootSeen[root.node] |= sideBit;
            ++rootSets[side];
        }
        const bool whiteForces = target_value(white, root.node, root.bitIndex);
        const bool blackForces = target_value(black, root.node, root.bitIndex);
        overlap += whiteForces && blackForces;
        flags[index] = 4 | (whiteForces ? 1 : 0) | (blackForces ? 2 : 0);
        const bool moverWins = state.side == Color::White ? whiteForces : blackForces;
        const bool moverLoses = state.side == Color::White ? blackForces : whiteForces;
        const Wdl result = moverWins ? Wdl::Win
                         : moverLoses ? Wdl::Loss : Wdl::Draw;
        ++totals[side][static_cast<std::size_t>(result)];
    }
    if (overlap)
        throw std::runtime_error("dual sure-win overlap is nonzero");
    for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t conserved = 0;
        for (std::size_t result = 1; result < 4; ++result)
            conserved += totals[side][result] + unreachable[side][result];
        if (conserved != ConcreteCount / 2)
            throw std::runtime_error("joint root conservation residual");
    }
    std::cout << "information_fixed_point white_variables " << white.variables
              << " white_edges " << white.fixedPoint.reverseEdges
              << " white_activated " << white.fixedPoint.activated
              << " black_variables " << black.variables
              << " black_edges " << black.fixedPoint.reverseEdges
              << " black_activated " << black.fixedPoint.activated
              << " bellman_residual 0 rank_residual 0\n";
    for (std::size_t side = 0; side < 2; ++side)
        std::cout << "information_summary side " << side
                  << " win " << totals[side][1]
                  << " loss " << totals[side][2]
                  << " draw " << totals[side][3]
                  << " unreachable_win " << unreachable[side][1]
                  << " unreachable_loss " << unreachable[side][2]
                  << " unreachable_draw " << unreachable[side][3]
                  << " sets " << rootSets[side]
                  << " concrete " << ConcreteCount / 2
                  << " bellman_residual 0 rank_residual 0"
                  << " belief_cap none exhaustive 1\n";

    std::ofstream output(arguments.output, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("cannot create joint information overlay");
    output.write("UFIW2\0\0\0", 8);
    write_u32(output, 2);
    write_u32(output, static_cast<std::uint32_t>(PieceType::Jester));
    write_u32(output, static_cast<std::uint32_t>(PieceType::Jester));
    write_u32(output, static_cast<std::uint32_t>(Color::Black));
    write_u32(output, ConcreteCount);
    write_u32(output, 1);
    output.write(arguments.sourceSha.data(),
                 static_cast<std::streamsize>(arguments.sourceSha.size()));
    output.write(arguments.modelSha.data(),
                 static_cast<std::streamsize>(arguments.modelSha.size()));
    output.write(reinterpret_cast<const char*>(flags.data()),
                 static_cast<std::streamsize>(flags.size()));
    if (!output)
        throw std::runtime_error("failed writing joint information overlay");
    std::cout << "information_overlay " << arguments.output
              << " bytes " << flags.size() + 160
              << " source_sha256 " << arguments.sourceSha
              << " model_sha256 " << arguments.modelSha
              << " semantics " << SemanticsId
              << " conservation_residual 0 dual_overlap_residual 0\n";
    struct rusage usage {};
    if (::getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("cannot read joint solver peak RSS");
#if defined(__APPLE__)
    const std::uint64_t peakRssBytes =
      static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    const std::uint64_t peakRssBytes =
      static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
    std::cout << "joint_resource_certificate peak_rss_bytes " << peakRssBytes
              << '\n';
    return 0;
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    try {
        return Stockfish::Ultimate::run(argc, argv);
    }
    catch (const std::exception& error) {
        std::cerr << "joint Jester information error: " << error.what() << '\n';
        return 1;
    }
}
