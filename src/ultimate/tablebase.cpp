/*
  Ultimate Fish exact retrograde tablebase generator
  GPLv3 or later
*/

#include "information.h"
#include "information_solver.h"
#include "position.h"
#include "tablebase_probe.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <future>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unistd.h>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t SquareCount = Position::BoardSquares;
constexpr std::uint32_t PlacementStateCount =
  2 * SquareCount * (SquareCount - 1) * (SquareCount - 2);

enum class Wdl : std::uint8_t { Unknown, Win, Loss, Draw };

Wdl parent_wdl(Wdl child, bool sameSide) {
    if (sameSide || child == Wdl::Draw || child == Wdl::Unknown)
        return child;
    return child == Wdl::Win ? Wdl::Loss : Wdl::Win;
}

Wdl parent_wdl(TablebaseWdl child, bool sameSide) {
    return parent_wdl(static_cast<Wdl>(child), sameSide);
}

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
constexpr std::uint32_t CompoundCopycatStateCount =
  2 * SquareCount * (SquareCount - 1) * (SquareCount - 2) * (SquareCount - 3);

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

std::uint32_t encode_compound_copycat(FourState state) {
    const std::uint32_t blackRank = rank_excluding(state.blackKing, {state.whiteKing});
    const std::uint32_t firstRank = rank_excluding(
      state.first, {state.whiteKing, state.blackKing});
    const std::uint32_t secondRank = rank_excluding(
      state.second, {state.whiteKing, state.blackKing, state.first});
    return ((((static_cast<std::uint32_t>(state.side) * SquareCount + state.whiteKing)
               * (SquareCount - 1) + blackRank)
              * (SquareCount - 2) + firstRank)
             * (SquareCount - 3) + secondRank);
}

FourState decode_compound_copycat(std::uint32_t index) {
    const std::uint32_t secondRank = index % (SquareCount - 3);
    index /= SquareCount - 3;
    const std::uint32_t firstRank = index % (SquareCount - 2);
    index /= SquareCount - 2;
    const std::uint32_t blackRank = index % (SquareCount - 1);
    index /= SquareCount - 1;
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(index % SquareCount);
    const Color side = static_cast<Color>(index / SquareCount);
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
    for (std::uint32_t index = 0; index < CompoundCopycatStateCount; ++index) {
        const FourState state = decode_compound_copycat(index);
        if (state.whiteKing == state.blackKing || state.whiteKing == state.first ||
            state.whiteKing == state.second || state.blackKing == state.first ||
            state.blackKing == state.second || state.first == state.second ||
            encode_compound_copycat(state) != index)
            throw std::runtime_error("compound Copycat codec is not bijective");
    }
    std::cout << "fourcodecok states " << FourPlacementStateCount << '\n';
    std::cout << "identicalfourcodecok states " << IdenticalFourStateCount << '\n';
    std::cout << "compoundcopycatcodecok states " << CompoundCopycatStateCount << '\n';
}

struct Node {
    Wdl wdl = Wdl::Unknown;
    std::uint16_t dtw = 0;
    std::uint16_t remaining = 0;
    std::uint16_t longestWinChild = 0;
};

template<typename T>
class MappedArray {
   public:
    MappedArray(const std::string& path, std::uint64_t count) : count_(count) {
        if (count_ > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::runtime_error("mapped tablebase array is too large");
        bytes_ = static_cast<std::size_t>(count_) * sizeof(T);
        fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (fd_ == -1)
            throw std::runtime_error("cannot create mapped tablebase scratch file");
        if (::ftruncate(fd_, static_cast<off_t>(bytes_)) != 0) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("cannot size mapped tablebase scratch file");
        }
        void* mapping = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd_, 0);
        if (mapping == MAP_FAILED) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("cannot map tablebase scratch file");
        }
        data_ = static_cast<T*>(mapping);
        // The live mapping keeps the inode and disk allocation alive. Removing
        // the directory entry here guarantees cleanup if generation is killed.
        ::unlink(path.c_str());
    }

    MappedArray(const MappedArray&) = delete;
    MappedArray& operator=(const MappedArray&) = delete;

    ~MappedArray() {
        if (data_)
            ::munmap(data_, bytes_);
        if (fd_ != -1)
            ::close(fd_);
    }

    T& operator[](std::uint64_t index) { return data_[index]; }
    const T& operator[](std::uint64_t index) const { return data_[index]; }
    T* data() { return data_; }

   private:
    int fd_ = -1;
    std::uint64_t count_ = 0;
    std::size_t bytes_ = 0;
    T* data_ = nullptr;
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

class JesterInformationOverlay {
   public:
    explicit JesterInformationOverlay(const std::string& path,
                                      const std::string& expectedSourceSha256,
                                      const std::string& expectedModelSha256) {
        if (path.empty())
            return;
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open lower Jester information overlay");
        std::array<char, 160> header{};
        input.read(header.data(), header.size());
        if (static_cast<std::size_t>(input.gcount()) != header.size() ||
            std::memcmp(header.data(), "UFIW2\0\0\0", 8) != 0)
            throw std::runtime_error("invalid lower Jester information overlay");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        if (word(8) != 2 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Jester) ||
            word(16) != static_cast<std::uint32_t>(PieceType::Count) ||
            word(20) != static_cast<std::uint32_t>(Color::White) ||
            word(24) != PlacementStateCount || word(28) != 1)
            throw std::runtime_error("lower Jester overlay has the wrong material class");
        const std::string sourceSha256(header.data() + 32, 64);
        const std::string modelSha256(header.data() + 96, 64);
        if (expectedSourceSha256.size() != 64 || sourceSha256 != expectedSourceSha256)
            throw std::runtime_error(
              "lower Jester overlay does not match its concrete table SHA-256");
        if (expectedModelSha256.size() != 64 || modelSha256 != expectedModelSha256)
            throw std::runtime_error(
              "lower Jester overlay does not match the information model SHA-256");
        flags_.resize(PlacementStateCount);
        input.read(reinterpret_cast<char*>(flags_.data()), flags_.size());
        if (static_cast<std::size_t>(input.gcount()) != flags_.size())
            throw std::runtime_error("truncated lower Jester information overlay");
    }

    [[nodiscard]] bool loaded() const { return !flags_.empty(); }

    [[nodiscard]] std::optional<bool> forces(const Position& position,
                                             Color target) const {
        int jester = Position::NoPiece;
        std::array<int, 2> kings{{Position::NoPiece, Position::NoPiece}};
        int alive = 0;
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive || !piece.onBoard)
                continue;
            ++alive;
            if (piece.type == PieceType::Jester) {
                if (jester != Position::NoPiece)
                    return std::nullopt;
                jester = id;
            }
            else if (piece.type == PieceType::King)
                kings[static_cast<std::size_t>(piece.color)] = id;
            else
                return std::nullopt;
        }
        if (alive != 3 || jester == Position::NoPiece ||
            kings[0] == Position::NoPiece || kings[1] == Position::NoPiece)
            return std::nullopt;
        if (!loaded())
            throw std::runtime_error(
              "a K+Jester-v-K successor requires --lower-information-overlay");

        const Color owner = position.piece(jester).color;
        const Color mappedSide = owner == Color::White
                               ? position.side_to_move() : ~position.side_to_move();
        const int ownerKing = kings[static_cast<std::size_t>(owner)];
        const int enemyKing = kings[static_cast<std::size_t>(~owner)];
        const std::uint32_t index = encode_placement({
          mappedSide,
          position.piece(ownerKing).square,
          position.piece(enemyKing).square,
          position.piece(jester).square,
          0});
        const std::uint8_t flags = flags_.at(index);
        if (!(flags & 4))
            throw std::runtime_error(
              "lower Jester successor is outside the admitted overlay domain");
        const bool targetIsOwner = target == owner;
        return (flags & (targetIsOwner ? 1 : 2)) != 0;
    }

   private:
    std::vector<std::uint8_t> flags_;
};

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
    case PieceType::Penguin: return 2;  // inactive / exact geometry-derived freeze aura
    default: return 1;
    }
}

}  // namespace

class TablebaseGenerator {
   public:
    TablebaseGenerator(PieceType attackerType, PieceType secondaryType,
                       Color secondaryColor, std::string output,
                       std::string checkpoint, std::uint32_t checkpointEvery,
                       bool diskBacked) :
        attackerType_(attackerType), output_(std::move(output)),
        checkpoint_(std::move(checkpoint)), checkpointEvery_(checkpointEvery),
        diskBacked_(diskBacked),
        copycatOnly_(attackerType == PieceType::Copycat &&
                     secondaryType == PieceType::Count),
        compoundCopycat_(attackerType == PieceType::Copycat &&
                         secondaryType != PieceType::Count),
        secondaryType_(copycatOnly_ ? PieceType::CopycatClone : secondaryType),
        secondaryColor_(secondaryColor),
        fourModels_(secondaryType_ != PieceType::Count),
        identicalExtras_(secondaryType == attackerType && secondaryColor == Color::White),
        primarySubstates_(substate_count(attackerType)),
        secondarySubstates_(fourModels_ ? substate_count(secondaryType_) : 1),
        substates_(primarySubstates_ * secondarySubstates_),
        stateCount_(copycatOnly_ ? PlacementStateCount
                    : compoundCopycat_ ? CompoundCopycatStateCount * substates_
                    : identicalExtras_ ? IdenticalFourStateCount * substates_
                    : fourModels_ ? FourPlacementStateCount * substates_
                               : PlacementStateCount * substates_) {}

    void allocate_state_planes() {
        if (diskBacked_) {
            mappedNodes_ = std::make_unique<MappedArray<Node>>(
              checkpoint_ + ".nodes", stateCount_);
            mappedPredecessorCounts_ = std::make_unique<MappedArray<std::uint32_t>>(
              checkpoint_ + ".degrees", stateCount_);
            nodes_ = mappedNodes_->data();
            predecessorCounts_ = mappedPredecessorCounts_->data();
        }
        else {
            nodeStorage_.resize(stateCount_);
            predecessorCountStorage_.resize(stateCount_);
            nodes_ = nodeStorage_.data();
            predecessorCounts_ = predecessorCountStorage_.data();
        }
    }

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
        if (compoundCopycat_) {
            constexpr std::uint32_t samples = 20'000;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const std::uint32_t index = static_cast<std::uint32_t>(
                  std::uint64_t(stateCount_) * sample / samples);
                Position position;
                if (make_position_at(index, position) && child_index(position) != index)
                    throw std::runtime_error("compound Copycat codec is not bijective");
            }
            std::cout << "compoundcopycatsubstatecodecok samples " << samples << '\n';
            return;
        }
        if (fourModels_ && !copycatOnly_) {
            self_test_four_codec();
            constexpr std::uint32_t samples = 20'000;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const std::uint32_t index = static_cast<std::uint32_t>(
                  std::uint64_t(stateCount_) * sample / samples);
                Position position;
                if (make_position_at(index, position) && child_index(position) != index)
                    throw std::runtime_error("four-model substate codec is not bijective");
            }
            if (attackerType_ == PieceType::Jester && !identicalExtras_) {
                for (std::uint32_t sample = 0; sample < samples; ++sample) {
                    const std::uint32_t index = static_cast<std::uint32_t>(
                      std::uint64_t(stateCount_) * sample / samples);
                    const std::uint32_t alternative =
                      primary_jester_alternative(index);
                    Position concrete, concreteAlternative;
                    if (!make_position_at(index, concrete) ||
                        !make_position_at(alternative, concreteAlternative))
                        continue;
                    Position physical, physicalAlternative;
                    if (primary_jester_alternative(alternative) != index ||
                        !make_primary_jester_world(index, false, physical) ||
                        !make_primary_jester_world(
                          index, true, physicalAlternative) ||
                        child_index(physicalAlternative) != alternative)
                        throw std::runtime_error(
                          "Jester royal-swap codec is not an involution");
                    if (physical.side_to_move() == Color::Black) {
                        const DisclosureContext onyx{Color::Black, false};
                        const bool compactEqual =
                          primary_jester_view_key(physical) ==
                            primary_jester_view_key(physicalAlternative) &&
                          primary_jester_decision_markers(physical) ==
                            primary_jester_decision_markers(physicalAlternative);
                        const bool generalEqual =
                          decision_observation_key(physical, onyx) ==
                          decision_observation_key(physicalAlternative, onyx);
                        if (compactEqual != generalEqual)
                            throw std::runtime_error(
                              "compact Jester legal-dot partition diverges from "
                              "the public-information model");
                    }
                }
                std::cout << "jesterroyalswapcodecok samples " << samples << '\n';
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

    void dry_run(std::uint32_t begin, std::uint32_t count) const {
        begin = std::min(begin, stateCount_);
        const std::uint32_t end = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(stateCount_, std::uint64_t(begin) + count));
        std::uint64_t edges = 0;
        for (std::uint32_t index = begin; index < end; ++index) {
            Position position;
            if (!make_position_at(index, position))
                continue;
            for (const Move& move : position.legal_moves()) {
                Position child = position;
                if (!child.apply_move_unchecked(move))
                    throw std::runtime_error("legal tablebase move failed trusted application");
                ++edges;
                if (in_class(child) && child_index(child) >= stateCount_)
                    throw std::runtime_error("dry-run child index exceeds tablebase domain");
            }
        }
        std::cout << "dryrun states " << begin << ".." << end << " edges " << edges << '\n';
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

    void audit_reachability(const std::string& input, bool full) const {
        std::ifstream stream(input, std::ios::binary);
        if (!stream)
            throw std::runtime_error("cannot open packed tablebase for reachability audit");
        std::array<std::uint8_t, 56> header{};
        stream.read(reinterpret_cast<char*>(header.data()), header.size());
        if (stream.gcount() < 40 || std::memcmp(header.data(), "UFTB1\0\0\0", 8) != 0)
            throw std::runtime_error("invalid packed tablebase header");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const std::uint32_t version = word(8);
        const std::uint32_t primary = word(12);
        const std::uint32_t count = word(16);
        const std::uint32_t fileSubstates = word(24);
        const std::uint32_t wdlBytes = word(28);
        if (version < 4 || version > 6 || primary != static_cast<std::uint32_t>(attackerType_) ||
            count != stateCount_ || fileSubstates != substates_ || wdlBytes != (count + 3) / 4)
            throw std::runtime_error("packed tablebase does not match requested material class");
        if (version >= 5 &&
            (word(40) != static_cast<std::uint32_t>(secondaryType_) ||
             word(44) != static_cast<std::uint32_t>(secondaryColor_)))
            throw std::runtime_error("packed tablebase secondary material does not match");
        const std::size_t planeOffset = 40 + (version >= 5 ? 8 : 0) + (version >= 6 ? 8 : 0);
        stream.seekg(static_cast<std::streamoff>(planeOffset));
        std::vector<std::uint8_t> wdl(wdlBytes);
        stream.read(reinterpret_cast<char*>(wdl.data()), wdl.size());
        if (static_cast<std::size_t>(stream.gcount()) != wdl.size())
            throw std::runtime_error("truncated packed WDL plane");

        using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
        using Examples = std::array<std::array<std::uint32_t, 4>, 2>;
        constexpr std::uint32_t Block = 10'000;
        const std::uint32_t workers = std::min<std::uint32_t>(
          4, std::max(1u, std::thread::hardware_concurrency()));
        std::atomic<std::uint32_t> next{0};
        std::vector<Counts> local(workers);
        std::vector<Examples> localExamples(workers);
        for (Examples& examples : localExamples)
            for (auto& side : examples)
                side.fill(std::numeric_limits<std::uint32_t>::max());
        std::vector<std::thread> tasks;
        for (std::uint32_t worker = 0; worker < workers; ++worker)
            tasks.emplace_back([&, worker] {
                while (true) {
                    const std::uint32_t begin = next.fetch_add(Block, std::memory_order_relaxed);
                    if (begin >= stateCount_)
                        break;
                    const std::uint32_t end = std::min(stateCount_, begin + Block);
                    for (std::uint32_t index = begin; index < end; ++index) {
                        const Color encodedSide = compoundCopycat_
                          ? decode_compound_copycat(index / substates_).side
                          : fourModels_ ? (identicalExtras_
                               ? decode_identical_four(index / substates_).side
                               : decode_four(index / substates_).side)
                          : decode(index).side;
                        Position position;
                        bool unreachable = !make_position_at(index, position);
                        if (!unreachable && !position.has_forced_action())
                            unreachable = !position.ordinary_predecessor_king_safe();
                        if (!unreachable && full) {
                            if (position.has_forced_action()) {
                                const int forced = position.forcedPiece_;
                                unreachable = forced == Position::NoPiece ||
                                  position.pieces_[forced].color != position.sideToMove_ ||
                                  position.legal_moves().empty();
                            }
                            for (int id = 0; !unreachable && id < position.pieceCount_; ++id) {
                                const PieceState& piece = position.pieces_[id];
                                if (!piece.alive || !piece.onBoard)
                                    continue;
                                if (piece.type == PieceType::Sniper) {
                                    // Shoot assigns 3, then the same turn boundary
                                    // immediately decrements it to 2. Subsequent
                                    // boundaries alternate owner/opponent at 1/2.
                                    unreachable = piece.cooldown >= 3 ||
                                      (piece.cooldown == 2 && position.sideToMove_ == piece.color) ||
                                      (piece.cooldown == 1 && position.sideToMove_ != piece.color);
                                }
                                else if (piece.type == PieceType::Pawn ||
                                         piece.type == PieceType::Checker) {
                                    const int promotionRank = piece.color == Color::White
                                                              ? Position::BoardRanks - 1 : 0;
                                    unreachable = piece.square / Position::BoardFiles == promotionRank;
                                }
                            }
                            if (!unreachable && !fourModels_ &&
                                attackerType_ == PieceType::Prince &&
                                position.has_forced_action()) {
                                const int prince = 2;
                                const int destination = position.pieces_[prince].square;
                                bool hasPredecessor = false;
                                for (int deltaFile = -1; !hasPredecessor && deltaFile <= 1;
                                     ++deltaFile)
                                    for (int deltaRank = -1; !hasPredecessor && deltaRank <= 1;
                                         ++deltaRank) {
                                        if (!deltaFile && !deltaRank)
                                            continue;
                                        const int file = int(destination % Position::BoardFiles) -
                                                         deltaFile;
                                        const int rank = int(destination / Position::BoardFiles) -
                                                         deltaRank;
                                        if (file < 0 || file >= Position::BoardFiles ||
                                            rank < 0 || rank >= Position::BoardRanks)
                                            continue;
                                        const int origin = rank * Position::BoardFiles + file;
                                        if (position.board_[origin] != Position::NoPiece)
                                            continue;
                                        Position predecessor = position;
                                        predecessor.continuation_ = Continuation::None;
                                        predecessor.forcedPiece_ = Position::NoPiece;
                                        predecessor.erase_from_board(prince);
                                        predecessor.pieces_[prince].square =
                                          static_cast<std::uint8_t>(origin);
                                        predecessor.place_on_board(prince);
                                        predecessor.sideToMove_ = predecessor.pieces_[prince].color;
                                        for (const Move& move : predecessor.legal_moves()) {
                                            if (move.from != origin || move.to != destination)
                                                continue;
                                            Position child = predecessor;
                                            if (child.apply_move_unchecked(move) &&
                                                in_class(child) && child_index(child) == index) {
                                                hasPredecessor = true;
                                                break;
                                            }
                                        }
                                    }
                                unreachable = !hasPredecessor;
                            }
                            // An active aura freezing a lone enemy King cannot
                            // survive until the Penguin owner's next turn: the
                            // enemy had no action with which to return the turn.
                            for (int id = 0; !unreachable && id < position.pieceCount_; ++id) {
                                const PieceState& penguin = position.pieces_[id];
                                if (!penguin.alive || !penguin.onBoard ||
                                    penguin.type != PieceType::Penguin || !penguin.action ||
                                    position.sideToMove_ != penguin.color)
                                    continue;
                                const Color enemy = ~penguin.color;
                                bool enemyHasPiece = false;
                                bool enemyCanAct = false;
                                for (int target = 0; target < position.pieceCount_; ++target) {
                                    const PieceState& piece = position.pieces_[target];
                                    if (!piece.alive || !piece.onBoard || piece.color != enemy)
                                        continue;
                                    enemyHasPiece = true;
                                    enemyCanAct = enemyCanAct ||
                                                  (!piece.freezeCount && !piece.cooldown);
                                }
                                unreachable = enemyHasPiece && !enemyCanAct;
                            }
                        }
                        const std::uint32_t result =
                          (wdl[index / 4] >> (2 * (index % 4))) & 3;
                        const std::size_t side = static_cast<std::size_t>(encodedSide);
                        if (!unreachable) {
                            localExamples[worker][side][result] = std::min(
                              localExamples[worker][side][result], index);
                            continue;
                        }
                        ++local[worker][side][result];
                    }
                }
            });
        for (std::thread& task : tasks)
            task.join();
        Counts totals{};
        Examples examples{};
        for (auto& side : examples)
            side.fill(std::numeric_limits<std::uint32_t>::max());
        for (const Counts& part : local)
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result)
                    totals[side][result] += part[side][result];
        for (const Examples& part : localExamples)
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result)
                    examples[side][result] = std::min(examples[side][result],
                                                      part[side][result]);
        for (std::size_t side = 0; side < 2; ++side)
            std::cout << (full ? "reachability" : "predecessor_safety")
                      << " side " << side
                      << " unknown " << totals[side][0]
                      << " win " << totals[side][1]
                      << " loss " << totals[side][2]
                      << " draw " << totals[side][3] << '\n';
        if (full)
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 1; result < 4; ++result)
                    if (examples[side][result] != std::numeric_limits<std::uint32_t>::max()) {
                        Position position;
                        if (make_position_at(examples[side][result], position))
                            std::cout << "legal_example side " << side << " result " << result
                                      << " index " << examples[side][result] << ' '
                                      << position.upn() << '\n';
                    }
    }

    // Exact, uncapped proof kernel for the first epistemic tablebase stratum.
    // K+Jester-v-K has two concrete royal assignments for every public pair
    // of Ivory silhouettes.  Onyx must use one action that is legal in both
    // retained worlds; Ivory knows its own King and may choose a different
    // action in each world.  Observations, rather than strategy inference,
    // are the only way the pair can collapse to a singleton.
    //
    // The implementation deliberately starts with this closed three-model
    // class.  Larger Jester classes use the same monotone gates but require
    // cross-class information probes after captures; Ghost classes additionally
    // need disk-backed arbitrary world-set interning.
    void solve_jester_information_reference(const std::string& input,
                                            const std::string& overlayOutput) const {
        if (attackerType_ != PieceType::Jester || fourModels_ || substates_ != 1)
            throw std::runtime_error(
              "--solve-jester-information currently requires K+Jester-v-K");

        std::ifstream stream(input, std::ios::binary);
        if (!stream)
            throw std::runtime_error("cannot open concrete Jester tablebase");
        std::array<std::uint8_t, 56> header{};
        stream.read(reinterpret_cast<char*>(header.data()), header.size());
        if (stream.gcount() < 40 || std::memcmp(header.data(), "UFTB1\0\0\0", 8) != 0)
            throw std::runtime_error("invalid concrete Jester tablebase header");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const std::uint32_t version = word(8);
        const std::uint32_t count = word(16);
        const std::uint32_t fileSubstates = word(24);
        const std::uint32_t wdlBytes = word(28);
        if (version < 4 || version > 6 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Jester) ||
            count != stateCount_ || fileSubstates != substates_ ||
            wdlBytes != (count + 3) / 4 ||
            (version >= 5 &&
             (word(40) != static_cast<std::uint32_t>(secondaryType_) ||
              word(44) != static_cast<std::uint32_t>(secondaryColor_))))
            throw std::runtime_error("concrete Jester tablebase does not match codec");
        const std::size_t planeOffset = 40 + (version >= 5 ? 8 : 0) +
                                        (version >= 6 ? 8 : 0);
        stream.seekg(static_cast<std::streamoff>(planeOffset));
        std::vector<std::uint8_t> concreteWdl(wdlBytes);
        stream.read(reinterpret_cast<char*>(concreteWdl.data()), concreteWdl.size());
        if (static_cast<std::size_t>(stream.gcount()) != concreteWdl.size())
            throw std::runtime_error("truncated concrete Jester WDL plane");

        const auto concrete_result = [&](std::uint32_t index) {
            return static_cast<Wdl>(
              (concreteWdl[index / 4] >> (2 * (index % 4))) & 3);
        };
        const auto alternative = [&](std::uint32_t index) {
            return primary_jester_alternative(index);
        };

        std::vector<std::int8_t> admittedCache(stateCount_, -1);
        const auto admitted = [&](std::uint32_t index) {
            std::int8_t& cached = admittedCache[index];
            if (cached >= 0)
                return cached != 0;
            Position position;
            const bool value = make_position_at(index, position) &&
                               !position.has_forced_action() &&
                               position.ordinary_predecessor_king_safe();
            cached = value ? 1 : 0;
            return value;
        };

        const DisclosureContext onyxView{Color::Black, false};
        const auto pair_representative = [&](std::uint32_t index)
          -> std::optional<std::uint32_t> {
            const std::uint32_t other = alternative(index);
            if (other == index || !admitted(index) || !admitted(other))
                return std::nullopt;
            Position first, second;
            if (!make_primary_jester_world(index, false, first) ||
                !make_primary_jester_world(index, true, second))
                throw std::runtime_error("admitted royal assignment failed reconstruction");
            if (view_key(first, onyxView) != view_key(second, onyxView))
                return std::nullopt;
            return std::min(index, other);
        };

        std::vector<std::uint32_t> pairs;
        pairs.reserve(stateCount_ / 2);
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            const std::uint32_t other = alternative(index);
            if (index >= other)
                continue;
            const auto representative = pair_representative(index);
            if (representative && *representative == index)
                pairs.push_back(index);
        }
        std::cout << "information_frontier class kjesterk concrete " << stateCount_
                  << " paired_sets " << pairs.size() << '\n' << std::flush;

        std::vector<std::uint8_t> ivoryForce(stateCount_, 0);
        std::vector<std::uint8_t> onyxForce(stateCount_, 0);
        std::uint64_t observationChecks = 0;

        const auto exact_index_forces = [&](std::uint32_t index, Color target) {
            const Wdl result = concrete_result(index);
            const Color side = encoded_side(index);
            return (result == Wdl::Win && target == side) ||
                   (result == Wdl::Loss && target != side);
        };
        const auto exact_position_forces = [&](const Position& position, Color target) {
            if (position.game_over()) {
                const auto winner = position.winner();
                return winner && *winner == target;
            }
            if (!position.is_checkmate_possible())
                return false;
            const auto result = TablebaseProbe::probe(position);
            if (!result)
                return false;
            return (result->wdl == TablebaseWdl::Win &&
                    target == position.side_to_move()) ||
                   (result->wdl == TablebaseWdl::Loss &&
                    target != position.side_to_move());
        };

        struct CachedSuccessor {
            std::uint32_t information = std::numeric_limits<std::uint32_t>::max();
            std::uint32_t actual = std::numeric_limits<std::uint32_t>::max();
            bool exactIvory = false;
            bool exactOnyx = false;
        };
        struct CachedNode {
            Color mover = Color::White;
            bool terminal = false;
            std::array<bool, 2> terminalIvory{false, false};
            bool terminalOnyx = false;
            std::array<std::vector<CachedSuccessor>, 2> informedMoves;
            std::vector<std::array<CachedSuccessor, 2>> commonMoves;
        };
        struct RawChild {
            bool sameClass = false;
            std::uint32_t index = 0;
            Position external;
        };
        struct MoveEdge {
            std::string action;
            std::string observation;
            RawChild child;
            CachedSuccessor successor;
        };

        const auto classify_successors = [&](const std::vector<RawChild>& raw) {
            std::vector<std::uint32_t> sameClass;
            std::vector<Position> external;
            for (const RawChild& child : raw) {
                if (child.sameClass)
                    sameClass.push_back(child.index);
                else
                    external.push_back(child.external);
            }
            std::sort(sameClass.begin(), sameClass.end());
            sameClass.erase(std::unique(sameClass.begin(), sameClass.end()), sameClass.end());
            std::sort(external.begin(), external.end(), [&](const Position& lhs,
                                                            const Position& rhs) {
                return lhs.upn() < rhs.upn();
            });
            external.erase(std::unique(external.begin(), external.end(),
              [&](const Position& lhs, const Position& rhs) {
                  return lhs.upn() == rhs.upn();
              }), external.end());
            if (sameClass.empty() && external.empty())
                throw std::runtime_error("actual information successor disappeared");
            if (!sameClass.empty() && !external.empty())
                throw std::runtime_error(
                  "one public observation mixed concrete material classes");

            CachedSuccessor result;
            if (!external.empty()) {
                result.exactOnyx = std::all_of(
                  external.begin(), external.end(), [&](const Position& child) {
                      return exact_position_forces(child, Color::Black);
                  });
                return result;
            }
            if (sameClass.size() == 1) {
                result.actual = sameClass.front();
                result.exactIvory = exact_index_forces(
                  sameClass.front(), Color::White);
                result.exactOnyx = exact_index_forces(
                  sameClass.front(), Color::Black);
                return result;
            }
            if (sameClass.size() != 2)
                throw std::runtime_error("Jester belief has more than two assignments");
            const auto representative = pair_representative(sameClass.front());
            if (!representative || *representative != sameClass.front() ||
                alternative(sameClass.front()) != sameClass.back())
                throw std::runtime_error(
                  "observation produced a noncanonical two-royal belief");
            result.information = *representative;
            return result;
        };

        const auto graphStart = std::chrono::steady_clock::now();
        std::vector<CachedNode> graph(pairs.size());
        std::uint64_t emptyCommonActionSets = 0;
        std::uint32_t firstEmptyCommonActionSet =
          std::numeric_limits<std::uint32_t>::max();
        for (std::size_t nodeIndex = 0; nodeIndex < pairs.size(); ++nodeIndex) {
            const std::uint32_t representative = pairs[nodeIndex];
            const std::array<std::uint32_t, 2> worlds{
              representative, alternative(representative)};
            std::array<Position, 2> positions;
            if (!make_primary_jester_world(representative, false, positions[0]) ||
                !make_primary_jester_world(representative, true, positions[1]))
                throw std::runtime_error("paired information node is invalid");

            CachedNode& node = graph[nodeIndex];
            node.mover = positions.front().side_to_move();
            if (positions.front().game_over()) {
                node.terminal = true;
                for (std::size_t world = 0; world < worlds.size(); ++world) {
                    const auto winner = positions[world].winner();
                    node.terminalIvory[world] = winner && *winner == Color::White;
                }
                node.terminalOnyx = std::all_of(
                  positions.begin(), positions.end(), [](const Position& position) {
                      const auto winner = position.winner();
                      return winner && *winner == Color::Black;
                  });
                continue;
            }

            std::array<std::vector<MoveEdge>, 2> edges;
            std::map<std::string, std::vector<std::pair<std::size_t, std::size_t>>>
              byObservation;
            for (std::size_t world = 0; world < worlds.size(); ++world) {
                const auto moves = positions[world].legal_moves();
                edges[world].reserve(moves.size());
                for (const Move& move : moves) {
                    Position after = positions[world];
                    if (!after.apply_move_unchecked(move))
                        throw std::runtime_error("information graph move failed");
                    MoveEdge edge;
                    edge.action = positions[world].move_to_string(move);
                    edge.observation = transition_observation_key(
                      positions[world], move, after, onyxView);
                    edge.child.sameClass = in_class(after);
                    if (edge.child.sameClass)
                        edge.child.index = child_index(after);
                    else
                        edge.child.external = std::move(after);
                    edges[world].push_back(std::move(edge));
                    byObservation[edges[world].back().observation].push_back(
                      {world, edges[world].size() - 1});
                    ++observationChecks;
                }
            }
            for (const auto& [observation, members] : byObservation) {
                (void)observation;
                std::vector<RawChild> raw;
                raw.reserve(members.size());
                for (const auto [world, edge] : members)
                    raw.push_back(edges[world][edge].child);
                const CachedSuccessor successor = classify_successors(raw);
                for (const auto [world, edge] : members) {
                    CachedSuccessor actualSuccessor = successor;
                    const RawChild& actualChild = edges[world][edge].child;
                    if (actualSuccessor.information !=
                        std::numeric_limits<std::uint32_t>::max()) {
                        if (!actualChild.sameClass)
                            throw std::runtime_error(
                              "paired information successor lost its actual world");
                        actualSuccessor.actual = actualChild.index;
                    }
                    else if (actualChild.sameClass) {
                        actualSuccessor.actual = actualChild.index;
                        actualSuccessor.exactIvory = exact_index_forces(
                          actualChild.index, Color::White);
                    }
                    else
                        actualSuccessor.exactIvory = exact_position_forces(
                          actualChild.external, Color::White);
                    edges[world][edge].successor = actualSuccessor;
                }
            }

            if (node.mover == Color::White) {
                for (std::size_t world = 0; world < worlds.size(); ++world)
                    for (const MoveEdge& edge : edges[world])
                        node.informedMoves[world].push_back(edge.successor);
            }
            else {
                std::array<std::map<std::string, CachedSuccessor>, 2> byAction;
                for (std::size_t world = 0; world < worlds.size(); ++world)
                    for (const MoveEdge& edge : edges[world])
                        if (!byAction[world].emplace(edge.action, edge.successor).second)
                            throw std::runtime_error(
                              "duplicate public action in one concrete Jester world");
                for (const auto& [action, successor] : byAction[0]) {
                    const auto other = byAction[1].find(action);
                    if (other != byAction[1].end())
                        node.commonMoves.push_back({successor, other->second});
                }
                if (node.commonMoves.empty()) {
                    ++emptyCommonActionSets;
                    firstEmptyCommonActionSet = std::min(
                      firstEmptyCommonActionSet, representative);
                }
            }
            if ((nodeIndex + 1) % 20'000 == 0) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - graphStart).count();
                std::cout << "information_graph nodes " << nodeIndex + 1 << '/'
                          << pairs.size() << " observations " << observationChecks
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        std::cout << "information_uniform_actions empty_sets "
                  << emptyCommonActionSets << " first_index ";
        if (firstEmptyCommonActionSet == std::numeric_limits<std::uint32_t>::max())
            std::cout << "none\n";
        else
            std::cout << firstEmptyCommonActionSet << '\n';

        const auto successor_forces_ivory = [&](const CachedSuccessor& successor,
                                                const std::vector<std::uint8_t>& force) {
            if (successor.information != std::numeric_limits<std::uint32_t>::max()) {
                if (successor.actual == std::numeric_limits<std::uint32_t>::max())
                    throw std::runtime_error("Ivory successor has no actual world");
                return force[successor.actual] != 0;
            }
            return successor.exactIvory;
        };
        const auto successor_forces_onyx = [&](const CachedSuccessor& successor,
                                               const std::vector<std::uint8_t>& force) {
            if (successor.information != std::numeric_limits<std::uint32_t>::max())
                return force[successor.information] != 0;
            return successor.exactOnyx;
        };
        const auto ivory_satisfies = [&](const CachedNode& node,
                                         std::size_t actualWorld,
                                         const std::vector<std::uint8_t>& force) {
            if (node.terminal)
                return node.terminalIvory[actualWorld];
            if (node.mover == Color::White)
                return std::any_of(node.informedMoves[actualWorld].begin(),
                                   node.informedMoves[actualWorld].end(),
                  [&](const CachedSuccessor& successor) {
                      return successor_forces_ivory(successor, force);
                  });
            return std::all_of(node.commonMoves.begin(), node.commonMoves.end(),
              [&](const std::array<CachedSuccessor, 2>& action) {
                  return successor_forces_ivory(action[actualWorld], force);
              });
        };
        const auto onyx_satisfies = [&](const CachedNode& node,
                                        const std::vector<std::uint8_t>& force) {
            if (node.terminal)
                return node.terminalOnyx;
            if (node.mover == Color::White)
                return std::all_of(node.informedMoves.begin(), node.informedMoves.end(),
                  [&](const std::vector<CachedSuccessor>& worldMoves) {
                      return std::all_of(worldMoves.begin(), worldMoves.end(),
                        [&](const CachedSuccessor& successor) {
                            return successor_forces_onyx(successor, force);
                        });
                  });
            return std::any_of(node.commonMoves.begin(), node.commonMoves.end(),
              [&](const std::array<CachedSuccessor, 2>& action) {
                  return std::all_of(action.begin(), action.end(),
                    [&](const CachedSuccessor& successor) {
                        return successor_forces_onyx(successor, force);
                    });
              });
        };

        std::uint32_t iteration = 0;
        for (;;) {
            bool changed = false;
            ++iteration;
            for (std::size_t nodeIndex = 0; nodeIndex < pairs.size(); ++nodeIndex) {
                const std::uint32_t representative = pairs[nodeIndex];
                const std::array<std::uint32_t, 2> worlds{
                  representative, alternative(representative)};
                for (std::size_t world = 0; world < worlds.size(); ++world) {
                    if (!ivoryForce[worlds[world]] &&
                        ivory_satisfies(graph[nodeIndex], world, ivoryForce)) {
                        ivoryForce[worlds[world]] = 1;
                        changed = true;
                    }
                }
                if (!onyxForce[representative] &&
                    onyx_satisfies(graph[nodeIndex], onyxForce)) {
                    onyxForce[representative] = 1;
                    changed = true;
                }
            }
            const auto ivoryCount = std::count(ivoryForce.begin(), ivoryForce.end(), 1);
            const auto onyxCount = std::count(onyxForce.begin(), onyxForce.end(), 1);
            std::cout << "information_propagate iteration " << iteration
                      << " ivory " << ivoryCount << " onyx " << onyxCount
                      << " observations " << observationChecks << '\n' << std::flush;
            if (!changed)
                break;
        }

        std::uint64_t bellmanResidual = 0;
        for (std::size_t nodeIndex = 0; nodeIndex < pairs.size(); ++nodeIndex) {
            const std::uint32_t representative = pairs[nodeIndex];
            const std::array<std::uint32_t, 2> worlds{
              representative, alternative(representative)};
            for (std::size_t world = 0; world < worlds.size(); ++world)
                bellmanResidual += (ivoryForce[worlds[world]] !=
                  ivory_satisfies(graph[nodeIndex], world, ivoryForce));
            bellmanResidual += (onyxForce[representative] !=
              onyx_satisfies(graph[nodeIndex], onyxForce));
            for (const std::uint32_t world : worlds)
                if (ivoryForce[world] && onyxForce[representative])
                    throw std::runtime_error(
                      "both teams have a sure win in one actual information state");
        }

        using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
        Counts totals{};
        Counts unreachable{};
        std::vector<std::uint8_t> epistemicFlags(stateCount_, 0);
        std::array<std::set<std::uint32_t>, 2> initialSets;
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            const std::size_t side = static_cast<std::size_t>(encoded_side(index));
            const Wdl concrete = concrete_result(index);
            if (!admitted(index)) {
                ++unreachable[side][static_cast<std::size_t>(concrete)];
                continue;
            }
            const auto representative = pair_representative(index);
            if (!representative) {
                ++totals[side][static_cast<std::size_t>(concrete)];
                initialSets[side].insert(index);
                epistemicFlags[index] = 4 |
                  (exact_index_forces(index, Color::White) ? 1 : 0) |
                  (exact_index_forces(index, Color::Black) ? 2 : 0);
                continue;
            }
            initialSets[side].insert(*representative);
            epistemicFlags[index] = 4 |
              (ivoryForce[index] ? 1 : 0) |
              (onyxForce[*representative] ? 2 : 0);
            const Color mover = encoded_side(index);
            const bool moverWins = mover == Color::White
                                 ? ivoryForce[index]
                                 : onyxForce[*representative];
            const bool moverLoses = mover == Color::White
                                  ? onyxForce[*representative]
                                  : ivoryForce[index];
            const Wdl result = moverWins ? Wdl::Win
                             : moverLoses ? Wdl::Loss : Wdl::Draw;
            ++totals[side][static_cast<std::size_t>(result)];
        }
        for (std::size_t side = 0; side < 2; ++side) {
            std::uint64_t conserved = 0;
            for (std::size_t result = 1; result < 4; ++result)
                conserved += totals[side][result] + unreachable[side][result];
            if (conserved != stateCount_ / 2)
                throw std::runtime_error("information root counts do not conserve states");
            std::cout << "information_summary side " << side
                      << " win " << totals[side][1]
                      << " loss " << totals[side][2]
                      << " draw " << totals[side][3]
                      << " unreachable_win " << unreachable[side][1]
                      << " unreachable_loss " << unreachable[side][2]
                      << " unreachable_draw " << unreachable[side][3]
                      << " sets " << initialSets[side].size()
                      << " concrete " << stateCount_ / 2
                      << " bellman_residual " << bellmanResidual
                      << " belief_cap none exhaustive 1\n";
        }
        if (!overlayOutput.empty()) {
            std::ofstream output(overlayOutput, std::ios::binary | std::ios::trunc);
            if (!output)
                throw std::runtime_error("cannot create information overlay");
            const std::array<char, 8> magic{{'U','F','I','W','1','\0','\0','\0'}};
            const std::uint32_t overlayVersion = 1;
            const std::uint32_t primary = static_cast<std::uint32_t>(attackerType_);
            const std::uint32_t secondary = static_cast<std::uint32_t>(secondaryType_);
            const std::uint32_t secondaryColor =
              static_cast<std::uint32_t>(secondaryColor_);
            output.write(magic.data(), magic.size());
            for (const std::uint32_t value : {
                   overlayVersion, primary, secondary, secondaryColor,
                   stateCount_, substates_})
                output.write(reinterpret_cast<const char*>(&value), sizeof(value));
            output.write(reinterpret_cast<const char*>(epistemicFlags.data()),
                         epistemicFlags.size());
            if (!output)
                throw std::runtime_error("failed writing information overlay");
            std::cout << "information_overlay " << overlayOutput
                      << " bytes " << epistemicFlags.size() + 32 << '\n';
        }
    }

    // Compact exact solver for every closed class with one primary Ivory
    // Jester.  It compiles the observation game into two monotone systems:
    // one variable per actual world for the informed Jester owner, and one
    // variable per royal pair that remains indistinguishable after the mover's
    // private pre-decision legal-dot observation. Black-to-move pairs whose
    // UI marker frontiers differ are exact singleton information states. The fixed-
    // point backend stores its CSR and queue in anonymous scratch mappings, so
    // the 38-million-state K+K+2 classes do not materialize a heap vector for
    // every move.
    void solve_jester_information(const std::string& input,
                                  const std::string& lowerOverlay,
                                  const std::string& lowerSourceSha256,
                                  const std::string& sourceSha256,
                                  const std::string& modelSha256,
                                  const std::string& overlayOutput,
                                  const std::string& scratchDirectory) const {
        if (attackerType_ != PieceType::Jester || identicalExtras_ ||
            compoundCopycat_ || copycatOnly_)
            throw std::runtime_error(
              "exact primary-Jester solver requires exactly one Ivory Jester");

        std::ifstream stream(input, std::ios::binary);
        if (!stream)
            throw std::runtime_error("cannot open concrete Jester tablebase");
        std::array<std::uint8_t, 56> header{};
        stream.read(reinterpret_cast<char*>(header.data()), header.size());
        if (stream.gcount() < 40 || std::memcmp(header.data(), "UFTB1\0\0\0", 8) != 0)
            throw std::runtime_error("invalid concrete Jester tablebase header");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const std::uint32_t version = word(8);
        const std::uint32_t count = word(16);
        const std::uint32_t wdlBytes = word(28);
        if (version < 4 || version > 6 ||
            word(12) != static_cast<std::uint32_t>(attackerType_) ||
            count != stateCount_ || word(24) != substates_ ||
            wdlBytes != (count + 3) / 4 ||
            (version >= 5 &&
             (word(40) != static_cast<std::uint32_t>(secondaryType_) ||
              word(44) != static_cast<std::uint32_t>(secondaryColor_))))
            throw std::runtime_error("concrete Jester tablebase does not match codec");
        const std::size_t planeOffset = 40 + (version >= 5 ? 8 : 0) +
                                        (version >= 6 ? 8 : 0);
        stream.seekg(static_cast<std::streamoff>(planeOffset));
        std::vector<std::uint8_t> concreteWdl(wdlBytes);
        stream.read(reinterpret_cast<char*>(concreteWdl.data()), concreteWdl.size());
        if (static_cast<std::size_t>(stream.gcount()) != concreteWdl.size())
            throw std::runtime_error("truncated concrete Jester WDL plane");
        const auto concrete_result = [&](std::uint32_t index) {
            return static_cast<Wdl>(
              (concreteWdl[index / 4] >> (2 * (index % 4))) & 3);
        };

        std::vector<std::int8_t> admittedCache(stateCount_, -1);
        const auto admitted = [&](std::uint32_t index) {
            std::int8_t& cached = admittedCache[index];
            if (cached >= 0)
                return cached != 0;
            Position position;
            const bool value = make_position_at(index, position) &&
                               !position.has_forced_action() &&
                               position.ordinary_predecessor_king_safe();
            cached = value ? 1 : 0;
            return value;
        };

        std::vector<std::uint32_t> pairs;
        pairs.reserve(stateCount_ / 2);
        std::vector<std::int32_t> pairForIndex(stateCount_, -1);
        std::array<std::uint64_t, 2> dotSplitPairs{};
        const auto frontierStart = std::chrono::steady_clock::now();
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            const std::uint32_t other = primary_jester_alternative(index);
            if (index >= other || !admitted(index) || !admitted(other))
                continue;
            Position first, second;
            if (!make_primary_jester_world(index, false, first) ||
                !make_primary_jester_world(index, true, second))
                throw std::runtime_error("admitted royal assignment failed reconstruction");
            if (primary_jester_view_key(first) != primary_jester_view_key(second))
                continue;
            if (first.side_to_move() == Color::Black) {
                if (primary_jester_decision_markers(first) !=
                    primary_jester_decision_markers(second)) {
                    ++dotSplitPairs[static_cast<std::size_t>(Color::Black)];
                    continue;
                }
            }
            if (pairs.size() >= static_cast<std::size_t>(InformationTrue))
                throw std::runtime_error("too many information pairs for token encoding");
            const std::int32_t pair = static_cast<std::int32_t>(pairs.size());
            pairs.push_back(index);
            pairForIndex[index] = pair;
            pairForIndex[other] = pair;
            if (pairs.size() % 1'000'000 == 0) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - frontierStart).count();
                std::cout << "information_frontier pairs " << pairs.size()
                          << " index " << index << '/' << stateCount_
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        if (pairs.size() > InformationTrue / 2)
            throw std::runtime_error("too many actual-world variables for token encoding");
        std::array<std::uint64_t, 2> pairedSets{};
        std::array<std::uint64_t, 2> singletonSets{};
        std::array<std::uint64_t, 2> admittedWorlds{};
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            if (!admitted(index))
                continue;
            const std::size_t side = static_cast<std::size_t>(encoded_side(index));
            ++admittedWorlds[side];
            if (pairForIndex[index] < 0)
                ++singletonSets[side];
            else if (index == pairs[static_cast<std::size_t>(pairForIndex[index])])
                ++pairedSets[side];
        }
        for (std::size_t side = 0; side < 2; ++side)
            if (admittedWorlds[side] != singletonSets[side] + 2 * pairedSets[side])
                throw std::runtime_error(
                  "legal-dot root partition does not conserve admitted worlds");
        std::cout << "information_frontier concrete " << stateCount_
                  << " paired_sets " << pairs.size()
                  << " white_pairs " << pairedSets[0]
                  << " black_pairs " << pairedSets[1]
                  << " white_singletons " << singletonSets[0]
                  << " black_singletons " << singletonSets[1]
                  << " black_dot_split_pairs " << dotSplitPairs[1]
                  << " partition_residual 0\n" << std::flush;

        // The v1 solver incorrectly forced concrete index 492966 to share an
        // action with its swapped royal assignment. Native pre-decision dots
        // distinguish those two Black-to-move worlds, so both must now be
        // singleton roots before any action gate is constructed.
        if (!fourModels_ && stateCount_ == PlacementStateCount) {
            constexpr std::uint32_t LegalDotWitness = 492'966;
            const std::uint32_t other =
              primary_jester_alternative(LegalDotWitness);
            if (admitted(LegalDotWitness) && admitted(other)) {
                Position first, second;
                if (!make_primary_jester_world(
                      LegalDotWitness, false, first) ||
                    !make_primary_jester_world(
                      LegalDotWitness, true, second))
                    throw std::runtime_error(
                      "legal-dot witness reconstruction failed");
                if (primary_jester_view_key(first) !=
                      primary_jester_view_key(second) ||
                    primary_jester_decision_markers(first) ==
                      primary_jester_decision_markers(second) ||
                    pairForIndex[LegalDotWitness] >= 0 ||
                    pairForIndex[other] >= 0)
                    throw std::runtime_error(
                      "index 492966 was not split by its private legal dots");
                std::cout << "information_legal_dot_witness index "
                          << LegalDotWitness << " alternative " << other
                          << " paired 0\n";
            }
        }

        const auto white_variable = [&](std::uint32_t index) -> InformationToken {
            const std::int32_t pair = pairForIndex.at(index);
            if (pair < 0)
                throw std::runtime_error("ambiguous successor lacks a pair variable");
            const std::uint32_t representative = pairs[static_cast<std::size_t>(pair)];
            const std::uint32_t alternative = primary_jester_alternative(representative);
            if (index != representative && index != alternative)
                throw std::runtime_error("actual successor is outside its royal pair");
            return static_cast<InformationToken>(2 * pair +
              (index == representative ? 0 : 1));
        };
        const auto boolean_token = [](bool value) {
            return value ? InformationTrue : InformationFalse;
        };
        const auto exact_index_forces = [&](std::uint32_t index, Color target) {
            const Wdl result = concrete_result(index);
            const Color side = encoded_side(index);
            return (result == Wdl::Win && target == side) ||
                   (result == Wdl::Loss && target != side);
        };

        if (sourceSha256.size() != 64 || modelSha256.size() != 64)
            throw std::runtime_error(
              "exact information solve requires 64-digit source/model SHA-256 bindings");
        const JesterInformationOverlay lower(
          lowerOverlay, lowerSourceSha256, modelSha256);
        const auto exact_position_forces = [&](const Position& position, Color target) {
            if (position.game_over()) {
                const auto winner = position.winner();
                return winner && *winner == target;
            }
            if (const auto result = lower.forces(position, target))
                return *result;
            if (!position.is_checkmate_possible())
                return false;
            const auto result = TablebaseProbe::probe(position);
            if (!result)
                throw std::runtime_error(
                  "missing exact lower-material table during information solve: " +
                  position.upn());
            return (result->wdl == TablebaseWdl::Win &&
                    target == position.side_to_move()) ||
                   (result->wdl == TablebaseWdl::Loss &&
                    target != position.side_to_move());
        };

        auto ivory = std::make_unique<InformationFixedPoint>(
          static_cast<std::uint32_t>(pairs.size() * 2), scratchDirectory);
        auto onyx = std::make_unique<InformationFixedPoint>(
          static_cast<std::uint32_t>(pairs.size()), scratchDirectory);

        struct RawChild {
            bool sameClass = false;
            std::uint32_t index = 0;
            Position external;
        };
        struct MoveEdge {
            std::string action;
            std::string observation;
            RawChild child;
            InformationToken ivory = InformationFalse;
            InformationToken onyx = InformationFalse;
        };

        std::uint64_t observationChecks = 0;
        std::uint64_t emptyCommonActionSets = 0;
        std::uint32_t firstEmptyCommonActionSet =
          std::numeric_limits<std::uint32_t>::max();
        const auto graphStart = std::chrono::steady_clock::now();
        for (std::size_t pairId = 0; pairId < pairs.size(); ++pairId) {
            const std::uint32_t representative = pairs[pairId];
            const std::array<std::uint32_t, 2> worlds{
              representative, primary_jester_alternative(representative)};
            std::array<Position, 2> positions;
            if (!make_primary_jester_world(representative, false, positions[0]) ||
                !make_primary_jester_world(representative, true, positions[1]))
                throw std::runtime_error("paired information node is invalid");

            if (positions.front().game_over()) {
                for (std::size_t world = 0; world < worlds.size(); ++world) {
                    const auto winner = positions[world].winner();
                    const std::array<InformationToken, 1> child{{boolean_token(
                      winner && *winner == Color::White)}};
                    ivory->define_or(static_cast<std::uint32_t>(2 * pairId + world),
                                     child.data(), child.size());
                }
                const auto winner = positions.front().winner();
                const std::array<InformationToken, 1> child{{boolean_token(
                  winner && *winner == Color::Black)}};
                onyx->define_or(static_cast<std::uint32_t>(pairId),
                                child.data(), child.size());
                continue;
            }

            std::array<std::vector<MoveEdge>, 2> edges;
            std::map<std::string, std::vector<std::pair<std::size_t, std::size_t>>>
              byObservation;
            for (std::size_t world = 0; world < worlds.size(); ++world) {
                const auto moves = positions[world].legal_moves();
                edges[world].reserve(moves.size());
                for (const Move& move : moves) {
                    Position after = positions[world];
                    if (!after.apply_move_unchecked(move))
                        throw std::runtime_error("information graph move failed");
                    MoveEdge edge;
                    edge.action = positions[world].move_to_string(move);
                    edge.observation = primary_jester_transition_key(
                      positions[world], move, after);
                    edge.child.sameClass = in_class(after);
                    if (edge.child.sameClass)
                        edge.child.index = child_index(after);
                    else
                        edge.child.external = std::move(after);
                    edges[world].push_back(std::move(edge));
                    byObservation[edges[world].back().observation].push_back(
                      {world, edges[world].size() - 1});
                    ++observationChecks;
                }
            }

            for (const auto& [observation, members] : byObservation) {
                (void)observation;
                std::vector<std::uint32_t> sameClass;
                std::vector<const Position*> external;
                for (const auto [world, edge] : members) {
                    const RawChild& child = edges[world][edge].child;
                    if (child.sameClass)
                        sameClass.push_back(child.index);
                    else
                        external.push_back(&child.external);
                }
                std::sort(sameClass.begin(), sameClass.end());
                sameClass.erase(std::unique(sameClass.begin(), sameClass.end()),
                                sameClass.end());
                std::sort(external.begin(), external.end(),
                          [](const Position* lhs, const Position* rhs) {
                              return lhs->upn() < rhs->upn();
                          });
                external.erase(std::unique(external.begin(), external.end(),
                  [](const Position* lhs, const Position* rhs) {
                      return lhs->upn() == rhs->upn();
                  }), external.end());
                if (sameClass.empty() == external.empty())
                    throw std::runtime_error(
                      "one observation is empty or mixes concrete material classes");

                InformationToken groupOnyx = InformationFalse;
                bool ambiguous = false;
                if (!sameClass.empty()) {
                    if (sameClass.size() == 1)
                        groupOnyx = boolean_token(exact_index_forces(
                          sameClass.front(), Color::Black));
                    else if (sameClass.size() == 2) {
                        const std::int32_t pair = pairForIndex[sameClass.front()];
                        if (pair < 0 || pairForIndex[sameClass.back()] != pair)
                            throw std::runtime_error(
                              "observation produced a noncanonical royal pair");
                        groupOnyx = static_cast<InformationToken>(pair);
                        ambiguous = true;
                    }
                    else
                        throw std::runtime_error(
                          "Jester belief has more than two royal assignments");
                }
                else {
                    const bool blackForces = std::all_of(
                      external.begin(), external.end(), [&](const Position* child) {
                          return exact_position_forces(*child, Color::Black);
                      });
                    groupOnyx = boolean_token(blackForces);
                }

                for (const auto [world, edgeIndex] : members) {
                    MoveEdge& edge = edges[world][edgeIndex];
                    edge.onyx = groupOnyx;
                    if (edge.child.sameClass) {
                        edge.ivory = ambiguous
                          ? white_variable(edge.child.index)
                          : boolean_token(exact_index_forces(
                              edge.child.index, Color::White));
                    }
                    else
                        edge.ivory = boolean_token(exact_position_forces(
                          edge.child.external, Color::White));
                }
            }

            if (positions.front().side_to_move() == Color::White) {
                std::vector<InformationToken> blackChildren;
                for (std::size_t world = 0; world < worlds.size(); ++world) {
                    std::vector<InformationToken> whiteChildren;
                    whiteChildren.reserve(edges[world].size());
                    for (const MoveEdge& edge : edges[world]) {
                        whiteChildren.push_back(edge.ivory);
                        blackChildren.push_back(edge.onyx);
                    }
                    ivory->define_or(static_cast<std::uint32_t>(2 * pairId + world),
                                     whiteChildren);
                }
                onyx->define_and(static_cast<std::uint32_t>(pairId), blackChildren);
            }
            else {
                std::array<std::map<std::string, const MoveEdge*>, 2> byAction;
                for (std::size_t world = 0; world < worlds.size(); ++world)
                    for (const MoveEdge& edge : edges[world])
                        if (!byAction[world].emplace(edge.action, &edge).second)
                            throw std::runtime_error(
                              "duplicate public action in one concrete Jester world");
                std::array<std::vector<InformationToken>, 2> whiteChildren;
                std::vector<InformationPair> blackChildren;
                for (const auto& [action, first] : byAction[0]) {
                    const auto other = byAction[1].find(action);
                    if (other == byAction[1].end())
                        continue;
                    whiteChildren[0].push_back(first->ivory);
                    whiteChildren[1].push_back(other->second->ivory);
                    blackChildren.push_back({first->onyx, other->second->onyx});
                }
                for (std::size_t world = 0; world < worlds.size(); ++world)
                    ivory->define_and(static_cast<std::uint32_t>(2 * pairId + world),
                                      whiteChildren[world]);
                onyx->define_or_of_pairs(static_cast<std::uint32_t>(pairId),
                                         blackChildren);
                if (blackChildren.empty()) {
                    ++emptyCommonActionSets;
                    firstEmptyCommonActionSet = std::min(
                      firstEmptyCommonActionSet, representative);
                }
            }

            if ((pairId + 1) % 100'000 == 0) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - graphStart).count();
                std::cout << "information_graph pairs " << pairId + 1 << '/'
                          << pairs.size() << " observations " << observationChecks
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        std::cout << "information_uniform_actions empty_sets "
                  << emptyCommonActionSets << " first_index ";
        if (firstEmptyCommonActionSet == std::numeric_limits<std::uint32_t>::max())
            std::cout << "none\n";
        else
            std::cout << firstEmptyCommonActionSet << '\n';

        const InformationSolveSummary ivorySummary = ivory->solve();
        std::vector<std::uint8_t> ivoryForce(pairs.size() * 2);
        for (std::size_t variable = 0; variable < ivoryForce.size(); ++variable)
            ivoryForce[variable] = ivory->value(
              static_cast<InformationToken>(variable));
        ivory.reset();
        const InformationSolveSummary onyxSummary = onyx->solve();
        std::vector<std::uint8_t> onyxForce(pairs.size());
        for (std::size_t variable = 0; variable < onyxForce.size(); ++variable)
            onyxForce[variable] = onyx->value(
              static_cast<InformationToken>(variable));
        onyx.reset();
        if (ivorySummary.bellmanResidual || ivorySummary.rankResidual ||
            onyxSummary.bellmanResidual || onyxSummary.rankResidual)
            throw std::runtime_error("information fixed point has a nonzero residual");
        std::cout << "information_fixed_point ivory_variables "
                  << ivorySummary.variables << " ivory_edges "
                  << ivorySummary.reverseEdges << " ivory_activated "
                  << ivorySummary.activated << " onyx_variables "
                  << onyxSummary.variables << " onyx_edges "
                  << onyxSummary.reverseEdges << " onyx_activated "
                  << onyxSummary.activated << " bellman_residual 0 rank_residual 0\n";

        using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
        using CrossTab = std::array<std::array<std::array<std::uint64_t, 4>, 4>, 2>;
        Counts totals{};
        Counts unreachable{};
        CrossTab crossTab{};
        std::array<std::uint64_t, 2> initialSets{};
        std::vector<std::uint8_t> epistemicFlags(stateCount_, 0);
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            const std::size_t side = static_cast<std::size_t>(encoded_side(index));
            const Wdl concrete = concrete_result(index);
            if (!admitted(index)) {
                ++unreachable[side][static_cast<std::size_t>(concrete)];
                continue;
            }
            const std::int32_t pair = pairForIndex[index];
            bool ivoryForces = false;
            bool onyxForces = false;
            if (pair < 0) {
                ++initialSets[side];
                ivoryForces = exact_index_forces(index, Color::White);
                onyxForces = exact_index_forces(index, Color::Black);
            }
            else {
                if (index == pairs[static_cast<std::size_t>(pair)])
                    ++initialSets[side];
                ivoryForces = ivoryForce[white_variable(index)] != 0;
                onyxForces = onyxForce[static_cast<std::size_t>(pair)] != 0;
            }
            if (ivoryForces && onyxForces)
                throw std::runtime_error(
                  "both teams have a sure win in one actual information state");
            epistemicFlags[index] = 4 | (ivoryForces ? 1 : 0) |
                                    (onyxForces ? 2 : 0);
            const Color mover = encoded_side(index);
            const bool moverWins = mover == Color::White ? ivoryForces : onyxForces;
            const bool moverLoses = mover == Color::White ? onyxForces : ivoryForces;
            const Wdl result = moverWins ? Wdl::Win
                             : moverLoses ? Wdl::Loss : Wdl::Draw;
            ++totals[side][static_cast<std::size_t>(result)];
            ++crossTab[side][static_cast<std::size_t>(concrete)]
                           [static_cast<std::size_t>(result)];
        }
        for (std::size_t side = 0; side < 2; ++side) {
            std::uint64_t conserved = 0;
            for (std::size_t result = 1; result < 4; ++result)
                conserved += totals[side][result] + unreachable[side][result];
            if (conserved != stateCount_ / 2)
                throw std::runtime_error("information root counts do not conserve states");
            std::cout << "information_summary side " << side
                      << " win " << totals[side][1]
                      << " loss " << totals[side][2]
                      << " draw " << totals[side][3]
                      << " unreachable_win " << unreachable[side][1]
                      << " unreachable_loss " << unreachable[side][2]
                      << " unreachable_draw " << unreachable[side][3]
                      << " sets " << initialSets[side]
                      << " concrete " << stateCount_ / 2
                      << " bellman_residual 0 rank_residual 0"
                      << " belief_cap none exhaustive 1\n";
            for (std::size_t concrete = 1; concrete < 4; ++concrete)
                std::cout << "information_crosstab side " << side
                          << " concrete " << wdl_name(static_cast<Wdl>(concrete))
                          << " public_win " << crossTab[side][concrete][1]
                          << " public_loss " << crossTab[side][concrete][2]
                          << " public_draw " << crossTab[side][concrete][3] << '\n';
        }
        if (!overlayOutput.empty()) {
            std::ofstream output(overlayOutput, std::ios::binary | std::ios::trunc);
            if (!output)
                throw std::runtime_error("cannot create information overlay");
            const std::array<char, 8> magic{{'U','F','I','W','2','\0','\0','\0'}};
            const std::uint32_t overlayVersion = 2;
            const std::uint32_t primary = static_cast<std::uint32_t>(attackerType_);
            const std::uint32_t secondary = static_cast<std::uint32_t>(secondaryType_);
            const std::uint32_t secondaryColor =
              static_cast<std::uint32_t>(secondaryColor_);
            output.write(magic.data(), magic.size());
            for (const std::uint32_t value : {
                   overlayVersion, primary, secondary, secondaryColor,
                   stateCount_, substates_})
                output.write(reinterpret_cast<const char*>(&value), sizeof(value));
            output.write(sourceSha256.data(), sourceSha256.size());
            output.write(modelSha256.data(), modelSha256.size());
            output.write(reinterpret_cast<const char*>(epistemicFlags.data()),
                         epistemicFlags.size());
            if (!output)
                throw std::runtime_error("failed writing information overlay");
            std::cout << "information_overlay " << overlayOutput
                      << " bytes " << epistemicFlags.size() + 160 << '\n';
        }
    }

    void generate() {
        // Discover and materialize lower table dependencies before dirtying
        // multi-gigabyte mapped state planes. The first opponent-to-move state
        // otherwise triggers this work at the side-half boundary, where macOS
        // can SIGBUS a scratch mapping under transient VM pressure.
        TablebaseProbe::preload();
        allocate_state_planes();
        const auto start = std::chrono::steady_clock::now();
        std::uint32_t begin = load_checkpoint();
        const std::uint32_t progressEvery = checkpointEvery_ ? checkpointEvery_ : 2'000'000;
        const bool parallelScan = !checkpointEvery_ &&
                                  (diskBacked_ || stateCount_ >= 300'000'000);
        const auto scan = [&](const char* phase, std::uint32_t scanBegin, auto&& action) {
            if (!parallelScan) {
                for (std::uint32_t index = scanBegin; index < stateCount_; ++index) {
                    action(index, false);
                    if ((index + 1) % progressEvery == 0)
                        progress(phase, index + 1, start);
                }
                return;
            }
            constexpr std::uint32_t Block = 10'000;
            const std::uint32_t workers = std::min<std::uint32_t>(
              4, std::max(1u, std::thread::hardware_concurrency()));
            std::atomic<std::uint32_t> next{scanBegin};
            std::atomic<std::uint32_t> completed{scanBegin};
            std::atomic<std::uint32_t> nextReport{
              static_cast<std::uint32_t>((scanBegin / progressEvery + 1) * progressEvery)};
            std::vector<std::thread> tasks;
            for (std::uint32_t worker = 0; worker < workers; ++worker)
                tasks.emplace_back([&] {
                    for (;;) {
                        const std::uint32_t blockBegin = next.fetch_add(
                          Block, std::memory_order_relaxed);
                        if (blockBegin >= stateCount_)
                            break;
                        const std::uint32_t blockEnd = std::min(
                          stateCount_, static_cast<std::uint32_t>(blockBegin + Block));
                        for (std::uint32_t index = blockBegin; index < blockEnd; ++index)
                            action(index, true);
                        const std::uint32_t done = completed.fetch_add(
                          blockEnd - blockBegin, std::memory_order_relaxed) +
                          blockEnd - blockBegin;
                        std::uint32_t report = nextReport.load(std::memory_order_relaxed);
                        while (done >= report && report <= stateCount_ &&
                               !nextReport.compare_exchange_weak(
                                 report, static_cast<std::uint32_t>(report + progressEvery),
                                 std::memory_order_relaxed)) {}
                        if (done >= report && report <= stateCount_)
                            progress(phase, report, start);
                    }
                });
            for (auto& task : tasks)
                task.join();
        };
        scan("frontier", begin, [&](std::uint32_t index, bool atomic) {
            analyze_node(index, true, [&](std::uint32_t child, bool) {
                if (atomic)
                    __atomic_fetch_add(&predecessorCounts_[child], 1u, __ATOMIC_RELAXED);
                else
                    ++predecessorCounts_[child];
            });
        });
        if (checkpointEvery_)
            save_checkpoint(stateCount_);

        std::uint64_t edgeCount = 0;
        for (std::uint32_t index = 0; index < stateCount_; ++index)
            edgeCount += predecessorCounts_[index];
        const auto solve_arrays = [&](auto& offsets, auto& predecessors) {
            using Offset = std::remove_reference_t<decltype(offsets[0])>;
            offsets[0] = 0;
            for (std::uint32_t index = 0; index < stateCount_; ++index)
                offsets[index + 1] = static_cast<Offset>(offsets[index] +
                                                         predecessorCounts_[index]);
            constexpr std::uint32_t SameSideMask = std::uint32_t{1} << 31;
            if (stateCount_ >= SameSideMask)
                throw std::runtime_error("tablebase state index exceeds packed edge capacity");
            scan("reverse", 0, [&](std::uint32_t index, bool atomic) {
                analyze_node(index, false, [&](std::uint32_t child, bool sameSide) {
                    const Offset cursor = atomic
                      ? __atomic_fetch_add(&offsets[child], Offset{1}, __ATOMIC_RELAXED)
                      : offsets[child]++;
                    predecessors[cursor] = index | (sameSide ? SameSideMask : 0);
                });
            });

            // Filling reused the offsets as cursors. Reconstruct their prefix
            // values from the degree plane, then release that 4-byte-per-state
            // plane before the retrograde queue starts growing.
            Offset running = 0;
            for (std::uint32_t index = 0; index < stateCount_; ++index) {
                offsets[index] = running;
                running = static_cast<Offset>(running + predecessorCounts_[index]);
            }
            offsets[stateCount_] = running;
            if (mappedPredecessorCounts_)
                mappedPredecessorCounts_.reset();
            else
                std::vector<std::uint32_t>().swap(predecessorCountStorage_);
            predecessorCounts_ = nullptr;

            // DTW edges have unit cost. A Dial-style bucket queue preserves the
            // distance ordering required for shortest wins/longest losses without
            // paying O(log N) heap cost for tens of millions of solved states.
            std::vector<std::vector<std::uint32_t>> buckets(
              std::numeric_limits<std::uint16_t>::max() + 1ULL);
            for (std::uint32_t index = 0; index < stateCount_; ++index)
                if (nodes_[index].wdl == Wdl::Win || nodes_[index].wdl == Wdl::Loss)
                    buckets[nodes_[index].dtw].push_back(index);
            std::uint64_t propagated = 0;
            for (std::uint32_t distance = 0; distance < buckets.size(); ++distance)
              for (std::size_t queued = 0; queued < buckets[distance].size(); ++queued) {
                const std::uint32_t child = buckets[distance][queued];
                const Node childNode = nodes_[child];
                if (distance != childNode.dtw)
                    continue;
                if (++propagated % progressEvery == 0) {
                    const auto elapsed = std::chrono::duration<double>(
                      std::chrono::steady_clock::now() - start).count();
                    std::cout << "propagate queue " << propagated
                              << " elapsed " << elapsed << "s\n";
                }
                for (Offset edge = offsets[child]; edge < offsets[child + 1]; ++edge) {
                    const std::uint32_t packedParent = predecessors[edge];
                    const std::uint32_t parentIndex = packedParent & ~SameSideMask;
                    const bool sameSide = (packedParent & SameSideMask) != 0;
                    Node& parent = nodes_[parentIndex];
                    const Wdl outcome = parent_wdl(childNode.wdl, sameSide);
                    if (parent.wdl == Wdl::Win && outcome == Wdl::Win) {
                        const std::uint16_t distance = static_cast<std::uint16_t>(
                          std::min<int>(std::numeric_limits<std::uint16_t>::max(),
                                        childNode.dtw + 1));
                        if (distance < parent.dtw) {
                            parent.dtw = distance;
                            buckets[parent.dtw].push_back(parentIndex);
                        }
                        continue;
                    }
                    if (parent.wdl != Wdl::Unknown)
                        continue;
                    if (outcome == Wdl::Win) {
                        parent.wdl = Wdl::Win;
                        parent.dtw = static_cast<std::uint16_t>(std::min<int>(
                          std::numeric_limits<std::uint16_t>::max(), childNode.dtw + 1));
                        buckets[parent.dtw].push_back(parentIndex);
                    }
                    else if (outcome == Wdl::Loss) {
                        if (parent.remaining)
                            --parent.remaining;
                        parent.longestWinChild = std::max(parent.longestWinChild, childNode.dtw);
                        if (!parent.remaining) {
                            parent.wdl = Wdl::Loss;
                            parent.dtw = static_cast<std::uint16_t>(std::min<int>(
                              std::numeric_limits<std::uint16_t>::max(),
                              parent.longestWinChild + 1));
                            buckets[parent.dtw].push_back(parentIndex);
                        }
                    }
                }
              }
            for (std::uint32_t index = 0; index < stateCount_; ++index)
                if (nodes_[index].wdl == Wdl::Unknown)
                    nodes_[index].wdl = Wdl::Draw;
        };
        const auto solve = [&](auto offsetZero) {
            using Offset = decltype(offsetZero);
            if (diskBacked_ || stateCount_ >= 300'000'000) {
                MappedArray<Offset> offsets(checkpoint_ + ".offsets", stateCount_ + 1ULL);
                MappedArray<std::uint32_t> predecessors(
                  checkpoint_ + ".predecessors", edgeCount);
                solve_arrays(offsets, predecessors);
            }
            else {
                std::vector<Offset> offsets(stateCount_ + 1);
                std::vector<std::uint32_t> predecessors(edgeCount);
                solve_arrays(offsets, predecessors);
            }
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
    static void append_information_word(std::string& output, std::int32_t value) {
        const std::uint32_t word = static_cast<std::uint32_t>(value);
        for (unsigned shift = 0; shift < 32; shift += 8)
            output.push_back(static_cast<char>((word >> shift) & 0xff));
    }

    static std::int32_t primary_jester_public_type(const PieceState& piece) {
        if (piece.color == Color::White &&
            (piece.type == PieceType::King || piece.type == PieceType::Jester))
            return static_cast<std::int32_t>(PieceType::Count) + 1;
        return static_cast<std::int32_t>(piece.type);
    }

    // Collision-free compact projection specialized to the closed stateless
    // one-primary-Jester strata.  These classes have no hidden Ghost, links,
    // attachments, forced continuation, or en-passant state.  Avoiding the
    // general relationship canonicalizer and text formatting saves billions
    // of allocations during the large exact solves while retaining a complete
    // fixed-width public serialization.
    static std::string primary_jester_view_key(const Position& position) {
        using Record = std::array<std::int32_t, 13>;
        std::vector<Record> records;
        records.reserve(position.piece_count());
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive)
                continue;
            if (piece.link != Position::NoPiece || piece.host != Position::NoPiece)
                throw std::runtime_error(
                  "compact Jester projection encountered a relationship piece");
            records.push_back({
              primary_jester_public_type(piece),
              static_cast<std::int32_t>(piece.color),
              piece.square,
              piece.onBoard,
              piece.action,
              piece.cooldown,
              piece.freezeCount,
              piece.power,
              piece.moved,
              piece.visible,
              piece.attachmentOrder,
              piece.link,
              piece.host});
        }
        std::sort(records.begin(), records.end());

        std::string output;
        output.reserve((12 + records.size() * 13) * sizeof(std::uint32_t));
        append_information_word(output, 1);  // compact schema version
        append_information_word(output, static_cast<std::int32_t>(position.side_to_move()));
        append_information_word(output, static_cast<std::int32_t>(position.continuation()));
        const int forced = position.forced_piece();
        append_information_word(output, forced == Position::NoPiece ? -1
          : primary_jester_public_type(position.piece(forced)));
        append_information_word(output, forced == Position::NoPiece ? -1
          : position.piece(forced).square);
        append_information_word(output, position.en_passant_square());
        const int victim = position.en_passant_victim();
        append_information_word(output, victim == Position::NoPiece ? -1
          : primary_jester_public_type(position.piece(victim)));
        append_information_word(output, victim == Position::NoPiece ? -1
          : position.piece(victim).square);
        const auto timeout = position.forced_timeout_winner();
        append_information_word(output, timeout
          ? static_cast<std::int32_t>(*timeout) : -1);
        std::int32_t terminal = 0;
        if (position.game_over()) {
            const auto winner = position.winner();
            terminal = winner ? 2 + static_cast<std::int32_t>(*winner) : 1;
        }
        append_information_word(output, terminal);
        append_information_word(output, static_cast<std::int32_t>(records.size()));
        for (const Record& record : records)
            for (const std::int32_t field : record)
                append_information_word(output, field);
        return output;
    }

    // Fixed-width specialization of decision_observation_key() for the same
    // closed primary-Jester strata. The compact transition/root key already
    // carries the ordinary public view, so this suffix contains only the
    // mover-private rendered dot frontier. One dot is identified solely by
    // source/destination; auxiliary IDs, promotion choices, and internal kinds
    // sharing that dot remain intentionally indistinguishable.
    static std::string primary_jester_decision_markers(
      const Position& position) {
        if (position.side_to_move() != Color::Black)
            throw std::runtime_error(
              "primary-Jester private dot projection called for non-Onyx turn");
        std::vector<std::uint32_t> markers;
        for (const Move& move : position.legal_moves()) {
            const std::uint32_t marker = move.kind == MoveKind::Pass ? 0u
              : 1u + static_cast<std::uint32_t>(move.from) * SquareCount +
                  static_cast<std::uint32_t>(move.to);
            markers.push_back(marker);
        }
        std::sort(markers.begin(), markers.end());
        markers.erase(std::unique(markers.begin(), markers.end()), markers.end());
        std::string output;
        output.reserve((2 + markers.size()) * sizeof(std::uint32_t));
        append_information_word(output, 2);  // legal-dot schema version
        append_information_word(output, static_cast<std::int32_t>(markers.size()));
        for (const std::uint32_t marker : markers)
            append_information_word(output, static_cast<std::int32_t>(marker));
        return output;
    }

    static std::string primary_jester_transition_key(
      const Position& before, const Move& move, const Position& after) {
        std::string output;
        output.reserve(32 + 13 * 4 * 4);
        append_information_word(output, 1);  // compact transition schema
        const int actor = move.kind == MoveKind::Pass
                        ? Position::NoPiece : before.piece_on(move.from);
        append_information_word(output, actor == Position::NoPiece ? -1
          : primary_jester_public_type(before.piece(actor)));
        append_information_word(output, static_cast<std::int32_t>(move.kind));
        append_information_word(output, move.kind == MoveKind::Pass ? -1 : move.from);
        append_information_word(output, move.kind == MoveKind::Pass ? -1 : move.to);
        append_information_word(output, static_cast<std::int32_t>(move.promotion));
        output += primary_jester_view_key(after);
        // Public animation/result observations remain shared. If the result is
        // Black's decision boundary, append only Black's private exhaustive
        // legal-dot signature so its own successor belief is refined before
        // action selection. White never receives this private observation (and
        // already knows its own concrete royal identity in this stratum).
        if (!after.game_over() && after.side_to_move() == Color::Black) {
            const std::string decision =
              primary_jester_decision_markers(after);
            append_information_word(output,
              static_cast<std::int32_t>(decision.size()));
            output += decision;
        }
        else
            append_information_word(output, -1);
        return output;
    }

    [[nodiscard]] Color encoded_side(std::uint32_t index) const {
        if (compoundCopycat_)
            return decode_compound_copycat(index / substates_).side;
        if (fourModels_)
            return (identicalExtras_ ? decode_identical_four(index / substates_)
                                     : decode_four(index / substates_)).side;
        return decode(index).side;
    }

    std::uint32_t primary_jester_alternative(std::uint32_t index) const {
        if (attackerType_ != PieceType::Jester || identicalExtras_ ||
            compoundCopycat_ || copycatOnly_)
            throw std::runtime_error(
              "royal-assignment swap requires exactly one primary Jester");
        if (!fourModels_) {
            const State state = decode(index);
            return encode({state.side, state.attacker, state.blackKing,
                           state.whiteKing, state.substate});
        }
        const std::uint32_t combinedSubstate = index % substates_;
        FourState state = decode_four(index / substates_);
        std::swap(state.whiteKing, state.first);
        return encode_four(state) * substates_ + combinedSubstate;
    }

    bool make_primary_jester_world(std::uint32_t representative, bool swapped,
                                   Position& position) const {
        if (!fourModels_)
            return make_position_at(swapped
              ? primary_jester_alternative(representative) : representative,
              position);
        if (attackerType_ != PieceType::Jester || identicalExtras_ ||
            compoundCopycat_ || copycatOnly_)
            return false;
        const std::uint32_t combinedSubstate = representative % substates_;
        const std::uint32_t primarySubstate =
          combinedSubstate / secondarySubstates_;
        const std::uint32_t secondarySubstate =
          combinedSubstate % secondarySubstates_;
        FourState state = decode_four(representative / substates_);
        if (swapped)
            std::swap(state.whiteKing, state.first);

        position.clear();
        const int whiteKing = position.add_piece(
          PieceType::King, Color::White, state.whiteKing);
        const int blackKing = position.add_piece(
          PieceType::King, Color::Black, state.blackKing);
        const int jester = position.add_piece(
          PieceType::Jester, Color::White, state.first);
        const int secondary = position.add_piece(
          represented_type(secondaryType_, secondarySubstate),
          secondaryColor_, state.second);
        if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
            jester == Position::NoPiece || secondary == Position::NoPiece)
            return false;
        for (const int id : {whiteKing, blackKing, jester, secondary})
            position.piece(id).moved = true;
        if (!apply_substate(position, jester, attackerType_, primarySubstate) ||
            !apply_substate(position, secondary, secondaryType_, secondarySubstate))
            return false;
        position.set_side_to_move(state.side);
        return true;
    }

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
        case PieceType::Penguin: break;
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
        case PieceType::Penguin: return position.piece(id).action ? 1u : 0u;
        default: return 0;
        }
    }

    bool make_position_at(std::uint32_t index, Position& position) const {
        if (!fourModels_)
            return make_position(decode(index), position);
        if (copycatOnly_) {
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
        const FourState state = compoundCopycat_ ? decode_compound_copycat(placement)
          : identicalExtras_ ? decode_identical_four(placement) : decode_four(placement);
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
        if (compoundCopycat_) {
            const int clone = position.piece(first).link;
            if (clone == Position::NoPiece || !position.piece(clone).alive ||
                !position.piece(clone).onBoard)
                return false;
            position.piece(clone).moved = true;
        }
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
        if (compoundCopycat_) {
            const int clone = position.piece(2).link;
            return clone == 3 && position.piece(3).alive && position.piece(3).onBoard &&
                   position.piece(3).type == PieceType::CopycatClone &&
                   position.piece(3).color == position.piece(2).color &&
                   position.piece(3).link == 2 &&
                   position.piece(3).square ==
                     horizontal_reflection(position.piece(2).square) &&
                   position.piece(4).alive && position.piece(4).onBoard &&
                   position.piece(4).link == Position::NoPiece &&
                   type_matches(secondaryType_, position.piece(4).type);
        }
        return position.piece(3).alive && position.piece(3).onBoard &&
               type_matches(secondaryType_, position.piece(3).type);
    }

    std::uint32_t child_index(const Position& position) const {
        if (copycatOnly_)
            return encode_placement({position.side_to_move(), position.piece(0).square,
                                     position.piece(1).square, position.piece(2).square});
        if (compoundCopycat_) {
            const FourState state{position.side_to_move(), position.piece(0).square,
                                  position.piece(1).square, position.piece(2).square,
                                  position.piece(4).square};
            return encode_compound_copycat(state) * substates_;
        }
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
            substate = position.piece(2).action ? 1 : 0;
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
                    const bool sameSide = child.side_to_move() == position.side_to_move();
                    const Wdl outcome = external
                      ? parent_wdl(external->wdl, sameSide) : Wdl::Unknown;
                    if (outcome == Wdl::Win) {
                        nodes_[index].wdl = Wdl::Win;
                        const std::uint16_t distance = static_cast<std::uint16_t>(external->dtw + 1);
                        nodes_[index].dtw = nodes_[index].dtw
                          ? std::min(nodes_[index].dtw, distance) : distance;
                    }
                    else if (outcome == Wdl::Loss) {
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
            if (successor >= stateCount_)
                throw std::runtime_error("child index exceeds tablebase domain at parent " +
                                         std::to_string(index));
            consume(successor, child.side_to_move() == position.side_to_move());
        }
    }

    void save_checkpoint(std::uint32_t processed) const {
        const std::string temporary = checkpoint_ + ".tmp";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("cannot write tablebase checkpoint");
        const std::array<char, 8> magic{{'U','F','T','B','C','P','3','\0'}};
        const std::uint32_t piece = static_cast<std::uint32_t>(attackerType_);
        stream.write(magic.data(), magic.size());
        stream.write(reinterpret_cast<const char*>(&piece), sizeof(piece));
        stream.write(reinterpret_cast<const char*>(&processed), sizeof(processed));
        stream.write(reinterpret_cast<const char*>(nodes_),
                     std::uint64_t(stateCount_) * sizeof(Node));
        stream.write(reinterpret_cast<const char*>(predecessorCounts_),
                     std::uint64_t(stateCount_) * sizeof(std::uint32_t));
        stream.close();
        if (std::rename(temporary.c_str(), checkpoint_.c_str()) != 0)
            throw std::runtime_error("cannot install tablebase checkpoint");
    }

    std::uint32_t load_checkpoint() {
        if (!checkpointEvery_)
            return 0;
        std::ifstream stream(checkpoint_, std::ios::binary);
        if (!stream)
            return 0;
        std::array<char, 8> magic{};
        std::uint32_t piece = 0, processed = 0;
        stream.read(magic.data(), magic.size());
        stream.read(reinterpret_cast<char*>(&piece), sizeof(piece));
        stream.read(reinterpret_cast<char*>(&processed), sizeof(processed));
        const std::array<char, 8> expected{{'U','F','T','B','C','P','3','\0'}};
        if (magic != expected || piece != static_cast<std::uint32_t>(attackerType_) ||
            processed > stateCount_)
            throw std::runtime_error("invalid tablebase checkpoint");
        stream.read(reinterpret_cast<char*>(nodes_),
                    std::uint64_t(stateCount_) * sizeof(Node));
        stream.read(reinterpret_cast<char*>(predecessorCounts_),
                    std::uint64_t(stateCount_) * sizeof(std::uint32_t));
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
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            const std::uint8_t distance = static_cast<std::uint8_t>(
              std::min<int>(nodes_[index].dtw, 255));
            stream.write(reinterpret_cast<const char*>(&distance), sizeof(distance));
        }
        for (const auto [index, distance] : exceptions) {
            stream.write(reinterpret_cast<const char*>(&index), sizeof(index));
            stream.write(reinterpret_cast<const char*>(&distance), sizeof(distance));
        }
        std::array<std::uint64_t, 4> totals{};
        for (std::uint32_t index = 0; index < stateCount_; ++index)
            ++totals[static_cast<std::size_t>(nodes_[index].wdl)];
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
                    const bool sameSide = child.side_to_move() == position.side_to_move();
                    const Wdl outcome = external
                      ? parent_wdl(external->wdl, sameSide) : Wdl::Unknown;
                    if (outcome == Wdl::Win) {
                        hasLoss = true;
                        shortestLoss = std::min(shortestLoss, external->dtw);
                        allWin = false;
                    }
                    else if (outcome == Wdl::Loss)
                        longestWin = std::max(longestWin, external->dtw);
                    else {
                        hasDraw = true;
                        allWin = false;
                    }
                    continue;
                }
                const Node successor = nodes_[child_index(child)];
                const Wdl outcome = parent_wdl(
                  successor.wdl, child.side_to_move() == position.side_to_move());
                if (outcome == Wdl::Win) {
                    hasLoss = true;
                    shortestLoss = std::min(shortestLoss, successor.dtw);
                    allWin = false;
                }
                else if (outcome == Wdl::Draw) {
                    hasDraw = true;
                    allWin = false;
                }
                else if (outcome == Wdl::Loss)
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
    bool diskBacked_;
    bool copycatOnly_;
    bool compoundCopycat_;
    PieceType secondaryType_;
    Color secondaryColor_;
    bool fourModels_;
    bool identicalExtras_;
    std::uint32_t primarySubstates_;
    std::uint32_t secondarySubstates_;
    std::uint32_t substates_;
    std::uint32_t stateCount_;
    std::vector<Node> nodeStorage_;
    std::vector<std::uint32_t> predecessorCountStorage_;
    std::unique_ptr<MappedArray<Node>> mappedNodes_;
    std::unique_ptr<MappedArray<std::uint32_t>> mappedPredecessorCounts_;
    Node* nodes_ = nullptr;
    std::uint32_t* predecessorCounts_ = nullptr;
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
    bool diskBacked = false;
    std::uint32_t dryRun = 0;
    std::uint32_t dryRunBegin = 0;
    std::uint32_t inspect = std::numeric_limits<std::uint32_t>::max();
    std::string auditPredecessorSafety;
    std::string auditReachability;
    std::string solveJesterInformation;
    std::string informationOverlay;
    std::string lowerInformationOverlay;
    std::string lowerInformationSourceSha256;
    std::string informationSourceSha256;
    std::string informationModelSha256;
    std::string informationScratch = "/tmp";
    bool referenceInformationSolver = false;
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
        else if (argument == "--disk-backed") diskBacked = true;
        else if (argument == "--checkpoint-every")
            checkpointEvery = static_cast<std::uint32_t>(std::stoul(value("--checkpoint-every")));
        else if (argument == "--dry-run")
            dryRun = static_cast<std::uint32_t>(std::stoul(value("--dry-run")));
        else if (argument == "--dry-run-begin")
            dryRunBegin = static_cast<std::uint32_t>(std::stoul(value("--dry-run-begin")));
        else if (argument == "--inspect")
            inspect = static_cast<std::uint32_t>(std::stoul(value("--inspect")));
        else if (argument == "--audit-predecessor-safety")
            auditPredecessorSafety = value("--audit-predecessor-safety");
        else if (argument == "--audit-reachability")
            auditReachability = value("--audit-reachability");
        else if (argument == "--solve-jester-information")
            solveJesterInformation = value("--solve-jester-information");
        else if (argument == "--information-overlay")
            informationOverlay = value("--information-overlay");
        else if (argument == "--lower-information-overlay")
            lowerInformationOverlay = value("--lower-information-overlay");
        else if (argument == "--lower-information-source-sha256")
            lowerInformationSourceSha256 = value("--lower-information-source-sha256");
        else if (argument == "--information-source-sha256")
            informationSourceSha256 = value("--information-source-sha256");
        else if (argument == "--information-model-sha256")
            informationModelSha256 = value("--information-model-sha256");
        else if (argument == "--information-scratch")
            informationScratch = value("--information-scratch");
        else if (argument == "--reference-information-solver")
            referenceInformationSolver = true;
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
        else {
            const bool linkedCopycatBishop =
              attackerType == PieceType::Copycat &&
              secondaryType == PieceType::Bishop && secondaryColor == Color::Black;
            if ((!closed_four_piece(attackerType) ||
                 !closed_four_piece(secondaryType)) && !linkedCopycatBishop)
                throw std::runtime_error("K+K+2 piece requires a larger non-closed model");
        }
        TablebaseGenerator generator(attackerType, secondaryType, secondaryColor,
                                     output, checkpoint, checkpointEvery, diskBacked);
        if (selfTest)
            generator.self_test();
        if (dryRun)
            generator.dry_run(dryRunBegin, dryRun);
        if (inspect != std::numeric_limits<std::uint32_t>::max())
            generator.inspect(inspect);
        if (!auditPredecessorSafety.empty())
            generator.audit_reachability(auditPredecessorSafety, false);
        if (!auditReachability.empty())
            generator.audit_reachability(auditReachability, true);
        if (!solveJesterInformation.empty()) {
            if (referenceInformationSolver)
                generator.solve_jester_information_reference(
                  solveJesterInformation, informationOverlay);
            else
                generator.solve_jester_information(
                  solveJesterInformation, lowerInformationOverlay,
                  lowerInformationSourceSha256, informationSourceSha256,
                  informationModelSha256, informationOverlay,
                  informationScratch);
        }
        if (!selfTest && !dryRun && inspect == std::numeric_limits<std::uint32_t>::max() &&
            auditPredecessorSafety.empty() && auditReachability.empty() &&
            solveJesterInformation.empty())
            generator.generate();
    }
    catch (const std::exception& error) {
        std::cerr << "tablebase error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
