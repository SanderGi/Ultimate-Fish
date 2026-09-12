/*
  Ultimate Fish - Chess Ultimate rules engine
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "position.h"
#include "nnue.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cctype>
#include <cmath>
#include <sstream>

namespace Stockfish::Ultimate {

bool Position::apply_tablebase_angel_substate(
  int angel, int other, std::uint32_t substate) {
    if (angel < 0 || angel >= piece_count() || substate >= 3)
        return false;
    PieceState& item = pieces_[angel];
    if (!item.alive || item.type != PieceType::Angel || !item.onBoard ||
        item.link != NoPiece || item.host != NoPiece || item.attachmentOrder)
        return false;
    if (!substate)
        return true;
    int host = NoPiece;
    if (substate == 1) {
        for (int id = 0; id < piece_count(); ++id)
            if (pieces_[id].alive && pieces_[id].onBoard &&
                pieces_[id].type == PieceType::King &&
                pieces_[id].color == item.color) {
                host = id;
                break;
            }
    }
    else
        host = other;
    if (host < 0 || host >= piece_count() || host == angel ||
        !pieces_[host].alive || !pieces_[host].onBoard ||
        pieces_[host].type == PieceType::Halo ||
        pieces_[host].color != item.color)
        return false;
    const int haloSquare = item.square;
    erase_from_board(angel);
    item.onBoard = false;
    const int halo = add_piece(PieceType::Halo, item.color, haloSquare);
    if (halo == NoPiece)
        return false;
    item.link = static_cast<std::int8_t>(halo);
    item.host = static_cast<std::int8_t>(host);
    item.attachmentOrder = nextAttachmentOrder_++;
    item.square = pieces_[host].square;
    pieces_[halo].link = static_cast<std::int8_t>(angel);
    return true;
}

std::optional<std::uint32_t> Position::tablebase_angel_substate(
  int angel, int other) const {
    if (angel < 0 || angel >= piece_count())
        return std::nullopt;
    const PieceState& item = pieces_[angel];
    if (!item.alive || item.type != PieceType::Angel || item.action ||
        item.cooldown || item.power || !item.visible || item.parasiteTracked)
        return std::nullopt;
    if (item.onBoard)
        return item.link == NoPiece && item.host == NoPiece &&
               !item.attachmentOrder
          ? std::optional<std::uint32_t>(0) : std::nullopt;
    if (item.freezeCount || item.link < 0 || item.link >= piece_count() ||
        item.host < 0 || item.host >= piece_count() || !item.attachmentOrder)
        return std::nullopt;
    const PieceState& halo = pieces_[item.link];
    const PieceState& host = pieces_[item.host];
    if (!halo.alive || !halo.onBoard || halo.type != PieceType::Halo ||
        halo.color != item.color || halo.link != angel ||
        halo.host != NoPiece || !host.alive || !host.onBoard ||
        host.type == PieceType::Halo || host.color != item.color ||
        item.square != host.square)
        return std::nullopt;
    if (host.type == PieceType::King)
        return 1;
    if (other >= 0 && other < piece_count() && item.host == other &&
        pieces_[other].color == item.color)
        return 2;
    return std::nullopt;
}

bool Position::apply_tablebase_substate(int id, PieceType logicalType,
                                        std::uint32_t substate) {
    if (id < 0 || id >= piece_count() || !pieces_[id].alive ||
        (logicalType != PieceType::Angel && !pieces_[id].onBoard))
        return false;
    switch (logicalType) {
    case PieceType::Angel: {
        int other = NoPiece;
        for (int candidate = 0; candidate < piece_count(); ++candidate) {
            const PieceState& piece = pieces_[candidate];
            if (candidate == id || !piece.alive || !piece.onBoard ||
                piece.color != pieces_[id].color ||
                piece.type == PieceType::King || piece.type == PieceType::Halo)
                continue;
            if (other != NoPiece) {
                other = NoPiece;
                break;
            }
            other = candidate;
        }
        return apply_tablebase_angel_substate(id, other, substate);
    }
    case PieceType::Berserker:
        if (substate >= 10) return false;
        pieces_[id].power = static_cast<std::uint8_t>(substate);
        return true;
    case PieceType::Ghost:
        if (substate >= 2) return false;
        pieces_[id].visible = substate != 0;
        pieces_[id].parasiteTracked = false;
        return true;
    case PieceType::Devil:
        if (substate >= 4) return false;
        pieces_[id].cooldown = static_cast<std::uint8_t>(substate);
        return true;
    case PieceType::Sniper:
        if (substate >= 4) return false;
        pieces_[id].cooldown = static_cast<std::uint8_t>(substate);
        return true;
    case PieceType::Prince:
        if (substate >= 2 || (substate && continuation_ != Continuation::None))
            return false;
        if (substate) {
            continuation_ = Continuation::PrinceSecondMove;
            forcedPiece_ = id;
        }
        return true;
    case PieceType::Checker:
        if (substate >= 4 || (substate & 1u &&
                              continuation_ != Continuation::None))
            return false;
        if (substate & 1u) {
            continuation_ = Continuation::CheckerJump;
            forcedPiece_ = id;
        }
        return true;
    case PieceType::Pawn:
        if (substate >= 2) return false;
        pieces_[id].moved = substate != 0;
        return true;
    case PieceType::Penguin: {
        if (substate >= 8 || pieces_[id].type != PieceType::Penguin)
            return false;
        pieces_[id].action = 0;
        std::uint32_t found = 0;
        const int penguinFile = pieces_[id].square % BoardFiles;
        const int penguinRank = pieces_[id].square / BoardFiles;
        constexpr int directions[8][2] = {
          {1, 0}, {-1, 0}, {0, 1}, {0, -1},
          {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        const auto directionBit = [](int file, int rank) -> std::uint8_t {
            if (file == 0 && rank == 1) return 1;
            if (file == 0 && rank == -1) return 2;
            if (file == -1 && rank == 0) return 4;
            if (file == 1 && rank == 0) return 8;
            if (file == -1 && rank == 1) return 16;
            if (file == 1 && rank == 1) return 32;
            if (file == -1 && rank == -1) return 64;
            if (file == 1 && rank == -1) return 128;
            return 0;
        };
        for (const auto& direction : directions) {
            const int file = penguinFile + direction[0];
            const int rank = penguinRank + direction[1];
            if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
                continue;
            const int target = piece_on(rank * BoardFiles + file);
            if (target == NoPiece || pieces_[target].type == PieceType::Penguin)
                continue;
            const std::uint32_t flag = pieces_[target].type == PieceType::King
              ? (pieces_[target].color == Color::White ? 1u : 2u) : 4u;
            if (!(substate & flag))
                continue;
            found |= flag;
            pieces_[id].action |= directionBit(direction[0], direction[1]);
            ++pieces_[target].freezeCount;
        }
        return found == substate;
    }
    default:
        return substate == 0;
    }
}

std::optional<std::uint32_t> Position::tablebase_substate(
  int id, PieceType logicalType) const {
    if (id < 0 || id >= piece_count() || !pieces_[id].alive ||
        (logicalType != PieceType::Angel && !pieces_[id].onBoard))
        return std::nullopt;
    switch (logicalType) {
    case PieceType::Angel: {
        int other = NoPiece;
        for (int candidate = 0; candidate < piece_count(); ++candidate) {
            const PieceState& piece = pieces_[candidate];
            if (candidate == id || !piece.alive || !piece.onBoard ||
                piece.color != pieces_[id].color ||
                piece.type == PieceType::King || piece.type == PieceType::Halo)
                continue;
            if (other != NoPiece) {
                other = NoPiece;
                break;
            }
            other = candidate;
        }
        return tablebase_angel_substate(id, other);
    }
    case PieceType::Berserker:
        return std::min<std::uint32_t>(pieces_[id].power, 9);
    case PieceType::Ghost:
        return pieces_[id].parasiteTracked
             ? std::nullopt
             : std::optional<std::uint32_t>(pieces_[id].visible ? 1u : 0u);
    case PieceType::Devil:
        return pieces_[id].cooldown < 4
             ? std::optional<std::uint32_t>(pieces_[id].cooldown)
             : std::nullopt;
    case PieceType::Sniper:
        return pieces_[id].cooldown < 4
             ? std::optional<std::uint32_t>(pieces_[id].cooldown)
             : std::nullopt;
    case PieceType::Prince:
        return continuation_ == Continuation::PrinceSecondMove &&
               forcedPiece_ == id ? 1u : 0u;
    case PieceType::Checker:
        if (pieces_[id].type != PieceType::Checker &&
            pieces_[id].type != PieceType::CheckerKing)
            return std::nullopt;
        return (pieces_[id].type == PieceType::CheckerKing ? 2u : 0u) +
          (continuation_ == Continuation::CheckerJump && forcedPiece_ == id
             ? 1u : 0u);
    case PieceType::Pawn: return pieces_[id].moved ? 1u : 0u;
    case PieceType::Penguin: {
        if (pieces_[id].type != PieceType::Penguin)
            return std::nullopt;
        std::uint32_t result = 0;
        std::uint8_t expectedAction = 0;
        const int penguinFile = pieces_[id].square % BoardFiles;
        const int penguinRank = pieces_[id].square / BoardFiles;
        constexpr int directions[8][2] = {
          {1, 0}, {-1, 0}, {0, 1}, {0, -1},
          {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        const auto directionBit = [](int file, int rank) -> std::uint8_t {
            if (file == 0 && rank == 1) return 1;
            if (file == 0 && rank == -1) return 2;
            if (file == -1 && rank == 0) return 4;
            if (file == 1 && rank == 0) return 8;
            if (file == -1 && rank == 1) return 16;
            if (file == 1 && rank == 1) return 32;
            if (file == -1 && rank == -1) return 64;
            if (file == 1 && rank == -1) return 128;
            return 0;
        };
        for (const auto& direction : directions) {
            const std::uint8_t bit = directionBit(direction[0], direction[1]);
            if (!(pieces_[id].action & bit))
                continue;
            const int file = penguinFile + direction[0];
            const int rank = penguinRank + direction[1];
            if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
                return std::nullopt;
            const int target = piece_on(rank * BoardFiles + file);
            if (target == NoPiece || pieces_[target].type == PieceType::Penguin ||
                !pieces_[target].freezeCount)
                return std::nullopt;
            const std::uint32_t flag = pieces_[target].type == PieceType::King
              ? (pieces_[target].color == Color::White ? 1u : 2u) : 4u;
            result |= flag;
            expectedAction |= bit;
        }
        return expectedAction == pieces_[id].action
             ? std::optional<std::uint32_t>(result) : std::nullopt;
    }
    default: return 0u;
    }
}
namespace {

constexpr int Orthogonal[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr int Diagonal[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
constexpr int Around[8][2] = {
  {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
constexpr int KnightOffsets[8][2] = {
  {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};

constexpr int file_of(int square) { return square & 7; }
constexpr int rank_of(int square) { return square >> 3; }
constexpr int make_square(int file, int rank) { return rank * 8 + file; }
constexpr Bitboard square_bb(int square) { return Bitboard{1} << square; }

std::string_view display_letter(PieceType type, bool concealJester) {
    switch (type) {
    case PieceType::King: return "K";
    case PieceType::Jester: return concealJester ? "K" : "J";
    case PieceType::Knight: return "N";
    case PieceType::Pawn: return "";
    case PieceType::Queen: return "Q";
    case PieceType::Rook: return "R";
    case PieceType::Bishop: return "B";
    case PieceType::Berserker: return "BS";
    case PieceType::Bomb: return "BM";
    case PieceType::Ninja: return "NJ";
    case PieceType::Turtle: return "T";
    case PieceType::Ghost: return "GH";
    case PieceType::Mage: return "M";
    case PieceType::Goop: return "GO";
    case PieceType::Penguin: return "PN";
    case PieceType::Parasite: return "P";
    case PieceType::Devil: return "DV";
    case PieceType::Minion: return "MN";
    case PieceType::Sludge: return "S";
    case PieceType::Sniper: return "SN";
    case PieceType::Prince: return "PR";
    case PieceType::Checker: return "C";
    case PieceType::CheckerKing: return "CK";
    case PieceType::Giant: return "G";
    case PieceType::Copycat:
    case PieceType::CopycatClone: return "CC";
    case PieceType::Angel: return "A";
    case PieceType::Halo: return "H";
    case PieceType::Fisherman: return "F";
    case PieceType::Dragon: return "D";
    case PieceType::Count: return "";
    }
    return "";
}

bool same_display_role(PieceType lhs, PieceType rhs, bool concealJester) {
    if ((lhs == PieceType::Copycat || lhs == PieceType::CopycatClone) &&
        (rhs == PieceType::Copycat || rhs == PieceType::CopycatClone))
        return true;
    if (concealJester &&
        (lhs == PieceType::King || lhs == PieceType::Jester) &&
        (rhs == PieceType::King || rhs == PieceType::Jester))
        return true;
    return lhs == rhs;
}

struct ThreatGeometry {
    std::array<Bitboard, Position::BoardSquares> king{};
    std::array<Bitboard, Position::BoardSquares> knight{};
    std::array<std::array<Bitboard, Position::BoardSquares>, 2> pawn{};
    std::array<Bitboard, Position::BoardSquares> rook{};
    std::array<Bitboard, Position::BoardSquares> bishop{};
    std::array<Bitboard, Position::BoardSquares> queen{};
    std::array<Bitboard, Position::BoardSquares> ninja{};
    std::array<Bitboard, Position::BoardSquares> turtle{};
    std::array<std::array<Bitboard, Position::BoardSquares>, 2> sniper{};
    std::array<Bitboard, Position::BoardSquares> dragon{};
};

const ThreatGeometry& threat_geometry() {
    static const ThreatGeometry geometry = [] {
        ThreatGeometry result;
        const auto addRays = [](Bitboard& mask, int square,
                                const int (*directions)[2], int count, int distance) {
            for (int direction = 0; direction < count; ++direction)
                for (int step = 1; step <= distance; ++step) {
                    const int file = file_of(square) + directions[direction][0] * step;
                    const int rank = rank_of(square) + directions[direction][1] * step;
                    if (file < 0 || file >= Position::BoardFiles ||
                        rank < 0 || rank >= Position::BoardRanks)
                        break;
                    mask |= square_bb(make_square(file, rank));
                }
        };
        for (int square = 0; square < Position::BoardSquares; ++square) {
            addRays(result.king[square], square, Around, 8, 1);
            addRays(result.rook[square], square, Orthogonal, 4, Position::BoardRanks);
            addRays(result.bishop[square], square, Diagonal, 4, Position::BoardRanks);
            result.queen[square] = result.rook[square] | result.bishop[square];
            addRays(result.ninja[square], square, Around, 8, 3);
            addRays(result.turtle[square], square, Orthogonal, 4, 1);
            for (const auto& offset : KnightOffsets) {
                const int file = file_of(square) + offset[0];
                const int rank = rank_of(square) + offset[1];
                if (file >= 0 && file < Position::BoardFiles &&
                    rank >= 0 && rank < Position::BoardRanks)
                    result.knight[square] |= square_bb(make_square(file, rank));
            }
            result.dragon[square] = result.bishop[square] | result.knight[square];
            for (Color color : {Color::White, Color::Black}) {
                const int colorIndex = static_cast<int>(color);
                const int direction = color == Color::White ? 1 : -1;
                const int targetRank = rank_of(square) + direction;
                if (targetRank >= 0 && targetRank < Position::BoardRanks)
                    for (const int horizontal : {-1, 1}) {
                        const int targetFile = file_of(square) + horizontal;
                        if (targetFile >= 0 && targetFile < Position::BoardFiles)
                            result.pawn[colorIndex][square] |=
                              square_bb(make_square(targetFile, targetRank));
                    }
                for (int rank = rank_of(square) + direction;
                     rank >= 0 && rank < Position::BoardRanks; rank += direction)
                    result.sniper[colorIndex][square] |=
                      square_bb(make_square(file_of(square), rank));
            }
        }
        return result;
    }();
    return geometry;
}

// SimulatedFreeze::Direction. These bits are serialized in Model_Piece.action
// and identify the characters frozen by the Penguin's most recent move.
constexpr std::uint8_t penguin_direction_bit(int deltaFile, int deltaRank) {
    if (deltaFile == 0 && deltaRank == 1) return 1;    // Up
    if (deltaFile == 0 && deltaRank == -1) return 2;   // Down
    if (deltaFile == -1 && deltaRank == 0) return 4;   // Left
    if (deltaFile == 1 && deltaRank == 0) return 8;    // Right
    if (deltaFile == -1 && deltaRank == 1) return 16;  // UpLeft
    if (deltaFile == 1 && deltaRank == 1) return 32;   // UpRight
    if (deltaFile == -1 && deltaRank == -1) return 64; // DownLeft
    if (deltaFile == 1 && deltaRank == -1) return 128; // DownRight
    return 0;
}

constexpr std::array<PieceInfo, static_cast<std::size_t>(PieceType::Count)> PieceTable = {{
  // Draft costs are filled from Character.value in the shipping Android
  // binary. Generated pieces deliberately cost zero.
  {"king", PieceClass::Melee, 0, 0, false, false},
  {"jester", PieceClass::Melee, 0, 10, true, false},
  {"knight", PieceClass::Melee, 0, 6, true, false},
  {"pawn", PieceClass::Melee, 0, 3, true, false},
  {"queen", PieceClass::Melee, 0, 17, true, false},
  {"rook", PieceClass::Melee, 0, 13, true, false},
  {"bishop", PieceClass::Melee, 0, 9, true, false},
  {"berserker", PieceClass::Melee, 0, 15, true, false},
  {"bomb", PieceClass::Melee, 0, 15, true, false},
  {"ninja", PieceClass::Melee, 0, 20, true, false},
  {"turtle", PieceClass::Melee, 0, 4, true, false},
  {"ghost", PieceClass::Melee, 0, 15, true, false},
  {"mage", PieceClass::Support, 0, 8, true, false},
  {"goop", PieceClass::Melee, 0, 0, false, true},
  {"penguin", PieceClass::Support, 5, 15, true, false},
  {"parasite", PieceClass::Melee, 0, 15, true, false},
  {"devil", PieceClass::Support, 3, 15, true, false},
  {"minion", PieceClass::Melee, 0, 0, false, true},
  {"sludge", PieceClass::Support, 0, 12, true, false},
  {"sniper", PieceClass::Ranged, 3, 17, true, false},
  {"prince", PieceClass::Melee, 0, 18, true, false},
  {"checker", PieceClass::Ranged, 0, 2, true, false},
  {"checkerKing", PieceClass::Ranged, 0, 2, false, true},
  {"giant", PieceClass::Ranged, 0, 1, true, false},
  {"copycat", PieceClass::Melee, 0, 5, true, false},
  {"copycatClone", PieceClass::Melee, 0, 0, false, true},
  {"angel", PieceClass::Support, 0, 13, true, false},
  {"halo", PieceClass::Support, 0, 0, false, true},
  {"fisherman", PieceClass::Support, 0, 12, true, false},
  {"dragon", PieceClass::Melee, 0, 15, true, false},
}};

constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> MaterialValue = {{
  // SimulatedPiece initializes pointValue from Character.value[type]. Scale
  // those designer-balanced native values into centipawn-like engine units.
  // Generated board effects get only a small standalone value; CopyCatClone
  // is already included in the deployable CopyCat pair's value.
  20000, 350, 210, 105, 595, 455, 315, 525, 525, 700,
  140, 525, 280, 25, 525, 525, 525, 20, 420, 595,
  630, 70, 90, 35, 175, 0, 455, 0, 420, 525,
}};

constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> TropismWeight = {{
  1, 1, 2, 1, 4, 3, 3, 3, 3, 3,
  1, 2, 0, 0, 1, 2, 0, 0, 0, 5,
  2, 2, 2, 1, 2, 0, 0, 0, 2, 4,
}};

constexpr bool valid_square(int square) {
    return square >= 0 && square < Position::BoardSquares;
}

int pop_lsb(Bitboard& mask) {
    const std::uint64_t low = static_cast<std::uint64_t>(mask);
    const int square = low ? __builtin_ctzll(low)
                           : 64 + __builtin_ctzll(static_cast<std::uint64_t>(mask >> 64));
    mask &= mask - 1;
    return square;
}

int popcount(Bitboard mask) {
    return __builtin_popcountll(static_cast<std::uint64_t>(mask)) +
           __builtin_popcountll(static_cast<std::uint64_t>(mask >> 64));
}

int lsb_square(Bitboard mask) {
    const std::uint64_t low = static_cast<std::uint64_t>(mask);
    return low ? __builtin_ctzll(low)
               : 64 + __builtin_ctzll(static_cast<std::uint64_t>(mask >> 64));
}

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    while (true) {
        const auto at = text.find(separator);
        parts.push_back(text.substr(0, at));
        if (at == std::string_view::npos)
            return parts;
        text.remove_prefix(at + 1);
    }
}

bool parse_int(std::string_view text, int& value) {
    if (text.empty())
        return false;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

}  // namespace

Position::Position() { clear(); }

void Position::clear() {
    board_.fill(NoPiece);
    pieces_.fill({});
    for (auto& color : byType_)
        color.fill(0);
    occupancy_.fill(0);
    pieceCount_ = 0;
    sideToMove_ = Color::White;
    enPassantSquare_ = NoSquare;
    enPassantVictim_ = NoPiece;
    forcedPiece_ = NoPiece;
    continuation_ = Continuation::None;
    forcedTimeoutWinner_ = -1;
    halfmove_ = 0;
    fullmove_ = 1;
    nextAttachmentOrder_ = 1;
}

int Position::add_piece(PieceType type, Color color, int square) {
    return add_piece_internal(type, color, square, true);
}

int Position::add_piece_internal(PieceType type, Color color, int square, bool generateCompanions,
                                 bool onBoard) {
    if (!valid_square(square) || (onBoard && board_[square] != NoPiece))
        return NoPiece;

    int availableSlots = MaxPieces - pieceCount_;
    for (int candidate = 0; candidate < pieceCount_; ++candidate)
        if (!pieces_[candidate].alive)
            ++availableSlots;
    if (availableSlots == 0)
        return NoPiece;

    // The native CopyCat constructor always creates its mirror clone at
    // (7-file, rank). Treat the pair as one deployable character.
    const int mirror = type == PieceType::Copycat
                     ? make_square(7 - file_of(square), rank_of(square)) : NoSquare;
    if (generateCompanions && type == PieceType::Copycat &&
        (mirror == square || board_[mirror] != NoPiece || availableSlots < 2))
        return NoPiece;

    int id = NoPiece;
    for (int candidate = 0; candidate < pieceCount_; ++candidate)
        if (!pieces_[candidate].alive) {
            id = candidate;
            break;
        }
    const bool appended = id == NoPiece;
    if (appended)
        id = pieceCount_++;
    PieceState state;
    state.type = type;
    state.color = color;
    state.square = static_cast<std::uint8_t>(square);
    state.onBoard = onBoard;
    state.alive = true;
    pieces_[id] = state;
    if (onBoard && !footprint_fits(id, square, false)) {
        pieces_[id] = {};
        if (appended)
            --pieceCount_;
        return NoPiece;
    }
    if (onBoard)
        place_on_board(id);

    if (generateCompanions && type == PieceType::Copycat) {
        const int clone = add_piece_internal(PieceType::CopycatClone, color, mirror, false);
        if (clone == NoPiece) {
            erase_from_board(id);
            pieces_[id] = {};
            if (appended)
                --pieceCount_;
            return NoPiece;
        }
        pieces_[id].link = static_cast<std::int8_t>(clone);
        pieces_[clone].link = static_cast<std::int8_t>(id);
    }
    return id;
}

int Position::attached_angel(int host) const {
    int found = NoPiece;
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& piece = pieces_[id];
        if (!piece.alive || piece.type != PieceType::Angel || piece.host != host)
            continue;
        if (found == NoPiece || piece.attachmentOrder < pieces_[found].attachmentOrder)
            found = id;
    }
    return found;
}

void Position::relocate_giant(int id, int destination, bool markMoved, bool eraseOrigin) {
    if (id < 0 || id >= pieceCount_ || !pieces_[id].alive ||
        pieces_[id].type != PieceType::Giant || !footprint(id, destination))
        return;

    if (eraseOrigin)
        erase_from_board(id);
    // SimulatedGiant::MakeMoveTurnSkip invokes SimulateDeath on every
    // character in the translated footprint, regardless of team.  Repeat the
    // scan because an Angel save can relocate its host onto another one of
    // those four cells; the native square-by-square routine then encounters
    // that relocated host as well.  Each repeated victim consumes an Angel or
    // dies, so MaxPieces is a conservative hard bound against malformed state.
    for (int pass = 0; pass < MaxPieces && pieces_[id].alive; ++pass) {
        const auto displaced = victims_on(footprint(id, destination), id);
        if (displaced.empty())
            break;
        for (const int occupant : displaced)
            if (pieces_[id].alive && pieces_[occupant].alive)
                capture_piece(occupant, id,
                              {pieces_[id].square, pieces_[occupant].square});
    }
    if (!pieces_[id].alive)
        return;
    // A legal native relocation cannot leave an occupied Giant footprint.
    // Failing closed here prevents a malformed externally supplied attachment
    // cycle from corrupting board/bitboard ownership.
    if (!victims_on(footprint(id, destination), id).empty()) {
        remove_piece(id);
        return;
    }
    pieces_[id].square = static_cast<std::uint8_t>(destination);
    pieces_[id].moved = pieces_[id].moved || markMoved;
    place_on_board(id);
}

void Position::sacrifice_angel(int angel, int host) {
    if (angel < 0 || angel >= pieceCount_ || host < 0 || host >= pieceCount_ ||
        !pieces_[angel].alive || !pieces_[host].alive)
        return;
    const int halo = pieces_[angel].link;
    const int returnSquare = halo != NoPiece && halo < pieceCount_ && pieces_[halo].alive
                           ? pieces_[halo].square : NoSquare;

    // SimulatedPiece::AngelFunction calls the oldest attached Angel's death
    // routine. That routine removes the Angel and Halo, then relocates the
    // protected character to the Halo coordinate.
    remove_piece(angel);
    if (pieces_[angel].alive || !pieces_[host].alive || returnSquare == NoSquare)
        return;

    // Native Angel rescue is a forced relocation. It removes the host from
    // every Penguin's currentFrozenPieces set; a rescued Penguin also drops
    // the aura it created at its old square.
    prepare_for_forced_relocation(host);
    erase_from_board(host);
    int destination = returnSquare;
    if (pieces_[host].type == PieceType::Giant) {
        // A Halo is the near home-side corner of a rescued Giant. Ivory's
        // footprint extends up/right from it, whereas Onyx's extends
        // down/left. SimulatedAngel::SimulateDeath explicitly subtracts one
        // from both Onyx coordinates before clamping and dispatching
        // SimulatedGiant::MakeMoveTurnSkip. Internally every Giant uses the
        // canonical lower-left anchor, so retain that team conversion here.
        int destinationFile = file_of(returnSquare);
        int destinationRank = rank_of(returnSquare);
        if (pieces_[host].color == Color::Black) {
            --destinationFile;
            --destinationRank;
        }
        destination = make_square(std::clamp(destinationFile, 0, BoardFiles - 2),
                                  std::clamp(destinationRank, 0, BoardRanks - 2));
        relocate_giant(host, destination);
        return;
    }
    pieces_[host].square = static_cast<std::uint8_t>(destination);
    apply_forced_promotion(host);
    place_on_board(host);
}

bool Position::remove_piece(int id) { return remove_piece_internal(id, true); }

bool Position::remove_piece_internal(int id, bool allowAngel) {
    if (id < 0 || id >= pieceCount_ || !pieces_[id].alive)
        return false;

    if (allowAngel) {
        const int protector = attached_angel(id);
        if (protector != NoPiece) {
            sacrifice_angel(protector, id);
            return true;
        }
    }

    const PieceType type = pieces_[id].type;
    const int linked = pieces_[id].link;
    if (type == PieceType::Penguin)
        clear_penguin_freeze(id);
    detach_from_penguin_freezes(id);
    erase_from_board(id);
    pieces_[id].alive = false;
    if (forcedPiece_ == id) {
        forcedPiece_ = NoPiece;
        continuation_ = Continuation::None;
    }
    if ((type == PieceType::Copycat || type == PieceType::CopycatClone) && linked != NoPiece &&
        linked < pieceCount_ && pieces_[linked].alive)
        // Native CopyCat death marks the struck half dying before dispatching
        // SimulateDeath to its partner.  The partner therefore bypasses its
        // own AngelFunction: an Angel on the struck half can save the whole
        // pair, but an Angel attached only to the other half cannot leave a
        // singleton CopyCat behind.
        remove_piece_internal(linked, false);
    if ((type == PieceType::Angel || type == PieceType::Halo) && linked != NoPiece &&
        linked < pieceCount_ && pieces_[linked].alive)
        remove_piece(linked);
    return true;
}

void Position::transfer_attached_angels(int fromHost, int toHost) {
    if (fromHost < 0 || fromHost >= pieceCount_ || toHost < 0 ||
        toHost >= pieceCount_ || fromHost == toHost)
        return;
    // Parasite.TakeOver moves every Angel attached to the Parasite onto the
    // possessed character.  It does not invoke AngelFunction, so no Angel or
    // Halo is consumed during possession.  Existing attachmentOrder values
    // retain the native UniqueAngelList order when the two lists are merged.
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].type == PieceType::Angel &&
            pieces_[id].host == fromHost)
            pieces_[id].host = static_cast<std::int8_t>(toHost);
}

int Position::piece_on(int square) const {
    return valid_square(square) ? board_[square] : NoPiece;
}

const PieceInfo& Position::info(PieceType type) { return PieceTable[index(type)]; }

int Position::material_value(PieceType type) { return MaterialValue[index(type)]; }

bool Position::swap_royal_roles(int first, int second) {
    if (first < 0 || first >= pieceCount_ || second < 0 || second >= pieceCount_)
        return false;
    PieceState& a = pieces_[first];
    PieceState& b = pieces_[second];
    if (!a.alive || !b.alive || a.color != b.color ||
        !((a.type == PieceType::King && b.type == PieceType::Jester) ||
          (a.type == PieceType::Jester && b.type == PieceType::King)))
        return false;
    const Color color = a.color;
    if (a.onBoard) {
        byType_[index(color)][index(a.type)] &= ~square_bb(a.square);
        byType_[index(color)][index(b.type)] |= square_bb(a.square);
    }
    if (b.onBoard) {
        byType_[index(color)][index(b.type)] &= ~square_bb(b.square);
        byType_[index(color)][index(a.type)] |= square_bb(b.square);
    }
    std::swap(a.type, b.type);
    return true;
}

int Position::material_points(int id) const {
    if (id < 0 || id >= pieceCount_ || !pieces_[id].alive)
        return 0;
    const PieceState& item = pieces_[id];
    const int base = info(item.type).draftCost;
    // Native SimulatedBerserker starts at power level one, multiplies its
    // pointValue by that level, and adds another base 15 after every attack.
    // UPN stores the number of gained levels in PieceState::power.
    return item.type == PieceType::Berserker ? base * (int(item.power) + 1) : base;
}

int Position::material_points(Color color) const {
    int total = 0;
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].color == color)
            total += material_points(id);
    return total;
}

std::string_view Position::type_name(PieceType type) { return info(type).name; }

std::optional<PieceType> Position::type_from_name(std::string_view name) {
    for (std::size_t i = 0; i < PieceTable.size(); ++i)
        if (PieceTable[i].name == name)
            return static_cast<PieceType>(i);
    return std::nullopt;
}

int Position::square_from_name(std::string_view name) {
    if (name.size() < 2 || name.size() > 3 || name[0] < 'a' || name[0] > 'h')
        return NoSquare;
    int rank = 0;
    if (!parse_int(name.substr(1), rank) || rank < 1 || rank > BoardRanks)
        return NoSquare;
    return make_square(name[0] - 'a', rank - 1);
}

std::string Position::square_name(int square) {
    if (!valid_square(square))
        return "-";
    return std::string(1, static_cast<char>('a' + file_of(square)))
         + std::to_string(rank_of(square) + 1);
}

bool Position::is_melee(PieceType type) const { return info(type).pieceClass == PieceClass::Melee; }

Bitboard Position::footprint(int id, int anchor) const {
    if (!valid_square(anchor))
        return 0;
    if (pieces_[id].type != PieceType::Giant)
        return square_bb(anchor);
    const int file = file_of(anchor);
    const int rank = rank_of(anchor);
    if (file == BoardFiles - 1 || rank == BoardRanks - 1)
        return 0;
    return square_bb(anchor) | square_bb(anchor + 1) | square_bb(anchor + 8) | square_bb(anchor + 9);
}

bool Position::footprint_fits(int id, int anchor, bool allowOwnCurrent) const {
    const Bitboard target = footprint(id, anchor);
    if (!target)
        return false;
    Bitboard blockers = occupied();
    if (allowOwnCurrent && pieces_[id].alive)
        blockers &= ~footprint(id, pieces_[id].square);
    return !(target & blockers);
}

void Position::place_on_board(int id) {
    const PieceState& piece = pieces_[id];
    if (!piece.onBoard)
        return;
    Bitboard mask = footprint(id, piece.square);
    while (mask) {
        const int square = pop_lsb(mask);
        board_[square] = static_cast<std::int8_t>(id);
    }
    const Bitboard occupiedSquares = footprint(id, piece.square);
    byType_[index(piece.color)][index(piece.type)] |= occupiedSquares;
    occupancy_[index(piece.color)] |= occupiedSquares;
}

void Position::erase_from_board(int id) {
    const PieceState& piece = pieces_[id];
    if (!piece.onBoard)
        return;
    Bitboard mask = footprint(id, piece.square);
    while (mask) {
        const int square = pop_lsb(mask);
        if (board_[square] == id)
            board_[square] = NoPiece;
    }
    const Bitboard occupiedSquares = footprint(id, piece.square);
    byType_[index(piece.color)][index(piece.type)] &= ~occupiedSquares;
    occupancy_[index(piece.color)] &= ~occupiedSquares;
}

void Position::rebuild_bitboards() {
    board_.fill(NoPiece);
    for (auto& color : byType_)
        color.fill(0);
    occupancy_.fill(0);
    // Attached Angels share their host's coordinate in the native model even
    // though they do not occupy a square. Keep that serialized state exact
    // after every host move, pull, swap, or halo rescue.
    for (int id = 0; id < pieceCount_; ++id) {
        PieceState& piece = pieces_[id];
        if (piece.alive && piece.type == PieceType::Angel && !piece.onBoard &&
            piece.host != NoPiece && piece.host < pieceCount_ && pieces_[piece.host].alive)
            piece.square = pieces_[piece.host].square;
    }
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].onBoard)
            place_on_board(id);
}

bool Position::can_land(int id, int square, bool attacksOnly) const {
    if (!valid_square(square))
        return false;
    const int target = board_[square];
    if (target == NoPiece)
        return !attacksOnly;
    // An enemy Ghost that is invisible to the mover is rendered as empty in
    // the app, but the apparent quiet move onto that cell is still available.
    // MakeMove then reveals that the move captured the Ghost.  A team's own
    // invisible Ghost is known private occupancy and remains unavailable.
    return pieces_[target].color != pieces_[id].color;
}

void Position::add_step_moves(std::vector<Move>& moves, int id, const int (*directions)[2],
                              int count, int distance, bool jump, bool attacksOnly) const {
    add_slider_moves(moves, id, directions, count, distance, jump, attacksOnly);
}

void Position::add_slider_moves(std::vector<Move>& moves, int id, const int (*directions)[2],
                                int count, int distance, bool jump, bool attacksOnly) const {
    const PieceState& piece = pieces_[id];
    const int fromFile = file_of(piece.square);
    const int fromRank = rank_of(piece.square);
    for (int direction = 0; direction < count; ++direction) {
        for (int step = 1; step <= distance; ++step) {
            const int file = fromFile + directions[direction][0] * step;
            const int rank = fromRank + directions[direction][1] * step;
            if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
                break;
            const int to = make_square(file, rank);
            const int target = board_[to];
            if (target == NoPiece) {
                if (!attacksOnly)
                    moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
                continue;
            }
            // Invisible Ghosts are transparent to rays. An enemy Ghost's
            // cell is also an available apparent quiet endpoint: entering it
            // captures the Ghost. Allied hidden occupancy is known to the
            // mover, so that cell is unavailable even though the ray carries
            // on beyond it.
            if (pieces_[target].type == PieceType::Ghost && !pieces_[target].visible) {
                if (pieces_[target].color != piece.color)
                    moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
                continue;
            }
            if (pieces_[target].color != piece.color)
                moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
            if (!jump)
                break;
        }
    }
}

void Position::add_knight_moves(std::vector<Move>& moves, int id, bool attacksOnly) const {
    const PieceState& piece = pieces_[id];
    const int fromFile = file_of(piece.square);
    const int fromRank = rank_of(piece.square);
    for (const auto& offset : KnightOffsets) {
        const int file = fromFile + offset[0];
        const int rank = fromRank + offset[1];
        if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
            continue;
        const int to = make_square(file, rank);
        if (can_land(id, to, attacksOnly))
            moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
    }
}

void Position::add_checker_moves(std::vector<Move>& moves, int id, bool attacksOnly) const {
    const PieceState& piece = pieces_[id];
    const int direction = piece.color == Color::White ? 1 : -1;
    const bool king = piece.type == PieceType::CheckerKing;
    // Native SimulatedChecker uses movementAmount=8 after promotion. A king
    // scans to the first visible blocker, then jumps exactly one cell beyond
    // an enemy; it does not choose arbitrary landing cells beyond the victim.
    const int range = king ? 8 : 1;
    bool foundCapture = false;
    const std::size_t firstMove = moves.size();
    for (const int vertical : {direction, -direction}) {
        if (vertical == -direction && !king)
            continue;
        for (const int horizontal : {-1, 1}) {
          for (int distance = 1; distance <= range; ++distance) {
            const int middleFile = file_of(piece.square) + distance * horizontal;
            const int middleRank = rank_of(piece.square) + distance * vertical;
            const int targetFile = middleFile + horizontal;
            const int targetRank = middleRank + vertical;
            if (middleFile < 0 || middleFile >= BoardFiles ||
                middleRank < 0 || middleRank >= BoardRanks)
                break;
            const int jumped = board_[make_square(middleFile, middleRank)];
            if (jumped == NoPiece) {
                if (!attacksOnly && !foundCapture)
                    moves.push_back({piece.square,
                      static_cast<std::uint8_t>(make_square(middleFile, middleRank))});
                continue;
            }
            if (jumped != NoPiece && pieces_[jumped].color != piece.color &&
                pieces_[jumped].type == PieceType::Ghost &&
                !pieces_[jumped].visible) {
                // A hidden enemy Ghost is not exposed as a public jump. The
                // native Checker offers the cell as a blind collision; a
                // king's apparent quiet ray also continues beyond it.
                if (!attacksOnly && !foundCapture)
                    moves.push_back({
                      piece.square,
                      static_cast<std::uint8_t>(make_square(middleFile, middleRank))});
                continue;
            }
            if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
                targetRank >= BoardRanks)
                break;
            const int target = make_square(targetFile, targetRank);
            const int landing = board_[target];
            const bool emptyOrHiddenEnemy =
              landing == NoPiece ||
              (pieces_[landing].color != piece.color &&
               pieces_[landing].type == PieceType::Ghost &&
               !pieces_[landing].visible);
            if (jumped != NoPiece && pieces_[jumped].color != piece.color &&
                pieces_[jumped].visible && emptyOrHiddenEnemy) {
                if (!foundCapture)
                    moves.resize(firstMove);
                foundCapture = true;
                moves.push_back({piece.square, static_cast<std::uint8_t>(target),
                                 static_cast<std::uint8_t>(make_square(middleFile, middleRank))});
            }
            break;
          }
        }
    }
}

void Position::add_giant_moves(std::vector<Move>& moves, int id, bool attacksOnly) const {
    const PieceState& piece = pieces_[id];
    const Bitboard own = footprint(id, piece.square);
    for (const auto& direction : Orthogonal) {
        // SimulatedGiant::GetPieceSimulations tests the four squares exactly
        // two files/ranks away. The character is a 2x2 footprint and moves by
        // one full footprint, not one-to-four single squares.
        const int file = file_of(piece.square) + direction[0] * 2;
        const int rank = rank_of(piece.square) + direction[1] * 2;
        if (file < 0 || file > BoardFiles - 2 || rank < 0 || rank > BoardRanks - 2)
            continue;
        const int anchor = make_square(file, rank);
        const Bitboard target = footprint(id, anchor);
        const Bitboard foreign = target & (occupied() & ~own);
        if (!(foreign & occupied(piece.color)) && (!attacksOnly || foreign))
            moves.push_back({piece.square, static_cast<std::uint8_t>(anchor)});
    }
}

void Position::add_copycat_moves(std::vector<Move>& moves, int id, bool attacksOnly) const {
    const PieceState& actor = pieces_[id];
    const int partner = actor.link;
    const bool paired = partner != NoPiece && partner < pieceCount_ && pieces_[partner].alive &&
                        pieces_[partner].onBoard && !frozen(partner) && !pieces_[partner].cooldown &&
                        (pieces_[partner].type == PieceType::Copycat ||
                         pieces_[partner].type == PieceType::CopycatClone);
    for (const auto& direction : Around) {
        const int toFile = file_of(actor.square) + direction[0];
        const int toRank = rank_of(actor.square) + direction[1];
        if (toFile < 0 || toFile >= BoardFiles || toRank < 0 || toRank >= BoardRanks)
            continue;
        const int to = make_square(toFile, toRank);
        if (!can_land(id, to, attacksOnly))
            continue;

        int partnerTo = 0xff;
        if (paired) {
            const PieceState& other = pieces_[partner];
            const int otherFile = file_of(other.square) - direction[0];
            const int otherRank = rank_of(other.square) + direction[1];
            if (otherFile < 0 || otherFile >= BoardFiles || otherRank < 0 ||
                otherRank >= BoardRanks)
                continue;
            partnerTo = make_square(otherFile, otherRank);
            if (partnerTo == to || !can_land(partner, partnerTo, attacksOnly))
                continue;
        }
        moves.push_back({actor.square, static_cast<std::uint8_t>(to),
                         static_cast<std::uint8_t>(partnerTo)});
    }
}

void Position::add_fisherman_moves(std::vector<Move>& moves, int id, bool attacksOnly) const {
    if (attacksOnly)
        return;
    const PieceState& fisherman = pieces_[id];
    for (const auto& direction : Around) {
        for (int distance = 1; distance < BoardRanks; ++distance) {
            const int file = file_of(fisherman.square) + direction[0] * distance;
            const int rank = rank_of(fisherman.square) + direction[1] * distance;
            if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
                break;
            const int square = make_square(file, rank);
            const int target = board_[square];
            if (target == NoPiece) {
                moves.push_back({fisherman.square, static_cast<std::uint8_t>(square)});
                continue;
            }

            // Native Fisherman ray generation hooks the first visible piece
            // (friend or foe) only when it is at least two squares away. The
            // target is pulled to the first square on the ray while the
            // Fisherman stays put. An invisible enemy Ghost is also a blind
            // movement endpoint; MakeMove knocks out both characters.
            if (!pieces_[target].visible) {
                if (pieces_[target].color != fisherman.color)
                    moves.push_back({fisherman.square, static_cast<std::uint8_t>(square)});
                // Unlike a visible target, a hidden Ghost does not terminate
                // the ray. Native generation continues to empty cells and to
                // the first later visible hook target.
                continue;
            }
            else if (distance > 1) {
                const int landing = make_square(file_of(fisherman.square) + direction[0],
                                                rank_of(fisherman.square) + direction[1]);
                moves.push_back({fisherman.square, static_cast<std::uint8_t>(square),
                                 static_cast<std::uint8_t>(landing), MoveKind::Pull});
            }
            break;
        }
    }
}

void Position::append_moves_for(std::vector<Move>& moves, int id,
                                bool attacksOnly) const {
    if (id < 0 || id >= pieceCount_ || !pieces_[id].alive || frozen(id) || pieces_[id].cooldown)
        return;

    const std::size_t first = moves.size();
    const PieceState& piece = pieces_[id];
    switch (piece.type) {
    case PieceType::King:
    case PieceType::Jester: {
        add_step_moves(moves, id, Around, 8, 1, false, attacksOnly);
        // The shipping game constructs both King and Jester as a
        // SimulatedKing. Its castle scan is deliberately not orthodox: it
        // walks to the first occupied square in either horizontal direction,
        // requires only an unmoved Rook at least three files away, and never
        // checks the Rook's team, cooldown, or freeze state. An enemy-owned
        // Rook is the native forced-timeout edge case described in
        // make_move_unchecked. Check legality is evaluated only after the
        // complete move, so starting and transit squares may be attacked.
        if (!attacksOnly && !piece.moved) {
            const int fromFile = file_of(piece.square);
            const int rank = rank_of(piece.square);
            for (const int direction : {-1, 1}) {
                for (int file = fromFile + direction;
                     file >= 0 && file < BoardFiles; file += direction) {
                    const int rook = board_[make_square(file, rank)];
                    if (rook == NoPiece)
                        continue;
                    if (std::abs(file - fromFile) > 2 &&
                        pieces_[rook].type == PieceType::Rook && !pieces_[rook].moved) {
                        const int target = make_square(fromFile + 2 * direction, rank);
                        moves.push_back({piece.square, static_cast<std::uint8_t>(target),
                                         static_cast<std::uint8_t>(rook), MoveKind::Castle});
                    }
                    break;
                }
            }
        }
        break;
    }
    case PieceType::Ghost:
    case PieceType::Parasite:
        add_step_moves(moves, id, Around, 8, 1, false, attacksOnly);
        break;
    case PieceType::Prince:
        for (const auto& direction : Around) {
            const int file = file_of(piece.square) + direction[0];
            const int rank = rank_of(piece.square) + direction[1];
            if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
                continue;
            const int to = make_square(file, rank);
            // A hidden enemy Ghost is an ordinary first-step capture, which
            // ends the action without a second move.
            if (can_land(id, to, attacksOnly))
                moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
        }
        break;
    case PieceType::Knight:
        add_knight_moves(moves, id, attacksOnly);
        break;
    case PieceType::Pawn: {
        const int direction = piece.color == Color::White ? 1 : -1;
        const int rank = rank_of(piece.square);
        const int file = file_of(piece.square);
        const int one = make_square(file, rank + direction);
        if (!attacksOnly && valid_square(one)) {
            const int forwardTarget = board_[one];
            const bool hiddenForward =
              forwardTarget != NoPiece &&
              pieces_[forwardTarget].type == PieceType::Ghost &&
              !pieces_[forwardTarget].visible;
            if (forwardTarget == NoPiece || hiddenForward) {
                if (forwardTarget == NoPiece ||
                    pieces_[forwardTarget].color != piece.color)
                    moves.push_back({piece.square, static_cast<std::uint8_t>(one)});

                // Native unmoved-Pawn generation scans both forward squares.
                // A hidden Ghost at the intermediate square is unavailable as
                // a friendly landing but does not block the double step. The
                // privilege is tied to the Pawn's moved flag, not its rank.
                const int two = make_square(file, rank + 2 * direction);
                if (!piece.moved && valid_square(two)) {
                    const int doubleTarget = board_[two];
                    const bool hiddenEnemyAtDouble =
                      doubleTarget != NoPiece &&
                      pieces_[doubleTarget].color != piece.color &&
                      pieces_[doubleTarget].type == PieceType::Ghost &&
                      !pieces_[doubleTarget].visible;
                    if (doubleTarget == NoPiece || hiddenEnemyAtDouble)
                        moves.push_back({piece.square,
                                         static_cast<std::uint8_t>(two)});
                }
            }
        }
        for (const int horizontal : {-1, 1}) {
            const int targetFile = file + horizontal;
            const int targetRank = rank + direction;
            if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
                targetRank >= BoardRanks)
                continue;
            const int target = make_square(targetFile, targetRank);
            if ((board_[target] != NoPiece && pieces_[board_[target]].color != piece.color &&
                 pieces_[board_[target]].visible) ||
                target == enPassantSquare_)
                moves.push_back({piece.square, static_cast<std::uint8_t>(target)});
        }
        break;
    }
    case PieceType::Queen:
        add_slider_moves(moves, id, Around, 8, BoardRanks, false, attacksOnly);
        break;
    case PieceType::Rook:
        add_slider_moves(moves, id, Orthogonal, 4, BoardRanks, false, attacksOnly);
        break;
    case PieceType::Bishop:
        add_slider_moves(moves, id, Diagonal, 4, BoardRanks, false, attacksOnly);
        break;
    case PieceType::Berserker: {
        // Native SimulatedBerserker iterates every coordinate in its growing
        // Chebyshev-radius box and does not trace rays between them.
        // Do not clamp to the eight-file width: the board is ten ranks high,
        // and native powerLevel continues growing. A level-nine Berserker on
        // rank 1 can therefore reach rank 10 even though horizontal reach had
        // already saturated two captures earlier.
        const int radius = 1 + int(piece.power);
        for (int file = std::max(0, file_of(piece.square) - radius);
             file <= std::min(BoardFiles - 1, file_of(piece.square) + radius); ++file)
            for (int rank = std::max(0, rank_of(piece.square) - radius);
                 rank <= std::min(BoardRanks - 1, rank_of(piece.square) + radius); ++rank) {
                const int to = make_square(file, rank);
                // The box generator compares destination occupancy directly;
                // hidden enemy Ghost cells are legal blind captures.
                if (to != piece.square && can_land(id, to, attacksOnly))
                    moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
            }
        break;
    }
    case PieceType::Bomb:
        add_slider_moves(moves, id, Orthogonal, 4, 2, false, attacksOnly);
        add_step_moves(moves, id, Diagonal, 4, 1, false, attacksOnly);
        break;
    case PieceType::Ninja:
        add_slider_moves(moves, id, Around, 8, 3, true, attacksOnly);
        break;
    case PieceType::Turtle:
        add_step_moves(moves, id, Orthogonal, 4, 1, false, attacksOnly);
        break;
    case PieceType::Mage:
        if (!attacksOnly)
            for (int target = 0; target < pieceCount_; ++target)
                if (target != id && pieces_[target].alive && pieces_[target].onBoard &&
                    pieces_[target].color == piece.color) {
                    if (pieces_[target].type != PieceType::Giant) {
                        moves.push_back({piece.square, pieces_[target].square,
                                         static_cast<std::uint8_t>(target), MoveKind::Swap});
                        continue;
                    }
                    // Native Mage generation visits all four Giant tiles.
                    // The selected tile goes to the Mage while the Giant is
                    // translated so that tile lands on the Mage's origin.
                    Bitboard tiles = footprint(target, pieces_[target].square);
                    while (tiles) {
                        const int selected = pop_lsb(tiles);
                        const int destinationFile = file_of(piece.square) +
                          file_of(pieces_[target].square) - file_of(selected);
                        const int destinationRank = rank_of(piece.square) +
                          rank_of(pieces_[target].square) - rank_of(selected);
                        if (destinationFile < 0 || destinationFile >= BoardFiles ||
                            destinationRank < 0 || destinationRank >= BoardRanks)
                            continue;
                        const int destination = make_square(destinationFile, destinationRank);
                        if (footprint(target, destination))
                            moves.push_back({piece.square, static_cast<std::uint8_t>(selected),
                                             static_cast<std::uint8_t>(target), MoveKind::Swap});
                    }
                }
        break;
    case PieceType::Penguin:
        // Native Freeze/Penguin can step to any adjacent empty square but has
        // no attack action.  Its offensive effect is the adjacent freeze aura;
        // visible occupied squares must not be generated as captures or
        // attack-map entries. An enemy hidden Ghost is deliberately the one
        // exception: native SimulatedFreeze routes that apparently empty cell
        // through SimulateMove(unavailable=false), and ordinary MakeMove then
        // reveals the blind capture. This matters alongside the live h1-h2
        // rejection against a visible enemy Dragon.
        if (!attacksOnly) {
            add_step_moves(moves, id, Around, 8, 1, false, false);
            moves.erase(std::remove_if(moves.begin() + first, moves.end(), [this](const Move& move) {
                            const int target = board_[move.to];
                            return target != NoPiece &&
                              (pieces_[target].visible ||
                               pieces_[target].type != PieceType::Ghost);
                        }), moves.end());
        }
        break;
    case PieceType::Devil:
        if (!attacksOnly)
            for (int file = std::max(0, file_of(piece.square) - 2);
                 file <= std::min(BoardFiles - 1, file_of(piece.square) + 2); ++file)
                for (int rank = std::max(0, rank_of(piece.square) - 2);
                     rank <= std::min(BoardRanks - 1, rank_of(piece.square) + 2); ++rank) {
                    const int to = make_square(file, rank);
                    const int target = board_[to];
                    const bool blindEnemyGhost =
                      target != NoPiece && pieces_[target].color != piece.color &&
                      pieces_[target].type == PieceType::Ghost &&
                      !pieces_[target].visible;
                    if (to != piece.square &&
                        (target == NoPiece || blindEnemyGhost))
                        moves.push_back({piece.square, static_cast<std::uint8_t>(to), 0,
                                         MoveKind::Spawn});
                }
        break;
    case PieceType::Sludge:
        add_slider_moves(moves, id, Orthogonal, 4, 2, false, false);
        // Sludge shares the generic transparent Ghost ray, but its MakeMove
        // override resolves a blind landing as a mutual knockout.
        moves.erase(std::remove_if(moves.begin() + first, moves.end(), [this](const Move& move) {
                        const int target = board_[move.to];
                        return target != NoPiece &&
                          !(pieces_[target].type == PieceType::Ghost &&
                            !pieces_[target].visible);
                    }), moves.end());
        break;
    case PieceType::Sniper:
        if (!attacksOnly) {
            const int file = file_of(piece.square);
            const int rank = rank_of(piece.square);
            for (const int horizontal : {-1, 1}) {
                const int targetFile = file + horizontal;
                if (targetFile < 0 || targetFile >= BoardFiles)
                    continue;
                const int to = make_square(targetFile, rank);
                const int target = board_[to];
                // Visible occupancy blocks a lateral step, but the native
                // generator treats a hidden enemy Ghost as an apparently
                // empty destination. Sniper.MakeMove then resolves a blind
                // mutual knockout on that square.
                const bool hiddenEnemyGhost =
                  target != NoPiece && pieces_[target].color != piece.color &&
                  pieces_[target].type == PieceType::Ghost &&
                  !pieces_[target].visible;
                if (target == NoPiece || hiddenEnemyGhost)
                    moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
            }
        }
        {
            const int direction = piece.color == Color::White ? 1 : -1;
            for (int rank = rank_of(piece.square) + direction;
                 rank >= 0 && rank < BoardRanks; rank += direction) {
                const int targetSquare = make_square(file_of(piece.square), rank);
                const int target = board_[targetSquare];
                if (target == NoPiece)
                    continue;
                if (pieces_[target].type == PieceType::Ghost && !pieces_[target].visible)
                    continue;
                if (pieces_[target].color != piece.color)
                    // A Giant's stored anchor need not lie on the Sniper's
                    // file. Native Shoot targets the actual footprint cell
                    // encountered by the ray, while auxiliary retains the
                    // shared Giant model that receives the hit.
                    moves.push_back({piece.square, static_cast<std::uint8_t>(targetSquare),
                                     static_cast<std::uint8_t>(target), MoveKind::Shoot});
                break;
            }
        }
        break;
    case PieceType::Checker:
    case PieceType::CheckerKing:
        add_checker_moves(moves, id, attacksOnly);
        break;
    case PieceType::Giant:
        add_giant_moves(moves, id, attacksOnly);
        break;
    case PieceType::Copycat:
    case PieceType::CopycatClone:
        add_copycat_moves(moves, id, attacksOnly);
        break;
    case PieceType::Angel:
        if (!attacksOnly && piece.host == NoPiece && piece.onBoard)
            for (int target = 0; target < pieceCount_; ++target)
                if (target != id && pieces_[target].alive && pieces_[target].onBoard &&
                    pieces_[target].color == piece.color && pieces_[target].square != piece.square &&
                    (pieces_[target].type != PieceType::Halo || pieces_[target].link == NoPiece))
                    moves.push_back({piece.square, pieces_[target].square,
                                     static_cast<std::uint8_t>(target), MoveKind::Link});
        break;
    case PieceType::Fisherman:
        add_fisherman_moves(moves, id, attacksOnly);
        break;
    case PieceType::Dragon:
        add_slider_moves(moves, id, Diagonal, 4, BoardRanks, false, attacksOnly);
        add_knight_moves(moves, id, attacksOnly);
        break;
    case PieceType::Goop:
    case PieceType::Minion:
    case PieceType::Halo:
    case PieceType::Count:
        break;
    }
}

std::vector<Move> Position::moves_for(int id, bool attacksOnly) const {
    std::vector<Move> moves;
    append_moves_for(moves, id, attacksOnly);
    return moves;
}

bool Position::checker_has_capture(int id) const {
    if (id < 0 || id >= pieceCount_ || !pieces_[id].alive)
        return false;
    const auto moves = moves_for(id, true);
    return std::any_of(moves.begin(), moves.end(), [this](const Move& move) {
        return move.auxiliary != 0 || board_[move.to] != NoPiece;
    });
}

bool Position::has_forced_action() const {
    return forcedPiece_ != NoPiece;
}

std::vector<Move> Position::legal_moves() const {
    if (forcedTimeoutWinner_ >= 0 ||
        !has_real_king(Color::White) || !has_real_king(Color::Black) ||
        !is_checkmate_possible())
        return {};

    return filter_legal_moves(pseudo_legal_moves());
}

bool Position::has_legal_move() const {
    if (forcedTimeoutWinner_ >= 0 ||
        !has_real_king(Color::White) || !has_real_king(Color::Black) ||
        !is_checkmate_possible())
        return false;
    const Color mover = sideToMove_;
    for (const Move& move : pseudo_legal_moves()) {
        Position child = *this;
        if (child.apply_move_unchecked(move) &&
            child.legal_after_unchecked_move(mover))
            return true;
    }
    return false;
}

std::vector<Move> Position::filter_legal_moves(std::vector<Move> moves) const {
    annotate_captures(moves);
    const Color mover = sideToMove_;
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const Move& move) {
        // A disposable child needs one Position copy. The former full-state
        // Undo path copied the same ~2 KiB state both into Undo and back into
        // the reused child for every candidate.
        Position child = *this;
        if (!child.apply_move_unchecked(move))
            return true;
        return !child.legal_after_unchecked_move(mover);
    }), moves.end());
    return moves;
}

bool Position::legal_after_unchecked_move(Color mover) const {
    const bool ownKingAlive = has_real_king(mover);
    const bool jesterAlive = pieces(mover, PieceType::Jester) != 0;
    const bool continuation = sideToMove_ == mover && has_forced_action();
    const bool opponentKingDead = !has_real_king(~mover);
        // Native hidden-royal semantics suspend ordinary check legality while
        // the moving side still owns a Jester. Either royal silhouette may be
        // attacked because the opponent has not proved which one is the real
        // King; the game ends only when the real King is actually removed.
        // Re-enable check immediately in a child where the Jester died (for
        // example in the moving side's own Bomb blast).
        // A Prince's first quiet step and a Checker's intermediate jump do
        // not end the turn, so check is judged after the forced sequence. Do
        // not accept an intermediate action merely because a continuation
        // exists: at least one recursively legal completion must actually
        // resolve check. Live Ranked exposed this with a Prince h3-g4 while a
        // Sniper on a8 shot through an unseen a3 Ghost to the King on a1.
    const bool continuationCanFinish = continuation && !legal_moves().empty();
    return ownKingAlive &&
      (opponentKingDead || jesterAlive || continuationCanFinish ||
       (!continuation && !real_king_threatened(mover)));
}

bool Position::is_forcing_action(const Move& move) const {
    if (is_capture(move))
        return true;
    if (move.kind == MoveKind::Pull) {
        const int target = piece_on(move.to);
        const int actor = piece_on(move.from);
        if (target == NoPiece || actor == NoPiece)
            return false;
        if (pieces_[target].color != pieces_[actor].color)
            return true;
        if (pieces_[target].type != PieceType::Giant)
            // A populated landing cell is the native invisible-Ghost branch;
            // both the dragged character and occupant receive death callbacks.
            return valid_square(move.auxiliary) &&
                   piece_on(move.auxiliary) != NoPiece;
        const int destinationFile = file_of(pieces_[target].square) +
                                    file_of(move.auxiliary) - file_of(move.to);
        const int destinationRank = rank_of(pieces_[target].square) +
                                    rank_of(move.auxiliary) - rank_of(move.to);
        return destinationFile >= 0 && destinationFile < BoardFiles &&
               destinationRank >= 0 && destinationRank < BoardRanks &&
               !victims_on(footprint(target,
                 make_square(destinationFile, destinationRank)), target).empty();
    }
    // A Mage swap with a Giant delegates to Giant relocation and may knock
    // out multiple characters even though neither selected piece is hostile.
    return move.kind == MoveKind::Swap && move.auxiliary < pieceCount_ &&
           pieces_[move.auxiliary].type == PieceType::Giant;
}

std::vector<Move> Position::legal_forcing_moves() const {
    if (forcedTimeoutWinner_ >= 0 ||
        !has_real_king(Color::White) || !has_real_king(Color::Black) ||
        !is_checkmate_possible())
        return {};
    if (has_forced_action())
        return legal_moves();

    return filter_legal_moves(pseudo_forcing_moves());
}

std::vector<Move> Position::pseudo_forcing_moves() const {
    if (has_forced_action()) {
        auto moves = pseudo_legal_moves();
        annotate_captures(moves);
        return moves;
    }

    std::vector<Move> moves;
    moves.reserve(64);
    for (int id = 0; id < pieceCount_; ++id) {
        if (!pieces_[id].alive || !pieces_[id].onBoard ||
            pieces_[id].color != sideToMove_)
            continue;
        const PieceType type = pieces_[id].type;
        // These pieces have indirect forcing actions which their native
        // attack-only generators omit: a mirrored CopyCat capture, a
        // Mage/Fisherman Giant relocation, or a blind Sludge/Penguin Ghost
        // collision.
        const bool needsFullList =
          type == PieceType::Mage || type == PieceType::Fisherman ||
          type == PieceType::Copycat || type == PieceType::CopycatClone ||
          type == PieceType::Sludge || type == PieceType::Penguin;
        append_moves_for(moves, id, !needsFullList);
    }
    annotate_captures(moves);
    moves.erase(std::remove_if(moves.begin(), moves.end(),
      [this](const Move& move) { return !is_forcing_action(move); }), moves.end());
    return moves;
}

std::vector<Move> Position::pseudo_legal_moves() const {
    std::vector<Move> moves;
    moves.reserve(128);
    if (forcedPiece_ != NoPiece) {
        append_moves_for(
            moves, forcedPiece_, continuation_ == Continuation::CheckerJump);
        return moves;
    }

    for (int id = 0; id < pieceCount_; ++id) {
        if (!pieces_[id].alive || !pieces_[id].onBoard || pieces_[id].color != sideToMove_)
            continue;
        append_moves_for(moves, id);
    }
    return moves;
}

bool Position::in_check() const {
    if (pieces(sideToMove_, PieceType::Jester) || !has_real_king(sideToMove_))
        return false;

    // In King/Devil/Minion endgames there are no rays, blasts, possessions or
    // rescues. With separated Kings and no Minion immediately behind either
    // royal, a turn phase cannot create check. This conservative bitboard
    // rejection avoids copying/marching an entire army at every quiet leaf;
    // collisions near a royal still use the full native simulation below.
    const Bitboard kings = pieces(Color::White, PieceType::King) |
                           pieces(Color::Black, PieceType::King);
    const Bitboard whiteMinions = pieces(Color::White, PieceType::Minion);
    const Bitboard blackMinions = pieces(Color::Black, PieceType::Minion);
    if ((whiteMinions || blackMinions) &&
        !(occupied() & ~(kings | whiteMinions | blackMinions |
                        pieces(Color::White, PieceType::Devil) |
                        pieces(Color::Black, PieceType::Devil))) &&
        !((whiteMinions << BoardFiles | blackMinions >> BoardFiles) & kings) &&
        !(threat_geometry().king[lsb_square(pieces(sideToMove_, PieceType::King))] &
          pieces(~sideToMove_, PieceType::King)))
        return false;

    // Bot.GetAllAvailableMoves calls ChangeTurn before DidKingDie, then undoes
    // it. Usually that changes no attack-relevant state: avoid copying the
    // position or running the automatic phase in the ordinary chess case.
    bool changesAttacks = bool(pieces(~sideToMove_, PieceType::Minion));
    for (int id = 0; !changesAttacks && id < pieceCount_; ++id)
        // Without Minions, only a newly readied attacker can change threats:
        // a cooldown above one stays blocked and a victim's cooldown cannot
        // prevent its capture. With friendly Minions, retain all decrements
        // because an indirect reply simulation can start their next phase.
        changesAttacks = pieces_[id].alive && pieces_[id].cooldown &&
          (pieces(sideToMove_, PieceType::Minion) ||
           (pieces_[id].color != sideToMove_ && pieces_[id].cooldown == 1));
    if (!changesAttacks)
        return real_king_threatened(sideToMove_);

    Position next = *this;
    next.finish_turn();
    // Automatic friendly collisions/Bomb chains may kill the attacking King,
    // including both Kings. Native DidKingDie requires an opposing winner,
    // not a simultaneous knockout, to report check.
    return next.has_real_king(~sideToMove_) &&
           next.real_king_threatened(sideToMove_);
}

bool Position::real_king_threatened(Color color) const {
    if (!has_real_king(color))
        return true;

    const int kingSquare = lsb_square(byType_[index(color)][index(PieceType::King)]);

    const auto adjacent = [](int lhs, int rhs) {
        return std::abs(file_of(lhs) - file_of(rhs)) <= 1 &&
               std::abs(rank_of(lhs) - rank_of(rhs)) <= 1;
    };

    // A captured Bomb can reach the King through a chain of adjacent Bombs.
    // Build that connected danger set once, then reject the many captures
    // whose victim and blast cannot possibly affect the royal.
    const ThreatGeometry& geometry = threat_geometry();
    const Bitboard allBombs = pieces(Color::White, PieceType::Bomb) |
                              pieces(Color::Black, PieceType::Bomb);
    Bitboard dangerousBombs = allBombs & geometry.king[kingSquare];
    Bitboard frontier = dangerousBombs;
    while (frontier) {
        const Bitboard added = geometry.king[pop_lsb(frontier)] &
                               allBombs & ~dangerousBombs;
        dangerousBombs |= added;
        frontier |= added;
    }

    const Bitboard royalDanger = square_bb(kingSquare) | dangerousBombs;
    const int kingId = board_[kingSquare];
    const bool kingProtected = attached_angel(kingId) != NoPiece;
    const auto isKingVictim = [&](int id) {
        return id == kingId;
    };
    const auto dangerousVictim = [&](int id) {
        return id != NoPiece && id < pieceCount_ && pieces_[id].alive &&
          ((pieces_[id].type == PieceType::King && pieces_[id].color == color) ||
           (pieces_[id].type == PieceType::Bomb &&
            (dangerousBombs & square_bb(pieces_[id].square))));
    };
    const auto dangerousBlast = [&](int center) {
        if (adjacent(center, kingSquare))
            return true;
        Bitboard bombs = dangerousBombs;
        while (bombs)
            if (adjacent(center, pop_lsb(bombs)))
                return true;
        return false;
    };

    // Chess Ultimate defines check through the same full piece simulations
    // used for ordinary actions.  This is important for indirect royal kills:
    // capturing a nearby Bomb, translating a Giant footprint, or possessing
    // the King can remove the real King even when the attack does not land on
    // the King's square.
    // Per-piece generation depends on the actor's state and color, not the
    // position's nominal side to move. Generate opponent replies directly
    // from this immutable position; construct a disposable attacker child
    // only for the rare indirect/Angel case that actually needs simulation.
    const Position& attacker = *this;
    const Color attackingColor = ~color;
    const auto ordinaryActorCanReachDanger = [&](int actor) {
        const PieceState& piece = attacker.pieces_[actor];
        Bitboard reach = 0;
        switch (piece.type) {
        case PieceType::King:
        case PieceType::Jester:
        case PieceType::Parasite:
        case PieceType::Prince: reach = geometry.king[piece.square]; break;
        case PieceType::Knight: reach = geometry.knight[piece.square]; break;
        case PieceType::Pawn:
            reach = geometry.pawn[static_cast<int>(piece.color)][piece.square]; break;
        case PieceType::Queen: reach = geometry.queen[piece.square]; break;
        case PieceType::Rook: reach = geometry.rook[piece.square]; break;
        case PieceType::Bishop: reach = geometry.bishop[piece.square]; break;
        case PieceType::Ninja: reach = geometry.ninja[piece.square]; break;
        case PieceType::Turtle: reach = geometry.turtle[piece.square]; break;
        case PieceType::Sniper:
            reach = geometry.sniper[static_cast<int>(piece.color)][piece.square]; break;
        case PieceType::Dragon: reach = geometry.dragon[piece.square]; break;
        case PieceType::Berserker: {
            Bitboard danger = royalDanger;
            const int radius = 1 + int(piece.power);
            while (danger) {
                const int target = pop_lsb(danger);
                if (std::max(std::abs(file_of(target) - file_of(piece.square)),
                             std::abs(rank_of(target) - rank_of(piece.square))) <= radius)
                    return true;
            }
            return false;
        }
        default:
            // Bomb blasts, Giant footprints, paired CopyCats, Checker
            // victims, and forced relocations need the full native action
            // generator below. Returning true is deliberately conservative.
            return true;
        }
        return bool(reach & royalDanger);
    };
    std::vector<Move> replies;
    for (int actor = 0; actor < attacker.pieceCount_; ++actor) {
        if (!attacker.pieces_[actor].alive || !attacker.pieces_[actor].onBoard ||
            attacker.pieces_[actor].color != attackingColor)
            continue;
        if (attacker.frozen(actor) || attacker.pieces_[actor].cooldown)
            continue;
        // Ghost attacks never produce check/checkmate in the app. A royal may
        // enter a hidden Ghost's adjacency, reveal it, and remain alive until
        // the Ghost actually captures it on a later action.
        // Minion advances belong to the turn phase, not the manual attack
        // list. Devil spawns and inert Goop/Halos also have no such attacks.
        if (attacker.pieces_[actor].type == PieceType::Ghost ||
            attacker.pieces_[actor].type == PieceType::Minion ||
            attacker.pieces_[actor].type == PieceType::Devil ||
            attacker.pieces_[actor].type == PieceType::Goop ||
            attacker.pieces_[actor].type == PieceType::Halo)
            continue;

        const PieceType actorType = attacker.pieces_[actor].type;
        if (!ordinaryActorCanReachDanger(actor))
            continue;
        const bool needsQuietCompanion =
          actorType == PieceType::Mage || actorType == PieceType::Fisherman ||
          actorType == PieceType::Copycat || actorType == PieceType::CopycatClone;
        // Most characters can knock out a King only through their native
        // attack list. Mage and Fisherman can forcibly translate a Giant,
        // while either CopyCat half may move quietly as its partner captures.
        if (replies.capacity() == 0)
            replies.reserve(128);
        replies.clear();
        attacker.append_moves_for(replies, actor, !needsQuietCompanion);
        for (const Move& reply : replies) {
            // Simulate only actions whose affected square set intersects the
            // King or a Bomb chain leading to it. This retains native death,
            // Angel, Parasite, and blast semantics without applying unrelated
            // captures merely to discover that the King survived.
            bool canKnockOut = false;
            bool directlyHitsKing = false;
            if (actorType == PieceType::Bomb) {
                canKnockOut = dangerousBlast(reply.to);
                directlyHitsKing = adjacent(reply.to, kingSquare);
            }
            else if (actorType == PieceType::Giant) {
                const Bitboard destination = attacker.footprint(actor, reply.to);
                canKnockOut = bool(destination & royalDanger);
                directlyHitsKing = bool(destination & square_bb(kingSquare));
            }
            else if (reply.kind == MoveKind::Swap &&
                     reply.auxiliary < attacker.pieceCount_ &&
                     attacker.pieces_[reply.auxiliary].type == PieceType::Giant) {
                const int giant = reply.auxiliary;
                const int destinationFile = file_of(reply.from) +
                  file_of(attacker.pieces_[giant].square) - file_of(reply.to);
                const int destinationRank = rank_of(reply.from) +
                  rank_of(attacker.pieces_[giant].square) - rank_of(reply.to);
                if (destinationFile >= 0 && destinationFile < BoardFiles &&
                    destinationRank >= 0 && destinationRank < BoardRanks) {
                    const Bitboard destination = attacker.footprint(
                      giant, make_square(destinationFile, destinationRank));
                    canKnockOut = bool(destination & royalDanger);
                    directlyHitsKing = bool(destination & square_bb(kingSquare));
                }
            }
            else if (reply.kind == MoveKind::Pull) {
                const int pulled = valid_square(reply.to) ? attacker.board_[reply.to] : NoPiece;
                if (pulled != NoPiece && attacker.pieces_[pulled].type == PieceType::Giant) {
                    const int destinationFile = file_of(attacker.pieces_[pulled].square) +
                                                file_of(reply.auxiliary) - file_of(reply.to);
                    const int destinationRank = rank_of(attacker.pieces_[pulled].square) +
                                                rank_of(reply.auxiliary) - rank_of(reply.to);
                    if (destinationFile >= 0 && destinationFile < BoardFiles &&
                        destinationRank >= 0 && destinationRank < BoardRanks) {
                        const Bitboard destination = attacker.footprint(
                          pulled, make_square(destinationFile, destinationRank));
                        canKnockOut = bool(destination & royalDanger);
                        directlyHitsKing = bool(destination & square_bb(kingSquare));
                    }
                }
                else if (pulled != NoPiece && valid_square(reply.auxiliary) &&
                         attacker.board_[reply.auxiliary] != NoPiece) {
                    // Fisherman forced collision invokes SimulateDeath on the
                    // dragged character and invisible landing occupant. It is
                    // a direct royal threat when the dragged character is the
                    // real King, and a dragged Bomb can reach the King through
                    // its landing-square blast/chain.
                    directlyHitsKing =
                      attacker.pieces_[pulled].type == PieceType::King &&
                      attacker.pieces_[pulled].color == color;
                    canKnockOut = directlyHitsKing ||
                      (attacker.pieces_[pulled].type == PieceType::Bomb &&
                       dangerousBlast(reply.auxiliary));
                }
            }
            else if (actorType == PieceType::Copycat ||
                     actorType == PieceType::CopycatClone) {
                const int primary = valid_square(reply.to) ? attacker.board_[reply.to] : NoPiece;
                const int mirrored = valid_square(reply.auxiliary)
                                   ? attacker.board_[reply.auxiliary] : NoPiece;
                canKnockOut = dangerousVictim(primary) || dangerousVictim(mirrored);
                directlyHitsKing = isKingVictim(primary) || isKingVictim(mirrored);
            }
            else if (reply.kind == MoveKind::Shoot) {
                canKnockOut = dangerousVictim(reply.auxiliary);
                directlyHitsKing = isKingVictim(reply.auxiliary);
            }
            else if ((actorType == PieceType::Checker ||
                      actorType == PieceType::CheckerKing) &&
                     valid_square(reply.auxiliary)) {
                canKnockOut = dangerousVictim(attacker.board_[reply.auxiliary]);
                directlyHitsKing = isKingVictim(attacker.board_[reply.auxiliary]);
            }
            else if (valid_square(reply.to)) {
                canKnockOut = dangerousVictim(attacker.board_[reply.to]);
                directlyHitsKing = isKingVictim(attacker.board_[reply.to]);
            }
            if (!canKnockOut)
                continue;
            // SimulateDeath can save a King only through an attached Angel.
            // With no protector, every native attack class above necessarily
            // removes or possesses the real King. Avoid copying and replaying
            // the complete position merely to observe that guaranteed result.
            if (directlyHitsKing && !kingProtected)
                return true;
            Position child = attacker;
            child.sideToMove_ = attackingColor;
            child.forcedPiece_ = NoPiece;
            child.continuation_ = Continuation::None;
            if (!child.apply_move_unchecked(reply))
                continue;
            const bool killed = !child.has_real_king(color);
            if (killed)
                return true;
        }
    }
    return false;
}

bool Position::ordinary_predecessor_king_safe() const {
    if (has_forced_action())
        return true;
    const Color previous = ~sideToMove_;
    return pieces(previous, PieceType::Jester) != 0 ||
           !real_king_threatened(previous);
}

bool Position::is_capture(const Move& move) const {
    if (move.flags & Move::CaptureKnown)
        return move.flags & Move::Capture;
    if (move.kind == MoveKind::Shoot)
        return true;
    if (move.kind != MoveKind::Normal)
        return false;
    if (valid_square(move.to) && board_[move.to] != NoPiece)
        return true;
    const int actor = valid_square(move.from) ? board_[move.from] : NoPiece;
    if (actor == NoPiece)
        return false;
    if (pieces_[actor].type == PieceType::Pawn && move.to == enPassantSquare_)
        return true;
    if ((pieces_[actor].type == PieceType::Checker ||
         pieces_[actor].type == PieceType::CheckerKing) &&
        move.auxiliary != 0 && valid_square(move.auxiliary) &&
        board_[move.auxiliary] != NoPiece)
        return true;
    if ((pieces_[actor].type == PieceType::Copycat ||
         pieces_[actor].type == PieceType::CopycatClone) &&
        valid_square(move.auxiliary) && board_[move.auxiliary] != NoPiece)
        return true;
    if (pieces_[actor].type == PieceType::Giant) {
        const Bitboard own = footprint(actor, pieces_[actor].square);
        return bool(footprint(actor, move.to) & (occupied() & ~own));
    }
    return false;
}

void Position::annotate_captures(std::vector<Move>& moves) const {
    for (Move& move : moves) {
        if (move.flags & Move::CaptureKnown)
            continue;
        const bool capture = is_capture(move);
        move.flags = static_cast<std::uint8_t>(
          Move::CaptureKnown | (capture ? Move::Capture : 0));
    }
}

bool Position::supports_ordinary_exchange() const {
    const auto ordinary = [](PieceType type) {
        switch (type) {
        case PieceType::King:
        case PieceType::Jester:
        case PieceType::Knight:
        case PieceType::Pawn:
        case PieceType::Queen:
        case PieceType::Rook:
        case PieceType::Bishop:
        case PieceType::Ninja:
        case PieceType::Turtle:
        case PieceType::Dragon: return true;
        default: return false;
        }
    };
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& piece = pieces_[id];
        if (!piece.alive)
            continue;
        if (!ordinary(piece.type) || !piece.onBoard || piece.action || piece.cooldown ||
            piece.freezeCount || piece.power || piece.link != NoPiece ||
            piece.host != NoPiece || !piece.visible)
            return false;
    }
    return true;
}

std::optional<int> Position::static_exchange(const Move& move) const {
    if (move.kind != MoveKind::Normal || move.promotion != PieceType::Count ||
        !valid_square(move.from) || !valid_square(move.to) ||
        move.to == enPassantSquare_ || rank_of(move.to) == 0 ||
        rank_of(move.to) == BoardRanks - 1 || !supports_ordinary_exchange())
        return std::nullopt;
    const int firstAttacker = board_[move.from];
    const int firstVictim = board_[move.to];
    if (firstAttacker == NoPiece || firstVictim == NoPiece ||
        pieces_[firstAttacker].color == pieces_[firstVictim].color)
        return std::nullopt;

    std::array<int, MaxPieces> square{};
    std::array<bool, MaxPieces> alive{};
    std::array<int, BoardSquares> board{};
    board.fill(NoPiece);
    for (int id = 0; id < pieceCount_; ++id) {
        square[id] = pieces_[id].square;
        alive[id] = pieces_[id].alive;
        if (alive[id])
            board[square[id]] = id;
    }
    const auto attacks = [&](int id, int target) {
        const PieceState& piece = pieces_[id];
        const int fileDelta = file_of(target) - file_of(square[id]);
        const int rankDelta = rank_of(target) - rank_of(square[id]);
        const int fileDistance = std::abs(fileDelta);
        const int rankDistance = std::abs(rankDelta);
        const auto rayClear = [&] {
            const int fileStep = (fileDelta > 0) - (fileDelta < 0);
            const int rankStep = (rankDelta > 0) - (rankDelta < 0);
            int file = file_of(square[id]) + fileStep;
            int rank = rank_of(square[id]) + rankStep;
            while (file != file_of(target) || rank != rank_of(target)) {
                if (board[make_square(file, rank)] != NoPiece)
                    return false;
                file += fileStep;
                rank += rankStep;
            }
            return true;
        };
        switch (piece.type) {
        case PieceType::King:
        case PieceType::Jester:
            return std::max(fileDistance, rankDistance) == 1;
        case PieceType::Knight:
            return (fileDistance == 1 && rankDistance == 2) ||
                   (fileDistance == 2 && rankDistance == 1);
        case PieceType::Pawn:
            return fileDistance == 1 &&
              rankDelta == (piece.color == Color::White ? 1 : -1);
        case PieceType::Queen:
            return (fileDelta == 0 || rankDelta == 0 || fileDistance == rankDistance) &&
                   rayClear();
        case PieceType::Rook:
            return (fileDelta == 0 || rankDelta == 0) && rayClear();
        case PieceType::Bishop:
            return fileDistance == rankDistance && rayClear();
        case PieceType::Ninja:
            return std::max(fileDistance, rankDistance) <= 3 &&
              (fileDelta == 0 || rankDelta == 0 || fileDistance == rankDistance);
        case PieceType::Turtle:
            return fileDistance + rankDistance == 1;
        case PieceType::Dragon:
            return ((fileDistance == 1 && rankDistance == 2) ||
                    (fileDistance == 2 && rankDistance == 1)) ||
                   (fileDistance == rankDistance && rayClear());
        default: return false;
        }
    };

    std::array<int, MaxPieces> gains{};
    int depth = 0;
    gains[0] = material_value(pieces_[firstVictim].type);
    int capturedValue = material_value(pieces_[firstAttacker].type);
    int occupant = firstAttacker;
    alive[firstVictim] = false;
    board[move.from] = NoPiece;
    board[move.to] = firstAttacker;
    square[firstAttacker] = move.to;
    Color side = pieces_[firstVictim].color;

    while (depth + 1 < MaxPieces) {
        int least = NoPiece;
        int leastValue = 1'000'000;
        for (int id = 0; id < pieceCount_; ++id) {
            if (!alive[id] || id == occupant || pieces_[id].color != side ||
                !attacks(id, move.to))
                continue;
            const int value = material_value(pieces_[id].type);
            if (value < leastValue) {
                least = id;
                leastValue = value;
            }
        }
        if (least == NoPiece)
            break;
        ++depth;
        gains[depth] = capturedValue - gains[depth - 1];
        if (std::max(-gains[depth - 1], gains[depth]) < 0)
            break;
        alive[occupant] = false;
        board[square[least]] = NoPiece;
        board[move.to] = least;
        square[least] = move.to;
        occupant = least;
        capturedValue = leastValue;
        side = ~side;
    }
    while (depth > 0) {
        gains[depth - 1] = -std::max(-gains[depth - 1], gains[depth]);
        --depth;
    }
    return gains[0];
}

bool Position::is_legal(const Move& move) const {
    const auto moves = legal_moves();
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

std::vector<int> Position::victims_on(Bitboard mask, int exceptId) const {
    std::vector<int> victims;
    victims.reserve(9);
    std::array<bool, MaxPieces> seen{};
    while (mask) {
        const int square = pop_lsb(mask);
        const int id = board_[square];
        if (id != NoPiece && id != exceptId && !seen[id]) {
            seen[id] = true;
            victims.push_back(id);
        }
    }
    return victims;
}

void Position::explode_at(int center, int attacker) {
    Bitboard blast = 0;
    const int centerFile = file_of(center);
    const int centerRank = rank_of(center);
    for (int file = std::max(0, centerFile - 1);
         file <= std::min(BoardFiles - 1, centerFile + 1); ++file)
        for (int rank = std::max(0, centerRank - 1);
             rank <= std::min(BoardRanks - 1, centerRank + 1); ++rank)
            blast |= square_bb(make_square(file, rank));
    std::vector<int> chainedBombs;
    for (const int victim : victims_on(blast)) {
        if (!pieces_[victim].alive)
            continue;
        const bool bomb = pieces_[victim].type == PieceType::Bomb;
        const int bombSquare = pieces_[victim].square;
        remove_piece(victim);
        // SimulatedBomb::SimulateDeath invokes its own explosion even when it
        // is only collateral damage from another Bomb. An Angel save leaves
        // it alive and suppresses that secondary blast.
        if (bomb && !pieces_[victim].alive)
            chainedBombs.push_back(bombSquare);
    }
    for (const int bombSquare : chainedBombs)
        explode_at(bombSquare, NoPiece);
    if (attacker != NoPiece && attacker < pieceCount_ && pieces_[attacker].alive &&
        (footprint(attacker, pieces_[attacker].square) & blast))
        remove_piece(attacker);
}

void Position::capture_piece(int victim, int attacker, const Move& move) {
    if (victim == NoPiece || !pieces_[victim].alive)
        return;
    const PieceType attackerType = pieces_[attacker].type;
    const PieceType victimType = pieces_[victim].type;

    auto possess = [this](int id, Color color) {
        // CopyCat is one deployable character represented by two linked board
        // models.  The native material trace transfers the full five-point
        // pair even when Parasite strikes only the clone half, so ownership
        // must change atomically for both halves.
        std::array<int, 2> ids = {id, NoPiece};
        if ((pieces_[id].type == PieceType::Copycat ||
             pieces_[id].type == PieceType::CopycatClone) &&
            pieces_[id].link != NoPiece && pieces_[id].link < pieceCount_ &&
            pieces_[pieces_[id].link].alive)
            ids[1] = pieces_[id].link;

        std::array<bool, 2> wasPlaced = {false, false};
        for (std::size_t index = 0; index < ids.size(); ++index) {
            const int possessed = ids[index];
            if (possessed == NoPiece || !pieces_[possessed].alive)
                continue;
            wasPlaced[index] = pieces_[possessed].onBoard &&
                               valid_square(pieces_[possessed].square) &&
                               board_[pieces_[possessed].square] == possessed;
            if (wasPlaced[index])
                erase_from_board(possessed);
        }
        for (std::size_t index = 0; index < ids.size(); ++index) {
            const int possessed = ids[index];
            if (possessed == NoPiece || !pieces_[possessed].alive)
                continue;
            pieces_[possessed].color = color;
            if (pieces_[possessed].type == PieceType::Ghost) {
                pieces_[possessed].parasiteTracked = true;
                pieces_[possessed].visible = true;
            }
            if (wasPlaced[index])
                place_on_board(possessed);
        }
    };

    // SimulatedBerserker::MakeMove grows on any attack recorded in Move.target,
    // including an Angel save or a possession interaction.
    if (attackerType == PieceType::Berserker)
        // The native byte is not capped. Its useful move radius saturates at
        // the board edge, but dynamic material keeps gaining 15 per attack.
        pieces_[attacker].power = static_cast<std::uint8_t>(
          pieces_[attacker].power + 1);

    // Parasite.AttackHandler takes over every target type directly, including
    // Bomb, Goop, Halo, and another Parasite.  The target's death callback is
    // never invoked, while the attacking Parasite disappears without calling
    // AngelFunction and transfers its attached Angels to the new host.
    if (attackerType == PieceType::Parasite) {
        const Color parasiteColor = pieces_[attacker].color;
        transfer_attached_angels(attacker, victim);
        remove_piece_internal(attacker, false);
        possess(victim, parasiteColor);
        return;
    }

    // Native Bomb death always creates a radius-one blast. Ordinary attackers
    // land inside it and die, while a genuinely ranged Sniper remains on its
    // origin and survives when that square is outside the radius. Resolve this
    // before Parasite/Halo branches so they cannot suppress the explosion.
    if (attackerType == PieceType::Bomb) {
        remove_piece(victim);
        // An attacking Bomb explodes where it lands. This is also the victim
        // Bomb's coordinate when two Bombs collide, so one blast is sufficient.
        explode_at(move.to, NoPiece);
        remove_piece(attacker);
        return;
    }
    if (victimType == PieceType::Bomb) {
        // Checker jumps and Giant moves can capture a character on a square
        // other than move.to. Native SimulatedBomb explodes around its own
        // targetSquare, not the attacker's landing/anchor square.
        const int bombSquare = pieces_[victim].square;
        remove_piece(victim);
        if (pieces_[victim].alive)  // Saved by an attached Angel.
            return;
        explode_at(bombSquare, NoPiece);
        // Ordinary attackers land on move.to and are therefore inside the
        // blast. A Sniper's Shoot is genuinely ranged: it remains on its
        // origin and survives when that square lies outside radius one. The
        // app demonstrated this with f10xf2 followed later by f10xf8.
        const int attackerSquare = move.kind == MoveKind::Shoot
                                 ? pieces_[attacker].square : move.to;
        if (pieces_[attacker].alive &&
            std::abs(file_of(attackerSquare) - file_of(bombSquare)) <= 1 &&
            std::abs(rank_of(attackerSquare) - rank_of(bombSquare)) <= 1)
            remove_piece(attacker);
        return;
    }

    if (victimType == PieceType::Halo) {
        remove_piece(victim);
        return;
    }

    // A parasite possesses its victim when it attacks. When attacked by an
    // opposing melee character, it possesses that attacker instead. Ranged,
    // support, friendly, and bomb attacks kill it normally (native
    // SimulatedParasite::SimulateDeath).
    if (victimType == PieceType::Parasite && is_melee(attackerType) &&
        attackerType != PieceType::Bomb && pieces_[attacker].color != pieces_[victim].color) {
        const Color parasiteColor = pieces_[victim].color;
        // Parasite.Die possesses a melee attacker before the generic death
        // path.  Its own AngelFunction is bypassed and its attached Angels
        // follow the possessed attacker.
        transfer_attached_angels(victim, attacker);
        remove_piece_internal(victim, false);
        possess(attacker, parasiteColor);
        return;
    }

    remove_piece(victim);
    if (pieces_[victim].alive)
        return;

    // Goop dies when hit, but also kills a melee attacker standing on its
    // square. Ranged/support attacks and bombs are excluded in the native
    // SimulatedGoop::Explode routine.
    if (victimType == PieceType::Goop && is_melee(attackerType) && attackerType != PieceType::Bomb) {
        remove_piece(attacker);
        return;
    }
}

void Position::clear_penguin_freeze(int penguin) {
    if (penguin < 0 || penguin >= pieceCount_ || !pieces_[penguin].alive ||
        pieces_[penguin].type != PieceType::Penguin)
        return;
    const int file = file_of(pieces_[penguin].square);
    const int rank = rank_of(pieces_[penguin].square);
    std::array<bool, MaxPieces> thawed{};
    for (const auto& direction : Around) {
        const std::uint8_t bit = penguin_direction_bit(direction[0], direction[1]);
        if (!(pieces_[penguin].action & bit))
            continue;
        const int targetFile = file + direction[0];
        const int targetRank = rank + direction[1];
        if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
            targetRank >= BoardRanks)
            continue;
        const int target = board_[make_square(targetFile, targetRank)];
        if (target != NoPiece && pieces_[target].type != PieceType::Penguin)
            thawed[target] = true;
    }
    // currentFrozenPieces is a HashSet in the shipping simulator. A Giant may
    // occupy several adjacent cells, but one Penguin contributes exactly one
    // freeze layer to that character.
    for (int target = 0; target < pieceCount_; ++target)
        if (thawed[target] && pieces_[target].freezeCount)
            --pieces_[target].freezeCount;
    pieces_[penguin].action = 0;
}

void Position::apply_penguin_freeze(int penguin) {
    if (penguin < 0 || penguin >= pieceCount_ || !pieces_[penguin].alive ||
        pieces_[penguin].type != PieceType::Penguin)
        return;
    pieces_[penguin].action = 0;
    const int file = file_of(pieces_[penguin].square);
    const int rank = rank_of(pieces_[penguin].square);
    std::array<bool, MaxPieces> frozen{};
    for (const auto& direction : Around) {
        const int targetFile = file + direction[0];
        const int targetRank = rank + direction[1];
        if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
            targetRank >= BoardRanks)
            continue;
        const int target = board_[make_square(targetFile, targetRank)];
        if (target == NoPiece || pieces_[target].type == PieceType::Penguin)
            continue;
        if (!frozen[target]) {
            frozen[target] = true;
            ++pieces_[target].freezeCount;
        }
        pieces_[penguin].action |= penguin_direction_bit(direction[0], direction[1]);
    }
}

void Position::detach_from_penguin_freezes(int target) {
    if (target < 0 || target >= pieceCount_ || !pieces_[target].alive)
        return;
    for (int penguin = 0; penguin < pieceCount_; ++penguin) {
        if (penguin == target || !pieces_[penguin].alive ||
            pieces_[penguin].type != PieceType::Penguin)
            continue;
        bool detached = false;
        for (const auto& direction : Around) {
            const std::uint8_t bit = penguin_direction_bit(direction[0], direction[1]);
            if (!(pieces_[penguin].action & bit))
                continue;
            const int file = file_of(pieces_[penguin].square) + direction[0];
            const int rank = rank_of(pieces_[penguin].square) + direction[1];
            if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
                continue;
            if (board_[make_square(file, rank)] == target) {
                pieces_[penguin].action &= static_cast<std::uint8_t>(~bit);
                detached = true;
            }
        }
        if (detached && pieces_[target].freezeCount)
            --pieces_[target].freezeCount;
    }
}

void Position::prepare_for_forced_relocation(int target) {
    if (target < 0 || target >= pieceCount_ || !pieces_[target].alive)
        return;
    if (pieces_[target].type == PieceType::Penguin)
        clear_penguin_freeze(target);
    detach_from_penguin_freezes(target);
    // Native Mage/Fisherman/Angel paths explicitly zero the displaced model's
    // freezeCount after removing it from currentFrozenPieces sets.
    pieces_[target].freezeCount = 0;
}

void Position::apply_forced_promotion(int target) {
    if (target < 0 || target >= pieceCount_ || !pieces_[target].alive)
        return;
    const int promotionRank = pieces_[target].color == Color::White
                            ? BoardRanks - 1 : 0;
    if (rank_of(pieces_[target].square) != promotionRank)
        return;
    if (pieces_[target].type == PieceType::Pawn)
        pieces_[target].type = PieceType::Queen;
    else if (pieces_[target].type == PieceType::Checker)
        pieces_[target].type = PieceType::CheckerKing;
}

bool Position::ghost_near_enemy_royal(int square, Color ghostColor) const {
    const int file = file_of(square);
    const int rank = rank_of(square);
    for (const auto& direction : Around) {
        const int targetFile = file + direction[0];
        const int targetRank = rank + direction[1];
        if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
            targetRank >= BoardRanks)
            continue;
        const int target = board_[make_square(targetFile, targetRank)];
        if (target != NoPiece && pieces_[target].color != ghostColor &&
            (pieces_[target].type == PieceType::King || pieces_[target].type == PieceType::Jester))
            return true;
    }
    return false;
}

void Position::reveal_ghosts_near(int square, Color royalColor) {
    const int file = file_of(square);
    const int rank = rank_of(square);
    for (const auto& direction : Around) {
        const int targetFile = file + direction[0];
        const int targetRank = rank + direction[1];
        if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
            targetRank >= BoardRanks)
            continue;
        const int target = board_[make_square(targetFile, targetRank)];
        if (target != NoPiece && pieces_[target].alive &&
            pieces_[target].color != royalColor &&
            pieces_[target].type == PieceType::Ghost)
            pieces_[target].visible = true;
    }
}

void Position::advance_minions(Color color) {
    std::array<int, MaxPieces> minions{};
    int minionCount = 0;
    std::array<bool, MaxPieces> automatic{};
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].color == color &&
            pieces_[id].type == PieceType::Minion) {
            minions[minionCount++] = id;
            // ChangeTurn snapshots the Minions that are eligible after the
            // global cooldown decrement. A later collision does not add a
            // newly thawed/possessed Minion to this automatic pass.
            automatic[id] = !frozen(id) && !pieces_[id].cooldown;
        }

    const int direction = color == Color::White ? 1 : -1;
    std::array<std::uint8_t, MaxPieces> state{};
    auto advance = [&](auto&& self, int id) -> void {
        if (!automatic[id] || state[id] == 2 || !pieces_[id].alive)
            return;
        if (state[id] == 1)
            return;  // Forward-only movement cannot form a cycle.
        state[id] = 1;

        const int from = pieces_[id].square;
        const int targetRank = rank_of(from) + direction;
        if (targetRank < 0 || targetRank >= BoardRanks) {
            // SimulatedUndead::EndOfBoard temporarily replaces its attached
            // Angel list before invoking death. Reaching the far edge is
            // therefore not a lethal hit the Angel can rescue: the Minion
            // disappears while its off-board Angel and Halo remain.
            for (int angel = 0; angel < pieceCount_; ++angel)
                if (pieces_[angel].alive && pieces_[angel].type == PieceType::Angel &&
                    pieces_[angel].host == id)
                    pieces_[angel].host = NoPiece;
            remove_piece(id);
            state[id] = 2;
            return;
        }
        const int to = make_square(file_of(from), targetRank);
        int victim = board_[to];
        // SimulatedUndead::MakeMove clears deadPiece when the destination is
        // an unstunned allied Undead. If it has not taken its snapshotted
        // automatic action yet, advancing it first produces the same train
        // result without relying on HashSet iteration order.
        if (victim != NoPiece && pieces_[victim].alive &&
            pieces_[victim].color == color &&
            pieces_[victim].type == PieceType::Minion && automatic[victim]) {
            self(self, victim);
            victim = board_[to];
        }

        erase_from_board(id);
        if (victim != NoPiece)
            capture_piece(victim, id, {static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to)});
        if (pieces_[id].alive) {
            // Bomb/Goop retaliation may have consumed an attached Angel and
            // already relocated this Minion to its Halo.
            if (pieces_[id].square == from)
                pieces_[id].square = static_cast<std::uint8_t>(to);
            place_on_board(id);
        }
        state[id] = 2;
    };

    for (int index = 0; index < minionCount; ++index)
        advance(advance, minions[index]);
}

void Position::finish_turn() {
    forcedPiece_ = NoPiece;
    continuation_ = Continuation::None;
    sideToMove_ = ~sideToMove_;
    if (sideToMove_ == Color::White)
        ++fullmove_;
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].cooldown)
            --pieces_[id].cooldown;
    advance_minions(sideToMove_);
}

bool Position::make_move(const Move& move, Undo& undo) {
    if (!is_legal(move))
        return false;
    return make_move_unchecked(move, undo);
}

bool Position::make_move_unchecked(const Move& move, Undo& undo) {
    undo = {pieces_, pieceCount_, sideToMove_, enPassantSquare_,
            enPassantVictim_, forcedPiece_, continuation_, forcedTimeoutWinner_, halfmove_, fullmove_,
            nextAttachmentOrder_};

    return apply_move_unchecked(move);
}

bool Position::apply_move_unchecked(const Move& move) {

    const int id = board_[move.from];
    if (id == NoPiece)
        return false;
    PieceState& actor = pieces_[id];
    const int originalFrom = actor.square;
    const int target = board_[move.to];
    const bool checkerJump = actor.type == PieceType::Checker || actor.type == PieceType::CheckerKing;
    const bool checkerCapture = checkerJump && move.auxiliary != 0 &&
                              valid_square(move.auxiliary) &&
                              board_[move.auxiliary] != NoPiece;
    const bool capture = target != NoPiece || move.kind == MoveKind::Shoot
                      || checkerCapture;
    bool createdEnPassant = false;

    // A Penguin has no starting aura. SimulatedFreeze::MakeMove first removes
    // the set created by its preceding move, then freezes the non-Penguins
    // around its new square and serializes those directions in action.
    if (actor.type == PieceType::Penguin)
        clear_penguin_freeze(id);

    if (move.kind == MoveKind::Castle) {
        const int rook = move.auxiliary;
        const int direction = file_of(move.to) > file_of(originalFrom) ? 1 : -1;
        const int rookDestination = make_square(file_of(move.to) - direction,
                                                rank_of(originalFrom));
        if ((actor.type != PieceType::King && actor.type != PieceType::Jester) || actor.moved ||
            rook < 0 || rook >= pieceCount_ || !pieces_[rook].alive ||
            !pieces_[rook].onBoard || pieces_[rook].type != PieceType::Rook ||
            pieces_[rook].moved || rank_of(move.to) != rank_of(originalFrom) ||
            std::abs(file_of(move.to) - file_of(originalFrom)) != 2 ||
            rank_of(pieces_[rook].square) != rank_of(originalFrom) ||
            (file_of(pieces_[rook].square) - file_of(originalFrom)) * direction <= 2 ||
            target != NoPiece)
            return false;
        for (int file = file_of(originalFrom) + direction;
             file != file_of(pieces_[rook].square); file += direction)
            if (board_[make_square(file, rank_of(originalFrom))] != NoPiece)
                return false;

        erase_from_board(id);
        erase_from_board(rook);
        actor.square = move.to;
        actor.moved = true;
        pieces_[rook].square = static_cast<std::uint8_t>(rookDestination);
        pieces_[rook].moved = true;
        place_on_board(rook);
        place_on_board(id);
        reveal_ghosts_near(actor.square, actor.color);
        if (pieces_[rook].color != actor.color) {
            // SimulatedKing advertises an enemy-owned Rook castle and the live
            // Character path visibly relocates both models. It then changes
            // board.turn to the Rook's side without changing Local/online
            // playerTeam, so that opponent can never select a character and
            // deterministically loses when their action clock expires.
            forcedTimeoutWinner_ = static_cast<std::int8_t>(actor.color);
        }
    }
    else if (move.kind == MoveKind::Swap) {
        const int other = move.auxiliary;
        const int otherSquare = pieces_[other].square;
        // SimulatedMage removes the displaced ally from every Penguin freeze
        // set before swapping it. A displaced Penguin drops its old aura and
        // does not create one at the Mage's origin.
        prepare_for_forced_relocation(other);
        if (pieces_[other].type == PieceType::Giant) {
            const int destinationFile = file_of(originalFrom) + file_of(otherSquare) -
                                        file_of(move.to);
            const int destinationRank = rank_of(originalFrom) + rank_of(otherSquare) -
                                        rank_of(move.to);
            const int destination = make_square(destinationFile, destinationRank);
            erase_from_board(id);
            erase_from_board(other);
            actor.square = move.to;
            actor.moved = true;
            // SimulatedMage delegates this branch to Giant MakeMoveTurnSkip,
            // which removes every other character in the translated 2x2
            // footprint, including the Mage itself when the old selected tile
            // overlaps the Giant's translated footprint. The origin was
            // already erased above, so relocation must not clear it a second
            // time after the Mage has been placed there.
            if (actor.alive)
                place_on_board(id);
            relocate_giant(other, destination, true, false);
        }
        else {
            erase_from_board(id);
            erase_from_board(other);
            actor.square = static_cast<std::uint8_t>(otherSquare);
            actor.moved = true;
            pieces_[other].square = static_cast<std::uint8_t>(originalFrom);
            // Native Mage relocation is forced displacement of the target,
            // not that target taking a turn. Preserve its pieceMoved flag.
            // In particular an unmoved Pawn swapped from h9 to a3 can still
            // make the native two-step a3-a1 and promote.
            apply_forced_promotion(other);
            place_on_board(id);
            place_on_board(other);
            // Forced displacement preserves Ghost visibility and does not run
            // a displaced royal's normal-move reveal handler. Native Mage
            // invokes only the target's promotion hook before ChangeTurn.
        }
    }
    else if (move.kind == MoveKind::Spawn) {
        if (target != NoPiece) {
            // SimulatedDevil exposes an invisible enemy Ghost's square as a
            // blind spawn endpoint. MakeMove kills that Ghost but does not
            // construct an Undead on the now-empty target.
            remove_piece(target);
        }
        else {
            add_piece(PieceType::Minion, actor.color, move.to);
        }
        actor.cooldown = info(PieceType::Devil).baseCooldown;
    }
    else if (move.kind == MoveKind::Shoot) {
        capture_piece(move.auxiliary, id, move);
        if (actor.alive)
            actor.cooldown = info(PieceType::Sniper).baseCooldown;
    }
    else if (move.kind == MoveKind::Pull) {
        const int victim = board_[move.to];
        if (victim != NoPiece) {
            // Fisherman uses the same native penguinsForMage bookkeeping for
            // the character it forcibly displaces.
            prepare_for_forced_relocation(victim);
            erase_from_board(victim);
            if (pieces_[victim].alive) {
                int destination = move.auxiliary;
                bool collisionDispatched = false;
                if (pieces_[victim].type == PieceType::Giant) {
                    const int deltaFile = file_of(move.auxiliary) - file_of(move.to);
                    const int deltaRank = rank_of(move.auxiliary) - rank_of(move.to);
                    destination = make_square(file_of(pieces_[victim].square) + deltaFile,
                                              rank_of(pieces_[victim].square) + deltaRank);
                    // Fisherman delegates a hooked Giant to native
                    // MakeMoveTurnSkip. Forced displacement differs from an
                    // ordinary Giant move: every other character in all four
                    // destination cells is knocked out, including allies.
                    relocate_giant(victim, destination);
                }
                else {
                    const int occupant = board_[move.auxiliary];
                    if (occupant != NoPiece) {
                        collisionDispatched = true;
                        // Fisherman.MakeMove assigns the dragged character's
                        // targetSquare to the landing cell, then invokes
                        // SimulateDeath on both it and an invisible occupant.
                        // These are two independent death callbacks rather
                        // than a normal capture by the dragged character.
                        // This is observable when, for example, a King is
                        // pulled into its own hidden Ghost: both are knocked
                        // out, with an attached Angel still able to intercept
                        // either individual death.
                        pieces_[victim].square =
                          static_cast<std::uint8_t>(destination);
                        capture_piece(victim, id, move);
                        capture_piece(occupant, id, move);
                    }
                }
                if (pieces_[victim].alive && pieces_[victim].type != PieceType::Giant) {
                    // Surviving this collision means an Angel already moved
                    // the dragged character to its Halo and installed it on
                    // the board. Do not overwrite that rescue with the
                    // nominal landing cell (or leave the same id occupying
                    // both squares). An ordinary unobstructed pull still
                    // completes its forced relocation here.
                    if (!collisionDispatched) {
                        pieces_[victim].square =
                          static_cast<std::uint8_t>(destination);
                        apply_forced_promotion(victim);
                        place_on_board(victim);
                    }
                }
            }
        }
    }
    else if (move.kind == MoveKind::Link) {
        const int host = move.auxiliary;
        std::vector<int> inherited;
        for (int candidate = 0; candidate < pieceCount_; ++candidate)
            if (candidate != id && pieces_[candidate].alive &&
                pieces_[candidate].type == PieceType::Angel && pieces_[candidate].host == id)
                inherited.push_back(candidate);
        std::sort(inherited.begin(), inherited.end(), [this](int lhs, int rhs) {
            return pieces_[lhs].attachmentOrder < pieces_[rhs].attachmentOrder;
        });

        erase_from_board(id);
        actor.onBoard = false;
        const int halo = add_piece(PieceType::Halo, actor.color, originalFrom);
        actor.link = static_cast<std::int8_t>(halo);
        actor.host = static_cast<std::int8_t>(host);
        actor.attachmentOrder = nextAttachmentOrder_++;
        if (halo != NoPiece)
            pieces_[halo].link = static_cast<std::int8_t>(id);
        actor.square = pieces_[host].square;
        // Native MakeMove reparents Angels previously attached to this Angel
        // after the moving Angel in the destination host's attachment list.
        for (const int dependent : inherited) {
            pieces_[dependent].host = static_cast<std::int8_t>(host);
            pieces_[dependent].attachmentOrder = nextAttachmentOrder_++;
            pieces_[dependent].square = pieces_[host].square;
        }
    }
    else if ((actor.type == PieceType::Copycat || actor.type == PieceType::CopycatClone) &&
             actor.link != NoPiece && valid_square(move.auxiliary) &&
             pieces_[actor.link].alive && pieces_[actor.link].onBoard) {
        const int partner = actor.link;
        const int partnerFrom = pieces_[partner].square;
        const int partnerTarget = board_[move.auxiliary];
        erase_from_board(id);
        erase_from_board(partner);
        if (target != NoPiece)
            capture_piece(target, id, move);
        // Native CopyCat.MakeMove dispatches the queued clone submove when the
        // partner is available. If the first half dies to Goop/Bomb, linked
        // death marks both halves dying, but the queued target still resolves.
        if (partnerTarget != NoPiece)
            capture_piece(partnerTarget, partner,
                          {pieces_[partner].square, move.auxiliary});
        if (pieces_[id].alive) {
            pieces_[id].moved = true;
            if (pieces_[id].square == originalFrom) {
                pieces_[id].square = move.to;
                place_on_board(id);
            }
        }
        if (pieces_[partner].alive) {
            pieces_[partner].moved = true;
            if (pieces_[partner].square == partnerFrom) {
                pieces_[partner].square = move.auxiliary;
                place_on_board(partner);
            }
        }
    }
    else {
        erase_from_board(id);
        int victim = target;
        const bool blindGhostCollision =
          (actor.type == PieceType::Pawn || actor.type == PieceType::Sludge ||
           actor.type == PieceType::Sniper || actor.type == PieceType::Penguin ||
           actor.type == PieceType::Fisherman || actor.type == PieceType::Checker ||
           actor.type == PieceType::CheckerKing) &&
                                       victim != NoPiece &&
                                       pieces_[victim].type == PieceType::Ghost &&
                                       !pieces_[victim].visible &&
                                       pieces_[victim].color != actor.color &&
                                       (actor.type == PieceType::Sludge ||
                                        actor.type == PieceType::Sniper ||
                                        actor.type == PieceType::Penguin ||
                                        actor.type == PieceType::Fisherman ||
                                        actor.type == PieceType::Checker ||
                                        actor.type == PieceType::CheckerKing ||
                                        file_of(move.to) == file_of(originalFrom));
        const bool enPassantGhostCollision =
          actor.type == PieceType::Pawn && move.to == enPassantSquare_ &&
          target != NoPiece && pieces_[target].type == PieceType::Ghost &&
          !pieces_[target].visible;
        if (actor.type == PieceType::Pawn && move.to == enPassantSquare_ &&
            !enPassantGhostCollision)
            victim = enPassantVictim_;
        if (checkerCapture)
            victim = board_[move.auxiliary];
        if (actor.type == PieceType::Giant) {
            const Bitboard destination = footprint(id, move.to);
            // Giant resolves its four destination cells in sequence. An
            // attached Angel can rescue a struck enemy onto a later cell in
            // that same footprint, where native Giant resolution strikes it
            // again. A single victim snapshot left the rescued model and the
            // Giant occupying one square, producing a non-round-trippable UPN.
            // Each repeated hit consumes an Angel or removes a character, so
            // MaxPieces is a conservative bound for malformed attachment
            // chains supplied through analysis UPN.
            for (int pass = 0; pass < MaxPieces && pieces_[id].alive; ++pass) {
                const auto victims = victims_on(destination, id);
                bool attacked = false;
                for (const int occupant : victims)
                    if (pieces_[occupant].alive &&
                        pieces_[occupant].color != actor.color) {
                        capture_piece(occupant, id, move);
                        attacked = true;
                    }
                if (!attacked)
                    break;
            }
        }
        else if (checkerCapture) {
            // Checker resolves the jumped character first. A hidden enemy
            // Ghost on the landing cell is a queued blind-collision sub-move;
            // it runs only if that first capture did not already kill the
            // Checker (for example by detonating a Bomb).
            capture_piece(victim, id, move);
            if (pieces_[id].alive && blindGhostCollision) {
                remove_piece(target);
                remove_piece(id);
            }
        }
        else if (blindGhostCollision || enPassantGhostCollision) {
            // Pawn.MakeMove resolves a hidden Ghost already occupying the
            // en-passant destination as a blind mutual knockout—even when it
            // is allied to the capturing Pawn. In that branch the bypassing
            // Pawn stored as the nominal en-passant victim is not removed.
            remove_piece(victim);
            remove_piece(id);
        }
        else if (victim != NoPiece)
            capture_piece(victim, id, move);
        // Generic native movers install themselves on targetSquare before
        // dispatching the victim's SimulateDeath callback. If an attached
        // Angel rescues a Giant so that its new 2x2 footprint covers that
        // target, Giant MakeMoveTurnSkip therefore strikes the attacker. Our
        // compact model resolves deaths before final placement, so replay the
        // equivalent collision explicitly instead of later overwriting the
        // rescued Giant and emitting overlapping UPN state.
        if (actor.alive && actor.type != PieceType::Giant &&
            valid_square(move.to)) {
            const int occupyingTarget = board_[move.to];
            if (occupyingTarget != NoPiece && occupyingTarget != id &&
                pieces_[occupyingTarget].alive)
                capture_piece(id, occupyingTarget,
                              {pieces_[occupyingTarget].square, move.to});
        }
        if (actor.alive) {
            // Most native characters occupy the destination before resolving
            // its death effect. If that effect kills an Angel-protected
            // attacker (notably Bomb or Goop), AngelFunction has already
            // relocated it to the Halo. Preserve that rescue instead of
            // overwriting it with the nominal move destination.
            const bool relocatedByEffect = actor.square != originalFrom;
            if (!relocatedByEffect)
                actor.square = move.to;
            actor.moved = true;
            if (!relocatedByEffect && actor.type == PieceType::Pawn) {
                const int promotionRank = actor.color == Color::White ? BoardRanks - 1 : 0;
                if (rank_of(actor.square) == promotionRank)
                    actor.type = move.promotion == PieceType::Count ? PieceType::Queen : move.promotion;
                if (std::abs(int(move.to) - originalFrom) == 16) {
                    enPassantSquare_ = (originalFrom + move.to) / 2;
                    enPassantVictim_ = id;
                    createdEnPassant = true;
                }
            }
            if (!relocatedByEffect && actor.type == PieceType::Checker) {
                const int promotionRank = actor.color == Color::White ? BoardRanks - 1 : 0;
                if (rank_of(actor.square) == promotionRank)
                    actor.type = PieceType::CheckerKing;
            }
            place_on_board(id);
            if (actor.type == PieceType::Ghost)
                actor.visible = actor.parasiteTracked || capture ||
                                ghost_near_enemy_royal(actor.square, actor.color);
            else if (actor.type == PieceType::King || actor.type == PieceType::Jester)
                reveal_ghosts_near(actor.square, actor.color);
            else if (actor.type == PieceType::Penguin)
                apply_penguin_freeze(id);
        }
        if (actor.type == PieceType::Sludge && board_[originalFrom] == NoPiece) {
            add_piece(PieceType::Goop, actor.color, originalFrom);
            if (std::abs(file_of(move.to) - file_of(originalFrom)) > 1 ||
                std::abs(rank_of(move.to) - rank_of(originalFrom)) > 1) {
                const int middle = (originalFrom + move.to) / 2;
                const int trailVictim = board_[middle];
                if (trailVictim == NoPiece) {
                    add_piece(PieceType::Goop, actor.color, middle);
                }
                else {
                    // A hidden Ghost does not block the Sludge ray. The
                    // SimulatedGoop constructor's ExplodeOnStart branch kills
                    // that occupant but returns before installing the new
                    // Goop, so the Sludge continues and the middle cell stays
                    // empty (or the Ghost is Angel-rescued elsewhere).
                    remove_piece(trailVictim);
                }
            }
        }
    }

    ++halfmove_;
    // Every branch above maintains board_, occupancy_, and byType_
    // incrementally through erase/place/add/remove. Only off-board Angels
    // need their serialized coordinate matched to a host after it moves.
    for (int angel = 0; angel < pieceCount_; ++angel)
        if (pieces_[angel].alive && pieces_[angel].type == PieceType::Angel &&
            !pieces_[angel].onBoard && pieces_[angel].host != NoPiece &&
            pieces_[angel].host < pieceCount_ && pieces_[pieces_[angel].host].alive)
            pieces_[angel].square = pieces_[pieces_[angel].host].square;

#ifndef NDEBUG
    // The reference/test build keeps a differential invariant oracle. Release
    // search avoids this full reconstruction entirely.
    Position rebuilt = *this;
    rebuilt.rebuild_bitboards();
    assert(board_ == rebuilt.board_ && byType_ == rebuilt.byType_ &&
           occupancy_ == rebuilt.occupancy_);
    for (int id = 0; id < pieceCount_; ++id)
        assert(pieces_[id].square == rebuilt.pieces_[id].square);
#endif

    if (id < pieceCount_ && pieces_[id].alive &&
        (pieces_[id].type == PieceType::Checker || pieces_[id].type == PieceType::CheckerKing) &&
        capture && checker_has_capture(id)) {
        forcedPiece_ = id;
        continuation_ = Continuation::CheckerJump;
        return true;
    }
    if (id < pieceCount_ && pieces_[id].alive && pieces_[id].type == PieceType::Prince && !capture &&
        continuation_ == Continuation::None) {
        forcedPiece_ = id;
        continuation_ = Continuation::PrinceSecondMove;
        return true;
    }
    if (!createdEnPassant) {
        enPassantSquare_ = NoSquare;
        enPassantVictim_ = NoPiece;
    }
    finish_turn();
    return true;
}

std::uint64_t Position::perft(int depth) const {
    if (depth <= 0)
        return 1;
    std::uint64_t nodes = 0;
    for (const Move& move : legal_moves()) {
        Position child = *this;
        // The perft frontier is the position's own legal list; revalidating
        // each member would regenerate that complete list once per child.
        if (!child.apply_move_unchecked(move))
            continue;
        nodes += child.perft(depth - 1);
    }
    return nodes;
}

void Position::undo_move(const Undo& undo) {
    pieces_ = undo.pieces;
    pieceCount_ = undo.pieceCount;
    sideToMove_ = undo.sideToMove;
    enPassantSquare_ = undo.enPassantSquare;
    enPassantVictim_ = undo.enPassantVictim;
    forcedPiece_ = undo.forcedPiece;
    continuation_ = undo.continuation;
    forcedTimeoutWinner_ = undo.forcedTimeoutWinner;
    halfmove_ = undo.halfmove;
    fullmove_ = undo.fullmove;
    nextAttachmentOrder_ = undo.nextAttachmentOrder;
    rebuild_bitboards();
}

bool Position::has_real_king(Color color) const {
    return byType_[index(color)][index(PieceType::King)] != 0;
}

bool Position::team_has_sufficient_material(Color color) const {
    const auto& types = byType_[index(color)];
    const Bitboard decisive =
      types[index(PieceType::Jester)] |
      types[index(PieceType::Pawn)] |
      types[index(PieceType::Queen)] |
      types[index(PieceType::Rook)] |
      types[index(PieceType::Berserker)] |
      types[index(PieceType::Bomb)] |
      types[index(PieceType::Ninja)] |
      types[index(PieceType::Ghost)] |
      types[index(PieceType::Penguin)] |
      types[index(PieceType::Parasite)] |
      types[index(PieceType::Devil)] |
      // Spawned Minions continue their automatic advance after their Devil
      // is captured and can still capture the opposing King.  Treating that
      // position as immediate insufficient material truncated the exact
      // spawned-only Devil closure at precisely the states the sparse solver
      // must retain.
      types[index(PieceType::Minion)] |
      types[index(PieceType::Sniper)] |
      types[index(PieceType::Prince)] |
      types[index(PieceType::Giant)] |
      types[index(PieceType::Copycat)] |
      types[index(PieceType::Dragon)];
    if (decisive)
        return true;

    const int minorCount = popcount(
      types[index(PieceType::Knight)] | types[index(PieceType::Turtle)]);
    bool evenColorBound = false;
    bool oddColorBound = false;
    Bitboard colorBound = types[index(PieceType::Bishop)] |
                          types[index(PieceType::Checker)] |
                          types[index(PieceType::CheckerKing)];
    while (colorBound) {
        const int square = pop_lsb(colorBound);
        if ((file_of(square) + rank_of(square)) & 1)
            oddColorBound = true;
        else
            evenColorBound = true;
    }
    const bool support = types[index(PieceType::Mage)] |
                         types[index(PieceType::Fisherman)];
    if (minorCount > 1 || (evenColorBound && oddColorBound))
        return true;
    if (support && (evenColorBound || oddColorBound))
        return true;
    return minorCount == 1 && (evenColorBound || oddColorBound);
}

bool Position::is_checkmate_possible() const {
    return team_has_sufficient_material(Color::White) ||
           team_has_sufficient_material(Color::Black);
}

TerminalReason Position::terminal_reason() const {
    if (forcedTimeoutWinner_ >= 0)
        return TerminalReason::ForcedTimeout;
    const bool white = has_real_king(Color::White);
    const bool black = has_real_king(Color::Black);
    if (!white && !black)
        return TerminalReason::SimultaneousKingCapture;
    if (white != black)
        return TerminalReason::KingCaptured;
    if (!is_checkmate_possible())
        return TerminalReason::InsufficientMaterial;
    if (has_legal_move())
        return TerminalReason::Ongoing;
    return in_check()
      ? TerminalReason::Checkmate : TerminalReason::Stalemate;
}

bool Position::game_over() const {
    return terminal_reason() != TerminalReason::Ongoing;
}

std::optional<Color> Position::winner() const {
    if (forcedTimeoutWinner_ >= 0)
        return static_cast<Color>(forcedTimeoutWinner_);
    const bool white = has_real_king(Color::White);
    const bool black = has_real_king(Color::Black);
    if (white != black)
        return white ? Color::White : Color::Black;
    if (!white || !is_checkmate_possible())
        return std::nullopt;
    if (!has_legal_move() && in_check())
        return ~sideToMove_;
    return std::nullopt;
}

std::uint64_t Position::key() const {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 0x100000001b3ULL;
    };
    const bool canRetainCastling =
      (byType_[index(Color::White)][index(PieceType::Rook)] |
       byType_[index(Color::Black)][index(PieceType::Rook)]) &&
      (byType_[index(Color::White)][index(PieceType::King)] |
       byType_[index(Color::White)][index(PieceType::Jester)] |
       byType_[index(Color::Black)][index(PieceType::King)] |
       byType_[index(Color::Black)][index(PieceType::Jester)]);
    bool hasUnmovedRoyal = false;
    bool hasUnmovedRook = false;
    std::uint64_t unmovedCastleLow = 0;
    std::uint64_t unmovedCastleHigh = 0;
    mix(static_cast<std::uint8_t>(sideToMove_));
    mix(static_cast<std::uint8_t>(continuation_));
    mix(static_cast<std::uint64_t>(forcedTimeoutWinner_ + 1));
    mix(static_cast<std::uint64_t>(forcedPiece_ + 1));
    mix(static_cast<std::uint64_t>(enPassantSquare_ + 1));
    mix(static_cast<std::uint64_t>(enPassantVictim_ + 1));
    mix(nextAttachmentOrder_);
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& piece = pieces_[id];
        if (!piece.alive)
            continue;
        mix(static_cast<std::uint8_t>(piece.type));
        mix(static_cast<std::uint8_t>(piece.color));
        mix(piece.square | (std::uint64_t(piece.onBoard) << 8));
        mix(piece.action | (std::uint64_t(piece.cooldown) << 8) |
            (std::uint64_t(piece.freezeCount) << 16) | (std::uint64_t(piece.power) << 24));
        if (canRetainCastling && piece.onBoard && !piece.moved) {
            const bool royal = piece.type == PieceType::King ||
                               piece.type == PieceType::Jester;
            const bool rook = piece.type == PieceType::Rook;
            if (royal || rook) {
                hasUnmovedRoyal = hasUnmovedRoyal || royal;
                hasUnmovedRook = hasUnmovedRook || rook;
                if (id < 64)
                    unmovedCastleLow |= std::uint64_t{1} << id;
                else
                    unmovedCastleHigh |= std::uint64_t{1} << (id - 64);
            }
        }
        // Only a Pawn double-step or a surviving King/Jester+Rook castling
        // right makes the monotone moved bit alter future legal actions. Hash
        // those effective rights, rather than inert serialization history, so
        // TT reuse and threefold detection compare legal positions.
        mix(std::uint64_t(piece.type == PieceType::Pawn && piece.moved) |
            (std::uint64_t(piece.visible) << 1) |
            (std::uint64_t(piece.parasiteTracked) << 2) |
            (std::uint64_t(piece.link + 1) << 8) |
            (std::uint64_t(piece.host + 1) << 16) |
            (std::uint64_t(piece.attachmentOrder) << 24));
    }
    if (hasUnmovedRoyal && hasUnmovedRook) {
        mix(unmovedCastleLow);
        mix(unmovedCastleHigh);
    }
    return hash;
}

int Position::static_evaluate() const {
    const int handcrafted = handcrafted_evaluate();
    if (const auto correction = UltimateNnue::evaluate(*this))
        return handcrafted + *correction;
    return handcrafted;
}

int Position::handcrafted_evaluate() const {
    int score = 0;
    int kingSquare[2] = {NoSquare, NoSquare};
    for (Color color : {Color::White, Color::Black}) {
        const Bitboard king = byType_[index(color)][index(PieceType::King)];
        if (king)
            kingSquare[index(color)] = lsb_square(king);
    }
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& piece = pieces_[id];
        if (!piece.alive)
            continue;
        int value = MaterialValue[index(piece.type)];
        if (piece.type == PieceType::Berserker)
            value *= int(piece.power) + 1;
        if (piece.cooldown)
            value -= value / 4;
        if (piece.freezeCount)
            value -= value / 3;
        const int advancement = piece.color == Color::White ? rank_of(piece.square)
                                                            : BoardRanks - 1 - rank_of(piece.square);
        if (piece.type == PieceType::Pawn || piece.type == PieceType::Checker || piece.type == PieceType::Minion)
            value += advancement * 8;
        if (piece.onBoard) {
            const int fileCentrality = std::min(file_of(piece.square), BoardFiles - 1 - file_of(piece.square));
            const int rankCentrality = std::min(rank_of(piece.square), BoardRanks - 1 - rank_of(piece.square));
            const bool developing = piece.type == PieceType::Knight || piece.type == PieceType::Bishop ||
                                    piece.type == PieceType::Ninja || piece.type == PieceType::Dragon;
            value += fileCentrality * (developing ? 5 : 2) + rankCentrality * (developing ? 3 : 1);

            const int enemyKing = kingSquare[index(~piece.color)];
            if (enemyKing != NoSquare) {
                const int distance = std::abs(file_of(piece.square) - file_of(enemyKing)) +
                                     std::abs(rank_of(piece.square) - rank_of(enemyKing));
                value += TropismWeight[index(piece.type)] *
                         std::max(0, BoardFiles + BoardRanks - 1 - distance);
            }
        }
        if (piece.type == PieceType::Ghost && !piece.visible)
            value += 35;
        if (piece.type == PieceType::King && piece.onBoard) {
            for (const auto& direction : Around) {
                const int file = file_of(piece.square) + direction[0];
                const int rank = rank_of(piece.square) + direction[1];
                if (file < 0 || file >= BoardFiles || rank < 0 || rank >= BoardRanks)
                    continue;
                const int neighbor = board_[make_square(file, rank)];
                if (neighbor != NoPiece && pieces_[neighbor].color == piece.color)
                    value += 8;
            }
        }
        score += piece.color == Color::White ? value : -value;
    }
    return sideToMove_ == Color::White ? score : -score;
}

int Position::evaluate() const {
    int score = static_evaluate();
    if (game_over()) {
        const auto winning = winner();
        if (winning)
            score = *winning == sideToMove_ ? 30000 : -30000;
    }
    return score;
}

std::string Position::upn() const {
    // Dead pieces are deliberately omitted. Remap all relationship IDs to the
    // compact live-piece order so a position remains lossless after captures
    // and can be handed through the stateless UI bridge repeatedly.
    std::array<int, MaxPieces> liveId;
    liveId.fill(NoPiece);
    int nextId = 0;
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive)
            liveId[id] = nextId++;
    const auto remap = [&liveId, this](int id) {
        return id >= 0 && id < pieceCount_ ? liveId[id] : NoPiece;
    };

    std::ostringstream out;
    out << (sideToMove_ == Color::White ? 'w' : 'b') << ";hm=" << halfmove_ << ";fm=" << fullmove_
        << ";ep=" << square_name(enPassantSquare_) << ";cont=" << static_cast<int>(continuation_)
        << ";forced=" << remap(forcedPiece_) << ";epv=" << remap(enPassantVictim_)
        << ";win="
        << (forcedTimeoutWinner_ < 0 ? '-' :
            forcedTimeoutWinner_ == static_cast<std::int8_t>(Color::White) ? 'w' : 'b');
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& item = pieces_[id];
        if (!item.alive)
            continue;
        out << ';' << type_name(item.type) << ',' << (item.color == Color::White ? 'w' : 'b') << ','
            << square_name(item.square) << ',' << int(item.action) << ',' << int(item.cooldown) << ','
            << int(item.freezeCount) << ',' << int(item.power) << ',' << int(item.moved) << ','
            << int(item.visible) << ',' << remap(item.link) << ',' << int(item.onBoard) << ','
            << remap(item.host) << ',' << item.attachmentOrder;
        // Preserve the established canonical UPN for ordinary pieces. The
        // optional trailing marker appears only for the new tracked state, so
        // old tools remain byte-for-byte compatible while new parsers retain
        // the exception losslessly.
        if (item.parasiteTracked)
            out << ",1";
    }
    return out.str();
}

bool Position::set_upn(std::string_view text, std::string* error) {
    Position parsed;
    parsed.clear();
    std::array<bool, MaxPieces> implicitCopycatCompanion{};
    const auto fail = [error](std::string_view message) {
        if (error)
            *error = std::string(message);
        return false;
    };
    const auto fields = split(text, ';');
    if (fields.empty() || (fields[0] != "w" && fields[0] != "b")) {
        if (error)
            *error = "UPN must begin with 'w' or 'b'";
        return false;
    }
    parsed.sideToMove_ = fields[0] == "w" ? Color::White : Color::Black;
    for (std::size_t field = 1; field < fields.size(); ++field) {
        const auto item = fields[field];
        if (starts_with(item, "hm=")) {
            int value;
            if (!parse_int(item.substr(3), value) || value < 0)
                return false;
            parsed.halfmove_ = value;
            continue;
        }
        if (starts_with(item, "fm=")) {
            int value;
            if (!parse_int(item.substr(3), value) || value < 1)
                return false;
            parsed.fullmove_ = value;
            continue;
        }
        if (starts_with(item, "ep=")) {
            const std::string_view squareText = item.substr(3);
            parsed.enPassantSquare_ = squareText == "-" ? NoSquare : square_from_name(squareText);
            if (squareText != "-" && parsed.enPassantSquare_ == NoSquare)
                return fail("invalid en-passant square");
            continue;
        }
        if (starts_with(item, "cont=")) {
            int value;
            if (!parse_int(item.substr(5), value) || value < 0 || value > 2)
                return false;
            parsed.continuation_ = static_cast<Continuation>(value);
            continue;
        }
        if (starts_with(item, "forced=")) {
            if (!parse_int(item.substr(7), parsed.forcedPiece_) || parsed.forcedPiece_ < NoPiece)
                return fail("invalid forced piece id");
            continue;
        }
        if (starts_with(item, "epv=")) {
            if (!parse_int(item.substr(4), parsed.enPassantVictim_) ||
                parsed.enPassantVictim_ < NoPiece)
                return fail("invalid en-passant victim id");
            continue;
        }
        if (starts_with(item, "win=")) {
            const std::string_view value = item.substr(4);
            if (value == "-")
                parsed.forcedTimeoutWinner_ = -1;
            else if (value == "w")
                parsed.forcedTimeoutWinner_ = static_cast<std::int8_t>(Color::White);
            else if (value == "b")
                parsed.forcedTimeoutWinner_ = static_cast<std::int8_t>(Color::Black);
            else
                return fail("invalid forced-timeout winner");
            continue;
        }
        const auto values = split(item, ',');
        if (values.size() < 3) {
            if (error)
                *error = "piece entry requires type,color,square";
            return false;
        }
        const auto type = type_from_name(values[0]);
        const int square = square_from_name(values[2]);
        if (!type || (values[1] != "w" && values[1] != "b") || square == NoSquare) {
            if (error)
                *error = "invalid piece entry";
            return false;
        }
        int numbers[11] = {0, 0, 0, 0, 0, 1, -1, 1, -1, 0, 0};
        for (std::size_t i = 3; i < values.size() && i < 14; ++i)
            if (!parse_int(values[i], numbers[i - 3]))
                return fail("invalid numeric piece state");
        if (numbers[0] < 0 || numbers[0] > 255 || numbers[1] < 0 || numbers[1] > 255 ||
            numbers[2] < 0 || numbers[2] > 255 || numbers[3] < 0 || numbers[3] > 255 ||
            (numbers[4] != 0 && numbers[4] != 1) ||
            (numbers[5] != 0 && numbers[5] != 1) ||
            numbers[6] < NoPiece || numbers[6] >= MaxPieces ||
            (numbers[7] != 0 && numbers[7] != 1) ||
            numbers[8] < NoPiece || numbers[8] >= MaxPieces ||
            numbers[9] < 0 || numbers[9] > 65535 ||
            (numbers[10] != 0 && numbers[10] != 1))
            return fail("piece state field is out of range");
        if (numbers[7] == 0 && *type != PieceType::Angel)
            return fail("only an attached angel may be off board");
        const int id = parsed.add_piece_internal(*type,
                                                  values[1] == "w" ? Color::White : Color::Black,
                                                  square, false, numbers[7] != 0);
        if (id == NoPiece) {
            if (error)
                *error = "overlapping or invalid piece footprint";
            return false;
        }
        // Compact setup notation names only the deployable CopyCat and relies
        // on construction to create its mirror. Explicit arbitrary/legacy UPN
        // may still carry link=-1; preserve that supplied malformed state
        // rather than silently inventing a companion. Native play itself does
        // not create a singleton CopyCat.
        implicitCopycatCompanion[id] =
          *type == PieceType::Copycat && values.size() < 10;
        PieceState& piece = parsed.pieces_[id];
        piece.action = numbers[0];
        piece.cooldown = numbers[1];
        piece.freezeCount = numbers[2];
        piece.power = numbers[3];
        piece.moved = numbers[4] != 0;
        piece.visible = numbers[5] != 0;
        piece.link = static_cast<std::int8_t>(numbers[6]);
        piece.onBoard = numbers[7] != 0;
        piece.host = static_cast<std::int8_t>(numbers[8]);
        piece.attachmentOrder = static_cast<std::uint16_t>(std::max(0, numbers[9]));
        piece.parasiteTracked = numbers[10] != 0;
        if (piece.parasiteTracked &&
            (piece.type != PieceType::Ghost || !piece.visible))
            return fail("only a visible Ghost may be Parasite-tracked");
    }

    // Older/custom UPN strings may name only the deployable CopyCat. Native
    // construction creates a clone automatically; explicit lossless strings
    // name both members and are paired here without duplicating them.
    for (int id = 0; id < parsed.pieceCount_; ++id) {
        PieceState& copycat = parsed.pieces_[id];
        if (!copycat.alive || copycat.type != PieceType::Copycat ||
            copycat.link != NoPiece || !implicitCopycatCompanion[id])
            continue;
        const int mirror = make_square(7 - file_of(copycat.square), rank_of(copycat.square));
        int clone = NoPiece;
        for (int candidate = 0; candidate < parsed.pieceCount_; ++candidate)
            if (parsed.pieces_[candidate].alive &&
                parsed.pieces_[candidate].type == PieceType::CopycatClone &&
                parsed.pieces_[candidate].color == copycat.color &&
                parsed.pieces_[candidate].square == mirror &&
                parsed.pieces_[candidate].link == NoPiece) {
                clone = candidate;
                break;
            }
        if (clone == NoPiece) {
            if (parsed.board_[mirror] != NoPiece) {
                if (error)
                    *error = "copycat mirror square is occupied";
                return false;
            }
            clone = parsed.add_piece_internal(PieceType::CopycatClone, copycat.color, mirror, false);
            if (clone == NoPiece) {
                if (error)
                    *error = "could not create copycat mirror clone";
                return false;
            }
        }
        parsed.pieces_[id].link = static_cast<std::int8_t>(clone);
        parsed.pieces_[clone].link = static_cast<std::int8_t>(id);
    }

    // Migrate the original prototype's host->Angel encoding while accepting
    // newer lossless strings that carry Angel.host explicitly.
    for (int angel = 0; angel < parsed.pieceCount_; ++angel) {
        PieceState& piece = parsed.pieces_[angel];
        if (!piece.alive || piece.type != PieceType::Angel || piece.onBoard)
            continue;
        if (piece.host == NoPiece) {
            for (int candidate = 0; candidate < parsed.pieceCount_; ++candidate) {
                PieceState& possibleHost = parsed.pieces_[candidate];
                if (candidate != angel && possibleHost.alive &&
                    possibleHost.type != PieceType::Halo && possibleHost.link == angel) {
                    piece.host = static_cast<std::int8_t>(candidate);
                    possibleHost.link = NoPiece;
                    break;
                }
            }
        }
        if (piece.host != NoPiece && piece.attachmentOrder == 0)
            piece.attachmentOrder = parsed.nextAttachmentOrder_++;
        else
            parsed.nextAttachmentOrder_ = std::max<std::uint16_t>(
              parsed.nextAttachmentOrder_, piece.attachmentOrder + 1);
    }
    if (parsed.forcedPiece_ >= parsed.pieceCount_) {
        if (error)
            *error = "forced piece id is out of range";
        return false;
    }
    if (parsed.enPassantVictim_ >= parsed.pieceCount_) {
        if (error)
            *error = "en-passant victim id is out of range";
        return false;
    }
    for (int id = 0; id < parsed.pieceCount_; ++id) {
        const PieceState& piece = parsed.pieces_[id];
        if ((piece.link != NoPiece && piece.link >= parsed.pieceCount_) ||
            (piece.host != NoPiece && piece.host >= parsed.pieceCount_))
            return fail("piece relationship id is out of range");
    }
    // Backward compatibility for UPN produced before epv was introduced.
    if (parsed.enPassantSquare_ != NoSquare && parsed.enPassantVictim_ == NoPiece) {
        const int delta = parsed.sideToMove_ == Color::Black ? BoardFiles : -BoardFiles;
        const int victimSquare = parsed.enPassantSquare_ + delta;
        if (valid_square(victimSquare)) {
            const int victim = parsed.board_[victimSquare];
            if (victim != NoPiece && parsed.pieces_[victim].type == PieceType::Pawn &&
                parsed.pieces_[victim].color != parsed.sideToMove_)
                parsed.enPassantVictim_ = victim;
        }
    }
    parsed.rebuild_bitboards();
    *this = parsed;
    return true;
}

std::string Position::move_to_string(const Move& move) const {
    char separator = '-';
    switch (move.kind) {
    case MoveKind::Castle: break;
    case MoveKind::Swap: separator = '~'; break;
    case MoveKind::Spawn: separator = '@'; break;
    case MoveKind::Shoot: separator = 'x'; break;
    case MoveKind::Pull: separator = '!'; break;
    case MoveKind::Link: separator = '&'; break;
    case MoveKind::Pass: return "pass";
    case MoveKind::Normal: break;
    }
    return square_name(move.from) + separator + square_name(move.to);
}

std::string Position::move_to_display_string(const Move& move,
                                             bool concealJester) const {
    if (move.kind == MoveKind::Pass)
        return "pass";
    if (!valid_square(move.from) || !valid_square(move.to))
        return move_to_string(move);
    const int actorId = board_[move.from];
    if (actorId == NoPiece)
        return move_to_string(move);
    const PieceState& actor = pieces_[actorId];

    std::string notation;
    if (move.kind == MoveKind::Castle) {
        int castles = 0;
        for (const Move& candidate : legal_moves())
            if (candidate.kind == MoveKind::Castle && candidate.from == move.from)
                ++castles;
        notation = "0-0";
        if (castles > 1 && move.auxiliary < pieceCount_)
            notation += square_name(pieces_[move.auxiliary].square);
    }
    else {
        notation = display_letter(actor.type, concealJester);
        const bool capture = is_capture(move);
        if (actor.type == PieceType::Pawn && capture)
            notation += static_cast<char>('a' + file_of(move.from));
        else if (actor.type != PieceType::Pawn) {
            std::vector<int> alternatives;
            for (const Move& candidate : legal_moves()) {
                if (candidate.from == move.from || candidate.to != move.to ||
                    !valid_square(candidate.from))
                    continue;
                const int candidateId = board_[candidate.from];
                if (candidateId != NoPiece &&
                    same_display_role(actor.type, pieces_[candidateId].type,
                                      concealJester))
                    alternatives.push_back(candidate.from);
            }
            if (!alternatives.empty()) {
                const bool fileUnique = std::none_of(
                  alternatives.begin(), alternatives.end(), [&](int square) {
                      return file_of(square) == file_of(move.from);
                  });
                const bool rankUnique = std::none_of(
                  alternatives.begin(), alternatives.end(), [&](int square) {
                      return rank_of(square) == rank_of(move.from);
                  });
                if (fileUnique)
                    notation += static_cast<char>('a' + file_of(move.from));
                else if (rankUnique)
                    notation += std::to_string(rank_of(move.from) + 1);
                else
                    notation += square_name(move.from);
            }
        }
        if (capture)
            notation += 'x';
        notation += square_name(move.to);
        if (actor.type == PieceType::Pawn &&
            rank_of(move.to) == (actor.color == Color::White ? BoardRanks - 1 : 0))
            notation += "=Q";
    }

    Position child = *this;
    Undo undo;
    if (child.make_move(move, undo)) {
        if (concealJester && actor.type == PieceType::Ghost && !actor.visible &&
            (!child.piece(actorId).alive || !child.piece(actorId).onBoard ||
             !child.piece(actorId).visible))
            return "GH";
        if (child.winner() == actor.color)
            notation += '#';
        else if (child.side_to_move() != actor.color && child.in_check())
            notation += '+';
    }
    return notation;
}

std::optional<Move> Position::move_from_string(std::string_view text) const {
    if (text == "pass")
        return Move{0, 0, 0, MoveKind::Pass};
    const std::size_t separator = text.find_first_of("-~@x!&", 1);
    if (separator == std::string_view::npos)
        return std::nullopt;
    const int from = square_from_name(text.substr(0, separator));
    const int to = square_from_name(text.substr(separator + 1));
    if (from == NoSquare || to == NoSquare)
        return std::nullopt;
    MoveKind kind;
    switch (text[separator]) {
    case '-': kind = MoveKind::Normal; break;
    case '~': kind = MoveKind::Swap; break;
    case '@': kind = MoveKind::Spawn; break;
    case 'x': kind = MoveKind::Shoot; break;
    case '!': kind = MoveKind::Pull; break;
    case '&': kind = MoveKind::Link; break;
    default: return std::nullopt;
    }
    const auto moves = legal_moves();
    const auto found = std::find_if(moves.begin(), moves.end(), [=](const Move& move) {
        return move.from == from && move.to == to &&
          (move.kind == kind || (kind == MoveKind::Normal && move.kind == MoveKind::Castle));
    });
    return found == moves.end() ? std::nullopt : std::optional<Move>(*found);
}

}  // namespace Stockfish::Ultimate
