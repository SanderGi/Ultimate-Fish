/*
  Ultimate Fish - search for Chess Ultimate
  GPLv3 or later
*/

#ifndef ULTIMATE_SEARCH_H_INCLUDED
#define ULTIMATE_SEARCH_H_INCLUDED

#include "information.h"
#include "position.h"
#include "nnue.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Stockfish::Ultimate {

struct SearchResult;

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
    // Optional reporting hook invoked after each fully completed iterative-
    // deepening pass. Callers that do not opt in retain the single final result.
    std::function<void(const SearchResult&)> onIteration;
};

struct SearchResult {
    std::optional<Move> bestMove;
    int score = 0;
    // Exact signed distance-to-win in native Ultimate actions. This is kept
    // separate from score so long tablebase wins cannot be mistaken for a
    // large centipawn evaluation, and because same-side continuations make
    // orthodox ply-to-move conversion invalid.
    std::optional<int> mateActions;
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
    std::optional<int> mateActions;
    int worstScore = 0;
    int meanScore = 0;
    int completedDepth = 0;
    std::uint64_t nodes = 0;
    std::chrono::milliseconds elapsed{0};
    std::size_t beliefs = 0;
    std::size_t deepBeliefs = 0;
    std::size_t commonMoves = 0;
    std::size_t candidates = 0;
    // False when the configured observer is to move but the supplied state
    // still spans multiple privately visible legal-dot observations.
    bool validInformationCell = true;
    // The current live solver constrains only the root action across the
    // complete information set.  Deeper searches are explicitly concrete-
    // world searches until the history-preserving search lands.
    int historyPreservingPlies = 0;
    std::vector<std::string> principalVariation;
};

struct BeliefTransitionResult {
    std::size_t before = 0;
    std::size_t after = 0;
    std::size_t observations = 0;
    bool applied = false;
};

struct BeliefSuccessorBucket {
    std::string observation;
    std::vector<Position> worlds;
};

struct BeliefSuccessorPartitions {
    std::size_t before = 0;
    std::size_t incompatible = 0;
    std::vector<BeliefSuccessorBucket> buckets;
};

struct BeliefDecisionBucket {
    std::string observation;
    std::vector<Position> worlds;
};

// Collision-free, uncapped public information set shared by protocol clients.
// Every concrete world must have the same ordinary public view for the
// configured observer. Mover-private legal-dot differences are retained and
// reported as decision partitions; they are never silently exposed or used to
// select the actual world.
class PublicBeliefState {
   public:
    explicit PublicBeliefState(DisclosureContext disclosure = {});

    void clear();
    bool set_disclosure(DisclosureContext disclosure,
                        std::string* error = nullptr);
    [[nodiscard]] const DisclosureContext& disclosure() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::optional<Color> side_to_move() const;
    [[nodiscard]] const std::string& public_view() const;

    // Returns true for both a new world and an exact duplicate. `size()`
    // distinguishes the two. No hash-only identity or world bound is used.
    bool add(Position position, std::string* error = nullptr);
    [[nodiscard]] std::vector<Position> positions() const;
    [[nodiscard]] std::vector<std::string> common_moves() const;
    [[nodiscard]] std::vector<BeliefDecisionBucket> decision_cells() const;
    [[nodiscard]] std::size_t decision_partitions() const;

    // Enumerate every exact successor bucket without mutating this belief.
    // The observation key is the public transition observation, augmented by
    // the configured observer's private legal-dot observation when that
    // observer becomes the mover. `incompatible` counts retained worlds in
    // which the concretely spelled action was not legal.
    [[nodiscard]] BeliefSuccessorPartitions successor_partitions(
      std::string_view move) const;

    // Apply one concretely spelled action only if it is legal in every world.
    // If any world is incompatible, or if the
    // public animation/result partitions into more than one observation, the
    // state is left unchanged and the caller must condition on an observed
    // successor rather than discarding worlds or merging distinguishable
    // histories. A successor decision observation is included when the
    // configured observer becomes the mover.
    BeliefTransitionResult apply_known(std::string_view move,
                                       std::string* error = nullptr);

   private:
    DisclosureContext disclosure_{};
    std::optional<Color> side_;
    std::string publicView_;
    std::map<std::string, Position> worlds_;
};

class Search {
   public:
    explicit Search(std::size_t hashMegabytes = 64);

    SearchResult think(Position& position, const SearchLimits& limits);
    BeliefSearchResult think_beliefs(const PublicBeliefState& beliefs,
                                     const SearchLimits& limits,
                                     std::size_t maximumDeepBeliefs = 8,
                                     std::size_t maximumCandidates = 8);
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
