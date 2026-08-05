/*
  Ultimate Fish - Chess Ultimate rules engine
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "position.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace Stockfish::Ultimate {
namespace {

constexpr int Orthogonal[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr int Diagonal[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
constexpr int Around[8][2] = {
  {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
constexpr int KnightOffsets[8][2] = {
  {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};

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
constexpr int file_of(int square) { return square & 7; }
constexpr int rank_of(int square) { return square >> 3; }
constexpr int make_square(int file, int rank) { return rank * 8 + file; }
constexpr Bitboard square_bb(int square) { return Bitboard{1} << square; }

int pop_lsb(Bitboard& mask) {
    const std::uint64_t low = static_cast<std::uint64_t>(mask);
    const int square = low ? __builtin_ctzll(low)
                           : 64 + __builtin_ctzll(static_cast<std::uint64_t>(mask >> 64));
    mask &= mask - 1;
    return square;
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

    erase_from_board(host);
    int destination = returnSquare;
    if (pieces_[host].type == PieceType::Giant) {
        // The native Giant branch clamps the halo-derived anchor so its 2x2
        // footprint remains inside the 8x10 board, then delegates to
        // SimulatedGiant::MakeMoveTurnSkip so every collision is resolved.
        destination = make_square(std::clamp(file_of(returnSquare), 0, BoardFiles - 2),
                                  std::clamp(rank_of(returnSquare), 0, BoardRanks - 2));
        relocate_giant(host, destination);
        return;
    }
    pieces_[host].square = static_cast<std::uint8_t>(destination);
    place_on_board(host);
}

bool Position::remove_piece(int id) {
    if (id < 0 || id >= pieceCount_ || !pieces_[id].alive)
        return false;

    const int protector = attached_angel(id);
    if (protector != NoPiece) {
        sacrifice_angel(protector, id);
        return true;
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
        remove_piece(linked);
    if ((type == PieceType::Angel || type == PieceType::Halo) && linked != NoPiece &&
        linked < pieceCount_ && pieces_[linked].alive)
        remove_piece(linked);
    return true;
}

int Position::piece_on(int square) const {
    return valid_square(square) ? board_[square] : NoPiece;
}

const PieceInfo& Position::info(PieceType type) { return PieceTable[index(type)]; }

int Position::material_value(PieceType type) { return MaterialValue[index(type)]; }

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
    // SimulatedPiece::GetPieceSimulations marks an unrevealed Ghost square as
    // unavailable for both leapers and steppers. Slider, Pawn, Sniper, and
    // Fisherman have their own native blind-interaction branches.
    if (pieces_[target].type == PieceType::Ghost && !pieces_[target].visible)
        return false;
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
            // SimulatedPiece marks a square occupied by a hidden Ghost
            // unavailable, but continues scanning the ray through it. The
            // native simulation applies this to either team.
            if (pieces_[target].type == PieceType::Ghost && !pieces_[target].visible)
                continue;
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
    bool foundCapture = false;
    for (const int vertical : {direction, -direction}) {
        if (vertical == -direction && !king)
            continue;
        for (const int horizontal : {-1, 1}) {
            const int middleFile = file_of(piece.square) + horizontal;
            const int middleRank = rank_of(piece.square) + vertical;
            const int targetFile = file_of(piece.square) + 2 * horizontal;
            const int targetRank = rank_of(piece.square) + 2 * vertical;
            if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
                targetRank >= BoardRanks || middleFile < 0 || middleFile >= BoardFiles ||
                middleRank < 0 || middleRank >= BoardRanks)
                continue;
            const int jumped = board_[make_square(middleFile, middleRank)];
            const int target = make_square(targetFile, targetRank);
            if (jumped != NoPiece && pieces_[jumped].color != piece.color && board_[target] == NoPiece) {
                foundCapture = true;
                moves.push_back({piece.square, static_cast<std::uint8_t>(target),
                                 static_cast<std::uint8_t>(make_square(middleFile, middleRank))});
            }
        }
    }
    if (foundCapture || attacksOnly)
        return;
    for (const int vertical : {direction, -direction}) {
        if (vertical == -direction && !king)
            continue;
        for (const int horizontal : {-1, 1}) {
            const int file = file_of(piece.square) + horizontal;
            const int rank = rank_of(piece.square) + vertical;
            if (file >= 0 && file < BoardFiles && rank >= 0 && rank < BoardRanks &&
                board_[make_square(file, rank)] == NoPiece)
                moves.push_back({piece.square, static_cast<std::uint8_t>(make_square(file, rank))});
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
            // Fisherman stays put. An invisible enemy Ghost is instead a
            // normal capturable endpoint.
            if (!pieces_[target].visible) {
                if (pieces_[target].color != fisherman.color)
                    moves.push_back({fisherman.square, static_cast<std::uint8_t>(square)});
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

std::vector<Move> Position::moves_for(int id, bool attacksOnly) const {
    std::vector<Move> moves;
    if (id < 0 || id >= pieceCount_ || !pieces_[id].alive || frozen(id) || pieces_[id].cooldown)
        return moves;

    const PieceState& piece = pieces_[id];
    switch (piece.type) {
    case PieceType::King:
    case PieceType::Jester: {
        add_step_moves(moves, id, Around, 8, 1, false, attacksOnly);
        // The shipping game constructs both King and Jester as a
        // SimulatedKing. Its castle scan is deliberately not orthodox: it
        // walks to the first occupied square in either horizontal direction,
        // requires only an unmoved Rook at least three files away, and never
        // checks the Rook's team, cooldown, or freeze state. Check legality is
        // evaluated only after the complete move, so starting and transit
        // squares may be attacked.
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
        add_step_moves(moves, id, Around, 8, 1, false, attacksOnly);
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
            if (forwardTarget == NoPiece) {
                moves.push_back({piece.square, static_cast<std::uint8_t>(one)});
                const int two = make_square(file, rank + 2 * direction);
                // Native SimulatedPawn gates the double step only on
                // pieceMoved; deployment may put an unmoved pawn on any of
                // the three home ranks.
                if (!piece.moved && valid_square(two) && board_[two] == NoPiece)
                    moves.push_back({piece.square, static_cast<std::uint8_t>(two)});
            }
            else if (pieces_[forwardTarget].type == PieceType::Ghost &&
                     !pieces_[forwardTarget].visible &&
                     pieces_[forwardTarget].color != piece.color) {
                // SimulatedPawn has a dedicated blind-collision path: walking
                // forward into an unseen enemy Ghost knocks out both pieces.
                moves.push_back({piece.square, static_cast<std::uint8_t>(one)});
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
        const int radius = std::min(7, 1 + int(piece.power));
        for (int file = std::max(0, file_of(piece.square) - radius);
             file <= std::min(BoardFiles - 1, file_of(piece.square) + radius); ++file)
            for (int rank = std::max(0, rank_of(piece.square) - radius);
                 rank <= std::min(BoardRanks - 1, rank_of(piece.square) + radius); ++rank) {
                const int to = make_square(file, rank);
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
        // occupied squares must not be generated as captures or attack-map
        // entries.  This matters when answering check: the app rejected a
        // live h1-h2 attempt against an enemy Dragon on h2.
        if (!attacksOnly) {
            add_step_moves(moves, id, Around, 8, 1, false, false);
            moves.erase(std::remove_if(moves.begin(), moves.end(), [this](const Move& move) {
                            return board_[move.to] != NoPiece;
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
                    if (to != piece.square && board_[to] == NoPiece)
                        moves.push_back({piece.square, static_cast<std::uint8_t>(to), 0,
                                         MoveKind::Spawn});
                }
        break;
    case PieceType::Sludge:
        add_slider_moves(moves, id, Orthogonal, 4, 2, false, false);
        moves.erase(std::remove_if(moves.begin(), moves.end(), [this](const Move& move) {
                        return board_[move.to] != NoPiece;
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
                if (target == NoPiece ||
                    (pieces_[target].visible && pieces_[target].color != piece.color))
                    moves.push_back({piece.square, static_cast<std::uint8_t>(to)});
            }
        }
        {
            const int direction = piece.color == Color::White ? 1 : -1;
            for (int rank = rank_of(piece.square) + direction;
                 rank >= 0 && rank < BoardRanks; rank += direction) {
                const int target = board_[make_square(file_of(piece.square), rank)];
                if (target == NoPiece)
                    continue;
                if (pieces_[target].type == PieceType::Ghost && !pieces_[target].visible)
                    continue;
                if (pieces_[target].color != piece.color)
                    moves.push_back({piece.square, pieces_[target].square,
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
    if (!has_real_king(Color::White) || !has_real_king(Color::Black) ||
        !is_checkmate_possible())
        return {};

    auto moves = pseudo_legal_moves();
    // Reuse one mutable child across the complete frontier. make_move_unchecked
    // already snapshots every field in Undo, so constructing another full
    // Position for each sibling only duplicated the hottest copy in search.
    Position child = *this;
    const Color mover = sideToMove_;
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const Move& move) {
        Undo undo;
        if (!child.make_move_unchecked(move, undo))
            return true;
        const bool ownKingAlive = child.has_real_king(mover);
        const bool continuation = child.sideToMove_ == mover && child.has_forced_action();
        const bool opponentKingDead = !child.has_real_king(~mover);
        const bool legal = ownKingAlive &&
          (continuation || opponentKingDead || !child.real_king_threatened(mover));
        child.undo_move(undo);
        return !legal;
    }), moves.end());
    return moves;
}

std::vector<Move> Position::pseudo_legal_moves() const {
    if (forcedPiece_ != NoPiece)
        return moves_for(
            forcedPiece_, continuation_ == Continuation::CheckerJump);

    std::vector<Move> moves;
    for (int id = 0; id < pieceCount_; ++id) {
        if (!pieces_[id].alive || !pieces_[id].onBoard || pieces_[id].color != sideToMove_)
            continue;
        auto generated = moves_for(id);
        moves.insert(moves.end(), generated.begin(), generated.end());
    }
    return moves;
}

bool Position::real_king_threatened(Color color) const {
    if (!has_real_king(color))
        return true;

    int kingSquare = NoSquare;
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].color == color &&
            pieces_[id].type == PieceType::King) {
            kingSquare = pieces_[id].square;
            break;
        }

    const auto adjacent = [](int lhs, int rhs) {
        return std::abs(file_of(lhs) - file_of(rhs)) <= 1 &&
               std::abs(rank_of(lhs) - rank_of(rhs)) <= 1;
    };

    // A captured Bomb can reach the King through a chain of adjacent Bombs.
    // Build that connected danger set once, then reject the many captures
    // whose victim and blast cannot possibly affect the royal.
    Bitboard dangerousBombs = 0;
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].onBoard && pieces_[id].type == PieceType::Bomb &&
            adjacent(pieces_[id].square, kingSquare))
            dangerousBombs |= square_bb(pieces_[id].square);
    for (bool changed = true; changed;) {
        changed = false;
        for (int id = 0; id < pieceCount_; ++id) {
            if (!pieces_[id].alive || !pieces_[id].onBoard ||
                pieces_[id].type != PieceType::Bomb ||
                (dangerousBombs & square_bb(pieces_[id].square)))
                continue;
            Bitboard connected = dangerousBombs;
            while (connected) {
                if (adjacent(pieces_[id].square, pop_lsb(connected))) {
                    dangerousBombs |= square_bb(pieces_[id].square);
                    changed = true;
                    break;
                }
            }
        }
    }

    const Bitboard royalDanger = square_bb(kingSquare) | dangerousBombs;
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
    Position attacker = *this;
    attacker.sideToMove_ = ~color;
    attacker.forcedPiece_ = NoPiece;
    attacker.continuation_ = Continuation::None;
    for (int actor = 0; actor < attacker.pieceCount_; ++actor) {
        if (!attacker.pieces_[actor].alive || !attacker.pieces_[actor].onBoard ||
            attacker.pieces_[actor].color != attacker.sideToMove_)
            continue;
        // Ghost attacks never produce check/checkmate in the app.  A royal may
        // enter a hidden Ghost's adjacency, reveal it, and remain alive until
        // the Ghost actually captures it on a later action.
        if (attacker.pieces_[actor].type == PieceType::Ghost)
            continue;

        const PieceType actorType = attacker.pieces_[actor].type;
        const bool needsQuietCompanion =
          actorType == PieceType::Mage || actorType == PieceType::Fisherman ||
          actorType == PieceType::Copycat || actorType == PieceType::CopycatClone;
        // Most characters can knock out a King only through their native
        // attack list. The four exceptions need their full list: Mage and
        // Fisherman can forcibly translate a Giant, while either CopyCat half
        // may move quietly as its mirrored partner captures the King.
        const auto replies = attacker.moves_for(actor, !needsQuietCompanion);
        for (const Move& reply : replies) {
            // Simulate only actions whose affected square set intersects the
            // King or a Bomb chain leading to it. This retains native death,
            // Angel, Parasite, and blast semantics without applying unrelated
            // captures merely to discover that the King survived.
            bool canKnockOut = false;
            if (actorType == PieceType::Bomb) {
                canKnockOut = dangerousBlast(reply.to);
            }
            else if (actorType == PieceType::Giant) {
                canKnockOut = bool(attacker.footprint(actor, reply.to) & royalDanger);
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
                    destinationRank >= 0 && destinationRank < BoardRanks)
                    canKnockOut = bool(attacker.footprint(
                      giant, make_square(destinationFile, destinationRank)) & royalDanger);
            }
            else if (reply.kind == MoveKind::Pull) {
                const int pulled = valid_square(reply.to) ? attacker.board_[reply.to] : NoPiece;
                if (pulled != NoPiece && attacker.pieces_[pulled].type == PieceType::Giant) {
                    const int destinationFile = file_of(attacker.pieces_[pulled].square) +
                                                file_of(reply.auxiliary) - file_of(reply.to);
                    const int destinationRank = rank_of(attacker.pieces_[pulled].square) +
                                                rank_of(reply.auxiliary) - rank_of(reply.to);
                    if (destinationFile >= 0 && destinationFile < BoardFiles &&
                        destinationRank >= 0 && destinationRank < BoardRanks)
                        canKnockOut = bool(attacker.footprint(
                          pulled, make_square(destinationFile, destinationRank)) & royalDanger);
                }
            }
            else if (actorType == PieceType::Copycat ||
                     actorType == PieceType::CopycatClone) {
                const int primary = valid_square(reply.to) ? attacker.board_[reply.to] : NoPiece;
                const int mirrored = valid_square(reply.auxiliary)
                                   ? attacker.board_[reply.auxiliary] : NoPiece;
                canKnockOut = dangerousVictim(primary) || dangerousVictim(mirrored);
            }
            else if (reply.kind == MoveKind::Shoot) {
                canKnockOut = dangerousVictim(reply.auxiliary);
            }
            else if ((actorType == PieceType::Checker ||
                      actorType == PieceType::CheckerKing) &&
                     valid_square(reply.auxiliary)) {
                canKnockOut = dangerousVictim(attacker.board_[reply.auxiliary]);
            }
            else if (valid_square(reply.to)) {
                canKnockOut = dangerousVictim(attacker.board_[reply.to]);
            }
            if (!canKnockOut)
                continue;
            Undo undo;
            if (!attacker.make_move_unchecked(reply, undo))
                continue;
            const bool killed = !attacker.has_real_king(color);
            attacker.undo_move(undo);
            if (killed)
                return true;
        }
    }
    return false;
}

bool Position::is_capture(const Move& move) const {
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

bool Position::is_legal(const Move& move) const {
    const auto moves = legal_moves();
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

std::vector<int> Position::victims_on(Bitboard mask, int exceptId) const {
    std::vector<int> victims;
    std::unordered_set<int> seen;
    while (mask) {
        const int square = pop_lsb(mask);
        const int id = board_[square];
        if (id != NoPiece && id != exceptId && seen.insert(id).second)
            victims.push_back(id);
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

    // SimulatedBerserker::MakeMove grows on any attack recorded in Move.target,
    // including an Angel save or a possession interaction.
    if (attackerType == PieceType::Berserker)
        pieces_[attacker].power = std::min<std::uint8_t>(7, pieces_[attacker].power + 1);

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
    if (attackerType == PieceType::Parasite) {
        const Color parasiteColor = pieces_[attacker].color;
        remove_piece(attacker);
        erase_from_board(victim);
        pieces_[victim].color = parasiteColor;
        place_on_board(victim);
        return;
    }
    if (victimType == PieceType::Parasite && is_melee(attackerType) &&
        attackerType != PieceType::Bomb && pieces_[attacker].color != pieces_[victim].color) {
        const Color parasiteColor = pieces_[victim].color;
        remove_piece(victim);
        pieces_[attacker].color = parasiteColor;
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
        if (target != NoPiece && pieces_[target].type != PieceType::Penguin &&
            pieces_[target].freezeCount)
            --pieces_[target].freezeCount;
    }
    pieces_[penguin].action = 0;
}

void Position::apply_penguin_freeze(int penguin) {
    if (penguin < 0 || penguin >= pieceCount_ || !pieces_[penguin].alive ||
        pieces_[penguin].type != PieceType::Penguin)
        return;
    pieces_[penguin].action = 0;
    const int file = file_of(pieces_[penguin].square);
    const int rank = rank_of(pieces_[penguin].square);
    for (const auto& direction : Around) {
        const int targetFile = file + direction[0];
        const int targetRank = rank + direction[1];
        if (targetFile < 0 || targetFile >= BoardFiles || targetRank < 0 ||
            targetRank >= BoardRanks)
            continue;
        const int target = board_[make_square(targetFile, targetRank)];
        if (target == NoPiece || pieces_[target].type == PieceType::Penguin)
            continue;
        ++pieces_[target].freezeCount;
        pieces_[penguin].action |= penguin_direction_bit(direction[0], direction[1]);
    }
}

void Position::detach_from_penguin_freezes(int target) {
    if (target < 0 || target >= pieceCount_ || !pieces_[target].alive)
        return;
    const int targetFile = file_of(pieces_[target].square);
    const int targetRank = rank_of(pieces_[target].square);
    for (int penguin = 0; penguin < pieceCount_; ++penguin) {
        if (penguin == target || !pieces_[penguin].alive ||
            pieces_[penguin].type != PieceType::Penguin)
            continue;
        const int deltaFile = targetFile - file_of(pieces_[penguin].square);
        const int deltaRank = targetRank - rank_of(pieces_[penguin].square);
        const std::uint8_t bit = penguin_direction_bit(deltaFile, deltaRank);
        if (bit && (pieces_[penguin].action & bit))
            pieces_[penguin].action &= static_cast<std::uint8_t>(~bit);
    }
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
        if (target != NoPiece && pieces_[target].alive && pieces_[target].color != royalColor &&
            pieces_[target].type == PieceType::Ghost)
            pieces_[target].visible = true;
    }
}

void Position::advance_minions(Color color) {
    std::vector<int> minions;
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].color == color && pieces_[id].type == PieceType::Minion)
            minions.push_back(id);

    const int direction = color == Color::White ? 1 : -1;
    for (const int id : minions) {
        if (!pieces_[id].alive)
            continue;
        const int from = pieces_[id].square;
        const int targetRank = rank_of(from) + direction;
        if (targetRank < 0 || targetRank >= BoardRanks) {
            remove_piece(id);
            continue;
        }
        const int to = make_square(file_of(from), targetRank);
        const int victim = board_[to];
        if (victim != NoPiece && pieces_[victim].color == color)
            continue;
        erase_from_board(id);
        if (victim != NoPiece)
            capture_piece(victim, id, {static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to)});
        if (pieces_[id].alive) {
            pieces_[id].square = static_cast<std::uint8_t>(to);
            place_on_board(id);
        }
    }
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
    undo = {board_, pieces_, byType_, occupancy_, pieceCount_, sideToMove_, enPassantSquare_,
            enPassantVictim_, forcedPiece_, continuation_, halfmove_, fullmove_,
            nextAttachmentOrder_};

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
    }
    else if (move.kind == MoveKind::Swap) {
        const int other = move.auxiliary;
        const int otherSquare = pieces_[other].square;
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
            pieces_[other].moved = true;
            if (pieces_[other].type == PieceType::Pawn) {
                const int promotionRank = pieces_[other].color == Color::White ? BoardRanks - 1 : 0;
                if (rank_of(pieces_[other].square) == promotionRank)
                    pieces_[other].type = PieceType::Queen;
            }
            else if (pieces_[other].type == PieceType::Checker) {
                const int promotionRank = pieces_[other].color == Color::White ? BoardRanks - 1 : 0;
                if (rank_of(pieces_[other].square) == promotionRank)
                    pieces_[other].type = PieceType::CheckerKing;
            }
            place_on_board(id);
            place_on_board(other);
            if (pieces_[other].type == PieceType::Ghost)
                pieces_[other].visible = ghost_near_enemy_royal(pieces_[other].square,
                                                                 pieces_[other].color);
            else if (pieces_[other].type == PieceType::King ||
                     pieces_[other].type == PieceType::Jester)
                reveal_ghosts_near(pieces_[other].square, pieces_[other].color);
        }
    }
    else if (move.kind == MoveKind::Spawn) {
        add_piece(PieceType::Minion, actor.color, move.to);
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
            erase_from_board(victim);
            if (pieces_[victim].alive) {
                int destination = move.auxiliary;
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
                    if (occupant != NoPiece)
                        capture_piece(occupant, victim, move);
                }
                if (pieces_[victim].alive && pieces_[victim].type != PieceType::Giant) {
                    pieces_[victim].square = static_cast<std::uint8_t>(destination);
                    place_on_board(victim);
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
        const int partnerTarget = board_[move.auxiliary];
        erase_from_board(id);
        erase_from_board(partner);
        if (target != NoPiece)
            capture_piece(target, id, move);
        if (partnerTarget != NoPiece && pieces_[partner].alive)
            capture_piece(partnerTarget, partner, {pieces_[partner].square, move.auxiliary});
        if (pieces_[id].alive) {
            pieces_[id].square = move.to;
            pieces_[id].moved = true;
            place_on_board(id);
        }
        if (pieces_[partner].alive) {
            pieces_[partner].square = move.auxiliary;
            pieces_[partner].moved = true;
            place_on_board(partner);
        }
    }
    else {
        erase_from_board(id);
        int victim = target;
        const bool blindGhostCollision = actor.type == PieceType::Pawn && victim != NoPiece &&
                                       pieces_[victim].type == PieceType::Ghost &&
                                       !pieces_[victim].visible &&
                                       pieces_[victim].color != actor.color &&
                                       file_of(move.to) == file_of(originalFrom);
        if (actor.type == PieceType::Pawn && move.to == enPassantSquare_)
            victim = enPassantVictim_;
        if (checkerCapture)
            victim = board_[move.auxiliary];
        if (actor.type == PieceType::Giant) {
            const Bitboard destination = footprint(id, move.to);
            const auto victims = victims_on(destination, id);
            for (const int occupant : victims)
                if (pieces_[occupant].alive && pieces_[occupant].color != actor.color)
                    capture_piece(occupant, id, move);
        }
        else if (blindGhostCollision) {
            remove_piece(victim);
            remove_piece(id);
        }
        else if (victim != NoPiece)
            capture_piece(victim, id, move);
        if (actor.alive) {
            actor.square = move.to;
            actor.moved = true;
            if (actor.type == PieceType::Pawn) {
                const int promotionRank = actor.color == Color::White ? BoardRanks - 1 : 0;
                if (rank_of(actor.square) == promotionRank)
                    actor.type = move.promotion == PieceType::Count ? PieceType::Queen : move.promotion;
                if (std::abs(int(move.to) - originalFrom) == 16) {
                    enPassantSquare_ = (originalFrom + move.to) / 2;
                    enPassantVictim_ = id;
                    createdEnPassant = true;
                }
            }
            if (actor.type == PieceType::Checker) {
                const int promotionRank = actor.color == Color::White ? BoardRanks - 1 : 0;
                if (rank_of(actor.square) == promotionRank)
                    actor.type = PieceType::CheckerKing;
            }
            place_on_board(id);
            if (actor.type == PieceType::Ghost)
                actor.visible = capture || ghost_near_enemy_royal(actor.square, actor.color);
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
                if (board_[middle] == NoPiece)
                    add_piece(PieceType::Goop, actor.color, middle);
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
    Position child = *this;
    std::uint64_t nodes = 0;
    for (const Move& move : legal_moves()) {
        Undo undo;
        // The perft frontier is the position's own legal list; revalidating
        // each member would regenerate that complete list once per child.
        if (!child.make_move_unchecked(move, undo))
            continue;
        nodes += child.perft(depth - 1);
        child.undo_move(undo);
    }
    return nodes;
}

void Position::undo_move(const Undo& undo) {
    board_ = undo.board;
    pieces_ = undo.pieces;
    byType_ = undo.byType;
    occupancy_ = undo.occupancy;
    pieceCount_ = undo.pieceCount;
    sideToMove_ = undo.sideToMove;
    enPassantSquare_ = undo.enPassantSquare;
    enPassantVictim_ = undo.enPassantVictim;
    forcedPiece_ = undo.forcedPiece;
    continuation_ = undo.continuation;
    halfmove_ = undo.halfmove;
    fullmove_ = undo.fullmove;
    nextAttachmentOrder_ = undo.nextAttachmentOrder;
}

bool Position::has_real_king(Color color) const {
    for (int id = 0; id < pieceCount_; ++id)
        if (pieces_[id].alive && pieces_[id].color == color && pieces_[id].type == PieceType::King)
            return true;
    return false;
}

bool Position::team_has_sufficient_material(Color color) const {
    int minorCount = 0;
    bool evenColorBound = false;
    bool oddColorBound = false;
    bool support = false;
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& item = pieces_[id];
        if (!item.alive || item.color != color)
            continue;
        switch (item.type) {
        case PieceType::Jester:
        case PieceType::Pawn:
        case PieceType::Queen:
        case PieceType::Rook:
        case PieceType::Berserker:
        case PieceType::Bomb:
        case PieceType::Ninja:
        case PieceType::Ghost:
        case PieceType::Penguin:
        case PieceType::Parasite:
        case PieceType::Sniper:
        case PieceType::Prince:
        case PieceType::Giant:
        case PieceType::Copycat:
        case PieceType::Dragon:
            return true;
        case PieceType::Knight:
        case PieceType::Turtle:
            ++minorCount;
            break;
        case PieceType::Bishop:
        case PieceType::Checker:
        case PieceType::CheckerKing:
            if ((file_of(item.square) + rank_of(item.square)) & 1)
                oddColorBound = true;
            else
                evenColorBound = true;
            break;
        case PieceType::Mage:
        case PieceType::Fisherman:
            support = true;
            break;
        case PieceType::King:
        case PieceType::Goop:
        case PieceType::Devil:
        case PieceType::Minion:
        case PieceType::Sludge:
        case PieceType::CopycatClone:
        case PieceType::Angel:
        case PieceType::Halo:
        case PieceType::Count:
            break;
        }
    }
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

bool Position::game_over() const {
    if (!has_real_king(Color::White) || !has_real_king(Color::Black) ||
        !is_checkmate_possible())
        return true;
    return legal_moves().empty();
}

std::optional<Color> Position::winner() const {
    const bool white = has_real_king(Color::White);
    const bool black = has_real_king(Color::Black);
    if (white != black)
        return white ? Color::White : Color::Black;
    if (!white || !is_checkmate_possible())
        return std::nullopt;
    if (legal_moves().empty() && real_king_threatened(sideToMove_))
        return ~sideToMove_;
    return std::nullopt;
}

std::uint64_t Position::key() const {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 0x100000001b3ULL;
    };
    mix(static_cast<std::uint8_t>(sideToMove_));
    mix(static_cast<std::uint8_t>(continuation_));
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
        mix(std::uint64_t(piece.moved) | (std::uint64_t(piece.visible) << 1) |
            (std::uint64_t(piece.link + 1) << 8) |
            (std::uint64_t(piece.host + 1) << 16) |
            (std::uint64_t(piece.attachmentOrder) << 24));
    }
    return hash;
}

int Position::evaluate() const {
    int score = 0;
    int kingSquare[2] = {NoSquare, NoSquare};
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& piece = pieces_[id];
        if (piece.alive && piece.onBoard && piece.type == PieceType::King)
            kingSquare[index(piece.color)] = piece.square;
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
    if (game_over()) {
        const auto winning = winner();
        if (winning)
            score = *winning == Color::White ? 30000 : -30000;
    }
    return sideToMove_ == Color::White ? score : -score;
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
        << ";forced=" << remap(forcedPiece_) << ";epv=" << remap(enPassantVictim_);
    for (int id = 0; id < pieceCount_; ++id) {
        const PieceState& item = pieces_[id];
        if (!item.alive)
            continue;
        out << ';' << type_name(item.type) << ',' << (item.color == Color::White ? 'w' : 'b') << ','
            << square_name(item.square) << ',' << int(item.action) << ',' << int(item.cooldown) << ','
            << int(item.freezeCount) << ',' << int(item.power) << ',' << int(item.moved) << ','
            << int(item.visible) << ',' << remap(item.link) << ',' << int(item.onBoard) << ','
            << remap(item.host) << ',' << item.attachmentOrder;
    }
    return out.str();
}

bool Position::set_upn(std::string_view text, std::string* error) {
    Position parsed;
    parsed.clear();
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
        int numbers[10] = {0, 0, 0, 0, 0, 1, -1, 1, -1, 0};
        for (std::size_t i = 3; i < values.size() && i < 13; ++i)
            if (!parse_int(values[i], numbers[i - 3]))
                return fail("invalid numeric piece state");
        if (numbers[0] < 0 || numbers[0] > 255 || numbers[1] < 0 || numbers[1] > 255 ||
            numbers[2] < 0 || numbers[2] > 255 || numbers[3] < 0 || numbers[3] > 255 ||
            (numbers[4] != 0 && numbers[4] != 1) ||
            (numbers[5] != 0 && numbers[5] != 1) ||
            numbers[6] < NoPiece || numbers[6] >= MaxPieces ||
            (numbers[7] != 0 && numbers[7] != 1) ||
            numbers[8] < NoPiece || numbers[8] >= MaxPieces ||
            numbers[9] < 0 || numbers[9] > 65535)
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
    }

    // Older/custom UPN strings may name only the deployable CopyCat. Native
    // construction creates a clone automatically; explicit lossless strings
    // name both members and are paired here without duplicating them.
    for (int id = 0; id < parsed.pieceCount_; ++id) {
        PieceState& copycat = parsed.pieces_[id];
        if (!copycat.alive || copycat.type != PieceType::Copycat || copycat.link != NoPiece)
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
