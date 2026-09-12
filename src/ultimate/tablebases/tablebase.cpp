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
#include <cerrno>
#include <chrono>
#include <cstdlib>
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
#include <mutex>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_set>
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
constexpr std::uint32_t IdenticalCompoundCopycatStateCount =
  CompoundCopycatStateCount / 2;
constexpr std::uint64_t GiantAnchorV2Tag = 0x32474e4149474655ULL;
constexpr std::uint64_t TrackedGhostV1Tag = 0x3154534f48474655ULL;
constexpr std::uint64_t AngelGraphV1Tag = 0x314c45474e414655ULL;
constexpr std::uint64_t AngelGiantGraphV1Tag = 0x314741474e414655ULL;
constexpr std::uint64_t LinkedCopycatPairV1Tag = 0x314b4e4c43434655ULL;
constexpr std::uint64_t AngelCopycatGraphV1Tag = 0x3152504343414655ULL;
constexpr std::uint64_t SpawnedDevilRootV1Tag = 0x315256444e505355ULL;

std::size_t packed_header_size(std::uint32_t version) {
    return 40 + (version >= 5 ? 8 : 0) + (version >= 6 ? 8 : 0) +
           (version >= 7 ? 8 : 0);
}

std::uint8_t horizontal_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>((square / 8) * 8 + 7 - square % 8);
}

// A Giant stores the lower-left anchor of a 2x2 footprint. Reflecting the
// anchor as though it were an ordinary one-square piece shifts the mirrored
// footprint one file to the right (g1 would become b1 instead of a1). File h
// is not a legal Giant anchor, but keeping it fixed makes this a total
// involution over the dense codec's deliberately retained invalid records.
std::uint8_t horizontal_giant_anchor_reflection(std::uint8_t square) {
    const int file = square % 8;
    return file == 7 ? square
                     : static_cast<std::uint8_t>((square / 8) * 8 + 6 - file);
}

FourState canonicalize(FourState state, bool firstGiant = false,
                        bool secondGiant = false) {
    if (state.whiteKing % 8 >= 4) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.first = firstGiant ? horizontal_giant_anchor_reflection(state.first)
                                 : horizontal_reflection(state.first);
        state.second = secondGiant ? horizontal_giant_anchor_reflection(state.second)
                                   : horizontal_reflection(state.second);
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

std::uint32_t encode_four(FourState state, bool firstGiant = false,
                          bool secondGiant = false) {
    state = canonicalize(state, firstGiant, secondGiant);
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

std::uint32_t encode_identical_compound_copycat(FourState state) {
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    std::uint32_t firstRank = rank_excluding(
      state.first, {state.whiteKing, state.blackKing});
    std::uint32_t secondRank = rank_excluding(
      state.second, {state.whiteKing, state.blackKing});
    if (firstRank > secondRank)
        std::swap(firstRank, secondRank);
    constexpr std::uint32_t remaining = SquareCount - 2;
    constexpr std::uint32_t pairs = remaining * (remaining - 1) / 2;
    const std::uint32_t pairRank =
      firstRank * (2 * remaining - firstRank - 1) / 2 +
      secondRank - firstRank - 1;
    return ((static_cast<std::uint32_t>(state.side) * SquareCount +
             state.whiteKing) * (SquareCount - 1) + blackRank) * pairs + pairRank;
}

FourState decode_identical_compound_copycat(std::uint32_t index) {
    constexpr std::uint32_t remaining = SquareCount - 2;
    constexpr std::uint32_t pairs = remaining * (remaining - 1) / 2;
    const std::uint32_t pairRank = index % pairs;
    index /= pairs;
    const std::uint32_t blackRank = index % (SquareCount - 1);
    index /= SquareCount - 1;
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(index % SquareCount);
    const Color side = static_cast<Color>(index / SquareCount);
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

std::uint32_t encode_identical_four(FourState state, bool firstGiant = false,
                                    bool secondGiant = false) {
    state = canonicalize(state, firstGiant, secondGiant);
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
    for (std::uint32_t index = 0;
         index < IdenticalCompoundCopycatStateCount; ++index) {
        const FourState state = decode_identical_compound_copycat(index);
        FourState swapped = state;
        std::swap(swapped.first, swapped.second);
        if (state.whiteKing == state.blackKing ||
            state.whiteKing == state.first || state.whiteKing == state.second ||
            state.blackKing == state.first || state.blackKing == state.second ||
            state.first == state.second ||
            encode_identical_compound_copycat(state) != index ||
            encode_identical_compound_copycat(swapped) != index)
            throw std::runtime_error(
              "identical compound Copycat codec is not bijective");
    }
    std::cout << "fourcodecok states " << FourPlacementStateCount << '\n';
    std::cout << "identicalfourcodecok states " << IdenticalFourStateCount << '\n';
    std::cout << "compoundcopycatcodecok states " << CompoundCopycatStateCount << '\n';
    std::cout << "identicalcompoundcopycatcodecok states "
              << IdenticalCompoundCopycatStateCount << '\n';
}

struct Node {
    Wdl wdl = Wdl::Unknown;
    std::uint16_t dtw = 0;
    std::uint16_t remaining = 0;
    std::uint16_t longestWinChild = 0;
};

template<typename Edge>
constexpr Edge predecessor_same_side_mask() {
    static_assert(std::is_unsigned_v<Edge>);
    return Edge{1} << (std::numeric_limits<Edge>::digits - 1);
}

template<typename Edge>
constexpr Edge pack_predecessor(std::uint32_t index, bool sameSide) {
    const Edge mask = predecessor_same_side_mask<Edge>();
    return static_cast<Edge>(index) | (sameSide ? mask : Edge{0});
}

template<typename Edge>
constexpr std::uint32_t predecessor_index(Edge packed) {
    return static_cast<std::uint32_t>(
      packed & ~predecessor_same_side_mask<Edge>());
}

template<typename Edge>
constexpr bool predecessor_same_side(Edge packed) {
    return (packed & predecessor_same_side_mask<Edge>()) != 0;
}

static_assert(predecessor_index(
                pack_predecessor<std::uint64_t>(3'795'791'999U, false)) ==
              3'795'791'999U);
static_assert(predecessor_index(
                pack_predecessor<std::uint64_t>(3'795'791'999U, true)) ==
              3'795'791'999U);
static_assert(predecessor_same_side(
                pack_predecessor<std::uint64_t>(3'795'791'999U, true)));
static_assert(!predecessor_same_side(
                pack_predecessor<std::uint64_t>(3'795'791'999U, false)));

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
        // mmap(2) rejects a zero-length mapping.  A closed all-draw class can
        // legitimately have no in-class reverse edges, so its predecessor
        // array is empty even though the state and offset planes are not.
        if (bytes_) {
            void* mapping = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, fd_, 0);
            if (mapping == MAP_FAILED) {
                ::close(fd_);
                fd_ = -1;
                throw std::runtime_error("cannot map tablebase scratch file");
            }
            data_ = static_cast<T*>(mapping);
        }
        // The ordinary local generator keeps its historical kill-safe cleanup
        // behavior. Audited AWS preservation runs opt in to named scratch so
        // resource-limit stops and successful proofs retain every byte.
        const char* preserve = std::getenv("ULTIMATE_TABLEBASE_PRESERVE_SCRATCH");
        if (!preserve || std::strcmp(preserve, "1") != 0)
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

// A named, restartable mapping used by the sparse Devil closure.  MappedArray
// intentionally truncates ordinary scratch on every construction; the Devil
// index instead commits complete BFS layers and reopens those exact bytes.
template<typename T>
class PersistentMappedArray {
   public:
    PersistentMappedArray(const std::string& path, std::uint64_t count,
                          bool create) : path_(path), count_(count) {
        if (!count || count > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::runtime_error("invalid persistent Devil array extent");
        bytes_ = static_cast<std::size_t>(count) * sizeof(T);
        fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | (create ? O_TRUNC : 0), 0600);
        if (fd_ == -1)
            throw std::runtime_error("cannot open persistent Devil array " + path);
        struct stat status {};
        if (::fstat(fd_, &status) != 0 ||
            (!create && static_cast<std::uint64_t>(status.st_size) > bytes_))
            throw std::runtime_error("persistent Devil array extent residual " + path);
        if ((create || static_cast<std::uint64_t>(status.st_size) < bytes_) &&
            ::ftruncate(fd_, static_cast<off_t>(bytes_)) != 0)
            throw std::runtime_error("cannot size persistent Devil array " + path);
        void* mapping = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd_, 0);
        if (mapping == MAP_FAILED)
            throw std::runtime_error("cannot map persistent Devil array " + path);
        data_ = static_cast<T*>(mapping);
    }
    PersistentMappedArray(const PersistentMappedArray&) = delete;
    PersistentMappedArray& operator=(const PersistentMappedArray&) = delete;
    ~PersistentMappedArray() {
        if (data_)
            ::munmap(data_, bytes_);
        if (fd_ != -1)
            ::close(fd_);
    }
    T& operator[](std::uint64_t index) { return data_[index]; }
    const T& operator[](std::uint64_t index) const { return data_[index]; }
    T* data() { return data_; }
    std::uint64_t count() const { return count_; }
    void advise_sequential() const {
        advise(MADV_SEQUENTIAL, "sequential");
    }
    void advise_random() const {
        advise(MADV_RANDOM, "random");
    }
    void sync_prefix(std::uint64_t count) const {
        if (count > count_)
            throw std::runtime_error("persistent Devil sync extent residual " + path_);
        const std::size_t bytes = static_cast<std::size_t>(count) * sizeof(T);
        if ((bytes && ::msync(data_, bytes, MS_SYNC) != 0) || ::fsync(fd_) != 0)
            throw std::runtime_error("cannot sync persistent Devil array " + path_);
    }
   private:
    void advise(int mappingAdvice, const char* description) const {
        if (::madvise(data_, bytes_, mappingAdvice) != 0)
            throw std::runtime_error(
              "cannot set " + std::string(description) +
              " persistent Devil mapping advice " + path_ + ": " +
              std::strerror(errno));
#if defined(__linux__)
        const int fileAdvice = mappingAdvice == MADV_RANDOM
          ? POSIX_FADV_RANDOM : POSIX_FADV_SEQUENTIAL;
        const int error = ::posix_fadvise(fd_, 0, 0, fileAdvice);
        if (error != 0)
            throw std::runtime_error(
              "cannot set " + std::string(description) +
              " persistent Devil file advice " + path_ + ": " +
              std::strerror(error));
#endif
    }
    std::string path_;
    int fd_ = -1;
    std::uint64_t count_ = 0;
    std::size_t bytes_ = 0;
    T* data_ = nullptr;
};

// Zero-filled, process-lifetime scratch.  The Devil hash slots are rebuilt
// exactly from the committed key prefix after a restart and therefore must not
// generate tens of gigabytes of dirty filesystem writeback.
template<typename T>
class VolatileMappedArray {
   public:
    explicit VolatileMappedArray(std::uint64_t count) : count_(count) {
        if (!count || count > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::runtime_error("invalid volatile Devil array extent");
        bytes_ = static_cast<std::size_t>(count) * sizeof(T);
        void* mapping = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED)
            throw std::runtime_error("cannot map volatile Devil array");
        data_ = static_cast<T*>(mapping);
    }
    VolatileMappedArray(const VolatileMappedArray&) = delete;
    VolatileMappedArray& operator=(const VolatileMappedArray&) = delete;
    ~VolatileMappedArray() {
        if (data_)
            ::munmap(data_, bytes_);
    }
    T& operator[](std::uint64_t index) { return data_[index]; }
    const T& operator[](std::uint64_t index) const { return data_[index]; }
    std::uint64_t count() const { return count_; }

   private:
    std::uint64_t count_ = 0;
    std::size_t bytes_ = 0;
    T* data_ = nullptr;
};

// Read-only, restartable proof input.  A retained Devil frontier can contain
// billions of dense indexes; copying it into an anonymous vector needlessly
// duplicates the durable file and can push the disposable hash over its
// cgroup limit.  Keep the first resumed layer file-backed so clean pages are
// reclaimable, then return to ordinary vectors for newly generated layers.
template<typename T>
class ReadOnlyMappedArray {
   public:
    ReadOnlyMappedArray(const std::string& path, std::uint64_t count)
      : path_(path), count_(count) {
        if (!count || count > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::runtime_error("invalid read-only Devil array extent");
        bytes_ = static_cast<std::size_t>(count) * sizeof(T);
        fd_ = ::open(path.c_str(), O_RDONLY);
        struct stat status {};
        if (fd_ == -1)
            throw std::runtime_error("cannot open read-only Devil array " + path_);
        if (::fstat(fd_, &status) != 0 ||
            static_cast<std::uint64_t>(status.st_size) != bytes_) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("read-only Devil array extent residual " + path_);
        }
        void* mapping = ::mmap(nullptr, bytes_, PROT_READ, MAP_SHARED, fd_, 0);
        if (mapping == MAP_FAILED) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("cannot map read-only Devil array " + path_);
        }
        data_ = static_cast<const T*>(mapping);
        if (::madvise(const_cast<T*>(data_), bytes_, MADV_SEQUENTIAL) != 0) {
            ::munmap(const_cast<T*>(data_), bytes_);
            data_ = nullptr;
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("cannot advise read-only Devil array " + path_);
        }
#if defined(__linux__)
        const int error = ::posix_fadvise(fd_, 0, 0, POSIX_FADV_SEQUENTIAL);
        if (error != 0) {
            ::munmap(const_cast<T*>(data_), bytes_);
            data_ = nullptr;
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("cannot advise read-only Devil file " + path_);
        }
#endif
    }
    ReadOnlyMappedArray(const ReadOnlyMappedArray&) = delete;
    ReadOnlyMappedArray& operator=(const ReadOnlyMappedArray&) = delete;
    ~ReadOnlyMappedArray() {
        if (data_)
            ::munmap(const_cast<T*>(data_), bytes_);
        if (fd_ != -1)
            ::close(fd_);
    }
    const T& operator[](std::uint64_t index) const { return data_[index]; }
    std::uint64_t count() const { return count_; }

   private:
    std::string path_;
    int fd_ = -1;
    std::uint64_t count_ = 0;
    std::size_t bytes_ = 0;
    const T* data_ = nullptr;
};

struct DevilReverseRecord {
    std::uint64_t child = 0;
    std::uint64_t packedParent = 0;
};

static_assert(sizeof(DevilReverseRecord) == 16);

constexpr std::uint64_t DevilReverseSameSide = std::uint64_t{1} << 63;
constexpr std::uint64_t DevilReverseHashOffset = 1469598103934665603ULL;
constexpr std::uint64_t DevilReverseHashPrime = 1099511628211ULL;
constexpr std::uint32_t DevilReverseBucketCount = 16;

std::uint64_t devil_reverse_hash(std::uint64_t hash,
                                 const DevilReverseRecord& record) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&record);
    for (std::size_t index = 0; index < sizeof(record); ++index) {
        hash ^= bytes[index];
        hash *= DevilReverseHashPrime;
    }
    return hash;
}

void sync_file(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1)
        throw std::runtime_error("cannot open Devil reverse checkpoint " + path);
    const bool failed = ::fsync(fd) != 0;
    ::close(fd);
    if (failed)
        throw std::runtime_error("cannot sync Devil reverse checkpoint " + path);
}

void sync_directory(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1)
        throw std::runtime_error("cannot open Devil reverse checkpoint directory " + path);
    const bool failed = ::fsync(fd) != 0;
    ::close(fd);
    if (failed)
        throw std::runtime_error("cannot sync Devil reverse checkpoint directory " + path);
}

struct SpawnedDevilCheckpoint {
    std::uint32_t version = 0;
    std::uint32_t square = 0;
    std::uint64_t limit = 0;
    std::uint64_t size = 0;
    std::uint64_t frontier = 0;
    std::uint32_t ply = 0;
    bool operator==(const SpawnedDevilCheckpoint& other) const {
        return version == other.version && square == other.square &&
               limit == other.limit && size == other.size &&
               frontier == other.frontier && ply == other.ply;
    }
};

SpawnedDevilCheckpoint read_spawned_devil_checkpoint(
  const std::string& path, std::uint32_t expectedVersion) {
    std::ifstream stream(path, std::ios::binary);
    std::array<char, 8> magic{};
    SpawnedDevilCheckpoint value;
    stream.read(magic.data(), magic.size());
    stream.read(reinterpret_cast<char*>(&value.version), sizeof(value.version));
    stream.read(reinterpret_cast<char*>(&value.square), sizeof(value.square));
    stream.read(reinterpret_cast<char*>(&value.limit), sizeof(value.limit));
    stream.read(reinterpret_cast<char*>(&value.size), sizeof(value.size));
    stream.read(reinterpret_cast<char*>(&value.frontier), sizeof(value.frontier));
    stream.read(reinterpret_cast<char*>(&value.ply), sizeof(value.ply));
    const std::array<char, 8> expected{{'U','F','D','V','C','P','1','\0'}};
    if (!stream || stream.peek() != std::ifstream::traits_type::eof() ||
        magic != expected || (expectedVersion && value.version != expectedVersion) ||
        !value.limit ||
        value.size > value.limit || value.frontier > value.size ||
        value.square >= Position::BoardSquares)
        throw std::runtime_error("spawned-only Devil migration metadata residual " +
                                 path);
    return value;
}

void write_spawned_devil_checkpoint(const std::string& path,
                                    const SpawnedDevilCheckpoint& value) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    const std::array<char, 8> magic{{'U','F','D','V','C','P','1','\0'}};
    stream.write(magic.data(), magic.size());
    stream.write(reinterpret_cast<const char*>(&value.version),
                 sizeof(value.version));
    stream.write(reinterpret_cast<const char*>(&value.square),
                 sizeof(value.square));
    stream.write(reinterpret_cast<const char*>(&value.limit), sizeof(value.limit));
    stream.write(reinterpret_cast<const char*>(&value.size), sizeof(value.size));
    stream.write(reinterpret_cast<const char*>(&value.frontier),
                 sizeof(value.frontier));
    stream.write(reinterpret_cast<const char*>(&value.ply), sizeof(value.ply));
    if (!stream)
        throw std::runtime_error("cannot write spawned-only Devil migration metadata");
}

void read_exact_at(int fd, void* data, std::size_t bytes, std::uint64_t offset,
                   const std::string& label) {
    auto* cursor = static_cast<std::uint8_t*>(data);
    while (bytes) {
        const ssize_t count = ::pread(fd, cursor, bytes, static_cast<off_t>(offset));
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            throw std::runtime_error("short spawned-only Devil migration read " + label);
        cursor += count;
        bytes -= static_cast<std::size_t>(count);
        offset += static_cast<std::uint64_t>(count);
    }
}

void write_exact_at(int fd, const void* data, std::size_t bytes,
                    std::uint64_t offset, const std::string& label) {
    const auto* cursor = static_cast<const std::uint8_t*>(data);
    while (bytes) {
        const ssize_t count = ::pwrite(fd, cursor, bytes, static_cast<off_t>(offset));
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            throw std::runtime_error("short spawned-only Devil migration write " + label);
        cursor += count;
        bytes -= static_cast<std::size_t>(count);
        offset += static_cast<std::uint64_t>(count);
    }
}

constexpr std::uint64_t choose_devil(unsigned n, unsigned k) {
    if (k > n)
        return 0;
    if (k > n - k)
        k = n - k;
    std::uint64_t value = 1;
    for (unsigned item = 1; item <= k; ++item)
        value = value * (n - k + item) / item;
    return value;
}

constexpr std::uint64_t DevilMinionCodeCount =
  choose_devil(80, 0) + choose_devil(80, 1) + choose_devil(80, 2) +
  choose_devil(80, 3) + choose_devil(80, 4) + choose_devil(80, 5);
static_assert(DevilMinionCodeCount < (std::uint64_t{1} << 25));

std::uint64_t encode_devil_minions(std::uint64_t low, std::uint16_t high) {
    const unsigned count = static_cast<unsigned>(__builtin_popcountll(low) +
                                                  __builtin_popcount(high));
    if (count > 5)
        throw std::runtime_error("spawned-only Devil exceeds five Minions");
    std::uint64_t rank = 0;
    for (unsigned smaller = 0; smaller < count; ++smaller)
        rank += choose_devil(80, smaller);
    unsigned ordinal = 1;
    for (unsigned square = 0; square < 80; ++square) {
        const bool set = square < 64 ? ((low >> square) & 1ULL)
                                     : ((high >> (square - 64)) & 1U);
        if (set)
            rank += choose_devil(square, ordinal++);
    }
    if (rank >= DevilMinionCodeCount)
        throw std::runtime_error("spawned-only Devil Minion rank residual");
    return rank;
}

std::pair<std::uint64_t, std::uint16_t> decode_devil_minions(
  std::uint64_t code) {
    unsigned count = 0;
    std::uint64_t offset = 0;
    for (; count <= 5; ++count) {
        const std::uint64_t next = offset + choose_devil(80, count);
        if (code < next)
            break;
        offset = next;
    }
    if (count > 5 || code >= DevilMinionCodeCount)
        throw std::runtime_error("spawned-only Devil Minion code residual");
    std::uint64_t rank = code - offset;
    std::uint64_t low = 0;
    std::uint16_t high = 0;
    unsigned maximum = 79;
    for (unsigned ordinal = count; ordinal; --ordinal) {
        while (choose_devil(maximum, ordinal) > rank) {
            if (!maximum)
                throw std::runtime_error("spawned-only Devil Minion unrank residual");
            --maximum;
        }
        if (maximum < 64)
            low |= std::uint64_t{1} << maximum;
        else
            high |= static_cast<std::uint16_t>(1U << (maximum - 64));
        rank -= choose_devil(maximum, ordinal);
        if (maximum)
            --maximum;
    }
    if (rank)
        throw std::runtime_error("spawned-only Devil Minion rank remainder");
    return {low, high};
}

constexpr int LegacySideShift = 16;
constexpr int LegacyWhiteKingShift = 17;
constexpr int LegacyBlackKingShift = 24;
constexpr int LegacyDevilSquareShift = 31;
constexpr int LegacyDevilCooldownShift = 38;
constexpr int LegacySecondaryShift = 40;
constexpr std::uint64_t LegacySquareMask = 0x7fULL;
constexpr std::uint64_t DevilNoSquare = Position::BoardSquares;

std::uint64_t pack_compact_devil_key(std::uint64_t low, std::uint64_t high,
                                     unsigned fixedSquare) {
    if (high >> 47 || fixedSquare >= Position::BoardSquares)
        throw std::runtime_error("spawned-only Devil legacy high-key residual");
    const unsigned whiteKing = static_cast<unsigned>(
      (high >> LegacyWhiteKingShift) & LegacySquareMask);
    const unsigned blackKing = static_cast<unsigned>(
      (high >> LegacyBlackKingShift) & LegacySquareMask);
    const unsigned devilSquare = static_cast<unsigned>(
      (high >> LegacyDevilSquareShift) & LegacySquareMask);
    const unsigned secondary = static_cast<unsigned>(
      (high >> LegacySecondaryShift) & LegacySquareMask);
    const unsigned cooldown = static_cast<unsigned>(
      (high >> LegacyDevilCooldownShift) & 3ULL);
    if (whiteKing >= 80 || blackKing >= 80 || whiteKing == blackKing ||
        secondary > DevilNoSquare ||
        (devilSquare != DevilNoSquare && devilSquare != fixedSquare) ||
        (devilSquare == DevilNoSquare && cooldown))
        throw std::runtime_error("spawned-only Devil compact field residual");
    const std::uint64_t minions = encode_devil_minions(
      low, static_cast<std::uint16_t>(high & 0xffffULL));
    const unsigned blackIndex = blackKing < whiteKing ? blackKing : blackKing - 1;
    const std::uint64_t kings = whiteKing * 79ULL + blackIndex;
    const std::uint64_t side = (high >> LegacySideShift) & 1ULL;
    const std::uint64_t alive = devilSquare != DevilNoSquare;
    const std::uint64_t value = minions | (kings << 25) |
      (static_cast<std::uint64_t>(secondary) << 38) |
      (static_cast<std::uint64_t>(cooldown) << 45) | (side << 47) |
      (alive << 48);
    if (value >> 49)
        throw std::runtime_error("spawned-only Devil compact key overflow");
    return value;
}

std::pair<std::uint64_t, std::uint64_t> unpack_compact_devil_key(
  std::uint64_t value, unsigned fixedSquare) {
    if (value >> 49 || fixedSquare >= Position::BoardSquares)
        throw std::runtime_error("spawned-only Devil compact key residual");
    auto [low, minionHigh] = decode_devil_minions(
      value & ((std::uint64_t{1} << 25) - 1));
    const unsigned kings = static_cast<unsigned>((value >> 25) & 0x1fffULL);
    const unsigned whiteKing = kings / 79;
    const unsigned blackIndex = kings % 79;
    const unsigned blackKing = blackIndex >= whiteKing
      ? blackIndex + 1 : blackIndex;
    const unsigned secondary = static_cast<unsigned>((value >> 38) & 0x7fULL);
    const unsigned cooldown = static_cast<unsigned>((value >> 45) & 3ULL);
    const unsigned side = static_cast<unsigned>((value >> 47) & 1ULL);
    const bool alive = ((value >> 48) & 1ULL) != 0;
    if (whiteKing >= 80 || blackKing >= 80 || secondary > DevilNoSquare ||
        (!alive && cooldown))
        throw std::runtime_error("spawned-only Devil compact decode residual");
    std::uint64_t high = minionHigh;
    high |= static_cast<std::uint64_t>(side) << LegacySideShift;
    high |= static_cast<std::uint64_t>(whiteKing) << LegacyWhiteKingShift;
    high |= static_cast<std::uint64_t>(blackKing) << LegacyBlackKingShift;
    high |= static_cast<std::uint64_t>(alive ? fixedSquare : DevilNoSquare)
            << LegacyDevilSquareShift;
    high |= static_cast<std::uint64_t>(cooldown) << LegacyDevilCooldownShift;
    high |= static_cast<std::uint64_t>(secondary) << LegacySecondaryShift;
    return {low, high};
}

std::uint64_t migration_hash(std::uint64_t hash, const std::uint8_t* data,
                             std::size_t bytes) {
    for (std::size_t index = 0; index < bytes; ++index) {
        hash ^= data[index];
        hash *= DevilReverseHashPrime;
    }
    return hash;
}

// Convert a committed v1/v2/v3 proof-key prefix to the v4 seven-byte layout in a
// separate destination.  The source stays live and untouched: committed key
// prefixes are append-only, and opening the atomically replaced frontier before
// rereading identical metadata binds that exact generation.  The v4 metadata
// is installed last and acts as the destination commit marker.
void migrate_spawned_devil_checkpoint(const std::string& sourcePrefix,
                                      const std::string& destinationPrefix) {
    if (sourcePrefix.empty() || destinationPrefix.empty() ||
        sourcePrefix == destinationPrefix)
        throw std::runtime_error("invalid spawned-only Devil migration paths");
    const std::string sourceMetadata = sourcePrefix + ".closure";
    const std::string sourceKeys = sourcePrefix + ".keys";
    const std::string sourceFrontier = sourcePrefix + ".frontier";
    const std::string destinationMetadata = destinationPrefix + ".closure";
    const std::string destinationKeys = destinationPrefix + ".keys";
    const std::string destinationFrontier = destinationPrefix + ".frontier";
    const std::array<std::string, 6> outputs{{
      destinationMetadata, destinationKeys, destinationFrontier,
      destinationMetadata + ".tmp", destinationKeys + ".tmp",
      destinationFrontier + ".tmp"}};
    for (const auto& path : outputs)
        if (::access(path.c_str(), F_OK) == 0)
            throw std::runtime_error(
              "spawned-only Devil migration destination already exists " + path);

    const SpawnedDevilCheckpoint first =
      read_spawned_devil_checkpoint(sourceMetadata, 0);
    if (first.version != 1 && first.version != 2 && first.version != 3)
        throw std::runtime_error("spawned-only Devil migration source version residual");
    // V1 and V2 both persisted the original pair of uint64_t proof-key
    // fields.  V2 only strengthened checkpointing around that unchanged
    // record layout; V3 subsequently packed the same fields into 14 bytes.
    const std::uint64_t sourceRecordBytes = first.version <= 2 ? 16 : 14;
    const int sourceKeyFd = ::open(sourceKeys.c_str(), O_RDONLY);
    const int sourceFrontierFd = ::open(sourceFrontier.c_str(), O_RDONLY);
    if (sourceKeyFd == -1 || sourceFrontierFd == -1)
        throw std::runtime_error("cannot open spawned-only Devil migration source");
    const SpawnedDevilCheckpoint second =
      read_spawned_devil_checkpoint(sourceMetadata, first.version);
    if (!(first == second))
        throw std::runtime_error("spawned-only Devil migration generation changed");
    if (first.limit > std::numeric_limits<std::uint64_t>::max() /
                        sourceRecordBytes ||
        first.limit > std::numeric_limits<std::uint64_t>::max() / 7 ||
        first.frontier > std::numeric_limits<std::uint64_t>::max() / 8)
        throw std::runtime_error("spawned-only Devil migration extent overflow");
    struct stat keyStatus {}, frontierStatus {};
    if (::fstat(sourceKeyFd, &keyStatus) != 0 ||
        ::fstat(sourceFrontierFd, &frontierStatus) != 0 ||
        static_cast<std::uint64_t>(keyStatus.st_size) !=
          first.limit * sourceRecordBytes ||
        static_cast<std::uint64_t>(frontierStatus.st_size) != first.frontier * 8)
        throw std::runtime_error("spawned-only Devil migration source extent residual");

    const int destinationKeyFd = ::open(
      (destinationKeys + ".tmp").c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    const int destinationFrontierFd = ::open(
      (destinationFrontier + ".tmp").c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (destinationKeyFd == -1 || destinationFrontierFd == -1)
        throw std::runtime_error("cannot create spawned-only Devil migration destination");
    constexpr std::uint64_t RecordsPerBlock = 1 << 16;
    std::vector<std::uint8_t> oldBlock(RecordsPerBlock * sourceRecordBytes);
    std::vector<std::uint8_t> newBlock(RecordsPerBlock * 7);
    std::uint64_t hash = DevilReverseHashOffset;
    for (std::uint64_t begin = 0; begin < first.size; begin += RecordsPerBlock) {
        const std::uint64_t count = std::min(RecordsPerBlock, first.size - begin);
        read_exact_at(sourceKeyFd, oldBlock.data(), count * sourceRecordBytes,
                      begin * sourceRecordBytes, sourceKeys);
        for (std::uint64_t index = 0; index < count; ++index) {
            const std::uint8_t* oldKey = oldBlock.data() +
                                         index * sourceRecordBytes;
            std::uint64_t low = 0;
            std::uint64_t high = 0;
            std::memcpy(&low, oldKey, sizeof(low));
            std::memcpy(&high, oldKey + 8,
                        static_cast<std::size_t>(sourceRecordBytes - 8));
            const std::uint64_t compact = pack_compact_devil_key(
              low, high, first.square);
            const auto roundTrip = unpack_compact_devil_key(compact, first.square);
            if (roundTrip.first != low || roundTrip.second != high)
                throw std::runtime_error(
                  "spawned-only Devil migration round-trip residual at " +
                  std::to_string(begin + index));
            std::uint8_t* newKey = newBlock.data() + index * 7;
            std::memcpy(newKey, &compact, 7);
            hash = migration_hash(hash, newKey, 7);
        }
        write_exact_at(destinationKeyFd, newBlock.data(), count * 7,
                       begin * 7, destinationKeys);
        if ((begin + count) / 100'000'000 != begin / 100'000'000)
            std::cout << "devil_migrate_keys " << begin + count << '/'
                      << first.size << '\n' << std::flush;
    }
    if (::ftruncate(destinationKeyFd, static_cast<off_t>(first.limit * 7)) != 0 ||
        ::fsync(destinationKeyFd) != 0)
        throw std::runtime_error("cannot finalize spawned-only Devil migrated keys");

    std::vector<std::uint8_t> copyBlock(8 << 20);
    const std::uint64_t frontierBytes = first.frontier * 8;
    for (std::uint64_t offset = 0; offset < frontierBytes;) {
        const std::size_t bytes = static_cast<std::size_t>(
          std::min<std::uint64_t>(copyBlock.size(), frontierBytes - offset));
        read_exact_at(sourceFrontierFd, copyBlock.data(), bytes, offset,
                      sourceFrontier);
        write_exact_at(destinationFrontierFd, copyBlock.data(), bytes, offset,
                       destinationFrontier);
        offset += bytes;
    }
    if (::fsync(destinationFrontierFd) != 0)
        throw std::runtime_error("cannot finalize spawned-only Devil migrated frontier");
    ::close(sourceKeyFd);
    ::close(sourceFrontierFd);
    ::close(destinationKeyFd);
    ::close(destinationFrontierFd);

    SpawnedDevilCheckpoint migrated = first;
    migrated.version = 4;
    write_spawned_devil_checkpoint(destinationMetadata + ".tmp", migrated);
    sync_file(destinationMetadata + ".tmp");
    if (std::rename((destinationKeys + ".tmp").c_str(),
                    destinationKeys.c_str()) != 0 ||
        std::rename((destinationFrontier + ".tmp").c_str(),
                    destinationFrontier.c_str()) != 0 ||
        std::rename((destinationMetadata + ".tmp").c_str(),
                    destinationMetadata.c_str()) != 0)
        throw std::runtime_error("cannot install spawned-only Devil migration");
    const std::size_t separator = destinationPrefix.find_last_of('/');
    sync_directory(separator == std::string::npos
                     ? "." : destinationPrefix.substr(0, separator));
    std::cout << "DEVIL_CHECKPOINT_MIGRATED source_version " << first.version
              << " destination_version 4"
              << " square " << first.square << " ply " << first.ply
              << " states " << first.size << " frontier " << first.frontier
              << " key_hash " << std::hex << hash << std::dec << '\n';
}

void self_test_spawned_devil_checkpoint_migration(const std::string& root) {
    // Exercise combinadic boundaries and a deterministic spread through the
    // complete size-0..5 Minion code space before testing the on-disk
    // conversion.  This catches rank/unrank regressions without enumerating
    // all 25 million valid subsets in every build.
    std::vector<std::uint64_t> minionCodes;
    std::uint64_t boundary = 0;
    for (unsigned count = 0; count <= 5; ++count) {
        const std::uint64_t size = choose_devil(80, count);
        minionCodes.push_back(boundary);
        minionCodes.push_back(boundary + size - 1);
        boundary += size;
    }
    for (std::uint64_t code = 0; code < DevilMinionCodeCount;
         code += 7919)
        minionCodes.push_back(code);
    minionCodes.push_back(DevilMinionCodeCount - 1);
    for (const std::uint64_t code : minionCodes) {
        const auto bits = decode_devil_minions(code);
        if (encode_devil_minions(bits.first, bits.second) != code)
            throw std::runtime_error("Devil compact Minion codec residual");
    }

    // Cover every ordered King pair and all remaining field endpoints.  The
    // full legacy-to-compact-to-legacy equality is the proof obligation used
    // by both migration and checkpoint restart.
    for (unsigned whiteKing = 0; whiteKing < 80; ++whiteKing)
        for (unsigned blackKing = 0; blackKing < 80; ++blackKing) {
            if (whiteKing == blackKing)
                continue;
            for (unsigned variant = 0; variant < 8; ++variant) {
                const auto bits = decode_devil_minions(
                  minionCodes[(whiteKing * 80 + blackKing + variant) %
                              minionCodes.size()]);
                std::uint64_t high = bits.second;
                high |= static_cast<std::uint64_t>(variant & 1) <<
                        LegacySideShift;
                high |= static_cast<std::uint64_t>(whiteKing) <<
                        LegacyWhiteKingShift;
                high |= static_cast<std::uint64_t>(blackKing) <<
                        LegacyBlackKingShift;
                const bool alive = (variant & 4) == 0;
                high |= static_cast<std::uint64_t>(
                  alive ? 2 : DevilNoSquare) << LegacyDevilSquareShift;
                high |= static_cast<std::uint64_t>(alive ? (variant & 3) : 0) <<
                        LegacyDevilCooldownShift;
                high |= static_cast<std::uint64_t>(
                  (variant & 2) ? DevilNoSquare : (variant * 9) % 80) <<
                        LegacySecondaryShift;
                const auto roundTrip = unpack_compact_devil_key(
                  pack_compact_devil_key(bits.first, high, 2), 2);
                if (roundTrip.first != bits.first || roundTrip.second != high)
                    throw std::runtime_error("Devil compact proof-key residual");
            }
        }

    const std::string sourceDirectory = root + "/source";
    const std::string destinationDirectory = root + "/destination";
    if ((::mkdir(sourceDirectory.c_str(), 0700) != 0 && errno != EEXIST) ||
        (::mkdir(destinationDirectory.c_str(), 0700) != 0 && errno != EEXIST))
        throw std::runtime_error("cannot create Devil migration self-test directories");
    const std::string source = sourceDirectory + "/devil-2";
    const std::string destination = destinationDirectory + "/devil-2";
    SpawnedDevilCheckpoint metadata;
    // C1 is the last retained v1 checkpoint, so the migration self-test must
    // exercise that exact 16-byte source format rather than only its v2 twin.
    metadata.version = 1;
    metadata.square = 2;
    metadata.limit = 100;
    metadata.size = 3;
    metadata.frontier = 2;
    metadata.ply = 7;
    write_spawned_devil_checkpoint(source + ".closure", metadata);
    const int keyFd = ::open((source + ".keys").c_str(),
                             O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (keyFd == -1 || ::ftruncate(keyFd, 1600) != 0)
        throw std::runtime_error("cannot create Devil migration self-test keys");
    std::array<std::uint8_t, 48> oldKeys{};
    const std::array<std::uint64_t, 3> lows{{1, 9, (1ULL << 10) | (1ULL << 20)}};
    std::array<std::uint64_t, 3> highs{};
    for (std::size_t index = 0; index < highs.size(); ++index) {
        highs[index] = static_cast<std::uint64_t>(index & 1) << LegacySideShift;
        highs[index] |= static_cast<std::uint64_t>(4 + index) <<
                        LegacyWhiteKingShift;
        highs[index] |= static_cast<std::uint64_t>(20 + index) <<
                        LegacyBlackKingShift;
        highs[index] |= static_cast<std::uint64_t>(
          index == 2 ? DevilNoSquare : metadata.square) << LegacyDevilSquareShift;
        highs[index] |= static_cast<std::uint64_t>(index == 2 ? 0 : index) <<
                        LegacyDevilCooldownShift;
        highs[index] |= static_cast<std::uint64_t>(
          index == 1 ? 30 : DevilNoSquare) << LegacySecondaryShift;
    }
    for (std::size_t index = 0; index < lows.size(); ++index) {
        std::memcpy(oldKeys.data() + index * 16, &lows[index], 8);
        std::memcpy(oldKeys.data() + index * 16 + 8, &highs[index], 8);
    }
    write_exact_at(keyFd, oldKeys.data(), oldKeys.size(), 0, source + ".keys");
    ::close(keyFd);
    const std::array<std::uint64_t, 2> frontier{{0, 2}};
    const int frontierFd = ::open((source + ".frontier").c_str(),
      O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (frontierFd == -1)
        throw std::runtime_error("cannot create Devil migration self-test frontier");
    write_exact_at(frontierFd, frontier.data(), sizeof(frontier), 0,
                   source + ".frontier");
    ::close(frontierFd);

    migrate_spawned_devil_checkpoint(source, destination);
    const SpawnedDevilCheckpoint migrated =
      read_spawned_devil_checkpoint(destination + ".closure", 4);
    struct stat keyStatus {}, frontierStatus {};
    if (migrated.square != metadata.square || migrated.limit != metadata.limit ||
        migrated.size != metadata.size || migrated.frontier != metadata.frontier ||
        migrated.ply != metadata.ply ||
        ::stat((destination + ".keys").c_str(), &keyStatus) != 0 ||
        ::stat((destination + ".frontier").c_str(), &frontierStatus) != 0 ||
        keyStatus.st_size != 700 || frontierStatus.st_size != 16)
        throw std::runtime_error("Devil migration self-test extent residual");
    std::array<std::uint8_t, 21> newKeys{};
    const int migratedFd = ::open((destination + ".keys").c_str(), O_RDONLY);
    if (migratedFd == -1)
        throw std::runtime_error("cannot reopen Devil migration self-test keys");
    read_exact_at(migratedFd, newKeys.data(), newKeys.size(), 0,
                  destination + ".keys");
    ::close(migratedFd);
    for (std::size_t index = 0; index < lows.size(); ++index) {
        std::array<std::uint8_t, 7> expected{};
        const std::uint64_t compact = pack_compact_devil_key(
          lows[index], highs[index], metadata.square);
        std::memcpy(expected.data(), &compact, 7);
        if (!std::equal(expected.begin(), expected.end(),
                        newKeys.begin() + index * 7))
            throw std::runtime_error("Devil migration self-test key residual");
    }
    std::cout << "devilcheckpointmigrationok states 3 frontier 2\n";
}

struct DevilReverseShard {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    std::array<std::uint64_t, DevilReverseBucketCount> counts{};
    std::array<std::uint64_t, DevilReverseBucketCount> hashes{};
};

std::string devil_reverse_shard_stem(const std::string& directory,
                                     std::uint32_t shard) {
    return directory + "/shard-" + std::to_string(shard);
}

std::string devil_reverse_bucket_path(const std::string& directory,
                                      std::uint32_t shard,
                                      std::uint32_t bucket) {
    return devil_reverse_shard_stem(directory, shard) + "-bucket-" +
           std::to_string(bucket) + ".edges";
}

std::string devil_reverse_sorted_bucket_path(const std::string& directory,
                                             std::uint32_t shard,
                                             std::uint32_t bucket) {
    return devil_reverse_bucket_path(directory, shard, bucket) + ".sorted";
}

std::set<std::uint32_t> adopted_devil_reverse_buckets() {
    const char* raw = std::getenv("ULTIMATE_DEVIL_REVERSE_ADOPT_BUCKETS");
    std::set<std::uint32_t> result;
    if (!raw || !*raw)
        return result;
    std::istringstream stream(raw);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (token.empty() || token.find_first_not_of("0123456789") !=
                               std::string::npos)
            throw std::runtime_error("invalid adopted Devil reverse bucket list");
        const unsigned long oneBased = std::stoul(token);
        if (!oneBased || oneBased > DevilReverseBucketCount ||
            !result.insert(static_cast<std::uint32_t>(oneBased - 1)).second)
            throw std::runtime_error("invalid adopted Devil reverse bucket");
    }
    return result;
}

void write_devil_reverse_marker(const std::string& directory,
                                std::uint32_t shard, int fixedSquare,
                                std::uint64_t graphStates,
                                const DevilReverseShard& value) {
    const std::string path = devil_reverse_shard_stem(directory, shard) +
                             ".complete";
    const std::string temporary = path + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        const std::array<char, 8> magic{{'U','F','D','R','S','P','1','\0'}};
        const std::uint32_t version = 1;
        const std::uint32_t square = static_cast<std::uint32_t>(fixedSquare);
        const std::uint32_t buckets = DevilReverseBucketCount;
        stream.write(magic.data(), magic.size());
        stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
        stream.write(reinterpret_cast<const char*>(&square), sizeof(square));
        stream.write(reinterpret_cast<const char*>(&graphStates), sizeof(graphStates));
        stream.write(reinterpret_cast<const char*>(&value.begin), sizeof(value.begin));
        stream.write(reinterpret_cast<const char*>(&value.end), sizeof(value.end));
        stream.write(reinterpret_cast<const char*>(&buckets), sizeof(buckets));
        stream.write(reinterpret_cast<const char*>(value.counts.data()),
                     sizeof(value.counts));
        stream.write(reinterpret_cast<const char*>(value.hashes.data()),
                     sizeof(value.hashes));
        if (!stream)
            throw std::runtime_error("cannot write Devil reverse shard marker");
    }
    sync_file(temporary);
    if (std::rename(temporary.c_str(), path.c_str()) != 0)
        throw std::runtime_error("cannot install Devil reverse shard marker");
    sync_directory(directory);
}

std::optional<DevilReverseShard> read_devil_reverse_marker(
  const std::string& directory, std::uint32_t shard, int fixedSquare,
  std::uint64_t graphStates, std::uint64_t expectedBegin,
  std::uint64_t expectedEnd) {
    const std::string path = devil_reverse_shard_stem(directory, shard) +
                             ".complete";
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return std::nullopt;
    std::array<char, 8> magic{};
    std::uint32_t version = 0, square = 0, buckets = 0;
    std::uint64_t states = 0;
    DevilReverseShard value;
    stream.read(magic.data(), magic.size());
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));
    stream.read(reinterpret_cast<char*>(&square), sizeof(square));
    stream.read(reinterpret_cast<char*>(&states), sizeof(states));
    stream.read(reinterpret_cast<char*>(&value.begin), sizeof(value.begin));
    stream.read(reinterpret_cast<char*>(&value.end), sizeof(value.end));
    stream.read(reinterpret_cast<char*>(&buckets), sizeof(buckets));
    stream.read(reinterpret_cast<char*>(value.counts.data()), sizeof(value.counts));
    stream.read(reinterpret_cast<char*>(value.hashes.data()), sizeof(value.hashes));
    const std::array<char, 8> expectedMagic{{'U','F','D','R','S','P','1','\0'}};
    if (!stream || stream.peek() != std::ifstream::traits_type::eof() ||
        magic != expectedMagic || version != 1 ||
        square != static_cast<std::uint32_t>(fixedSquare) ||
        states != graphStates || value.begin != expectedBegin ||
        value.end != expectedEnd || buckets != DevilReverseBucketCount)
        throw std::runtime_error("Devil reverse shard marker residual " + path);
    for (std::uint32_t bucket = 0; bucket < DevilReverseBucketCount; ++bucket) {
        struct stat status {};
        const std::string edgePath = devil_reverse_bucket_path(
          directory, shard, bucket);
        const std::uint64_t bytes = value.counts[bucket] *
                                    sizeof(DevilReverseRecord);
        if (::stat(edgePath.c_str(), &status) != 0 ||
            static_cast<std::uint64_t>(status.st_size) != bytes)
            throw std::runtime_error("Devil reverse edge extent residual " + edgePath);
    }
    return value;
}

template<typename Successors>
void build_restartable_devil_reverse(
  const std::string& prefix, int fixedSquare, std::uint64_t graphStates,
  std::uint64_t edgeCount, std::uint32_t workers,
  const std::uint32_t* degrees, std::uint64_t* offsets,
  std::uint64_t* predecessors, std::uint8_t* predecessorSides,
  Successors&& successors) {
    if (!graphStates || !workers)
        throw std::runtime_error("invalid Devil reverse spool geometry");
    const std::string directory = prefix + ".reverse-spool";
    if (::mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST)
        throw std::runtime_error("cannot create Devil reverse spool directory");
    struct stat directoryStatus {};
    if (::stat(directory.c_str(), &directoryStatus) != 0 ||
        !S_ISDIR(directoryStatus.st_mode) ||
        ::access(directory.c_str(), W_OK | X_OK) != 0)
        throw std::runtime_error(
          "Devil reverse spool directory is not a writable resolved directory: " +
          directory);
    // The shard geometry is independent of worker count so a resumed service
    // may safely gain or lose CPUs without invalidating completed shards.
    const std::uint32_t shardCount = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(512, graphStates));
    const std::uint64_t shardWidth =
      (graphStates + shardCount - 1) / shardCount;
    const std::uint64_t bucketWidth =
      (graphStates + DevilReverseBucketCount - 1) /
      DevilReverseBucketCount;
    std::vector<DevilReverseShard> shards(shardCount);
    std::atomic<std::uint32_t> nextShard{0};
    std::atomic<std::uint64_t> completedStates{0};
    std::atomic<bool> failed{false};
    std::exception_ptr failure;
    std::mutex failureMutex;
    std::mutex outputMutex;
    std::vector<std::thread> tasks;
    for (std::uint32_t worker = 0; worker < workers; ++worker)
        tasks.emplace_back([&] {
            try {
                while (!failed.load(std::memory_order_relaxed)) {
                    const std::uint32_t shard = nextShard.fetch_add(
                      1, std::memory_order_relaxed);
                    if (shard >= shardCount)
                        break;
                    const std::uint64_t begin = std::min(
                      graphStates, std::uint64_t(shard) * shardWidth);
                    const std::uint64_t end = std::min(
                      graphStates, begin + shardWidth);
                    if (const auto saved = read_devil_reverse_marker(
                          directory, shard, fixedSquare, graphStates, begin, end)) {
                        shards[shard] = *saved;
                    }
                    else {
                        DevilReverseShard value;
                        value.begin = begin;
                        value.end = end;
                        value.hashes.fill(DevilReverseHashOffset);
                        std::array<std::ofstream, DevilReverseBucketCount> streams;
                        std::array<std::vector<char>, DevilReverseBucketCount> buffers;
                        for (std::uint32_t bucket = 0;
                             bucket < DevilReverseBucketCount; ++bucket) {
                            buffers[bucket].resize(64 * 1024);
                            streams[bucket].rdbuf()->pubsetbuf(
                              buffers[bucket].data(), buffers[bucket].size());
                            streams[bucket].open(
                              devil_reverse_bucket_path(directory, shard, bucket) +
                                ".tmp",
                              std::ios::binary | std::ios::trunc);
                            if (!streams[bucket])
                                throw std::runtime_error(
                                  "cannot create Devil reverse edge spool");
                        }
                        for (std::uint64_t parent = begin; parent < end; ++parent) {
                            successors(parent, [&](std::optional<std::uint64_t> child,
                                                   std::optional<Color>,
                                                   bool sameSide) {
                                if (!child)
                                    return;
                                const std::uint32_t bucket = static_cast<std::uint32_t>(
                                  std::min<std::uint64_t>(
                                    DevilReverseBucketCount - 1,
                                    *child / bucketWidth));
                                DevilReverseRecord record{
                                  *child, parent | (sameSide ? DevilReverseSameSide : 0)};
                                streams[bucket].write(
                                  reinterpret_cast<const char*>(&record),
                                  sizeof(record));
                                if (!streams[bucket])
                                    throw std::runtime_error(
                                      "cannot write Devil reverse edge spool");
                                ++value.counts[bucket];
                                value.hashes[bucket] = devil_reverse_hash(
                                  value.hashes[bucket], record);
                            });
                        }
                        for (auto& stream : streams)
                            stream.close();
                        for (std::uint32_t bucket = 0;
                             bucket < DevilReverseBucketCount; ++bucket) {
                            const std::string path = devil_reverse_bucket_path(
                              directory, shard, bucket);
                            const std::string temporary = path + ".tmp";
                            sync_file(temporary);
                            if (std::rename(temporary.c_str(), path.c_str()) != 0)
                                throw std::runtime_error(
                                  "cannot install Devil reverse edge spool");
                        }
                        sync_directory(directory);
                        write_devil_reverse_marker(
                          directory, shard, fixedSquare, graphStates, value);
                        shards[shard] = value;
                    }
                    const std::uint64_t done = completedStates.fetch_add(
                      end - begin, std::memory_order_relaxed) + end - begin;
                    std::lock_guard<std::mutex> lock(outputMutex);
                    std::cout << "devil_reverse_spool square " << fixedSquare
                              << " states " << done << '/' << graphStates
                              << " shards " << shard + 1 << '/' << shardCount
                              << '\n' << std::flush;
                }
            }
            catch (...) {
                failed.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(failureMutex);
                if (!failure)
                    failure = std::current_exception();
            }
        });
    for (auto& task : tasks)
        task.join();
    if (failure)
        std::rethrow_exception(failure);

    const std::set<std::uint32_t> adopted = adopted_devil_reverse_buckets();
    std::array<std::uint64_t, DevilReverseBucketCount> bucketRecords{};
    for (const DevilReverseShard& shard : shards)
        for (std::uint32_t bucket = 0; bucket < DevilReverseBucketCount; ++bucket)
            bucketRecords[bucket] += shard.counts[bucket];

    std::uint64_t computedEdges = 0;
    for (std::uint64_t index = 0; index < graphStates; ++index) {
        const std::uint64_t begin = computedEdges;
        computedEdges += degrees[index];
        const std::uint32_t bucket = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(DevilReverseBucketCount - 1,
                                  index / bucketWidth));
        if (adopted.count(bucket)) {
            // A stopped in-progress merge leaves a completed child's cursor
            // at its end; a fully finalized retained CSR restores it to the
            // begin offset.  Both forms prove completion when the complete
            // payload below is also structurally valid.
            if (offsets[index] != begin && offsets[index] != computedEdges)
                throw std::runtime_error(
                  "adopted Devil reverse cursor residual at graph index " +
                  std::to_string(index));
            for (std::uint64_t edge = begin; edge < computedEdges; ++edge)
                if (predecessors[edge] >= graphStates ||
                    predecessorSides[edge] > 1)
                    throw std::runtime_error(
                      "adopted Devil reverse payload residual");
            offsets[index] = computedEdges;
        }
        else
            offsets[index] = begin;
    }
    offsets[graphStates] = computedEdges;
    if (computedEdges != edgeCount)
        throw std::runtime_error("Devil reverse degree conservation residual");
    std::array<std::uint64_t, DevilReverseBucketCount> degreeRecords{};
    for (std::uint64_t index = 0; index < graphStates; ++index) {
        const std::uint32_t bucket = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(DevilReverseBucketCount - 1,
                                  index / bucketWidth));
        degreeRecords[bucket] += degrees[index];
    }
    if (degreeRecords != bucketRecords)
        throw std::runtime_error("Devil reverse bucket degree residual");
    if (!adopted.empty()) {
        std::cout << "devil_reverse_adopt square " << fixedSquare
                  << " buckets";
        for (const std::uint32_t bucket : adopted)
            std::cout << ' ' << bucket + 1;
        std::cout << '\n' << std::flush;
    }

    std::atomic<std::uint32_t> nextBucket{0};
    failed.store(false, std::memory_order_relaxed);
    failure = nullptr;
    tasks.clear();
    // Each shard/bucket file is only a few hundred MiB even when the complete
    // reverse graph has tens of billions of edges.  Sort those independent
    // files once, then perform a buffered k-way merge.  The old implementation
    // sorted one-million-record chunks and scattered each chunk through the
    // same mmap range, causing tens of TiB of write amplification for C1.
    // This path writes the retained predecessor planes monotonically.
    const auto processBucket = [&](std::uint32_t bucket) {
        std::atomic<std::uint32_t> nextSortShard{0};
        std::atomic<bool> sortFailed{false};
        std::exception_ptr sortFailure;
        std::mutex sortFailureMutex;
        std::vector<std::thread> sortTasks;
        const std::uint32_t bucketWorkers = std::min<std::uint32_t>(
          workers, DevilReverseBucketCount);
        const std::uint32_t sortWorkers = std::max<std::uint32_t>(
          1, workers / bucketWorkers);
        for (std::uint32_t worker = 0; worker < sortWorkers; ++worker)
            sortTasks.emplace_back([&, bucket] {
            try {
                std::vector<DevilReverseRecord> records;
                while (!sortFailed.load(std::memory_order_relaxed)) {
                    const std::uint32_t shard = nextSortShard.fetch_add(
                      1, std::memory_order_relaxed);
                    if (shard >= shardCount)
                        break;
                    const std::uint64_t count = shards[shard].counts[bucket];
                    if (count > std::numeric_limits<std::size_t>::max() /
                                  sizeof(DevilReverseRecord))
                        throw std::runtime_error(
                          "Devil reverse sorted shard allocation overflow");
                    records.resize(static_cast<std::size_t>(count));
                    const std::string input = devil_reverse_bucket_path(
                      directory, shard, bucket);
                    std::ifstream stream(input, std::ios::binary);
                    if (!stream)
                        throw std::runtime_error(
                          "cannot read Devil reverse edge spool");
                    if (count)
                        stream.read(reinterpret_cast<char*>(records.data()),
                                    static_cast<std::streamsize>(
                                      count * sizeof(DevilReverseRecord)));
                    if (!stream || stream.peek() !=
                                   std::ifstream::traits_type::eof())
                        throw std::runtime_error(
                          "cannot read complete Devil reverse edge spool");
                    std::uint64_t hash = DevilReverseHashOffset;
                    for (const auto& record : records)
                        hash = devil_reverse_hash(hash, record);
                    if (hash != shards[shard].hashes[bucket])
                        throw std::runtime_error(
                          "Devil reverse edge spool hash residual");
                    std::sort(records.begin(), records.end(),
                              [](const auto& left, const auto& right) {
                                  return std::tie(left.child, left.packedParent) <
                                         std::tie(right.child, right.packedParent);
                              });
                    const std::string output = devil_reverse_sorted_bucket_path(
                      directory, shard, bucket);
                    const std::string temporary = output + ".tmp";
                    std::ofstream sorted(temporary,
                      std::ios::binary | std::ios::trunc);
                    if (count)
                        sorted.write(reinterpret_cast<const char*>(records.data()),
                                     static_cast<std::streamsize>(
                                       count * sizeof(DevilReverseRecord)));
                    if (!sorted)
                        throw std::runtime_error(
                          "cannot write sorted Devil reverse shard");
                    sorted.close();
                    sync_file(temporary);
                    if (std::rename(temporary.c_str(), output.c_str()) != 0)
                        throw std::runtime_error(
                          "cannot install sorted Devil reverse shard");
                }
            }
            catch (...) {
                sortFailed.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(sortFailureMutex);
                if (!sortFailure)
                    sortFailure = std::current_exception();
            }
        });
        for (auto& task : sortTasks)
            task.join();
        if (sortFailure)
            std::rethrow_exception(sortFailure);
        sync_directory(directory);
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "devil_reverse_sort square " << fixedSquare
                      << " bucket " << bucket + 1 << '/'
                      << DevilReverseBucketCount << " shards " << shardCount
                      << '\n' << std::flush;
        }

        struct SortedReader {
            std::ifstream stream;
            std::uint64_t remaining = 0;
            std::vector<DevilReverseRecord> buffer;
            std::size_t cursor = 0;
            std::size_t size = 0;
            SortedReader(const std::string& path, std::uint64_t count)
                : stream(path, std::ios::binary), remaining(count),
                  buffer(4096) {
                if (!stream)
                    throw std::runtime_error(
                      "cannot open sorted Devil reverse shard");
            }
            bool next(DevilReverseRecord& value) {
                if (cursor == size) {
                    if (!remaining) {
                        if (stream.peek() != std::ifstream::traits_type::eof())
                            throw std::runtime_error(
                              "sorted Devil reverse shard extent residual");
                        return false;
                    }
                    size = static_cast<std::size_t>(
                      std::min<std::uint64_t>(remaining, buffer.size()));
                    stream.read(reinterpret_cast<char*>(buffer.data()),
                                static_cast<std::streamsize>(
                                  size * sizeof(DevilReverseRecord)));
                    if (!stream)
                        throw std::runtime_error(
                          "cannot read sorted Devil reverse shard");
                    remaining -= size;
                    cursor = 0;
                }
                value = buffer[cursor++];
                return true;
            }
        };
        struct HeapItem {
            DevilReverseRecord record;
            std::uint32_t shard = 0;
        };
        const auto greater = [](const HeapItem& left, const HeapItem& right) {
            return std::tie(left.record.child, left.record.packedParent,
                            left.shard) >
                   std::tie(right.record.child, right.record.packedParent,
                            right.shard);
        };
        std::priority_queue<HeapItem, std::vector<HeapItem>, decltype(greater)>
          heap(greater);
        std::vector<std::unique_ptr<SortedReader>> readers;
        readers.reserve(shardCount);
        for (std::uint32_t shard = 0; shard < shardCount; ++shard) {
            readers.push_back(std::make_unique<SortedReader>(
              devil_reverse_sorted_bucket_path(directory, shard, bucket),
              shards[shard].counts[bucket]));
            DevilReverseRecord record;
            if (readers.back()->next(record))
                heap.push({record, shard});
        }
        const std::uint64_t bucketBegin = std::uint64_t(bucket) * bucketWidth;
        const std::uint64_t bucketEnd = std::min(
          graphStates, bucketBegin + bucketWidth);
        std::uint64_t merged = 0;
        while (!heap.empty()) {
            const HeapItem item = heap.top();
            heap.pop();
            if (item.record.child < bucketBegin ||
                item.record.child >= bucketEnd)
                throw std::runtime_error(
                  "sorted Devil reverse edge bucket residual");
            const std::uint64_t cursor = offsets[item.record.child]++;
            if (cursor >= edgeCount)
                throw std::runtime_error("Devil reverse cursor overflow");
            predecessors[cursor] =
              item.record.packedParent & ~DevilReverseSameSide;
            predecessorSides[cursor] =
              (item.record.packedParent & DevilReverseSameSide) != 0;
            ++merged;
            if (merged % 100'000'000 == 0) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "devil_reverse_sequential square " << fixedSquare
                          << " bucket " << bucket + 1 << " records "
                          << merged << '/' << bucketRecords[bucket]
                          << '\n' << std::flush;
            }
            DevilReverseRecord next;
            if (readers[item.shard]->next(next))
                heap.push({next, item.shard});
        }
        if (merged != bucketRecords[bucket])
            throw std::runtime_error("Devil reverse sorted count residual");
        readers.clear();
        for (std::uint32_t shard = 0; shard < shardCount; ++shard)
            if (::unlink(devil_reverse_sorted_bucket_path(
                  directory, shard, bucket).c_str()) != 0)
                throw std::runtime_error(
                  "cannot remove sorted Devil reverse shard");
        sync_directory(directory);
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "devil_reverse_merge square " << fixedSquare
                      << " bucket " << bucket + 1 << '/'
                      << DevilReverseBucketCount << " parallel "
                      << std::min<std::uint32_t>(workers,
                                                DevilReverseBucketCount)
                      << '\n' << std::flush;
        }
    };
    const std::uint32_t bucketWorkers = std::min<std::uint32_t>(
      workers, DevilReverseBucketCount);
    for (std::uint32_t worker = 0; worker < bucketWorkers; ++worker)
        tasks.emplace_back([&] {
            try {
                while (!failed.load(std::memory_order_relaxed)) {
                    std::uint32_t bucket = nextBucket.fetch_add(
                      1, std::memory_order_relaxed);
                    while (bucket < DevilReverseBucketCount &&
                           adopted.count(bucket))
                        bucket = nextBucket.fetch_add(1,
                                                       std::memory_order_relaxed);
                    if (bucket >= DevilReverseBucketCount)
                        break;
                    processBucket(bucket);
                }
            }
            catch (...) {
                failed.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(failureMutex);
                if (!failure)
                    failure = std::current_exception();
            }
        });
    for (auto& task : tasks)
        task.join();
    if (failure)
        std::rethrow_exception(failure);
    std::uint64_t running = 0;
    for (std::uint64_t index = 0; index < graphStates; ++index) {
        if (offsets[index] != running + degrees[index])
            throw std::runtime_error("Devil reverse merge conservation residual");
        offsets[index] = running;
        running += degrees[index];
    }
    offsets[graphStates] = running;
    if (running != edgeCount)
        throw std::runtime_error("Devil reverse edge-count residual");
}

void self_test_devil_reverse_spool(const std::string& workDirectory) {
    if (::mkdir(workDirectory.c_str(), 0700) != 0 && errno != EEXIST)
        throw std::runtime_error("cannot create Devil reverse self-test directory");
    constexpr std::uint64_t States = 64;
    std::array<std::vector<std::pair<std::uint64_t, bool>>, States> expected;
    for (std::uint64_t parent = 0; parent < States; ++parent) {
        const std::array<std::pair<std::uint64_t, bool>, 3> children{{
          {(parent + 1) % States, false},
          {(parent * 7 + 3) % States, true},
          {(parent * 13 + 11) % States, parent % 3 == 0}}};
        for (const auto& edge : children)
            expected[edge.first].push_back({parent, edge.second});
    }
    const std::uint64_t edges = States * 3;
    const auto run = [&](std::uint64_t expectedSuccessorCalls) {
        std::array<std::uint64_t, States + 1> offsets{};
        std::array<std::uint64_t, edges> predecessors{};
        std::array<std::uint8_t, edges> sides{};
        std::array<std::uint32_t, States> observedDegrees{};
        std::atomic<std::uint64_t> successorCalls{0};
        const auto observedSuccessors = [&](std::uint64_t parent, auto&& consume) {
            successorCalls.fetch_add(1, std::memory_order_relaxed);
            const std::array<std::pair<std::uint64_t, bool>, 3> children{{
              {(parent + 1) % States, false},
              {(parent * 7 + 3) % States, true},
              {(parent * 13 + 11) % States, parent % 3 == 0}}};
            for (const auto& [child, sameSide] : children)
                consume(std::optional<std::uint64_t>{child},
                        std::optional<Color>{}, sameSide);
            consume(std::optional<std::uint64_t>{}, Color::White, false);
        };
        for (std::uint64_t child = 0; child < States; ++child)
            observedDegrees[child] = static_cast<std::uint32_t>(
              expected[child].size());
        build_restartable_devil_reverse(
          workDirectory + "/synthetic", 2, States, edges, 4,
          observedDegrees.data(), offsets.data(), predecessors.data(),
          sides.data(), observedSuccessors);
        if (successorCalls.load(std::memory_order_relaxed) !=
            expectedSuccessorCalls)
            throw std::runtime_error(
              "Devil reverse self-test resume did not reuse completed shards");
        for (std::uint64_t child = 0; child < States; ++child) {
            std::vector<std::pair<std::uint64_t, bool>> actual;
            for (std::uint64_t edge = offsets[child];
                 edge < offsets[child + 1]; ++edge)
                actual.push_back({predecessors[edge], sides[edge] != 0});
            std::sort(actual.begin(), actual.end());
            auto wanted = expected[child];
            std::sort(wanted.begin(), wanted.end());
            if (actual != wanted)
                throw std::runtime_error(
                  "Devil reverse self-test predecessor residual");
        }
    };
    // The second run must authenticate and reuse the completed edge shards
    // while rebuilding identical final arrays from scratch.
    run(States);
    run(0);
    // A production recovery may retain already-merged predecessor planes.
    // Exercise the explicit adoption gate against those exact bytes: every
    // cursor and payload must validate, and no successor replay is allowed.
    std::array<std::uint64_t, States + 1> adoptedOffsets{};
    std::array<std::uint64_t, edges> adoptedPredecessors{};
    std::array<std::uint8_t, edges> adoptedSides{};
    std::array<std::uint32_t, States> adoptedDegrees{};
    for (std::uint64_t child = 0; child < States; ++child)
        adoptedDegrees[child] = static_cast<std::uint32_t>(
          expected[child].size());
    const auto noReplay = [&](std::uint64_t, auto&&) {
        throw std::runtime_error(
          "adopted Devil reverse self-test replayed successors");
        return std::uint32_t{0};
    };
    // Populate the retained planes once from authenticated spool shards.
    build_restartable_devil_reverse(
      workDirectory + "/synthetic", 2, States, edges, 4,
      adoptedDegrees.data(), adoptedOffsets.data(), adoptedPredecessors.data(),
      adoptedSides.data(), noReplay);
    const char* prior = std::getenv("ULTIMATE_DEVIL_REVERSE_ADOPT_BUCKETS");
    const std::string priorValue = prior ? prior : "";
    if (::setenv("ULTIMATE_DEVIL_REVERSE_ADOPT_BUCKETS",
                 "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16", 1) != 0)
        throw std::runtime_error("cannot set Devil reverse adoption self-test");
    build_restartable_devil_reverse(
      workDirectory + "/synthetic", 2, States, edges, 4,
      adoptedDegrees.data(), adoptedOffsets.data(), adoptedPredecessors.data(),
      adoptedSides.data(), noReplay);
    if (prior) {
        if (::setenv("ULTIMATE_DEVIL_REVERSE_ADOPT_BUCKETS",
                     priorValue.c_str(), 1) != 0)
            throw std::runtime_error(
              "cannot restore Devil reverse adoption environment");
    }
    else if (::unsetenv("ULTIMATE_DEVIL_REVERSE_ADOPT_BUCKETS") != 0)
        throw std::runtime_error(
          "cannot clear Devil reverse adoption self-test");
    std::cout << "devilreversespoolok states " << States
              << " edges " << edges << '\n';
}

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
    struct ConcreteWorld {
        std::uint32_t index = 0;
        Color owner = Color::White;
    };

    struct PairForces {
        std::array<std::uint32_t, 2> indices{};
        std::array<bool, 2> owner{};
        bool uninformed = false;
        Color ownerColor = Color::White;
    };

    explicit JesterInformationOverlay(const std::string& path,
                                      const std::string& expectedSourceSha256,
                                      const std::string& expectedModelSha256,
                                      PieceType secondary = PieceType::Count,
                                      Color secondaryColor = Color::White,
                                      Color ownerColor = Color::White)
        : secondary_(secondary), secondaryColor_(secondaryColor),
          ownerColor_(ownerColor),
          stateCount_(secondary == PieceType::Count
                        ? PlacementStateCount : FourPlacementStateCount) {
        if (secondary_ != PieceType::Count && secondary_ != PieceType::Queen)
            throw std::runtime_error(
              "unsupported lower Jester information-overlay material");
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
        // The dense K+Jester overlay keeps the historical placeholder color
        // in word 20 even though there is no secondary piece.  Only a real
        // secondary piece changes color under the Black-owner normalization.
        const Color encodedSecondaryColor =
          secondary_ == PieceType::Count || ownerColor_ == Color::White
            ? secondaryColor_ : ~secondaryColor_;
        if (word(8) != 2 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Jester) ||
            word(16) != static_cast<std::uint32_t>(secondary_) ||
            word(20) != static_cast<std::uint32_t>(encodedSecondaryColor) ||
            word(24) != stateCount_ || word(28) != 1)
            throw std::runtime_error("lower Jester overlay has the wrong material class");
        const std::string sourceSha256(header.data() + 32, 64);
        const std::string modelSha256(header.data() + 96, 64);
        if (expectedSourceSha256.size() != 64 || sourceSha256 != expectedSourceSha256)
            throw std::runtime_error(
              "lower Jester overlay does not match its concrete table SHA-256");
        if (expectedModelSha256.size() != 64 || modelSha256 != expectedModelSha256)
            throw std::runtime_error(
              "lower Jester overlay does not match the information model SHA-256");
        flags_.resize(stateCount_);
        input.read(reinterpret_cast<char*>(flags_.data()), flags_.size());
        if (static_cast<std::size_t>(input.gcount()) != flags_.size())
            throw std::runtime_error("truncated lower Jester information overlay");
    }

    [[nodiscard]] bool loaded() const { return !flags_.empty(); }

    [[nodiscard]] std::optional<ConcreteWorld> concrete_world(
      const Position& position) const {
        int jester = Position::NoPiece;
        int secondary = Position::NoPiece;
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
            else if (secondary_ != PieceType::Count &&
                     piece.type == secondary_ && piece.color == secondaryColor_ &&
                     secondary == Position::NoPiece)
                secondary = id;
            else
                return std::nullopt;
        }
        const int expectedAlive = secondary_ == PieceType::Count ? 3 : 4;
        if (alive != expectedAlive || jester == Position::NoPiece ||
            kings[0] == Position::NoPiece || kings[1] == Position::NoPiece)
            return std::nullopt;
        if ((secondary_ == PieceType::Count) !=
            (secondary == Position::NoPiece))
            return std::nullopt;
        const Color owner = position.piece(jester).color;
        if (owner != ownerColor_)
            return std::nullopt;
        const auto normalize_square = [&](int id) {
            const std::uint8_t square = static_cast<std::uint8_t>(
              position.piece(id).square);
            return ownerColor_ == Color::White ? square
              : static_cast<std::uint8_t>(
                  (Position::BoardRanks - 1 -
                   square / Position::BoardFiles) * Position::BoardFiles +
                  square % Position::BoardFiles);
        };
        const Color normalizedSide = ownerColor_ == Color::White
                                   ? position.side_to_move()
                                   : ~position.side_to_move();
        const int normalizedWhiteKing = ownerColor_ == Color::White
                                      ? kings[0] : kings[1];
        const int normalizedBlackKing = ownerColor_ == Color::White
                                      ? kings[1] : kings[0];
        const std::uint32_t index = secondary_ == PieceType::Count
          ? encode_placement({normalizedSide,
                              normalize_square(normalizedWhiteKing),
                              normalize_square(normalizedBlackKing),
                              normalize_square(jester), 0})
          : encode_four({normalizedSide,
                         normalize_square(normalizedWhiteKing),
                         normalize_square(normalizedBlackKing),
                         normalize_square(jester),
                         normalize_square(secondary)});
        return ConcreteWorld{index, owner};
    }

    // Probe only an information set that is known to contain both canonical
    // King/Jester assignments.  A singleton child must instead use the
    // concrete .uftb WDL: probing one concrete world through this dense overlay
    // would silently replace its history-refined belief with a fresh maximal
    // public-view root.
    [[nodiscard]] PairForces pair_forces(const Position& first,
                                         const Position& second,
                                         Color uninformedTarget) const {
        const auto firstWorld = concrete_world(first);
        const auto secondWorld = concrete_world(second);
        if (!firstWorld || !secondWorld)
            throw std::runtime_error(
              "paired lower Jester successor is not K+Jester-v-K");
        if (!loaded())
            throw std::runtime_error(
              "a paired K+Jester-v-K successor requires "
              "--lower-information-overlay");
        if (firstWorld->owner != secondWorld->owner ||
            uninformedTarget == firstWorld->owner)
            throw std::runtime_error(
              "lower Jester pair must be probed for its uninformed side");

        std::uint32_t alternative = 0;
        if (secondary_ == PieceType::Count) {
            State decoded = decode_placement(firstWorld->index);
            std::swap(decoded.whiteKing, decoded.attacker);
            alternative = encode_placement(decoded);
        }
        else {
            FourState decoded = decode_four(firstWorld->index);
            std::swap(decoded.whiteKing, decoded.first);
            alternative = encode_four(decoded);
        }
        if (alternative != secondWorld->index ||
            firstWorld->index == secondWorld->index)
            throw std::runtime_error(
              "lower Jester successor is not the exact canonical royal pair");

        const std::uint8_t firstFlags = flags_.at(firstWorld->index);
        const std::uint8_t secondFlags = flags_.at(secondWorld->index);
        if (!(firstFlags & 4) || !(secondFlags & 4))
            throw std::runtime_error(
              "lower Jester successor is outside the admitted overlay domain");
        const bool firstForces = (firstFlags & 2) != 0;
        const bool secondForces = (secondFlags & 2) != 0;
        if (firstForces != secondForces)
            throw std::runtime_error(
              "lower Jester pair has inconsistent uninformed-side flags");
        return {{firstWorld->index, secondWorld->index},
                {{(firstFlags & 1) != 0, (secondFlags & 1) != 0}},
                firstForces, firstWorld->owner};
    }

   private:
    PieceType secondary_ = PieceType::Count;
    Color secondaryColor_ = Color::White;
    Color ownerColor_ = Color::White;
    std::uint32_t stateCount_ = PlacementStateCount;
    std::vector<std::uint8_t> flags_;
};

void self_test_jester_overlay_color_symmetry() {
    const auto build = [](Color owner) {
        Position position;
        position.clear();
        if (owner == Color::White) {
            position.add_piece(PieceType::King, Color::White, 0);
            position.add_piece(PieceType::King, Color::Black, 79);
            position.add_piece(PieceType::Jester, Color::White, 18);
            position.set_side_to_move(Color::Black);
        }
        else {
            position.add_piece(PieceType::King, Color::White, 7);
            position.add_piece(PieceType::King, Color::Black, 72);
            position.add_piece(PieceType::Jester, Color::Black, 58);
            position.set_side_to_move(Color::White);
        }
        for (int id = 0; id < position.piece_count(); ++id)
            position.piece(id).moved = true;
        return position;
    };
    const JesterInformationOverlay white(
      "", "", "", PieceType::Count, Color::White, Color::White);
    const JesterInformationOverlay black(
      "", "", "", PieceType::Count, Color::White, Color::Black);
    const auto whiteWorld = white.concrete_world(build(Color::White));
    const auto blackWorld = black.concrete_world(build(Color::Black));
    if (!whiteWorld || !blackWorld ||
        whiteWorld->index != blackWorld->index ||
        whiteWorld->owner != Color::White ||
        blackWorld->owner != Color::Black)
        throw std::runtime_error(
          "lower Jester overlay color-symmetry normalization residual");
    std::cout << "information_lower_jester_color_symmetry index "
              << whiteWorld->index << " residual 0\n";
}

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
    case PieceType::Angel:
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
    case PieceType::Checker:
    case PieceType::Angel: return true;
    default: return false;
    }
}

bool closed_unsplit_copycat_secondary(PieceType type) {
    // The tablebase starts with a linked mirror pair and retains only material
    // classes whose native moves cannot separate its two models. Penguin can
    // freeze one half, Mage can swap one allied half, and Fisherman can pull
    // either half; those pairings require a larger displaced-pair codec.
    if (type == PieceType::Penguin || type == PieceType::Mage ||
        type == PieceType::Fisherman || type == PieceType::Angel)
        return false;
    return type == PieceType::Copycat || closed_four_piece(type);
}

std::uint32_t ordinary_substate_count(PieceType type) {
    switch (type) {
    case PieceType::Berserker: return 10;  // power 0..8, then board-saturating 9+
    case PieceType::Ghost: return 2;
    case PieceType::Devil: return 4;
    case PieceType::Sniper: return 4;
    case PieceType::Prince: return 2;
    case PieceType::Checker: return 4;  // ordinary/promoted x normal/forced jump
    case PieceType::Pawn: return 2;
    default: return 1;
    }
}

std::uint32_t material_substate_count(PieceType type, bool fourModels,
                                      PieceType other, Color typeColor,
                                      Color otherColor) {
    if (type == PieceType::Angel)
        return 2 + (fourModels && typeColor == otherColor
          ? other == PieceType::Copycat ? 2 : 1 : 0);
    if (type != PieceType::Penguin)
        return ordinary_substate_count(type);
    // A Penguin remembers exactly which currently adjacent characters it
    // froze on its preceding move.  A later move may enter its aura without
    // becoming frozen, so an inactive/active bit is not an exact state model.
    // Bits 0/1 identify the two Kings.  In a one-Penguin four-model class bit
    // 2 identifies the other non-King; two Penguins never freeze each other.
    if (!fourModels || other == PieceType::Penguin)
        return 4;
    return 8;
}

bool parallel_graph_scan_enabled(bool reverse, std::uint32_t checkpointEvery,
                                 bool diskBacked, std::uint32_t stateCount) {
    const bool capable = diskBacked || stateCount >= 300'000'000;
    return capable && (reverse || !checkpointEvery);
}

constexpr bool packed_codec_matches(
  std::uint32_t version, bool trackedGhost, bool angelGraph,
  bool foldedGiant, bool linkedCopycatPair, bool angelCopycatGraph,
  std::uint64_t codecTag) {
    if (version == 11)
        return !trackedGhost && !angelGraph && !foldedGiant &&
          !linkedCopycatPair && !angelCopycatGraph &&
          codecTag == SpawnedDevilRootV1Tag;
    if (version == 10)
        return (linkedCopycatPair && !angelGraph && !trackedGhost &&
                codecTag == LinkedCopycatPairV1Tag) ||
               (angelCopycatGraph && angelGraph && !trackedGhost &&
                !foldedGiant && codecTag == AngelCopycatGraphV1Tag);
    if (version == 9)
        return angelGraph && !trackedGhost && !angelCopycatGraph &&
          codecTag == (foldedGiant ? AngelGiantGraphV1Tag : AngelGraphV1Tag);
    if (version == 8)
        return trackedGhost && !angelGraph && !foldedGiant &&
          codecTag == TrackedGhostV1Tag;
    if (version == 7)
        return !trackedGhost && !angelGraph && foldedGiant &&
          codecTag == GiantAnchorV2Tag;
    return version >= 4 && version <= 6 && !trackedGhost &&
      !angelGraph && !foldedGiant;
}

static_assert(packed_codec_matches(
  10, false, false, false, true, false, LinkedCopycatPairV1Tag));
static_assert(packed_codec_matches(
  10, false, true, false, false, true, AngelCopycatGraphV1Tag));
static_assert(!packed_codec_matches(
  10, false, true, false, false, true, AngelGraphV1Tag));
static_assert(!packed_codec_matches(
  9, false, true, false, false, true, AngelGraphV1Tag));

}  // namespace

class TablebaseGenerator {
   public:
    TablebaseGenerator(PieceType attackerType, PieceType secondaryType,
                       Color secondaryColor, std::string output,
                       std::string checkpoint, std::uint32_t checkpointEvery,
                       bool diskBacked, bool trackedGhost,
                       bool linkedCopycatPair,
                       std::uint32_t workerThreads) :
        attackerType_(attackerType), output_(std::move(output)),
        checkpoint_(std::move(checkpoint)), checkpointEvery_(checkpointEvery),
        diskBacked_(diskBacked), trackedGhost_(trackedGhost),
        workerThreads_(workerThreads),
        linkedCopycatPair_(linkedCopycatPair),
        copycatOnly_(attackerType == PieceType::Copycat &&
                     secondaryType == PieceType::Count && !linkedCopycatPair_),
        compoundCopycat_(attackerType == PieceType::Copycat &&
                         secondaryType != PieceType::Count),
        identicalCompoundCopycats_(compoundCopycat_ &&
          secondaryType == PieceType::Copycat && secondaryColor == Color::White),
        secondaryType_((copycatOnly_ || linkedCopycatPair_)
                         ? PieceType::CopycatClone : secondaryType),
        secondaryColor_(secondaryColor),
        fourModels_(secondaryType_ != PieceType::Count),
        identicalExtras_(secondaryType == attackerType && secondaryColor == Color::White),
        primarySubstates_(trackedGhost ? 1 : material_substate_count(
          attackerType, fourModels_, secondaryType_, Color::White,
          secondaryColor_)),
        secondarySubstates_(fourModels_ ? material_substate_count(
          secondaryType_, true, attackerType, secondaryColor_, Color::White) : 1),
        substates_(primarySubstates_ * secondarySubstates_),
        stateCount_(linkedCopycatPair_ ? FourPlacementStateCount
                    : copycatOnly_ ? PlacementStateCount
                    : identicalCompoundCopycats_
                        ? IdenticalCompoundCopycatStateCount * substates_
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

    void self_test_penguin_causal_codec() const {
        if (attackerType_ != PieceType::Penguin &&
            secondaryType_ != PieceType::Penguin)
            return;
        std::uint32_t index = 0;
        if (!fourModels_) {
            index = encode({Color::White, 0, 2, 1, 1});
        }
        else {
            const auto distant = [](PieceType type) {
                return static_cast<std::uint8_t>(
                  type == PieceType::Giant ? 60 : 64);
            };
            const FourState state{
              Color::White, 0, 2,
              attackerType_ == PieceType::Penguin ? std::uint8_t{1}
                                                   : distant(attackerType_),
              secondaryType_ == PieceType::Penguin ?
                (attackerType_ == PieceType::Penguin ? std::uint8_t{64}
                                                     : std::uint8_t{1})
                : distant(secondaryType_)};
            const std::uint32_t placement = identicalExtras_
              ? encode_identical_four_material(state)
              : encode_four_material(state);
            const std::uint32_t primary =
              attackerType_ == PieceType::Penguin ? 1 : 0;
            const std::uint32_t secondary =
              secondaryType_ == PieceType::Penguin &&
              attackerType_ != PieceType::Penguin ? 1 : 0;
            index = placement * substates_ +
                    primary * secondarySubstates_ + secondary;
        }
        Position position;
        if (!make_position_at(index, position) || child_index(position) != index)
            throw std::runtime_error(
              "Penguin partial causal freeze mask does not round trip");
        int penguin = Position::NoPiece;
        for (int id = 0; id < position.piece_count(); ++id)
            if (position.piece(id).type == PieceType::Penguin &&
                position.piece(id).square == 1)
                penguin = id;
        if (penguin == Position::NoPiece || position.piece(penguin).action != 4 ||
            position.piece(0).freezeCount != 1 ||
            position.piece(1).freezeCount != 0)
            throw std::runtime_error(
              "Penguin partial causal freeze mask reconstructed the wrong aura");
        if (attackerType_ == PieceType::Jester &&
            secondaryType_ == PieceType::Penguin &&
            secondaryColor_ == Color::White) {
            // The Penguin on a2 froze both adjacent Kings in the first world.
            // Swapping the hidden a1/c1 King/Jester identities must preserve
            // the frozen public silhouettes, changing target bits 3 -> 6.
            constexpr std::uint32_t First = 151'831'723;
            constexpr std::uint32_t Swapped = 159'471'358;
            Position first, swapped;
            if (primary_jester_alternative(First) != Swapped ||
                primary_jester_alternative(Swapped) != First ||
                !make_primary_jester_world(First, false, first) ||
                !make_primary_jester_world(First, true, swapped) ||
                child_index(first) != First || child_index(swapped) != Swapped ||
                primary_jester_view_key(first) !=
                  primary_jester_view_key(swapped) ||
                primary_jester_decision_markers(first) !=
                  primary_jester_decision_markers(swapped))
                throw std::runtime_error(
                  "Penguin/Jester causal royal-pair regression failed");
            std::cout << "penguinjesterroyalswap first " << First
                      << " swapped " << Swapped << " substate 3 6\n";
        }
        std::cout << "penguincausalfreezemaskok partial_mask 1 action 4\n";
    }

    void self_test_angel_transition_decision() const {
        if (attackerType_ != PieceType::Jester ||
            secondaryType_ != PieceType::Angel)
            return;
        constexpr std::uint32_t LegalDotWitness = 492'966;
        const State state = decode_placement(LegalDotWitness);
        const auto build = [&](bool swapped) {
            Position position;
            position.clear();
            position.add_piece(
              PieceType::King, Color::White,
              swapped ? state.attacker : state.whiteKing);
            position.add_piece(
              PieceType::King, Color::Black, state.blackKing);
            position.add_piece(
              PieceType::Jester, Color::White,
              swapped ? state.whiteKing : state.attacker);
            position.set_side_to_move(state.side);
            for (int id = 0; id < position.piece_count(); ++id)
                position.piece(id).moved = true;
            return position;
        };
        const Position first = build(false);
        const Position second = build(true);
        const DisclosureContext observer{
          jester_observer_color(), false};
        const Move pass{0, 0, 0, MoveKind::Pass, PieceType::Count};
        const std::string firstPublic = transition_observation_key(
          first, pass, first, observer);
        const std::string secondPublic = transition_observation_key(
          second, pass, second, observer);
        const std::string firstObserved = primary_jester_transition_key(
          first, pass, first);
        const std::string secondObserved = primary_jester_transition_key(
          second, pass, second);
        if (state.side != observer.observer ||
            primary_jester_view_key(first) !=
              primary_jester_view_key(second) ||
            primary_jester_decision_markers(first) ==
              primary_jester_decision_markers(second) ||
            firstPublic != secondPublic ||
            firstObserved == secondObserved ||
            firstObserved.find("|nextDecision=") == std::string::npos ||
            secondObserved.find("|nextDecision=") == std::string::npos)
            throw std::runtime_error(
              "Angel/Jester transition lost the mover-private legal-dot split");
        std::cout << "angeljestertransitiondecisionok index "
                  << LegalDotWitness << " residual 0\n";
    }

    static bool terminal_successor(const Position& child) {
        return child.game_over() || child.forced_timeout_winner() ||
               !child.has_real_king(Color::White) ||
               !child.has_real_king(Color::Black) ||
               !child.is_checkmate_possible();
    }

    void self_test_penguin_ghost_terminal_capture() const {
        if (attackerType_ != PieceType::Ghost ||
            secondaryType_ != PieceType::Penguin ||
            secondaryColor_ != Color::Black)
            return;
        constexpr std::uint32_t Witness = 318'855'161;
        Position position;
        if (!make_position_at(Witness, position))
            throw std::runtime_error("Penguin/Ghost terminal witness is invalid");
        bool found = false;
        for_each_legal_successor(position, [&](const Move& move,
                                                const Position& child) {
            if (move.from == 0 && move.to == 9) {
                found = terminal_successor(child) && child.game_over() &&
                        child.winner() == position.side_to_move();
            }
        });
        if (!found)
            throw std::runtime_error(
              "Penguin/Ghost frozen-King capture was not a terminal win");
        std::cout << "penguinghostterminalcaptureok index " << Witness
                  << " residual 0\n";
    }

    void self_test() const {
        self_test_jester_overlay_color_symmetry();
        self_test_penguin_causal_codec();
        self_test_angel_transition_decision();
        self_test_penguin_ghost_terminal_capture();
        if (!parallel_graph_scan_enabled(
              true, stateCount_, true, stateCount_) ||
            parallel_graph_scan_enabled(
              false, stateCount_, true, stateCount_))
            throw std::runtime_error(
              "completed-frontier resume scan scheduling residual");
        std::cout << "parallelresumereversescanok workers "
                  << workerThreads_ << '\n';
        if (linkedCopycatPair_) {
            constexpr std::uint32_t samples = 20'000;
            std::uint64_t transitions = 0;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const std::uint32_t index = static_cast<std::uint32_t>(
                  std::uint64_t(stateCount_) * sample / samples);
                Position position;
                if (!make_position_at(index, position))
                    continue;
                if (!in_class(position) || child_index(position) != index)
                    throw std::runtime_error(
                      "linked Copycat pair position codec is not bijective");
                for (const Move& move : position.legal_moves()) {
                    Position child = position;
                    if (!child.apply_move_unchecked(move))
                        throw std::runtime_error(
                          "linked Copycat pair self-test move failed");
                    ++transitions;
                    if (child.has_real_king(Color::White) &&
                        child.has_real_king(Color::Black) &&
                        child.is_checkmate_possible() && !in_class(child))
                        throw std::runtime_error(
                          "linked Copycat pair left its exact lower codec");
                }
            }
            std::cout << "linkedcopycatpaircodecok samples " << samples
                      << " transitions " << transitions << '\n';
            return;
        }
        if (copycatOnly_ &&
            (encoded_side(0) != Color::White ||
             encoded_side(stateCount_ / 2) != Color::Black))
            throw std::runtime_error(
              "single Copycat side-to-move codec is not partitioned");
        if (compoundCopycat_) {
            constexpr std::uint32_t samples = 20'000;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const std::uint32_t index = static_cast<std::uint32_t>(
                  std::uint64_t(stateCount_) * sample / samples);
                Position position;
                if (make_position_at(index, position) &&
                    (!in_class(position) || child_index(position) != index))
                    throw std::runtime_error(
                      "compound Copycat position codec is not bijective");
            }
            constexpr std::uint32_t transitionSamples = 2'000;
            std::uint64_t checkedTransitions = 0;
            std::uint64_t checkedLinks = 0;
            std::uint64_t splitRescues = 0;
            std::uint64_t orphanDraws = 0;
            for (std::uint32_t sample = 0; sample < transitionSamples; ++sample) {
                const std::uint32_t index = static_cast<std::uint32_t>(
                  std::uint64_t(stateCount_) * sample / transitionSamples);
                Position position;
                if (!make_position_at(index, position))
                    continue;
                int originalMaterial = 0;
                for (int id = 0; id < position.piece_count(); ++id)
                    originalMaterial += position.piece(id).alive &&
                      position.piece(id).type != PieceType::King &&
                      position.piece(id).type != PieceType::Halo;
                for (const Move& move : position.legal_moves()) {
                    Position child = position;
                    if (!child.apply_move_unchecked(move))
                        throw std::runtime_error(
                          "compound Copycat self-test move failed");
                    ++checkedTransitions;
                    checkedLinks += move.kind == MoveKind::Link;
                    int childMaterial = 0;
                    int childCopycats = 0;
                    int childAngels = 0;
                    bool orphanAngel = false;
                    for (int id = 0; id < child.piece_count(); ++id)
                        if (child.piece(id).alive) {
                            childMaterial +=
                              child.piece(id).type != PieceType::King &&
                              child.piece(id).type != PieceType::Halo;
                            childCopycats +=
                              child.piece(id).type == PieceType::Copycat ||
                              child.piece(id).type == PieceType::CopycatClone;
                            childAngels += child.piece(id).type == PieceType::Angel;
                            orphanAngel = orphanAngel ||
                              (child.piece(id).type == PieceType::Angel &&
                               !child.piece(id).onBoard &&
                               (child.piece(id).host < 0 ||
                                child.piece(id).host >= child.piece_count() ||
                                !child.piece(child.piece(id).host).alive));
                        }
                    if (secondaryType_ == PieceType::Angel &&
                        secondaryColor_ == Color::White) {
                        splitRescues += childCopycats == 2 && !childAngels &&
                          child.is_checkmate_possible() && !in_class(child);
                        orphanDraws += !childCopycats && childAngels == 1 &&
                          orphanAngel && !child.is_checkmate_possible();
                    }
                    if (child.has_real_king(Color::White) &&
                        child.has_real_king(Color::Black) &&
                        childMaterial == originalMaterial && !in_class(child))
                        throw std::runtime_error(
                          "retained-material move splits the compound Copycat domain");
                }
            }
            if (secondaryType_ == PieceType::Angel && !checkedLinks)
                throw std::runtime_error(
                  "Copycat/Angel codec found no attachment transition");
            if (secondaryType_ == PieceType::Angel &&
                secondaryColor_ == Color::White &&
                (!splitRescues || !orphanDraws))
                throw std::runtime_error(
                  "same Copycat/Angel codec missed split-rescue or orphan-draw witness");
            std::cout << "compoundcopycatsubstatecodecok samples " << samples
                      << " transition_samples " << transitionSamples
                      << " transitions " << checkedTransitions
                      << " links " << checkedLinks
                      << " split_rescues " << splitRescues
                      << " orphan_draws " << orphanDraws << '\n';
            self_test_jester_royal_codec(samples);
            return;
        }
        if (fourModels_ && !copycatOnly_) {
            self_test_four_codec();
            if (primary_is_giant() || secondary_is_giant())
                self_test_giant_four_codec();
            constexpr std::uint32_t samples = 20'000;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const std::uint32_t index = static_cast<std::uint32_t>(
                  std::uint64_t(stateCount_) * sample / samples);
                Position position;
                if (make_position_at(index, position) &&
                    (!in_class(position) || child_index(position) != index))
                    throw std::runtime_error("four-model substate codec is not bijective");
            }
            if (attackerType_ == PieceType::Angel ||
                secondaryType_ == PieceType::Angel) {
                constexpr std::uint32_t transitionSamples = 2'000;
                std::uint64_t transitions = 0;
                std::uint64_t links = 0;
                for (std::uint32_t sample = 0; sample < transitionSamples;
                     ++sample) {
                    const std::uint32_t index = static_cast<std::uint32_t>(
                      std::uint64_t(stateCount_) * sample / transitionSamples);
                    Position position;
                    if (!make_position_at(index, position))
                        continue;
                    const auto referenceMoves = position.legal_moves();
                    std::vector<Move> foldedMoves;
                    foldedMoves.reserve(referenceMoves.size());
                    for_each_legal_successor(
                      position, [&](const Move& move, const Position& child) {
                        foldedMoves.push_back(move);
                        ++transitions;
                        links += move.kind == MoveKind::Link;
                        const bool retained = child.piece_count() > 3 &&
                          child.piece(2).alive && child.piece(3).alive &&
                          type_matches(attackerType_, child.piece(2).type) &&
                          type_matches(secondaryType_, child.piece(3).type);
                        if (child.has_real_king(Color::White) &&
                            child.has_real_king(Color::Black) && retained &&
                            !in_class(child))
                            throw std::runtime_error(
                              "retained Angel material left its exact codec");
                    });
                    if (foldedMoves != referenceMoves)
                        throw std::runtime_error(
                          "folded Angel successor frontier differs from legal_moves");
                }
                if (!links)
                    throw std::runtime_error(
                      "Angel codec self-test found no attachment transition");
                std::cout << "angelgraphcodecok samples " << samples
                          << " transition_samples " << transitionSamples
                          << " transitions " << transitions
                          << " links " << links << '\n';
            }
            self_test_jester_royal_codec(samples);
            std::cout << "foursubstatecodecok samples " << samples << '\n';
            return;
        }
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            const State state = decode(index);
            if (state.whiteKing == state.blackKing || state.whiteKing == state.attacker ||
                state.blackKing == state.attacker || encode(state) != index)
                throw std::runtime_error("tablebase state codec is not bijective");
            if (attackerType_ == PieceType::Penguin ||
                attackerType_ == PieceType::Angel) {
                Position position;
                if (make_position_at(index, position) &&
                    (!in_class(position) || child_index(position) != index))
                    throw std::runtime_error(
                      "stateful causal/attachment codec is not bijective");
            }
        }
        std::cout << "codecok states " << stateCount_ << '\n';
    }

    template<typename Consumer>
    std::uint32_t for_each_legal_successor(
      const Position& position, Consumer&& consume) const {
        if (position.forced_timeout_winner() ||
            !position.has_real_king(Color::White) ||
            !position.has_real_king(Color::Black) ||
            !position.is_checkmate_possible())
            return 0;

        auto moves = position.pseudo_legal_moves();
        position.annotate_captures(moves);
        const Color mover = position.side_to_move();
        std::uint32_t legal = 0;
        for (const Move& move : moves) {
            Position child = position;
            if (!child.apply_move_unchecked(move) ||
                !child.legal_after_unchecked_move(mover))
                continue;
            ++legal;
            consume(move, child);
        }
        return legal;
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
            for_each_legal_successor(position, [&](const Move&, const Position& child) {
                ++edges;
                if (in_class(child) && child_index(child) >= stateCount_)
                    throw std::runtime_error("dry-run child index exceeds tablebase domain");
            });
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

    [[nodiscard]] bool is_trivial_reachable_position(
      const Position& position) const {
        const std::vector<Move> moves = position.legal_moves();

        // A terminal position with no legal action is trivial only when it is
        // already stalemate. Checkmate remains a substantive tablebase result.
        if (moves.empty())
            return position.is_checkmate_possible() &&
              !position.in_check();

        const auto leaves_material_class = [&](const Position& parent,
                                               const Move& move) {
            Position child = parent;
            return child.apply_move_unchecked(move) && !in_class(child);
        };

        // The side to move can immediately take a hanging character (including
        // the real King), promote, or otherwise simplify to a lower-material
        // dependency. These positions require no tablebase technique.
        if (std::any_of(moves.begin(), moves.end(), [&](const Move& move) {
                return leaves_material_class(position, move);
            }))
            return true;

        // A rejected non-King pseudo-move is an operational pin: the move is
        // generated by the character but illegal because it leaves the real
        // King threatened. Together with direct check, this gates the two-ply
        // fork/skewer/pin test below and avoids classifying unrelated hanging
        // material merely because a capture might occur on the next turn.
        bool checkedOrPinned =
          position.in_check();
        if (!checkedOrPinned) {
            const Color mover = position.side_to_move();
            for (const Move& move : position.pseudo_legal_moves()) {
                const int actor = move.from < Position::BoardSquares
                                  ? position.board_[move.from] : Position::NoPiece;
                if (actor == Position::NoPiece ||
                    position.pieces_[actor].type == PieceType::King)
                    continue;
                Position child = position;
                if (child.apply_move_unchecked(move) &&
                    !child.legal_after_unchecked_move(mover)) {
                    checkedOrPinned = true;
                    break;
                }
            }
        }
        if (!checkedOrPinned)
            return false;

        // A check, fork, skewer, or pin is trivial when every legal response
        // still permits an immediate reply that leaves this material class.
        // The existential reply models the opponent choosing the simplifying
        // capture; the universal response prevents counting avoidable tactics.
        for (const Move& move : moves) {
            Position child = position;
            if (!child.apply_move_unchecked(move))
                return false;
            const std::vector<Move> replies = child.legal_moves();
            if (!std::any_of(replies.begin(), replies.end(), [&](const Move& reply) {
                    return leaves_material_class(child, reply);
                }))
                return false;
        }
        return true;
    }

    void audit_reachability(const std::string& input, bool full,
                            bool turnBoundaryOnly = false) const {
        if (turnBoundaryOnly &&
            attackerType_ != PieceType::Prince &&
            secondaryType_ != PieceType::Prince)
            throw std::runtime_error(
              "turn-boundary reachability requires a Prince material class");
        std::ifstream stream(input, std::ios::binary);
        if (!stream)
            throw std::runtime_error("cannot open packed tablebase for reachability audit");
        std::array<std::uint8_t, 64> header{};
        stream.read(reinterpret_cast<char*>(header.data()), header.size());
        if (stream.gcount() < 40 || std::memcmp(header.data(), "UFTB1\0\0\0", 8) != 0)
            throw std::runtime_error("invalid packed tablebase header");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const auto qword = [&](std::size_t offset) {
            std::uint64_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const std::uint32_t version = word(8);
        const std::uint32_t primary = word(12);
        const std::uint32_t count = word(16);
        const std::uint32_t fileSubstates = word(24);
        const std::uint32_t wdlBytes = word(28);
        const bool foldedGiant = fourModels_ &&
          (primary_is_giant() || secondary_is_giant());
        const bool angelGraph = attackerType_ == PieceType::Angel ||
          secondaryType_ == PieceType::Angel;
        const bool angelCopycatGraph = compoundCopycat_ &&
          secondaryType_ == PieceType::Angel && secondaryColor_ == Color::White;
        const bool codecMatches = packed_codec_matches(
          version, trackedGhost_, angelGraph, foldedGiant, linkedCopycatPair_,
          angelCopycatGraph, qword(56));
        if (!codecMatches ||
            primary != static_cast<std::uint32_t>(attackerType_) ||
            count != stateCount_ || fileSubstates != substates_ || wdlBytes != (count + 3) / 4)
            throw std::runtime_error("packed tablebase does not match requested material class");
        if (version >= 5 &&
            (word(40) != static_cast<std::uint32_t>(secondaryType_) ||
             word(44) != static_cast<std::uint32_t>(secondaryColor_)))
            throw std::runtime_error("packed tablebase secondary material does not match");
        const std::size_t planeOffset = packed_header_size(version);
        stream.seekg(static_cast<std::streamoff>(planeOffset));
        std::vector<std::uint8_t> wdl(wdlBytes);
        stream.read(reinterpret_cast<char*>(wdl.data()), wdl.size());
        if (static_cast<std::size_t>(stream.gcount()) != wdl.size())
            throw std::runtime_error("truncated packed WDL plane");

        using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
        using GiantClassCounts = std::array<Counts, 4>;
        using SniperRankCounts = std::array<Counts, Position::BoardRanks>;
        using AngelSquareCounts = std::array<
          Counts, Position::BoardRanks * (Position::BoardFiles / 2)>;
        using Examples = std::array<std::array<std::uint32_t, 4>, 2>;
        constexpr std::uint32_t Block = 10'000;
        const std::uint32_t workers = std::min(
          workerThreads_, std::max(1u, std::thread::hardware_concurrency()));
        std::atomic<std::uint32_t> next{0};
        std::vector<Counts> local(workers);
        std::vector<Counts> localAll(workers);
        std::vector<Counts> localTrivial(workers);
        std::vector<GiantClassCounts> localGiantPrimaryAll(workers);
        std::vector<GiantClassCounts> localGiantPrimaryExcluded(workers);
        std::vector<GiantClassCounts> localGiantPrimaryTrivial(workers);
        std::vector<GiantClassCounts> localGiantSecondaryAll(workers);
        std::vector<GiantClassCounts> localGiantSecondaryExcluded(workers);
        std::vector<GiantClassCounts> localGiantSecondaryTrivial(workers);
        std::vector<SniperRankCounts> localSniperPrimaryAll(workers);
        std::vector<SniperRankCounts> localSniperPrimaryExcluded(workers);
        std::vector<SniperRankCounts> localSniperPrimaryTrivial(workers);
        std::vector<SniperRankCounts> localSniperSecondaryAll(workers);
        std::vector<SniperRankCounts> localSniperSecondaryExcluded(workers);
        std::vector<SniperRankCounts> localSniperSecondaryTrivial(workers);
        std::vector<AngelSquareCounts> localAngelPrimaryAll(workers);
        std::vector<AngelSquareCounts> localAngelPrimaryExcluded(workers);
        std::vector<AngelSquareCounts> localAngelPrimaryTrivial(workers);
        std::vector<AngelSquareCounts> localAngelSecondaryAll(workers);
        std::vector<AngelSquareCounts> localAngelSecondaryExcluded(workers);
        std::vector<AngelSquareCounts> localAngelSecondaryTrivial(workers);
        std::vector<std::vector<Counts>> localByPrimarySubstate(
          workers, std::vector<Counts>(primarySubstates_));
        std::vector<std::vector<Counts>> localAllByPrimarySubstate(
          workers, std::vector<Counts>(primarySubstates_));
        std::vector<std::vector<Counts>> localTrivialByPrimarySubstate(
          workers, std::vector<Counts>(primarySubstates_));
        std::vector<std::vector<Counts>> localBySecondarySubstate(
          workers, std::vector<Counts>(secondarySubstates_));
        std::vector<std::vector<Counts>> localAllBySecondarySubstate(
          workers, std::vector<Counts>(secondarySubstates_));
        std::vector<std::vector<Counts>> localTrivialBySecondarySubstate(
          workers, std::vector<Counts>(secondarySubstates_));
        std::vector<std::vector<Counts>> localByCombinedSubstate(
          workers, std::vector<Counts>(substates_));
        std::vector<std::vector<Counts>> localAllByCombinedSubstate(
          workers, std::vector<Counts>(substates_));
        std::vector<std::vector<Counts>> localTrivialByCombinedSubstate(
          workers, std::vector<Counts>(substates_));
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
                        const std::size_t primarySubstate =
                          (index % substates_) / secondarySubstates_;
                        const std::size_t secondarySubstate =
                          index % secondarySubstates_;
                        const std::size_t combinedSubstate = index % substates_;
                        if (turnBoundaryOnly &&
                            ((attackerType_ == PieceType::Prince && primarySubstate) ||
                             (secondaryType_ == PieceType::Prince && secondarySubstate)))
                            continue;
                        const Color encodedSide = encoded_side(index);
                        Position position;
                        const bool hasPosition = make_position_at(index, position);
                        bool unreachable = !hasPosition;
                        if (!unreachable && version == 11 &&
                            attackerType_ == PieceType::Devil) {
                            bool admittedDevil = false;
                            for (int id = 0; id < position.piece_count(); ++id) {
                                const PieceState& piece = position.piece(id);
                                if (piece.alive && piece.onBoard &&
                                    piece.type == PieceType::Devil &&
                                    piece.color == Color::White) {
                                    admittedDevil = piece.square /
                                      Position::BoardFiles < 3;
                                    break;
                                }
                            }
                            unreachable = !admittedDevil;
                        }
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
                        const auto primaryGiantClass = hasPosition && primary_is_giant()
                          ? std::optional<std::size_t>(
                              giant_start_class_for_slot(position, true))
                          : std::nullopt;
                        const auto secondaryGiantClass = hasPosition && secondary_is_giant()
                          ? std::optional<std::size_t>(
                              giant_start_class_for_slot(position, false))
                          : std::nullopt;
                        const auto primarySniperRank = hasPosition && primary_is_sniper()
                          ? std::optional<std::size_t>(
                              sniper_relative_rank_for_slot(position, true))
                          : std::nullopt;
                        const auto secondarySniperRank = hasPosition && secondary_is_sniper()
                          ? std::optional<std::size_t>(
                              sniper_relative_rank_for_slot(position, false))
                          : std::nullopt;
                        const auto primaryAngelSquare = hasPosition && primary_is_angel()
                          ? std::optional<std::size_t>(
                              angel_root_square_class_for_slot(position, true))
                          : std::nullopt;
                        const auto secondaryAngelSquare = hasPosition && secondary_is_angel()
                          ? std::optional<std::size_t>(
                              angel_root_square_class_for_slot(position, false))
                          : std::nullopt;
                        ++localAll[worker][side][result];
                        if (primaryGiantClass)
                            ++localGiantPrimaryAll[worker]
                              [*primaryGiantClass][side][result];
                        if (secondaryGiantClass)
                            ++localGiantSecondaryAll[worker]
                              [*secondaryGiantClass][side][result];
                        if (primarySniperRank)
                            ++localSniperPrimaryAll[worker]
                              [*primarySniperRank][side][result];
                        if (secondarySniperRank)
                            ++localSniperSecondaryAll[worker]
                              [*secondarySniperRank][side][result];
                        if (primaryAngelSquare)
                            ++localAngelPrimaryAll[worker]
                              [*primaryAngelSquare][side][result];
                        if (secondaryAngelSquare)
                            ++localAngelSecondaryAll[worker]
                              [*secondaryAngelSquare][side][result];
                        ++localAllByPrimarySubstate[worker][primarySubstate][side][result];
                        ++localAllBySecondarySubstate[worker][secondarySubstate][side][result];
                        ++localAllByCombinedSubstate[worker][combinedSubstate][side][result];
                        if (!unreachable) {
                            if (full && is_trivial_reachable_position(position)) {
                                ++localTrivial[worker][side][result];
                                if (primaryGiantClass)
                                    ++localGiantPrimaryTrivial[worker]
                                      [*primaryGiantClass][side][result];
                                if (secondaryGiantClass)
                                    ++localGiantSecondaryTrivial[worker]
                                      [*secondaryGiantClass][side][result];
                                if (primarySniperRank)
                                    ++localSniperPrimaryTrivial[worker]
                                      [*primarySniperRank][side][result];
                                if (secondarySniperRank)
                                    ++localSniperSecondaryTrivial[worker]
                                      [*secondarySniperRank][side][result];
                                if (primaryAngelSquare)
                                    ++localAngelPrimaryTrivial[worker]
                                      [*primaryAngelSquare][side][result];
                                if (secondaryAngelSquare)
                                    ++localAngelSecondaryTrivial[worker]
                                      [*secondaryAngelSquare][side][result];
                                ++localTrivialByPrimarySubstate[worker][primarySubstate]
                                                                     [side][result];
                                ++localTrivialBySecondarySubstate[worker][secondarySubstate]
                                                                       [side][result];
                                ++localTrivialByCombinedSubstate[worker][combinedSubstate]
                                                                      [side][result];
                            }
                            localExamples[worker][side][result] = std::min(
                              localExamples[worker][side][result], index);
                            continue;
                        }
                        ++local[worker][side][result];
                        if (primaryGiantClass)
                            ++localGiantPrimaryExcluded[worker]
                              [*primaryGiantClass][side][result];
                        if (secondaryGiantClass)
                            ++localGiantSecondaryExcluded[worker]
                              [*secondaryGiantClass][side][result];
                        if (primarySniperRank)
                            ++localSniperPrimaryExcluded[worker]
                              [*primarySniperRank][side][result];
                        if (secondarySniperRank)
                            ++localSniperSecondaryExcluded[worker]
                              [*secondarySniperRank][side][result];
                        if (primaryAngelSquare)
                            ++localAngelPrimaryExcluded[worker]
                              [*primaryAngelSquare][side][result];
                        if (secondaryAngelSquare)
                            ++localAngelSecondaryExcluded[worker]
                              [*secondaryAngelSquare][side][result];
                        ++localByPrimarySubstate[worker][primarySubstate][side][result];
                        ++localBySecondarySubstate[worker][secondarySubstate][side][result];
                        ++localByCombinedSubstate[worker][combinedSubstate][side][result];
                    }
                }
            });
        for (std::thread& task : tasks)
            task.join();
        Counts totals{}, allTotals{}, trivialTotals{};
        GiantClassCounts giantPrimaryAll{}, giantPrimaryExcluded{},
          giantPrimaryTrivial{}, giantSecondaryAll{}, giantSecondaryExcluded{},
          giantSecondaryTrivial{};
        SniperRankCounts sniperPrimaryAll{}, sniperPrimaryExcluded{},
          sniperPrimaryTrivial{}, sniperSecondaryAll{}, sniperSecondaryExcluded{},
          sniperSecondaryTrivial{};
        AngelSquareCounts angelPrimaryAll{}, angelPrimaryExcluded{},
          angelPrimaryTrivial{}, angelSecondaryAll{}, angelSecondaryExcluded{},
          angelSecondaryTrivial{};
        std::vector<Counts> byPrimarySubstate(primarySubstates_);
        std::vector<Counts> allByPrimarySubstate(primarySubstates_);
        std::vector<Counts> trivialByPrimarySubstate(primarySubstates_);
        std::vector<Counts> bySecondarySubstate(secondarySubstates_);
        std::vector<Counts> allBySecondarySubstate(secondarySubstates_);
        std::vector<Counts> trivialBySecondarySubstate(secondarySubstates_);
        std::vector<Counts> byCombinedSubstate(substates_);
        std::vector<Counts> allByCombinedSubstate(substates_);
        std::vector<Counts> trivialByCombinedSubstate(substates_);
        Examples examples{};
        for (auto& side : examples)
            side.fill(std::numeric_limits<std::uint32_t>::max());
        for (const Counts& part : local)
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result)
                    totals[side][result] += part[side][result];
        for (const Counts& part : localAll)
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result)
                    allTotals[side][result] += part[side][result];
        for (const Counts& part : localTrivial)
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result)
                    trivialTotals[side][result] += part[side][result];
        const auto mergeGiantClasses = [](
          GiantClassCounts& target, const std::vector<GiantClassCounts>& parts) {
            for (const GiantClassCounts& part : parts)
                for (std::size_t giantClass = 0; giantClass < target.size(); ++giantClass)
                    for (std::size_t side = 0; side < 2; ++side)
                        for (std::size_t result = 0; result < 4; ++result)
                            target[giantClass][side][result] +=
                              part[giantClass][side][result];
        };
        mergeGiantClasses(giantPrimaryAll, localGiantPrimaryAll);
        mergeGiantClasses(giantPrimaryExcluded, localGiantPrimaryExcluded);
        mergeGiantClasses(giantPrimaryTrivial, localGiantPrimaryTrivial);
        mergeGiantClasses(giantSecondaryAll, localGiantSecondaryAll);
        mergeGiantClasses(giantSecondaryExcluded, localGiantSecondaryExcluded);
        mergeGiantClasses(giantSecondaryTrivial, localGiantSecondaryTrivial);
        const auto mergeSniperRanks = [](
          SniperRankCounts& target, const std::vector<SniperRankCounts>& parts) {
            for (const SniperRankCounts& part : parts)
                for (std::size_t rank = 0; rank < target.size(); ++rank)
                    for (std::size_t side = 0; side < 2; ++side)
                        for (std::size_t result = 0; result < 4; ++result)
                            target[rank][side][result] += part[rank][side][result];
        };
        mergeSniperRanks(sniperPrimaryAll, localSniperPrimaryAll);
        mergeSniperRanks(sniperPrimaryExcluded, localSniperPrimaryExcluded);
        mergeSniperRanks(sniperPrimaryTrivial, localSniperPrimaryTrivial);
        mergeSniperRanks(sniperSecondaryAll, localSniperSecondaryAll);
        mergeSniperRanks(sniperSecondaryExcluded, localSniperSecondaryExcluded);
        mergeSniperRanks(sniperSecondaryTrivial, localSniperSecondaryTrivial);
        const auto mergeAngelSquares = [](
          AngelSquareCounts& target, const std::vector<AngelSquareCounts>& parts) {
            for (const AngelSquareCounts& part : parts)
                for (std::size_t square = 0; square < target.size(); ++square)
                    for (std::size_t side = 0; side < 2; ++side)
                        for (std::size_t result = 0; result < 4; ++result)
                            target[square][side][result] +=
                              part[square][side][result];
        };
        mergeAngelSquares(angelPrimaryAll, localAngelPrimaryAll);
        mergeAngelSquares(angelPrimaryExcluded, localAngelPrimaryExcluded);
        mergeAngelSquares(angelPrimaryTrivial, localAngelPrimaryTrivial);
        mergeAngelSquares(angelSecondaryAll, localAngelSecondaryAll);
        mergeAngelSquares(angelSecondaryExcluded, localAngelSecondaryExcluded);
        mergeAngelSquares(angelSecondaryTrivial, localAngelSecondaryTrivial);
        const auto verifyGiantClasses = [&](
          const GiantClassCounts& all, const GiantClassCounts& excluded,
          const GiantClassCounts& trivial) {
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result) {
                    std::uint64_t admittedSum = 0;
                    std::uint64_t trivialSum = 0;
                    for (std::size_t giantClass = 0;
                         giantClass < GiantStartClassSizes.size(); ++giantClass) {
                        admittedSum += all[giantClass][side][result] -
                                       excluded[giantClass][side][result];
                        trivialSum += trivial[giantClass][side][result];
                    }
                    if (admittedSum != allTotals[side][result] - totals[side][result] ||
                        trivialSum != trivialTotals[side][result])
                        throw std::runtime_error(
                          "Giant start-class reachability conservation residual");
                }
        };
        if (primary_is_giant())
            verifyGiantClasses(
              giantPrimaryAll, giantPrimaryExcluded, giantPrimaryTrivial);
        if (secondary_is_giant())
            verifyGiantClasses(
              giantSecondaryAll, giantSecondaryExcluded, giantSecondaryTrivial);
        const auto verifySniperRanks = [&](
          const SniperRankCounts& all, const SniperRankCounts& excluded,
          const SniperRankCounts& trivial) {
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result) {
                    std::uint64_t admittedSum = 0;
                    std::uint64_t trivialSum = 0;
                    for (std::size_t rank = 0; rank < Position::BoardRanks; ++rank) {
                        admittedSum += all[rank][side][result] -
                                       excluded[rank][side][result];
                        trivialSum += trivial[rank][side][result];
                    }
                    if (admittedSum != allTotals[side][result] - totals[side][result] ||
                        trivialSum != trivialTotals[side][result])
                        throw std::runtime_error(
                          "Sniper root-rank reachability conservation residual");
                }
        };
        if (primary_is_sniper())
            verifySniperRanks(
              sniperPrimaryAll, sniperPrimaryExcluded, sniperPrimaryTrivial);
        if (secondary_is_sniper())
            verifySniperRanks(
              sniperSecondaryAll, sniperSecondaryExcluded, sniperSecondaryTrivial);
        const auto verifyAngelSquares = [&](
          const AngelSquareCounts& all, const AngelSquareCounts& excluded,
          const AngelSquareCounts& trivial) {
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result) {
                    std::uint64_t admittedSum = 0;
                    std::uint64_t trivialSum = 0;
                    for (std::size_t square = 0; square < all.size(); ++square) {
                        admittedSum += all[square][side][result] -
                                       excluded[square][side][result];
                        trivialSum += trivial[square][side][result];
                    }
                    if (admittedSum != allTotals[side][result] - totals[side][result] ||
                        trivialSum != trivialTotals[side][result])
                        throw std::runtime_error(
                          "Angel root-square reachability conservation residual");
                }
        };
        if (primary_is_angel())
            verifyAngelSquares(
              angelPrimaryAll, angelPrimaryExcluded, angelPrimaryTrivial);
        if (secondary_is_angel())
            verifyAngelSquares(
              angelSecondaryAll, angelSecondaryExcluded, angelSecondaryTrivial);
        for (const auto& worker : localByPrimarySubstate)
            for (std::size_t substate = 0; substate < primarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        byPrimarySubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localAllByPrimarySubstate)
            for (std::size_t substate = 0; substate < primarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        allByPrimarySubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localTrivialByPrimarySubstate)
            for (std::size_t substate = 0; substate < primarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        trivialByPrimarySubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localBySecondarySubstate)
            for (std::size_t substate = 0; substate < secondarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        bySecondarySubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localAllBySecondarySubstate)
            for (std::size_t substate = 0; substate < secondarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        allBySecondarySubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localTrivialBySecondarySubstate)
            for (std::size_t substate = 0; substate < secondarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        trivialBySecondarySubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localByCombinedSubstate)
            for (std::size_t substate = 0; substate < substates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        byCombinedSubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localAllByCombinedSubstate)
            for (std::size_t substate = 0; substate < substates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        allByCombinedSubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const auto& worker : localTrivialByCombinedSubstate)
            for (std::size_t substate = 0; substate < substates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        trivialByCombinedSubstate[substate][side][result] +=
                          worker[substate][side][result];
        for (const Examples& part : localExamples)
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result)
                    examples[side][result] = std::min(examples[side][result],
                                                      part[side][result]);
        if (turnBoundaryOnly)
            std::cout << "reachability_scope turn_boundary\n";
        for (std::size_t side = 0; side < 2; ++side)
            std::cout << (full ? "reachability" : "predecessor_safety")
                      << " side " << side
                      << " unknown " << totals[side][0]
                      << " win " << totals[side][1]
                      << " loss " << totals[side][2]
                      << " draw " << totals[side][3] << '\n';
        for (std::size_t side = 0; side < 2; ++side)
            std::cout << (full ? "reachability_total" :
                                      "predecessor_safety_total")
                      << " side " << side
                      << " unknown " << allTotals[side][0]
                      << " win " << allTotals[side][1]
                      << " loss " << allTotals[side][2]
                      << " draw " << allTotals[side][3] << '\n';
        // The original `reachability side` record predates publication and
        // reports the states rejected by the causal predecessor predicate.
        // Its terse name caused human importers to reverse admitted and
        // excluded buckets. Keep that record for versioned-sidecar
        // compatibility, but emit explicit, independently conserved aliases
        // so new finalizers and audits can fail closed on the semantics.
        if (full)
            for (std::size_t side = 0; side < 2; ++side) {
                std::cout << "reachability_excluded side " << side
                          << " unknown " << totals[side][0]
                          << " win " << totals[side][1]
                          << " loss " << totals[side][2]
                          << " draw " << totals[side][3] << '\n';
                std::cout << "reachability_admitted side " << side
                          << " unknown " << allTotals[side][0] - totals[side][0]
                          << " win " << allTotals[side][1] - totals[side][1]
                          << " loss " << allTotals[side][2] - totals[side][2]
                          << " draw " << allTotals[side][3] - totals[side][3]
                          << '\n';
                std::cout << "reachability_trivial side " << side
                          << " unknown " << trivialTotals[side][0]
                          << " win " << trivialTotals[side][1]
                          << " loss " << trivialTotals[side][2]
                          << " draw " << trivialTotals[side][3] << '\n';
            }
        if (full) {
            const auto printGiantClasses = [&](
              const char* slot, const GiantClassCounts& all,
              const GiantClassCounts& excluded,
              const GiantClassCounts& trivial) {
                for (std::size_t giantClass = 0;
                     giantClass < GiantStartClassSizes.size(); ++giantClass)
                    for (std::size_t side = 0; side < 2; ++side) {
                        const auto print = [&](const char* suffix,
                                               const Counts& source) {
                            std::cout << "reachability_" << slot
                                      << "_giant_class" << suffix
                                      << " class " << GiantStartClassSizes[giantClass]
                                      << " side " << side
                                      << " unknown " << source[side][0]
                                      << " win " << source[side][1]
                                      << " loss " << source[side][2]
                                      << " draw " << source[side][3] << '\n';
                        };
                        print("_total", all[giantClass]);
                        print("_excluded", excluded[giantClass]);
                        print("_trivial", trivial[giantClass]);
                    }
            };
            if (primary_is_giant())
                printGiantClasses("primary", giantPrimaryAll,
                                  giantPrimaryExcluded, giantPrimaryTrivial);
            if (secondary_is_giant())
                printGiantClasses("secondary", giantSecondaryAll,
                                  giantSecondaryExcluded, giantSecondaryTrivial);
            const auto printSniperRanks = [&](
              const char* slot, const SniperRankCounts& all,
              const SniperRankCounts& excluded,
              const SniperRankCounts& trivial) {
                for (std::size_t rank = 0; rank < Position::BoardRanks; ++rank)
                    for (std::size_t side = 0; side < 2; ++side) {
                        const auto print = [&](const char* suffix,
                                               const Counts& source) {
                            std::cout << "reachability_" << slot
                                      << "_sniper_rank" << suffix
                                      << " rank " << rank + 1
                                      << " side " << side
                                      << " unknown " << source[side][0]
                                      << " win " << source[side][1]
                                      << " loss " << source[side][2]
                                      << " draw " << source[side][3] << '\n';
                        };
                        print("_total", all[rank]);
                        print("_excluded", excluded[rank]);
                        print("_trivial", trivial[rank]);
                    }
            };
            if (primary_is_sniper())
                printSniperRanks("primary", sniperPrimaryAll,
                                  sniperPrimaryExcluded, sniperPrimaryTrivial);
            if (secondary_is_sniper())
                printSniperRanks("secondary", sniperSecondaryAll,
                                  sniperSecondaryExcluded, sniperSecondaryTrivial);
            const auto printAngelSquares = [&](
              const char* slot, const AngelSquareCounts& all,
              const AngelSquareCounts& excluded,
              const AngelSquareCounts& trivial) {
                for (std::size_t square = 0; square < all.size(); ++square)
                    for (std::size_t side = 0; side < 2; ++side) {
                        const auto print = [&](const char* suffix,
                                               const Counts& source) {
                            const char file = static_cast<char>(
                              'a' + square % (Position::BoardFiles / 2));
                            const std::size_t rank =
                              square / (Position::BoardFiles / 2) + 1;
                            std::cout << "reachability_" << slot
                                      << "_angel_square" << suffix
                                      << " square " << file << rank
                                      << " side " << side
                                      << " unknown " << source[side][0]
                                      << " win " << source[side][1]
                                      << " loss " << source[side][2]
                                      << " draw " << source[side][3] << '\n';
                        };
                        print("_total", all[square]);
                        print("_excluded", excluded[square]);
                        print("_trivial", trivial[square]);
                    }
            };
            if (primary_is_angel())
                printAngelSquares("primary", angelPrimaryAll,
                                   angelPrimaryExcluded, angelPrimaryTrivial);
            if (secondary_is_angel())
                printAngelSquares("secondary", angelSecondaryAll,
                                   angelSecondaryExcluded, angelSecondaryTrivial);
        }
        if (primarySubstates_ > 1)
            for (std::size_t substate = 0; substate < primarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side) {
                    const auto& source = byPrimarySubstate[substate][side];
                    std::cout << (full ? "reachability_primary_substate" :
                                          "predecessor_safety_primary_substate")
                              << " substate " << substate
                              << " side " << side
                              << " unknown " << source[0]
                              << " win " << source[1]
                              << " loss " << source[2]
                              << " draw " << source[3] << '\n';
                    const auto& allSource = allByPrimarySubstate[substate][side];
                    std::cout << (full ? "reachability_primary_substate_total" :
                                          "predecessor_safety_primary_substate_total")
                              << " substate " << substate
                              << " side " << side
                              << " unknown " << allSource[0]
                              << " win " << allSource[1]
                              << " loss " << allSource[2]
                              << " draw " << allSource[3] << '\n';
                    if (full) {
                        const auto& trivialSource =
                          trivialByPrimarySubstate[substate][side];
                        std::cout << "reachability_primary_substate_trivial"
                                  << " substate " << substate
                                  << " side " << side
                                  << " unknown " << trivialSource[0]
                                  << " win " << trivialSource[1]
                                  << " loss " << trivialSource[2]
                                  << " draw " << trivialSource[3] << '\n';
                    }
                }
        if (secondarySubstates_ > 1)
            for (std::size_t substate = 0; substate < secondarySubstates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side) {
                    const auto& source = bySecondarySubstate[substate][side];
                    std::cout << (full ? "reachability_secondary_substate" :
                                          "predecessor_safety_secondary_substate")
                              << " substate " << substate
                              << " side " << side
                              << " unknown " << source[0]
                              << " win " << source[1]
                              << " loss " << source[2]
                              << " draw " << source[3] << '\n';
                    const auto& allSource = allBySecondarySubstate[substate][side];
                    std::cout << (full ? "reachability_secondary_substate_total" :
                                          "predecessor_safety_secondary_substate_total")
                              << " substate " << substate
                              << " side " << side
                              << " unknown " << allSource[0]
                              << " win " << allSource[1]
                              << " loss " << allSource[2]
                              << " draw " << allSource[3] << '\n';
                    if (full) {
                        const auto& trivialSource =
                          trivialBySecondarySubstate[substate][side];
                        std::cout << "reachability_secondary_substate_trivial"
                                  << " substate " << substate
                                  << " side " << side
                                  << " unknown " << trivialSource[0]
                                  << " win " << trivialSource[1]
                                  << " loss " << trivialSource[2]
                                  << " draw " << trivialSource[3] << '\n';
                    }
                }
        if (substates_ > 1)
            for (std::size_t substate = 0; substate < substates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side) {
                    const auto& excluded = byCombinedSubstate[substate][side];
                    const auto& total = allByCombinedSubstate[substate][side];
                    const auto& trivial = trivialByCombinedSubstate[substate][side];
                    const auto print = [&](
                      const char* suffix,
                      const std::array<std::uint64_t, 4>& source) {
                        std::cout << "reachability_combined_substate" << suffix
                                  << " substate " << substate
                                  << " side " << side
                                  << " unknown " << source[0]
                                  << " win " << source[1]
                                  << " loss " << source[2]
                                  << " draw " << source[3] << '\n';
                    };
                    print("", excluded);
                    print("_total", total);
                    if (full)
                        print("_trivial", trivial);
                }
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

    void audit_information_trivial(
      const std::string& input, const std::string& overlay,
      const std::string& expectedSourceSha256,
      const std::string& expectedModelSha256,
      bool transposeInformationSubstates = false) const {
        const auto validSha256 = [](const std::string& value) {
            return value.size() == 64 &&
              std::all_of(value.begin(), value.end(), [](unsigned char byte) {
                  return (byte >= '0' && byte <= '9') ||
                         (byte >= 'a' && byte <= 'f');
              });
        };
        if (!validSha256(expectedSourceSha256) ||
            !validSha256(expectedModelSha256))
            throw std::runtime_error(
              "information trivial audit requires lowercase source/model SHA-256");

        std::ifstream stream(input, std::ios::binary);
        if (!stream)
            throw std::runtime_error(
              "cannot open packed tablebase for information trivial audit");
        std::array<std::uint8_t, 64> header{};
        stream.read(reinterpret_cast<char*>(header.data()), header.size());
        if (stream.gcount() < 40 ||
            std::memcmp(header.data(), "UFTB1\0\0\0", 8) != 0)
            throw std::runtime_error("invalid packed tablebase header");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const auto qword = [&](std::size_t offset) {
            std::uint64_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const std::uint32_t version = word(8);
        const std::uint32_t count = word(16);
        const std::uint32_t fileSubstates = word(24);
        const std::uint32_t wdlBytes = word(28);
        const bool foldedGiant = fourModels_ &&
          (primary_is_giant() || secondary_is_giant());
        const bool angelGraph = attackerType_ == PieceType::Angel ||
          secondaryType_ == PieceType::Angel;
        const bool angelCopycatGraph = compoundCopycat_ &&
          secondaryType_ == PieceType::Angel && secondaryColor_ == Color::White;
        if (!packed_codec_matches(
              version, trackedGhost_, angelGraph, foldedGiant,
              linkedCopycatPair_, angelCopycatGraph, qword(56)) ||
            word(12) != static_cast<std::uint32_t>(attackerType_) ||
            count != stateCount_ || fileSubstates != substates_ ||
            wdlBytes != (count + 3) / 4)
            throw std::runtime_error(
              "packed tablebase does not match information audit material class");
        if (version >= 5 &&
            (word(40) != static_cast<std::uint32_t>(secondaryType_) ||
             word(44) != static_cast<std::uint32_t>(secondaryColor_)))
            throw std::runtime_error(
              "packed tablebase information-audit orientation mismatch");
        const std::size_t planeOffset = packed_header_size(version);
        stream.seekg(static_cast<std::streamoff>(planeOffset));
        std::vector<std::uint8_t> wdl(wdlBytes);
        stream.read(reinterpret_cast<char*>(wdl.data()), wdl.size());
        if (static_cast<std::size_t>(stream.gcount()) != wdl.size())
            throw std::runtime_error("truncated packed WDL plane");

        std::ifstream information(overlay, std::ios::binary);
        if (!information)
            throw std::runtime_error("cannot open information overlay");
        std::array<char, 160> informationHeader{};
        information.read(informationHeader.data(), informationHeader.size());
        if (static_cast<std::size_t>(information.gcount()) !=
              informationHeader.size() ||
            std::memcmp(informationHeader.data(), "UFIW2\0\0\0", 8) != 0)
            throw std::runtime_error("invalid information overlay header");
        const auto informationWord = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, informationHeader.data() + offset,
                        sizeof(value));
            return value;
        };
        if (informationWord(8) != 2 || informationWord(24) != stateCount_ ||
            informationWord(28) != substates_)
            throw std::runtime_error(
              "information overlay does not match the dense codec");
        const std::string sourceSha256(informationHeader.data() + 32, 64);
        const std::string modelSha256(informationHeader.data() + 96, 64);
        if (sourceSha256 != expectedSourceSha256 ||
            modelSha256 != expectedModelSha256)
            throw std::runtime_error(
              "information overlay source/model SHA-256 binding mismatch");
        std::vector<std::uint8_t> flags(stateCount_);
        information.read(reinterpret_cast<char*>(flags.data()), flags.size());
        if (static_cast<std::size_t>(information.gcount()) != flags.size())
            throw std::runtime_error("truncated information overlay flags");
        char trailing = 0;
        if (information.read(&trailing, 1))
            throw std::runtime_error("information overlay has trailing bytes");

        const PieceType overlayPrimary =
          static_cast<PieceType>(informationWord(12));
        const PieceType overlaySecondary =
          static_cast<PieceType>(informationWord(16));
        const bool jesterSemantics = overlayPrimary == PieceType::Jester ||
                                     overlaySecondary == PieceType::Jester;
        const bool ghostSemantics = overlayPrimary == PieceType::Ghost ||
                                    overlaySecondary == PieceType::Ghost;
        if (!jesterSemantics && !ghostSemantics)
            throw std::runtime_error(
              "information overlay has no Jester/Ghost semantics");
        if (jesterSemantics && ghostSemantics)
            throw std::runtime_error(
              "crossed Jester/Ghost overlay needs explicit force semantics");
        if (informationWord(20) > static_cast<std::uint32_t>(Color::Black))
            throw std::runtime_error("information overlay owner color is invalid");
        const Color ownerColor =
          static_cast<Color>(informationWord(20));
        if (transposeInformationSubstates &&
            (attackerType_ != PieceType::Ghost ||
             secondaryType_ == PieceType::Count ||
             primarySubstates_ != 2 || secondarySubstates_ <= 1 ||
             overlayPrimary != PieceType::Ghost ||
             overlaySecondary != secondaryType_))
            throw std::runtime_error(
              "information substate transpose needs a stateful Ghost-primary "
              "material match");

        using Counts = std::array<std::array<std::uint64_t, 4>, 2>;
        using SubstateCounts = std::vector<Counts>;
        using GiantClassCounts = std::array<Counts, 4>;
        using SniperRankCounts = std::array<Counts, Position::BoardRanks>;
        using AngelSquareCounts = std::array<
          Counts, Position::BoardRanks * (Position::BoardFiles / 2)>;
        constexpr std::uint32_t Block = 10'000;
        const std::uint32_t workers = std::min(
          workerThreads_, std::max(1u, std::thread::hardware_concurrency()));
        std::atomic<std::uint32_t> next{0};
        std::atomic<bool> invalid{false};
        std::vector<Counts> localAdmitted(workers);
        std::vector<Counts> localExcluded(workers);
        std::vector<Counts> localTrivial(workers);
        std::vector<GiantClassCounts> localGiantPrimaryAdmitted(workers);
        std::vector<GiantClassCounts> localGiantPrimaryTrivial(workers);
        std::vector<GiantClassCounts> localGiantSecondaryAdmitted(workers);
        std::vector<GiantClassCounts> localGiantSecondaryTrivial(workers);
        std::vector<SniperRankCounts> localSniperPrimaryAdmitted(workers);
        std::vector<SniperRankCounts> localSniperPrimaryTrivial(workers);
        std::vector<SniperRankCounts> localSniperSecondaryAdmitted(workers);
        std::vector<SniperRankCounts> localSniperSecondaryTrivial(workers);
        std::vector<AngelSquareCounts> localAngelPrimaryAdmitted(workers);
        std::vector<AngelSquareCounts> localAngelPrimaryTrivial(workers);
        std::vector<AngelSquareCounts> localAngelSecondaryAdmitted(workers);
        std::vector<AngelSquareCounts> localAngelSecondaryTrivial(workers);
        std::vector<SubstateCounts> localAdmittedBySubstate(
          workers, SubstateCounts(substates_));
        std::vector<SubstateCounts> localExcludedBySubstate(
          workers, SubstateCounts(substates_));
        std::vector<SubstateCounts> localTrivialBySubstate(
          workers, SubstateCounts(substates_));
        std::vector<std::thread> tasks;
        for (std::uint32_t worker = 0; worker < workers; ++worker)
            tasks.emplace_back([&, worker] {
                while (!invalid.load(std::memory_order_relaxed)) {
                    const std::uint32_t begin =
                      next.fetch_add(Block, std::memory_order_relaxed);
                    if (begin >= stateCount_)
                        break;
                    const std::uint32_t end = std::min(stateCount_, begin + Block);
                    for (std::uint32_t index = begin; index < end; ++index) {
                        const std::size_t primarySubstate =
                          (index % substates_) / secondarySubstates_;
                        const std::size_t secondarySubstate =
                          index % secondarySubstates_;
                        if ((attackerType_ == PieceType::Prince && primarySubstate) ||
                            (secondaryType_ == PieceType::Prince && secondarySubstate))
                            continue;
                        // Generic Ghost-primary concrete UFTBs pack [Ghost
                        // visibility][secondary substate].  The information
                        // solvers use the role-logical [public-extra substate]
                        // [Ghost visibility] order.  Keep concrete WDL and
                        // geometry on the raw index and transpose only the
                        // overlay lookup.  The material/header gate above and
                        // the runner's exact source/model allowlist prevent a
                        // stale overlay from opting into this conversion.
                        const std::uint32_t overlayIndex =
                          transposeInformationSubstates
                          ? index - static_cast<std::uint32_t>(index % substates_) +
                              static_cast<std::uint32_t>(
                                secondarySubstate * primarySubstates_ +
                                primarySubstate)
                          : index;
                        const std::uint8_t flag = flags[overlayIndex];
                        if ((flag & ~std::uint8_t{7}) ||
                            ((flag & 1) && (flag & 2)) ||
                            (!(flag & 4) && flag)) {
                            invalid.store(true, std::memory_order_relaxed);
                            break;
                        }
                        const std::size_t side =
                          static_cast<std::size_t>(encoded_side(index));
                        if (!(flag & 4)) {
                            const std::uint32_t result =
                              (wdl[index / 4] >> (2 * (index % 4))) & 3;
                            if (!result) {
                                invalid.store(true, std::memory_order_relaxed);
                                break;
                            }
                            ++localExcluded[worker][side][result];
                            ++localExcludedBySubstate[worker]
                              [index % substates_][side][result];
                            continue;
                        }
                        const bool firstForces = (flag & 1) != 0;
                        const bool secondForces = (flag & 2) != 0;
                        const Color mover = encoded_side(index);
                        const bool moverIsFirst = jesterSemantics
                          ? mover == Color::White : mover == ownerColor;
                        const bool moverForces = moverIsFirst
                          ? firstForces : secondForces;
                        const bool opponentForces = moverIsFirst
                          ? secondForces : firstForces;
                        const std::size_t result = moverForces ? 1
                                                 : opponentForces ? 2 : 3;
                        ++localAdmitted[worker][side][result];
                        ++localAdmittedBySubstate[worker]
                          [index % substates_][side][result];
                        Position position;
                        if (!make_position_at(index, position)) {
                            invalid.store(true, std::memory_order_relaxed);
                            break;
                        }
                        const auto primaryGiantClass = primary_is_giant()
                          ? std::optional<std::size_t>(
                              giant_start_class_for_slot(position, true))
                          : std::nullopt;
                        const auto secondaryGiantClass = secondary_is_giant()
                          ? std::optional<std::size_t>(
                              giant_start_class_for_slot(position, false))
                          : std::nullopt;
                        const auto primarySniperRank = primary_is_sniper()
                          ? std::optional<std::size_t>(
                              sniper_relative_rank_for_slot(position, true))
                          : std::nullopt;
                        const auto secondarySniperRank = secondary_is_sniper()
                          ? std::optional<std::size_t>(
                              sniper_relative_rank_for_slot(position, false))
                          : std::nullopt;
                        const auto primaryAngelSquare = primary_is_angel()
                          ? std::optional<std::size_t>(
                              angel_root_square_class_for_slot(position, true))
                          : std::nullopt;
                        const auto secondaryAngelSquare = secondary_is_angel()
                          ? std::optional<std::size_t>(
                              angel_root_square_class_for_slot(position, false))
                          : std::nullopt;
                        if (primaryGiantClass)
                            ++localGiantPrimaryAdmitted[worker]
                              [*primaryGiantClass][side][result];
                        if (secondaryGiantClass)
                            ++localGiantSecondaryAdmitted[worker]
                              [*secondaryGiantClass][side][result];
                        if (primarySniperRank)
                            ++localSniperPrimaryAdmitted[worker]
                              [*primarySniperRank][side][result];
                        if (secondarySniperRank)
                            ++localSniperSecondaryAdmitted[worker]
                              [*secondarySniperRank][side][result];
                        if (primaryAngelSquare)
                            ++localAngelPrimaryAdmitted[worker]
                              [*primaryAngelSquare][side][result];
                        if (secondaryAngelSquare)
                            ++localAngelSecondaryAdmitted[worker]
                              [*secondaryAngelSquare][side][result];
                        const bool isTrivial =
                          is_trivial_reachable_position(position);
                        if (isTrivial) {
                            ++localTrivial[worker][side][result];
                            if (primaryGiantClass)
                                ++localGiantPrimaryTrivial[worker]
                                  [*primaryGiantClass][side][result];
                            if (secondaryGiantClass)
                                ++localGiantSecondaryTrivial[worker]
                                  [*secondaryGiantClass][side][result];
                            if (primarySniperRank)
                                ++localSniperPrimaryTrivial[worker]
                                  [*primarySniperRank][side][result];
                            if (secondarySniperRank)
                                ++localSniperSecondaryTrivial[worker]
                                  [*secondarySniperRank][side][result];
                            if (primaryAngelSquare)
                                ++localAngelPrimaryTrivial[worker]
                                  [*primaryAngelSquare][side][result];
                            if (secondaryAngelSquare)
                                ++localAngelSecondaryTrivial[worker]
                                  [*secondaryAngelSquare][side][result];
                        }
                        if (isTrivial)
                            ++localTrivialBySubstate[worker]
                              [index % substates_][side][result];
                    }
                }
            });
        for (std::thread& task : tasks)
            task.join();
        if (invalid.load(std::memory_order_relaxed))
            throw std::runtime_error(
              "invalid information flags, concrete WDL, or admitted geometry");

        Counts admitted{}, excluded{}, trivial{};
        GiantClassCounts giantPrimaryAdmitted{}, giantPrimaryTrivial{},
          giantSecondaryAdmitted{}, giantSecondaryTrivial{};
        SniperRankCounts sniperPrimaryAdmitted{}, sniperPrimaryTrivial{},
          sniperSecondaryAdmitted{}, sniperSecondaryTrivial{};
        AngelSquareCounts angelPrimaryAdmitted{}, angelPrimaryTrivial{},
          angelSecondaryAdmitted{}, angelSecondaryTrivial{};
        SubstateCounts admittedBySubstate(substates_);
        SubstateCounts excludedBySubstate(substates_);
        SubstateCounts trivialBySubstate(substates_);
        const auto merge = [](Counts& target, const std::vector<Counts>& parts) {
            for (const Counts& part : parts)
                for (std::size_t side = 0; side < 2; ++side)
                    for (std::size_t result = 0; result < 4; ++result)
                        target[side][result] += part[side][result];
        };
        merge(admitted, localAdmitted);
        merge(excluded, localExcluded);
        merge(trivial, localTrivial);
        const auto mergeGiantClasses = [](
          GiantClassCounts& target, const std::vector<GiantClassCounts>& parts) {
            for (const GiantClassCounts& part : parts)
                for (std::size_t giantClass = 0; giantClass < target.size(); ++giantClass)
                    for (std::size_t side = 0; side < 2; ++side)
                        for (std::size_t result = 0; result < 4; ++result)
                            target[giantClass][side][result] +=
                              part[giantClass][side][result];
        };
        mergeGiantClasses(giantPrimaryAdmitted, localGiantPrimaryAdmitted);
        mergeGiantClasses(giantPrimaryTrivial, localGiantPrimaryTrivial);
        mergeGiantClasses(giantSecondaryAdmitted, localGiantSecondaryAdmitted);
        mergeGiantClasses(giantSecondaryTrivial, localGiantSecondaryTrivial);
        const auto mergeSniperRanks = [](
          SniperRankCounts& target, const std::vector<SniperRankCounts>& parts) {
            for (const SniperRankCounts& part : parts)
                for (std::size_t rank = 0; rank < target.size(); ++rank)
                    for (std::size_t side = 0; side < 2; ++side)
                        for (std::size_t result = 0; result < 4; ++result)
                            target[rank][side][result] += part[rank][side][result];
        };
        mergeSniperRanks(sniperPrimaryAdmitted, localSniperPrimaryAdmitted);
        mergeSniperRanks(sniperPrimaryTrivial, localSniperPrimaryTrivial);
        mergeSniperRanks(sniperSecondaryAdmitted, localSniperSecondaryAdmitted);
        mergeSniperRanks(sniperSecondaryTrivial, localSniperSecondaryTrivial);
        const auto mergeAngelSquares = [](
          AngelSquareCounts& target, const std::vector<AngelSquareCounts>& parts) {
            for (const AngelSquareCounts& part : parts)
                for (std::size_t square = 0; square < target.size(); ++square)
                    for (std::size_t side = 0; side < 2; ++side)
                        for (std::size_t result = 0; result < 4; ++result)
                            target[square][side][result] +=
                              part[square][side][result];
        };
        mergeAngelSquares(angelPrimaryAdmitted, localAngelPrimaryAdmitted);
        mergeAngelSquares(angelPrimaryTrivial, localAngelPrimaryTrivial);
        mergeAngelSquares(angelSecondaryAdmitted, localAngelSecondaryAdmitted);
        mergeAngelSquares(angelSecondaryTrivial, localAngelSecondaryTrivial);
        const auto verifyGiantClasses = [&](
          const GiantClassCounts& classAdmitted,
          const GiantClassCounts& classTrivial) {
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result) {
                    std::uint64_t admittedSum = 0;
                    std::uint64_t trivialSum = 0;
                    for (std::size_t giantClass = 0;
                         giantClass < GiantStartClassSizes.size(); ++giantClass) {
                        admittedSum += classAdmitted[giantClass][side][result];
                        trivialSum += classTrivial[giantClass][side][result];
                    }
                    if (admittedSum != admitted[side][result] ||
                        trivialSum != trivial[side][result])
                        throw std::runtime_error(
                          "information Giant start-class conservation residual");
                }
        };
        if (primary_is_giant())
            verifyGiantClasses(giantPrimaryAdmitted, giantPrimaryTrivial);
        if (secondary_is_giant())
            verifyGiantClasses(giantSecondaryAdmitted, giantSecondaryTrivial);
        const auto verifySniperRanks = [&](
          const SniperRankCounts& rankAdmitted,
          const SniperRankCounts& rankTrivial) {
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result) {
                    std::uint64_t admittedSum = 0;
                    std::uint64_t trivialSum = 0;
                    for (std::size_t rank = 0; rank < Position::BoardRanks; ++rank) {
                        admittedSum += rankAdmitted[rank][side][result];
                        trivialSum += rankTrivial[rank][side][result];
                    }
                    if (admittedSum != admitted[side][result] ||
                        trivialSum != trivial[side][result])
                        throw std::runtime_error(
                          "information Sniper root-rank conservation residual");
                }
        };
        if (primary_is_sniper())
            verifySniperRanks(sniperPrimaryAdmitted, sniperPrimaryTrivial);
        if (secondary_is_sniper())
            verifySniperRanks(sniperSecondaryAdmitted, sniperSecondaryTrivial);
        const auto verifyAngelSquares = [&](
          const AngelSquareCounts& squareAdmitted,
          const AngelSquareCounts& squareTrivial) {
            for (std::size_t side = 0; side < 2; ++side)
                for (std::size_t result = 0; result < 4; ++result) {
                    std::uint64_t admittedSum = 0;
                    std::uint64_t trivialSum = 0;
                    for (std::size_t square = 0;
                         square < squareAdmitted.size(); ++square) {
                        admittedSum += squareAdmitted[square][side][result];
                        trivialSum += squareTrivial[square][side][result];
                    }
                    if (admittedSum != admitted[side][result] ||
                        trivialSum != trivial[side][result])
                        throw std::runtime_error(
                          "information Angel root-square conservation residual");
                }
        };
        if (primary_is_angel())
            verifyAngelSquares(angelPrimaryAdmitted, angelPrimaryTrivial);
        if (secondary_is_angel())
            verifyAngelSquares(angelSecondaryAdmitted, angelSecondaryTrivial);
        const auto mergeSubstates = [](
          SubstateCounts& target,
          const std::vector<SubstateCounts>& parts) {
            for (const SubstateCounts& part : parts)
                for (std::size_t substate = 0; substate < target.size(); ++substate)
                    for (std::size_t side = 0; side < 2; ++side)
                        for (std::size_t result = 0; result < 4; ++result)
                            target[substate][side][result] +=
                              part[substate][side][result];
        };
        mergeSubstates(admittedBySubstate, localAdmittedBySubstate);
        mergeSubstates(excludedBySubstate, localExcludedBySubstate);
        mergeSubstates(trivialBySubstate, localTrivialBySubstate);
        for (std::size_t side = 0; side < 2; ++side) {
            std::uint64_t conserved = 0;
            for (std::size_t result = 1; result < 4; ++result) {
                conserved += admitted[side][result] + excluded[side][result];
                if (trivial[side][result] > admitted[side][result])
                    throw std::runtime_error(
                      "information trivial subset exceeds admitted result");
            }
            const std::uint64_t princeFactor =
              (attackerType_ == PieceType::Prince ? 2 : 1) *
              (secondaryType_ == PieceType::Prince ? 2 : 1);
            if (conserved != stateCount_ / (2 * princeFactor))
                throw std::runtime_error(
                  "information reachability conservation residual");
            const auto print = [&](const char* label, const Counts& counts) {
                std::cout << label << " side " << side
                          << " unknown " << counts[side][0]
                          << " win " << counts[side][1]
                          << " loss " << counts[side][2]
                          << " draw " << counts[side][3] << '\n';
            };
            print("information_reachability_admitted", admitted);
            print("information_reachability_excluded", excluded);
            print("information_reachability_trivial", trivial);
        }
        const auto printSubstate = [&](const char* label,
                                       const SubstateCounts& counts) {
            for (std::size_t substate = 0; substate < substates_; ++substate)
                for (std::size_t side = 0; side < 2; ++side)
                    std::cout << label << " substate " << substate
                              << " side " << side
                              << " unknown " << counts[substate][side][0]
                              << " win " << counts[substate][side][1]
                              << " loss " << counts[substate][side][2]
                              << " draw " << counts[substate][side][3] << '\n';
        };
        for (std::size_t side = 0; side < 2; ++side)
            for (std::size_t result = 0; result < 4; ++result) {
                const auto sum = [&](const SubstateCounts& counts) {
                    std::uint64_t total = 0;
                    for (const Counts& substate : counts)
                        total += substate[side][result];
                    return total;
                };
                if (sum(admittedBySubstate) != admitted[side][result] ||
                    sum(excludedBySubstate) != excluded[side][result] ||
                    sum(trivialBySubstate) != trivial[side][result])
                    throw std::runtime_error(
                      "information substate reporting conservation residual");
            }
        printSubstate("information_reachability_substate_admitted",
                      admittedBySubstate);
        printSubstate("information_reachability_substate_excluded",
                      excludedBySubstate);
        printSubstate("information_reachability_substate_trivial",
                      trivialBySubstate);
        const auto printGiantClasses = [&](
          const char* slot, const GiantClassCounts& classAdmitted,
          const GiantClassCounts& classTrivial) {
            for (std::size_t giantClass = 0;
                 giantClass < GiantStartClassSizes.size(); ++giantClass)
                for (std::size_t side = 0; side < 2; ++side) {
                    const auto print = [&](const char* suffix,
                                           const Counts& source) {
                        std::cout << "information_reachability_" << slot
                                  << "_giant_class_" << suffix
                                  << " class " << GiantStartClassSizes[giantClass]
                                  << " side " << side
                                  << " unknown " << source[side][0]
                                  << " win " << source[side][1]
                                  << " loss " << source[side][2]
                                  << " draw " << source[side][3] << '\n';
                    };
                    print("admitted", classAdmitted[giantClass]);
                    print("trivial", classTrivial[giantClass]);
                }
        };
        if (primary_is_giant())
            printGiantClasses("primary", giantPrimaryAdmitted,
                              giantPrimaryTrivial);
        if (secondary_is_giant())
            printGiantClasses("secondary", giantSecondaryAdmitted,
                              giantSecondaryTrivial);
        const auto printSniperRanks = [&](
          const char* slot, const SniperRankCounts& rankAdmitted,
          const SniperRankCounts& rankTrivial) {
            for (std::size_t rank = 0; rank < Position::BoardRanks; ++rank)
                for (std::size_t side = 0; side < 2; ++side) {
                    const auto print = [&](const char* suffix,
                                           const Counts& source) {
                        std::cout << "information_reachability_" << slot
                                  << "_sniper_rank_" << suffix
                                  << " rank " << rank + 1
                                  << " side " << side
                                  << " unknown " << source[side][0]
                                  << " win " << source[side][1]
                                  << " loss " << source[side][2]
                                  << " draw " << source[side][3] << '\n';
                    };
                    print("admitted", rankAdmitted[rank]);
                    print("trivial", rankTrivial[rank]);
                }
        };
        if (primary_is_sniper())
            printSniperRanks("primary", sniperPrimaryAdmitted,
                              sniperPrimaryTrivial);
        if (secondary_is_sniper())
            printSniperRanks("secondary", sniperSecondaryAdmitted,
                              sniperSecondaryTrivial);
        const auto printAngelSquares = [&](
          const char* slot, const AngelSquareCounts& squareAdmitted,
          const AngelSquareCounts& squareTrivial) {
            for (std::size_t square = 0;
                 square < squareAdmitted.size(); ++square)
                for (std::size_t side = 0; side < 2; ++side) {
                    const auto print = [&](const char* suffix,
                                           const Counts& source) {
                        const char file = static_cast<char>(
                          'a' + square % (Position::BoardFiles / 2));
                        const std::size_t rank =
                          square / (Position::BoardFiles / 2) + 1;
                        std::cout << "information_reachability_" << slot
                                  << "_angel_square_" << suffix
                                  << " square " << file << rank
                                  << " side " << side
                                  << " unknown " << source[side][0]
                                  << " win " << source[side][1]
                                  << " loss " << source[side][2]
                                  << " draw " << source[side][3] << '\n';
                    };
                    print("admitted", squareAdmitted[square]);
                    print("trivial", squareTrivial[square]);
                }
        };
        if (primary_is_angel())
            printAngelSquares("primary", angelPrimaryAdmitted,
                               angelPrimaryTrivial);
        if (secondary_is_angel())
            printAngelSquares("secondary", angelSecondaryAdmitted,
                               angelSecondaryTrivial);
        if (attackerType_ == PieceType::Prince ||
            secondaryType_ == PieceType::Prince)
            std::cout << "information_reachability_scope turn_boundary\n";
        std::cout << "information_reachability_binding source_sha256 "
                  << sourceSha256 << " model_sha256 " << modelSha256
                  << " primary " << informationWord(12)
                  << " secondary " << informationWord(16)
                  << " owner_color " << informationWord(20)
                  << " states " << stateCount_
                  << " substates " << substates_
                  << " semantics "
                  << (jesterSemantics ? "white-black-forces-v1" :
                                        "ghost-owner-observer-forces-v1")
                  << " conservation_residual 0\n";
        if (transposeInformationSubstates)
            std::cout << "information_reachability_plane_order "
                      << "concrete_primary_secondary overlay_secondary_primary\n";
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
        std::array<std::uint8_t, 64> header{};
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
        if (version < 4 || version > 7 || version == 7 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Jester) ||
            count != stateCount_ || fileSubstates != substates_ ||
            wdlBytes != (count + 3) / 4 ||
            (version >= 5 &&
             (word(40) != static_cast<std::uint32_t>(secondaryType_) ||
              word(44) != static_cast<std::uint32_t>(secondaryColor_))))
            throw std::runtime_error("concrete Jester tablebase does not match codec");
        const std::size_t planeOffset = packed_header_size(version);
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
                                  const std::string& lowerModelSha256,
                                  const std::string& lowerExtraOverlay,
                                  const std::string& lowerExtraSourceSha256,
                                  const std::string& lowerExtraModelSha256,
                                  const std::string& sourceSha256,
                                  const std::string& modelSha256,
                                  const std::string& overlayOutput,
                                  const std::string& scratchDirectory) const {
        if (!has_single_ivory_jester())
            throw std::runtime_error(
              "exact primary-Jester solver requires exactly one Ivory Jester");
        const Color ownerColor = jester_owner_color();
        const Color observerColor = ~ownerColor;

        std::ifstream stream(input, std::ios::binary);
        if (!stream)
            throw std::runtime_error("cannot open concrete Jester tablebase");
        std::array<std::uint8_t, 64> header{};
        stream.read(reinterpret_cast<char*>(header.data()), header.size());
        if (stream.gcount() < 40 || std::memcmp(header.data(), "UFTB1\0\0\0", 8) != 0)
            throw std::runtime_error("invalid concrete Jester tablebase header");
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const auto qword = [&](std::size_t offset) {
            std::uint64_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        };
        const std::uint32_t version = word(8);
        const std::uint32_t count = word(16);
        const std::uint32_t wdlBytes = word(28);
        const bool foldedGiant = fourModels_ &&
          (primary_is_giant() || secondary_is_giant());
        const bool angelGraph = attackerType_ == PieceType::Angel ||
          secondaryType_ == PieceType::Angel;
        const bool angelCopycatGraph = compoundCopycat_ &&
          secondaryType_ == PieceType::Angel && secondaryColor_ == Color::White;
        if (!packed_codec_matches(
              version, trackedGhost_, angelGraph, foldedGiant,
              linkedCopycatPair_, angelCopycatGraph, qword(56)) ||
            word(12) != static_cast<std::uint32_t>(attackerType_) ||
            count != stateCount_ || word(24) != substates_ ||
            wdlBytes != (count + 3) / 4 ||
            (version >= 5 &&
             (word(40) != static_cast<std::uint32_t>(secondaryType_) ||
              word(44) != static_cast<std::uint32_t>(secondaryColor_))))
            throw std::runtime_error("concrete Jester tablebase does not match codec");
        const std::size_t planeOffset = packed_header_size(version);
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

        // Forced Prince continuations are not legal turn-boundary roots, so
        // they remain excluded from the causal reachability totals below.
        // They are nevertheless genuine internal information-game nodes: the
        // Prince owner observes the compulsory second-step dots before making
        // that choice. Keep those nodes in the paired graph without making
        // them appear reachable as standalone positions in the ledger.
        std::vector<std::int8_t> graphNodeCache(stateCount_, 0);
        const std::uint32_t informationWorkers = std::min(
          workerThreads_, std::max(1u, std::thread::hardware_concurrency()));
        constexpr std::uint32_t FrontierBlock = 4096;
        std::atomic<std::uint32_t> nextFrontier{0};
        std::vector<std::future<void>> frontierTasks;
        frontierTasks.reserve(informationWorkers);
        for (std::uint32_t worker = 0; worker < informationWorkers; ++worker)
            frontierTasks.push_back(std::async(std::launch::async,
              [&, this] {
                  while (true) {
                      const std::uint32_t begin = nextFrontier.fetch_add(
                        FrontierBlock, std::memory_order_relaxed);
                      if (begin >= stateCount_)
                          break;
                      const std::uint32_t end = std::min(
                        stateCount_, static_cast<std::uint32_t>(begin + FrontierBlock));
                      for (std::uint32_t index = begin; index < end; ++index) {
                          Position position;
                          const bool reconstructed = make_position_at(index, position);
                          graphNodeCache[index] = reconstructed &&
                            (position.has_forced_action()
                              ? !position.legal_moves().empty()
                              : position.ordinary_predecessor_king_safe());
                      }
                  }
              }));
        for (auto& task : frontierTasks)
            task.get();

        std::vector<std::uint32_t> pairs;
        pairs.reserve(stateCount_ / 2);
        std::vector<std::int32_t> pairForIndex(stateCount_, -1);
        // 1 is an admitted royal pair; 2 is a pair split by the observer's
        // private pre-decision legal dots. Each representative is independent,
        // and the serial compaction below preserves the canonical pair order.
        std::vector<std::uint8_t> pairClass(stateCount_, 0);
        std::atomic<std::uint32_t> nextPairCandidate{0};
        frontierTasks.clear();
        for (std::uint32_t worker = 0; worker < informationWorkers; ++worker)
            frontierTasks.push_back(std::async(std::launch::async,
              [&, this] {
                  while (true) {
                      const std::uint32_t begin = nextPairCandidate.fetch_add(
                        FrontierBlock, std::memory_order_relaxed);
                      if (begin >= stateCount_)
                          break;
                      const std::uint32_t end = std::min(
                        stateCount_, static_cast<std::uint32_t>(begin + FrontierBlock));
                      for (std::uint32_t index = begin; index < end; ++index) {
                          if (!graphNodeCache[index])
                              continue;
                          const std::uint32_t other =
                            primary_jester_alternative(index);
                          if (index >= other || !graphNodeCache[other])
                              continue;
                          Position first, second;
                          if (!make_primary_jester_world(index, false, first) ||
                              !make_primary_jester_world(index, true, second))
                              throw std::runtime_error(
                                "admitted royal assignment failed reconstruction");
                          if (primary_jester_view_key(first) !=
                              primary_jester_view_key(second))
                              continue;
                          if (first.side_to_move() == observerColor &&
                              primary_jester_decision_markers(first) !=
                                primary_jester_decision_markers(second))
                              pairClass[index] = 2;
                          else
                              pairClass[index] = 1;
                      }
                  }
              }));
        for (auto& task : frontierTasks)
            task.get();
        std::array<std::uint64_t, 2> dotSplitPairs{};
        const auto frontierStart = std::chrono::steady_clock::now();
        for (std::uint32_t index = 0; index < stateCount_; ++index) {
            if (!pairClass[index])
                continue;
            const std::uint32_t other = primary_jester_alternative(index);
            if (pairClass[index] == 2) {
                ++dotSplitPairs[static_cast<std::size_t>(observerColor)];
                continue;
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
                  << " observer_dot_split_pairs "
                  << dotSplitPairs[static_cast<std::size_t>(observerColor)]
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
          lowerOverlay, lowerSourceSha256, lowerModelSha256,
          PieceType::Count, Color::White, ownerColor);
        const JesterInformationOverlay lowerPromotedQueen(
          lowerExtraOverlay, lowerExtraSourceSha256, lowerExtraModelSha256,
          PieceType::Queen, secondaryColor_, ownerColor);
        const auto concrete_position_forces = [&](const Position& position,
                                                  Color target) {
            if (position.game_over()) {
                const auto winner = position.winner();
                return winner && *winner == target;
            }
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

        struct InformationGraphStats {
            std::uint64_t observationChecks = 0;
            std::uint64_t emptyCommonActionSets = 0;
            std::uint64_t lowerPairProbes = 0;
            std::uint64_t lowerSingletonProbes = 0;
            std::uint64_t lowerTerminalGroups = 0;
            std::uint64_t lowerOwnerOverlayDifferences = 0;
            std::uint32_t firstEmptyCommonActionSet =
              std::numeric_limits<std::uint32_t>::max();
        };
        const std::uint32_t graphWorkers = std::min<std::uint32_t>(
          informationWorkers, static_cast<std::uint32_t>(pairs.size()));
        std::vector<InformationGraphStats> graphStats(graphWorkers);
        std::mutex definitionMutex;
        std::mutex progressMutex;
        std::atomic<std::size_t> nextGraphPair{0};
        std::atomic<std::uint64_t> completedGraphPairs{0};
        std::atomic<std::uint64_t> progressObservations{0};
        const auto graphStart = std::chrono::steady_clock::now();
        const auto processPair = [&](std::size_t pairId,
                                     InformationGraphStats& stats) {
            auto& observationChecks = stats.observationChecks;
            auto& emptyCommonActionSets = stats.emptyCommonActionSets;
            auto& lowerPairProbes = stats.lowerPairProbes;
            auto& lowerSingletonProbes = stats.lowerSingletonProbes;
            auto& lowerTerminalGroups = stats.lowerTerminalGroups;
            auto& lowerOwnerOverlayDifferences =
              stats.lowerOwnerOverlayDifferences;
            auto& firstEmptyCommonActionSet = stats.firstEmptyCommonActionSet;
            const std::uint32_t representative = pairs[pairId];
            const std::array<std::uint32_t, 2> worlds{
              representative, primary_jester_alternative(representative)};
            std::array<Position, 2> positions;
            if (!make_primary_jester_world(representative, false, positions[0]) ||
                !make_primary_jester_world(representative, true, positions[1]))
                throw std::runtime_error("paired information node is invalid");

            if (positions.front().game_over()) {
                std::lock_guard<std::mutex> lock(definitionMutex);
                for (std::size_t world = 0; world < worlds.size(); ++world) {
                    const auto winner = positions[world].winner();
                    const std::array<InformationToken, 1> child{{boolean_token(
                      winner && *winner == ownerColor)}};
                    ivory->define_or(static_cast<std::uint32_t>(2 * pairId + world),
                                     child.data(), child.size());
                }
                const auto winner = positions.front().winner();
                const std::array<InformationToken, 1> child{{boolean_token(
                  winner && *winner == observerColor)}};
                onyx->define_or(static_cast<std::uint32_t>(pairId),
                                child.data(), child.size());
                return;
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
                std::optional<JesterInformationOverlay::PairForces>
                  lowerPairForces;
                const JesterInformationOverlay* lowerPairOverlay = nullptr;
                if (!sameClass.empty()) {
                    if (sameClass.size() == 1)
                        groupOnyx = boolean_token(exact_index_forces(
                          sameClass.front(), observerColor));
                    else if (sameClass.size() == 2) {
                        const std::int32_t pair = pairForIndex[sameClass.front()];
                        if (pair < 0 || pairForIndex[sameClass.back()] != pair) {
                            Position firstChild, secondChild;
                            const bool firstOk = make_position_at(
                              sameClass.front(), firstChild);
                            const bool secondOk = make_position_at(
                              sameClass.back(), secondChild);
                            throw std::runtime_error(
                              "observation produced a noncanonical royal pair: " +
                              std::to_string(sameClass.front()) + " alternative " +
                              std::to_string(primary_jester_alternative(
                                sameClass.front())) + " pair " +
                              std::to_string(pair) + " upn " +
                              (firstOk ? firstChild.upn() : "invalid") + "; " +
                              std::to_string(sameClass.back()) + " alternative " +
                              std::to_string(primary_jester_alternative(
                                sameClass.back())) + " pair " +
                              std::to_string(pairForIndex[sameClass.back()]) +
                              " upn " +
                              (secondOk ? secondChild.upn() : "invalid"));
                        }
                        groupOnyx = static_cast<InformationToken>(pair);
                        ambiguous = true;
                    }
                    else
                        throw std::runtime_error(
                          "Jester belief has more than two royal assignments");
                }
                else {
                    bool blackForces = false;
                    if (external.size() == 1) {
                        if (lower.concrete_world(*external.front()))
                            ++lowerSingletonProbes;
                        blackForces = concrete_position_forces(
                          *external.front(), observerColor);
                    }
                    else if (external.size() == 2) {
                        const bool firstTerminal = external[0]->game_over();
                        const bool secondTerminal = external[1]->game_over();
                        if (firstTerminal || secondTerminal) {
                            // Capturing an indistinguishable royal silhouette
                            // can leave different concrete material in the two
                            // assignments while both outcomes are already the
                            // same publicly announced terminal result. Such a
                            // bucket is a constant, not a fresh K+Jester-v-K
                            // belief. A terminal/ongoing mix or distinct winner
                            // would have different transition observations and
                            // is therefore a projection defect.
                            if (!firstTerminal || !secondTerminal ||
                                external[0]->winner() != external[1]->winner())
                                throw std::runtime_error(
                                  "one lower Jester observation mixes public terminal outcomes");
                            ++lowerTerminalGroups;
                            const auto winner = external[0]->winner();
                            blackForces = winner && *winner == observerColor;
                        }
                        else {
                            // Preserve a continuing canonical royal pair as a
                            // narrowed belief. Pawn promotion is the one closed
                            // transition that retains a fourth piece here, so
                            // cross-probe its exact Jester+Queen overlay rather
                            // than resetting either world to a maximal root.
                            ++lowerPairProbes;
                            lowerPairOverlay = lower.concrete_world(*external[0])
                              ? &lower : lowerPromotedQueen.concrete_world(
                                  *external[0]) ? &lowerPromotedQueen : nullptr;
                            if (!lowerPairOverlay)
                                throw std::runtime_error(
                                  "paired lower Jester successor has unsupported material");
                            lowerPairForces = lowerPairOverlay->pair_forces(
                              *external[0], *external[1], observerColor);
                            blackForces = lowerPairForces->uninformed;
                        }
                    }
                    else
                        throw std::runtime_error(
                          "lower Jester observation is neither singleton nor pair");
                    groupOnyx = boolean_token(blackForces);
                }

                for (const auto [world, edgeIndex] : members) {
                    MoveEdge& edge = edges[world][edgeIndex];
                    edge.onyx = groupOnyx;
                    if (edge.child.sameClass) {
                        edge.ivory = ambiguous
                          ? white_variable(edge.child.index)
                          : boolean_token(exact_index_forces(
                              edge.child.index, ownerColor));
                    }
                    else if (lowerPairForces) {
                        const auto actual = lowerPairOverlay->concrete_world(
                          edge.child.external);
                        if (!actual || actual->owner != ownerColor)
                            throw std::runtime_error(
                              "paired lower Jester edge lost its Ivory owner");
                        std::size_t member = lowerPairForces->indices.size();
                        for (std::size_t candidate = 0;
                             candidate < lowerPairForces->indices.size(); ++candidate)
                            if (lowerPairForces->indices[candidate] == actual->index)
                                member = candidate;
                        if (member == lowerPairForces->indices.size())
                            throw std::runtime_error(
                              "lower Jester edge is outside its exact pair");
                        if (lowerPairForces->owner[member] !=
                            concrete_position_forces(
                              edge.child.external, ownerColor))
                            ++lowerOwnerOverlayDifferences;
                        edge.ivory = boolean_token(
                          lowerPairForces->owner[member]);
                    }
                    else
                        edge.ivory = boolean_token(concrete_position_forces(
                          edge.child.external, ownerColor));
                }
            }

            std::lock_guard<std::mutex> lock(definitionMutex);
            if (positions.front().side_to_move() == ownerColor) {
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

        };
        constexpr std::size_t GraphBlock = 128;
        std::vector<std::future<void>> graphTasks;
        graphTasks.reserve(graphWorkers);
        for (std::uint32_t worker = 0; worker < graphWorkers; ++worker)
            graphTasks.push_back(std::async(std::launch::async,
              [&, worker] {
                  InformationGraphStats& stats = graphStats[worker];
                  while (true) {
                      const std::size_t begin = nextGraphPair.fetch_add(
                        GraphBlock, std::memory_order_relaxed);
                      if (begin >= pairs.size())
                          break;
                      const std::size_t end = std::min(
                        pairs.size(), begin + GraphBlock);
                      for (std::size_t pairId = begin; pairId < end; ++pairId) {
                          const std::uint64_t observationsBefore =
                            stats.observationChecks;
                          processPair(pairId, stats);
                          progressObservations.fetch_add(
                            stats.observationChecks - observationsBefore,
                            std::memory_order_relaxed);
                          const std::uint64_t completed =
                            completedGraphPairs.fetch_add(
                              1, std::memory_order_relaxed) + 1;
                          if (completed % 100'000 == 0) {
                              std::lock_guard<std::mutex> progressLock(progressMutex);
                              const double elapsed = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - graphStart).count();
                              std::cout << "information_graph pairs " << completed
                                        << '/' << pairs.size() << " observations "
                                        << progressObservations.load(
                                             std::memory_order_relaxed)
                                        << " elapsed " << elapsed << "s\n"
                                        << std::flush;
                          }
                      }
                  }
              }));
        for (auto& task : graphTasks)
            task.get();
        InformationGraphStats graphTotals;
        for (const InformationGraphStats& stats : graphStats) {
            graphTotals.observationChecks += stats.observationChecks;
            graphTotals.emptyCommonActionSets += stats.emptyCommonActionSets;
            graphTotals.lowerPairProbes += stats.lowerPairProbes;
            graphTotals.lowerSingletonProbes += stats.lowerSingletonProbes;
            graphTotals.lowerTerminalGroups += stats.lowerTerminalGroups;
            graphTotals.lowerOwnerOverlayDifferences +=
              stats.lowerOwnerOverlayDifferences;
            graphTotals.firstEmptyCommonActionSet = std::min(
              graphTotals.firstEmptyCommonActionSet,
              stats.firstEmptyCommonActionSet);
        }
        const std::uint64_t emptyCommonActionSets =
          graphTotals.emptyCommonActionSets;
        const std::uint64_t lowerPairProbes = graphTotals.lowerPairProbes;
        const std::uint64_t lowerSingletonProbes =
          graphTotals.lowerSingletonProbes;
        const std::uint64_t lowerTerminalGroups = graphTotals.lowerTerminalGroups;
        const std::uint64_t lowerOwnerOverlayDifferences =
          graphTotals.lowerOwnerOverlayDifferences;
        const std::uint32_t firstEmptyCommonActionSet =
          graphTotals.firstEmptyCommonActionSet;
        std::cout << "information_uniform_actions empty_sets "
                  << emptyCommonActionSets << " first_index ";
        if (firstEmptyCommonActionSet == std::numeric_limits<std::uint32_t>::max())
            std::cout << "none\n";
        else
            std::cout << firstEmptyCommonActionSet << '\n';
        std::cout << "information_lower_jester pair_probes "
                  << lowerPairProbes << " singleton_probes "
                  << lowerSingletonProbes << " owner_overlay_differences "
                  << lowerOwnerOverlayDifferences << " terminal_groups "
                  << lowerTerminalGroups << '\n';

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
                ivoryForces = exact_index_forces(index, ownerColor);
                onyxForces = exact_index_forces(index, observerColor);
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
            const bool whiteForces = ownerColor == Color::White
              ? ivoryForces : onyxForces;
            const bool blackForces = ownerColor == Color::Black
              ? ivoryForces : onyxForces;
            epistemicFlags[index] = 4 | (whiteForces ? 1 : 0) |
                                    (blackForces ? 2 : 0);
            const Color mover = encoded_side(index);
            const bool moverWins = mover == Color::White
              ? whiteForces : blackForces;
            const bool moverLoses = mover == Color::White
              ? blackForces : whiteForces;
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
        const auto scan = [&](const char* phase, std::uint32_t scanBegin,
                              bool parallel, auto&& action) {
            if (!parallel) {
                for (std::uint32_t index = scanBegin; index < stateCount_; ++index) {
                    action(index, false);
                    if ((index + 1) % progressEvery == 0)
                        progress(phase, index + 1, start);
                }
                return;
            }
            constexpr std::uint32_t Block = 10'000;
            const std::uint32_t workers = std::min(
              workerThreads_, std::max(1u, std::thread::hardware_concurrency()));
            std::cout << phase << " workers " << workers << '\n' << std::flush;
            std::atomic<std::uint32_t> next{scanBegin};
            std::atomic<std::uint32_t> completed{scanBegin};
            std::atomic<std::uint32_t> nextReport{
              static_cast<std::uint32_t>((scanBegin / progressEvery + 1) * progressEvery)};
            std::atomic<bool> failed{false};
            std::exception_ptr failure;
            std::mutex failureMutex;
            std::vector<std::thread> tasks;
            for (std::uint32_t worker = 0; worker < workers; ++worker)
                tasks.emplace_back([&] {
                    try {
                        while (!failed.load(std::memory_order_relaxed)) {
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
                                     report,
                                     static_cast<std::uint32_t>(report + progressEvery),
                                     std::memory_order_relaxed)) {}
                            if (done >= report && report <= stateCount_)
                                progress(phase, report, start);
                        }
                    }
                    catch (...) {
                        failed.store(true, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (!failure)
                            failure = std::current_exception();
                    }
                });
            for (auto& task : tasks)
                task.join();
            if (failure)
                std::rethrow_exception(failure);
        };
        // A checkpointed frontier remains serial so that its processed prefix
        // is deterministic and restartable.  Reverse construction has no
        // incremental checkpoint of its own: if interrupted it is rebuilt
        // from the already authenticated frontier.  It can therefore use the
        // worker pool even when checkpoint loading was required.  The former
        // shared condition accidentally serialized every completed-frontier
        // resume, leaving the requested workers idle for the entire replay.
        scan("frontier", begin, parallel_graph_scan_enabled(
               false, checkpointEvery_, diskBacked_, stateCount_),
             [&](std::uint32_t index, bool atomic) {
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
            using Edge = std::remove_reference_t<decltype(predecessors[0])>;
            offsets[0] = 0;
            for (std::uint32_t index = 0; index < stateCount_; ++index)
                offsets[index + 1] = static_cast<Offset>(offsets[index] +
                                                         predecessorCounts_[index]);
            constexpr Edge SameSideMask = predecessor_same_side_mask<Edge>();
            if (std::uint64_t(stateCount_) >= std::uint64_t(SameSideMask))
                throw std::runtime_error("tablebase state index exceeds packed edge capacity");
            scan("reverse", 0, parallel_graph_scan_enabled(
                   true, checkpointEvery_, diskBacked_, stateCount_),
                 [&](std::uint32_t index, bool atomic) {
                analyze_node(index, false, [&](std::uint32_t child, bool sameSide) {
                    const Offset cursor = atomic
                      ? __atomic_fetch_add(&offsets[child], Offset{1}, __ATOMIC_RELAXED)
                      : offsets[child]++;
                    predecessors[cursor] = pack_predecessor<Edge>(index, sameSide);
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
                    const Edge packedParent = predecessors[edge];
                    const std::uint32_t parentIndex = predecessor_index(packedParent);
                    const bool sameSide = predecessor_same_side(packedParent);
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
        const auto solve = [&](auto offsetZero, auto edgeZero) {
            using Offset = decltype(offsetZero);
            using Edge = decltype(edgeZero);
            if (diskBacked_ || stateCount_ >= 300'000'000) {
                MappedArray<Offset> offsets(checkpoint_ + ".offsets", stateCount_ + 1ULL);
                MappedArray<Edge> predecessors(
                  checkpoint_ + ".predecessors", edgeCount);
                solve_arrays(offsets, predecessors);
            }
            else {
                std::vector<Offset> offsets(stateCount_ + 1);
                std::vector<Edge> predecessors(edgeCount);
                solve_arrays(offsets, predecessors);
            }
        };
        const bool wideOffsets =
          edgeCount > std::numeric_limits<std::uint32_t>::max();
        const bool wideEdges =
          stateCount_ >= predecessor_same_side_mask<std::uint32_t>();
        if (!wideOffsets && !wideEdges)
            solve(std::uint32_t{}, std::uint32_t{});
        else if (!wideOffsets)
            solve(std::uint32_t{}, std::uint64_t{});
        else if (!wideEdges)
            solve(std::uint64_t{}, std::uint32_t{});
        else
            solve(std::uint64_t{}, std::uint64_t{});
        verify_solution();
        write_output(edgeCount);
        progress("complete", stateCount_, start);
    }

    void census_devil_frontier(std::uint32_t depth, std::uint32_t shard,
                               std::uint32_t shardCount,
                               std::uint64_t stateLimit) const {
        if (attackerType_ != PieceType::Devil || !depth ||
            !shardCount || shard >= shardCount || !stateLimit)
            throw std::runtime_error("invalid Devil closure census arguments");

        // A Devil closure state used to be an owning std::string.  At tens of
        // millions of states its object/node overhead and repeated Minion
        // sorting dominated both memory and CPU.  The board is only 80
        // squares, so retain the exact same information in two words:
        //   word 0: Minions on squares 0..63
        //   word 1: Minions on 64..79, then side/kings/Devil/cooldown/secondary
        // This remains exact for any number of simultaneously live Minions;
        // in particular it does not turn the currently observed maximum of
        // two into a model assumption.
        struct DevilKey {
            std::uint64_t low = 0;
            std::uint64_t highPacked = 0;

            bool operator==(const DevilKey& other) const {
                return low == other.low && highPacked == other.highPacked;
            }

            bool operator<(const DevilKey& other) const {
                return highPacked < other.highPacked ||
                  (highPacked == other.highPacked && low < other.low);
            }
        };
        struct DevilKeyHash {
            std::size_t operator()(const DevilKey& key) const {
                std::uint64_t value = key.low ^
                  (key.highPacked + 0x9e3779b97f4a7c15ULL +
                   (key.low << 6) + (key.low >> 2));
                value ^= value >> 30;
                value *= 0xbf58476d1ce4e5b9ULL;
                value ^= value >> 27;
                value *= 0x94d049bb133111ebULL;
                value ^= value >> 31;
                return static_cast<std::size_t>(value);
            }
        };
        // The census reaches hundreds of millions of keys.  A node-based
        // unordered_set spends more memory on allocation metadata, pointers,
        // and buckets than on the exact 16-byte key, which made otherwise idle
        // CPUs unusable behind the per-worker RAM gate.  Keep the identical
        // key and equality semantics in a zero-initialized, open-addressed
        // table.  DevilKey::highPacked uses fewer than 48 bits, so bit 63 is a
        // lossless occupied marker.  A maximum load of 80% bounds probing while
        // requiring 2^28 slots (4 GiB) for the 200-million-state census cap.
        class FlatDevilSet {
           public:
            explicit FlatDevilSet(std::uint64_t limit) {
                if (!limit || limit > std::numeric_limits<std::size_t>::max())
                    throw std::runtime_error("invalid flat Devil set limit");
                const std::uint64_t target = limit + (limit + 3) / 4;
                while (capacity_ < target) {
                    if (capacity_ >
                        std::numeric_limits<std::size_t>::max() / 2)
                        throw std::runtime_error("flat Devil set too large");
                    capacity_ <<= 1;
                }
                if (capacity_ >
                    std::uint64_t{std::numeric_limits<std::uint32_t>::max()} + 1)
                    throw std::runtime_error("flat Devil set index too wide");
                slots_ = static_cast<DevilKey*>(
                  std::calloc(capacity_, sizeof(DevilKey)));
                if (!slots_)
                    throw std::bad_alloc();
            }

            FlatDevilSet(const FlatDevilSet&) = delete;
            FlatDevilSet& operator=(const FlatDevilSet&) = delete;

            ~FlatDevilSet() { std::free(slots_); }

            std::pair<bool, std::uint32_t> insert(const DevilKey& key) {
                std::size_t index = DevilKeyHash{}(key) & (capacity_ - 1);
                for (;;) {
                    DevilKey& slot = slots_[index];
                    if (!(slot.highPacked & occupied_marker())) {
                        slot.low = key.low;
                        slot.highPacked = key.highPacked | occupied_marker();
                        ++size_;
                        return {true, static_cast<std::uint32_t>(index)};
                    }
                    if (slot.low == key.low &&
                        (slot.highPacked & ~occupied_marker()) == key.highPacked)
                        return {false, static_cast<std::uint32_t>(index)};
                    index = (index + 1) & (capacity_ - 1);
                }
            }

            std::size_t size() const { return size_; }

            DevilKey key_at(std::uint32_t index) const {
                if (index >= capacity_ ||
                    !(slots_[index].highPacked & occupied_marker()))
                    throw std::runtime_error("invalid flat Devil set index");
                return {slots_[index].low,
                        slots_[index].highPacked & ~occupied_marker()};
            }

           private:
            static constexpr std::uint64_t occupied_marker() {
                return std::uint64_t{1} << 63;
            }
            DevilKey* slots_ = nullptr;
            std::size_t capacity_ = 1;
            std::size_t size_ = 0;
        };
        constexpr std::uint64_t DevilMinionHighMask = 0xffffULL;
        constexpr int DevilSideShift = 16;
        constexpr int DevilWhiteKingShift = 17;
        constexpr int DevilBlackKingShift = 24;
        constexpr int DevilSquareShift = 31;
        constexpr int DevilCooldownShift = 38;
        constexpr int DevilSecondaryShift = 40;
        constexpr std::uint64_t DevilSquareMask = 0x7fULL;
        constexpr std::uint64_t DevilNoSecondary = Position::BoardSquares;

        const auto encodeDevil = [&](const Position& position) {
            int whiteKing = Position::NoSquare;
            int blackKing = Position::NoSquare;
            int devilSquare = Position::NoSquare;
            int devilCooldown = 0;
            int secondarySquare = Position::NoSquare;
            DevilKey minions;
            for (int id = 0; id < position.piece_count(); ++id) {
                const PieceState& piece = position.piece(id);
                if (!piece.alive || !piece.onBoard)
                    continue;
                if (piece.type == PieceType::King) {
                    (piece.color == Color::White ? whiteKing : blackKing) =
                      piece.square;
                }
                else if (piece.type == PieceType::Devil &&
                         piece.color == Color::White &&
                         devilSquare == Position::NoSquare) {
                    devilSquare = piece.square;
                    devilCooldown = piece.cooldown;
                }
                else if (piece.type == PieceType::Minion &&
                         piece.color == Color::White) {
                    if (piece.square < 64)
                        minions.low |= std::uint64_t{1} << piece.square;
                    else
                        minions.highPacked |=
                          std::uint64_t{1} << (piece.square - 64);
                }
                else if (fourModels_ && piece.type == secondaryType_ &&
                         piece.color == secondaryColor_ &&
                         secondarySquare == Position::NoSquare) {
                    secondarySquare = piece.square;
                }
                else {
                    return std::optional<DevilKey>{};
                }
            }
            if (whiteKing == Position::NoSquare ||
                blackKing == Position::NoSquare ||
                devilSquare == Position::NoSquare || devilCooldown > 3 ||
                devilSquare / Position::BoardFiles >= 3 ||
                (fourModels_ != (secondarySquare != Position::NoSquare)))
                return std::optional<DevilKey>{};
            const auto build = [&](bool reflected) {
                const auto square = [&](int value) -> std::uint64_t {
                    return reflected
                      ? horizontal_reflection(static_cast<std::uint8_t>(value))
                      : static_cast<std::uint64_t>(value);
                };
                DevilKey key{minions.low,
                             minions.highPacked & DevilMinionHighMask};
                if (reflected) {
                    key = {};
                    std::uint64_t values = minions.low;
                    while (values) {
                        const int value = __builtin_ctzll(values);
                        values &= values - 1;
                        const int target = static_cast<int>(square(value));
                        if (target < 64)
                            key.low |= std::uint64_t{1} << target;
                        else
                            key.highPacked |=
                              std::uint64_t{1} << (target - 64);
                    }
                    values = minions.highPacked & DevilMinionHighMask;
                    while (values) {
                        const int value = 64 + __builtin_ctzll(values);
                        values &= values - 1;
                        const int target = static_cast<int>(square(value));
                        if (target < 64)
                            key.low |= std::uint64_t{1} << target;
                        else
                            key.highPacked |=
                              std::uint64_t{1} << (target - 64);
                    }
                }
                key.highPacked |=
                  static_cast<std::uint64_t>(position.side_to_move() ==
                                             Color::Black) << DevilSideShift;
                key.highPacked |= square(whiteKing) << DevilWhiteKingShift;
                key.highPacked |= square(blackKing) << DevilBlackKingShift;
                key.highPacked |= square(devilSquare) << DevilSquareShift;
                key.highPacked |=
                  static_cast<std::uint64_t>(devilCooldown) <<
                  DevilCooldownShift;
                key.highPacked |=
                  (secondarySquare == Position::NoSquare
                    ? DevilNoSecondary : square(secondarySquare)) <<
                  DevilSecondaryShift;
                return key;
            };
            DevilKey direct = build(false);
            DevilKey reflected = build(true);
            return std::optional<DevilKey>(std::min(direct, reflected));
        };
        const auto decodeDevil = [&](const DevilKey& key) {
            const auto field = [&](int shift) {
                return static_cast<int>((key.highPacked >> shift) &
                                        DevilSquareMask);
            };
            Position position;
            position.clear();
            position.add_piece(PieceType::King, Color::White,
                               field(DevilWhiteKingShift));
            position.add_piece(PieceType::King, Color::Black,
                               field(DevilBlackKingShift));
            const int devil = position.add_piece(
              PieceType::Devil, Color::White, field(DevilSquareShift));
            position.piece(devil).cooldown = static_cast<int>(
              (key.highPacked >> DevilCooldownShift) & 3ULL);
            const int secondary = field(DevilSecondaryShift);
            if (secondary != static_cast<int>(DevilNoSecondary))
                position.add_piece(secondaryType_, secondaryColor_,
                                   secondary);
            std::uint64_t minions = key.low;
            while (minions) {
                const int square = __builtin_ctzll(minions);
                minions &= minions - 1;
                position.add_piece(PieceType::Minion, Color::White, square);
            }
            minions = key.highPacked & DevilMinionHighMask;
            while (minions) {
                const int square = 64 + __builtin_ctzll(minions);
                minions &= minions - 1;
                position.add_piece(PieceType::Minion, Color::White, square);
            }
            position.set_side_to_move(
              ((key.highPacked >> DevilSideShift) & 1ULL)
                ? Color::Black : Color::White);
            for (int id = 0; id < position.piece_count(); ++id)
                position.piece(id).moved = true;
            return position;
        };

        FlatDevilSet visited(stateLimit);
        // Frontier entries are 32-bit slots in the exact visited table rather
        // than duplicate 128-bit keys.  Current and next layers are disjoint,
        // so even at the cap their combined worst case is 800 MB instead of
        // 3.2 GB.
        std::vector<std::uint32_t> frontier;
        for (std::uint32_t index = shard; index < stateCount_;
             index += shardCount) {
            Position position;
            if (!make_position_at(index, position))
                continue;
            std::optional<DevilKey> key = encodeDevil(position);
            if (key) {
                const auto [inserted, slot] = visited.insert(*key);
                if (inserted)
                    frontier.push_back(slot);
            }
        }
        std::uint64_t boundary = 0;
        std::size_t maxMinions = 0;
        bool capped = visited.size() >= stateLimit;
        std::cout << "devil_frontier shard " << shard << '/' << shardCount
                  << " depth 0 frontier " << frontier.size()
                  << " visited " << visited.size() << " boundary 0 capped "
                  << capped << '\n' << std::flush;
        for (std::uint32_t ply = 1; ply <= depth && !frontier.empty() && !capped;
             ++ply) {
            std::vector<std::uint32_t> next;
            for (const std::uint32_t slot : frontier) {
                const DevilKey key = visited.key_at(slot);
                Position position = decodeDevil(key);
                for (const Move& move : position.legal_moves()) {
                    Position child = position;
                    if (!child.apply_move_unchecked(move))
                        throw std::runtime_error("Devil closure legal move failed");
                    std::optional<DevilKey> childKey = encodeDevil(child);
                    if (!childKey) {
                        ++boundary;
                        continue;
                    }
                    const std::size_t childMinions =
                      static_cast<std::size_t>(__builtin_popcountll(
                        childKey->low)) +
                      static_cast<std::size_t>(__builtin_popcountll(
                        childKey->highPacked & DevilMinionHighMask));
                    maxMinions = std::max(maxMinions, childMinions);
                    const auto [inserted, childSlot] =
                      visited.insert(*childKey);
                    if (inserted) {
                        next.push_back(childSlot);
                        if (visited.size() >= stateLimit) {
                            capped = true;
                            break;
                        }
                    }
                }
                if (capped)
                    break;
            }
            frontier.swap(next);
            std::cout << "devil_frontier shard " << shard << '/' << shardCount
                      << " depth " << ply << " frontier " << frontier.size()
                      << " visited " << visited.size() << " boundary "
                      << boundary << " max_minions " << maxMinions
                      << " capped " << capped << '\n' << std::flush;
        }
        std::cout << "DEVIL_CLOSURE_CENSUS_OK shard " << shard << '/'
                  << shardCount << " visited " << visited.size()
                  << " boundary " << boundary << " max_minions "
                  << maxMinions << " capped " << capped << '\n';
    }

    // Solve one of the twelve disjoint fixed-Devil-square graphs used by the
    // spawned-only Devil tablebase.  Unlike census_devil_frontier(), the
    // partition key is invariant under every in-class transition: the Devil
    // cannot move, and its canonical starting square remains latent provenance
    // after capture while surviving Minions finish their automatic runs.  The
    // closure is global within that partition, edges are regenerated against
    // the completed exact index, WDL/DTW is solved by retrograde, and every
    // node receives an exhaustive Bellman replay before a root fragment is
    // emitted.
    void solve_devil_spawned_square(int fixedSquare,
                                    std::uint64_t stateLimit,
                                    std::uint64_t hashCapacity,
                                    const std::string& workDirectory) const {
        // The exact key already reserves a seven-bit secondary square.  Admit
        // ordinary companion pieces whose complete material state is their
        // type/color/square; stateful companions need a wider substate codec
        // and fail closed here instead of being silently flattened.
        if (attackerType_ != PieceType::Devil ||
            (fourModels_ && (secondarySubstates_ != 1 ||
                             secondaryType_ == PieceType::Devil)) ||
            fixedSquare < 0 || fixedSquare >= Position::BoardSquares ||
            fixedSquare % Position::BoardFiles >= Position::BoardFiles / 2 ||
            fixedSquare / Position::BoardFiles >= 3 || !stateLimit ||
            workDirectory.empty())
            throw std::runtime_error("invalid spawned-only Devil solve arguments");
        if (!hashCapacity)
            hashCapacity = stateLimit;
        if (hashCapacity > stateLimit)
            throw std::runtime_error(
              "spawned-only Devil hash capacity exceeds state limit");

        // At most five spawned Minions coexist.  Rank that sparse subset among
        // all size-0..5 subsets of 80 squares (25 bits), rank the ordered Kings
        // in 13 bits, and retain secondary square, cooldown, side, and Devil
        // presence in another 11 bits.  The fixed Devil square is partition
        // provenance and is restored from fixedSquare.  Thus the complete
        // logical proof key fits in 49 bits and seven persistent bytes rather
        // than the former 14/16.  At the fail-closed 12-billion-state gate this
        // removes 84/108 GB from the random-read plane without weakening the
        // state model.
        struct __attribute__((packed)) DevilKey {
            std::uint64_t value : 56;
            bool operator==(const DevilKey& other) const {
                return value == other.value;
            }
        };
        static_assert(sizeof(DevilKey) == 7,
                      "spawned-only Devil proof key must remain seven bytes");
        struct DevilKeyHash {
            std::uint64_t operator()(const DevilKey& key) const {
                std::uint64_t value = key.value;
                value ^= value >> 30;
                value *= 0xbf58476d1ce4e5b9ULL;
                value ^= value >> 27;
                value *= 0x94d049bb133111ebULL;
                value ^= value >> 31;
                return value;
            }
        };
        struct Checkpoint {
            std::uint32_t version = 0;
            std::uint64_t size = 0;
            std::uint64_t frontier = 0;
            std::uint32_t ply = 0;
            std::uint64_t limit = 0;
        };
        const std::string prefix = workDirectory + "/devil-" +
          std::to_string(fixedSquare);
        const std::string metadataPath = prefix + ".closure";
        const std::string frontierPath = prefix + ".frontier";
        const std::string keyPath = prefix + ".keys";
        const std::string fragmentPath = prefix + ".roots";
        if (::mkdir(workDirectory.c_str(), 0700) != 0 && errno != EEXIST)
            throw std::runtime_error("cannot create spawned-only Devil work directory");

        const auto load_checkpoint = [&]() -> std::optional<Checkpoint> {
            std::ifstream stream(metadataPath, std::ios::binary);
            if (!stream)
                return std::nullopt;
            std::array<char, 8> magic{};
            std::uint32_t version = 0, square = 0;
            std::uint64_t limit = 0;
            Checkpoint result;
            stream.read(magic.data(), magic.size());
            stream.read(reinterpret_cast<char*>(&version), sizeof(version));
            stream.read(reinterpret_cast<char*>(&square), sizeof(square));
            stream.read(reinterpret_cast<char*>(&limit), sizeof(limit));
            stream.read(reinterpret_cast<char*>(&result.size), sizeof(result.size));
            stream.read(reinterpret_cast<char*>(&result.frontier), sizeof(result.frontier));
            stream.read(reinterpret_cast<char*>(&result.ply), sizeof(result.ply));
            const std::array<char, 8> expected{{'U','F','D','V','C','P','1','\0'}};
            // Version 4 binds the seven-byte combinatorial proof-key layout.
            // Earlier layouts are accepted only through the explicit,
            // source-preserving migration path.
            if (!stream || magic != expected || version != 4 ||
                square != static_cast<std::uint32_t>(fixedSquare) ||
                limit > stateLimit || result.size > limit)
                throw std::runtime_error("spawned-only Devil checkpoint residual");
            result.version = version;
            result.limit = limit;
            return result;
        };
        const auto saved = load_checkpoint();
        if (saved && saved->size >= hashCapacity)
            throw std::runtime_error(
              "spawned-only Devil retained checkpoint exceeds hash capacity");
        bool create = !saved;
        PersistentMappedArray<DevilKey> keys(keyPath, stateLimit, create);
        // A retained-index rebuild consumes the committed proof keys in dense
        // order, whereas closure expansion probes them randomly on hash
        // fingerprint matches.  Tell both the VM and filesystem about that
        // phase transition.  Otherwise Linux readahead amplifies a sparse
        // seven-byte equality check into unnecessary storage traffic, which
        // starves the parallel expansion workers on bandwidth-bound hosts.
        if (saved)
            keys.advise_sequential();
        // The hash slots are a disposable acceleration index, not proof data.
        // Rebuilding them from the committed dense key prefix on restart is
        // both exact and dramatically cheaper during normal execution than
        // synchronizing a sparse multi-gigabyte mapping after every BFS layer.
        if (stateLimit >= (std::uint64_t{1} << 40))
            throw std::runtime_error(
              "spawned-only Devil state limit exceeds packed hash geometry");
        std::uint32_t slotBits = 0;
        for (std::uint64_t capacity = 1; capacity <= stateLimit;
             capacity <<= 1)
            ++slotBits;
        constexpr std::uint32_t StripeCount = 4096;
        // V19 sizes the disposable hash for the reviewed current layer rather
        // than the larger fail-closed proof-state limit.  A too-small hash
        // capacity stops without committing the partial layer, so operators
        // can raise only this restart-time cache envelope while the durable
        // state limit and checkpoint remain unchanged.  This is especially
        // valuable for retained C1, whose 5.1B committed states otherwise
        // paid the anonymous-memory cost of a 16B worst-case proof gate.
        // The table still targets 90% occupancy.  The four-bit
        // fingerprint keeps the extra packed-slot probes in anonymous RAM
        // while exact-key reads still occur only for the matching 1/16
        // candidates.  At a 16B gate this releases about 10 GiB for the
        // retained key cache, which is materially more valuable than the few
        // additional in-RAM probes on the memory-bound r8gd hosts.
        const std::uint64_t desiredSlots =
          hashCapacity + (hashCapacity + 7) / 8;
        // Each stripe starts on a packed-word boundary, but its length need
        // not be a power of two.  Rounding the whole table to a power of two
        // made a 16B-state campaign allocate 2^35 35-bit slots (about
        // 150 GiB) when 18B slots (about 74 GiB) provide the intended 90%
        // load factor.  The excess mapping evicted the retained key prefix
        // and converted a parallel hash rebuild into random EBS reads.
        const std::uint64_t stripeAlignment =
          64 / std::gcd<std::uint64_t>(64, slotBits);
        const std::uint64_t desiredStripe =
          (desiredSlots + StripeCount - 1) / StripeCount;
        const std::uint64_t stripeSize =
          ((desiredStripe + stripeAlignment - 1) / stripeAlignment) *
          stripeAlignment;
        const std::uint64_t slotCount = stripeSize * StripeCount;
        const std::uint64_t slotWordCount =
          (slotCount * slotBits + 63) / 64;
        VolatileMappedArray<std::uint64_t> slots(slotWordCount);
        // An eight-bit hash fingerprint prevents an occupied probe from
        // fetching a random seven-byte proof key unless the candidate can
        // actually be equal.  Exact equality is still checked on every
        // fingerprint match, so collisions cannot hide a duplicate. V21
        // uses one byte per slot to reject 255/256 unrelated candidates.
        // Besides cutting false proof-key reads by another factor of four,
        // the byte-aligned load removes the cross-word packed-bit path from
        // every hash probe.  The extra two bits consume about 5 GiB at an
        // 18B-state envelope and remain inside the reviewed 224-GiB cgroup;
        // exact equality is still authoritative on every fingerprint match.
        constexpr std::uint32_t FingerprintBits = 8;
        constexpr std::uint64_t FingerprintMask =
          (std::uint64_t{1} << FingerprintBits) - 1;
        VolatileMappedArray<std::uint8_t> fingerprints(slotCount);
        const std::uint64_t slotMask =
          (std::uint64_t{1} << slotBits) - 1;
        // Hash construction is stripe-locked, and every stripe contains an
        // aligned number of slots so its packed bit span ends on a word
        // boundary.  locate() runs only after construction.
        // Exact bit packing is therefore race-free while avoiding the padding
        // of even the earlier five-byte representation: a 12B campaign needs
        // 34 bits per slot, saving another 12 GiB at 2^34 slots.
        const auto load_slot = [&](std::uint64_t index) {
            const std::uint64_t bit = index * slotBits;
            const std::uint64_t word = bit >> 6;
            const std::uint32_t shift = static_cast<std::uint32_t>(bit & 63);
            std::uint64_t value = slots[word] >> shift;
            if (shift + slotBits > 64)
                value |= slots[word + 1] << (64 - shift);
            return value & slotMask;
        };
        const auto store_slot = [&](std::uint64_t index, std::uint64_t value) {
            if (value > slotMask)
                throw std::runtime_error(
                  "spawned-only Devil packed hash index overflow");
            const std::uint64_t bit = index * slotBits;
            const std::uint64_t word = bit >> 6;
            const std::uint32_t shift = static_cast<std::uint32_t>(bit & 63);
            slots[word] = (slots[word] & ~(slotMask << shift)) |
                          (value << shift);
            if (shift + slotBits > 64) {
                const std::uint32_t spill = shift + slotBits - 64;
                const std::uint64_t spillMask =
                  (std::uint64_t{1} << spill) - 1;
                slots[word + 1] =
                  (slots[word + 1] & ~spillMask) |
                  ((value >> (64 - shift)) & spillMask);
            }
        };
        const auto fingerprint_for = [](std::uint64_t hash) {
            return static_cast<std::uint8_t>(
              (hash ^ (hash >> 17) ^ (hash >> 37) ^ (hash >> 53)) &
              FingerprintMask);
        };
        const auto load_fingerprint = [&](std::uint64_t index) {
            return fingerprints[index];
        };
        const auto store_fingerprint = [&](std::uint64_t index,
                                           std::uint8_t value) {
            if (value > FingerprintMask)
                throw std::runtime_error(
                  "spawned-only Devil fingerprint overflow");
            fingerprints[index] = value;
        };
        if (slotCount < StripeCount || slotCount % StripeCount)
            throw std::runtime_error("spawned-only Devil hash geometry residual");
        auto stripeLocks = std::make_unique<std::mutex[]>(StripeCount);
        std::atomic<std::uint64_t> keyCount{saved ? saved->size : 0};

        const auto locate = [&](const DevilKey& key) -> std::optional<std::uint64_t> {
            const std::uint64_t hash = DevilKeyHash{}(key);
            const std::uint8_t fingerprint = fingerprint_for(hash);
            const std::uint32_t stripe = static_cast<std::uint32_t>(hash) &
                                         (StripeCount - 1);
            const std::uint64_t base = std::uint64_t(stripe) * stripeSize;
            std::uint64_t offset = (hash >> 12) % stripeSize;
            for (std::uint64_t probe = 0; probe < stripeSize; ++probe) {
                const std::uint64_t value = load_slot(base + offset);
                if (!value)
                    return std::nullopt;
                const std::uint64_t index = value - 1;
                if (load_fingerprint(base + offset) == fingerprint &&
                    keys[index] == key)
                    return index;
                if (++offset == stripeSize)
                    offset = 0;
            }
            return std::nullopt;
        };
        const auto insert = [&](const DevilKey& key) {
            const std::uint64_t hash = DevilKeyHash{}(key);
            const std::uint8_t fingerprint = fingerprint_for(hash);
            const std::uint32_t stripe = static_cast<std::uint32_t>(hash) &
                                         (StripeCount - 1);
            std::lock_guard<std::mutex> lock(stripeLocks[stripe]);
            const std::uint64_t base = std::uint64_t(stripe) * stripeSize;
            std::uint64_t offset = (hash >> 12) % stripeSize;
            for (std::uint64_t probe = 0; probe < stripeSize; ++probe) {
                const std::uint64_t slot = base + offset;
                const std::uint64_t value = load_slot(slot);
                if (!value) {
                    const std::uint64_t dense = keyCount.fetch_add(
                      1, std::memory_order_relaxed);
                    if (dense >= stateLimit)
                        throw std::runtime_error(
                          "spawned-only Devil exact graph exceeds configured limit");
                    if (dense >= hashCapacity)
                        throw std::runtime_error(
                          "spawned-only Devil exact graph exceeds hash capacity");
                    keys[dense] = key;
                    store_fingerprint(slot, fingerprint);
                    store_slot(slot, dense + 1);
                    return std::pair<bool, std::uint64_t>{true, dense};
                }
                const std::uint64_t dense = value - 1;
                if (load_fingerprint(slot) == fingerprint &&
                    keys[dense] == key)
                    return std::pair<bool, std::uint64_t>{false, dense};
                if (++offset == stripeSize)
                    offset = 0;
            }
            throw std::runtime_error("spawned-only Devil hash stripe saturated");
        };
        if (saved) {
            const std::uint32_t rebuildWorkers = std::min(
              workerThreads_, std::max(1u, std::thread::hardware_concurrency()));
            std::atomic<std::uint64_t> next{0};
            std::atomic<std::uint64_t> done{0};
            std::atomic<bool> failed{false};
            std::exception_ptr failure;
            std::mutex failureMutex;
            std::vector<std::thread> tasks;
            for (std::uint32_t worker = 0; worker < rebuildWorkers; ++worker)
                tasks.emplace_back([&] {
                    try {
                        while (!failed.load(std::memory_order_relaxed)) {
                            const std::uint64_t begin = next.fetch_add(
                              4096, std::memory_order_relaxed);
                            if (begin >= saved->size)
                                break;
                            const std::uint64_t end = std::min(
                              saved->size, begin + 4096);
                            for (std::uint64_t item = begin; item < end; ++item) {
                                const std::uint64_t dense = item;
                                const DevilKey& key = keys[dense];
                                const std::uint64_t hash = DevilKeyHash{}(key);
                                const std::uint8_t fingerprint =
                                  fingerprint_for(hash);
                                const std::uint32_t stripe =
                                  static_cast<std::uint32_t>(hash) &
                                  (StripeCount - 1);
                                std::lock_guard<std::mutex> lock(
                                  stripeLocks[stripe]);
                                const std::uint64_t base =
                                  std::uint64_t(stripe) * stripeSize;
                                std::uint64_t offset =
                                  (hash >> 12) % stripeSize;
                                bool bound = false;
                                for (std::uint64_t probe = 0;
                                     probe < stripeSize; ++probe) {
                                    const std::uint64_t slot = base + offset;
                                    const std::uint64_t value = load_slot(slot);
                                    if (!value) {
                                        store_fingerprint(slot, fingerprint);
                                        store_slot(slot, dense + 1);
                                        bound = true;
                                        break;
                                    }
                                    if (load_fingerprint(slot) == fingerprint &&
                                        keys[value - 1] == key)
                                        throw std::runtime_error(
                                          "duplicate committed spawned-only Devil key");
                                    if (++offset == stripeSize)
                                        offset = 0;
                                }
                                if (!bound)
                                    throw std::runtime_error(
                                      "spawned-only Devil rebuilt hash stripe saturated");
                            }
                            const std::uint64_t completed = done.fetch_add(
                              end - begin, std::memory_order_relaxed) + end - begin;
                            if (completed / 100'000'000 !=
                                (completed - (end - begin)) / 100'000'000)
                                std::cout << "devil_resume_index square "
                                          << fixedSquare << " states "
                                          << completed << '/' << saved->size
                                          << '\n' << std::flush;
                        }
                    }
                    catch (...) {
                        failed.store(true, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (!failure)
                            failure = std::current_exception();
                    }
                });
            for (auto& task : tasks)
                task.join();
            if (failure)
                std::rethrow_exception(failure);
            std::cout << "devil_resume_index square " << fixedSquare
                      << " states " << saved->size << " workers "
                      << rebuildWorkers << '\n' << std::flush;
        }
        keys.advise_random();

        constexpr std::uint64_t MinionHighMask = 0xffffULL;
        constexpr int SideShift = LegacySideShift;
        constexpr int WhiteKingShift = LegacyWhiteKingShift;
        constexpr int BlackKingShift = LegacyBlackKingShift;
        constexpr int DevilSquareShift = LegacyDevilSquareShift;
        constexpr int DevilCooldownShift = LegacyDevilCooldownShift;
        constexpr int SecondaryShift = LegacySecondaryShift;
        constexpr std::uint64_t SquareMask = LegacySquareMask;
        constexpr std::uint64_t NoSquare = DevilNoSquare;
        const auto encode_position = [&](const Position& position,
                                         bool canonicalRoot)
          -> std::optional<DevilKey> {
            int whiteKing = Position::NoSquare;
            int blackKing = Position::NoSquare;
            int devilSquare = Position::NoSquare;
            int devilCooldown = 0;
            int secondarySquare = Position::NoSquare;
            std::uint64_t minionLow = 0;
            std::uint64_t minionHigh = 0;
            for (int id = 0; id < position.piece_count(); ++id) {
                const PieceState& piece = position.piece(id);
                if (!piece.alive || !piece.onBoard)
                    continue;
                if (piece.type == PieceType::King)
                    (piece.color == Color::White ? whiteKing : blackKing) =
                      piece.square;
                else if (piece.type == PieceType::Devil &&
                         piece.color == Color::White &&
                         devilSquare == Position::NoSquare) {
                    devilSquare = piece.square;
                    devilCooldown = piece.cooldown;
                }
                else if (piece.type == PieceType::Minion &&
                         piece.color == Color::White) {
                    if (piece.square < 64)
                        minionLow |= std::uint64_t{1} << piece.square;
                    else
                        minionHigh |=
                          std::uint64_t{1} << (piece.square - 64);
                }
                else if (fourModels_ && piece.type == secondaryType_ &&
                         piece.color == secondaryColor_ &&
                         secondarySquare == Position::NoSquare)
                    secondarySquare = piece.square;
                else
                    return std::nullopt;
            }
            const bool hasMinion = minionLow || (minionHigh & MinionHighMask);
            // Spawn sets cooldown 3 and finish_turn immediately decrements it;
            // the Devil can therefore spawn only every second White turn.  A
            // White Minion advances on every subsequent White turn and even a
            // rank-1 spawn is gone on its tenth advance.  Thus at most five
            // spawned Minions coexist.  This is a proved model invariant, not
            // the old census's observed maximum of three.
            const int minionCount = __builtin_popcountll(minionLow) +
              __builtin_popcountll(minionHigh & MinionHighMask);
            if (whiteKing == Position::NoSquare ||
                blackKing == Position::NoSquare || devilCooldown > 3 ||
                (devilSquare == Position::NoSquare && !hasMinion &&
                 secondarySquare == Position::NoSquare) ||
                (!fourModels_ && secondarySquare != Position::NoSquare) ||
                minionCount > 5)
                return std::nullopt;
            bool reflected = canonicalRoot && devilSquare % Position::BoardFiles >= 4;
            const auto reflect = [&](int square) {
                return reflected
                  ? static_cast<int>(horizontal_reflection(
                      static_cast<std::uint8_t>(square)))
                  : square;
            };
            std::uint64_t legacyLow = 0;
            std::uint64_t legacyHigh = 0;
            auto add_minion = [&](int square) {
                const int target = reflect(square);
                const int minimumFile = std::max(
                  0, fixedSquare % Position::BoardFiles - 2);
                const int maximumFile = std::min(
                  Position::BoardFiles - 1,
                  fixedSquare % Position::BoardFiles + 2);
                if (target % Position::BoardFiles < minimumFile ||
                    target % Position::BoardFiles > maximumFile)
                    throw std::runtime_error(
                      "spawned-only Devil Minion escaped its fixed spawn files");
                if (target < 64)
                    legacyLow |= std::uint64_t{1} << target;
                else
                    legacyHigh |= std::uint64_t{1} << (target - 64);
            };
            std::uint64_t bits = minionLow;
            while (bits) {
                const int square = __builtin_ctzll(bits);
                bits &= bits - 1;
                add_minion(square);
            }
            bits = minionHigh & MinionHighMask;
            while (bits) {
                const int square = 64 + __builtin_ctzll(bits);
                bits &= bits - 1;
                add_minion(square);
            }
            legacyHigh |= static_cast<std::uint64_t>(
              position.side_to_move() == Color::Black) << SideShift;
            legacyHigh |= static_cast<std::uint64_t>(reflect(whiteKing)) <<
                              WhiteKingShift;
            legacyHigh |= static_cast<std::uint64_t>(reflect(blackKing)) <<
                              BlackKingShift;
            legacyHigh |= static_cast<std::uint64_t>(
              devilSquare == Position::NoSquare ? NoSquare : reflect(devilSquare)) <<
              DevilSquareShift;
            legacyHigh |= static_cast<std::uint64_t>(devilCooldown) <<
                              DevilCooldownShift;
            legacyHigh |= static_cast<std::uint64_t>(
              secondarySquare == Position::NoSquare
                ? NoSquare : reflect(secondarySquare)) << SecondaryShift;
            const int encodedDevil = static_cast<int>(
              (legacyHigh >> DevilSquareShift) & SquareMask);
            if (encodedDevil != static_cast<int>(NoSquare) &&
                encodedDevil != fixedSquare)
                return std::nullopt;
            return DevilKey{pack_compact_devil_key(
              legacyLow, legacyHigh, static_cast<unsigned>(fixedSquare))};
        };
        const auto decode_position = [&](const DevilKey& key) {
            const auto legacy = unpack_compact_devil_key(
              key.value, static_cast<unsigned>(fixedSquare));
            const std::uint64_t legacyLow = legacy.first;
            const std::uint64_t legacyHigh = legacy.second;
            const auto field = [&](int shift) {
                return static_cast<int>((legacyHigh >> shift) & SquareMask);
            };
            Position position;
            position.clear();
            position.add_piece(PieceType::King, Color::White,
                               field(WhiteKingShift));
            position.add_piece(PieceType::King, Color::Black,
                               field(BlackKingShift));
            const int devilSquare = field(DevilSquareShift);
            if (devilSquare != static_cast<int>(NoSquare)) {
                const int devil = position.add_piece(
                  PieceType::Devil, Color::White, devilSquare);
                position.piece(devil).cooldown = static_cast<std::uint8_t>(
                  (legacyHigh >> DevilCooldownShift) & 3ULL);
            }
            const int secondarySquare = field(SecondaryShift);
            if (secondarySquare != static_cast<int>(NoSquare))
                position.add_piece(secondaryType_, secondaryColor_,
                                   secondarySquare);
            std::uint64_t bits = legacyLow;
            while (bits) {
                const int square = __builtin_ctzll(bits);
                bits &= bits - 1;
                position.add_piece(PieceType::Minion, Color::White, square);
            }
            bits = legacyHigh & MinionHighMask;
            while (bits) {
                const int square = 64 + __builtin_ctzll(bits);
                bits &= bits - 1;
                position.add_piece(PieceType::Minion, Color::White, square);
            }
            position.set_side_to_move(
              ((legacyHigh >> SideShift) & 1ULL)
                ? Color::Black : Color::White);
            for (int id = 0; id < position.piece_count(); ++id)
                position.piece(id).moved = true;
            return position;
        };

        const std::uint32_t workers = std::min(
          workerThreads_, std::max(1u, std::thread::hardware_concurrency()));
        std::vector<std::uint64_t> frontier;
        std::unique_ptr<ReadOnlyMappedArray<std::uint64_t>> retainedFrontier;
        std::uint32_t ply = 0;
        if (saved) {
            ply = saved->ply;
            if (saved->frontier)
                retainedFrontier =
                  std::make_unique<ReadOnlyMappedArray<std::uint64_t>>(
                    frontierPath, saved->frontier);
        }
        else {
            std::atomic<std::uint32_t> nextRoot{0};
            std::atomic<bool> failed{false};
            std::exception_ptr failure;
            std::mutex failureMutex;
            std::vector<std::vector<std::uint64_t>> roots(workers);
            std::vector<std::thread> tasks;
            for (std::uint32_t worker = 0; worker < workers; ++worker)
                tasks.emplace_back([&, worker] {
                    try {
                        auto& local = roots[worker];
                        while (!failed.load(std::memory_order_relaxed)) {
                            const std::uint32_t begin = nextRoot.fetch_add(
                              4096, std::memory_order_relaxed);
                            if (begin >= stateCount_)
                                break;
                            const std::uint32_t end = std::min<std::uint32_t>(
                              stateCount_, begin + 4096);
                            for (std::uint32_t index = begin; index < end; ++index) {
                                Position position;
                                if (!make_position_at(index, position))
                                    continue;
                                const auto key = encode_position(position, true);
                                if (!key)
                                    continue;
                                const auto [inserted, dense] = insert(*key);
                                if (inserted)
                                    local.push_back(dense);
                            }
                        }
                    }
                    catch (...) {
                        failed.store(true, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (!failure)
                            failure = std::current_exception();
                    }
                });
            for (auto& task : tasks)
                task.join();
            if (failure)
                std::rethrow_exception(failure);
            std::size_t rootCount = 0;
            for (const auto& local : roots)
                rootCount += local.size();
            frontier.reserve(rootCount);
            for (auto& local : roots)
                frontier.insert(frontier.end(), local.begin(), local.end());
            std::cout << "devil_roots square " << fixedSquare
                      << " roots " << frontier.size() << " workers "
                      << workers << '\n' << std::flush;
        }
        const auto save_layer = [&](std::uint32_t completedPly,
                                    const std::vector<std::uint64_t>& values) {
            keys.sync_prefix(keyCount.load(std::memory_order_relaxed));
            const std::string frontierTemporary = frontierPath + ".tmp";
            {
                std::ofstream stream(frontierTemporary,
                                     std::ios::binary | std::ios::trunc);
                stream.write(reinterpret_cast<const char*>(values.data()),
                             values.size() * sizeof(std::uint64_t));
                if (!stream)
                    throw std::runtime_error("cannot write spawned-only Devil frontier");
            }
            if (std::rename(frontierTemporary.c_str(), frontierPath.c_str()) != 0)
                throw std::runtime_error("cannot install spawned-only Devil frontier");
            const std::string temporary = metadataPath + ".tmp";
            {
                std::ofstream stream(temporary,
                                     std::ios::binary | std::ios::trunc);
                const std::array<char, 8> magic{{'U','F','D','V','C','P','1','\0'}};
                const std::uint32_t version = 4;
                const std::uint32_t square = fixedSquare;
                const std::uint64_t size = keyCount.load(std::memory_order_relaxed);
                const std::uint64_t frontierSize = values.size();
                stream.write(magic.data(), magic.size());
                stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
                stream.write(reinterpret_cast<const char*>(&square), sizeof(square));
                stream.write(reinterpret_cast<const char*>(&stateLimit), sizeof(stateLimit));
                stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
                stream.write(reinterpret_cast<const char*>(&frontierSize), sizeof(frontierSize));
                stream.write(reinterpret_cast<const char*>(&completedPly), sizeof(completedPly));
                if (!stream)
                    throw std::runtime_error("cannot write spawned-only Devil checkpoint");
            }
            if (std::rename(temporary.c_str(), metadataPath.c_str()) != 0)
                throw std::runtime_error("cannot install spawned-only Devil checkpoint");
        };
        if (!saved)
            save_layer(0, frontier);

        while (retainedFrontier || !frontier.empty()) {
            const std::uint64_t frontierSize = retainedFrontier
              ? retainedFrontier->count() : frontier.size();
            const auto frontier_at = [&](std::uint64_t index) {
                return retainedFrontier
                  ? (*retainedFrontier)[index] : frontier[index];
            };
            std::atomic<std::size_t> next{0};
            std::atomic<bool> failed{false};
            std::exception_ptr failure;
            std::mutex failureMutex;
            std::vector<std::vector<std::uint64_t>> additions(workers);
            std::vector<std::thread> tasks;
            for (std::uint32_t worker = 0; worker < workers; ++worker)
                tasks.emplace_back([&, worker] {
                    try {
                        auto& local = additions[worker];
                        while (!failed.load(std::memory_order_relaxed)) {
                            const std::size_t begin = next.fetch_add(
                              4096, std::memory_order_relaxed);
                            if (begin >= frontierSize)
                                break;
                            const std::size_t end = std::min(
                              static_cast<std::size_t>(frontierSize),
                              begin + 4096);
                            // Closure layers contain many repeated children.
                            // Probing the global exact index immediately made
                            // every repetition pay another random proof-key
                            // page read.  Retain a bounded worker-local batch,
                            // order it by its global hash path, and eliminate
                            // exact duplicates before acquiring stripe locks.
                            // This changes only disposable insertion order;
                            // the committed exact key set and proof semantics
                            // remain unchanged.
                            std::vector<std::pair<std::uint64_t, DevilKey>> children;
                            children.reserve((end - begin) * 16);
                            for (std::size_t item = begin; item < end; ++item) {
                                Position position = decode_position(
                                  keys[frontier_at(item)]);
                                for (const Move& move : position.legal_moves()) {
                                    Position child = position;
                                    if (!child.apply_move_unchecked(move))
                                        throw std::runtime_error(
                                          "spawned-only Devil legal move failed");
                                    if (child.game_over())
                                        continue;
                                    const auto childKey = encode_position(child, false);
                                    if (!childKey)
                                        throw std::runtime_error(
                                          "spawned-only Devil nonterminal closure residual: " +
                                          child.upn());
                                    children.emplace_back(
                                      DevilKeyHash{}(*childKey), *childKey);
                                }
                            }
                            std::sort(children.begin(), children.end(),
                                      [](const auto& lhs, const auto& rhs) {
                                return lhs.first < rhs.first ||
                                  (lhs.first == rhs.first &&
                                   lhs.second.value < rhs.second.value);
                            });
                            DevilKey previous{};
                            bool havePrevious = false;
                            for (const auto& candidate : children) {
                                if (havePrevious && candidate.second == previous)
                                    continue;
                                previous = candidate.second;
                                havePrevious = true;
                                const auto [inserted, dense] = insert(candidate.second);
                                    if (inserted)
                                        local.push_back(dense);
                            }
                        }
                    }
                    catch (...) {
                        failed.store(true, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (!failure)
                            failure = std::current_exception();
                    }
                });
            for (auto& task : tasks)
                task.join();
            if (failure)
                std::rethrow_exception(failure);
            std::vector<std::uint64_t> nextFrontier;
            std::size_t nextSize = 0;
            for (const auto& local : additions)
                nextSize += local.size();
            nextFrontier.reserve(nextSize);
            for (auto& local : additions)
                nextFrontier.insert(nextFrontier.end(), local.begin(), local.end());
            retainedFrontier.reset();
            frontier.swap(nextFrontier);
            ++ply;
            save_layer(ply, frontier);
            std::cout << "devil_closure square " << fixedSquare
                      << " ply " << ply << " frontier " << frontier.size()
                      << " states " << keyCount.load(std::memory_order_relaxed)
                      << " workers " << workers << '\n' << std::flush;
        }

        const std::uint64_t graphStates = keyCount.load(std::memory_order_relaxed);
        if (!graphStates)
            throw std::runtime_error("empty spawned-only Devil graph");
        const char* adoptedReverseRaw = std::getenv(
          "ULTIMATE_DEVIL_REVERSE_ADOPT_BUCKETS");
        const bool resumeReverse = adoptedReverseRaw && *adoptedReverseRaw;
        PersistentMappedArray<Node> graphNodes(
          prefix + ".nodes", graphStates, !resumeReverse);
        PersistentMappedArray<std::uint32_t> degrees(
          prefix + ".degrees", graphStates, !resumeReverse);
        const auto graph_scan = [&](const char* phase, auto&& action) {
            std::atomic<std::uint64_t> next{0};
            std::atomic<std::uint64_t> done{0};
            std::atomic<bool> failed{false};
            std::exception_ptr failure;
            std::mutex failureMutex;
            std::vector<std::thread> tasks;
            for (std::uint32_t worker = 0; worker < workers; ++worker)
                tasks.emplace_back([&] {
                    try {
                        while (!failed.load(std::memory_order_relaxed)) {
                            const std::uint64_t begin = next.fetch_add(
                              4096, std::memory_order_relaxed);
                            if (begin >= graphStates)
                                break;
                            const std::uint64_t end = std::min(
                              graphStates, begin + 4096);
                            for (std::uint64_t index = begin; index < end; ++index)
                                action(index);
                            const std::uint64_t completed = done.fetch_add(
                              end - begin, std::memory_order_relaxed) + end - begin;
                            if (completed / 10'000'000 !=
                                (completed - (end - begin)) / 10'000'000)
                                std::cout << phase << " square " << fixedSquare
                                          << " states " << completed << '/'
                                          << graphStates << '\n' << std::flush;
                        }
                    }
                    catch (...) {
                        failed.store(true, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (!failure)
                            failure = std::current_exception();
                    }
                });
            for (auto& task : tasks)
                task.join();
            if (failure)
                std::rethrow_exception(failure);
        };
        const auto successors = [&](std::uint64_t index, auto&& consume) {
            Position position = decode_position(keys[index]);
            std::uint32_t legal = 0;
            for (const Move& move : position.legal_moves()) {
                ++legal;
                Position child = position;
                if (!child.apply_move_unchecked(move))
                    throw std::runtime_error("spawned-only Devil graph move failed");
                if (child.game_over()) {
                    consume(std::nullopt, child.winner(),
                            child.side_to_move() == position.side_to_move());
                    continue;
                }
                const auto childKey = encode_position(child, false);
                if (!childKey)
                    throw std::runtime_error(
                      "spawned-only Devil edge leaves nonterminal closure");
                const auto childIndex = locate(*childKey);
                if (!childIndex || *childIndex >= graphStates)
                    throw std::runtime_error(
                      "spawned-only Devil completed closure misses successor");
                consume(childIndex, std::optional<Color>{},
                        child.side_to_move() == position.side_to_move());
            }
            return legal;
        };
        if (!resumeReverse)
            graph_scan("devil_graph", [&](std::uint64_t index) {
                Node& node = graphNodes[index];
                const Position parent = decode_position(keys[index]);
                const std::uint32_t legal = successors(
                  index, [&](std::optional<std::uint64_t> child,
                             std::optional<Color> winner, bool) {
                    if (node.remaining ==
                        std::numeric_limits<std::uint16_t>::max())
                        throw std::runtime_error(
                          "spawned-only Devil move-count overflow");
                    ++node.remaining;
                    if (child) {
                        __atomic_fetch_add(&degrees[*child], 1u,
                                           __ATOMIC_RELAXED);
                        return;
                    }
                    if (winner && *winner == parent.side_to_move()) {
                        node.wdl = Wdl::Win;
                        node.dtw = 1;
                    }
                    else if (winner && node.remaining)
                        --node.remaining;
                });
                if (!legal) {
                    const auto winner = parent.winner();
                    node.wdl = winner && *winner != parent.side_to_move()
                      ? Wdl::Loss : Wdl::Draw;
                }
                else if (node.wdl == Wdl::Unknown && !node.remaining) {
                    node.wdl = Wdl::Loss;
                    node.dtw = 1;
                }
            });
        else
            std::cout << "devil_graph_adopt square " << fixedSquare
                      << " states " << graphStates << '\n' << std::flush;
        // The degree plane can contain tens of billions of entries.  A
        // single-thread prefix reduction left an otherwise idle 32-vCPU host
        // scanning hundreds of gigabytes after the parallel graph pass.
        // Reduce disjoint ranges in parallel; addition is associative and the
        // resulting exact edge count is independent of worker scheduling.
        std::vector<std::uint64_t> partialEdgeCounts(workers, 0);
        std::atomic<std::uint64_t> nextDegree{0};
        std::atomic<bool> degreeFailed{false};
        std::exception_ptr degreeFailure;
        std::mutex degreeFailureMutex;
        std::vector<std::thread> degreeTasks;
        for (std::uint32_t worker = 0; worker < workers; ++worker)
            degreeTasks.emplace_back([&, worker] {
                try {
                    std::uint64_t local = 0;
                    while (!degreeFailed.load(std::memory_order_relaxed)) {
                        const std::uint64_t begin = nextDegree.fetch_add(
                          1'048'576, std::memory_order_relaxed);
                        if (begin >= graphStates)
                            break;
                        const std::uint64_t end = std::min(
                          graphStates, begin + 1'048'576);
                        for (std::uint64_t index = begin; index < end; ++index) {
                            if (local > std::numeric_limits<std::uint64_t>::max() -
                                          degrees[index])
                                throw std::runtime_error(
                                  "spawned-only Devil edge-count overflow");
                            local += degrees[index];
                        }
                    }
                    partialEdgeCounts[worker] = local;
                }
                catch (...) {
                    degreeFailed.store(true, std::memory_order_relaxed);
                    std::lock_guard<std::mutex> lock(degreeFailureMutex);
                    if (!degreeFailure)
                        degreeFailure = std::current_exception();
                }
            });
        for (auto& task : degreeTasks)
            task.join();
        if (degreeFailure)
            std::rethrow_exception(degreeFailure);
        std::uint64_t edgeCount = 0;
        for (const std::uint64_t partial : partialEdgeCounts) {
            if (edgeCount > std::numeric_limits<std::uint64_t>::max() - partial)
                throw std::runtime_error("spawned-only Devil edge-count overflow");
            edgeCount += partial;
        }
        std::cout << "devil_degree_sum square " << fixedSquare
                  << " states " << graphStates << " edges " << edgeCount
                  << " workers " << workers << '\n' << std::flush;
        PersistentMappedArray<std::uint64_t> offsets(
          prefix + ".offsets", graphStates + 1, !resumeReverse);
        // Central-square Devil closures can exceed 2^32 states.  Keep the
        // dense predecessor index wide and retain its same-side flag separately.
        PersistentMappedArray<std::uint64_t> predecessors(
          prefix + ".predecessors", edgeCount, !resumeReverse);
        PersistentMappedArray<std::uint8_t> predecessorSides(
          prefix + ".predecessor-sides", edgeCount, !resumeReverse);
        build_restartable_devil_reverse(
          prefix, fixedSquare, graphStates, edgeCount, workers, degrees.data(),
          offsets.data(), predecessors.data(), predecessorSides.data(),
          successors);
        std::vector<std::vector<std::uint64_t>> buckets(
          std::numeric_limits<std::uint16_t>::max() + 1ULL);
        for (std::uint64_t index = 0; index < graphStates; ++index)
            if (graphNodes[index].wdl == Wdl::Win ||
                graphNodes[index].wdl == Wdl::Loss)
                buckets[graphNodes[index].dtw].push_back(index);
        std::uint64_t propagated = 0;
        for (std::uint32_t distance = 0; distance < buckets.size(); ++distance)
            for (std::size_t queued = 0; queued < buckets[distance].size(); ++queued) {
                const std::uint64_t child = buckets[distance][queued];
                const Node childNode = graphNodes[child];
                if (distance != childNode.dtw)
                    continue;
                if (++propagated % 10'000'000 == 0)
                    std::cout << "devil_propagate square " << fixedSquare
                              << " queue " << propagated << '\n' << std::flush;
                for (std::uint64_t edge = offsets[child];
                     edge < offsets[child + 1]; ++edge) {
                    const std::uint64_t parentIndex = predecessors[edge];
                    Node& parent = graphNodes[parentIndex];
                    const Wdl outcome = parent_wdl(
                      childNode.wdl, predecessorSides[edge] != 0);
                    if (parent.wdl == Wdl::Win && outcome == Wdl::Win) {
                        const std::uint16_t value = static_cast<std::uint16_t>(
                          std::min<int>(std::numeric_limits<std::uint16_t>::max(),
                                        childNode.dtw + 1));
                        if (value < parent.dtw) {
                            parent.dtw = value;
                            buckets[value].push_back(parentIndex);
                        }
                    }
                    else if (parent.wdl == Wdl::Unknown && outcome == Wdl::Win) {
                        parent.wdl = Wdl::Win;
                        parent.dtw = static_cast<std::uint16_t>(std::min<int>(
                          std::numeric_limits<std::uint16_t>::max(),
                          childNode.dtw + 1));
                        buckets[parent.dtw].push_back(parentIndex);
                    }
                    else if (parent.wdl == Wdl::Unknown && outcome == Wdl::Loss) {
                        if (parent.remaining)
                            --parent.remaining;
                        parent.longestWinChild = std::max(
                          parent.longestWinChild, childNode.dtw);
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
        for (std::uint64_t index = 0; index < graphStates; ++index)
            if (graphNodes[index].wdl == Wdl::Unknown)
                graphNodes[index].wdl = Wdl::Draw;

        graph_scan("devil_verify", [&](std::uint64_t index) {
            const Node node = graphNodes[index];
            const Position parent = decode_position(keys[index]);
            bool hasWinningMove = false;
            bool hasDraw = false;
            bool allChildrenWin = true;
            std::uint16_t shortestLoss = std::numeric_limits<std::uint16_t>::max();
            std::uint16_t longestWin = 0;
            const std::uint32_t legal = successors(
              index, [&](std::optional<std::uint64_t> child,
                         std::optional<Color> winner, bool sameSide) {
                Wdl outcome = Wdl::Draw;
                std::uint16_t childDtw = 0;
                if (child) {
                    outcome = parent_wdl(graphNodes[*child].wdl, sameSide);
                    childDtw = graphNodes[*child].dtw;
                }
                else if (winner)
                    outcome = *winner == parent.side_to_move()
                      ? Wdl::Win : Wdl::Loss;
                if (outcome == Wdl::Win) {
                    hasWinningMove = true;
                    shortestLoss = std::min(shortestLoss, childDtw);
                    allChildrenWin = false;
                }
                else if (outcome == Wdl::Draw) {
                    hasDraw = true;
                    allChildrenWin = false;
                }
                else
                    longestWin = std::max(longestWin, childDtw);
            });
            allChildrenWin = legal && allChildrenWin;
            const bool valid = node.wdl == Wdl::Win
              ? hasWinningMove && node.dtw == shortestLoss + 1
              : node.wdl == Wdl::Loss
                ? ((!legal && node.dtw == 0) ||
                   (allChildrenWin && node.dtw == longestWin + 1))
                : !hasWinningMove && (!legal || hasDraw);
            if (!valid)
                throw std::runtime_error(
                  "spawned-only Devil Bellman residual at graph index " +
                  std::to_string(index));
        });

        std::ofstream fragment(fragmentPath, std::ios::binary | std::ios::trunc);
        const std::array<char, 12> magic{{'U','F','D','V','R','O','O','T','1','\0','\0','\0'}};
        const std::uint32_t version = 1;
        const std::uint32_t square = fixedSquare;
        std::vector<std::tuple<std::uint32_t, std::uint8_t, std::uint16_t>> roots;
        for (std::uint32_t dense = 0; dense < stateCount_; ++dense) {
            Position position;
            if (!make_position_at(dense, position))
                continue;
            const auto key = encode_position(position, true);
            if (!key)
                continue;
            const auto graph = locate(*key);
            if (!graph || *graph >= graphStates)
                throw std::runtime_error("spawned-only Devil root closure residual");
            roots.emplace_back(dense,
              static_cast<std::uint8_t>(graphNodes[*graph].wdl),
              graphNodes[*graph].dtw);
        }
        const std::uint64_t rootCount = roots.size();
        fragment.write(magic.data(), magic.size());
        fragment.write(reinterpret_cast<const char*>(&version), sizeof(version));
        fragment.write(reinterpret_cast<const char*>(&square), sizeof(square));
        fragment.write(reinterpret_cast<const char*>(&stateLimit), sizeof(stateLimit));
        fragment.write(reinterpret_cast<const char*>(&graphStates), sizeof(graphStates));
        fragment.write(reinterpret_cast<const char*>(&edgeCount), sizeof(edgeCount));
        fragment.write(reinterpret_cast<const char*>(&rootCount), sizeof(rootCount));
        for (const auto [dense, wdl, dtw] : roots) {
            fragment.write(reinterpret_cast<const char*>(&dense), sizeof(dense));
            fragment.write(reinterpret_cast<const char*>(&wdl), sizeof(wdl));
            fragment.write(reinterpret_cast<const char*>(&dtw), sizeof(dtw));
        }
        if (!fragment)
            throw std::runtime_error("cannot write spawned-only Devil root fragment");
        std::cout << "DEVIL_SPAWNED_SQUARE_OK square " << fixedSquare
                  << " states " << graphStates << " edges " << edgeCount
                  << " roots " << rootCount << " fragment " << fragmentPath
                  << '\n';
    }

   private:
    void self_test_jester_royal_codec(std::uint32_t samples) const {
        if (!has_single_ivory_jester())
            return;
        for (std::uint32_t sample = 0; sample < samples; ++sample) {
            const std::uint32_t index = static_cast<std::uint32_t>(
              std::uint64_t(stateCount_) * sample / samples);
            Position concrete, concreteAlternative;
            if (!make_position_at(index, concrete))
                continue;
            const std::uint32_t alternative =
              primary_jester_alternative(index);
            if (!make_position_at(alternative, concreteAlternative))
                throw std::runtime_error(
                  "valid Jester world reflected to invalid geometry");
            Position physical, physicalAlternative;
            const std::uint32_t roundTrip =
              primary_jester_alternative(alternative);
            const bool madePhysical =
              make_primary_jester_world(index, false, physical);
            const bool madeAlternative =
              make_primary_jester_world(index, true, physicalAlternative);
            const std::uint32_t encodedAlternative = madeAlternative
              ? child_index(physicalAlternative)
              : std::numeric_limits<std::uint32_t>::max();
            if (roundTrip != index || !madePhysical || !madeAlternative ||
                encodedAlternative != alternative) {
                std::ostringstream error;
                error << "Jester royal-swap codec is not an involution"
                      << " index " << index
                      << " alternative " << alternative
                      << " roundtrip " << roundTrip
                      << " made " << madePhysical << '/' << madeAlternative
                      << " encoded " << encodedAlternative;
                throw std::runtime_error(error.str());
            }
            if (physical.side_to_move() == jester_observer_color()) {
                const DisclosureContext observer{jester_observer_color(), false};
                const bool compactEqual =
                  primary_jester_view_key(physical) ==
                    primary_jester_view_key(physicalAlternative) &&
                  primary_jester_decision_markers(physical) ==
                    primary_jester_decision_markers(physicalAlternative);
                const bool generalEqual =
                  decision_observation_key(physical, observer) ==
                    decision_observation_key(physicalAlternative, observer);
                if (compactEqual != generalEqual)
                    throw std::runtime_error(
                      "compact Jester legal-dot partition diverges from "
                      "the public-information model");
            }
        }
        std::cout << "jesterroyalswapcodecok samples " << samples << '\n';
    }

    static void append_information_word(std::string& output, std::int32_t value) {
        const std::uint32_t word = static_cast<std::uint32_t>(value);
        for (unsigned shift = 0; shift < 32; shift += 8)
            output.push_back(static_cast<char>((word >> shift) & 0xff));
    }

    std::int32_t primary_jester_public_type(const PieceState& piece) const {
        if (piece.color == jester_owner_color() &&
            (piece.type == PieceType::King || piece.type == PieceType::Jester))
            return static_cast<std::int32_t>(PieceType::Count) + 1;
        return static_cast<std::int32_t>(piece.type);
    }

    // Collision-free compact projection specialized to the closed stateless
    // one-Jester strata. Copycat's derived mirror link is public and its IDs
    // are stable across the paired royal assignments. Avoiding the
    // general relationship canonicalizer and text formatting saves billions
    // of allocations during the large exact solves while retaining a complete
    // fixed-width public serialization.
    std::string primary_jester_view_key(const Position& position,
                                        bool* terminalOut = nullptr) const {
        if (secondaryType_ == PieceType::Angel) {
            if (terminalOut)
                *terminalOut = position.game_over();
            return view_key(
              position, DisclosureContext{jester_observer_color(), false});
        }
        using Record = std::array<std::int32_t, 13>;
        std::vector<Record> records;
        records.reserve(position.piece_count());
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive)
                continue;
            if (piece.host != Position::NoPiece)
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
        const bool gameOver = position.game_over();
        if (terminalOut)
            *terminalOut = gameOver;
        if (gameOver) {
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
    std::string primary_jester_decision_markers(
      const Position& position) const {
        if (position.side_to_move() != jester_observer_color())
            throw std::runtime_error(
              "primary-Jester private dot projection called for informed turn");
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

    std::string primary_jester_transition_key(
      const Position& before, const Move& move, const Position& after) const {
        if (secondaryType_ == PieceType::Angel) {
            const DisclosureContext observer{
              jester_observer_color(), false};
            std::string publicView;
            std::string output = transition_observation_key(
              before, move, after, observer, &publicView);
            if (!after.game_over() &&
                after.side_to_move() == observer.observer) {
                const std::string decision = decision_observation_key(
                  after, observer, &publicView);
                output += "|nextDecision=" +
                          std::to_string(decision.size()) + ':' + decision;
            }
            return output;
        }
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
        bool terminal = false;
        output += primary_jester_view_key(after, &terminal);
        // Public animation/result observations remain shared. If the result is
        // Black's decision boundary, append only Black's private exhaustive
        // legal-dot signature so its own successor belief is refined before
        // action selection. White never receives this private observation (and
        // already knows its own concrete royal identity in this stratum).
        if (!terminal && after.side_to_move() == jester_observer_color()) {
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
        if (linkedCopycatPair_)
            return decode_four(index / substates_).side;
        if (copycatOnly_)
            return decode_placement(index / substates_).side;
        if (identicalCompoundCopycats_)
            return decode_identical_compound_copycat(index / substates_).side;
        if (compoundCopycat_)
            return decode_compound_copycat(index / substates_).side;
        if (fourModels_)
            return (identicalExtras_ ? decode_identical_four(index / substates_)
                                     : decode_four(index / substates_)).side;
        return decode(index).side;
    }

    [[nodiscard]] bool primary_is_giant() const {
        return attackerType_ == PieceType::Giant;
    }

    [[nodiscard]] bool secondary_is_giant() const {
        return fourModels_ && secondaryType_ == PieceType::Giant;
    }

    [[nodiscard]] bool primary_is_sniper() const {
        return attackerType_ == PieceType::Sniper;
    }

    [[nodiscard]] bool secondary_is_sniper() const {
        return fourModels_ && secondaryType_ == PieceType::Sniper;
    }

    [[nodiscard]] bool primary_is_angel() const {
        return attackerType_ == PieceType::Angel;
    }

    [[nodiscard]] bool secondary_is_angel() const {
        return fourModels_ && secondaryType_ == PieceType::Angel;
    }

    static constexpr std::array<unsigned, 4> GiantStartClassSizes{
      20, 16, 15, 12};

    static std::size_t giant_start_class(std::uint8_t square) {
        const unsigned file = square % Position::BoardFiles;
        const unsigned rank = square / Position::BoardFiles;
        if (file >= Position::BoardFiles - 1 || rank >= Position::BoardRanks - 1)
            throw std::runtime_error("Giant start-class square is not an anchor");
        // A Giant translates by two squares orthogonally. Its own moves
        // therefore preserve anchor-file and anchor-rank parity. On the 7x9
        // lower-left-anchor grid the four components contain 20, 16, 15, and
        // 12 placements respectively.
        return (file & 1u ? 2u : 0u) + (rank & 1u ? 1u : 0u);
    }

    std::size_t giant_start_class_for_slot(
      const Position& position, bool primary) const {
        const int id = primary ? 2 : compoundCopycat_ ? 4 : 3;
        if (id >= position.piece_count() || !position.piece(id).alive ||
            !position.piece(id).onBoard ||
            position.piece(id).type != PieceType::Giant)
            throw std::runtime_error(
              "Giant start-class material slot does not contain a Giant");
        return giant_start_class(position.piece(id).square);
    }

    std::size_t sniper_relative_rank_for_slot(
      const Position& position, bool primary) const {
        const int id = primary ? 2 : compoundCopycat_ ? 4 : 3;
        if (id >= position.piece_count() || !position.piece(id).alive ||
            !position.piece(id).onBoard ||
            position.piece(id).type != PieceType::Sniper)
            throw std::runtime_error(
              "Sniper root-rank material slot does not contain a Sniper");
        const PieceState& sniper = position.piece(id);
        const std::size_t absoluteRank =
          sniper.square / Position::BoardFiles;
        return sniper.color == Color::White
          ? absoluteRank : Position::BoardRanks - 1 - absoluteRank;
    }

    std::size_t angel_root_square_class_for_slot(
      const Position& position, bool primary) const {
        const int angel = primary ? 2 : compoundCopycat_ ? 4 : 3;
        const int other = primary
          ? (fourModels_ ? 3 : Position::NoPiece) : 2;
        if (angel >= position.piece_count() || !position.piece(angel).alive ||
            position.piece(angel).type != PieceType::Angel)
            throw std::runtime_error(
              "Angel root-square material slot does not contain an Angel");
        // For a deployed Angel this is its board square. For an attached Angel
        // the material proxy is its immobile Halo, which preserves the root
        // return/start square while the off-board Angel tracks its host.
        const std::uint8_t square = material_square(
          position, angel, PieceType::Angel, other);
        const std::size_t absoluteFile = square % Position::BoardFiles;
        const std::size_t mirroredFile = std::min(
          absoluteFile, Position::BoardFiles - 1 - absoluteFile);
        const std::size_t absoluteRank = square / Position::BoardFiles;
        const std::size_t relativeRank = position.piece(angel).color == Color::White
          ? absoluteRank : Position::BoardRanks - 1 - absoluteRank;
        return relativeRank * (Position::BoardFiles / 2) + mirroredFile;
    }

    FourState canonicalize_four(FourState state) const {
        return canonicalize(state, primary_is_giant(), secondary_is_giant());
    }

    std::uint32_t encode_four_material(FourState state) const {
        return encode_four(state, primary_is_giant(), secondary_is_giant());
    }

    std::uint32_t encode_identical_four_material(FourState state) const {
        return encode_identical_four(
          state, primary_is_giant(), secondary_is_giant());
    }

    void self_test_giant_four_codec() const {
        const bool firstGiant = primary_is_giant();
        const bool secondGiant = secondary_is_giant();
        if (!fourModels_ || (!firstGiant && !secondGiant))
            throw std::runtime_error(
              "Giant symmetry test requires a four-model Giant class");

        const auto piece_mask = [](std::uint8_t square, bool giant) -> Bitboard {
            const Bitboard origin = Bitboard(1) << square;
            if (!giant)
                return origin;
            if (square % Position::BoardFiles == Position::BoardFiles - 1 ||
                square / Position::BoardFiles == Position::BoardRanks - 1)
                return 0;
            return origin | (origin << 1) |
                   (origin << Position::BoardFiles) |
                   (origin << (Position::BoardFiles + 1));
        };
        const auto valid_geometry = [&](const FourState& state) {
            const std::array<Bitboard, 4> occupied{
              piece_mask(state.whiteKing, false),
              piece_mask(state.blackKing, false),
              piece_mask(state.first, firstGiant),
              piece_mask(state.second, secondGiant)};
            for (const Bitboard mask : occupied)
                if (!mask)
                    return false;
            for (std::size_t first = 0; first < occupied.size(); ++first)
                for (std::size_t second = first + 1;
                     second < occupied.size(); ++second)
                    if (occupied[first] & occupied[second])
                        return false;
            return true;
        };
        const auto reflect = [&](FourState state) {
            state.whiteKing = horizontal_reflection(state.whiteKing);
            state.blackKing = horizontal_reflection(state.blackKing);
            state.first = firstGiant
              ? horizontal_giant_anchor_reflection(state.first)
              : horizontal_reflection(state.first);
            state.second = secondGiant
              ? horizontal_giant_anchor_reflection(state.second)
              : horizontal_reflection(state.second);
            return state;
        };
        const auto same_state = [](const FourState& first,
                                   const FourState& second) {
            return first.side == second.side &&
                   first.whiteKing == second.whiteKing &&
                   first.blackKing == second.blackKing &&
                   first.first == second.first &&
                   first.second == second.second;
        };

        const std::uint32_t placements = identicalExtras_
          ? IdenticalFourStateCount : FourPlacementStateCount;
        std::uint64_t validOrbits = 0;
        for (std::uint32_t index = 0; index < placements; ++index) {
            const FourState state = identicalExtras_
              ? decode_identical_four(index) : decode_four(index);
            const std::uint32_t roundTrip = identicalExtras_
              ? encode_identical_four_material(state)
              : encode_four_material(state);
            if (roundTrip != index)
                throw std::runtime_error(
                  "Giant four-model canonical codec is not bijective");
            if (!valid_geometry(state))
                continue;
            const FourState mirrored = reflect(state);
            if (!same_state(reflect(mirrored), state) ||
                !valid_geometry(mirrored))
                throw std::runtime_error(
                  "Giant horizontal reflection does not preserve geometry");
            const std::uint32_t mirroredIndex = identicalExtras_
              ? encode_identical_four_material(mirrored)
              : encode_four_material(mirrored);
            if (mirroredIndex != index)
                throw std::runtime_error(
                  "Giant horizontal reflection changed its codec orbit");
            ++validOrbits;
        }

        // Minimal failure that exposed the old point-square anchor transform.
        // In one world e1 is the Jester and in the other it is the King. After
        // the common public action e1-f1, g1's 2x2 Giant must reflect to a1.
        // The old codec reflected its anchor to b1, overlapping the King on c1
        // and destroying an otherwise canonical royal pair.
        if (!identicalExtras_ && attackerType_ == PieceType::Jester &&
            secondaryType_ == PieceType::Giant && substates_ == 1) {
            constexpr std::uint32_t FirstChild = 18'985'200;
            constexpr std::uint32_t SwappedChild = 19'952'317;
            Position first, swapped;
            if (primary_jester_alternative(FirstChild) != SwappedChild ||
                primary_jester_alternative(SwappedChild) != FirstChild ||
                !make_primary_jester_world(FirstChild, false, first) ||
                !make_primary_jester_world(FirstChild, true, swapped) ||
                child_index(swapped) != SwappedChild ||
                primary_jester_view_key(first) !=
                  primary_jester_view_key(swapped) ||
                primary_jester_decision_markers(first) !=
                  primary_jester_decision_markers(swapped))
                throw std::runtime_error(
                  "Giant/Jester royal-pair reflection regression failed");
            Position formerlyShifted;
            if (make_position_at(19'952'318, formerlyShifted))
                throw std::runtime_error(
                  "shifted Giant-anchor regression record became valid");
            std::cout << "giantjesterreflectionwitness first " << FirstChild
                      << " swapped " << SwappedChild << '\n';
        }
        std::cout << "giantfourreflectioncodecok valid_orbits "
                  << validOrbits << '\n';
    }

    std::uint32_t primary_jester_alternative(std::uint32_t index) const {
        if (!has_single_ivory_jester())
            throw std::runtime_error(
              "royal-assignment swap requires exactly one primary Jester");
        if (compoundCopycat_) {
            const std::uint32_t combinedSubstate = index % substates_;
            FourState state = decode_compound_copycat(index / substates_);
            if (secondaryColor_ == Color::White)
                std::swap(state.whiteKing, state.second);
            else
                std::swap(state.blackKing, state.second);
            return encode_compound_copycat(state) * substates_ +
                   combinedSubstate;
        }
        if (!fourModels_) {
            const State state = decode(index);
            return encode({state.side, state.attacker, state.blackKing,
                           state.whiteKing, state.substate});
        }
        std::uint32_t combinedSubstate = index % substates_;
        if (secondaryType_ == PieceType::Penguin) {
            const std::uint32_t primarySubstate =
              combinedSubstate / secondarySubstates_;
            const std::uint32_t secondarySubstate =
              combinedSubstate % secondarySubstates_;
            // Penguin bits name concrete identities: 1 is the White King and
            // 4 is the other non-King (the Jester here). A hidden royal swap
            // must exchange those bits so the same public silhouettes remain
            // frozen. Keeping the numeric substate unchanged pairs a
            // different causal aura history.
            const std::uint32_t swappedSecondary =
              (secondarySubstate & 2u) |
              (secondarySubstate & 1u ? 4u : 0u) |
              (secondarySubstate & 4u ? 1u : 0u);
            combinedSubstate =
              primarySubstate * secondarySubstates_ + swappedSecondary;
        }
        else if (secondaryType_ == PieceType::Angel &&
                 secondaryColor_ == Color::White) {
            const std::uint32_t primarySubstate =
              combinedSubstate / secondarySubstates_;
            std::uint32_t secondarySubstate =
              combinedSubstate % secondarySubstates_;
            // Same-team Angel substates 1 and 2 name the hidden royal
            // identities (King-hosted and Jester-hosted). Swapping which
            // public silhouette is the real King must swap those host tags
            // as well, or the paired world attaches to a different visible
            // model and the royal-assignment transform is not an involution.
            if (secondarySubstate == 1)
                secondarySubstate = 2;
            else if (secondarySubstate == 2)
                secondarySubstate = 1;
            combinedSubstate =
              primarySubstate * secondarySubstates_ + secondarySubstate;
        }
        FourState state = decode_four(index / substates_);
        std::swap(state.whiteKing, state.first);
        return encode_four_material(state) * substates_ + combinedSubstate;
    }

    bool make_primary_jester_world(std::uint32_t representative, bool swapped,
                                   Position& position) const {
        if (compoundCopycat_ && has_single_ivory_jester())
            return make_position_at(swapped
              ? primary_jester_alternative(representative) : representative,
              position);
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
        std::uint32_t secondarySubstate =
          combinedSubstate % secondarySubstates_;
        FourState state = decode_four(representative / substates_);
        if (swapped) {
            std::swap(state.whiteKing, state.first);
            if (secondaryType_ == PieceType::Penguin)
                secondarySubstate =
                  (secondarySubstate & 2u) |
                  (secondarySubstate & 1u ? 4u : 0u) |
                  (secondarySubstate & 4u ? 1u : 0u);
            else if (secondaryType_ == PieceType::Angel &&
                     secondaryColor_ == Color::White) {
                if (secondarySubstate == 1)
                    secondarySubstate = 2;
                else if (secondarySubstate == 2)
                    secondarySubstate = 1;
            }
        }

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
            (secondaryType_ != PieceType::Angel &&
             !apply_substate(
               position, secondary, secondaryType_, secondarySubstate)))
            return false;
        if (secondaryType_ == PieceType::Angel &&
            !apply_angel_substate(
              position, secondary, jester, secondarySubstate))
            return false;
        if (secondaryType_ == PieceType::Penguin &&
            !apply_penguin_substate(
              position, secondary, jester, secondarySubstate))
            return false;
        position.set_side_to_move(state.side);
        return true;
    }

    [[nodiscard]] bool has_single_ivory_jester() const {
        const bool primary = attackerType_ == PieceType::Jester &&
                             !identicalExtras_ && !compoundCopycat_ &&
                             !copycatOnly_;
        const bool copycatSecondary = compoundCopycat_ &&
          secondaryType_ == PieceType::Jester;
        return primary || copycatSecondary;
    }

    [[nodiscard]] Color jester_owner_color() const {
        if (attackerType_ == PieceType::Jester)
            return Color::White;
        if (compoundCopycat_ && secondaryType_ == PieceType::Jester)
            return secondaryColor_;
        throw std::runtime_error("material has no single Jester owner");
    }

    [[nodiscard]] Color jester_observer_color() const {
        return ~jester_owner_color();
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

    static constexpr std::array<std::array<int, 2>, 8> PenguinDirections{{
      {{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}},
      {{1, 1}}, {{-1, 1}}, {{1, -1}}, {{-1, -1}}
    }};

    static std::uint8_t penguin_direction_bit(int deltaFile, int deltaRank) {
        if (deltaFile == 0 && deltaRank == 1) return 1;
        if (deltaFile == 0 && deltaRank == -1) return 2;
        if (deltaFile == -1 && deltaRank == 0) return 4;
        if (deltaFile == 1 && deltaRank == 0) return 8;
        if (deltaFile == -1 && deltaRank == 1) return 16;
        if (deltaFile == 1 && deltaRank == 1) return 32;
        if (deltaFile == -1 && deltaRank == -1) return 64;
        if (deltaFile == 1 && deltaRank == -1) return 128;
        return 0;
    }

    static std::uint32_t penguin_target_flag(int target, int other) {
        if (target == 0) return 1;
        if (target == 1) return 2;
        if (other != Position::NoPiece && target == other) return 4;
        return 0;
    }

    static bool penguin_action_substate(const Position& position, int penguin,
                                        int other, std::uint32_t& substate) {
        if (penguin < 0 || penguin >= position.piece_count() ||
            !position.piece(penguin).alive ||
            position.piece(penguin).type != PieceType::Penguin)
            return false;
        substate = 0;
        const PieceState& item = position.piece(penguin);
        const int file = item.square % Position::BoardFiles;
        const int rank = item.square / Position::BoardFiles;
        for (const auto& direction : PenguinDirections) {
            const std::uint8_t directionBit = penguin_direction_bit(
              direction[0], direction[1]);
            if (!(item.action & directionBit))
                continue;
            const int targetFile = file + direction[0];
            const int targetRank = rank + direction[1];
            if (targetFile < 0 || targetFile >= Position::BoardFiles ||
                targetRank < 0 || targetRank >= Position::BoardRanks)
                return false;
            const int target = position.piece_on(
              targetRank * Position::BoardFiles + targetFile);
            const std::uint32_t targetFlag = penguin_target_flag(target, other);
            if (!targetFlag || position.piece(target).type == PieceType::Penguin)
                return false;
            substate |= targetFlag;
        }

        std::uint8_t expectedAction = 0;
        for (const auto& direction : PenguinDirections) {
            const int targetFile = file + direction[0];
            const int targetRank = rank + direction[1];
            if (targetFile < 0 || targetFile >= Position::BoardFiles ||
                targetRank < 0 || targetRank >= Position::BoardRanks)
                continue;
            const int target = position.piece_on(
              targetRank * Position::BoardFiles + targetFile);
            const std::uint32_t targetFlag = penguin_target_flag(target, other);
            if (targetFlag && (substate & targetFlag))
                expectedAction |= penguin_direction_bit(direction[0], direction[1]);
        }
        return expectedAction == item.action;
    }

    static bool apply_penguin_substate(Position& position, int penguin,
                                       int other, std::uint32_t substate) {
        if (penguin < 0 || penguin >= position.piece_count() ||
            position.piece(penguin).type != PieceType::Penguin ||
            (other == Position::NoPiece && (substate & ~3u)) ||
            (other != Position::NoPiece &&
             position.piece(other).type == PieceType::Penguin && (substate & ~3u)) ||
            (substate & ~7u))
            return false;
        PieceState& item = position.piece(penguin);
        item.action = 0;
        std::uint32_t found = 0;
        std::array<bool, Position::MaxPieces> frozen{};
        const int file = item.square % Position::BoardFiles;
        const int rank = item.square / Position::BoardFiles;
        for (const auto& direction : PenguinDirections) {
            const int targetFile = file + direction[0];
            const int targetRank = rank + direction[1];
            if (targetFile < 0 || targetFile >= Position::BoardFiles ||
                targetRank < 0 || targetRank >= Position::BoardRanks)
                continue;
            const int target = position.piece_on(
              targetRank * Position::BoardFiles + targetFile);
            const std::uint32_t targetFlag = penguin_target_flag(target, other);
            if (!targetFlag || !(substate & targetFlag))
                continue;
            found |= targetFlag;
            item.action |= penguin_direction_bit(direction[0], direction[1]);
            if (!frozen[target]) {
                frozen[target] = true;
                ++position.piece(target).freezeCount;
            }
        }
        return found == substate;
    }

    static bool penguin_freeze_state_matches(
      const Position& position, int first, int firstOther,
      std::uint32_t firstSubstate, int second = Position::NoPiece,
      int secondOther = Position::NoPiece, std::uint32_t secondSubstate = 0) {
        Position expected = position;
        for (int id = 0; id < expected.piece_count(); ++id) {
            expected.piece(id).freezeCount = 0;
            if (expected.piece(id).type == PieceType::Penguin)
                expected.piece(id).action = 0;
        }
        if (!apply_penguin_substate(expected, first, firstOther, firstSubstate) ||
            (second != Position::NoPiece &&
             !apply_penguin_substate(
               expected, second, secondOther, secondSubstate)))
            return false;
        for (int id = 0; id < expected.piece_count(); ++id) {
            if (!expected.piece(id).alive)
                continue;
            if (expected.piece(id).freezeCount != position.piece(id).freezeCount)
                return false;
            if (expected.piece(id).type == PieceType::Penguin &&
                expected.piece(id).action != position.piece(id).action)
                return false;
        }
        return true;
    }

    bool apply_angel_substate(Position& position, int angel, int other,
                              std::uint32_t substate) const {
        if (angel < 0 || angel >= position.piece_count() || substate >= 4)
            return false;
        PieceState& item = position.piece(angel);
        if (!item.alive || item.type != PieceType::Angel || !item.onBoard ||
            item.link != Position::NoPiece || item.host != Position::NoPiece ||
            item.attachmentOrder)
            return false;
        const int ownKing = item.color == Color::White ? 0 : 1;
        if (!substate)
            return true;
        int host = substate == 1 ? ownKing : other;
        if (substate == 3) {
            if (other < 0 || other >= position.piece_count() ||
                position.piece(other).type != PieceType::Copycat)
                return false;
            host = position.piece(other).link;
        }
        if (host < 0 || host >= position.piece_count() || host == angel ||
            !position.piece(host).alive || !position.piece(host).onBoard ||
            position.piece(host).type == PieceType::Halo ||
            position.piece(host).color != item.color ||
            (substate == 2 && (other == Position::NoPiece || host != other)))
            return false;
        const int haloSquare = item.square;
        position.erase_from_board(angel);
        item.onBoard = false;
        const int halo = position.add_piece(PieceType::Halo, item.color,
                                            haloSquare);
        if (halo == Position::NoPiece)
            return false;
        item.link = static_cast<std::int8_t>(halo);
        item.host = static_cast<std::int8_t>(host);
        item.attachmentOrder = position.nextAttachmentOrder_++;
        item.square = position.piece(host).square;
        position.piece(halo).link = static_cast<std::int8_t>(angel);
        return true;
    }

    std::optional<std::uint32_t> angel_substate(
      const Position& position, int angel, int other) const {
        if (angel < 0 || angel >= position.piece_count())
            return std::nullopt;
        const PieceState& item = position.piece(angel);
        if (!item.alive || item.type != PieceType::Angel || item.action ||
            item.cooldown || item.power || !item.visible ||
            item.parasiteTracked)
            return std::nullopt;
        if (item.onBoard)
            return item.link == Position::NoPiece &&
                   item.host == Position::NoPiece && !item.attachmentOrder
              ? std::optional<std::uint32_t>(0) : std::nullopt;
        if (item.freezeCount || item.link < 0 ||
            item.link >= position.piece_count() || item.host < 0 ||
            item.host >= position.piece_count() || !item.attachmentOrder)
            return std::nullopt;
        const PieceState& halo = position.piece(item.link);
        const PieceState& host = position.piece(item.host);
        if (!halo.alive || !halo.onBoard || halo.type != PieceType::Halo ||
            halo.color != item.color || halo.link != angel ||
            halo.host != Position::NoPiece || !host.alive || !host.onBoard ||
            host.type == PieceType::Halo || host.color != item.color ||
            item.square != host.square)
            return std::nullopt;
        const int ownKing = item.color == Color::White ? 0 : 1;
        if (item.host == ownKing)
            return 1;
        if (other != Position::NoPiece && item.host == other &&
            position.piece(other).color == item.color)
            return 2;
        if (other != Position::NoPiece && other >= 0 &&
            other < position.piece_count() &&
            position.piece(other).type == PieceType::Copycat) {
            const int clone = position.piece(other).link;
            if (clone != Position::NoPiece && clone >= 0 &&
                clone < position.piece_count() && item.host == clone &&
                position.piece(clone).alive &&
                position.piece(clone).type == PieceType::CopycatClone &&
                position.piece(clone).color == item.color)
                return 3;
        }
        return std::nullopt;
    }

    int material_board_piece(const Position& position, int id,
                             PieceType type, int other) const {
        if (type != PieceType::Angel)
            return id;
        const auto substate = angel_substate(position, id, other);
        if (!substate)
            return Position::NoPiece;
        return *substate ? position.piece(id).link : id;
    }

    std::uint8_t material_square(const Position& position, int id,
                                 PieceType type, int other) const {
        const int boardPiece = material_board_piece(position, id, type, other);
        if (boardPiece == Position::NoPiece)
            throw std::runtime_error("Angel material has no board proxy");
        return position.piece(boardPiece).square;
    }

    bool exact_angel_live_shape(const Position& position, int first,
                                int second = Position::NoPiece) const {
        int attached = 0;
        int angel = Position::NoPiece;
        int other = Position::NoPiece;
        if (attackerType_ == PieceType::Angel) {
            angel = first;
            other = second;
        }
        else if (fourModels_ && secondaryType_ == PieceType::Angel) {
            angel = second;
            other = first;
        }
        else return true;
        const auto substate = angel_substate(position, angel, other);
        if (!substate)
            return false;
        attached = *substate != 0;
        int live = 0;
        int halos = 0;
        for (int id = 0; id < position.piece_count(); ++id) {
            if (!position.piece(id).alive)
                continue;
            ++live;
            halos += position.piece(id).type == PieceType::Halo;
        }
        return live == (compoundCopycat_ ? 5 : fourModels_ ? 4 : 3) + attached &&
               halos == attached;
    }

    bool apply_substate(Position& position, int id, PieceType type,
                        std::uint32_t substate) const {
        switch (type) {
        case PieceType::Berserker: position.piece(id).power = substate; break;
        case PieceType::Ghost: position.piece(id).visible = substate != 0; break;
        case PieceType::Devil: position.piece(id).cooldown = substate; break;
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
                                 PieceType type, int other) const {
        switch (type) {
        case PieceType::Berserker:
            return std::min<std::uint32_t>(position.piece(id).power, 9);
        case PieceType::Ghost: return position.piece(id).visible ? 1 : 0;
        case PieceType::Devil: return position.piece(id).cooldown;
        case PieceType::Sniper: return position.piece(id).cooldown;
        case PieceType::Prince:
            return position.continuation_ == Continuation::PrinceSecondMove &&
                   position.forcedPiece_ == id;
        case PieceType::Checker:
            return (position.piece(id).type == PieceType::CheckerKing ? 2u : 0u) +
                   (position.continuation_ == Continuation::CheckerJump &&
                    position.forcedPiece_ == id ? 1u : 0u);
        case PieceType::Pawn: return position.piece(id).moved ? 1 : 0;
        case PieceType::Penguin: {
            std::uint32_t substate = 0;
            if (!penguin_action_substate(position, id, other, substate))
                throw std::runtime_error("Penguin action mask is outside its exact state codec");
            return substate;
        }
        case PieceType::Angel: {
            const auto substate = angel_substate(position, id, other);
            if (!substate)
                throw std::runtime_error(
                  "Angel attachment graph is outside its exact state codec");
            return *substate;
        }
        default: return 0;
        }
    }

    bool make_position_at(std::uint32_t index, Position& position) const {
        if (!fourModels_)
            return make_position(decode(index), position);
        if (linkedCopycatPair_) {
            const FourState state = decode_four(index / substates_);
            position.clear();
            const int whiteKing = position.add_piece_internal(
              PieceType::King, Color::White, state.whiteKing, false);
            const int blackKing = position.add_piece_internal(
              PieceType::King, Color::Black, state.blackKing, false);
            const int first = position.add_piece_internal(
              PieceType::Copycat, Color::White, state.first, false);
            const int second = position.add_piece_internal(
              PieceType::CopycatClone, Color::White, state.second, false);
            if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
                first == Position::NoPiece || second == Position::NoPiece)
                return false;
            position.piece(first).link = static_cast<std::int8_t>(second);
            position.piece(second).link = static_cast<std::int8_t>(first);
            for (int id : {whiteKing, blackKing, first, second})
                position.piece(id).moved = true;
            position.set_side_to_move(state.side);
            return true;
        }
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
        const FourState state = identicalCompoundCopycats_
          ? decode_identical_compound_copycat(placement)
          : compoundCopycat_ ? decode_compound_copycat(placement)
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
            if (secondaryType_ == PieceType::Copycat) {
                const int secondaryClone = position.piece(second).link;
                if (secondaryClone == Position::NoPiece ||
                    !position.piece(secondaryClone).alive ||
                    !position.piece(secondaryClone).onBoard)
                    return false;
                position.piece(secondaryClone).moved = true;
            }
        }
        if ((attackerType_ != PieceType::Angel &&
             !apply_substate(position, first, attackerType_, primarySubstate)) ||
            (secondaryType_ != PieceType::Angel &&
             !apply_substate(position, second, secondaryType_, secondarySubstate)))
            return false;
        if ((attackerType_ == PieceType::Angel &&
             !apply_angel_substate(
               position, first, second, primarySubstate)) ||
            (secondaryType_ == PieceType::Angel &&
             !apply_angel_substate(
               position, second, first, secondarySubstate)))
            return false;
        const int firstOther = material_board_piece(
          position, second, secondaryType_, first);
        const int secondOther = material_board_piece(
          position, first, attackerType_, second);
        if (firstOther == Position::NoPiece || secondOther == Position::NoPiece)
            return false;
        if ((attackerType_ == PieceType::Penguin &&
             !apply_penguin_substate(
               position, first, firstOther, primarySubstate)) ||
            (secondaryType_ == PieceType::Penguin &&
             !apply_penguin_substate(
               position, second, secondOther, secondarySubstate)))
            return false;
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
            position.piece(attacker).visible = trackedGhost_ || state.substate != 0;
            position.piece(attacker).parasiteTracked = trackedGhost_;
            break;
        case PieceType::Devil:
            position.piece(attacker).cooldown = state.substate;
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
        case PieceType::Checker:
            if (state.substate & 1u) {
                position.continuation_ = Continuation::CheckerJump;
                position.forcedPiece_ = attacker;
            }
            if (state.substate & 2u)
                position.piece(attacker).type = PieceType::CheckerKing;
            break;
        case PieceType::Pawn:
            position.piece(attacker).moved = state.substate != 0;
            break;
        case PieceType::Penguin:
            if (!apply_penguin_substate(
                  position, attacker, Position::NoPiece, state.substate))
                return false;
            break;
        case PieceType::Angel:
            if (!apply_angel_substate(
                  position, attacker, Position::NoPiece, state.substate))
                return false;
            break;
        default: break;
        }
        position.set_side_to_move(state.side);
        return true;
    }

    bool in_class(const Position& position) const {
        if (!position.has_real_king(Color::White) ||
            !position.has_real_king(Color::Black) || position.piece_count() < 3)
            return false;
        const auto member = [&](int id, PieceType represented, Color color,
                                int other, bool tracked) {
            if (id < 0 || id >= position.piece_count())
                return false;
            const PieceState& item = position.piece(id);
            if (!item.alive || item.color != color ||
                !type_matches(represented, item.type))
                return false;
            if (represented == PieceType::Angel)
                return angel_substate(position, id, other).has_value();
            return item.onBoard &&
              (represented != PieceType::Ghost ||
               (item.parasiteTracked == tracked &&
                (!tracked || item.visible)));
        };
        if (linkedCopycatPair_) {
            if (position.piece_count() < 4)
                return false;
            const PieceState& first = position.piece(2);
            const PieceState& second = position.piece(3);
            if (!first.alive || !first.onBoard ||
                first.type != PieceType::Copycat ||
                first.color != Color::White || first.link != 3 ||
                !second.alive || !second.onBoard ||
                second.type != PieceType::CopycatClone ||
                second.color != Color::White || second.link != 2)
                return false;
            int live = 0;
            for (int id = 0; id < position.piece_count(); ++id)
                live += position.piece(id).alive;
            return live == 4;
        }
        const int primaryOther = fourModels_ ? 3 : Position::NoPiece;
        const bool primary = member(
          2, attackerType_, Color::White, primaryOther, trackedGhost_);
        if (!primary)
            return false;
        if (!fourModels_)
            return exact_angel_live_shape(position, 2);
        if (compoundCopycat_) {
            const int clone = position.piece(2).link;
            const bool primaryPair =
                   clone == 3 && position.piece(3).alive && position.piece(3).onBoard &&
                   position.piece(3).type == PieceType::CopycatClone &&
                   position.piece(3).color == position.piece(2).color &&
                   position.piece(3).link == 2 &&
                   position.piece(3).square ==
                     horizontal_reflection(position.piece(2).square);
            if (!primaryPair ||
                !member(4, secondaryType_, secondaryColor_, 2, false))
                return false;
            if (secondaryType_ != PieceType::Copycat)
                return (secondaryType_ == PieceType::Angel ||
                        position.piece(4).link == Position::NoPiece) &&
                       exact_angel_live_shape(position, 2, 4);
            const int secondaryClone = position.piece(4).link;
            return secondaryClone == 5 && position.piece(5).alive &&
                   position.piece(5).onBoard &&
                   position.piece(5).type == PieceType::CopycatClone &&
                   position.piece(5).color == position.piece(4).color &&
                   position.piece(5).link == 4 &&
                   position.piece(5).square ==
                     horizontal_reflection(position.piece(4).square);
        }
        return member(3, secondaryType_, secondaryColor_, 2, false) &&
               exact_angel_live_shape(position, 2, 3);
    }

    std::uint32_t child_index(const Position& position) const {
        if (linkedCopycatPair_)
            return encode_four({position.side_to_move(),
                                position.piece(0).square,
                                position.piece(1).square,
                                position.piece(2).square,
                                position.piece(3).square});
        if (copycatOnly_)
            return encode_placement({position.side_to_move(), position.piece(0).square,
                                     position.piece(1).square, position.piece(2).square});
        if (compoundCopycat_) {
            const FourState state{position.side_to_move(), position.piece(0).square,
                                  position.piece(1).square, position.piece(2).square,
                                  material_square(
                                    position, 4, secondaryType_, 2)};
            const std::uint32_t placement = identicalCompoundCopycats_
              ? encode_identical_compound_copycat(state)
              : encode_compound_copycat(state);
            const std::uint32_t secondarySubstate =
              piece_substate(position, 4, secondaryType_, 2);
            return placement * substates_ + secondarySubstate;
        }
        if (fourModels_) {
            const int firstOther = material_board_piece(
              position, 3, secondaryType_, 2);
            const int secondOther = material_board_piece(
              position, 2, attackerType_, 3);
            if (firstOther == Position::NoPiece ||
                secondOther == Position::NoPiece)
                throw std::runtime_error(
                  "four-model material has no exact board proxy");
            std::uint32_t primarySubstate =
              piece_substate(position, 2, attackerType_,
                attackerType_ == PieceType::Angel ? 3 : firstOther);
            std::uint32_t secondarySubstate =
              piece_substate(position, 3, secondaryType_,
                secondaryType_ == PieceType::Angel ? 2 : secondOther);
            if (attackerType_ == PieceType::Penguin ||
                secondaryType_ == PieceType::Penguin) {
                const bool exact = attackerType_ == PieceType::Penguin &&
                                   secondaryType_ == PieceType::Penguin
                  ? penguin_freeze_state_matches(
                      position, 2, 3, primarySubstate, 3, 2, secondarySubstate)
                  : attackerType_ == PieceType::Penguin
                  ? penguin_freeze_state_matches(
                      position, 2, firstOther, primarySubstate)
                  : penguin_freeze_state_matches(
                      position, 3, secondOther, secondarySubstate);
                if (!exact)
                    throw std::runtime_error(
                      "Penguin freeze layers are outside their exact state codec");
            }
            const FourState state{position.side_to_move(), position.piece(0).square,
                                  position.piece(1).square,
                                  material_square(position, 2, attackerType_, 3),
                                  material_square(position, 3, secondaryType_, 2)};
            std::uint32_t placement = 0;
            if (identicalExtras_) {
                FourState canonical = canonicalize_four(state);
                const std::uint32_t firstRank = rank_excluding(
                  canonical.first, {canonical.whiteKing, canonical.blackKing});
                const std::uint32_t secondRank = rank_excluding(
                  canonical.second, {canonical.whiteKing, canonical.blackKing});
                if (firstRank > secondRank)
                    std::swap(primarySubstate, secondarySubstate);
                placement = encode_identical_four_material(state);
            }
            else placement = encode_four_material(state);
            return placement * substates_ +
                   primarySubstate * secondarySubstates_ + secondarySubstate;
        }
        std::uint8_t substate = 0;
        switch (attackerType_) {
        case PieceType::Berserker:
            substate = std::min<std::uint8_t>(position.piece(2).power, 9);
            break;
        case PieceType::Ghost:
            substate = trackedGhost_ ? 0 : position.piece(2).visible ? 1 : 0;
            break;
        case PieceType::Devil: substate = position.piece(2).cooldown; break;
        case PieceType::Sniper: substate = position.piece(2).cooldown; break;
        case PieceType::Prince:
            substate = position.continuation_ == Continuation::PrinceSecondMove ? 1 : 0;
            break;
        case PieceType::Checker:
            substate = (position.piece(2).type == PieceType::CheckerKing ? 2u : 0u) +
              (position.continuation_ == Continuation::CheckerJump ? 1u : 0u);
            break;
        case PieceType::Pawn: substate = position.piece(2).moved ? 1 : 0; break;
        case PieceType::Penguin: {
            substate = static_cast<std::uint8_t>(
              piece_substate(position, 2, attackerType_, Position::NoPiece));
            if (!penguin_freeze_state_matches(
                  position, 2, Position::NoPiece, substate))
                throw std::runtime_error(
                  "Penguin freeze layers are outside their exact state codec");
            break;
        }
        case PieceType::Angel:
            substate = static_cast<std::uint8_t>(
              piece_substate(position, 2, attackerType_, Position::NoPiece));
            break;
        default: break;
        }
        return encode({position.side_to_move(), position.piece(0).square,
                       position.piece(1).square,
                       material_square(position, 2, attackerType_,
                                       Position::NoPiece), substate});
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
        if (initialize) {
            Node& node = nodes_[index];
            node = {};
        }
        const std::uint32_t legalMoves = for_each_legal_successor(
          position, [&](const Move&, const Position& child) {
            if (initialize) {
                if (nodes_[index].remaining ==
                    std::numeric_limits<std::uint16_t>::max())
                    throw std::runtime_error(
                      "tablebase node exceeds the exact move-count plane");
                ++nodes_[index].remaining;
            }
            if (terminal_successor(child)) {
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
                    }
                }
                return;
            }
            if (!in_class(child)) {
                if (initialize) {
                    const auto external = TablebaseProbe::probe(child);
                    if (!external)
                        throw std::runtime_error(
                          "missing exact lower-material table for nonterminal child: " +
                          child.upn());
                    const bool sameSide = child.side_to_move() == position.side_to_move();
                    const Wdl outcome = parent_wdl(external->wdl, sameSide);
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
                    }
                }
                return;  // Captures enter K-v-K; promotions use a lower table.
            }
            const std::uint32_t successor = child_index(child);
            if (successor >= stateCount_)
                throw std::runtime_error("child index exceeds tablebase domain at parent " +
                                         std::to_string(index));
            consume(successor, child.side_to_move() == position.side_to_move());
        });
        if (!initialize)
            return;
        Node& node = nodes_[index];
        if (!legalMoves) {
            const auto winner = position.winner();
            node.wdl = winner && *winner != position.side_to_move()
              ? Wdl::Loss : Wdl::Draw;
        }
        else if (node.wdl == Wdl::Unknown && !node.remaining) {
            node.wdl = Wdl::Loss;
            node.dtw = static_cast<std::uint16_t>(node.longestWinChild + 1);
        }
    }

    void save_checkpoint(std::uint32_t processed) const {
        const std::string temporary = checkpoint_ + ".tmp";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("cannot write tablebase checkpoint");
        const std::array<char, 8> magic{{'U','F','T','B','C','P','4','\0'}};
        const std::uint32_t piece = static_cast<std::uint32_t>(attackerType_);
        const std::uint32_t secondary = static_cast<std::uint32_t>(secondaryType_);
        const std::uint32_t secondaryColor = static_cast<std::uint32_t>(secondaryColor_);
        stream.write(magic.data(), magic.size());
        stream.write(reinterpret_cast<const char*>(&piece), sizeof(piece));
        stream.write(reinterpret_cast<const char*>(&secondary), sizeof(secondary));
        stream.write(reinterpret_cast<const char*>(&secondaryColor), sizeof(secondaryColor));
        stream.write(reinterpret_cast<const char*>(&stateCount_), sizeof(stateCount_));
        stream.write(reinterpret_cast<const char*>(&substates_), sizeof(substates_));
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
        std::uint32_t piece = 0, secondary = 0, secondaryColor = 0;
        std::uint32_t stateCount = 0, substates = 0, processed = 0;
        stream.read(magic.data(), magic.size());
        stream.read(reinterpret_cast<char*>(&piece), sizeof(piece));
        stream.read(reinterpret_cast<char*>(&secondary), sizeof(secondary));
        stream.read(reinterpret_cast<char*>(&secondaryColor), sizeof(secondaryColor));
        stream.read(reinterpret_cast<char*>(&stateCount), sizeof(stateCount));
        stream.read(reinterpret_cast<char*>(&substates), sizeof(substates));
        stream.read(reinterpret_cast<char*>(&processed), sizeof(processed));
        const std::array<char, 8> expected{{'U','F','T','B','C','P','4','\0'}};
        if (magic != expected || piece != static_cast<std::uint32_t>(attackerType_) ||
            secondary != static_cast<std::uint32_t>(secondaryType_) ||
            secondaryColor != static_cast<std::uint32_t>(secondaryColor_) ||
            stateCount != stateCount_ || substates != substates_ ||
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
        // v7 authenticates the corrected lower-left Giant-anchor reflection.
        // Folded Giant payloads from v4-v6 used point-square reflection and
        // must never be interpreted by the corrected codec.
        const bool foldedGiant = fourModels_ &&
          (primary_is_giant() || secondary_is_giant());
        const bool angelGraph = attackerType_ == PieceType::Angel ||
          secondaryType_ == PieceType::Angel;
        const bool angelCopycatGraph = compoundCopycat_ &&
          secondaryType_ == PieceType::Angel && secondaryColor_ == Color::White;
        const std::uint32_t version = linkedCopycatPair_ || angelCopycatGraph
          ? 10 : angelGraph ? 9 : trackedGhost_ ? 8
          : foldedGiant ? 7
          : edges > std::numeric_limits<std::uint32_t>::max() ? 6
          : fourModels_ ? 5 : 4;
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
        if (version >= 7) {
            const std::uint64_t codecTag = linkedCopycatPair_
              ? LinkedCopycatPairV1Tag
              : angelCopycatGraph ? AngelCopycatGraphV1Tag
              : angelGraph
              ? (foldedGiant ? AngelGiantGraphV1Tag : AngelGraphV1Tag)
              : trackedGhost_ ? TrackedGhostV1Tag : GiantAnchorV2Tag;
            stream.write(reinterpret_cast<const char*>(&codecTag),
                         sizeof(GiantAnchorV2Tag));
        }
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
            bool hasLoss = false;
            bool hasDraw = false;
            bool allWin = true;
            std::uint16_t shortestLoss = std::numeric_limits<std::uint16_t>::max();
            std::uint16_t longestWin = 0;
            const std::uint32_t legalMoves = for_each_legal_successor(
              position, [&](const Move&, const Position& child) {
                if (terminal_successor(child)) {
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
                    return;
                }
                if (!in_class(child)) {
                    const auto external = TablebaseProbe::probe(child);
                    if (!external)
                        throw std::runtime_error(
                          "verification lacks exact lower-material child: " +
                          child.upn());
                    const bool sameSide = child.side_to_move() == position.side_to_move();
                    const Wdl outcome = parent_wdl(external->wdl, sameSide);
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
                    return;
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
            });
            allWin = legalMoves != 0 && allWin;
            bool valid = false;
            if (node.wdl == Wdl::Win)
                valid = hasLoss && node.dtw == shortestLoss + 1;
            else if (node.wdl == Wdl::Loss)
                valid = (!legalMoves && node.dtw == 0) ||
                        (allWin && node.dtw == longestWin + 1);
            else if (node.wdl == Wdl::Draw)
                valid = !hasLoss && (!legalMoves || hasDraw);
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
        const std::uint32_t workers = std::min(
          workerThreads_, std::max(1u, std::thread::hardware_concurrency()));
        constexpr std::uint32_t Block = 4096;
        std::atomic<std::uint32_t> next{0};
        std::vector<std::future<void>> tasks;
        for (std::uint32_t worker = 0; worker < workers; ++worker)
            tasks.push_back(std::async(std::launch::async, [this, &next] {
                while (true) {
                    const std::uint32_t begin = next.fetch_add(
                      Block, std::memory_order_relaxed);
                    if (begin >= stateCount_)
                        break;
                    verify_range(begin, std::min(
                      stateCount_, static_cast<std::uint32_t>(begin + Block)));
                }
            }));
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
    bool trackedGhost_;
    std::uint32_t workerThreads_;
    bool linkedCopycatPair_;
    bool copycatOnly_;
    bool compoundCopycat_;
    bool identicalCompoundCopycats_;
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
    std::string devilReverseSelfTest;
    std::string devilCheckpointMigrationSelfTest;
    std::string devilCheckpointMigrationSource;
    std::string devilCheckpointMigrationDestination;
    bool diskBacked = false;
    bool trackedGhost = false;
    bool linkedCopycatPair = false;
    std::uint32_t workerThreads = 4;
    std::uint32_t dryRun = 0;
    std::uint32_t dryRunBegin = 0;
    std::uint32_t inspect = std::numeric_limits<std::uint32_t>::max();
    std::string auditPredecessorSafety;
    std::string auditReachability;
    std::string auditTurnBoundaryReachability;
    std::string auditInformationTrivial;
    std::string solveJesterInformation;
    std::string informationOverlay;
    std::string lowerInformationOverlay;
    std::string lowerInformationSourceSha256;
    std::string lowerInformationModelSha256;
    std::string lowerExtraInformationOverlay;
    std::string lowerExtraInformationSourceSha256;
    std::string lowerExtraInformationModelSha256;
    std::string informationSourceSha256;
    std::string informationModelSha256;
    bool transposeInformationSubstates = false;
    std::string informationScratch = "/tmp";
    bool referenceInformationSolver = false;
    std::uint32_t devilFrontierDepth = 0;
    std::uint32_t devilFrontierShard = 0;
    std::uint32_t devilFrontierShards = 1;
    std::uint64_t devilFrontierLimit = 2'000'000;
    int devilSpawnedSquare = Position::NoSquare;
    std::uint64_t devilSpawnedLimit = 250'000'000;
    std::uint64_t devilSpawnedHashCapacity = 0;
    std::string devilSpawnedWork;
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
        else if (argument == "--tracked-ghost") trackedGhost = true;
        else if (argument == "--linked-copycat-pair") linkedCopycatPair = true;
        else if (argument == "--workers")
            workerThreads = static_cast<std::uint32_t>(
              std::stoul(value("--workers")));
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
        else if (argument == "--audit-turn-boundary-reachability")
            auditTurnBoundaryReachability =
              value("--audit-turn-boundary-reachability");
        else if (argument == "--audit-information-trivial")
            auditInformationTrivial = value("--audit-information-trivial");
        else if (argument == "--solve-jester-information")
            solveJesterInformation = value("--solve-jester-information");
        else if (argument == "--information-overlay")
            informationOverlay = value("--information-overlay");
        else if (argument == "--lower-information-overlay")
            lowerInformationOverlay = value("--lower-information-overlay");
        else if (argument == "--lower-information-source-sha256")
            lowerInformationSourceSha256 = value("--lower-information-source-sha256");
        else if (argument == "--lower-information-model-sha256")
            lowerInformationModelSha256 = value("--lower-information-model-sha256");
        else if (argument == "--lower-extra-information-overlay")
            lowerExtraInformationOverlay = value("--lower-extra-information-overlay");
        else if (argument == "--lower-extra-information-source-sha256")
            lowerExtraInformationSourceSha256 =
              value("--lower-extra-information-source-sha256");
        else if (argument == "--lower-extra-information-model-sha256")
            lowerExtraInformationModelSha256 =
              value("--lower-extra-information-model-sha256");
        else if (argument == "--information-source-sha256")
            informationSourceSha256 = value("--information-source-sha256");
        else if (argument == "--information-model-sha256")
            informationModelSha256 = value("--information-model-sha256");
        else if (argument == "--information-transpose-substates")
            transposeInformationSubstates = true;
        else if (argument == "--information-scratch")
            informationScratch = value("--information-scratch");
        else if (argument == "--reference-information-solver")
            referenceInformationSolver = true;
        else if (argument == "--devil-frontier-depth")
            devilFrontierDepth = static_cast<std::uint32_t>(
              std::stoul(value("--devil-frontier-depth")));
        else if (argument == "--devil-frontier-shard")
            devilFrontierShard = static_cast<std::uint32_t>(
              std::stoul(value("--devil-frontier-shard")));
        else if (argument == "--devil-frontier-shards")
            devilFrontierShards = static_cast<std::uint32_t>(
              std::stoul(value("--devil-frontier-shards")));
        else if (argument == "--devil-frontier-limit")
            devilFrontierLimit = std::stoull(value("--devil-frontier-limit"));
        else if (argument == "--solve-devil-spawned-square")
            devilSpawnedSquare = std::stoi(value("--solve-devil-spawned-square"));
        else if (argument == "--devil-spawned-limit")
            devilSpawnedLimit = std::stoull(value("--devil-spawned-limit"));
        else if (argument == "--devil-spawned-hash-capacity")
            devilSpawnedHashCapacity =
              std::stoull(value("--devil-spawned-hash-capacity"));
        else if (argument == "--devil-spawned-work")
            devilSpawnedWork = value("--devil-spawned-work");
        else if (argument == "--devil-reverse-self-test")
            devilReverseSelfTest = value("--devil-reverse-self-test");
        else if (argument == "--devil-checkpoint-migration-self-test")
            devilCheckpointMigrationSelfTest =
              value("--devil-checkpoint-migration-self-test");
        else if (argument == "--migrate-devil-checkpoint-v2" ||
                 argument == "--migrate-devil-checkpoint")
            devilCheckpointMigrationSource =
              value("--migrate-devil-checkpoint");
        else if (argument == "--devil-migration-destination")
            devilCheckpointMigrationDestination =
              value("--devil-migration-destination");
        else if (argument == "--self-test") selfTest = true;
        else if (argument == "--four-codec-self-test") fourCodecSelfTest = true;
        else throw std::runtime_error("unknown argument: " + argument);
    }
    try {
        if (trackedGhost &&
            (attackerType != PieceType::Ghost || secondaryType != PieceType::Count))
            throw std::runtime_error(
              "--tracked-ghost requires the K+tracked-Ghost-v-K class");
        if (linkedCopycatPair &&
            (attackerType != PieceType::Copycat ||
             secondaryType != PieceType::Count ||
             secondaryColor != Color::White || trackedGhost))
            throw std::runtime_error(
              "--linked-copycat-pair requires the same-team K+linked-Copycat-v-K class");
        if (fourCodecSelfTest) {
            self_test_four_codec();
            return 0;
        }
        if (!devilReverseSelfTest.empty()) {
            self_test_devil_reverse_spool(devilReverseSelfTest);
            return 0;
        }
        if (!devilCheckpointMigrationSelfTest.empty()) {
            self_test_spawned_devil_checkpoint_migration(
              devilCheckpointMigrationSelfTest);
            return 0;
        }
        if (!devilCheckpointMigrationSource.empty() ||
            !devilCheckpointMigrationDestination.empty()) {
            if (devilCheckpointMigrationSource.empty() ||
                devilCheckpointMigrationDestination.empty())
                throw std::runtime_error(
                  "Devil checkpoint migration requires source and destination");
            migrate_spawned_devil_checkpoint(
              devilCheckpointMigrationSource,
              devilCheckpointMigrationDestination);
            return 0;
        }
        if (secondaryType == PieceType::Count) {
            if (!devilFrontierDepth && devilSpawnedSquare == Position::NoSquare &&
                !closed_position_only_attacker(attackerType) &&
                !(attackerType == PieceType::Devil &&
                  (!auditPredecessorSafety.empty() ||
                   !auditReachability.empty())))
                throw std::runtime_error("piece is not a closed K+K+1 tablebase class");
        }
        else if (!devilFrontierDepth) {
            if (attackerType == PieceType::Angel &&
                secondaryType == PieceType::Angel)
                throw std::runtime_error(
                  "two Angels require the ordered multi-rescue codec");
            const bool closedUnsplitCopycat =
              attackerType == PieceType::Copycat &&
              (closed_unsplit_copycat_secondary(secondaryType) ||
               secondaryType == PieceType::Angel);
            const bool closedAngelGhost =
              (attackerType == PieceType::Angel &&
               secondaryType == PieceType::Ghost) ||
              (attackerType == PieceType::Ghost &&
               secondaryType == PieceType::Angel);
            const bool closedSpawnedDevilCompanion =
              devilSpawnedSquare != Position::NoSquare &&
              attackerType == PieceType::Devil &&
              stateless_four_piece(secondaryType);
            if ((!closed_four_piece(attackerType) ||
                 !closed_four_piece(secondaryType)) && !closedUnsplitCopycat &&
                !closedAngelGhost && !closedSpawnedDevilCompanion)
                throw std::runtime_error("K+K+2 piece requires a larger non-closed model");
        }
        const std::uint32_t hardwareWorkers = std::max(
          1u, std::thread::hardware_concurrency());
        if (!workerThreads || workerThreads > hardwareWorkers)
            throw std::runtime_error(
              "tablebase workers exceed available hardware concurrency");
        TablebaseGenerator generator(attackerType, secondaryType, secondaryColor,
                                     output, checkpoint, checkpointEvery, diskBacked,
                                     trackedGhost, linkedCopycatPair,
                                     workerThreads);
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
        if (!auditTurnBoundaryReachability.empty())
            generator.audit_reachability(
              auditTurnBoundaryReachability, true, true);
        if (!auditInformationTrivial.empty()) {
            if (informationOverlay.empty())
                throw std::runtime_error(
                  "--audit-information-trivial requires --information-overlay");
            generator.audit_information_trivial(
              auditInformationTrivial, informationOverlay,
              informationSourceSha256, informationModelSha256,
              transposeInformationSubstates);
        }
        if (devilFrontierDepth)
            generator.census_devil_frontier(
              devilFrontierDepth, devilFrontierShard,
              devilFrontierShards, devilFrontierLimit);
        if (devilSpawnedSquare != Position::NoSquare)
            generator.solve_devil_spawned_square(
              devilSpawnedSquare, devilSpawnedLimit,
              devilSpawnedHashCapacity, devilSpawnedWork);
        if (!solveJesterInformation.empty()) {
            if (referenceInformationSolver)
                generator.solve_jester_information_reference(
                  solveJesterInformation, informationOverlay);
            else
                generator.solve_jester_information(
                  solveJesterInformation, lowerInformationOverlay,
                  lowerInformationSourceSha256, lowerInformationModelSha256,
                  lowerExtraInformationOverlay,
                  lowerExtraInformationSourceSha256,
                  lowerExtraInformationModelSha256,
                  informationSourceSha256,
                  informationModelSha256, informationOverlay,
                  informationScratch);
        }
        if (!selfTest && !dryRun && inspect == std::numeric_limits<std::uint32_t>::max() &&
            auditPredecessorSafety.empty() && auditReachability.empty() &&
            auditTurnBoundaryReachability.empty() &&
            auditInformationTrivial.empty() &&
            solveJesterInformation.empty() && !devilFrontierDepth &&
            devilSpawnedSquare == Position::NoSquare)
            generator.generate();
    }
    catch (const std::exception& error) {
        std::cerr << "tablebase error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
