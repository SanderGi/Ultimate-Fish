/*
  Ultimate Fish - search for Chess Ultimate
  GPLv3 or later
*/

#include "search.h"
#include "tablebase_probe.h"

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>
#include <numeric>
#include <set>

namespace Stockfish::Ultimate {
namespace {

constexpr int Infinity = 32000;
constexpr int Mate = 30000;
constexpr int MateThreshold = Mate - 128;

int piece_order_value(PieceType type) {
    return type == PieceType::Count ? 0 : Position::material_value(type);
}

int score_to_tt(int score, int ply) {
    if (score >= MateThreshold)
        return score + ply;
    if (score <= -MateThreshold)
        return score - ply;
    return score;
}

int score_from_tt(int score, int ply) {
    if (score >= MateThreshold)
        return score - ply;
    if (score <= -MateThreshold)
        return score + ply;
    return score;
}

// Protocol notation is the public action chosen by a UI, but Move also carries
// internal auxiliary and promotion identity. Most native actions have one
// legal Move for a spelling; if a future position exposes two, selecting the
// first would silently collapse distinct actions. Treat such a spelling as
// unusable until the protocol has an explicit disambiguator.
std::optional<Move> unique_notated_move(const Position& position,
                                        std::string_view notation) {
    std::optional<Move> result;
    for (const Move& move : position.legal_moves()) {
        if (position.move_to_string(move) != notation)
            continue;
        if (result)
            return std::nullopt;
        result = move;
    }
    return result;
}

std::set<std::string> unambiguous_notated_moves(const Position& position) {
    std::map<std::string, std::size_t> counts;
    for (const Move& move : position.legal_moves())
        ++counts[position.move_to_string(move)];
    std::set<std::string> result;
    for (const auto& [notation, count] : counts)
        if (count == 1)
            result.insert(notation);
    return result;
}

}  // namespace

PublicBeliefState::PublicBeliefState(DisclosureContext disclosure) :
    disclosure_(disclosure) {}

void PublicBeliefState::clear() {
    side_.reset();
    publicView_.clear();
    worlds_.clear();
}

bool PublicBeliefState::set_disclosure(DisclosureContext disclosure,
                                       std::string* error) {
    if (!worlds_.empty()) {
        if (error)
            *error = "cannot change belief disclosure with retained worlds";
        return false;
    }
    disclosure_ = disclosure;
    return true;
}

const DisclosureContext& PublicBeliefState::disclosure() const {
    return disclosure_;
}

std::size_t PublicBeliefState::size() const {
    return worlds_.size();
}

bool PublicBeliefState::empty() const {
    return worlds_.empty();
}

std::optional<Color> PublicBeliefState::side_to_move() const {
    return side_;
}

const std::string& PublicBeliefState::public_view() const {
    return publicView_;
}

const std::map<std::string, Position>& PublicBeliefState::concrete_worlds() const {
    return worlds_;
}

bool PublicBeliefState::add(Position position, std::string* error) {
    if (side_ && position.side_to_move() != *side_) {
        if (error)
            *error = "side-to-move differs from the retained belief";
        return false;
    }
    const std::string view = view_key(position, disclosure_);
    if (!publicView_.empty() && view != publicView_) {
        if (error)
            *error = "ordinary public view differs from the retained belief";
        return false;
    }
    const std::string upn = position.upn();
    const auto [iterator, inserted] = worlds_.emplace(upn, std::move(position));
    (void)iterator;
    if (inserted && worlds_.size() == 1) {
        side_ = worlds_.begin()->second.side_to_move();
        publicView_ = std::move(view);
    }
    return true;
}

std::vector<Position> PublicBeliefState::positions() const {
    std::vector<Position> result;
    result.reserve(worlds_.size());
    for (const auto& [upn, position] : worlds_) {
        (void)upn;
        result.push_back(position);
    }
    return result;
}

std::vector<std::string> PublicBeliefState::common_moves() const {
    std::set<std::string> common;
    bool first = true;
    for (const auto& [upn, position] : worlds_) {
        (void)upn;
        std::set<std::string> legal = unambiguous_notated_moves(position);
        if (first) {
            common = std::move(legal);
            first = false;
            continue;
        }
        std::set<std::string> intersection;
        std::set_intersection(common.begin(), common.end(), legal.begin(),
                              legal.end(),
                              std::inserter(intersection, intersection.begin()));
        common = std::move(intersection);
        if (common.empty())
            break;
    }
    return {common.begin(), common.end()};
}

std::vector<BeliefDecisionBucket> PublicBeliefState::decision_cells() const {
    std::map<std::string, std::vector<Position>> observations;
    for (const auto& [upn, position] : worlds_) {
        (void)upn;
        const std::string observation = side_ &&
          *side_ == disclosure_.observer
          ? decision_observation_key(position, disclosure_) : std::string();
        observations[observation].push_back(position);
    }
    std::vector<BeliefDecisionBucket> result;
    result.reserve(observations.size());
    for (auto& [observation, worlds] : observations)
        result.push_back({std::move(observation), std::move(worlds)});
    return result;
}

std::size_t PublicBeliefState::decision_partitions() const {
    if (!side_ || *side_ != disclosure_.observer)
        return 0;
    std::set<std::string> observations;
    for (const auto& [upn, position] : worlds_) {
        (void)upn;
        observations.insert(decision_observation_key(position, disclosure_));
    }
    return observations.size();
}

bool PublicBeliefState::condition_on_decision_markers(
  const std::vector<std::string>& supplied, std::string* error) {
    if (worlds_.empty()) {
        if (error)
            *error = "cannot observe legal dots on an empty belief";
        return false;
    }
    if (!side_ || *side_ != disclosure_.observer) {
        if (error)
            *error = "legal-dot observation is private to the side to move";
        return false;
    }
    std::vector<std::string> expected = supplied;
    std::sort(expected.begin(), expected.end());
    expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
    for (const std::string& marker : expected) {
        if (marker == "pass")
            continue;
        const std::size_t separator = marker.find('>');
        if (separator == std::string::npos ||
            marker.find('>', separator + 1) != std::string::npos ||
            Position::square_from_name(marker.substr(0, separator)) < 0 ||
            Position::square_from_name(marker.substr(separator + 1)) < 0) {
            if (error)
                *error = "legal-dot marker must be source>destination or pass";
            return false;
        }
    }

    std::map<std::string, Position> retained;
    std::size_t matchingCells = 0;
    for (const BeliefDecisionBucket& cell : decision_cells()) {
        std::vector<std::string> actual;
        for (const Move& move : cell.worlds.front().legal_moves()) {
            if (move.kind == MoveKind::Pass)
                actual.emplace_back("pass");
            else
                actual.push_back(Position::square_name(move.from) + ">" +
                                 Position::square_name(move.to));
        }
        std::sort(actual.begin(), actual.end());
        actual.erase(std::unique(actual.begin(), actual.end()), actual.end());
        if (actual != expected)
            continue;
        ++matchingCells;
        for (const Position& world : cell.worlds)
            retained.emplace(world.upn(), world);
    }
    if (matchingCells != 1 || retained.empty()) {
        if (error)
            *error = matchingCells
              ? "legal-dot markers ambiguously match multiple decision cells"
              : "legal-dot markers match no retained decision cell";
        return false;
    }
    worlds_ = std::move(retained);
    return true;
}

BeliefSuccessorPartitions PublicBeliefState::successor_partitions(
  std::string_view moveText, bool includeDecisionObservation) const {
    BeliefSuccessorPartitions result;
    result.before = worlds_.size();
    std::map<std::string, std::map<std::string, Position>> observations;
    for (const auto& [upn, before] : worlds_) {
        (void)upn;
        const std::optional<Move> move = unique_notated_move(before, moveText);
        if (!move) {
            ++result.incompatible;
            continue;
        }
        Position after = before;
        Undo undo;
        if (!after.make_move(*move, undo)) {
            ++result.incompatible;
            continue;
        }
        std::string observation = transition_observation_key(
          before, *move, after, disclosure_);
        if (includeDecisionObservation && !after.game_over() &&
            after.side_to_move() == disclosure_.observer) {
            const std::string decision = decision_observation_key(
              after, disclosure_);
            observation += "|nextDecision=" +
                           std::to_string(decision.size()) + ':' + decision;
        }
        observations[observation].emplace(after.upn(), std::move(after));
    }
    result.buckets.reserve(observations.size());
    for (auto& [observation, worlds] : observations) {
        BeliefSuccessorBucket bucket;
        bucket.observation = std::move(observation);
        bucket.worlds.reserve(worlds.size());
        for (auto& [upn, position] : worlds) {
            (void)upn;
            bucket.worlds.push_back(std::move(position));
        }
        result.buckets.push_back(std::move(bucket));
    }
    return result;
}

BeliefTransitionResult PublicBeliefState::apply_known(
  std::string_view moveText, std::string* error) {
    BeliefTransitionResult result;
    const BeliefSuccessorPartitions partitions = successor_partitions(moveText);
    result.before = partitions.before;
    result.observations = partitions.buckets.size();
    if (worlds_.empty()) {
        if (error)
            *error = "cannot apply an action to an empty belief";
        return result;
    }
    if (partitions.incompatible) {
        if (error)
            *error = "known action is incompatible with " +
                     std::to_string(partitions.incompatible) + " of " +
                     std::to_string(worlds_.size()) + " retained worlds";
        return result;
    }
    if (partitions.buckets.empty()) {
        if (error)
            *error = "known action is inconsistent with every retained world";
        return result;
    }
    if (partitions.buckets.size() != 1) {
        if (error)
            *error = "known action has " +
                     std::to_string(partitions.buckets.size()) +
                     " distinguishable public outcomes";
        return result;
    }

    worlds_.clear();
    for (const Position& position : partitions.buckets.front().worlds)
        worlds_.emplace(position.upn(), position);
    side_ = worlds_.begin()->second.side_to_move();
    publicView_ = view_key(worlds_.begin()->second, disclosure_);
    result.after = worlds_.size();
    result.applied = true;
    return result;
}

Search::Search(std::size_t hashMegabytes) {
    const std::size_t bytes = std::max<std::size_t>(1, hashMegabytes) * 1024 * 1024;
    std::size_t entries = 1;
    while ((entries << 1) * sizeof(Cluster) <= bytes)
        entries <<= 1;
    table_.resize(entries);
}

void Search::clear() {
    std::fill(table_.begin(), table_.end(), Cluster{});
    history_ = {};
    killers_ = {};
    generation_ = 0;
}

Search::Entry* Search::find_entry(std::uint64_t key) {
    Cluster& cluster = table_[key & (table_.size() - 1)];
    for (Entry& entry : cluster.entries)
        if (entry.bound != Bound::None && entry.key == key)
            return &entry;
    return nullptr;
}

Search::Entry& Search::replacement_entry(std::uint64_t key) {
    Cluster& cluster = table_[key & (table_.size() - 1)];
    for (Entry& entry : cluster.entries)
        if (entry.bound == Bound::None || entry.key == key)
            return entry;
    return *std::max_element(cluster.entries.begin(), cluster.entries.end(),
      [this](const Entry& lhs, const Entry& rhs) {
          const int lhsAge = static_cast<std::uint8_t>(generation_ - lhs.generation);
          const int rhsAge = static_cast<std::uint8_t>(generation_ - rhs.generation);
          return lhsAge * 256 - lhs.depth < rhsAge * 256 - rhs.depth;
      });
}

int Search::evaluate(const Position& position, int ply) const {
    int score = position.handcrafted_evaluate();
    if (useNnue_) {
#ifdef ULTIMATE_NNUE_REFRESH_EVERY_EVAL
        (void) ply;
        score += *UltimateNnue::evaluate(position);
#else
        score += UltimateNnue::correction(position, accumulators_[ply]);
#endif
    }
    return score;
}

bool Search::stopped() {
    if (stop_)
        return true;
    if (limits_.nodes && nodes_ >= limits_.nodes)
        return stop_ = true;
    // Ultimate nodes are substantially more expensive than orthodox chess
    // nodes because move legality can simulate Bomb blasts, Giant footprints,
    // and royal survival. Checking only every 1024 nodes overshot a 3-second
    // phone budget by almost two seconds in a measured Unranked position.
    // A 64-node cadence keeps the cutoff tight with negligible clock overhead.
    if (limits_.moveTime.count() && (nodes_ & 63) == 0 &&
        std::chrono::steady_clock::now() - start_ >= limits_.moveTime)
        return stop_ = true;
    return false;
}

int Search::move_score(const Position& position, const Move& move,
                       const Move* ttMove, int ply) const {
    if (ttMove && move == *ttMove)
        return 1'000'000;
    int score = 0;
    const bool capture = position.is_capture(move);
    if (capture) {
        const int victim = position.piece_on(move.to);
        const int attacker = position.piece_on(move.from);
        if (victim != Position::NoPiece)
            score += 100'000 + piece_order_value(position.piece(victim).type);
        const PieceType attackerType = attacker == Position::NoPiece ? PieceType::Count
                                                                    : position.piece(attacker).type;
        const bool linkedCapture = attackerType == PieceType::Checker ||
                                   attackerType == PieceType::CheckerKing ||
                                   attackerType == PieceType::Copycat ||
                                   attackerType == PieceType::CopycatClone;
        const int secondary = linkedCapture && move.auxiliary < Position::BoardSquares
                            ? position.piece_on(move.auxiliary) : Position::NoPiece;
        if (secondary != Position::NoPiece && secondary != victim)
            score += 100'000 + piece_order_value(position.piece(secondary).type);
        if (victim == Position::NoPiece && secondary == Position::NoPiece)
            score += 100'000;
        if (attacker != Position::NoPiece)
            score -= piece_order_value(position.piece(attacker).type) / 16;
#ifndef ULTIMATE_DISABLE_SEE_ORDERING
        if (const auto exchange = position.static_exchange(move))
            score += *exchange >= 0
                   ? 20'000 + std::min(*exchange, 2'000) * 4
                   : -150'000 + std::max(*exchange, -2'000) * 4;
#endif
    }
    if (move.promotion != PieceType::Count)
        score += 80'000 + piece_order_value(move.promotion);
    if (move.kind == MoveKind::Shoot)
        score += 90'000;
    if (move.kind == MoveKind::Pull)
        score += 8'000;
    if (move.kind == MoveKind::Swap)
        score += 6'000;
    if (move.kind == MoveKind::Link || move.kind == MoveKind::Spawn)
        score += 2'000;
    const int attacker = position.piece_on(move.from);
    if (attacker != Position::NoPiece && !capture) {
        if (ply < static_cast<int>(killers_.size()) && move == killers_[ply][0])
            score += 70'000;
        else if (ply < static_cast<int>(killers_.size()) && move == killers_[ply][1])
            score += 60'000;
        score += history_[static_cast<std::size_t>(position.piece(attacker).type)][move.to];
    }
    return score;
}

int Search::quiescence(Position& position, int alpha, int beta, int ply) {
    ++nodes_;
    if (stopped())
        return evaluate(position, ply);
    const Color side = position.side_to_move();
    if (const auto winner = position.forced_timeout_winner())
        return *winner == side ? Mate - ply : -Mate + ply;
    const bool ownKing = position.has_real_king(side);
    const bool enemyKing = position.has_real_king(~side);
    if (!ownKing || !enemyKing)
        return ownKing == enemyKing ? 0 : ownKing ? Mate - ply : -Mate + ply;
    if (!position.is_checkmate_possible())
        return 0;
    // Repeated checks, pulls, or other forcing actions can form a reversible
    // quiescence cycle. Never let such a line consume the native thread's
    // stack; the ordinary stand-pat cap below cannot apply while in check.
    if (ply >= MaxPly - 1)
        return evaluate(position, ply);
    if (ply > 0)
        if (const auto tablebase = TablebaseProbe::probe(position)) {
            if (tablebase->wdl == TablebaseWdl::Draw)
                return 0;
            const int distance = std::min<int>(tablebase->dtw, MateThreshold - 1);
            return tablebase->wdl == TablebaseWdl::Win
                 ? Mate - ply - distance : -Mate + ply + distance;
        }

    // A Checker jump or Prince follow-up is not optional. Likewise, when any
    // Checker has a capture the native rules suppress every quiet action. Do
    // not apply the usual quiescence "stand pat" assumption in those states.
    const bool inCheck = !position.pieces(side, PieceType::Jester) &&
                         position.real_king_threatened(side);
    const bool forced = position.has_forced_action() || inCheck;
    if (!forced) {
        const int standPat = evaluate(position, ply);
        if (standPat >= beta)
            return beta;
        alpha = std::max(alpha, standPat);
        if (ply >= 24)
            return alpha;
    }

    // Search orders pseudo-legal actions and validates each action on the
    // child it will actually search. The public legal_moves() path remains the
    // reference API, but using it here simulated every legal move twice and
    // every move beyond an early alpha-beta cutoff unnecessarily.
    auto moves = forced ? position.pseudo_legal_moves()
                        : position.pseudo_forcing_moves();
    position.annotate_captures(moves);
    if (moves.empty())
        return inCheck ? -Mate + ply : forced ? 0 : alpha;
    for (Move& move : moves)
        move.orderScore = move_score(position, move, nullptr, ply);
    std::sort(moves.begin(), moves.end(), [](const Move& lhs, const Move& rhs) {
        return lhs.orderScore > rhs.orderScore;
    });

    bool foundLegal = false;
    for (const Move& move : moves) {
#ifndef ULTIMATE_DISABLE_SEE_PRUNING
        // In a quiet ordinary position, do not extend a losing exchange into
        // quiescence. static_exchange() deliberately declines every Ultimate
        // action with blast, attachment, hidden-information, forced-action,
        // promotion, or other non-orthodox semantics; those actions always
        // retain the full reference search below.
        if (!forced)
            if (const auto exchange = position.static_exchange(move); exchange && *exchange < 0)
                continue;
#endif
        const Color before = position.side_to_move();
        // Search a disposable child. This copies Position once; mutating and
        // restoring the parent through the reference Undo path copied the
        // complete board/piece/bitboard state twice per searched edge.
        Position child = position;
        if (!child.apply_move_unchecked(move))
            continue;
        if (!child.legal_after_unchecked_move(side))
            continue;
        if (useNnue_)
            UltimateNnue::update(position, child, accumulators_[ply], accumulators_[ply + 1]);
        foundLegal = true;
        const bool sameSide = child.side_to_move() == before;
        const int score = sameSide ? quiescence(child, alpha, beta, ply + 1)
                                   : -quiescence(child, -beta, -alpha, ply + 1);
        if (stopped())
            return alpha;
        if (score >= beta)
            return beta;
        alpha = std::max(alpha, score);
    }
    if (!foundLegal)
        return inCheck ? -Mate + ply : forced ? 0 : alpha;
    return alpha;
}

int Search::negamax(Position& position, int depth, int alpha, int beta, int ply,
                    std::vector<Move>& pv, const Move* excludedMove) {
    pv.clear();
    if (stopped())
        return evaluate(position, ply);
    const Color side = position.side_to_move();
    if (const auto winner = position.forced_timeout_winner())
        return *winner == side ? Mate - ply : -Mate + ply;
    const bool ownKing = position.has_real_king(side);
    const bool enemyKing = position.has_real_king(~side);
    if (!ownKing || !enemyKing)
        return ownKing == enemyKing ? 0 : ownKing ? Mate - ply : -Mate + ply;
    if (!position.is_checkmate_possible())
        return 0;
    if (ply >= MaxPly - 1)
        return evaluate(position, ply);
    if (ply > 0)
        if (const auto tablebase = TablebaseProbe::probe(position)) {
            if (tablebase->wdl == TablebaseWdl::Draw)
                return 0;
            const int distance = std::min<int>(tablebase->dtw, MateThreshold - 1);
            return tablebase->wdl == TablebaseWdl::Win
                 ? Mate - ply - distance : -Mate + ply + distance;
        }
    if (depth <= 0)
        return quiescence(position, alpha, beta, ply);

    ++nodes_;
    const int originalAlpha = alpha;
    const bool pvNode = beta - alpha > 1;
    const std::uint64_t key = position.key();
    Entry* entry = find_entry(key);
    const bool restrictedRoot = ply == 0 && !rootMoves_.empty();
    const bool adjustedRoot = restrictedRoot || (ply == 0 && !rootDrawMoves_.empty());
    const bool adjustedNode = adjustedRoot || excludedMove;
    Move ttMove{};
    const Move* ttMovePtr = nullptr;
    if (entry && !adjustedNode) {
        ttMove = entry->move.unpack();
        ttMovePtr = &ttMove;
        if (entry->depth >= depth) {
            const int ttScore = score_from_tt(entry->score, ply);
            if (entry->bound == Bound::Exact) {
                pv.push_back(ttMove);
                return ttScore;
            }
            if (entry->bound == Bound::Lower && ttScore >= beta) {
                pv.push_back(ttMove);
                return ttScore;
            }
            if (entry->bound == Bound::Upper && ttScore <= alpha) {
                pv.push_back(ttMove);
                return ttScore;
            }
        }
    }

    auto moves = position.pseudo_legal_moves();
    position.annotate_captures(moves);
    if (restrictedRoot)
        moves.erase(std::remove_if(moves.begin(), moves.end(), [this](const Move& move) {
            return std::find(rootMoves_.begin(), rootMoves_.end(), move) == rootMoves_.end();
        }), moves.end());
    if (moves.empty())
        return position.real_king_threatened(side) ? -Mate + ply : 0;
    for (Move& move : moves)
        move.orderScore = move_score(position, move, ttMovePtr, ply);
    std::stable_sort(moves.begin(), moves.end(), [](const Move& lhs, const Move& rhs) {
        return lhs.orderScore > rhs.orderScore;
    });

#ifndef ULTIMATE_DISABLE_SINGULAR_EXTENSIONS
    bool singularTtMove = false;
    if (ply > 0 && depth >= 6 && entry && ttMovePtr &&
        entry->bound == Bound::Lower && entry->depth >= depth - 2 &&
        std::abs(score_from_tt(entry->score, ply)) < MateThreshold &&
        ttMove.kind == MoveKind::Normal &&
        ttMove.promotion == PieceType::Count &&
        position.supports_ordinary_exchange()) {
        const int ttScore = score_from_tt(entry->score, ply);
        const int singularBeta = ttScore - 2 * depth;
        std::vector<Move> exclusionPv;
        const int alternative = negamax(position, (depth - 1) / 2,
                                        singularBeta - 1, singularBeta, ply,
                                        exclusionPv, &ttMove);
        singularTtMove = !stopped() && alternative < singularBeta;
    }
#else
    constexpr bool singularTtMove = false;
#endif

    int bestScore = -Infinity;
    Move bestMove{};
    std::vector<Move> childPv;
    int moveNumber = 0;
    const bool quietPruningNode = ply > 0 && !pvNode && depth <= 3 &&
      std::abs(alpha) < MateThreshold && !position.has_forced_action() &&
      position.supports_ordinary_exchange() &&
      (position.pieces(side, PieceType::Jester) ||
       !position.real_king_threatened(side));
    const int staticEval = quietPruningNode ? evaluate(position, ply) : 0;
    for (const Move& move : moves) {
        if (excludedMove && move == *excludedMove)
            continue;
        const Color before = position.side_to_move();
        // Special actions can relocate multiple pieces, create material, or
        // trigger a Giant collision. Treat only ordinary non-captures as LMR
        // candidates; reducing those actions was a large tactical blind spot.
        const bool capture = position.is_capture(move);
        const bool quiet = move.kind == MoveKind::Normal && !capture;
        const int attacker = position.piece_on(move.from);
        int score;
        const bool rootDraw = ply == 0 &&
          std::find(rootDrawMoves_.begin(), rootDrawMoves_.end(), move) != rootDrawMoves_.end();
        if (rootDraw) {
            score = 0;
            childPv.clear();
        } else {
            Position child = position;
            // Apply once, validate the resulting child, and search that same
            // child. This replaces the former legal-frontier pass plus a
            // duplicate application of every action actually searched.
            if (!child.apply_move_unchecked(move))
                continue;
            if (!child.legal_after_unchecked_move(side))
                continue;
#ifndef ULTIMATE_DISABLE_LATE_MOVE_PRUNING
            if (quietPruningNode && quiet && depth <= 2 &&
                moveNumber >= 6 + 5 * depth) {
                ++moveNumber;
                continue;
            }
#endif
#ifndef ULTIMATE_DISABLE_FORWARD_FUTILITY
            if (quietPruningNode && quiet && moveNumber >= 4 + 3 * depth &&
                staticEval + 140 * depth <= alpha) {
                ++moveNumber;
                continue;
            }
#endif
            if (useNnue_)
                UltimateNnue::update(position, child, accumulators_[ply],
                                     accumulators_[ply + 1]);
            const bool sameSide = child.side_to_move() == before;
            int nextDepth = depth - (sameSide ? 0 : 1);
            // Same-side Checker/Prince actions already retain the current
            // nominal turn depth. Extend a singular TT action only when it
            // actually hands the turn to the opponent.
            if (!sameSide && singularTtMove && move == ttMove)
                ++nextDepth;
            int reduction = 0;
            if (depth >= 3 && moveNumber >= 4 && quiet && !sameSide) {
                reduction = 1;
                // A one-ply reduction leaves the very broad late quiet tail
                // almost unpruned at Ultimate depths. Increase it only after
                // several ordered alternatives have failed, and always
                // re-search a move which raises alpha below. Special actions,
                // captures, and same-side continuations remain unreduced.
#ifndef ULTIMATE_CONSERVATIVE_LMR
                if (depth >= 6 && moveNumber >= 8)
                    ++reduction;
                if (depth >= 9 && moveNumber >= 16)
                    ++reduction;
#endif
                reduction = std::min(reduction, std::max(0, nextDepth - 1));
            }
            if (moveNumber == 0) {
                score = sameSide ? negamax(child, nextDepth, alpha, beta, ply + 1, childPv)
                                 : -negamax(child, nextDepth, -beta, -alpha, ply + 1, childPv);
            }
            else if (sameSide) {
                score = negamax(child, nextDepth, alpha, alpha + 1, ply + 1, childPv);
                if (score > alpha && score < beta)
                    score = negamax(child, nextDepth, alpha, beta, ply + 1, childPv);
            }
            else {
                score = -negamax(child, nextDepth - reduction, -alpha - 1, -alpha, ply + 1, childPv);
                if (reduction && score > alpha)
                    score = -negamax(child, nextDepth, -alpha - 1, -alpha, ply + 1, childPv);
                if (score > alpha && score < beta)
                    score = -negamax(child, nextDepth, -beta, -alpha, ply + 1, childPv);
            }
        }
        ++moveNumber;
        if (stopped())
            break;
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
            pv.assign(1, move);
            pv.insert(pv.end(), childPv.begin(), childPv.end());
        }
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            if (quiet && attacker != Position::NoPiece) {
                if (ply < static_cast<int>(killers_.size()) && !(move == killers_[ply][0])) {
                    killers_[ply][1] = killers_[ply][0];
                    killers_[ply][0] = move;
                }
                int& value = history_[static_cast<std::size_t>(position.piece(attacker).type)][move.to];
                value = std::min(50'000, value + depth * depth);
            }
            break;
        }
    }

    if (bestScore == -Infinity)
        return stop_ ? evaluate(position, ply)
                     : position.real_king_threatened(side) ? -Mate + ply : 0;

    if (!stop_ && !adjustedNode &&
        (!entry || depth >= entry->depth || entry->generation != generation_)) {
        Entry& replacement = replacement_entry(key);
        replacement.key = key;
        replacement.move = bestMove;
        replacement.score = static_cast<std::int16_t>(
          std::clamp(score_to_tt(bestScore, ply), -Infinity, Infinity));
        replacement.depth = static_cast<std::int8_t>(std::min(depth, 127));
        replacement.bound = bestScore <= originalAlpha ? Bound::Upper
                          : bestScore >= beta          ? Bound::Lower
                                                       : Bound::Exact;
        replacement.generation = generation_;
    }
    return bestScore;
}

SearchResult Search::think(Position& position, const SearchLimits& limits) {
    limits_ = limits;
    softTime_ = std::chrono::milliseconds{0};
    if (!limits_.moveTime.count() && limits_.remainingTime.count()) {
        const auto usable = std::max(
          std::chrono::milliseconds{1}, limits_.remainingTime - limits_.moveOverhead);
        const int moves = limits_.movesToGo > 0 ? limits_.movesToGo : 30;
        softTime_ = std::min(usable, usable / moves + limits_.increment * 3 / 4);
        limits_.moveTime = std::min(
          usable, std::max(softTime_ * 4, softTime_ + std::chrono::milliseconds{50}));
    }
    rootMoves_ = limits.rootMoves;
    rootDrawMoves_.clear();
    for (const std::string& notation : limits.rootDrawMoveStrings)
        if (const auto move = position.move_from_string(notation))
            rootDrawMoves_.push_back(*move);
    start_ = std::chrono::steady_clock::now();
    nodes_ = 0;
    stop_ = false;
    useNnue_ = UltimateNnue::enabled();
    if (useNnue_)
        UltimateNnue::refresh(position, accumulators_[0]);
    ++generation_;
    for (auto& bySquare : history_)
        for (int& value : bySquare)
            value /= 2;

    SearchResult result;
    // An unrestricted concrete root can retain the tablebase's exact WDL/DTW
    // independently of the numeric search score. Searching one iteration is
    // still necessary to select the DTW-optimal legal root action.
    const auto rootTablebase = rootMoves_.empty() && rootDrawMoves_.empty()
                             ? TablebaseProbe::probe(position)
                             : std::nullopt;
    const int maxDepth = std::clamp(limits.depth, 1, MaxPly - 2);
    int previousScore = 0;
    std::optional<Move> previousBest;
    int stableBest = 0;
    for (int depth = 1; depth <= maxDepth; ++depth) {
        const auto iterationStart = std::chrono::steady_clock::now();
        std::vector<Move> pv;
        // At deeper Ultimate plies, whole-character swings make the narrow
        // probe fail often enough that its work is a net loss before the
        // mandatory full-window re-search. Keep aspiration where it is stable
        // and start directly with the exact window from depth eight onward.
        const bool aspirate = depth >= 3 && depth < 8;
        int alpha = aspirate ? std::max(-Infinity, previousScore - 60) : -Infinity;
        int beta = aspirate ? std::min(Infinity, previousScore + 60) : Infinity;
        int score = negamax(position, depth, alpha, beta, 0, pv);
        if (!stop_ && (score <= alpha || score >= beta)) {
            pv.clear();
            score = negamax(position, depth, -Infinity, Infinity, 0, pv);
        }
        if (stop_)
            break;
        const int scoreChange = std::abs(score - previousScore);
        previousScore = score;
        result.score = score;
        result.completedDepth = depth;
        result.principalVariation = pv;
        if (!pv.empty())
            result.bestMove = pv.front();
        if (rootTablebase && rootTablebase->wdl != TablebaseWdl::Draw) {
            const int distance = static_cast<int>(rootTablebase->dtw);
            result.mateActions = rootTablebase->wdl == TablebaseWdl::Win
                               ? distance : -distance;
        }
        result.nodes = nodes_;
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start_);
        if (limits_.onIteration)
            limits_.onIteration(result);
        if (rootTablebase)
            break;
        if (std::abs(score) >= Mate - 128)
            break;
        if (result.bestMove && previousBest && *result.bestMove == *previousBest)
            ++stableBest;
        else
            stableBest = 0;
        previousBest = result.bestMove;
        if (softTime_.count() && depth >= 4) {
            int scale = stableBest >= 3 ? 65 : stableBest >= 2 ? 80 : 115;
            if (scoreChange > 100)
                scale = std::max(scale, 140);
            const auto elapsed = std::chrono::steady_clock::now() - start_;
            const auto iterationElapsed = std::chrono::steady_clock::now() - iterationStart;
            const auto softDeadline = softTime_ * scale / 100;
            if (elapsed >= softDeadline ||
                (depth >= 5 && elapsed + iterationElapsed * 2 >= softDeadline))
                break;
        }
    }
    result.nodes = nodes_;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_);
    return result;
}

BeliefSearchResult Search::think_beliefs(const PublicBeliefState& beliefs,
                                         const SearchLimits& limits,
                                         std::size_t maximumDeepBeliefs,
                                         std::size_t maximumCandidates,
                                         bool legalDotObservations) {
    if (beliefs.side_to_move() &&
        *beliefs.side_to_move() == beliefs.disclosure().observer &&
        beliefs.decision_partitions() != 1 && legalDotObservations) {
        BeliefSearchResult result;
        result.beliefs = beliefs.size();
        result.validInformationCell = false;
        return result;
    }
    const auto beliefStart = std::chrono::steady_clock::now();
    BeliefSearchResult result;
    result.beliefs = beliefs.size();
    if (beliefs.empty() || !beliefs.side_to_move())
        return result;

    // A singleton has no information-set branching. Retain the mature native
    // search (including quiescence/tablebases) and report the whole completed
    // principal variation as history preserving.
    if (beliefs.size() == 1) {
        Position position = beliefs.positions().front();
        SearchResult exact = think(position, limits);
        result.bestMove = exact.bestMove
                        ? std::optional<std::string>(
                            position.move_to_string(*exact.bestMove))
                        : std::nullopt;
        result.score = result.worstScore = result.meanScore = exact.score;
        result.mateActions = exact.mateActions;
        result.completedDepth = exact.completedDepth;
        result.historyPreservingPlies = exact.completedDepth;
        result.nodes = exact.nodes;
        result.deepBeliefs = 1;
        result.commonMoves = position.legal_moves().size();
        result.candidates = result.commonMoves;
        for (const Move& move : exact.principalVariation)
            result.principalVariation.push_back(position.move_to_string(move));
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - beliefStart);
        return result;
    }

    limits_ = limits;
    start_ = beliefStart;
    nodes_ = 0;
    stop_ = false;
    // NNUE accumulators are concrete-path state. Until a belief accumulator is
    // introduced, use the exact handcrafted evaluator at information leaves.
    useNnue_ = false;
    const Color observer = beliefs.disclosure().observer;
    std::set<std::string> rootRestriction;
    if (!limits.rootMoves.empty()) {
        const Position& reference = beliefs.concrete_worlds().begin()->second;
        for (const Move& move : limits.rootMoves)
            rootRestriction.insert(reference.move_to_string(move));
    }
    const std::set<std::string> rootDraws(
      limits.rootDrawMoveStrings.begin(), limits.rootDrawMoveStrings.end());
    const auto observer_evaluate = [&](const PublicBeliefState& state, int ply) {
        int robust = Infinity;
        for (const auto& [upn, position] : state.concrete_worlds()) {
            (void)upn;
            int score = 0;
            const Color mover = position.side_to_move();
            if (const auto winner = position.forced_timeout_winner())
                score = *winner == mover ? Mate - ply : -Mate + ply;
            else {
                const bool ownKing = position.has_real_king(mover);
                const bool enemyKing = position.has_real_king(~mover);
                if (!ownKing || !enemyKing)
                    score = ownKing == enemyKing ? 0
                          : ownKing ? Mate - ply : -Mate + ply;
                else if (!position.is_checkmate_possible())
                    score = 0;
                else
                    score = position.handcrafted_evaluate();
            }
            if (mover != observer)
                score = -score;
            robust = std::min(robust, score);
        }
        return robust == Infinity ? 0 : robust;
    };

    using BeliefPv = std::vector<std::string>;
    std::function<int(const PublicBeliefState&, int, int, int, int, BeliefPv&)>
      solve = [&](const PublicBeliefState& state, int depth, int alpha, int beta,
                  int ply, BeliefPv& pv) -> int {
        nodes_ += std::max<std::size_t>(1, state.size());
        if (stopped() || depth <= 0 || ply >= MaxPly - 1)
            return observer_evaluate(state, ply);
        const std::optional<Color> side = state.side_to_move();
        if (!side)
            return 0;

        std::set<std::string> actionSet;
        if (*side == observer) {
            const std::vector<std::string> common = state.common_moves();
            actionSet.insert(common.begin(), common.end());
        }
        else {
            std::set<std::string> ambiguous;
            for (const auto& [upn, world] : state.concrete_worlds()) {
                (void)upn;
                std::map<std::string, std::size_t> counts;
                for (const Move& move : world.legal_moves())
                    ++counts[world.move_to_string(move)];
                for (const auto& [notation, count] : counts) {
                    if (count == 1)
                        actionSet.insert(notation);
                    else
                        ambiguous.insert(notation);
                }
            }
            for (const std::string& notation : ambiguous)
                actionSet.erase(notation);
        }
        if (ply == 0 && !rootRestriction.empty()) {
            std::set<std::string> restricted;
            std::set_intersection(
              actionSet.begin(), actionSet.end(),
              rootRestriction.begin(), rootRestriction.end(),
              std::inserter(restricted, restricted.begin()));
            actionSet = std::move(restricted);
        }
        if (actionSet.empty())
            return observer_evaluate(state, ply);

        const bool maximizing = *side == observer;
        int best = maximizing ? -Infinity : Infinity;
        BeliefPv bestPv;
        for (const std::string& action : actionSet) {
            int actionWorst = ply == 0 && rootDraws.count(action)
                            ? 0 : Infinity;
            BeliefPv actionPv;
            if (actionWorst == Infinity) {
                const BeliefSuccessorPartitions partitions =
                  state.successor_partitions(action, legalDotObservations);
                if (partitions.buckets.empty() ||
                    (maximizing && partitions.incompatible))
                    continue;

                // The concrete action is public, so an opponent action may
                // itself eliminate worlds where that action was illegal. For
                // either mover, nature then selects the worst compatible
                // public/private observation bucket. Future strategy branches
                // only after that bucket has actually been observed.
                for (const BeliefSuccessorBucket& bucket : partitions.buckets) {
                    PublicBeliefState child(state.disclosure());
                    std::string error;
                    bool valid = true;
                    for (const Position& world : bucket.worlds)
                        valid = valid && child.add(world, &error);
                    if (!valid || child.empty())
                        continue;
                    const bool changedSide = child.side_to_move() &&
                                             *child.side_to_move() != *side;
                    BeliefPv childPv;
                    const int score = solve(
                      child, depth - (changedSide ? 1 : 0),
                      alpha, beta, ply + 1, childPv);
                    if (score < actionWorst) {
                        actionWorst = score;
                        actionPv = std::move(childPv);
                    }
                    if (actionWorst <= alpha || stopped())
                        break;
                }
            }
            if (actionWorst == Infinity)
                continue;
            if ((maximizing && actionWorst > best) ||
                (!maximizing && actionWorst < best)) {
                best = actionWorst;
                bestPv.assign(1, action);
                bestPv.insert(bestPv.end(), actionPv.begin(), actionPv.end());
            }
            if (maximizing)
                alpha = std::max(alpha, best);
            else
                beta = std::min(beta, best);
            if (alpha >= beta || stopped())
                break;
        }
        if (best == (maximizing ? -Infinity : Infinity))
            return observer_evaluate(state, ply);
        pv = std::move(bestPv);
        return best;
    };

    const int maxDepth = std::clamp(limits.depth, 1, MaxPly - 2);
    for (int depth = 1; depth <= maxDepth; ++depth) {
        BeliefPv pv;
        const int score = solve(beliefs, depth, -Infinity, Infinity, 0, pv);
        if (stop_)
            break;
        result.score = result.worstScore = result.meanScore = score;
        result.completedDepth = depth;
        result.historyPreservingPlies = depth;
        result.principalVariation = std::move(pv);
        result.bestMove = result.principalVariation.empty()
                        ? std::nullopt
                        : std::optional<std::string>(
                            result.principalVariation.front());
    }
    result.nodes = nodes_;
    result.deepBeliefs = beliefs.size();
    result.commonMoves = beliefs.common_moves().size();
    result.candidates = result.commonMoves;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - beliefStart);
    (void)maximumDeepBeliefs;
    (void)maximumCandidates;
    return result;
}

BeliefSearchResult Search::think_beliefs(const std::vector<Position>& beliefs,
                                         const SearchLimits& limits,
                                         std::size_t maximumDeepBeliefs,
                                         std::size_t maximumCandidates) {
    const auto beliefStart = std::chrono::steady_clock::now();
    BeliefSearchResult result;
    result.beliefs = beliefs.size();
    if (beliefs.empty())
        return result;

    const Color side = beliefs.front().side_to_move();
    for (const Position& belief : beliefs)
        if (belief.side_to_move() != side)
            return result;

    std::set<std::string> common;
    for (const Move& move : beliefs.front().legal_moves())
        common.insert(beliefs.front().move_to_string(move));
    for (std::size_t index = 1; index < beliefs.size() && !common.empty(); ++index) {
        std::set<std::string> legal;
        for (const Move& move : beliefs[index].legal_moves())
            legal.insert(beliefs[index].move_to_string(move));
        std::set<std::string> intersection;
        std::set_intersection(common.begin(), common.end(), legal.begin(), legal.end(),
                              std::inserter(intersection, intersection.begin()));
        common = std::move(intersection);
    }
    const std::vector<std::string> roots(common.begin(), common.end());
    result.commonMoves = roots.size();
    if (roots.empty())
        return result;

    // Exact information should retain the normal engine's full root search.
    if (beliefs.size() == 1) {
        Position position = beliefs.front();
        SearchResult exact = think(position, limits);
        result.bestMove = exact.bestMove
                        ? std::optional<std::string>(position.move_to_string(*exact.bestMove))
                        : std::nullopt;
        result.score = result.worstScore = result.meanScore = exact.score;
        result.mateActions = exact.mateActions;
        result.completedDepth = exact.completedDepth;
        result.nodes = exact.nodes;
        result.deepBeliefs = 1;
        result.candidates = roots.size();
        for (const Move& move : exact.principalVariation)
            result.principalVariation.push_back(position.move_to_string(move));
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - beliefStart);
        return result;
    }

    // First audit every common action in every retained world. A depth-one
    // root search enters quiescence after our action, so hidden-Ghost
    // recaptures and every other forcing reply are still resolved. Keeping
    // this pass at one full turn leaves materially more of a phone clock for
    // deep search while preserving complete uncertainty-set coverage.
    const int shallowDepth = 1;
    std::vector<std::vector<int>> shallowScores(
      beliefs.size(), std::vector<int>(roots.size(), -Infinity));
    const std::size_t workerCount = std::min<std::size_t>(8, beliefs.size());
    std::vector<std::future<std::uint64_t>> shallowWorkers;
    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        shallowWorkers.push_back(std::async(std::launch::async, [&, worker] {
            Search local(4);
            std::uint64_t workerNodes = 0;
            for (std::size_t beliefIndex = worker; beliefIndex < beliefs.size();
                 beliefIndex += workerCount) {
                for (std::size_t rootIndex = 0; rootIndex < roots.size(); ++rootIndex) {
                    Position position = beliefs[beliefIndex];
                    const auto move = position.move_from_string(roots[rootIndex]);
                    if (!move)
                        continue;
                    SearchLimits audit;
                    audit.depth = shallowDepth;
                    audit.rootMoves = {*move};
                    const SearchResult searched = local.think(position, audit);
                    workerNodes += searched.nodes;
                    if (searched.bestMove)
                        shallowScores[beliefIndex][rootIndex] = searched.score;
                }
            }
            return workerNodes;
        }));
    }
    for (auto& worker : shallowWorkers)
        result.nodes += worker.get();

    const auto shallowElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - beliefStart);

    struct Summary {
        std::size_t root = 0;
        int worst = -Infinity;
        int mean = -Infinity;
        std::size_t worstBelief = 0;
    };
    std::vector<Summary> summaries;
    summaries.reserve(roots.size());
    for (std::size_t root = 0; root < roots.size(); ++root) {
        int worst = Infinity;
        std::size_t worstBelief = 0;
        std::int64_t total = 0;
        for (std::size_t belief = 0; belief < beliefs.size(); ++belief) {
            if (shallowScores[belief][root] < worst) {
                worst = shallowScores[belief][root];
                worstBelief = belief;
            }
            total += shallowScores[belief][root];
        }
        summaries.push_back({root, worst,
                             static_cast<int>(std::llround(
                               static_cast<double>(total) / beliefs.size())),
                             worstBelief});
    }
    std::stable_sort(summaries.begin(), summaries.end(), [&](const Summary& lhs,
                                                              const Summary& rhs) {
        if (lhs.worst != rhs.worst)
            return lhs.worst > rhs.worst;
        if (lhs.mean != rhs.mean)
            return lhs.mean > rhs.mean;
        return roots[lhs.root] < roots[rhs.root];
    });
    const std::size_t candidateCount = std::min(
      std::max<std::size_t>(1, maximumCandidates), summaries.size());
    summaries.resize(candidateCount);
    result.candidates = candidateCount;

    // The exhaustive public-information audit is part of the caller's move
    // budget, not a surcharge on top of it.  If it used the whole budget, its
    // complete-set result is still a safe move choice; return that instead of
    // starting deep searches which are guaranteed to overrun the clock.
    if (limits.moveTime.count() && shallowElapsed >= limits.moveTime) {
        const Summary& best = summaries.front();
        result.bestMove = roots[best.root];
        result.score = result.worstScore = best.worst;
        result.meanScore = best.mean;
        result.completedDepth = shallowDepth;
        result.elapsed = shallowElapsed;
        return result;
    }

    // Deep search evenly spaced representative worlds plus each candidate's
    // own worst shallow world. This preserves the complete-set Ghost safety
    // audit without capping a depth-six result at a depth-two evaluation.
    const std::size_t deepCount = std::min(
      std::max<std::size_t>(1, maximumDeepBeliefs), beliefs.size());
    result.deepBeliefs = deepCount;
    std::vector<std::size_t> representatives;
    representatives.reserve(deepCount);
    for (std::size_t index = 0; index < deepCount; ++index)
        representatives.push_back(deepCount == 1 ? 0
          : (index * (beliefs.size() - 1) + (deepCount - 1) / 2) / (deepCount - 1));

    struct DeepRow {
        std::vector<SearchResult> searches;
        std::vector<std::vector<std::string>> pvs;
    };
    std::vector<std::future<DeepRow>> deepWorkers;
    for (std::size_t candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
        deepWorkers.push_back(std::async(std::launch::async, [&, candidateIndex] {
            Search local(8);
            DeepRow row;
            row.searches.reserve(deepCount);
            row.pvs.reserve(deepCount);
            std::vector<std::size_t> candidateBeliefs = representatives;
            const std::size_t worstBelief = summaries[candidateIndex].worstBelief;
            if (std::find(candidateBeliefs.begin(), candidateBeliefs.end(), worstBelief)
                == candidateBeliefs.end())
                candidateBeliefs.back() = worstBelief;
            for (const std::size_t beliefIndex : candidateBeliefs) {
                Position position = beliefs[beliefIndex];
                SearchLimits deep = limits;
                deep.rootMoves.clear();
                const auto move = position.move_from_string(
                  roots[summaries[candidateIndex].root]);
                if (!move) {
                    row.searches.push_back({});
                    row.pvs.emplace_back();
                    continue;
                }
                deep.rootMoves.push_back(*move);
                if (deep.nodes)
                    deep.nodes = std::max<std::uint64_t>(1, deep.nodes / deepCount);
                if (deep.moveTime.count()) {
                    const auto remaining = deep.moveTime - shallowElapsed;
                    deep.moveTime = std::chrono::milliseconds(std::max<std::int64_t>(
                      1, remaining.count() / static_cast<std::int64_t>(deepCount)));
                }
                SearchResult searched = local.think(position, deep);
                std::vector<std::string> pv;
                for (const Move& pvMove : searched.principalVariation)
                    pv.push_back(position.move_to_string(pvMove));
                row.searches.push_back(std::move(searched));
                row.pvs.push_back(std::move(pv));
            }
            return row;
        }));
    }
    std::vector<DeepRow> deepRows;
    deepRows.reserve(candidateCount);
    for (auto& worker : deepWorkers)
        deepRows.push_back(worker.get());

    struct DeepSummary {
        std::size_t candidate = 0;
        int robust = -Infinity;
        int mean = -Infinity;
    };
    std::vector<DeepSummary> deepSummaries;
    for (std::size_t candidate = 0; candidate < candidateCount; ++candidate) {
        int deepWorst = Infinity;
        std::int64_t total = 0;
        const DeepRow& row = deepRows[candidate];
        for (const SearchResult& searched : row.searches) {
            const int score = searched.bestMove ? searched.score : -Infinity;
            deepWorst = std::min(deepWorst, score);
            total += score;
            result.nodes += searched.nodes;
        }
        deepSummaries.push_back({candidate,
          deepWorst,
          static_cast<int>(std::llround(static_cast<double>(total) / deepCount))});
    }
    std::stable_sort(deepSummaries.begin(), deepSummaries.end(), [&](const DeepSummary& lhs,
                                                                      const DeepSummary& rhs) {
        if (lhs.robust != rhs.robust)
            return lhs.robust > rhs.robust;
        if (summaries[lhs.candidate].worst != summaries[rhs.candidate].worst)
            return summaries[lhs.candidate].worst > summaries[rhs.candidate].worst;
        if (lhs.mean != rhs.mean)
            return lhs.mean > rhs.mean;
        const std::size_t lhsRoot = summaries[lhs.candidate].root;
        const std::size_t rhsRoot = summaries[rhs.candidate].root;
        return roots[lhsRoot] < roots[rhsRoot];
    });

    const DeepSummary& best = deepSummaries.front();
    const Summary& bestCandidate = summaries[best.candidate];
    result.bestMove = roots[bestCandidate.root];
    result.score = result.worstScore = best.robust;
    result.meanScore = best.mean;
    result.completedDepth = limits.depth;
    std::size_t pvBelief = 0;
    int pvScore = Infinity;
    const DeepRow& bestRow = deepRows[best.candidate];
    for (std::size_t belief = 0; belief < bestRow.searches.size(); ++belief) {
        const SearchResult& searched = bestRow.searches[belief];
        result.completedDepth = std::min(result.completedDepth, searched.completedDepth);
        if (searched.score < pvScore) {
            pvScore = searched.score;
            pvBelief = belief;
        }
    }
    result.principalVariation = bestRow.pvs[pvBelief];
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - beliefStart);
    return result;
}

}  // namespace Stockfish::Ultimate
