/* Cross-check the fast Devil plot classifier against native Position. GPLv3+. */

#include "ultimate_devil_stateful_slice.h"

#include "position.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using Stockfish::Ultimate::Color;
using Stockfish::Ultimate::Move;
using Stockfish::Ultimate::PieceType;
using Stockfish::Ultimate::Position;
using Stockfish::Ultimate::TerminalReason;
using Stockfish::Ultimate::Undo;
using UltimateDevilSlice::Minions;
using UltimateDevilSlice::State;

constexpr std::array<unsigned, 12> FixedSquares{
  0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19};

Position make_position(const State& state, unsigned devilSquare,
                       const Minions& minions) {
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, static_cast<int>(state.whiteKing));
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, static_cast<int>(state.blackKing));
    const int devil = position.add_piece(
      PieceType::Devil, Color::White, static_cast<int>(devilSquare));
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        devil == Position::NoPiece)
        throw std::runtime_error("cannot construct Devil verifier position");
    position.piece(whiteKing).moved = true;
    position.piece(blackKing).moved = true;
    position.piece(devil).moved = true;
    position.piece(devil).cooldown = static_cast<std::uint8_t>(state.cooldown);
    for (unsigned index = 0; index < minions.count; ++index) {
        const int minion = position.add_piece(
          PieceType::Minion, Color::White,
          static_cast<int>(minions.squares[index]));
        if (minion == Position::NoPiece)
            throw std::runtime_error("cannot construct Devil verifier Minion");
        position.piece(minion).moved = true;
    }
    position.set_side_to_move(state.side ? Color::Black : Color::White);
    return position;
}

bool stateful_class(const Position& position, unsigned fixedSquare) {
    bool whiteKing = false;
    bool blackKing = false;
    bool devil = false;
    unsigned minions = 0;
    const int minimumFile = std::max(
      0, static_cast<int>(fixedSquare % Position::BoardFiles) - 2);
    const int maximumFile = std::min(
      Position::BoardFiles - 1,
      static_cast<int>(fixedSquare % Position::BoardFiles) + 2);
    for (int id = 0; id < position.piece_count(); ++id) {
        const auto& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        if (piece.type == PieceType::King) {
            (piece.color == Color::White ? whiteKing : blackKing) = true;
            continue;
        }
        if (piece.type == PieceType::Devil && piece.color == Color::White &&
            piece.square == fixedSquare && !devil) {
            devil = true;
            continue;
        }
        if (piece.type == PieceType::Minion && piece.color == Color::White &&
            piece.square % Position::BoardFiles >= minimumFile &&
            piece.square % Position::BoardFiles <= maximumFile) {
            ++minions;
            continue;
        }
        return false;
    }
    return whiteKing && blackKing && minions <= UltimateDevilSlice::MaxMinions &&
      (devil || minions);
}

bool slow_trivial(const Position& position, unsigned fixedSquare) {
    const std::vector<Move> moves = position.legal_moves();
    if (moves.empty())
        return position.terminal_reason() == TerminalReason::Stalemate;
    const auto leaves = [&](const Position& parent, const Move& move) {
        Position child = parent;
        Undo undo;
        return child.make_move(move, undo) && !stateful_class(child, fixedSquare);
    };
    for (const Move& move : moves)
        if (leaves(position, move))
            return true;
    // In this material class only the Kings attack. An admitted position has
    // non-adjacent Kings, while Devil spawns and automatic Minions do not add
    // attack-map entries, so the generic checked/pinned two-ply branch cannot
    // fire after ordinary-predecessor filtering.
    return false;
}

std::uint64_t encode_minions(const Minions& minions) {
    std::uint64_t rank = 0;
    for (unsigned count = 0; count < minions.count; ++count)
        rank += UltimateDevilSlice::choose(Position::BoardSquares, count);
    for (unsigned index = 0; index < minions.count; ++index)
        rank += UltimateDevilSlice::choose(minions.squares[index], index + 1);
    return rank;
}

void check_minion_codec(State state, const Minions& minions) {
    state.minionCode = encode_minions(minions);
    state.minionCount = minions.count;
    const Minions decoded = UltimateDevilSlice::decode_minions(state);
    if (decoded.count != minions.count ||
        !std::equal(decoded.squares.begin(),
                    decoded.squares.begin() + decoded.count,
                    minions.squares.begin()))
        throw std::runtime_error("fast Devil Minion codec round-trip residual");
}

void check(const State& state, unsigned fixedSquare, const Minions& minions,
           std::uint64_t& checked, std::uint64_t& admitted) {
    const Position position = make_position(state, fixedSquare, minions);
    const bool slowAdmitted = position.ordinary_predecessor_king_safe();
    const bool fastAdmitted = UltimateDevilSlice::admitted(
      state, fixedSquare, &minions);
    if (slowAdmitted != fastAdmitted)
        throw std::runtime_error("fast Devil admission disagrees with Position");
    ++checked;
    if (!slowAdmitted)
        return;
    ++admitted;
    const bool slow = slow_trivial(position, fixedSquare);
    const bool fast = UltimateDevilSlice::trivial(
      state, fixedSquare, &minions);
    if (slow != fast)
        throw std::runtime_error(
          "fast Devil trivial test disagrees with Position: slow=" +
          std::to_string(slow) + " fast=" + std::to_string(fast) +
          " fixed=" + std::to_string(fixedSquare) + " side=" +
          std::to_string(state.side) + " cooldown=" +
          std::to_string(state.cooldown) + " minions=" +
          std::to_string(state.minionCount) + " upn=" + position.upn());
}

std::uint64_t random_next(std::uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

}  // namespace

int main(int argc, char** argv) try {
    const std::uint64_t samples = argc > 1 ? std::stoull(argv[1]) : 250000;
    std::uint64_t checked = 0;
    std::uint64_t admitted = 0;
    // Native turn-start mate witness: b8 protects a9/b9, whose automatic
    // advances trap the bare king on a10. This must remain in the plot.
    State mate;
    mate.whiteKing = 57;
    mate.blackKing = 72;
    mate.secondary = Position::BoardSquares;
    mate.side = 1;
    mate.cooldown = 0;
    mate.alive = true;
    mate.minionCount = 2;
    Minions matingMinions;
    matingMinions.count = 2;
    matingMinions.squares[0] = 64;
    matingMinions.squares[1] = 65;
    if (UltimateDevilSlice::trivial(mate, 0, &matingMinions) ||
        make_position(mate, 0, matingMinions).terminal_reason() !=
          TerminalReason::Checkmate)
        throw std::runtime_error("Minion checkmate was excluded from plot");
    check(mate, 0, matingMinions, checked, admitted);
    const Minions none{};
    for (const unsigned fixedSquare : FixedSquares)
        for (unsigned whiteKing = 0; whiteKing < Position::BoardSquares;
             ++whiteKing) {
            if (whiteKing == fixedSquare)
                continue;
            for (unsigned blackKing = 0; blackKing < Position::BoardSquares;
                 ++blackKing) {
                if (blackKing == fixedSquare || blackKing == whiteKing)
                    continue;
                for (unsigned side = 0; side < 2; ++side)
                    for (unsigned cooldown = 0; cooldown < 4; ++cooldown) {
                        State state;
                        state.whiteKing = whiteKing;
                        state.blackKing = blackKing;
                        state.secondary = Position::BoardSquares;
                        state.cooldown = cooldown;
                        state.side = side;
                        state.alive = true;
                        check(state, fixedSquare, none, checked, admitted);
                    }
            }
        }

    std::uint64_t random = 0x4d595df4d0f33173ULL;
    for (std::uint64_t sample = 0; sample < samples; ++sample) {
        const unsigned fixedSquare = FixedSquares[
          random_next(random) % FixedSquares.size()];
        State state;
        state.whiteKing = random_next(random) % Position::BoardSquares;
        state.blackKing = random_next(random) % Position::BoardSquares;
        if (state.whiteKing == fixedSquare || state.blackKing == fixedSquare ||
            state.whiteKing == state.blackKing) {
            --sample;
            continue;
        }
        state.minionCount = 1 + random_next(random) %
          UltimateDevilSlice::MaxMinions;
        state.secondary = Position::BoardSquares;
        state.cooldown = random_next(random) % 4;
        state.side = random_next(random) % 2;
        state.alive = true;
        Minions minions;
        minions.count = state.minionCount;
        unsigned filled = 0;
        while (filled < minions.count) {
            const unsigned square = random_next(random) % Position::BoardSquares;
            const int distance = static_cast<int>(square % Position::BoardFiles) -
              static_cast<int>(fixedSquare % Position::BoardFiles);
            if (std::abs(distance) > 2 || square == fixedSquare ||
                square == state.whiteKing || square == state.blackKing ||
                minions.contains(square))
                continue;
            minions.squares[filled++] = square;
        }
        std::sort(minions.squares.begin(),
                  minions.squares.begin() + minions.count);
        check_minion_codec(state, minions);
        check(state, fixedSquare, minions, checked, admitted);
    }
    std::cout << "DEVIL_SLICE_VERIFIED states=" << checked
              << " admitted=" << admitted << '\n';
    return 0;
} catch (const std::exception& error) {
    std::cerr << "Devil slice verifier error: " << error.what() << '\n';
    return 1;
}
