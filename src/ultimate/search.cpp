/*
  Ultimate Fish - search for Chess Ultimate
  GPLv3 or later
*/

#include "search.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

}  // namespace

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

bool Search::stopped() {
    if (stop_)
        return true;
    if (limits_.nodes && nodes_ >= limits_.nodes)
        return stop_ = true;
    if (limits_.moveTime.count() && (nodes_ & 1023) == 0 &&
        std::chrono::steady_clock::now() - start_ >= limits_.moveTime)
        return stop_ = true;
    return false;
}

int Search::move_score(const Position& position, const Move& move, const Move* ttMove, int ply) const {
    if (ttMove && move == *ttMove)
        return 1'000'000;
    int score = 0;
    if (position.is_capture(move)) {
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
    }
    if (move.promotion != PieceType::Count)
        score += 80'000 + piece_order_value(move.promotion);
    if (move.kind == MoveKind::Shoot)
        score += 90'000;
    if (move.kind == MoveKind::Link || move.kind == MoveKind::Spawn)
        score += 2'000;
    const int attacker = position.piece_on(move.from);
    if (attacker != Position::NoPiece && !position.is_capture(move)) {
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
        return position.evaluate();
    if (position.game_over()) {
        const auto winner = position.winner();
        if (!winner)
            return 0;
        return *winner == position.side_to_move() ? Mate - ply : -Mate + ply;
    }

    // A Checker jump or Prince follow-up is not optional. Likewise, when any
    // Checker has a capture the native rules suppress every quiet action. Do
    // not apply the usual quiescence "stand pat" assumption in those states.
    const bool forced = position.has_forced_action();
    if (!forced) {
        const int standPat = position.evaluate();
        if (standPat >= beta)
            return beta;
        alpha = std::max(alpha, standPat);
        if (ply >= 24)
            return alpha;
    }

    auto moves = position.legal_moves();
    if (!forced)
        moves.erase(std::remove_if(moves.begin(), moves.end(), [&position](const Move& move) {
                        return !position.is_capture(move);
                    }), moves.end());
    if (moves.empty())
        return forced ? position.evaluate() : alpha;
    std::sort(moves.begin(), moves.end(), [this, &position, ply](const Move& lhs, const Move& rhs) {
        return move_score(position, lhs, nullptr, ply) > move_score(position, rhs, nullptr, ply);
    });

    for (const Move& move : moves) {
        const Color before = position.side_to_move();
        Undo undo;
        if (!position.make_move(move, undo))
            continue;
        const bool sameSide = position.side_to_move() == before;
        const int score = sameSide ? quiescence(position, alpha, beta, ply + 1)
                                   : -quiescence(position, -beta, -alpha, ply + 1);
        position.undo_move(undo);
        if (stopped())
            return alpha;
        if (score >= beta)
            return beta;
        alpha = std::max(alpha, score);
    }
    return alpha;
}

int Search::negamax(Position& position, int depth, int alpha, int beta, int ply,
                    std::vector<Move>& pv) {
    pv.clear();
    if (stopped())
        return position.evaluate();
    if (position.game_over()) {
        const auto winner = position.winner();
        if (!winner)
            return 0;
        return *winner == position.side_to_move() ? Mate - ply : -Mate + ply;
    }
    if (depth <= 0)
        return quiescence(position, alpha, beta, ply);

    ++nodes_;
    const int originalAlpha = alpha;
    const std::uint64_t key = position.key();
    Entry* entry = find_entry(key);
    Move ttMove{};
    const Move* ttMovePtr = nullptr;
    if (entry) {
        ttMove = entry->move;
        ttMovePtr = &ttMove;
        if (entry->depth >= depth) {
            const int ttScore = score_from_tt(entry->score, ply);
            if (entry->bound == Bound::Exact) {
                pv.push_back(entry->move);
                return ttScore;
            }
            if (entry->bound == Bound::Lower && ttScore >= beta) {
                pv.push_back(entry->move);
                return ttScore;
            }
            if (entry->bound == Bound::Upper && ttScore <= alpha) {
                pv.push_back(entry->move);
                return ttScore;
            }
        }
    }

    auto moves = position.legal_moves();
    if (moves.empty())
        return position.evaluate();
    std::stable_sort(moves.begin(), moves.end(), [this, &position, ttMovePtr, ply](const Move& lhs,
                                                                             const Move& rhs) {
        return move_score(position, lhs, ttMovePtr, ply) > move_score(position, rhs, ttMovePtr, ply);
    });

    int bestScore = -Infinity;
    Move bestMove{};
    std::vector<Move> childPv;
    int moveNumber = 0;
    for (const Move& move : moves) {
        const Color before = position.side_to_move();
        const bool quiet = !position.is_capture(move);
        const int attacker = position.piece_on(move.from);
        Undo undo;
        if (!position.make_move(move, undo))
            continue;
        const bool sameSide = position.side_to_move() == before;
        const int nextDepth = depth - (sameSide ? 0 : 1);
        int score;
        const int reduction = depth >= 3 && moveNumber >= 4 && quiet && !sameSide ? 1 : 0;
        if (moveNumber == 0) {
            score = sameSide ? negamax(position, nextDepth, alpha, beta, ply + 1, childPv)
                             : -negamax(position, nextDepth, -beta, -alpha, ply + 1, childPv);
        }
        else if (sameSide) {
            score = negamax(position, nextDepth, alpha, alpha + 1, ply + 1, childPv);
            if (score > alpha && score < beta)
                score = negamax(position, nextDepth, alpha, beta, ply + 1, childPv);
        }
        else {
            score = -negamax(position, nextDepth - reduction, -alpha - 1, -alpha, ply + 1, childPv);
            if (reduction && score > alpha)
                score = -negamax(position, nextDepth, -alpha - 1, -alpha, ply + 1, childPv);
            if (score > alpha && score < beta)
                score = -negamax(position, nextDepth, -beta, -alpha, ply + 1, childPv);
        }
        position.undo_move(undo);
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

    if (!stop_ && bestScore != -Infinity &&
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
    return bestScore == -Infinity ? position.evaluate() : bestScore;
}

SearchResult Search::think(Position& position, const SearchLimits& limits) {
    limits_ = limits;
    start_ = std::chrono::steady_clock::now();
    nodes_ = 0;
    stop_ = false;
    ++generation_;
    for (auto& bySquare : history_)
        for (int& value : bySquare)
            value /= 2;

    SearchResult result;
    const int maxDepth = std::max(1, limits.depth);
    int previousScore = 0;
    for (int depth = 1; depth <= maxDepth; ++depth) {
        std::vector<Move> pv;
        int alpha = depth >= 3 ? std::max(-Infinity, previousScore - 60) : -Infinity;
        int beta = depth >= 3 ? std::min(Infinity, previousScore + 60) : Infinity;
        int score = negamax(position, depth, alpha, beta, 0, pv);
        if (!stop_ && (score <= alpha || score >= beta)) {
            pv.clear();
            score = negamax(position, depth, -Infinity, Infinity, 0, pv);
        }
        if (stop_)
            break;
        previousScore = score;
        result.score = score;
        result.completedDepth = depth;
        result.principalVariation = pv;
        if (!pv.empty())
            result.bestMove = pv.front();
        if (std::abs(score) >= Mate - 128)
            break;
    }
    result.nodes = nodes_;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_);
    return result;
}

}  // namespace Stockfish::Ultimate
