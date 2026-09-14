// Fast, exact root filtering for the lone-Devil stateful plot audit. GPLv3+.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace UltimateDevilSlice {

constexpr unsigned BoardFiles = 8;
constexpr unsigned BoardRanks = 10;
constexpr unsigned BoardSquares = BoardFiles * BoardRanks;
constexpr unsigned MaxMinions = 5;

constexpr std::uint64_t choose(unsigned n, unsigned k) {
    if (k > n)
        return 0;
    k = std::min(k, n - k);
    std::uint64_t value = 1;
    for (unsigned item = 1; item <= k; ++item)
        value = value * (n - k + item) / item;
    return value;
}

constexpr std::uint64_t MinionCodeCount =
  choose(80, 0) + choose(80, 1) + choose(80, 2) + choose(80, 3) +
  choose(80, 4) + choose(80, 5);

struct State {
    std::uint64_t key = 0;
    std::uint64_t minionCode = 0;
    unsigned minionCount = 0;
    unsigned whiteKing = 0;
    unsigned blackKing = 0;
    unsigned secondary = 0;
    unsigned cooldown = 0;
    unsigned side = 0;
    bool alive = false;
};

struct Minions {
    std::array<unsigned, MaxMinions> squares{};
    unsigned count = 0;

    [[nodiscard]] bool contains(unsigned square) const {
        for (unsigned index = 0; index < count; ++index)
            if (squares[index] == square)
                return true;
        return false;
    }
};

enum class Bucket : unsigned { Dead, Excluded, Trivial, Display };

inline bool adjacent(unsigned lhs, unsigned rhs) {
    const int fileDistance = static_cast<int>(lhs % BoardFiles) -
      static_cast<int>(rhs % BoardFiles);
    const int rankDistance = static_cast<int>(lhs / BoardFiles) -
      static_cast<int>(rhs / BoardFiles);
    return std::abs(fileDistance) <= 1 && std::abs(rankDistance) <= 1;
}

inline bool within_spawn_area(unsigned square, unsigned devilSquare) {
    const int fileDistance = static_cast<int>(square % BoardFiles) -
      static_cast<int>(devilSquare % BoardFiles);
    const int rankDistance = static_cast<int>(square / BoardFiles) -
      static_cast<int>(devilSquare / BoardFiles);
    return square != devilSquare && std::abs(fileDistance) <= 2 &&
      std::abs(rankDistance) <= 2;
}

inline State decode(std::uint64_t key) {
    if (key >> 49)
        throw std::runtime_error("Devil logical key exceeds 49 bits");
    State state;
    state.key = key;
    state.minionCode = key & ((std::uint64_t{1} << 25) - 1);
    std::uint64_t lower = 0;
    for (unsigned count = 0; count <= MaxMinions; ++count) {
        const std::uint64_t upper = lower + choose(BoardSquares, count);
        if (state.minionCode < upper) {
            state.minionCount = count;
            break;
        }
        lower = upper;
        if (count == MaxMinions)
            throw std::runtime_error("invalid Devil Minion combination rank");
    }
    const unsigned kings = static_cast<unsigned>((key >> 25) & 0x1fffULL);
    state.whiteKing = kings / 79;
    const unsigned blackIndex = kings % 79;
    state.blackKing = blackIndex >= state.whiteKing
      ? blackIndex + 1 : blackIndex;
    state.secondary = static_cast<unsigned>((key >> 38) & 0x7fULL);
    state.cooldown = static_cast<unsigned>((key >> 45) & 3ULL);
    state.side = static_cast<unsigned>((key >> 47) & 1ULL);
    state.alive = ((key >> 48) & 1ULL) != 0;
    if (state.whiteKing >= BoardSquares || state.blackKing >= BoardSquares ||
        state.whiteKing == state.blackKing || state.secondary > BoardSquares)
        throw std::runtime_error("invalid Devil compact root fields");
    return state;
}

inline Minions decode_minions(const State& state) {
    Minions result;
    result.count = state.minionCount;
    std::uint64_t offset = 0;
    for (unsigned count = 0; count < state.minionCount; ++count)
        offset += choose(BoardSquares, count);
    std::uint64_t rank = state.minionCode - offset;
    unsigned maximum = BoardSquares - 1;
    for (unsigned ordinal = state.minionCount; ordinal; --ordinal) {
        while (choose(maximum, ordinal) > rank) {
            if (!maximum)
                throw std::runtime_error("invalid Devil Minion unrank");
            --maximum;
        }
        result.squares[ordinal - 1] = maximum;
        rank -= choose(maximum, ordinal);
        if (maximum)
            --maximum;
    }
    if (rank)
        throw std::runtime_error("Devil Minion unrank remainder");
    return result;
}

inline bool admitted(const State& state, unsigned devilSquare,
                     const Minions* = nullptr) {
    if (!state.alive)
        return false;
    if (devilSquare >= BoardSquares || devilSquare / BoardFiles >= 3 ||
        devilSquare % BoardFiles >= 4)
        throw std::runtime_error("invalid canonical fixed Devil square");
    if (state.secondary != BoardSquares)
        throw std::runtime_error("lone-Devil state has a secondary character");
    if (state.whiteKing == devilSquare || state.blackKing == devilSquare)
        throw std::runtime_error("Devil overlaps a King in certified state");
    if (adjacent(state.whiteKing, state.blackKing))
        return false;
    // Match the canonical plot's root-substate semantics: as with Berserker
    // power, all encoded Devil cooldown values are eligible starting
    // substates. Causality of Minion configurations comes from membership in
    // the certified stateful closure, while ordinary predecessor safety is
    // the reachability filter applied to the root geometry.
    return true;
}

inline bool minion_advances_onto(const Minions& minions, unsigned square) {
    return square >= BoardFiles && minions.contains(square - BoardFiles);
}

inline bool white_king_has_move(const State& state, unsigned devilSquare,
                                const Minions& minions) {
    const int file = static_cast<int>(state.whiteKing % BoardFiles);
    const int rank = static_cast<int>(state.whiteKing / BoardFiles);
    for (int df = -1; df <= 1; ++df)
        for (int dr = -1; dr <= 1; ++dr) {
            if (!df && !dr)
                continue;
            const int targetFile = file + df;
            const int targetRank = rank + dr;
            if (targetFile < 0 || targetFile >= static_cast<int>(BoardFiles) ||
                targetRank < 0 || targetRank >= static_cast<int>(BoardRanks))
                continue;
            const unsigned target = static_cast<unsigned>(
              targetRank * static_cast<int>(BoardFiles) + targetFile);
            if (target == devilSquare || minions.contains(target) ||
                adjacent(target, state.blackKing))
                continue;
            return true;
        }
    return false;
}

struct BlackMoves {
    bool legal = false;
    bool leavesClass = false;
};

inline BlackMoves black_moves(const State& state, unsigned devilSquare,
                              const Minions& minions) {
    BlackMoves result;
    const int file = static_cast<int>(state.blackKing % BoardFiles);
    const int rank = static_cast<int>(state.blackKing / BoardFiles);
    for (int df = -1; df <= 1; ++df)
        for (int dr = -1; dr <= 1; ++dr) {
            if (!df && !dr)
                continue;
            const int targetFile = file + df;
            const int targetRank = rank + dr;
            if (targetFile < 0 || targetFile >= static_cast<int>(BoardFiles) ||
                targetRank < 0 || targetRank >= static_cast<int>(BoardRanks))
                continue;
            const unsigned target = static_cast<unsigned>(
              targetRank * static_cast<int>(BoardFiles) + targetFile);
            const bool capturedMinion = minions.contains(target);
            const auto surviving_minion_at = [&](unsigned square) {
                return minions.contains(square) &&
                  !(capturedMinion && square == target);
            };
            const auto minion_advances_to = [&](unsigned square) {
                return square >= BoardFiles &&
                  surviving_minion_at(square - BoardFiles);
            };
            const bool blackKingAlive = !minion_advances_to(target);
            const bool whiteKingAlive = target != state.whiteKing &&
              !minion_advances_to(state.whiteKing);
            const bool devilAlive = target != devilSquare &&
              !minion_advances_to(devilSquare);
            if (!blackKingAlive ||
                (whiteKingAlive && adjacent(target, state.whiteKing)))
                continue;
            result.legal = true;
            unsigned survivingMinions = 0;
            for (unsigned index = 0; index < minions.count; ++index)
                if ((!capturedMinion || minions.squares[index] != target) &&
                    minions.squares[index] / BoardFiles != BoardRanks - 1)
                    ++survivingMinions;
            result.leavesClass = result.leavesClass || !whiteKingAlive ||
              (!devilAlive && !survivingMinions);
        }
    return result;
}

inline bool trivial(const State& state, unsigned devilSquare,
                    const Minions* decoded = nullptr) {
    const Minions minions = decoded ? *decoded : decode_minions(state);
    if (state.side == 0) {
        // With fewer than five Minions, cooldown 0 always has an empty spawn
        // cell even in a corner (8 cells versus at most 7 other occupants).
        if (state.cooldown == 0 && state.minionCount < MaxMinions)
            return false;
        // A five-Minion cooldown-0 root can immediately spawn outside the
        // certified at-most-five stateful class and is therefore trivial.
        if (state.cooldown == 0)
            return true;
        return !white_king_has_move(state, devilSquare, minions);
    }
    const BlackMoves moves = black_moves(state, devilSquare, minions);
    // ChangeTurn advances Ivory's Minions before classifying a trapped Onyx
    // king. A simultaneous royal collision has no opposing winner and is a
    // stalemate; a surviving Ivory king makes the Onyx collision checkmate.
    const bool check = minion_advances_onto(minions, state.blackKing) &&
      !minion_advances_onto(minions, state.whiteKing);
    return moves.leavesClass || (!moves.legal && !check);
}

inline Bucket classify(const State& state, unsigned devilSquare) {
    if (!state.alive)
        return Bucket::Dead;
    const bool needsMinions = state.side == 1 || state.cooldown != 0 ||
      state.minionCount == MaxMinions;
    const Minions minions = needsMinions ? decode_minions(state) : Minions{};
    if (!admitted(state, devilSquare, needsMinions ? &minions : nullptr))
        return Bucket::Excluded;
    return trivial(state, devilSquare, needsMinions ? &minions : nullptr)
      ? Bucket::Trivial : Bucket::Display;
}

}  // namespace UltimateDevilSlice
