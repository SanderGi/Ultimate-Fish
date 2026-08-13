/*
  Ultimate Fish - search for Chess Ultimate
  GPLv3 or later
*/

#include "search.h"
#include "tablebases/tablebase_probe.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <future>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace Stockfish::Ultimate {
namespace {

constexpr int Infinity = 32000;
constexpr int Mate = 30000;
constexpr int MateThreshold = Mate - 128;

std::uint64_t mix_key(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

std::uint64_t string_key(std::string_view value) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

bool belief_stays_concrete(const Position& position, Color observer) {
    // Royal identity is monotone knowledge, but Ghost location is not: a
    // currently observed Ghost can make a quiet move, become invisible, and
    // expand one concrete world into several observer-equivalent worlds.
    const Color enemy = ~observer;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (piece.alive && piece.onBoard && piece.color == enemy &&
            piece.type == PieceType::Ghost)
            return false;
    }
    return true;
}

bool royal_assignment_equivalent(const Position& first,
                                 const Position& second,
                                 Color royalOwner) {
    if (first.piece_count() != second.piece_count())
        return false;
    int firstKing = Position::NoPiece;
    int secondKing = Position::NoPiece;
    int firstJesters = 0;
    int secondJesters = 0;
    for (int id = 0; id < first.piece_count(); ++id) {
        const PieceState& piece = first.piece(id);
        if (piece.alive && piece.type == PieceType::Ghost)
            return false;
        if (!piece.alive || piece.color != royalOwner)
            continue;
        if (piece.type == PieceType::King)
            firstKing = id;
        else if (piece.type == PieceType::Jester)
            ++firstJesters;
    }
    for (int id = 0; id < second.piece_count(); ++id) {
        const PieceState& piece = second.piece(id);
        if (!piece.alive || piece.color != royalOwner)
            continue;
        if (piece.type == PieceType::King)
            secondKing = id;
        else if (piece.type == PieceType::Jester)
            ++secondJesters;
    }
    if (firstKing == Position::NoPiece || secondKing == Position::NoPiece ||
        !firstJesters || !secondJesters)
        return false;
    Position swapped = first;
    if (firstKing != secondKing &&
        !swapped.swap_royal_roles(firstKing, secondKing))
        return false;
    return swapped.upn() == second.upn();
}

std::vector<std::string> decision_markers(const Position& position) {
    std::vector<std::string> result;
    for (const Move& move : position.legal_moves())
        result.push_back(move.kind == MoveKind::Pass
          ? "pass"
          : Position::square_name(move.from) + ">" +
            Position::square_name(move.to));
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::string observed_transition(const Position& before, const Move& move,
                                const Position& after,
                                const DisclosureContext& disclosure) {
    std::string publicView;
    std::string result = transition_observation_key(
      before, move, after, disclosure, &publicView);
    if (!after.game_over() && after.side_to_move() == disclosure.observer) {
        const std::string decision = decision_observation_key(
          after, disclosure, &publicView);
        result += "|nextDecision=" + std::to_string(decision.size()) + ':' +
                  decision;
    }
    return result;
}

bool rebuild_position(const Position& mutated, Position& rebuilt) {
    std::string ignored;
    return rebuilt.set_upn(mutated.upn(), &ignored);
}

bool initial_public_beliefs(const Position& actual,
                            const DisclosureContext& disclosure,
                            bool initialDeploymentKnown,
                            PublicBeliefState& beliefs,
                            std::string* error) {
    beliefs = PublicBeliefState(disclosure);
    const Color enemy = disclosure.observer == Color::White
                      ? Color::Black : Color::White;
    std::vector<int> hiddenGhosts;
    std::vector<int> enemyRoyals;
    for (int id = 0; id < actual.piece_count(); ++id) {
        const PieceState& piece = actual.piece(id);
        if (!piece.alive || !piece.onBoard || piece.color != enemy)
            continue;
        if (piece.type == PieceType::Ghost && !piece.visible)
            hiddenGhosts.push_back(id);
        if (piece.type == PieceType::King || piece.type == PieceType::Jester)
            enemyRoyals.push_back(id);
    }
    const auto ghostStateKey = [&](int id) {
        const PieceState& piece = actual.piece(id);
        return std::tie(piece.type, piece.color, piece.onBoard, piece.action,
                        piece.cooldown, piece.freezeCount, piece.power,
                        piece.link, piece.host, piece.attachmentOrder,
                        piece.alive, piece.moved, piece.visible);
    };
    std::stable_sort(hiddenGhosts.begin(), hiddenGhosts.end(),
      [&](int left, int right) {
          return ghostStateKey(left) < ghostStateKey(right);
      });

    const std::set<int> hiddenIds(hiddenGhosts.begin(), hiddenGhosts.end());
    std::vector<int> observerRoyals;
    for (int id = 0; id < actual.piece_count(); ++id) {
        const PieceState& piece = actual.piece(id);
        if (piece.alive && piece.onBoard &&
            piece.color == disclosure.observer &&
            (piece.type == PieceType::King ||
             piece.type == PieceType::Jester))
            observerRoyals.push_back(piece.square);
    }
    const auto wouldBeVisible = [&](int square) {
        const int file = square % Position::BoardFiles;
        const int rank = square / Position::BoardFiles;
        return std::any_of(observerRoyals.begin(), observerRoyals.end(),
          [&](int royalSquare) {
              const int royalFile = royalSquare % Position::BoardFiles;
              const int royalRank = royalSquare / Position::BoardFiles;
              return std::max(std::abs(file - royalFile),
                              std::abs(rank - royalRank)) <= 1;
          });
    };
    std::vector<int> ghostSquares;
    // A history root may be an arbitrary midgame snapshot rather than the
    // original deployment. Only a draft/deployment history proves that an
    // invisible Ghost is still inside its home zone at the root.
    const int firstRank = initialDeploymentKnown
                        ? (enemy == Color::White ? 1 : 8) : 1;
    const int lastRank = initialDeploymentKnown
                       ? (enemy == Color::White ? 3 : 10)
                       : Position::BoardRanks;
    for (int rank = firstRank; rank <= lastRank; ++rank)
        for (char file = 'a'; file <= 'h'; ++file) {
            const int square = Position::square_from_name(
              std::string(1, file) + std::to_string(rank));
            const int occupant = actual.piece_on(square);
            if ((occupant == Position::NoPiece || hiddenIds.count(occupant)) &&
                !wouldBeVisible(square))
                ghostSquares.push_back(square);
        }
    if (ghostSquares.size() < hiddenGhosts.size()) {
        if (error)
            *error = "not enough publicly possible cells for hidden Ghosts";
        return false;
    }

    const std::string expectedView = view_key(actual, disclosure);
    std::vector<int> kingCandidates;
    if (!disclosure.enemyKingKnown && enemyRoyals.size() > 1) {
        for (const int id : enemyRoyals)
            if (!disclosure.enemyRoyalCandidatesSpecified ||
                disclosure.is_enemy_royal_candidate(id))
                kingCandidates.push_back(id);
    }
    else
        kingCandidates.push_back(Position::NoPiece);

    if (kingCandidates.empty()) {
        if (error)
            *error = "no enemy royal matches the retained King candidates";
        return false;
    }

    const auto actualKing = std::find_if(
      enemyRoyals.begin(), enemyRoyals.end(), [&](int id) {
          return actual.piece(id).type == PieceType::King;
      });

    for (const int king : kingCandidates) {
        Position royalVariant = actual;
        bool royalNeedsRebuild = false;
        if (king != Position::NoPiece) {
            if (actualKing == enemyRoyals.end()) {
                for (const int id : enemyRoyals)
                    royalVariant.piece(id).type = id == king
                                                ? PieceType::King
                                                : PieceType::Jester;
                royalNeedsRebuild = true;
            }
            else if (king != *actualKing &&
                     !royalVariant.swap_royal_roles(*actualKing, king)) {
                if (error)
                    *error = "could not factor the enemy royal candidate set";
                return false;
            }
        }

        std::vector<bool> used(ghostSquares.size(), false);
        std::vector<std::size_t> assigned(hiddenGhosts.size(), 0);
        std::function<void(std::size_t, Position)> place =
          [&](std::size_t index, Position variant) {
            if (index == hiddenGhosts.size()) {
                Position candidate;
                std::string addError;
                const bool ready = hiddenGhosts.empty() && !royalNeedsRebuild
                  ? (candidate = std::move(variant), true)
                  : rebuild_position(variant, candidate);
                if (!ready ||
                    view_key(candidate, disclosure) != expectedView)
                    return;
                if (!beliefs.add(std::move(candidate), &addError) && error &&
                    error->empty())
                    *error = addError;
                return;
            }
            const bool sameAsPrevious = index > 0 &&
              ghostStateKey(hiddenGhosts[index]) ==
                ghostStateKey(hiddenGhosts[index - 1]);
            for (std::size_t squareIndex = 0;
                 squareIndex < ghostSquares.size(); ++squareIndex) {
                if (used[squareIndex] ||
                    (sameAsPrevious && squareIndex <= assigned[index - 1]))
                    continue;
                Position next = variant;
                next.piece(hiddenGhosts[index]).square =
                  ghostSquares[squareIndex];
                used[squareIndex] = true;
                assigned[index] = squareIndex;
                place(index + 1, std::move(next));
                used[squareIndex] = false;
            }
          };
        place(0, std::move(royalVariant));
    }

    if (beliefs.empty()) {
        if (error && error->empty())
            *error = "initial public view has no legal concrete worlds";
        return false;
    }
    if (!actual.game_over() && actual.side_to_move() == disclosure.observer) {
        std::string observationError;
        if (!beliefs.condition_on_decision_markers(
              decision_markers(actual), &observationError)) {
            if (error)
                *error = "initial legal-dot observation is inconsistent: " +
                         observationError;
            return false;
        }
    }
    return true;
}

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
    compactPublicView_.reset();
    worlds_.clear();
    legalMovesCache_.clear();
    worldKeyXor_ = 0;
    worldKeySum_ = 0;
    worldVerificationXor_ = 0;
    worldVerificationSum_ = 0;
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

std::uint64_t PublicBeliefState::transposition_key() const {
    std::uint64_t disclosure = static_cast<std::uint64_t>(disclosure_.observer);
    disclosure |= std::uint64_t(disclosure_.enemyKingKnown) << 8;
    disclosure |= std::uint64_t(disclosure_.enemyRoyalCandidatesSpecified) << 9;
    disclosure ^= mix_key(disclosure_.enemyRoyalCandidatesLow);
    disclosure ^= mix_key(disclosure_.enemyRoyalCandidatesHigh + 1);
    return mix_key(worldKeyXor_) ^ mix_key(worldKeySum_) ^
           mix_key(worlds_.size()) ^ mix_key(disclosure);
}

std::uint64_t PublicBeliefState::transposition_verification() const {
    return mix_key(worldVerificationXor_) ^ mix_key(worldVerificationSum_) ^
           mix_key(worlds_.size() + 0x7f4a7c15ULL);
}

bool PublicBeliefState::add(Position position, std::string* error) {
    const std::string view = view_key(position, disclosure_);
    std::string upn = position.upn();
    return add_prevalidated(std::move(upn), std::move(position), view, error);
}

bool PublicBeliefState::add_prevalidated(
  std::string upn, Position position, std::string_view publicView,
  std::string* error) {
    if (side_ && position.side_to_move() != *side_) {
        if (error)
            *error = "side-to-move differs from the retained belief";
        return false;
    }
    if (!publicView_.empty() && publicView != publicView_) {
        if (error)
            *error = "ordinary public view differs from the retained belief";
        return false;
    }
    const std::uint64_t positionKey = position.key();
    const auto [iterator, inserted] = worlds_.emplace(upn, std::move(position));
    if (inserted) {
        const std::uint64_t mixedPosition = mix_key(positionKey);
        const std::uint64_t mixedVerification = mix_key(string_key(iterator->first));
        worldKeyXor_ ^= mixedPosition;
        worldKeySum_ += mixedPosition;
        worldVerificationXor_ ^= mixedVerification;
        worldVerificationSum_ += mixedVerification;
    }
    if (inserted && worlds_.size() == 1) {
        side_ = worlds_.begin()->second.side_to_move();
        publicView_ = publicView;
    }
    return true;
}

bool PublicBeliefState::add_compact_prevalidated(
  std::string upn, Position position, const InformationViewKey& publicView,
  std::optional<std::vector<Move>> legalMoves, std::string* error) {
    if (side_ && position.side_to_move() != *side_) {
        if (error)
            *error = "side-to-move differs from the retained belief";
        return false;
    }
    if (compactPublicView_ && !(publicView == *compactPublicView_)) {
        if (error)
            *error = "ordinary compact public view differs from the retained belief";
        return false;
    }
    const std::uint64_t positionKey = position.key();
    const auto [iterator, inserted] = worlds_.emplace(upn, std::move(position));
    if (legalMoves)
        legalMovesCache_.emplace(std::move(upn), std::move(*legalMoves));
    if (inserted) {
        const std::uint64_t mixedPosition = mix_key(positionKey);
        const std::uint64_t mixedVerification = mix_key(string_key(iterator->first));
        worldKeyXor_ ^= mixedPosition;
        worldKeySum_ += mixedPosition;
        worldVerificationXor_ ^= mixedVerification;
        worldVerificationSum_ += mixedVerification;
    }
    if (inserted && worlds_.size() == 1) {
        side_ = iterator->second.side_to_move();
        compactPublicView_ = publicView;
    }
    return true;
}

const std::vector<Move>& PublicBeliefState::cached_legal_moves(
  const std::string& upn, const Position& position) const {
    const auto found = legalMovesCache_.find(upn);
    if (found != legalMovesCache_.end())
        return found->second;
    // The hidden royal owner may condition its policy on the fixed identity,
    // but while one of its Jesters remains alive the King/Jester silhouettes
    // have identical movement and ordinary check filtering is suspended.
    // Reuse only the owner's frontier; the observer's legal dots remain
    // candidate-specific and are generated independently.
    if (position.side_to_move() != disclosure_.observer)
        for (const auto& [cachedUpn, moves] : legalMovesCache_) {
            const auto world = worlds_.find(cachedUpn);
            if (world != worlds_.end() && royal_assignment_equivalent(
                  world->second, position, position.side_to_move()))
                return legalMovesCache_.emplace(upn, moves).first->second;
        }
    return legalMovesCache_.emplace(upn, position.legal_moves()).first->second;
}

void PublicBeliefState::rebuild_transposition_digest() {
    worldKeyXor_ = 0;
    worldKeySum_ = 0;
    worldVerificationXor_ = 0;
    worldVerificationSum_ = 0;
    for (const auto& [upn, position] : worlds_) {
        const std::uint64_t mixedPosition = mix_key(position.key());
        const std::uint64_t mixedVerification = mix_key(string_key(upn));
        worldKeyXor_ ^= mixedPosition;
        worldKeySum_ += mixedPosition;
        worldVerificationXor_ ^= mixedVerification;
        worldVerificationSum_ += mixedVerification;
    }
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

bool PublicBeliefState::enemy_king_known() const {
    if (worlds_.empty())
        return false;
    if (disclosure_.enemyKingKnown)
        return true;
    const Color enemy = disclosure_.observer == Color::White
                      ? Color::Black : Color::White;
    int expectedSquare = Position::NoSquare;
    bool first = true;
    for (const auto& [upn, world] : worlds_) {
        (void)upn;
        int kingSquare = Position::NoSquare;
        for (int id = 0; id < world.piece_count(); ++id) {
            const PieceState& piece = world.piece(id);
            if (piece.alive && piece.onBoard && piece.color == enemy &&
                piece.type == PieceType::King) {
                kingSquare = piece.square;
                break;
            }
        }
        if (first) {
            expectedSquare = kingSquare;
            first = false;
        }
        else if (kingSquare != expectedSquare)
            return false;
    }
    return true;
}

std::vector<int> PublicBeliefState::enemy_king_candidate_squares() const {
    const Color enemy = disclosure_.observer == Color::White
                      ? Color::Black : Color::White;
    std::set<int> candidates;
    for (const auto& [upn, world] : worlds_) {
        (void)upn;
        for (int id = 0; id < world.piece_count(); ++id) {
            const PieceState& piece = world.piece(id);
            if (piece.alive && piece.onBoard && piece.color == enemy &&
                piece.type == PieceType::King)
                candidates.insert(piece.square);
        }
    }
    return {candidates.begin(), candidates.end()};
}

bool PublicBeliefState::piece_location_known(int pieceId) const {
    if (worlds_.empty())
        return false;
    int expectedSquare = Position::NoSquare;
    bool first = true;
    for (const auto& [upn, world] : worlds_) {
        (void)upn;
        if (pieceId < 0 || pieceId >= world.piece_count())
            return false;
        const PieceState& piece = world.piece(pieceId);
        if (!piece.alive || !piece.onBoard)
            return false;
        if (first) {
            expectedSquare = piece.square;
            first = false;
        }
        else if (piece.square != expectedSquare)
            return false;
    }
    return !first;
}

std::vector<int> PublicBeliefState::piece_location_candidates(int pieceId) const {
    std::set<int> candidates;
    for (const auto& [upn, world] : worlds_) {
        (void)upn;
        if (pieceId < 0 || pieceId >= world.piece_count())
            continue;
        const PieceState& piece = world.piece(pieceId);
        if (piece.alive && piece.onBoard)
            candidates.insert(piece.square);
    }
    return {candidates.begin(), candidates.end()};
}

bool PublicBeliefState::piece_type_known(int pieceId, PieceType type) const {
    if (worlds_.empty())
        return false;
    for (const auto& [upn, world] : worlds_) {
        (void)upn;
        if (pieceId < 0 || pieceId >= world.piece_count())
            return false;
        const PieceState& piece = world.piece(pieceId);
        if (!piece.alive || !piece.onBoard || piece.type != type)
            return false;
    }
    return true;
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
    legalMovesCache_.clear();
    compactPublicView_.reset();
    rebuild_transposition_digest();
    return true;
}

BeliefSuccessorPartitions PublicBeliefState::successor_partitions(
  std::string_view moveText, bool includeDecisionObservation) const {
    BeliefSuccessorPartitions result;
    result.before = worlds_.size();
    struct Observation {
        std::string publicView;
        std::map<std::string, Position> worlds;
    };
    std::map<std::string, Observation> observations;
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
        std::string publicView;
        std::string observation = transition_observation_key(
          before, *move, after, disclosure_, &publicView);
        if (includeDecisionObservation && !after.game_over() &&
            after.side_to_move() == disclosure_.observer) {
            const std::string decision = decision_observation_key(
              after, disclosure_, &publicView);
            observation += "|nextDecision=" +
                           std::to_string(decision.size()) + ':' + decision;
        }
        Observation& bucket = observations[observation];
        bucket.publicView = std::move(publicView);
        bucket.worlds.emplace(after.upn(), std::move(after));
    }
    result.buckets.reserve(observations.size());
    for (auto& [observation, contents] : observations) {
        BeliefSuccessorBucket bucket;
        bucket.observation = std::move(observation);
        bucket.publicView = std::move(contents.publicView);
        bucket.worlds.reserve(contents.worlds.size());
        bucket.worldKeys.reserve(contents.worlds.size());
        for (auto& [upn, position] : contents.worlds) {
            bucket.worldKeys.push_back(std::move(upn));
            bucket.worlds.push_back(std::move(position));
        }
        result.buckets.push_back(std::move(bucket));
    }
    return result;
}

BeliefSuccessorPartitions PublicBeliefState::adversarial_successor_partitions(
  bool includeDecisionObservation,
  const std::vector<std::string>& allowedActions) const {
    BeliefSuccessorPartitions result;
    result.before = worlds_.size();
    const std::set<std::string> allowed(
      allowedActions.begin(), allowedActions.end());
    struct Observation {
        std::string publicView;
        std::map<std::string, Position> worlds;
        std::set<std::string> actions;
    };
    using ObservationMap = std::map<std::string, Observation>;
    std::vector<const Position*> sourceWorlds;
    sourceWorlds.reserve(worlds_.size());
    for (const auto& [upn, before] : worlds_) {
        (void)upn;
        sourceWorlds.push_back(&before);
    }
    const auto expand = [&](std::size_t begin, std::size_t end) {
        ObservationMap local;
        for (std::size_t index = begin; index < end; ++index) {
            const Position& before = *sourceWorlds[index];
            for (const Move& move : before.legal_moves()) {
                const std::string notation = before.move_to_string(move);
                if (!allowed.empty() && !allowed.count(notation))
                    continue;
                Position after = before;
                Undo undo;
                if (!after.make_move(move, undo))
                    continue;
                std::string publicView;
                std::string observation = transition_observation_key(
                  before, move, after, disclosure_, &publicView);
                if (includeDecisionObservation && !after.game_over() &&
                    after.side_to_move() == disclosure_.observer) {
                    const std::string decision = decision_observation_key(
                      after, disclosure_, &publicView);
                    observation += "|nextDecision=" +
                                   std::to_string(decision.size()) + ':' + decision;
                }
                Observation& bucket = local[observation];
                bucket.publicView = std::move(publicView);
                bucket.actions.insert(notation);
                bucket.worlds.emplace(after.upn(), std::move(after));
            }
        }
        return local;
    };
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t workers = sourceWorlds.size() < 512
      ? 1 : std::min<std::size_t>(4, hardware);
    std::vector<std::future<ObservationMap>> futures;
    futures.reserve(workers > 1 ? workers : 0);
    if (workers > 1) {
        for (std::size_t worker = 0; worker < workers; ++worker) {
            const std::size_t begin = sourceWorlds.size() * worker / workers;
            const std::size_t end = sourceWorlds.size() * (worker + 1) / workers;
            futures.push_back(std::async(
              std::launch::async, expand, begin, end));
        }
    }
    ObservationMap observations = workers == 1
      ? expand(0, sourceWorlds.size()) : ObservationMap{};
    for (auto& future : futures)
        for (auto& [observation, contents] : future.get()) {
            Observation& merged = observations[observation];
            if (merged.publicView.empty())
                merged.publicView = std::move(contents.publicView);
            merged.actions.insert(
              std::make_move_iterator(contents.actions.begin()),
              std::make_move_iterator(contents.actions.end()));
            merged.worlds.merge(contents.worlds);
        }
    result.buckets.reserve(observations.size());
    for (auto& [observation, contents] : observations) {
        BeliefSuccessorBucket bucket;
        bucket.observation = std::move(observation);
        bucket.publicView = std::move(contents.publicView);
        bucket.worlds.reserve(contents.worlds.size());
        bucket.worldKeys.reserve(contents.worlds.size());
        for (auto& [upn, position] : contents.worlds) {
            bucket.worldKeys.push_back(std::move(upn));
            bucket.worlds.push_back(std::move(position));
        }
        bucket.actions.assign(
          contents.actions.begin(), contents.actions.end());
        result.buckets.push_back(std::move(bucket));
    }
    return result;
}

BeliefSuccessorPartitions
PublicBeliefState::adversarial_successor_partitions_compact(
  bool includeDecisionObservation,
  const InformationObservationKey* expected) const {
    BeliefSuccessorPartitions result;
    result.before = worlds_.size();
    struct CachedWorld {
        Position position;
        std::vector<Move> legalMoves;
    };
    struct Observation {
        std::map<std::string, CachedWorld> worlds;
        std::set<std::string> actions;
    };
    using ObservationMap = std::map<InformationObservationKey, Observation>;
    struct SourceWorld {
        const Position* position = nullptr;
        const std::vector<Move>* cachedMoves = nullptr;
    };
    std::vector<SourceWorld> sources;
    sources.reserve(worlds_.size());
    for (const auto& [upn, position] : worlds_) {
        const auto cached = legalMovesCache_.find(upn);
        sources.push_back({
          &position,
          cached == legalMovesCache_.end() ? nullptr : &cached->second});
    }

    const auto expand = [&](std::size_t begin, std::size_t end) {
        ObservationMap local;
        for (std::size_t index = begin; index < end; ++index) {
            const SourceWorld& source = sources[index];
            const Position& before = *source.position;
            std::vector<Move> generated;
            const std::vector<Move>* moves = source.cachedMoves;
            if (!moves) {
                generated = before.legal_moves();
                moves = &generated;
            }
            for (const Move& move : *moves) {
                if (expected && !compact_transition_action_may_match(
                      before, move, disclosure_, expected->action))
                    continue;
                Position after = before;
                if (!after.apply_move_unchecked(move))
                    continue;
                if (expected && compact_transition_action_key(
                      before, move, after, disclosure_) != expected->action)
                    continue;
                std::vector<Move> afterLegalMoves;
                const bool ongoing = expected
                  ? ((expected->view.state >> 30) & 3) == 0
                  : !after.game_over();
                const bool observeDecision = includeDecisionObservation &&
                  ongoing &&
                  after.side_to_move() == disclosure_.observer;
                if (observeDecision)
                    afterLegalMoves = after.legal_moves();
                InformationObservationKey observation =
                  compact_transition_observation_key(
                    before, move, after, disclosure_, observeDecision,
                    observeDecision ? &afterLegalMoves : nullptr);
                if (expected && !(observation == *expected))
                    continue;
                Observation& bucket = local[std::move(observation)];
                bucket.actions.insert(before.move_to_string(move));
                std::string childUpn = after.upn();
                bucket.worlds.emplace(
                  std::move(childUpn), CachedWorld{
                    std::move(after), std::move(afterLegalMoves)});
            }
        }
        return local;
    };

    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t workers = sources.size() < 384
      ? 1 : std::min<std::size_t>(4, hardware);
    std::vector<std::future<ObservationMap>> futures;
    if (workers > 1) {
        futures.reserve(workers);
        for (std::size_t worker = 0; worker < workers; ++worker) {
            const std::size_t begin = sources.size() * worker / workers;
            const std::size_t end = sources.size() * (worker + 1) / workers;
            futures.push_back(std::async(
              std::launch::async, expand, begin, end));
        }
    }
    ObservationMap observations = workers == 1
      ? expand(0, sources.size()) : ObservationMap{};
    for (auto& future : futures)
        for (auto& [observation, contents] : future.get()) {
            Observation& merged = observations[observation];
            merged.actions.insert(
              std::make_move_iterator(contents.actions.begin()),
              std::make_move_iterator(contents.actions.end()));
            merged.worlds.merge(contents.worlds);
        }

    result.buckets.reserve(observations.size());
    for (auto& [observation, contents] : observations) {
        BeliefSuccessorBucket bucket;
        bucket.compactPublicView = observation.view;
        bucket.compactObservation = std::move(observation);
        bucket.actions.assign(
          contents.actions.begin(), contents.actions.end());
        bucket.worlds.reserve(contents.worlds.size());
        bucket.worldKeys.reserve(contents.worlds.size());
        bucket.worldLegalMoves.reserve(contents.worlds.size());
        for (auto& [upn, world] : contents.worlds) {
            bucket.worldKeys.push_back(std::move(upn));
            bucket.worlds.push_back(std::move(world.position));
            bucket.worldLegalMoves.push_back(std::move(world.legalMoves));
        }
        result.buckets.push_back(std::move(bucket));
    }
    return result;
}

BeliefTransitionResult PublicBeliefState::apply_known(
  std::string_view moveText, std::string* error) {
    BeliefTransitionResult result;
    BeliefSuccessorPartitions partitions = successor_partitions(moveText);
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

    replace_with_successor(std::move(partitions.buckets.front()));
    result.after = size();
    result.applied = true;
    return result;
}

void PublicBeliefState::replace_with_successor(BeliefSuccessorBucket bucket) {
    worlds_.clear();
    legalMovesCache_.clear();
    for (std::size_t world = 0; world < bucket.worlds.size(); ++world) {
        std::string key = std::move(bucket.worldKeys[world]);
        const auto [iterator, inserted] = worlds_.emplace(
          key, std::move(bucket.worlds[world]));
        if (inserted && world < bucket.worldLegalMoves.size() &&
            !bucket.worldLegalMoves[world].empty())
            legalMovesCache_.emplace(
              std::move(key), std::move(bucket.worldLegalMoves[world]));
        (void)iterator;
    }
    side_ = worlds_.empty()
          ? std::optional<Color>{}
          : std::optional<Color>{worlds_.begin()->second.side_to_move()};
    if (bucket.compactPublicView) {
        publicView_.clear();
        compactPublicView_ = std::move(bucket.compactPublicView);
    }
    else {
        publicView_ = std::move(bucket.publicView);
        compactPublicView_.reset();
    }
    rebuild_transposition_digest();
}

bool PublicHistoryState::start(Position initial,
                               DisclosureContext disclosure,
                               std::string* error) {
    return start(std::move(initial), disclosure, false, error);
}

bool PublicHistoryState::start(Position initial,
                               DisclosureContext disclosure,
                               bool initialDeploymentKnown,
                               std::string* error) {
    PublicBeliefState initialBeliefs(disclosure);
    std::string localError;
    if (!initial_public_beliefs(
          initial, disclosure, initialDeploymentKnown,
          initialBeliefs, &localError)) {
        initialized_ = false;
        if (error)
            *error = localError;
        return false;
    }
    actual_ = std::move(initial);
    beliefs_ = std::move(initialBeliefs);
    preparedOpponentTransitions_.reset();
    initialized_ = true;
    return true;
}

bool PublicHistoryState::start(
  Position initial, DisclosureContext disclosure,
  bool initialDeploymentKnown,
  const std::vector<int>& enemyKingCandidateSquares,
  std::string* error) {
    if (enemyKingCandidateSquares.empty()) {
        if (error)
            *error = "an exact enemy King candidate set cannot be empty";
        initialized_ = false;
        return false;
    }
    const Color enemy = disclosure.observer == Color::White
                      ? Color::Black : Color::White;
    std::set<int> requested;
    std::vector<int> candidateIds;
    int actualKing = Position::NoPiece;
    for (int id = 0; id < initial.piece_count(); ++id) {
        const PieceState& piece = initial.piece(id);
        if (!piece.alive || !piece.onBoard || piece.color != enemy ||
            (piece.type != PieceType::King && piece.type != PieceType::Jester))
            continue;
        if (piece.type == PieceType::King)
            actualKing = id;
        if (std::find(enemyKingCandidateSquares.begin(),
                      enemyKingCandidateSquares.end(), piece.square) !=
            enemyKingCandidateSquares.end()) {
            candidateIds.push_back(id);
            requested.insert(piece.square);
        }
    }
    const std::set<int> supplied(
      enemyKingCandidateSquares.begin(), enemyKingCandidateSquares.end());
    if (supplied.size() != enemyKingCandidateSquares.size() ||
        requested != supplied) {
        if (error)
            *error = "enemy King candidates must be distinct current enemy "
                     "royal squares";
        initialized_ = false;
        return false;
    }
    if (actualKing == Position::NoPiece ||
        std::find(candidateIds.begin(), candidateIds.end(), actualKing) ==
          candidateIds.end()) {
        if (error)
            *error = "the authoritative enemy King is absent from its public "
                     "candidate set";
        initialized_ = false;
        return false;
    }
    if (disclosure.enemyKingKnown && candidateIds.size() != 1) {
        if (error)
            *error = "known enemy King disclosure requires one candidate";
        initialized_ = false;
        return false;
    }
    if (candidateIds.size() == 1)
        disclosure.enemyKingKnown = true;
    else {
        disclosure.enemyRoyalCandidatesSpecified = true;
        disclosure.enemyRoyalCandidatesLow = 0;
        disclosure.enemyRoyalCandidatesHigh = 0;
        for (const int id : candidateIds)
            disclosure.set_enemy_royal_candidate(id);
    }
    return start(std::move(initial), disclosure, initialDeploymentKnown, error);
}

bool PublicHistoryState::apply_actual(std::string_view moveText,
                                      std::string* error) {
    if (!initialized_) {
        if (error)
            *error = "public history has not been initialized";
        return false;
    }
    const std::optional<Move> move = unique_notated_move(actual_, moveText);
    if (!move) {
        if (error)
            *error = "actual history action is illegal or ambiguous";
        return false;
    }
    Position after = actual_;
    Undo undo;
    if (!after.make_move(*move, undo)) {
        if (error)
            *error = "actual history action could not be applied";
        return false;
    }

    const bool observerMoved =
      actual_.side_to_move() == beliefs_.disclosure().observer;
    std::string observed;
    std::optional<InformationObservationKey> compactObserved;
    if (observerMoved)
        observed = observed_transition(
          actual_, *move, after, beliefs_.disclosure());
    else {
        std::vector<Move> afterLegalMoves;
        const bool observeDecision = !after.game_over() &&
          after.side_to_move() == beliefs_.disclosure().observer;
        if (observeDecision)
            afterLegalMoves = after.legal_moves();
        compactObserved = compact_transition_observation_key(
          actual_, *move, after, beliefs_.disclosure(), observeDecision,
          observeDecision ? &afterLegalMoves : nullptr);
    }
    BeliefSuccessorPartitions partitions;
    if (observerMoved)
        partitions = beliefs_.successor_partitions(moveText);
    else if (preparedOpponentTransitions_)
        partitions = std::move(*preparedOpponentTransitions_);
    else
        partitions = beliefs_.adversarial_successor_partitions_compact(
          true, &*compactObserved);
    preparedOpponentTransitions_.reset();
    BeliefSuccessorBucket* selected = nullptr;
    for (BeliefSuccessorBucket& bucket : partitions.buckets) {
        const bool matches = compactObserved
          ? bucket.compactObservation &&
            *bucket.compactObservation == *compactObserved
          : bucket.observation == observed;
        if (!matches)
            continue;
        if (selected) {
            if (error)
                *error = "actual history observation matched multiple buckets";
            return false;
        }
        selected = &bucket;
    }
    if (!selected) {
        if (error)
            *error = "actual history observation matches no retained belief";
        return false;
    }

    actual_ = std::move(after);
    beliefs_.replace_with_successor(std::move(*selected));
    return true;
}

bool PublicHistoryState::prepare_opponent_transition(std::string* error) {
    if (!initialized_) {
        if (error)
            *error = "public history has not been initialized";
        return false;
    }
    if (actual_.side_to_move() == beliefs_.disclosure().observer)
        return true;
    if (!preparedOpponentTransitions_)
        preparedOpponentTransitions_ =
          beliefs_.adversarial_successor_partitions_compact();
    return true;
}

bool PublicHistoryState::initialized() const { return initialized_; }

const Position& PublicHistoryState::actual_position() const { return actual_; }

const PublicBeliefState& PublicHistoryState::beliefs() const { return beliefs_; }

Search::Search(std::size_t hashMegabytes) {
    const std::size_t bytes = std::max<std::size_t>(1, hashMegabytes) * 1024 * 1024;
    std::size_t entries = 1;
    while ((entries << 1) * sizeof(Cluster) <= bytes)
        entries <<= 1;
    table_.resize(entries);
    std::size_t beliefEntries = 1;
    while ((beliefEntries << 1) * sizeof(BeliefCluster) <= bytes)
        beliefEntries <<= 1;
    beliefTable_.resize(beliefEntries);
}

void Search::clear() {
    std::fill(table_.begin(), table_.end(), Cluster{});
    std::fill(beliefTable_.begin(), beliefTable_.end(), BeliefCluster{});
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

Search::BeliefEntry* Search::find_belief_entry(
  std::uint64_t key, std::uint64_t verification) {
    BeliefCluster& cluster = beliefTable_[key & (beliefTable_.size() - 1)];
    for (BeliefEntry& entry : cluster.entries)
        if (entry.bound != Bound::None && entry.key == key &&
            entry.verification == verification)
            return &entry;
    return nullptr;
}

Search::BeliefEntry& Search::replacement_belief_entry(
  std::uint64_t key, std::uint64_t verification) {
    BeliefCluster& cluster = beliefTable_[key & (beliefTable_.size() - 1)];
    for (BeliefEntry& entry : cluster.entries)
        if (entry.bound == Bound::None ||
            (entry.key == key && entry.verification == verification))
            return entry;
    return *std::max_element(cluster.entries.begin(), cluster.entries.end(),
      [this](const BeliefEntry& lhs, const BeliefEntry& rhs) {
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

int Search::LazyRoyalContext::count() const {
    return __builtin_popcountll(candidatesLow) +
           __builtin_popcountll(candidatesHigh);
}

int Search::LazyRoyalContext::first_except(int excluded) const {
    for (int id = 0; id < Position::MaxPieces; ++id)
        if (id != excluded && contains(id))
            return id;
    return Position::NoPiece;
}

void Search::LazyRoyalContext::erase(int id) {
    if (id < 64)
        candidatesLow &= ~(std::uint64_t{1} << id);
    else
        candidatesHigh &= ~(std::uint64_t{1} << (id - 64));
}

int Search::evaluate_lazy_royals(
  const Position& position, int ply,
  const LazyRoyalContext* lazyRoyals) const {
    if (!lazyRoyals || lazyRoyals->count() <= 1)
        return evaluate(position, ply);
    int canonicalKing = Position::NoPiece;
    for (int id = 0; id < position.piece_count(); ++id)
        if (position.piece(id).alive && position.piece(id).onBoard &&
            position.piece(id).color == lazyRoyals->owner &&
            position.piece(id).type == PieceType::King) {
            canonicalKing = id;
            break;
        }
    const bool observerMoves = position.side_to_move() == lazyRoyals->observer;
    int robust = observerMoves ? Infinity : -Infinity;
    for (int id = 0; id < Position::MaxPieces; ++id) {
        if (!lazyRoyals->contains(id))
            continue;
        Position world = position;
        if (id != canonicalKing && !world.swap_royal_roles(canonicalKing, id))
            continue;
        const int score = evaluate(world, ply);
        robust = observerMoves ? std::min(robust, score)
                               : std::max(robust, score);
    }
    return robust == Infinity || robust == -Infinity
         ? evaluate(position, ply) : robust;
}

bool Search::apply_lazy_royal_move(
  const Position& parent, Position& child, const Move& move,
  LazyRoyalContext& lazyRoyals) const {
    const int target = parent.piece_on(move.to);
    if (lazyRoyals.count() > 1 && lazyRoyals.contains(target)) {
        int canonicalKing = Position::NoPiece;
        for (int id = 0; id < child.piece_count(); ++id)
            if (child.piece(id).alive && child.piece(id).onBoard &&
                child.piece(id).color == lazyRoyals.owner &&
                child.piece(id).type == PieceType::King) {
                canonicalKing = id;
                break;
            }
        if (canonicalKing == target) {
            const int survivor = lazyRoyals.first_except(target);
            if (survivor == Position::NoPiece ||
                !child.swap_royal_roles(canonicalKing, survivor))
                return false;
        }
        lazyRoyals.erase(target);
    }
    return child.apply_move_unchecked(move);
}

bool Search::stopped() {
    if (stop_)
        return true;
    if (limits_.nodes && nodes_ >= limits_.nodes)
        return stop_ = true;
    // Belief nodes advance by whole information-set sizes rather than by one.
    // A bit-mask cadence can therefore skip the deadline for thousands of
    // expensive worlds. Callers already invoke stopped() at coarse search
    // boundaries, so checking the clock on every invocation is both cheap and
    // necessary for a trustworthy Play movetime.
    if (limits_.moveTime.count() &&
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

int Search::quiescence(Position& position, int alpha, int beta, int ply,
                       const LazyRoyalContext* lazyRoyals) {
    ++nodes_;
    if (stopped())
        return evaluate_lazy_royals(position, ply, lazyRoyals);
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
        return evaluate_lazy_royals(position, ply, lazyRoyals);
    if (ply > 0 && !lazyRoyals)
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
        const int standPat = evaluate_lazy_royals(position, ply, lazyRoyals);
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
        LazyRoyalContext childRoyals =
          lazyRoyals ? *lazyRoyals : LazyRoyalContext{};
        if (lazyRoyals
              ? !apply_lazy_royal_move(position, child, move, childRoyals)
              : !child.apply_move_unchecked(move))
            continue;
        if (!child.legal_after_unchecked_move(side))
            continue;
        if (useNnue_)
            UltimateNnue::update(position, child, accumulators_[ply], accumulators_[ply + 1]);
        foundLegal = true;
        const bool sameSide = child.side_to_move() == before;
        const LazyRoyalContext* nextRoyals =
          lazyRoyals && childRoyals.count() > 1 ? &childRoyals : nullptr;
        const int score = sameSide
          ? quiescence(child, alpha, beta, ply + 1, nextRoyals)
          : -quiescence(child, -beta, -alpha, ply + 1, nextRoyals);
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
                    std::vector<Move>& pv, const Move* excludedMove,
                    const LazyRoyalContext* lazyRoyals) {
    pv.clear();
    if (stopped())
        return evaluate_lazy_royals(position, ply, lazyRoyals);
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
        return evaluate_lazy_royals(position, ply, lazyRoyals);
    if (ply > 0 && !lazyRoyals)
        if (const auto tablebase = TablebaseProbe::probe(position)) {
            if (tablebase->wdl == TablebaseWdl::Draw)
                return 0;
            const int distance = std::min<int>(tablebase->dtw, MateThreshold - 1);
            return tablebase->wdl == TablebaseWdl::Win
                 ? Mate - ply - distance : -Mate + ply + distance;
        }
    if (depth <= 0)
        return quiescence(position, alpha, beta, ply, lazyRoyals);

    ++nodes_;
    const int originalAlpha = alpha;
    const bool pvNode = beta - alpha > 1;
    std::uint64_t key = position.key();
    if (lazyRoyals)
        key ^= mix_key(lazyRoyals->candidatesLow) ^
               mix_key(lazyRoyals->candidatesHigh ^ 0xd8a4f3b2761c905eULL);
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
                                        exclusionPv, &ttMove, lazyRoyals);
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
    const int staticEval = quietPruningNode
                         ? evaluate_lazy_royals(position, ply, lazyRoyals) : 0;
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
            LazyRoyalContext childRoyals =
              lazyRoyals ? *lazyRoyals : LazyRoyalContext{};
            if (lazyRoyals
                  ? !apply_lazy_royal_move(position, child, move, childRoyals)
                  : !child.apply_move_unchecked(move))
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
            const LazyRoyalContext* nextRoyals =
              lazyRoyals && childRoyals.count() > 1 ? &childRoyals : nullptr;
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
                score = sameSide
                  ? negamax(child, nextDepth, alpha, beta, ply + 1, childPv,
                            nullptr, nextRoyals)
                  : -negamax(child, nextDepth, -beta, -alpha, ply + 1,
                             childPv, nullptr, nextRoyals);
            }
            else if (sameSide) {
                score = negamax(child, nextDepth, alpha, alpha + 1, ply + 1,
                                childPv, nullptr, nextRoyals);
                if (score > alpha && score < beta)
                    score = negamax(child, nextDepth, alpha, beta, ply + 1,
                                    childPv, nullptr, nextRoyals);
            }
            else {
                score = -negamax(child, nextDepth - reduction, -alpha - 1,
                                 -alpha, ply + 1, childPv, nullptr, nextRoyals);
                if (reduction && score > alpha)
                    score = -negamax(child, nextDepth, -alpha - 1, -alpha,
                                     ply + 1, childPv, nullptr, nextRoyals);
                if (score > alpha && score < beta)
                    score = -negamax(child, nextDepth, -beta, -alpha, ply + 1,
                                     childPv, nullptr, nextRoyals);
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
        return stop_ ? evaluate_lazy_royals(position, ply, lazyRoyals)
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

std::optional<BeliefSearchResult> Search::think_factored_royals(
  const PublicBeliefState& beliefs, const SearchLimits& limits,
  bool legalDotObservations) {
    if (!limits.factoredBeliefs || beliefs.size() < 2 ||
        !beliefs.side_to_move())
        return std::nullopt;

    const Color observer = beliefs.disclosure().observer;
    const Color royalOwner = ~observer;
    const Position& reference = beliefs.concrete_worlds().begin()->second;
    bool pureRoyalMaterial = true;
    for (int id = 0; id < reference.piece_count(); ++id) {
        const PieceState& piece = reference.piece(id);
        pureRoyalMaterial = pureRoyalMaterial &&
          (!piece.alive || !piece.onBoard || piece.type == PieceType::King ||
           piece.type == PieceType::Jester);
    }
    std::vector<int> candidates;
    candidates.reserve(beliefs.size());
    for (const auto& [upn, world] : beliefs.concrete_worlds()) {
        (void)upn;
        int king = Position::NoPiece;
        for (int id = 0; id < world.piece_count(); ++id) {
            const PieceState& piece = world.piece(id);
            if (piece.alive && piece.onBoard && piece.color == royalOwner &&
                piece.type == PieceType::King)
                king = id;
            // Ghost location can later re-expand. It is handled by the
            // correlated factored solver, never by the monotone royal mask.
            if (piece.alive && piece.onBoard && piece.type == PieceType::Ghost)
                return std::nullopt;
        }
        if (king == Position::NoPiece ||
            !royal_assignment_equivalent(reference, world, royalOwner))
            return std::nullopt;
        candidates.push_back(king);
    }
    std::sort(candidates.begin(), candidates.end());
    if (std::adjacent_find(candidates.begin(), candidates.end()) != candidates.end())
        return std::nullopt;

    int referenceKing = Position::NoPiece;
    for (int id = 0; id < reference.piece_count(); ++id)
        if (reference.piece(id).alive && reference.piece(id).onBoard &&
            reference.piece(id).color == royalOwner &&
            reference.piece(id).type == PieceType::King) {
            referenceKing = id;
            break;
        }
    Position canonical = reference;
    if (referenceKing != candidates.front() &&
        !canonical.swap_royal_roles(referenceKing, candidates.front()))
        return std::nullopt;

    struct RoyalState {
        Position position;
        std::vector<int> candidates;
    };
    struct RoyalWorld {
        int candidate = Position::NoPiece;
        Position position;
        std::map<std::string, Move> moves;
    };
    struct RoyalBucket {
        InformationViewKey view;
        std::map<int, Position> worlds;
        std::set<std::string> actions;
    };
    using RoyalPv = std::vector<std::string>;

    std::uint64_t materializations = 0;
    std::uint64_t legalCacheHits = 0;
    std::unordered_map<std::uint64_t, std::vector<Move>> legalFrontierCache;
    legalFrontierCache.reserve(262'144);

    const auto materialize = [&](const RoyalState& state, int candidate) {
        ++materializations;
        Position world = state.position;
        const int canonicalKing = state.candidates.empty()
                                 ? Position::NoPiece : state.candidates.front();
        if (candidate != canonicalKing)
            world.swap_royal_roles(canonicalKing, candidate);
        return world;
    };
    const auto cached_legal = [&](const Position& position) -> const std::vector<Move>& {
        const std::uint64_t key = position.key();
        auto found = legalFrontierCache.find(key);
        if (found != legalFrontierCache.end()) {
            ++legalCacheHits;
            return found->second;
        }
        return legalFrontierCache.emplace(key, position.legal_moves()).first->second;
    };
    const auto prepare = [&](const RoyalState& state) {
        std::vector<RoyalWorld> worlds;
        std::map<std::string, Move> canonicalMoves;
        if (pureRoyalMaterial) {
            std::set<std::string> ambiguous;
            std::vector<Move> pseudo = state.position.pseudo_legal_moves();
            state.position.annotate_captures(pseudo);
            for (const Move& move : pseudo) {
                const std::string notation = state.position.move_to_string(move);
                if (ambiguous.count(notation))
                    continue;
                const auto [iterator, inserted] =
                  canonicalMoves.emplace(notation, move);
                if (!inserted) {
                    canonicalMoves.erase(iterator);
                    ambiguous.insert(notation);
                }
            }
            RoyalWorld prepared;
            prepared.candidate = state.candidates.front();
            prepared.position = state.position;
            prepared.moves = std::move(canonicalMoves);
            worlds.push_back(std::move(prepared));
            return worlds;
        }
        worlds.reserve(state.candidates.size());
        for (const int candidate : state.candidates) {
            RoyalWorld prepared;
            prepared.candidate = candidate;
            prepared.position = materialize(state, candidate);
            std::set<std::string> ambiguous;
            for (const Move& move : cached_legal(prepared.position)) {
                const std::string notation = prepared.position.move_to_string(move);
                if (ambiguous.count(notation))
                    continue;
                const auto [iterator, inserted] =
                  prepared.moves.emplace(notation, move);
                if (!inserted) {
                    prepared.moves.erase(iterator);
                    ambiguous.insert(notation);
                }
            }
            worlds.push_back(std::move(prepared));
        }
        return worlds;
    };
    const auto make_child = [&](const RoyalBucket& bucket) {
        RoyalState child;
        for (const auto& [candidate, world] : bucket.worlds)
            if (world.has_real_king(royalOwner))
                child.candidates.push_back(candidate);
        if (child.candidates.empty()) {
            child.position = bucket.worlds.begin()->second;
            return child;
        }
        child.position = bucket.worlds.at(child.candidates.front());
        return child;
    };
    const auto decision_signature = [](const RoyalWorld& world) {
        std::vector<std::uint16_t> markers;
        markers.reserve(world.moves.size());
        for (const auto& [notation, move] : world.moves) {
            (void)notation;
            markers.push_back(move.kind == MoveKind::Pass
              ? 0
              : static_cast<std::uint16_t>(
                  ((static_cast<unsigned>(move.from) + 1U) << 7U) |
                  (static_cast<unsigned>(move.to) + 1U)));
        }
        std::sort(markers.begin(), markers.end());
        markers.erase(std::unique(markers.begin(), markers.end()), markers.end());
        return markers;
    };
    const auto fast_children = [&](const RoyalState& state,
                                   const std::vector<RoyalWorld>& worlds,
                                   const std::string& action) {
        std::vector<RoyalState> children;
        const Move& canonicalMove = worlds.front().moves.at(action);
        const int target = state.position.piece_on(canonicalMove.to);
        const bool capturesCandidate =
          std::find(state.candidates.begin(), state.candidates.end(), target) !=
          state.candidates.end();
        RoyalState child;
        const Color mover = state.position.side_to_move();
        if (capturesCandidate && state.position.side_to_move() == observer &&
            state.candidates.size() > 1) {
            child.candidates = state.candidates;
            child.candidates.erase(std::remove(child.candidates.begin(),
                                               child.candidates.end(), target),
                                   child.candidates.end());
            child.position = materialize(state, child.candidates.front());
            if (!child.position.apply_move_unchecked(canonicalMove))
                return children;
            if (!child.position.legal_after_unchecked_move(mover))
                return children;
            // The assignment where the captured silhouette was the King is
            // an immediate win. The live Jester observation is the unique
            // worst case while another candidate remains.
        } else {
            child = state;
            if (!child.position.apply_move_unchecked(canonicalMove))
                return children;
            if (!child.position.legal_after_unchecked_move(mover))
                return children;
            if (capturesCandidate) {
                child.candidates.erase(
                  std::remove(child.candidates.begin(), child.candidates.end(), target),
                  child.candidates.end());
            }
        }
        if (pureRoyalMaterial || !legalDotObservations ||
            child.candidates.size() < 2 ||
            child.position.side_to_move() != observer) {
            children.push_back(std::move(child));
            return children;
        }
        const std::vector<RoyalWorld> childWorlds = prepare(child);
        std::map<std::vector<std::uint16_t>, std::vector<int>> partitions;
        for (const RoyalWorld& world : childWorlds)
            partitions[decision_signature(world)].push_back(world.candidate);
        for (auto& [signature, subset] : partitions) {
            (void)signature;
            RoyalState partitioned;
            partitioned.candidates = std::move(subset);
            partitioned.position = materialize(child, partitioned.candidates.front());
            children.push_back(std::move(partitioned));
        }
        return children;
    };

    const auto beliefStart = std::chrono::steady_clock::now();
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
    start_ = beliefStart;
    nodes_ = 0;
    stop_ = false;
    useNnue_ = UltimateNnue::enabled();
    rootMoves_.clear();
    rootDrawMoves_.clear();
    ++generation_;
    for (auto& bySquare : history_)
        for (int& value : bySquare)
            value /= 2;

    RoyalState root{std::move(canonical), std::move(candidates)};
    BeliefSearchResult result;
    result.searchPath = "factored-royal";
    result.beliefs = result.deepBeliefs = beliefs.size();
    result.peakBeliefs = root.candidates.size();
    std::set<std::string> rootRestriction;
    for (const Move& move : limits.rootMoves)
        rootRestriction.insert(root.position.move_to_string(move));
    const std::set<std::string> rootDraws(
      limits.rootDrawMoveStrings.begin(), limits.rootDrawMoveStrings.end());
    std::uint64_t beliefNodes = 0;
    std::uint64_t beliefTtHits = 0;
    std::uint64_t singletonHandoffs = 0;
    std::uint64_t observationBuckets = 0;
    RoyalPv previousIterationPv;

    if (pureRoyalMaterial) {
        result.searchPath = "lazy-royal-native";
        LazyRoyalContext lazy;
        lazy.observer = observer;
        lazy.owner = royalOwner;
        for (const int candidate : root.candidates) {
            if (candidate < 64)
                lazy.candidatesLow |= std::uint64_t{1} << candidate;
            else
                lazy.candidatesHigh |= std::uint64_t{1} << (candidate - 64);
        }
        // Candidate materializations do not share one NNUE accumulator. The
        // handcrafted evaluator is exact for this tiny symbolic material and
        // avoids refreshing a network once per retained royal identity.
        useNnue_ = false;
        rootMoves_ = limits.rootMoves;
        for (const std::string& notation : limits.rootDrawMoveStrings)
            if (const auto move = root.position.move_from_string(notation))
                rootDrawMoves_.push_back(*move);
        result.commonMoves = root.position.legal_moves().size();
        result.candidates = result.commonMoves;
        const int perspective = root.position.side_to_move() == observer ? 1 : -1;
        const int maxDepth = std::clamp(limits.depth, 1, MaxPly - 2);
        int previousScore = 0;
        for (int depth = 1; depth <= maxDepth; ++depth) {
            std::vector<Move> pv;
            const bool aspirate = depth >= 3 && depth < 8;
            int alpha = aspirate ? std::max(-Infinity, previousScore - 60) : -Infinity;
            int beta = aspirate ? std::min(Infinity, previousScore + 60) : Infinity;
            int score = negamax(root.position, depth, alpha, beta, 0, pv,
                                nullptr, &lazy);
            if (!stop_ && (score <= alpha || score >= beta)) {
                pv.clear();
                score = negamax(root.position, depth, -Infinity, Infinity, 0,
                                pv, nullptr, &lazy);
            }
            if (stop_)
                break;
            previousScore = score;
            result.score = result.worstScore = result.meanScore = perspective * score;
            result.completedDepth = depth;
            result.historyPreservingPlies = depth;
            result.principalVariation.clear();
            for (const Move& move : pv)
                result.principalVariation.push_back(
                  root.position.move_to_string(move));
            result.bestMove = result.principalVariation.empty()
              ? std::nullopt
              : std::optional<std::string>(result.principalVariation.front());
            result.nodes = nodes_;
            result.beliefNodes = nodes_;
            result.materializations = materializations;
            result.legalCacheHits = legalCacheHits;
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - beliefStart);
            if (limits.onBeliefIteration)
                limits.onBeliefIteration(result);
        }
        result.nodes = nodes_;
        result.beliefNodes = nodes_;
        result.materializations = materializations;
        result.legalCacheHits = legalCacheHits;
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - beliefStart);
        return result;
    }

    const auto observer_evaluate = [&](const RoyalState& state, int ply) {
        const auto evaluate_world = [&](Position position) {
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
                else if (!position.has_legal_move())
                    score = position.real_king_threatened(mover)
                          ? -Mate + ply : 0;
                else
                    score = position.handcrafted_evaluate();
            }
            return mover == observer ? score : -score;
        };
        if (state.candidates.empty())
            return evaluate_world(state.position);
        int robust = Infinity;
        for (const int candidate : state.candidates)
            robust = std::min(robust, evaluate_world(materialize(state, candidate)));
        return robust;
    };

    std::function<int(const RoyalState&, int, int, int, int, RoyalPv&)> solve;
    solve = [&](const RoyalState& state, int depth, int alpha, int beta,
                int ply, RoyalPv& pv) -> int {
        pv.clear();
        ++beliefNodes;
        result.peakBeliefs = std::max(result.peakBeliefs, state.candidates.size());
        if (stopped())
            return observer_evaluate(state, ply);
        if (state.candidates.empty() || !state.position.has_real_king(observer))
            return observer_evaluate(state, ply);
        if (state.candidates.size() == 1 && ply < MaxPly - 1) {
            ++singletonHandoffs;
            Position position = state.position;
            if (useNnue_)
                UltimateNnue::refresh(position, accumulators_[ply]);
            std::vector<Move> concretePv;
            const int score = position.side_to_move() == observer
              ? negamax(position, depth, alpha, beta, ply, concretePv)
              : -negamax(position, depth, -beta, -alpha, ply, concretePv);
            for (const Move& move : concretePv)
                pv.push_back(position.move_to_string(move));
            return score;
        }
        nodes_ += 1;
        if (depth <= 0 || ply >= MaxPly - 1)
            return observer_evaluate(state, ply);

        const int originalAlpha = alpha;
        const int originalBeta = beta;
        std::uint64_t maskKey = 0x87c6e3b19a245df0ULL;
        for (const int candidate : state.candidates)
            maskKey ^= mix_key(static_cast<std::uint64_t>(candidate + 1));
        const std::uint64_t mode = legalDotObservations
                                 ? 0x3d97a4f16b82ce50ULL
                                 : 0xa56b09e217cf438dULL;
        const std::uint64_t key = state.position.key() ^ maskKey ^ mode;
        // Native search accepts the Position's 64-bit incremental key as its
        // complete TT identity. Keep the belief table equally cheap; the
        // candidate mask and observation mode are part of both table lanes.
        const std::uint64_t verification =
          mix_key(state.position.key() ^ mix_key(maskKey) ^ mix_key(mode));
        BeliefEntry* ttEntry = find_belief_entry(key, verification);
        if (ttEntry && ply != 0 && ttEntry->depth >= depth) {
            const int ttScore = score_from_tt(ttEntry->score, ply);
            if (ttEntry->bound == Bound::Exact ||
                (ttEntry->bound == Bound::Lower && ttScore >= beta) ||
                (ttEntry->bound == Bound::Upper && ttScore <= alpha)) {
                ++beliefTtHits;
                return ttScore;
            }
        }
        const std::uint64_t ttActionHash = ttEntry ? ttEntry->actionHash : 0;
        const std::uint64_t pvActionHash =
          static_cast<std::size_t>(ply) < previousIterationPv.size()
            ? string_key(previousIterationPv[static_cast<std::size_t>(ply)]) : 0;
        const auto store = [&](int score, std::string_view action) {
            if (stop_ || ply == 0)
                return;
            BeliefEntry& replacement = replacement_belief_entry(key, verification);
            replacement.key = key;
            replacement.verification = verification;
            replacement.actionHash = action.empty() ? 0 : string_key(action);
            replacement.score = static_cast<std::int16_t>(
              std::clamp(score_to_tt(score, ply), -Infinity, Infinity));
            replacement.depth = static_cast<std::int8_t>(std::min(depth, 127));
            replacement.bound = score <= originalAlpha ? Bound::Upper
                              : score >= originalBeta  ? Bound::Lower
                                                       : Bound::Exact;
            replacement.generation = generation_;
        };

        std::vector<RoyalWorld> worlds = prepare(state);
        const Color side = state.position.side_to_move();
        const bool maximizing = side == observer;
        if (!maximizing) {
            if (pureRoyalMaterial) {
                std::vector<std::string> actions;
                for (const auto& [notation, move] : worlds.front().moves) {
                    (void)move;
                    if (ply != 0 || rootRestriction.empty() ||
                        rootRestriction.count(notation))
                        actions.push_back(notation);
                }
                std::stable_sort(actions.begin(), actions.end(),
                  [&](const std::string& lhs, const std::string& rhs) {
                      const auto rank = [&](const std::string& action) {
                          const auto hash = string_key(action);
                          if (hash == ttActionHash)
                              return 2'000'000;
                          if (hash == pvActionHash)
                              return 1'500'000;
                          return move_score(worlds.front().position,
                                            worlds.front().moves.at(action),
                                            nullptr, ply);
                      };
                      return rank(lhs) > rank(rhs);
                  });
                int best = Infinity;
                std::string bestAction;
                RoyalPv bestPv;
                int actionNumber = 0;
                for (const std::string& action : actions) {
                    const std::vector<RoyalState> children =
                      fast_children(state, worlds, action);
                    observationBuckets += children.size();
                    int actionWorst = Infinity;
                    RoyalPv actionPv;
                    for (const RoyalState& child : children) {
                        const bool changedSide = child.position.side_to_move() != side;
                        RoyalPv childPv;
                        const int childDepth = depth - (changedSide ? 1 : 0);
                        int score = actionNumber == 0 || beta >= Infinity
                          ? solve(child, childDepth, alpha, beta, ply + 1, childPv)
                          : solve(child, childDepth, beta - 1, beta,
                                  ply + 1, childPv);
                        if (actionNumber > 0 && score > alpha && score < beta &&
                            !stopped()) {
                            childPv.clear();
                            score = solve(child, childDepth, alpha, beta,
                                          ply + 1, childPv);
                        }
                        if (score < actionWorst) {
                            actionWorst = score;
                            actionPv = std::move(childPv);
                        }
                        beta = std::min(beta, actionWorst);
                        if (alpha >= beta || stopped())
                            break;
                    }
                    if (ply == 0 && rootDraws.count(action))
                        actionWorst = std::min(actionWorst, 0);
                    if (actionWorst < best) {
                        best = actionWorst;
                        bestAction = action;
                        bestPv.assign(1, action);
                        bestPv.insert(bestPv.end(), actionPv.begin(), actionPv.end());
                    }
                    beta = std::min(beta, best);
                    ++actionNumber;
                    if (alpha >= beta || stopped())
                        break;
                }
                if (best == Infinity)
                    return observer_evaluate(state, ply);
                store(best, bestAction);
                pv = std::move(bestPv);
                return best;
            }
            std::map<InformationObservationKey, RoyalBucket> observations;
            for (const RoyalWorld& world : worlds)
                for (const auto& [notation, move] : world.moves) {
                    if (ply == 0 && !rootRestriction.empty() &&
                        !rootRestriction.count(notation))
                        continue;
                    Position after = world.position;
                    if (!after.apply_move_unchecked(move))
                        continue;
                    std::optional<std::vector<Move>> afterMoves;
                    if (legalDotObservations && after.side_to_move() == observer)
                        afterMoves = cached_legal(after);
                    InformationObservationKey observation =
                      compact_transition_observation_key(
                        world.position, move, after, beliefs.disclosure(),
                        legalDotObservations,
                        afterMoves ? &*afterMoves : nullptr);
                    RoyalBucket& bucket = observations[observation];
                    bucket.view = observation.view;
                    bucket.actions.insert(notation);
                    bucket.worlds.emplace(world.candidate, std::move(after));
                }
            observationBuckets += observations.size();
            if (observations.empty())
                return observer_evaluate(state, ply);
            std::vector<decltype(observations)::iterator> order;
            for (auto it = observations.begin(); it != observations.end(); ++it)
                order.push_back(it);
            std::stable_sort(order.begin(), order.end(), [&](auto lhs, auto rhs) {
                const auto priority = [&](const RoyalBucket& bucket) {
                    int value = 0;
                    for (const std::string& action : bucket.actions) {
                        const auto hash = string_key(action);
                        value = std::max(value, hash == ttActionHash ? 2
                                              : hash == pvActionHash ? 1 : 0);
                    }
                    return value;
                };
                return priority(lhs->second) > priority(rhs->second);
            });
            int best = Infinity;
            std::string bestAction;
            RoyalPv bestPv;
            int number = 0;
            for (auto iterator : order) {
                RoyalState child = make_child(iterator->second);
                const bool changedSide = child.position.side_to_move() != side;
                RoyalPv childPv;
                int score = number++ == 0
                  ? solve(child, depth - (changedSide ? 1 : 0), alpha, beta,
                          ply + 1, childPv)
                  : solve(child, depth - (changedSide ? 1 : 0), beta - 1, beta,
                          ply + 1, childPv);
                if (number > 1 && score > alpha && score < beta && !stopped()) {
                    childPv.clear();
                    score = solve(child, depth - (changedSide ? 1 : 0), alpha,
                                  beta, ply + 1, childPv);
                }
                if (score < best) {
                    best = score;
                    bestAction = iterator->second.actions.empty()
                      ? std::string() : *iterator->second.actions.begin();
                    bestPv.clear();
                    if (!bestAction.empty())
                        bestPv.push_back(bestAction);
                    bestPv.insert(bestPv.end(), childPv.begin(), childPv.end());
                }
                beta = std::min(beta, best);
                if (alpha >= beta || stopped())
                    break;
            }
            store(best, bestAction);
            pv = std::move(bestPv);
            return best;
        }

        std::set<std::string> actions;
        bool first = true;
        for (const RoyalWorld& world : worlds) {
            std::set<std::string> legal;
            for (const auto& [notation, move] : world.moves) {
                (void)move;
                legal.insert(notation);
            }
            if (first) {
                actions = std::move(legal);
                first = false;
            } else {
                std::set<std::string> intersection;
                std::set_intersection(actions.begin(), actions.end(),
                                      legal.begin(), legal.end(),
                                      std::inserter(intersection, intersection.begin()));
                actions = std::move(intersection);
            }
        }
        if (ply == 0 && !rootRestriction.empty()) {
            std::set<std::string> restricted;
            std::set_intersection(actions.begin(), actions.end(),
                                  rootRestriction.begin(), rootRestriction.end(),
                                  std::inserter(restricted, restricted.begin()));
            actions = std::move(restricted);
        }
        if (ply == 0) {
            result.commonMoves = actions.size();
            result.candidates = actions.size();
        }
        if (actions.empty())
            return observer_evaluate(state, ply);

        std::vector<std::string> ordered(actions.begin(), actions.end());
        std::map<std::string, int> priority;
        std::map<std::string, bool> quiet;
        for (const std::string& action : ordered) {
            const auto hash = string_key(action);
            int score = hash == ttActionHash ? 2'000'000
                      : hash == pvActionHash ? 1'500'000 : -Infinity;
            bool isQuiet = true;
            for (const RoyalWorld& world : worlds) {
                const Move& move = world.moves.at(action);
                score = std::max(score, move_score(world.position, move, nullptr, ply));
                isQuiet = isQuiet && move.kind == MoveKind::Normal &&
                          !world.position.is_capture(move);
            }
            priority[action] = score;
            quiet[action] = isQuiet;
        }
        std::stable_sort(ordered.begin(), ordered.end(), [&](const auto& lhs,
                                                             const auto& rhs) {
            return priority[lhs] > priority[rhs];
        });
        const auto recordQuietCutoff = [&](const std::string& action) {
            if (worlds.empty())
                return;
            const Move& representative = worlds.front().moves.at(action);
            if (ply < static_cast<int>(killers_.size()) &&
                !(representative == killers_[ply][0])) {
                killers_[ply][1] = killers_[ply][0];
                killers_[ply][0] = representative;
            }
            std::set<PieceType> attackers;
            for (const RoyalWorld& world : worlds) {
                const Move& move = world.moves.at(action);
                const int attacker = world.position.piece_on(move.from);
                if (attacker != Position::NoPiece)
                    attackers.insert(world.position.piece(attacker).type);
            }
            for (const PieceType type : attackers) {
                int& value = history_[static_cast<std::size_t>(type)]
                                     [representative.to];
                value = std::min(50'000, value + depth * depth);
            }
        };

        int best = -Infinity;
        std::string bestAction;
        RoyalPv bestPv;
        int actionNumber = 0;
        const bool pvNode = beta - alpha > 1;
        bool quietPruningNode = ply > 0 && !pvNode && depth <= 3 &&
          std::abs(alpha) < MateThreshold;
        for (const RoyalWorld& world : worlds)
            quietPruningNode = quietPruningNode &&
              !world.position.has_forced_action() &&
              world.position.supports_ordinary_exchange() &&
              (world.position.pieces(side, PieceType::Jester) ||
               !world.position.real_king_threatened(side));
        const int staticEval = quietPruningNode
                             ? observer_evaluate(state, ply) : 0;
        for (const std::string& action : ordered) {
            if (ply == 0 && rootDraws.count(action)) {
                if (0 > best) {
                    best = 0;
                    bestAction = action;
                    bestPv = {action};
                }
                alpha = std::max(alpha, best);
                ++actionNumber;
                continue;
            }
#ifndef ULTIMATE_DISABLE_LATE_MOVE_PRUNING
            if (quietPruningNode && quiet[action] && depth <= 2 &&
                actionNumber >= 6 + 5 * depth) {
                ++actionNumber;
                continue;
            }
#endif
#ifndef ULTIMATE_DISABLE_FORWARD_FUTILITY
            if (quietPruningNode && quiet[action] &&
                actionNumber >= 4 + 3 * depth &&
                staticEval + 140 * depth <= alpha) {
                ++actionNumber;
                continue;
            }
#endif
            if (pureRoyalMaterial) {
                const std::vector<RoyalState> children =
                  fast_children(state, worlds, action);
                observationBuckets += children.size();
                const bool isQuiet = quiet[action];
                const bool handsTurn = std::all_of(
                  children.begin(), children.end(), [&](const RoyalState& child) {
                      return child.position.side_to_move() != side;
                  });
                int reduction = 0;
                if (depth >= 3 && actionNumber >= 4 && isQuiet && handsTurn) {
                    reduction = 1;
#ifndef ULTIMATE_CONSERVATIVE_LMR
                    if (depth >= 6 && actionNumber >= 8)
                        ++reduction;
                    if (depth >= 9 && actionNumber >= 16)
                        ++reduction;
#endif
                    reduction = std::min(reduction, depth - 2);
                }
                const auto evaluateFastAction = [&](int searchDepth,
                                                    int windowBeta,
                                                    RoyalPv& actionPv) {
                    int worst = Infinity;
                    int natureBeta = windowBeta;
                    for (const RoyalState& child : children) {
                        const bool changedSide = child.position.side_to_move() != side;
                        RoyalPv childPv;
                        const int score = solve(
                          child, searchDepth - (changedSide ? 1 : 0), alpha,
                          natureBeta, ply + 1, childPv);
                        if (score < worst) {
                            worst = score;
                            actionPv = std::move(childPv);
                        }
                        natureBeta = std::min(natureBeta, worst);
                        if (worst <= alpha || stopped())
                            break;
                    }
                    return worst;
                };
                const int searchBeta = actionNumber == 0 || beta >= Infinity
                                     ? beta : std::min(beta, alpha + 1);
                RoyalPv actionPv;
                int actionWorst = evaluateFastAction(
                  depth - reduction, searchBeta, actionPv);
                if (reduction && actionWorst > alpha && !stopped()) {
                    actionPv.clear();
                    actionWorst = evaluateFastAction(depth, searchBeta, actionPv);
                }
                if (searchBeta < beta && actionWorst > alpha &&
                    actionWorst < beta && !stopped()) {
                    actionPv.clear();
                    actionWorst = evaluateFastAction(depth, beta, actionPv);
                }
                if (actionWorst > best) {
                    best = actionWorst;
                    bestAction = action;
                    bestPv.assign(1, action);
                    bestPv.insert(bestPv.end(), actionPv.begin(), actionPv.end());
                }
                alpha = std::max(alpha, best);
                ++actionNumber;
                if (alpha >= beta || stopped()) {
                    if (alpha >= beta && isQuiet)
                        recordQuietCutoff(action);
                    break;
                }
                continue;
            }
            std::map<InformationObservationKey, RoyalBucket> observations;
            for (const RoyalWorld& world : worlds) {
                const Move& move = world.moves.at(action);
                Position after = world.position;
                if (!after.apply_move_unchecked(move))
                    continue;
                std::optional<std::vector<Move>> afterMoves;
                if (legalDotObservations && after.side_to_move() == observer)
                    afterMoves = cached_legal(after);
                InformationObservationKey observation =
                  compact_transition_observation_key(
                    world.position, move, after, beliefs.disclosure(),
                    legalDotObservations, afterMoves ? &*afterMoves : nullptr);
                RoyalBucket& bucket = observations[observation];
                bucket.view = observation.view;
                bucket.actions.insert(action);
                bucket.worlds.emplace(world.candidate, std::move(after));
            }
            observationBuckets += observations.size();
            if (observations.empty())
                continue;
            const auto evaluate_action = [&](int searchDepth, int windowBeta,
                                             RoyalPv& actionPv) {
                int worst = Infinity;
                int natureBeta = windowBeta;
                for (const auto& [observation, bucket] : observations) {
                    (void)observation;
                    RoyalState child = make_child(bucket);
                    const bool changedSide = child.position.side_to_move() != side;
                    RoyalPv childPv;
                    const int score = solve(
                      child, searchDepth - (changedSide ? 1 : 0), alpha,
                      natureBeta, ply + 1, childPv);
                    if (score < worst) {
                        worst = score;
                        actionPv = std::move(childPv);
                    }
                    natureBeta = std::min(natureBeta, worst);
                    if (worst <= alpha || stopped())
                        break;
                }
                return worst;
            };
            const int searchBeta = actionNumber == 0 || beta >= Infinity
                                 ? beta : std::min(beta, alpha + 1);
            const bool handsTurn = std::all_of(
              observations.begin(), observations.end(), [&](const auto& entry) {
                  const RoyalBucket& bucket = entry.second;
                  return !bucket.worlds.empty() &&
                         bucket.worlds.begin()->second.side_to_move() != side;
              });
            int reduction = 0;
            if (depth >= 3 && actionNumber >= 4 && quiet[action] && handsTurn) {
                reduction = 1;
#ifndef ULTIMATE_CONSERVATIVE_LMR
                if (depth >= 6 && actionNumber >= 8)
                    ++reduction;
                if (depth >= 9 && actionNumber >= 16)
                    ++reduction;
#endif
                reduction = std::min(reduction, depth - 2);
            }
            RoyalPv actionPv;
            int actionScore = evaluate_action(depth - reduction, searchBeta, actionPv);
            if (reduction && actionScore > alpha && !stopped()) {
                actionPv.clear();
                actionScore = evaluate_action(depth, searchBeta, actionPv);
            }
            if (searchBeta < beta && actionScore > alpha &&
                actionScore < beta && !stopped()) {
                actionPv.clear();
                actionScore = evaluate_action(depth, beta, actionPv);
            }
            if (actionScore > best) {
                best = actionScore;
                bestAction = action;
                bestPv.assign(1, action);
                bestPv.insert(bestPv.end(), actionPv.begin(), actionPv.end());
            }
            alpha = std::max(alpha, best);
            ++actionNumber;
            if (alpha >= beta || stopped()) {
                if (alpha >= beta && quiet[action])
                    recordQuietCutoff(action);
                break;
            }
        }
        if (best == -Infinity)
            return observer_evaluate(state, ply);
        store(best, bestAction);
        pv = std::move(bestPv);
        return best;
    };

    const int maxDepth = std::clamp(limits.depth, 1, MaxPly - 2);
    int previousScore = 0;
    for (int depth = 1; depth <= maxDepth; ++depth) {
        RoyalPv pv;
        const bool aspirate = depth >= 3 && depth < 8;
        int alpha = aspirate ? std::max(-Infinity, previousScore - 120) : -Infinity;
        int beta = aspirate ? std::min(Infinity, previousScore + 120) : Infinity;
        int score = solve(root, depth, alpha, beta, 0, pv);
        if (!stop_ && (score <= alpha || score >= beta)) {
            pv.clear();
            score = solve(root, depth, -Infinity, Infinity, 0, pv);
        }
        if (stop_)
            break;
        previousScore = score;
        previousIterationPv = pv;
        result.score = result.worstScore = result.meanScore = score;
        result.completedDepth = depth;
        result.historyPreservingPlies = depth;
        result.principalVariation = std::move(pv);
        result.bestMove = result.principalVariation.empty()
          ? std::nullopt
          : std::optional<std::string>(result.principalVariation.front());
        result.nodes = nodes_;
        result.beliefNodes = beliefNodes;
        result.beliefTtHits = beliefTtHits;
        result.singletonHandoffs = singletonHandoffs;
        result.observationBuckets = observationBuckets;
        result.materializations = materializations;
        result.legalCacheHits = legalCacheHits;
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - beliefStart);
        if (limits.onBeliefIteration)
            limits.onBeliefIteration(result);
    }
    result.nodes = nodes_;
    result.beliefNodes = beliefNodes;
    result.beliefTtHits = beliefTtHits;
    result.singletonHandoffs = singletonHandoffs;
    result.observationBuckets = observationBuckets;
    result.materializations = materializations;
    result.legalCacheHits = legalCacheHits;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - beliefStart);
    return result;
}

std::optional<BeliefSearchResult> Search::think_factored_ghost_steppers(
  const PublicBeliefState& beliefs, const SearchLimits& limits,
  bool legalDotObservations) {
    if (!limits.factoredBeliefs || beliefs.size() < 2 ||
        !beliefs.side_to_move())
        return std::nullopt;
    const Color observer = beliefs.disclosure().observer;
    const Position& first = beliefs.concrete_worlds().begin()->second;
    std::vector<int> ghostIds;
    bool symbolicGhostTurnsSafe = true;
    for (int id = 0; id < first.piece_count(); ++id) {
        const PieceState& piece = first.piece(id);
        if (piece.type == PieceType::Ghost && piece.color != observer)
            ghostIds.push_back(id);
        // ChangeTurn automatically advances Minions. Their collision result
        // can depend on a hidden Ghost square, so that material retains the
        // concrete transition fallback. Every other quiet invisible Ghost
        // step changes only its own correlated facts plus public turn clocks.
        if (piece.alive && piece.type == PieceType::Minion)
            symbolicGhostTurnsSafe = false;
    }
    if (ghostIds.empty())
        return std::nullopt;

    using CorrelatedAssignment = std::vector<PieceState>;
    struct JointGhostState {
        Position geometry;
        std::vector<CorrelatedAssignment> assignments;
    };
    struct TupleBucket {
        Position geometry;
        bool initialized = false;
        std::vector<CorrelatedAssignment> assignments;
        std::set<std::string> actions;
    };
    JointGhostState root;
    root.geometry = first;
    for (const auto& [upn, world] : beliefs.concrete_worlds()) {
        (void)upn;
        if (world.piece_count() != first.piece_count())
            return std::nullopt;
        std::vector<PieceState> assignment;
        assignment.reserve(world.piece_count());
        for (int id = 0; id < world.piece_count(); ++id)
            assignment.push_back(world.piece(id));
        for (const int id : ghostIds) {
            const PieceState& ghost = world.piece(id);
            if (ghost.type != PieceType::Ghost || ghost.color == observer)
                return std::nullopt;
        }
        root.assignments.push_back(std::move(assignment));
    }
    std::sort(root.assignments.begin(), root.assignments.end());
    root.assignments.erase(
      std::unique(root.assignments.begin(), root.assignments.end()),
      root.assignments.end());
    if (root.assignments.size() != beliefs.size())
        return std::nullopt;
    std::uint64_t materializations = 0;
    std::uint64_t symbolicTransitions = 0;
    std::uint64_t legalCacheHits = 0;

    const auto assignment_hash = [](const CorrelatedAssignment& assignment) {
        std::uint64_t tuple = 0;
        for (const PieceState& piece : assignment) {
            std::uint64_t fact = static_cast<std::uint64_t>(piece.type);
            fact = fact * 3 + static_cast<std::uint64_t>(piece.color);
            fact = fact * 97 + piece.square;
            fact = fact * 2 + piece.onBoard;
            fact = fact * 257 + piece.action;
            fact = fact * 257 + piece.cooldown;
            fact = fact * 257 + piece.freezeCount;
            fact = fact * 257 + piece.power;
            fact = fact * 193 + static_cast<std::uint8_t>(piece.link + 1);
            fact = fact * 193 + static_cast<std::uint8_t>(piece.host + 1);
            fact = mix_key(fact ^ piece.attachmentOrder);
            fact = fact * 2 + piece.alive;
            fact = fact * 2 + piece.moved;
            fact = fact * 2 + piece.visible;
            tuple = mix_key(tuple ^ fact);
        }
        return tuple;
    };

    // Each tuple stores every varying piece fact, not merely an independent
    // square per Ghost. This makes the domain exact for arbitrary material:
    // blind captures, visibility, attachments, cooldowns, promotions, royal
    // identity, and piece death remain correlated within one assignment.
    const auto materialize = [&](const JointGhostState& state,
                                 const CorrelatedAssignment& assignment) {
        ++materializations;
        Position world = state.geometry;
        for (int id = 0; id < world.piece_count(); ++id)
            world.pieces_[id] = assignment[static_cast<std::size_t>(id)];
        world.rebuild_bitboards();
        return world;
    };
    const auto assignment_from = [](const Position& world) {
        CorrelatedAssignment assignment;
        assignment.reserve(world.piece_count());
        for (int id = 0; id < world.piece_count(); ++id)
            assignment.push_back(world.piece(id));
        return assignment;
    };
    const auto add_position = [&](TupleBucket& bucket, Position world,
                                  std::string_view action) {
        if (!bucket.initialized) {
            bucket.geometry = world;
            bucket.initialized = true;
        }
#ifndef NDEBUG
        else {
            Position globals = world;
            for (int id = 0; id < globals.piece_count(); ++id)
                globals.pieces_[id] = bucket.geometry.pieces_[id];
            globals.rebuild_bitboards();
            assert(globals.upn() == bucket.geometry.upn());
        }
#endif
        bucket.assignments.push_back(assignment_from(world));
        if (!action.empty())
            bucket.actions.emplace(action);
    };
    const auto add_assignment = [&](TupleBucket& bucket,
                                    const Position& geometry,
                                    CorrelatedAssignment assignment,
                                    std::string_view action) {
        if (!bucket.initialized) {
            bucket.geometry = geometry;
            bucket.initialized = true;
        }
        bucket.assignments.push_back(std::move(assignment));
        if (!action.empty())
            bucket.actions.emplace(action);
    };
    const auto child_from = [](TupleBucket bucket) {
        JointGhostState child;
        if (!bucket.initialized)
            return child;
        child.geometry = std::move(bucket.geometry);
        child.assignments = std::move(bucket.assignments);
        std::sort(child.assignments.begin(), child.assignments.end());
        child.assignments.erase(
          std::unique(child.assignments.begin(), child.assignments.end()),
          child.assignments.end());
        return child;
    };
    const auto start = std::chrono::steady_clock::now();
    limits_ = limits;
    start_ = start;
    nodes_ = 0;
    stop_ = false;
    useNnue_ = false;
    rootMoves_.clear();
    rootDrawMoves_.clear();
    ++generation_;
    for (auto& bySquare : history_)
        for (int& value : bySquare)
            value /= 2;
    std::set<std::string> rootRestriction;
    for (const Move& move : limits.rootMoves)
        rootRestriction.insert(first.move_to_string(move));
    const std::set<std::string> rootDraws(
      limits.rootDrawMoveStrings.begin(), limits.rootDrawMoveStrings.end());
    using GhostPv = std::vector<std::string>;
    BeliefSearchResult result;
    result.searchPath = "correlated-tuples";
    result.beliefs = result.deepBeliefs = beliefs.size();
    result.peakBeliefs = root.assignments.size();
    std::uint64_t beliefNodes = 0;
    std::uint64_t ttHits = 0;
    std::uint64_t buckets = 0;
    std::uint64_t singletonHandoffs = 0;
    std::unordered_map<std::uint64_t, std::vector<Move>> legalFrontierCache;
    legalFrontierCache.reserve(std::min<std::size_t>(
      262'144, beliefs.size() * 64));
    std::unordered_map<std::uint64_t, std::vector<std::uint16_t>>
      decisionMarkerCache;
    decisionMarkerCache.reserve(std::min<std::size_t>(
      262'144, beliefs.size() * 32));
    GhostPv previousIterationPv;
    int iterationDepth = 0;

    const auto robust_eval = [&](const JointGhostState& state, int ply) {
        int robust = Infinity;
        for (const auto& assignment : state.assignments) {
            Position world = materialize(state, assignment);
            const Color mover = world.side_to_move();
            int score = 0;
            if (const auto winner = world.forced_timeout_winner())
                score = *winner == mover ? Mate - ply : -Mate + ply;
            else {
                const bool ownKing = world.has_real_king(mover);
                const bool enemyKing = world.has_real_king(~mover);
                if (!ownKing || !enemyKing)
                    score = ownKing == enemyKing ? 0
                          : ownKing ? Mate - ply : -Mate + ply;
                else if (!world.is_checkmate_possible())
                    score = 0;
                else if (!world.has_legal_move())
                    score = world.real_king_threatened(mover)
                          ? -Mate + ply : 0;
                else
                    score = world.handcrafted_evaluate();
            }
            if (mover != observer)
                score = -score;
            robust = std::min(robust, score);
        }
        return robust == Infinity ? 0 : robust;
    };

    std::function<int(const JointGhostState&, int, int, int, int, GhostPv&)> solve;
    solve = [&](const JointGhostState& state, int depth, int alpha, int beta,
                int ply, GhostPv& pv) -> int {
        pv.clear();
        ++beliefNodes;
        nodes_ += state.assignments.size();
        result.peakBeliefs = std::max(result.peakBeliefs, state.assignments.size());
        if (stopped() || depth <= 0 || ply >= MaxPly - 1)
            return robust_eval(state, ply);
        const Color side = state.geometry.side_to_move();
        const bool maximizing = side == observer;

        if (state.assignments.size() == 1) {
            Position concrete = materialize(state, state.assignments.front());
            if (belief_stays_concrete(concrete, observer)) {
                ++singletonHandoffs;
                std::vector<Move> concretePv;
                const int score = side == observer
                  ? negamax(concrete, depth, alpha, beta, ply, concretePv)
                  : -negamax(concrete, depth, -beta, -alpha, ply, concretePv);
                for (const Move& move : concretePv)
                    pv.push_back(concrete.move_to_string(move));
                return score;
            }
        }

        std::uint64_t domainKey = state.geometry.key();
        std::uint64_t verification = mix_key(domainKey);
        for (const auto& assignment : state.assignments) {
            const std::uint64_t tuple = assignment_hash(assignment);
            domainKey ^= mix_key(tuple);
            verification += mix_key(tuple ^ 0xa4d927f06b31ce85ULL);
        }
        BeliefEntry* entry = find_belief_entry(domainKey, verification);
        if (entry && ply && entry->depth >= depth) {
            const int score = score_from_tt(entry->score, ply);
            if (entry->bound == Bound::Exact ||
                (entry->bound == Bound::Lower && score >= beta) ||
                (entry->bound == Bound::Upper && score <= alpha)) {
                ++ttHits;
                return score;
            }
        }
        const int originalAlpha = alpha;
        const int originalBeta = beta;
        const std::uint64_t ttActionHash = entry ? entry->actionHash : 0;
        const std::uint64_t pvActionHash = iterationDepth >= 2 &&
          static_cast<std::size_t>(ply) < previousIterationPv.size()
          ? string_key(previousIterationPv[static_cast<std::size_t>(ply)]) : 0;

        struct Prepared {
            Position world;
            std::map<std::string, Move> moves;
            const std::vector<PieceState>* assignment = nullptr;
        };
        std::vector<Prepared> prepared;
        prepared.reserve(state.assignments.size());
        std::set<std::string> common;
        bool firstWorld = true;
        for (const auto& assignment : state.assignments) {
            if (stopped())
                return robust_eval(state, ply);
            Prepared item;
            item.assignment = &assignment;
            item.world = materialize(state, assignment);
            std::set<std::string> ambiguous;
            const std::uint64_t worldKey = item.world.key();
            auto cached = legalFrontierCache.find(worldKey);
            if (cached == legalFrontierCache.end())
                cached = legalFrontierCache.emplace(
                  worldKey, item.world.legal_moves()).first;
            else
                ++legalCacheHits;
            for (const Move& move : cached->second) {
                const std::string notation = item.world.move_to_string(move);
                if (ambiguous.count(notation))
                    continue;
                const auto [it, inserted] = item.moves.emplace(notation, move);
                if (!inserted) {
                    item.moves.erase(it);
                    ambiguous.insert(notation);
                }
            }
            if (maximizing) {
                std::set<std::string> legal;
                for (const auto& [notation, move] : item.moves) {
                    (void)move;
                    legal.insert(notation);
                }
                if (firstWorld)
                    common = std::move(legal);
                else {
                    std::set<std::string> intersection;
                    std::set_intersection(common.begin(), common.end(),
                                          legal.begin(), legal.end(),
                                          std::inserter(intersection,
                                                        intersection.begin()));
                    common = std::move(intersection);
                }
            }
            firstWorld = false;
            prepared.push_back(std::move(item));
        }

        if (!maximizing) {
            std::map<InformationObservationKey, TupleBucket> observations;
            std::optional<Position> symbolicGeometry;
            std::optional<InformationObservationKey> symbolicBaseObservation;
            for (const Prepared& item : prepared) {
                if (stopped())
                    break;
                for (const auto& [notation, move] : item.moves) {
                    if (ply == 0 && !rootRestriction.empty() &&
                        !rootRestriction.count(notation))
                        continue;
                    const int actor = item.world.piece_on(move.from);
                    bool symbolicQuietGhost = symbolicGhostTurnsSafe &&
                      !state.geometry.has_forced_action() &&
                      actor != Position::NoPiece &&
                      item.world.piece(actor).type == PieceType::Ghost &&
                      item.world.piece(actor).color == side &&
                      !item.world.piece(actor).visible &&
                      item.world.piece_on(move.to) == Position::NoPiece &&
                      move.kind == MoveKind::Normal;
                    if (symbolicQuietGhost) {
                        const int toFile = move.to % Position::BoardFiles;
                        const int toRank = move.to / Position::BoardFiles;
                        for (const PieceState& piece : *item.assignment) {
                            if (!piece.alive || !piece.onBoard ||
                                piece.color == side ||
                                (piece.type != PieceType::King &&
                                 piece.type != PieceType::Jester))
                                continue;
                            const int file = piece.square % Position::BoardFiles;
                            const int rank = piece.square / Position::BoardFiles;
                            if (std::max(std::abs(file - toFile),
                                         std::abs(rank - toRank)) <= 1) {
                                symbolicQuietGhost = false;
                                break;
                            }
                        }
                    }
                    if (symbolicQuietGhost) {
                        std::vector<PieceState> next = *item.assignment;
                        next[static_cast<std::size_t>(actor)].square = move.to;
                        next[static_cast<std::size_t>(actor)].moved = true;
                        next[static_cast<std::size_t>(actor)].visible = false;
                        for (PieceState& piece : next) {
                            if (piece.alive && piece.type == PieceType::Angel &&
                                !piece.onBoard && piece.host == actor)
                                piece.square = move.to;
                            if (piece.alive && piece.cooldown)
                                --piece.cooldown;
                        }
                        if (!symbolicGeometry) {
                            symbolicGeometry = item.world;
                            if (!symbolicGeometry->apply_move_unchecked(move))
                                throw std::runtime_error(
                                  "legal symbolic Ghost edge failed prototype application");
                            symbolicBaseObservation =
                              compact_transition_observation_key(
                                item.world, move, *symbolicGeometry,
                                beliefs.disclosure(), false, nullptr);
                        }
                        InformationObservationKey observation =
                          *symbolicBaseObservation;
                        if (legalDotObservations &&
                            symbolicGeometry->side_to_move() == observer) {
                            const std::uint64_t markerKey = assignment_hash(next) ^
                              mix_key(symbolicGeometry->key());
                            auto markers = decisionMarkerCache.find(markerKey);
                            if (markers == decisionMarkerCache.end()) {
                                Position after = *symbolicGeometry;
                                ++materializations;
                                for (int id = 0; id < after.piece_count(); ++id)
                                    after.pieces_[id] =
                                      next[static_cast<std::size_t>(id)];
                                after.rebuild_bitboards();
                                const std::uint64_t afterKey = after.key();
                                auto cached = legalFrontierCache.find(afterKey);
                                if (cached == legalFrontierCache.end())
                                    cached = legalFrontierCache.emplace(
                                      afterKey, after.legal_moves()).first;
                                else
                                    ++legalCacheHits;
                                markers = decisionMarkerCache.emplace(
                                  markerKey, compact_decision_markers(
                                    after, beliefs.disclosure(),
                                    &cached->second)).first;
                            } else
                                ++legalCacheHits;
                            observation.decisionMarkers = markers->second;
                        }
                        add_assignment(observations[observation],
                                       *symbolicGeometry, std::move(next),
                                       notation);
                        ++symbolicTransitions;
                        continue;
                    }
                    Position after = item.world;
                    if (!after.apply_move_unchecked(move))
                        continue;
                    std::optional<std::vector<Move>> afterMoves;
                    if (legalDotObservations && after.side_to_move() == observer)
                        afterMoves = after.legal_moves();
                    const auto observation = compact_transition_observation_key(
                      item.world, move, after, beliefs.disclosure(),
                      legalDotObservations, afterMoves ? &*afterMoves : nullptr);
                    add_position(observations[observation], std::move(after),
                                 notation);
                }
            }
            buckets += observations.size();
            std::vector<decltype(observations)::iterator> ordered;
            ordered.reserve(observations.size());
            for (auto it = observations.begin(); it != observations.end(); ++it)
                ordered.push_back(it);
            std::stable_sort(ordered.begin(), ordered.end(), [&](auto lhs, auto rhs) {
                const auto priority = [&](const InformationObservationKey& observation) {
                    int value = 0;
                    for (const std::string& action : observations[observation].actions) {
                        const std::uint64_t hash = string_key(action);
                        value = std::max(value, hash == ttActionHash ? 2
                                              : hash == pvActionHash ? 1 : 0);
                    }
                    return value;
                };
                return priority(lhs->first) > priority(rhs->first);
            });
            int best = Infinity;
            std::string bestAction;
            GhostPv bestPv;
            int observationNumber = 0;
            for (const auto iterator : ordered) {
                const std::string representativeAction =
                  iterator->second.actions.empty()
                  ? std::string() : *iterator->second.actions.begin();
                JointGhostState child = child_from(std::move(iterator->second));
                if (child.assignments.empty())
                    throw std::runtime_error(
                      "correlated tuple bucket has incompatible public globals");
                GhostPv childPv;
                const bool changedSide = child.geometry.side_to_move() != side;
                const int childDepth = depth - (changedSide ? 1 : 0);
                int score = observationNumber++ == 0 || beta >= Infinity
                  ? solve(child, childDepth, alpha, beta, ply + 1, childPv)
                  : solve(child, childDepth, beta - 1, beta, ply + 1, childPv);
                if (observationNumber > 1 && score > alpha && score < beta &&
                    !stopped()) {
                    childPv.clear();
                    score = solve(child, childDepth, alpha, beta,
                                  ply + 1, childPv);
                }
                if (ply == 0 && !representativeAction.empty() &&
                    rootDraws.count(representativeAction))
                    score = std::min(score, 0);
                if (score < best) {
                    best = score;
                    bestAction = representativeAction;
                    bestPv.clear();
                    if (!bestAction.empty())
                        bestPv.push_back(bestAction);
                    bestPv.insert(bestPv.end(), childPv.begin(), childPv.end());
                }
                beta = std::min(beta, best);
                if (alpha >= beta || stopped())
                    break;
            }
            if (best == Infinity)
                return robust_eval(state, ply);
            if (!stop_ && ply) {
                BeliefEntry& replacement = replacement_belief_entry(
                  domainKey, verification);
                replacement.key = domainKey;
                replacement.verification = verification;
                replacement.actionHash = bestAction.empty() ? 0 : string_key(bestAction);
                replacement.score = static_cast<std::int16_t>(score_to_tt(best, ply));
                replacement.depth = static_cast<std::int8_t>(std::min(depth, 127));
                replacement.bound = best <= originalAlpha ? Bound::Upper
                                  : best >= originalBeta ? Bound::Lower : Bound::Exact;
                replacement.generation = generation_;
            }
            pv = std::move(bestPv);
            return best;
        }

        if (ply == 0 && !rootRestriction.empty()) {
            std::set<std::string> restricted;
            std::set_intersection(common.begin(), common.end(),
                                  rootRestriction.begin(), rootRestriction.end(),
                                  std::inserter(restricted, restricted.begin()));
            common = std::move(restricted);
        }
        if (ply == 0) {
            result.commonMoves = common.size();
            result.candidates = common.size();
        }
        std::vector<std::string> orderedActions(common.begin(), common.end());
        std::map<std::string, int> actionPriority;
        std::map<std::string, bool> actionQuiet;
        for (const std::string& action : orderedActions) {
            const std::uint64_t hash = string_key(action);
            int priority = hash == ttActionHash ? 2'000'000
                         : hash == pvActionHash ? 1'500'000 : -Infinity;
            bool quiet = true;
            for (const Prepared& item : prepared) {
                const auto found = item.moves.find(action);
                if (found == item.moves.end()) {
                    quiet = false;
                    continue;
                }
                priority = std::max(priority, move_score(
                  item.world, found->second, nullptr, ply));
                quiet = quiet && found->second.kind == MoveKind::Normal &&
                        !item.world.is_capture(found->second);
            }
            actionPriority.emplace(action, priority);
            actionQuiet.emplace(action, quiet);
        }
        std::stable_sort(orderedActions.begin(), orderedActions.end(),
          [&](const std::string& lhs, const std::string& rhs) {
              return actionPriority.at(lhs) > actionPriority.at(rhs);
          });
        const auto recordQuietCutoff = [&](const std::string& action) {
            if (prepared.empty())
                return;
            const Move& representative = prepared.front().moves.at(action);
            if (ply < static_cast<int>(killers_.size()) &&
                !(representative == killers_[ply][0])) {
                killers_[ply][1] = killers_[ply][0];
                killers_[ply][0] = representative;
            }
            std::set<PieceType> attackers;
            for (const Prepared& item : prepared) {
                const Move& move = item.moves.at(action);
                const int attacker = item.world.piece_on(move.from);
                if (attacker != Position::NoPiece)
                    attackers.insert(item.world.piece(attacker).type);
            }
            for (const PieceType type : attackers) {
                int& value = history_[static_cast<std::size_t>(type)]
                                     [representative.to];
                value = std::min(50'000, value + depth * depth);
            }
        };
        int best = -Infinity;
        std::string bestAction;
        GhostPv bestPv;
        int actionNumber = 0;
        const bool pvNode = beta - alpha > 1;
        bool quietPruningNode = ply > 0 && !pvNode && depth <= 3 &&
          std::abs(alpha) < MateThreshold;
        for (const Prepared& item : prepared)
            quietPruningNode = quietPruningNode &&
              !item.world.has_forced_action() &&
              item.world.supports_ordinary_exchange() &&
              (item.world.pieces(side, PieceType::Jester) ||
               !item.world.real_king_threatened(side));
        const int staticEval = quietPruningNode ? robust_eval(state, ply) : 0;
        for (const std::string& action : orderedActions) {
            if (stopped())
                break;
            const bool quiet = actionQuiet.at(action);
            if (ply == 0 && rootDraws.count(action)) {
                best = std::max(best, 0);
                if (bestAction.empty()) {
                    bestAction = action;
                    bestPv = {action};
                }
                ++actionNumber;
                continue;
            }
#ifndef ULTIMATE_DISABLE_LATE_MOVE_PRUNING
            if (quietPruningNode && quiet && depth <= 2 &&
                actionNumber >= 6 + 5 * depth) {
                ++actionNumber;
                continue;
            }
#endif
#ifndef ULTIMATE_DISABLE_FORWARD_FUTILITY
            if (quietPruningNode && quiet && actionNumber >= 4 + 3 * depth &&
                staticEval + 140 * depth <= alpha) {
                ++actionNumber;
                continue;
            }
#endif
            std::map<InformationObservationKey, TupleBucket> observations;
            for (const Prepared& item : prepared) {
                if (stopped())
                    break;
                const Move& move = item.moves.at(action);
                Position after = item.world;
                if (!after.apply_move_unchecked(move))
                    continue;
                std::optional<std::vector<Move>> afterMoves;
                if (legalDotObservations && after.side_to_move() == observer)
                    afterMoves = after.legal_moves();
                const auto observation = compact_transition_observation_key(
                  item.world, move, after, beliefs.disclosure(),
                  legalDotObservations, afterMoves ? &*afterMoves : nullptr);
                add_position(observations[observation], std::move(after), action);
            }
            buckets += observations.size();
            const auto evaluateAction = [&](int searchDepth, int windowBeta,
                                            GhostPv& actionPv) {
                int worst = Infinity;
                int natureBeta = windowBeta;
                for (const auto& [observation, bucket] : observations) {
                    (void)observation;
                    JointGhostState child = child_from(bucket);
                    if (child.assignments.empty())
                        throw std::runtime_error(
                          "correlated tuple bucket has incompatible public globals");
                    GhostPv childPv;
                    const bool changedSide = child.geometry.side_to_move() != side;
                    const int score = solve(
                      child, searchDepth - (changedSide ? 1 : 0), alpha,
                      natureBeta, ply + 1, childPv);
                    if (score < worst) {
                        worst = score;
                        actionPv = std::move(childPv);
                    }
                    natureBeta = std::min(natureBeta, worst);
                    if (worst <= alpha || stopped())
                        break;
                }
                return worst;
            };
            bool handsTurn = true;
            for (const auto& [observation, bucket] : observations) {
                (void)observation;
                handsTurn = handsTurn && bucket.initialized &&
                            bucket.geometry.side_to_move() != side;
            }
            int reduction = 0;
            const int searchBeta = actionNumber == 0 || beta >= Infinity
                                 ? beta : std::min(beta, alpha + 1);
            if (depth >= 3 && actionNumber >= 4 && quiet && handsTurn) {
                reduction = 1;
#ifndef ULTIMATE_CONSERVATIVE_LMR
                if (depth >= 6 && actionNumber >= 8)
                    ++reduction;
                if (depth >= 9 && actionNumber >= 16)
                    ++reduction;
#endif
                reduction = std::min(reduction, depth - 2);
            }
            GhostPv actionPv;
            int worst = evaluateAction(depth - reduction, searchBeta, actionPv);
            if (reduction && worst > alpha && !stopped()) {
                actionPv.clear();
                worst = evaluateAction(depth, searchBeta, actionPv);
            }
            if (searchBeta < beta && worst > alpha && worst < beta && !stopped()) {
                actionPv.clear();
                worst = evaluateAction(depth, beta, actionPv);
            }
            if (worst > best) {
                best = worst;
                bestAction = action;
                bestPv.assign(1, action);
                bestPv.insert(bestPv.end(), actionPv.begin(), actionPv.end());
            }
            alpha = std::max(alpha, best);
            ++actionNumber;
            if (alpha >= beta || stopped()) {
                if (alpha >= beta && quiet)
                    recordQuietCutoff(action);
                break;
            }
        }
        if (best == -Infinity)
            return robust_eval(state, ply);
        if (!stop_ && ply) {
            BeliefEntry& replacement = replacement_belief_entry(domainKey, verification);
            replacement.key = domainKey;
            replacement.verification = verification;
            replacement.actionHash = bestAction.empty() ? 0 : string_key(bestAction);
            replacement.score = static_cast<std::int16_t>(score_to_tt(best, ply));
            replacement.depth = static_cast<std::int8_t>(std::min(depth, 127));
            replacement.bound = best <= originalAlpha ? Bound::Upper
                              : best >= originalBeta ? Bound::Lower : Bound::Exact;
            replacement.generation = generation_;
        }
        pv = std::move(bestPv);
        return best;
    };

    const int maxDepth = std::clamp(limits.depth, 1, MaxPly - 2);
    int previousScore = 0;
    for (int depth = 1; depth <= maxDepth; ++depth) {
        iterationDepth = depth;
        GhostPv pv;
        const bool aspirate = depth >= 3 && depth < 8;
        int alpha = aspirate ? std::max(-Infinity, previousScore - 120) : -Infinity;
        int beta = aspirate ? std::min(Infinity, previousScore + 120) : Infinity;
        int score = solve(root, depth, alpha, beta, 0, pv);
        if (!stop_ && (score <= alpha || score >= beta)) {
            pv.clear();
            score = solve(root, depth, -Infinity, Infinity, 0, pv);
        }
        if (stop_)
            break;
        previousScore = score;
        previousIterationPv = pv;
        result.score = result.worstScore = result.meanScore = score;
        result.completedDepth = depth;
        result.historyPreservingPlies = depth;
        result.principalVariation = std::move(pv);
        result.bestMove = result.principalVariation.empty()
          ? std::nullopt
          : std::optional<std::string>(result.principalVariation.front());
        result.nodes = nodes_;
        result.beliefNodes = beliefNodes;
        result.beliefTtHits = ttHits;
        result.singletonHandoffs = singletonHandoffs;
        result.observationBuckets = buckets;
        result.materializations = materializations;
        result.symbolicTransitions = symbolicTransitions;
        result.legalCacheHits = legalCacheHits;
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start);
        if (limits.onBeliefIteration)
            limits.onBeliefIteration(result);
    }
    result.nodes = nodes_;
    result.beliefNodes = beliefNodes;
    result.beliefTtHits = ttHits;
    result.singletonHandoffs = singletonHandoffs;
    result.observationBuckets = buckets;
    result.materializations = materializations;
    result.symbolicTransitions = symbolicTransitions;
    result.legalCacheHits = legalCacheHits;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
    return result;
}

BeliefSearchResult Search::think_beliefs(const PublicBeliefState& beliefs,
                                         const SearchLimits& limits,
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
    if (beliefs.size() == 1 && belief_stays_concrete(
          beliefs.concrete_worlds().begin()->second,
          beliefs.disclosure().observer)) {
        Position position = beliefs.positions().front();
        const auto converted = [&](const SearchResult& exact) {
            const int perspective =
              position.side_to_move() == beliefs.disclosure().observer ? 1 : -1;
            BeliefSearchResult iteration;
            iteration.searchPath = "concrete-singleton";
            iteration.beliefs = iteration.deepBeliefs = 1;
            iteration.bestMove = exact.bestMove
              ? std::optional<std::string>(
                  position.move_to_string(*exact.bestMove))
              : std::nullopt;
            iteration.score = iteration.worstScore =
              iteration.meanScore = perspective * exact.score;
            if (exact.mateActions)
                iteration.mateActions = perspective * *exact.mateActions;
            iteration.completedDepth = exact.completedDepth;
            iteration.historyPreservingPlies = exact.completedDepth;
            iteration.nodes = exact.nodes;
            iteration.commonMoves = position.legal_moves().size();
            iteration.candidates = iteration.commonMoves;
            iteration.singletonHandoffs = 1;
            iteration.peakBeliefs = 1;
            for (const Move& move : exact.principalVariation)
                iteration.principalVariation.push_back(
                  position.move_to_string(move));
            iteration.elapsed = std::chrono::duration_cast<
              std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - beliefStart);
            return iteration;
        };
        SearchLimits exactLimits = limits;
        if (limits.onBeliefIteration)
            exactLimits.onIteration = [&](const SearchResult& iteration) {
                limits.onBeliefIteration(converted(iteration));
            };
        result = converted(think(position, exactLimits));
        return result;
    }

    if (const auto factored = think_factored_royals(
          beliefs, limits, legalDotObservations))
        return *factored;
    if (const auto factored = think_factored_ghost_steppers(
          beliefs, limits, legalDotObservations))
        return *factored;

    limits_ = limits;
    start_ = beliefStart;
    nodes_ = 0;
    stop_ = false;
    useNnue_ = UltimateNnue::enabled();
    rootMoves_.clear();
    rootDrawMoves_.clear();
    ++generation_;
    for (auto& bySquare : history_)
        for (int& value : bySquare)
            value /= 2;
    const Color observer = beliefs.disclosure().observer;
    result.deepBeliefs = beliefs.size();
    result.commonMoves = beliefs.common_moves().size();
    result.candidates = result.commonMoves;
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
                else if (!position.has_legal_move())
                    score = position.real_king_threatened(mover)
                          ? -Mate + ply : 0;
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
    struct PreparedBeliefWorld {
        const Position* position = nullptr;
        std::map<std::string, Move> moves;
    };
    struct CachedBeliefWorld {
        Position position;
        std::vector<Move> legalMoves;
    };
    BeliefPv previousIterationPv;
    int currentIterationDepth = 0;
    std::uint64_t beliefNodes = 0;
    std::uint64_t beliefTtHits = 0;
    std::uint64_t singletonHandoffs = 0;
    std::uint64_t observationBuckets = 0;
    std::size_t peakBeliefs = beliefs.size();
    std::function<int(const PublicBeliefState&, int, int, int, int, BeliefPv&)>
      solve = [&](const PublicBeliefState& state, int depth, int alpha, int beta,
                  int ply, BeliefPv& pv) -> int {
        pv.clear();
        ++beliefNodes;
        peakBeliefs = std::max(peakBeliefs, state.size());
        if (stopped())
            return observer_evaluate(state, ply);
        const std::optional<Color> side = state.side_to_move();
        if (!side)
            return 0;

        // Information can collapse after a capture, reveal, or legal-dot
        // observation. Hand the exact child to the mature concrete engine at
        // any depth, not only when the root happened to be a singleton.
        if (state.size() == 1 && ply < MaxPly - 1 &&
            belief_stays_concrete(state.concrete_worlds().begin()->second,
                                  observer)) {
            ++singletonHandoffs;
            Position position = state.concrete_worlds().begin()->second;
            if (useNnue_)
                UltimateNnue::refresh(position, accumulators_[ply]);
            std::vector<Move> concretePv;
            int score = 0;
            if (*side == observer)
                score = negamax(position, depth, alpha, beta, ply, concretePv);
            else
                score = -negamax(position, depth, -beta, -alpha, ply, concretePv);
            for (const Move& move : concretePv)
                pv.push_back(position.move_to_string(move));
            return score;
        }

        nodes_ += state.size();
        if (depth <= 0 || ply >= MaxPly - 1)
            return observer_evaluate(state, ply);

        const int originalAlpha = alpha;
        const int originalBeta = beta;
        const std::uint64_t observationMode = legalDotObservations
                                            ? 0x7b1d5a6f3c9428e1ULL
                                            : 0x13c6e8a9457fb20dULL;
        const std::uint64_t beliefKey = state.transposition_key() ^
                                        observationMode;
        const std::uint64_t beliefVerification =
          state.transposition_verification() ^ mix_key(observationMode);
        BeliefEntry* ttEntry = find_belief_entry(
          beliefKey, beliefVerification);
        const bool adjustedRoot = ply == 0;
        if (ttEntry && !adjustedRoot && ttEntry->depth >= depth) {
            const int ttScore = score_from_tt(ttEntry->score, ply);
            bool ghostDomain = false;
            for (const auto& [upn, world] : state.concrete_worlds()) {
                (void)upn;
                for (int id = 0; id < world.piece_count(); ++id) {
                    const PieceState& piece = world.piece(id);
                    ghostDomain = ghostDomain ||
                      (piece.alive && piece.onBoard &&
                       piece.type == PieceType::Ghost &&
                       piece.color != observer);
                }
            }
            // The current concrete-world digest is sufficient for monotone
            // royal candidates. It is not yet a complete Ghost-history key:
            // a Ghost domain can collapse and later re-expand after an
            // unobserved step. Retain every Ghost-domain entry as an ordering
            // hint only, including entries labelled Exact. Reusing such an
            // entry as a value makes an unrestricted root search depend on
            // move order even though independently restricted actions agree.
            if (!ghostDomain &&
                (ttEntry->bound == Bound::Exact ||
                 (ttEntry->bound == Bound::Lower && ttScore >= beta) ||
                 (ttEntry->bound == Bound::Upper && ttScore <= alpha)))
            {
                ++beliefTtHits;
                return ttScore;
            }
        }
        const std::uint64_t ttActionHash = ttEntry ? ttEntry->actionHash : 0;
        const std::uint64_t pvActionHash =
          currentIterationDepth >= 2 &&
          static_cast<std::size_t>(ply) < previousIterationPv.size()
            ? string_key(previousIterationPv[static_cast<std::size_t>(ply)])
            : 0;
        const auto store = [&](int score, std::string_view action) {
            if (stop_ || adjustedRoot)
                return;
            BeliefEntry& replacement = replacement_belief_entry(
              beliefKey, beliefVerification);
            replacement.key = beliefKey;
            replacement.verification = beliefVerification;
            replacement.actionHash = action.empty() ? 0 : string_key(action);
            replacement.score = static_cast<std::int16_t>(
              std::clamp(score_to_tt(score, ply), -Infinity, Infinity));
            replacement.depth = static_cast<std::int8_t>(
              std::min(depth, 127));
            replacement.bound = score <= originalAlpha ? Bound::Upper
                              : score >= originalBeta  ? Bound::Lower
                                                       : Bound::Exact;
            replacement.generation = generation_;
        };

        const bool maximizing = *side == observer;
        if (!maximizing) {
            struct CompactAdversarialBucket {
                std::map<std::string, CachedBeliefWorld> worlds;
                std::set<std::string> actions;
            };
            std::map<InformationObservationKey, CompactAdversarialBucket>
              observations;
            for (const auto& [upn, world] : state.concrete_worlds()) {
                if (stopped())
                    break;
                (void)upn;
                for (const Move& move : state.cached_legal_moves(upn, world)) {
                    const std::string notation = world.move_to_string(move);
                    if (ply == 0 && !rootRestriction.empty() &&
                        !rootRestriction.count(notation))
                        continue;
                    Position after = world;
                    // `move` came from this exact world's cached legal
                    // frontier. Apply it once; Position::make_move would
                    // regenerate that entire frontier merely to revalidate
                    // the same action.
                    if (!after.apply_move_unchecked(move))
                        continue;
                    std::vector<Move> afterLegalMoves = after.legal_moves();
                    InformationObservationKey observation =
                      compact_transition_observation_key(
                        world, move, after, state.disclosure(),
                        legalDotObservations, &afterLegalMoves);
                    CompactAdversarialBucket& bucket =
                      observations[std::move(observation)];
                    bucket.actions.insert(notation);
                    bucket.worlds.emplace(after.upn(), CachedBeliefWorld{
                      std::move(after), std::move(afterLegalMoves)});
                }
            }
            observationBuckets += observations.size();
            std::vector<decltype(observations)::iterator> order;
            order.reserve(observations.size());
            for (auto iterator = observations.begin();
                 iterator != observations.end(); ++iterator)
                order.push_back(iterator);
            std::map<const CompactAdversarialBucket*, int> priorities;
            for (const auto& [observation, bucket] : observations) {
                (void)observation;
                int priority = 0;
                for (const std::string& action : bucket.actions) {
                    const std::uint64_t hash = string_key(action);
                    if (hash == ttActionHash)
                        priority = std::max(priority, 2);
                    else if (hash == pvActionHash)
                        priority = std::max(priority, 1);
                }
                priorities.emplace(&bucket, priority);
            }
            std::stable_sort(order.begin(), order.end(),
              [&](const auto lhs, const auto rhs) {
                  return priorities.at(&lhs->second) >
                         priorities.at(&rhs->second);
              });

            int best = Infinity;
            std::string bestAction;
            BeliefPv bestPv;
            int observationNumber = 0;
            for (const auto iterator : order) {
                const InformationObservationKey& observation = iterator->first;
                const CompactAdversarialBucket& bucket = iterator->second;
                PublicBeliefState child(state.disclosure());
                std::string error;
                bool valid = true;
                for (const auto& [upn, world] : bucket.worlds)
                    valid = valid && child.add_compact_prevalidated(
                      upn, world.position, observation.view,
                      world.legalMoves, &error);
                if (!valid || child.empty())
                    throw std::runtime_error(
                      "one adversarial observation bucket crosses public views");
                const bool changedSide = child.side_to_move() &&
                                         *child.side_to_move() != *side;
                BeliefPv childPv;
                const int childDepth = depth - (changedSide ? 1 : 0);
                int score;
                if (observationNumber == 0 || beta >= Infinity)
                    score = solve(child, childDepth, alpha, beta,
                                  ply + 1, childPv);
                else {
                    score = solve(child, childDepth, beta - 1, beta,
                                  ply + 1, childPv);
                    if (score > alpha && score < beta)
                        score = solve(child, childDepth, alpha, beta,
                                      ply + 1, childPv);
                }
                ++observationNumber;
                if (ply == 0 && std::any_of(
                      bucket.actions.begin(), bucket.actions.end(),
                      [&](const std::string& action) {
                          return rootDraws.count(action) != 0;
                      }))
                    score = std::min(score, 0);
                if (score < best) {
                    best = score;
                    bestAction = bucket.actions.empty()
                               ? std::string() : *bucket.actions.begin();
                    bestPv.clear();
                    if (!bestAction.empty())
                        bestPv.push_back(bestAction);
                    bestPv.insert(bestPv.end(), childPv.begin(), childPv.end());
                }
                beta = std::min(beta, best);
                if (alpha >= beta || stopped())
                    break;
            }
            if (best == Infinity)
                return observer_evaluate(state, ply);
            store(best, bestAction);
            pv = std::move(bestPv);
            return best;
        }

        std::set<std::string> actionSet;
        std::vector<PreparedBeliefWorld> prepared;
        prepared.reserve(state.size());
        bool firstWorld = true;
        for (const auto& [upn, world] : state.concrete_worlds()) {
            if (stopped())
                return observer_evaluate(state, ply);
            (void)upn;
            PreparedBeliefWorld entry;
            entry.position = &world;
            std::set<std::string> worldAmbiguous;
            for (const Move& move : state.cached_legal_moves(upn, world)) {
                const std::string notation = world.move_to_string(move);
                if (worldAmbiguous.count(notation))
                    continue;
                const auto [iterator, inserted] = entry.moves.emplace(notation, move);
                if (!inserted) {
                    entry.moves.erase(iterator);
                    worldAmbiguous.insert(notation);
                }
            }
            std::set<std::string> legal;
            for (const auto& [notation, move] : entry.moves) {
                (void)move;
                legal.insert(notation);
            }
            if (firstWorld)
                actionSet = std::move(legal);
            else {
                std::set<std::string> intersection;
                std::set_intersection(
                  actionSet.begin(), actionSet.end(), legal.begin(), legal.end(),
                  std::inserter(intersection, intersection.begin()));
                actionSet = std::move(intersection);
            }
            prepared.push_back(std::move(entry));
            firstWorld = false;
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

        int best = -Infinity;
        std::string bestAction;
        BeliefPv bestPv;
        std::vector<std::string> orderedActions(actionSet.begin(), actionSet.end());
        std::map<std::string, int> actionPriorities;
        std::map<std::string, bool> actionQuiets;
        for (const std::string& action : orderedActions) {
            const std::uint64_t hash = string_key(action);
            int priority = hash == ttActionHash ? 2'000'000
                         : hash == pvActionHash ? 1'500'000 : -Infinity;
            if (priority < 1'500'000)
                for (const PreparedBeliefWorld& entry : prepared) {
                    const auto found = entry.moves.find(action);
                    if (found != entry.moves.end())
                        priority = std::max(priority, move_score(
                          *entry.position, found->second, nullptr, ply));
                }
            actionPriorities.emplace(action, priority);
            bool quiet = true;
            for (const PreparedBeliefWorld& entry : prepared) {
                const auto found = entry.moves.find(action);
                quiet = quiet && found != entry.moves.end() &&
                        found->second.kind == MoveKind::Normal &&
                        !entry.position->is_capture(found->second);
            }
            actionQuiets.emplace(action, quiet);
        }
        std::stable_sort(orderedActions.begin(), orderedActions.end(),
          [&](const std::string& lhs, const std::string& rhs) {
              return actionPriorities.at(lhs) > actionPriorities.at(rhs);
          });
        const auto recordQuietCutoff = [&](const std::string& action) {
            if (prepared.empty())
                return;
            const Move& representative = prepared.front().moves.at(action);
            if (ply < static_cast<int>(killers_.size()) &&
                !(representative == killers_[ply][0])) {
                killers_[ply][1] = killers_[ply][0];
                killers_[ply][0] = representative;
            }
            std::set<PieceType> attackers;
            for (const PreparedBeliefWorld& entry : prepared) {
                const Move& move = entry.moves.at(action);
                const int attacker = entry.position->piece_on(move.from);
                if (attacker != Position::NoPiece)
                    attackers.insert(entry.position->piece(attacker).type);
            }
            for (const PieceType type : attackers) {
                int& value = history_[static_cast<std::size_t>(type)]
                                     [representative.to];
                value = std::min(50'000, value + depth * depth);
            }
        };
        int actionNumber = 0;
        const bool pvNode = beta - alpha > 1;
        bool quietPruningNode = ply > 0 && !pvNode && depth <= 3 &&
          std::abs(alpha) < MateThreshold;
        for (const PreparedBeliefWorld& entry : prepared) {
            const Position& position = *entry.position;
            quietPruningNode = quietPruningNode &&
              !position.has_forced_action() &&
              position.supports_ordinary_exchange() &&
              (position.pieces(*side, PieceType::Jester) ||
               !position.real_king_threatened(*side));
        }
        const int staticEval = quietPruningNode
                             ? observer_evaluate(state, ply) : 0;
        for (const std::string& action : orderedActions) {
            if (stopped())
                break;
            const bool quiet = actionQuiets.at(action);
#ifndef ULTIMATE_DISABLE_LATE_MOVE_PRUNING
            if (quietPruningNode && quiet && depth <= 2 &&
                actionNumber >= 6 + 5 * depth) {
                ++actionNumber;
                continue;
            }
#endif
#ifndef ULTIMATE_DISABLE_FORWARD_FUTILITY
            if (quietPruningNode && quiet &&
                actionNumber >= 4 + 3 * depth &&
                staticEval + 140 * depth <= alpha) {
                ++actionNumber;
                continue;
            }
#endif
            int actionWorst = ply == 0 && rootDraws.count(action)
                            ? 0 : Infinity;
            BeliefPv actionPv;
            if (actionWorst == Infinity) {
                std::map<InformationObservationKey,
                         std::map<std::string, CachedBeliefWorld>> observations;
                bool incompatible = false;
                for (const PreparedBeliefWorld& entry : prepared) {
                    if (stopped()) {
                        incompatible = true;
                        break;
                    }
                    const auto found = entry.moves.find(action);
                    if (found == entry.moves.end()) {
                        incompatible = true;
                        continue;
                    }
                    const Position& before = *entry.position;
                    Position after = before;
                    if (!after.apply_move_unchecked(found->second)) {
                        incompatible = true;
                        continue;
                    }
                    std::vector<Move> afterLegalMoves = after.legal_moves();
                    InformationObservationKey observation =
                      compact_transition_observation_key(
                        before, found->second, after, state.disclosure(),
                        legalDotObservations, &afterLegalMoves);
                    observations[std::move(observation)].emplace(
                      after.upn(), CachedBeliefWorld{
                        std::move(after), std::move(afterLegalMoves)});
                }
                if (observations.empty() || incompatible)
                    continue;
                observationBuckets += observations.size();

                // The concrete action is public, so an opponent action may
                // itself eliminate worlds where that action was illegal. For
                // either mover, nature then selects the worst compatible
                // public/private observation bucket. Future strategy branches
                // only after that bucket has actually been observed.
                const auto evaluateAction = [&](int searchDepth, int windowBeta,
                                                BeliefPv& evaluatedPv) {
                    int worst = Infinity;
                    int natureBeta = windowBeta;
                    for (const auto& [observation, bucket] : observations) {
                        PublicBeliefState child(state.disclosure());
                        std::string error;
                        bool valid = true;
                        for (const auto& [upn, world] : bucket)
                            valid = valid && child.add_compact_prevalidated(
                              upn, world.position, observation.view,
                              world.legalMoves, &error);
                        if (!valid || child.empty())
                            throw std::runtime_error(
                              "one known-action observation bucket crosses public views");
                        const bool changedSide = child.side_to_move() &&
                                                 *child.side_to_move() != *side;
                        BeliefPv childPv;
                        const int score = solve(
                          child, searchDepth - (changedSide ? 1 : 0),
                          alpha, natureBeta, ply + 1, childPv);
                        if (score < worst) {
                            worst = score;
                            evaluatedPv = std::move(childPv);
                        }
                        natureBeta = std::min(natureBeta, worst);
                        if (worst <= alpha || stopped())
                            break;
                    }
                    return worst;
                };
                const int searchBeta = actionNumber == 0 || beta >= Infinity
                                     ? beta : std::min(beta, alpha + 1);
                bool handsTurn = true;
                for (const auto& [observation, bucket] : observations) {
                    (void)observation;
                    for (const auto& [upn, child] : bucket) {
                        (void)upn;
                        handsTurn = handsTurn &&
                                    child.position.side_to_move() != *side;
                    }
                }
                int reduction = 0;
                if (depth >= 3 && actionNumber >= 4 && quiet && handsTurn) {
                    reduction = 1;
#ifndef ULTIMATE_CONSERVATIVE_LMR
                    if (depth >= 6 && actionNumber >= 8)
                        ++reduction;
                    if (depth >= 9 && actionNumber >= 16)
                        ++reduction;
#endif
                    reduction = std::min(reduction, depth - 2);
                }
                actionWorst = evaluateAction(
                  depth - reduction, searchBeta, actionPv);
                if (reduction && actionWorst > alpha && !stopped()) {
                    actionPv.clear();
                    actionWorst = evaluateAction(depth, searchBeta, actionPv);
                }
                if (searchBeta < beta && actionWorst > alpha &&
                    actionWorst < beta && !stopped()) {
                    actionPv.clear();
                    actionWorst = evaluateAction(depth, beta, actionPv);
                }
            }
            if (actionWorst == Infinity)
                continue;
            if (actionWorst > best) {
                best = actionWorst;
                bestAction = action;
                bestPv.assign(1, action);
                bestPv.insert(bestPv.end(), actionPv.begin(), actionPv.end());
            }
            alpha = std::max(alpha, best);
            ++actionNumber;
            if (alpha >= beta || stopped()) {
                if (alpha >= beta && quiet)
                    recordQuietCutoff(action);
                break;
            }
        }
        if (best == -Infinity)
            return observer_evaluate(state, ply);
        store(best, bestAction);
        pv = std::move(bestPv);
        return best;
    };

    const int maxDepth = std::clamp(limits.depth, 1, MaxPly - 2);
    int previousScore = 0;
    for (int depth = 1; depth <= maxDepth; ++depth) {
        currentIterationDepth = depth;
        BeliefPv pv;
        // A failed narrow probe is always repeated with the full window, so
        // aspiration changes pruning only, never belief/observation semantics.
        const bool aspirate = depth >= 3;
        int alpha = aspirate ? std::max(-Infinity, previousScore - 120) : -Infinity;
        int beta = aspirate ? std::min(Infinity, previousScore + 120) : Infinity;
        int score = solve(beliefs, depth, alpha, beta, 0, pv);
        if (!stop_ && (score <= alpha || score >= beta)) {
            pv.clear();
            score = solve(beliefs, depth, -Infinity, Infinity, 0, pv);
        }
        if (stop_)
            break;
        previousScore = score;
        result.score = result.worstScore = result.meanScore = score;
        result.completedDepth = depth;
        result.historyPreservingPlies = depth;
        result.principalVariation = std::move(pv);
        previousIterationPv = result.principalVariation;
        result.bestMove = result.principalVariation.empty()
                        ? std::nullopt
                        : std::optional<std::string>(
                            result.principalVariation.front());
        result.nodes = nodes_;
        result.beliefNodes = beliefNodes;
        result.beliefTtHits = beliefTtHits;
        result.singletonHandoffs = singletonHandoffs;
        result.observationBuckets = observationBuckets;
        result.peakBeliefs = peakBeliefs;
        result.elapsed = std::chrono::duration_cast<
          std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - beliefStart);
        if (limits.onBeliefIteration)
            limits.onBeliefIteration(result);
    }
    result.nodes = nodes_;
    result.beliefNodes = beliefNodes;
    result.beliefTtHits = beliefTtHits;
    result.singletonHandoffs = singletonHandoffs;
    result.observationBuckets = observationBuckets;
    result.peakBeliefs = peakBeliefs;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - beliefStart);
    return result;
}

}  // namespace Stockfish::Ultimate
