/*
  Ultimate Fish - search for Chess Ultimate
  GPLv3 or later
*/

#ifndef ULTIMATE_SEARCH_H_INCLUDED
#define ULTIMATE_SEARCH_H_INCLUDED

#include "position.h"
#include "nnue.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {

struct SearchLimits {
    int depth = 10;
    std::uint64_t nodes = 0;
    std::chrono::milliseconds moveTime{0};
    std::chrono::milliseconds remainingTime{0};
    std::chrono::milliseconds increment{0};
    std::chrono::milliseconds moveOverhead{50};
    int movesToGo = 0;
    std::vector<Move> rootMoves;
    // Roots which immediately complete a public threefold repetition. They
    // remain legal candidates but must score as draws inside native search.
    std::vector<std::string> rootDrawMoveStrings;
};

struct SearchResult {
    std::optional<Move> bestMove;
    int score = 0;
    int completedDepth = 0;
    std::uint64_t nodes = 0;
    std::chrono::milliseconds elapsed{0};
    std::vector<Move> principalVariation;
};

// A public-information position is a set of concrete positions that differ
// only in facts hidden by Chess Ultimate (currently Ghost coordinates and
// ambiguous King/Jester identities).  The engine, rather than its caller,
// owns root selection across that set.
struct BeliefSearchResult {
    std::optional<std::string> bestMove;
    int score = 0;
    int worstScore = 0;
    int meanScore = 0;
    int completedDepth = 0;
    std::uint64_t nodes = 0;
    std::chrono::milliseconds elapsed{0};
    std::size_t beliefs = 0;
    std::size_t deepBeliefs = 0;
    std::size_t commonMoves = 0;
    std::size_t candidates = 0;
    std::vector<std::string> principalVariation;
};

class Search {
   public:
    explicit Search(std::size_t hashMegabytes = 64);

    SearchResult think(Position& position, const SearchLimits& limits);
    BeliefSearchResult think_beliefs(const std::vector<Position>& beliefs,
                                     const SearchLimits& limits,
                                     std::size_t maximumDeepBeliefs = 8,
                                     std::size_t maximumCandidates = 8);
    void clear();

   private:
    static constexpr int MaxPly = 128;
    enum class Bound : std::uint8_t { None, Upper, Lower, Exact };
    // Keep the transposition table's move payload compact even though the
    // search-local Move carries a cached ordering score.
    struct StoredMove {
        std::uint8_t from = 0;
        std::uint8_t to = 0;
        std::uint8_t auxiliary = 0;
        MoveKind kind = MoveKind::Normal;
        PieceType promotion = PieceType::Count;

        StoredMove& operator=(const Move& move) {
            from = move.from;
            to = move.to;
            auxiliary = move.auxiliary;
            kind = move.kind;
            promotion = move.promotion;
            return *this;
        }
        [[nodiscard]] Move unpack() const {
            return {from, to, auxiliary, kind, promotion};
        }
    };
    static_assert(sizeof(StoredMove) == 5,
                  "TT move storage must remain compact");
    struct Entry {
        std::uint64_t key = 0;
        StoredMove move{};
        std::int16_t score = 0;
        std::int8_t depth = -1;
        Bound bound = Bound::None;
        std::uint8_t generation = 0;
    };
    static constexpr std::size_t ClusterSize = 4;
    struct Cluster {
        std::array<Entry, ClusterSize> entries{};
    };

    int negamax(Position& position, int depth, int alpha, int beta, int ply,
                std::vector<Move>& pv, const Move* excludedMove = nullptr);
    int quiescence(Position& position, int alpha, int beta, int ply);
    int move_score(const Position& position, const Move& move,
                   const Move* ttMove, int ply) const;
    int evaluate(const Position& position, int ply) const;
    bool stopped();
    Entry* find_entry(std::uint64_t key);
    Entry& replacement_entry(std::uint64_t key);

    std::vector<Cluster> table_;
    SearchLimits limits_;
    std::chrono::steady_clock::time_point start_;
    std::chrono::milliseconds softTime_{0};
    std::uint64_t nodes_ = 0;
    std::uint8_t generation_ = 0;
    bool stop_ = false;
    bool useNnue_ = false;
    std::vector<Move> rootMoves_;
    std::vector<Move> rootDrawMoves_;
    std::array<std::array<int, Position::BoardSquares>, static_cast<std::size_t>(PieceType::Count)>
      history_{};
    std::array<std::array<Move, 2>, MaxPly> killers_{};
    std::array<UltimateNnue::Accumulator, MaxPly> accumulators_{};
};

}  // namespace Stockfish::Ultimate

#endif
