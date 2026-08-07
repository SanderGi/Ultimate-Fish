/*
  Ultimate Fish - Chess Ultimate rules engine
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_POSITION_H_INCLUDED
#define ULTIMATE_POSITION_H_INCLUDED

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Stockfish::Ultimate {

// Chess Ultimate uses an 8x10 board. A native 128-bit integer keeps the hot
// occupancy operations branch-free while leaving 48 spare bits for sentinels
// and future rule work.
using Bitboard = unsigned __int128;

enum class Color : std::uint8_t { White, Black };

constexpr Color operator~(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

// Numeric values deliberately match Character.Type in Chess Ultimate 5.731.
enum class PieceType : std::uint8_t {
    King,
    Jester,
    Knight,
    Pawn,
    Queen,
    Rook,
    Bishop,
    Berserker,
    Bomb,
    Ninja,
    Turtle,
    Ghost,
    Mage,
    Goop,
    Penguin,
    Parasite,
    Devil,
    Minion,
    Sludge,
    Sniper,
    Prince,
    Checker,
    CheckerKing,
    Giant,
    Copycat,
    CopycatClone,
    Angel,
    Halo,
    Fisherman,
    Dragon,
    Count
};

enum class PieceClass : std::uint8_t { Melee, Ranged, Support };

enum class MoveKind : std::uint8_t {
    Normal,
    Castle,
    Swap,
    Spawn,
    Shoot,
    Pull,
    Link,
    Pass
};

enum class Continuation : std::uint8_t {
    None,
    CheckerJump,
    PrinceSecondMove
};

struct PieceInfo {
    std::string_view name;
    PieceClass pieceClass;
    std::uint8_t baseCooldown;
    std::uint8_t draftCost;
    bool selectable;
    bool generated;
};

struct PieceState {
    PieceType type = PieceType::Pawn;
    Color color = Color::White;
    std::uint8_t square = 0;
    bool onBoard = true;
    std::uint8_t action = 0;
    std::uint8_t cooldown = 0;
    std::uint8_t freezeCount = 0;
    std::uint8_t power = 0;
    std::int8_t link = -1;
    std::int8_t host = -1;
    std::uint16_t attachmentOrder = 0;
    bool alive = false;
    bool moved = false;
    bool visible = true;
};

struct Move {
    static constexpr std::uint8_t CaptureKnown = 1;
    static constexpr std::uint8_t Capture = 2;

    std::uint8_t from = 0;
    std::uint8_t to = 0;
    std::uint8_t auxiliary = 0;
    MoveKind kind = MoveKind::Normal;
    PieceType promotion = PieceType::Count;
    // Generated legal frontiers cache capture classification. Flags are
    // deliberately excluded from move identity and protocol notation.
    std::uint8_t flags = 0;
    // Search ordering is position-local scratch state. Caching it here avoids
    // recomputing board lookups and history values O(N log N) times inside a
    // sort comparator; it is likewise excluded from move identity/notation.
    std::int32_t orderScore = 0;

    friend bool operator==(const Move& lhs, const Move& rhs) {
        return lhs.from == rhs.from && lhs.to == rhs.to && lhs.auxiliary == rhs.auxiliary
            && lhs.kind == rhs.kind && lhs.promotion == rhs.promotion;
    }
};

// The reference model intentionally stores a complete previous position in an
// Undo record. It is slower than the production search state, but provides a
// simple differential oracle for special-effect and undo conformance tests.
struct Undo;
class TablebaseGenerator;
class TablebaseProbe;
class UltimateNnue;

class Position {
   public:
    static constexpr int BoardFiles = 8;
    static constexpr int BoardRanks = 10;
    static constexpr int BoardSquares = BoardFiles * BoardRanks;
    static constexpr int MaxPieces = 96;
    static constexpr int NoSquare = -1;
    static constexpr int NoPiece = -1;

    Position();

    void clear();
    int add_piece(PieceType type, Color color, int square);
    bool remove_piece(int id);

    [[nodiscard]] Color side_to_move() const { return sideToMove_; }
    void set_side_to_move(Color color) { sideToMove_ = color; }
    [[nodiscard]] int piece_on(int square) const;
    [[nodiscard]] const PieceState& piece(int id) const { return pieces_.at(id); }
    [[nodiscard]] PieceState& piece(int id) { return pieces_.at(id); }
    [[nodiscard]] int piece_count() const { return pieceCount_; }
    [[nodiscard]] Bitboard occupied() const { return occupancy_[0] | occupancy_[1]; }
    [[nodiscard]] Bitboard occupied(Color color) const { return occupancy_[index(color)]; }
    [[nodiscard]] Bitboard pieces(Color color, PieceType type) const {
        return byType_[index(color)][index(type)];
    }
    [[nodiscard]] Continuation continuation() const { return continuation_; }
    [[nodiscard]] int forced_piece() const { return forcedPiece_; }
    [[nodiscard]] bool has_forced_action() const;
    [[nodiscard]] int en_passant_square() const { return enPassantSquare_; }

    [[nodiscard]] std::vector<Move> legal_moves() const;
    [[nodiscard]] std::vector<Move> legal_forcing_moves() const;
    [[nodiscard]] bool is_legal(const Move& move) const;
    [[nodiscard]] bool is_capture(const Move& move) const;
    [[nodiscard]] std::optional<int> static_exchange(const Move& move) const;
    bool make_move(const Move& move, Undo& undo);
    void undo_move(const Undo& undo);

    [[nodiscard]] bool has_real_king(Color color) const;
    [[nodiscard]] bool team_has_sufficient_material(Color color) const;
    [[nodiscard]] bool is_checkmate_possible() const;
    [[nodiscard]] bool game_over() const;
    [[nodiscard]] std::optional<Color> winner() const;
    [[nodiscard]] std::optional<Color> forced_timeout_winner() const {
        if (forcedTimeoutWinner_ < 0)
            return std::nullopt;
        return static_cast<Color>(forcedTimeoutWinner_);
    }
    [[nodiscard]] std::uint64_t key() const;
    // Material/placement evaluation only. Search performs its own terminal
    // checks and must not regenerate a complete legal frontier at every leaf.
    [[nodiscard]] int static_evaluate() const;
    [[nodiscard]] int evaluate() const;
    [[nodiscard]] std::uint64_t perft(int depth) const;

    [[nodiscard]] std::string upn() const;
    bool set_upn(std::string_view text, std::string* error = nullptr);
    [[nodiscard]] std::string move_to_string(const Move& move) const;
    [[nodiscard]] std::optional<Move> move_from_string(std::string_view text) const;

    static const PieceInfo& info(PieceType type);
    static int material_value(PieceType type);
    [[nodiscard]] int material_points(int id) const;
    [[nodiscard]] int material_points(Color color) const;
    static std::optional<PieceType> type_from_name(std::string_view name);
    static std::string_view type_name(PieceType type);
    static int square_from_name(std::string_view name);
    static std::string square_name(int square);

   private:
    friend struct Undo;
    friend class Search;
    friend class TablebaseGenerator;
    friend class TablebaseProbe;
    friend class UltimateNnue;

    [[nodiscard]] int handcrafted_evaluate() const;

    static constexpr std::size_t index(Color color) { return static_cast<std::size_t>(color); }
    static constexpr std::size_t index(PieceType type) { return static_cast<std::size_t>(type); }

    [[nodiscard]] std::vector<Move> moves_for(int id, bool attacksOnly = false) const;
    void append_moves_for(std::vector<Move>& moves, int id,
                          bool attacksOnly = false) const;
    [[nodiscard]] std::vector<Move> pseudo_legal_moves() const;
    [[nodiscard]] std::vector<Move> pseudo_forcing_moves() const;
    [[nodiscard]] std::vector<Move> filter_legal_moves(
      std::vector<Move> moves) const;
    [[nodiscard]] bool legal_after_unchecked_move(Color mover) const;
    void annotate_captures(std::vector<Move>& moves) const;
    [[nodiscard]] bool is_forcing_action(const Move& move) const;
    [[nodiscard]] bool real_king_threatened(Color color) const;
    bool make_move_unchecked(const Move& move, Undo& undo);
    bool apply_move_unchecked(const Move& move);
    int add_piece_internal(PieceType type, Color color, int square, bool generateCompanions,
                           bool onBoard = true);
    void add_step_moves(std::vector<Move>& moves, int id, const int (*directions)[2], int count,
                        int distance = 1, bool jump = false, bool attacksOnly = false) const;
    void add_knight_moves(std::vector<Move>& moves, int id, bool attacksOnly = false) const;
    void add_slider_moves(std::vector<Move>& moves, int id, const int (*directions)[2], int count,
                          int distance, bool jump, bool attacksOnly = false) const;
    void add_checker_moves(std::vector<Move>& moves, int id, bool attacksOnly) const;
    void add_giant_moves(std::vector<Move>& moves, int id, bool attacksOnly) const;
    void add_copycat_moves(std::vector<Move>& moves, int id, bool attacksOnly) const;
    void add_fisherman_moves(std::vector<Move>& moves, int id, bool attacksOnly) const;

    [[nodiscard]] bool can_land(int id, int square, bool attacksOnly,
                                bool hiddenEnemyTargetable = false) const;
    [[nodiscard]] bool frozen(int id) const { return pieces_[id].freezeCount != 0; }
    [[nodiscard]] bool is_melee(PieceType type) const;
    [[nodiscard]] Bitboard footprint(int id, int anchor) const;
    [[nodiscard]] bool footprint_fits(int id, int anchor, bool allowOwnCurrent) const;
    [[nodiscard]] std::vector<int> victims_on(Bitboard mask, int exceptId = NoPiece) const;

    void rebuild_bitboards();
    void place_on_board(int id);
    void erase_from_board(int id);
    bool remove_piece_internal(int id, bool allowAngel);
    void transfer_attached_angels(int fromHost, int toHost);
    void capture_piece(int victim, int attacker, const Move& move);
    void explode_at(int center, int attacker);
    void relocate_giant(int id, int destination, bool markMoved = true,
                        bool eraseOrigin = true);
    [[nodiscard]] int attached_angel(int host) const;
    void sacrifice_angel(int angel, int host);
    void clear_penguin_freeze(int penguin);
    void apply_penguin_freeze(int penguin);
    void detach_from_penguin_freezes(int target);
    void prepare_for_forced_relocation(int target);
    void apply_forced_promotion(int target);
    void reveal_ghosts_near(int square, Color royalColor);
    [[nodiscard]] bool ghost_near_enemy_royal(int square, Color ghostColor) const;
    void advance_minions(Color color);
    void finish_turn();
    [[nodiscard]] bool checker_has_capture(int id) const;

    std::array<std::int8_t, BoardSquares> board_{};
    std::array<PieceState, MaxPieces> pieces_{};
    std::array<std::array<Bitboard, static_cast<std::size_t>(PieceType::Count)>, 2> byType_{};
    std::array<Bitboard, 2> occupancy_{};
    int pieceCount_ = 0;
    Color sideToMove_ = Color::White;
    int enPassantSquare_ = NoSquare;
    int enPassantVictim_ = NoPiece;
    int forcedPiece_ = NoPiece;
    Continuation continuation_ = Continuation::None;
    std::int8_t forcedTimeoutWinner_ = -1;
    std::uint32_t halfmove_ = 0;
    std::uint32_t fullmove_ = 1;
    std::uint16_t nextAttachmentOrder_ = 1;
};

struct Undo {
    std::array<std::int8_t, Position::BoardSquares> board;
    std::array<PieceState, Position::MaxPieces> pieces;
    std::array<std::array<Bitboard, static_cast<std::size_t>(PieceType::Count)>, 2> byType;
    std::array<Bitboard, 2> occupancy;
    int pieceCount;
    Color sideToMove;
    int enPassantSquare;
    int enPassantVictim;
    int forcedPiece;
    Continuation continuation;
    std::int8_t forcedTimeoutWinner;
    std::uint32_t halfmove;
    std::uint32_t fullmove;
    std::uint16_t nextAttachmentOrder;
};

}  // namespace Stockfish::Ultimate

#endif
