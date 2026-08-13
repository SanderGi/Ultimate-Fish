/*
  Ultimate Fish - Chess Ultimate public-information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_INFORMATION_H_INCLUDED
#define ULTIMATE_INFORMATION_H_INCLUDED

#include "position.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace Stockfish::Ultimate {

// Knowledge which is not recoverable from the current board alone.  A player
// always knows the concrete identities of their own King and Jesters.  The
// enemyKingKnown bit is for public draft/history disclosure (for example, the
// King was locked before a later Jester was drafted); it must never be inferred
// merely from the concrete Position supplied to the information-set solver.
struct DisclosureContext {
    Color observer = Color::White;
    bool enemyKingKnown = false;
    // When present, only these enemy royal piece identities came from the
    // first simultaneously revealed Ranked pick group and can still be the
    // real King. Later-group Jesters are public Jesters, even while two or
    // more first-group silhouettes remain ambiguous. An unspecified mask
    // retains the legacy/fresh-snapshot meaning that every enemy royal is a
    // candidate.
    bool enemyRoyalCandidatesSpecified = false;
    std::uint64_t enemyRoyalCandidatesLow = 0;
    std::uint64_t enemyRoyalCandidatesHigh = 0;

    [[nodiscard]] constexpr bool knows_royal_identity(Color color) const {
        return color == observer || enemyKingKnown;
    }

    [[nodiscard]] constexpr bool is_enemy_royal_candidate(int piece) const {
        if (!enemyRoyalCandidatesSpecified || piece < 0 ||
            piece >= Position::MaxPieces)
            return false;
        return piece < 64
             ? bool(enemyRoyalCandidatesLow & (std::uint64_t{1} << piece))
             : bool(enemyRoyalCandidatesHigh &
                    (std::uint64_t{1} << (piece - 64)));
    }

    [[nodiscard]] constexpr bool knows_royal_identity(Color color,
                                                       int piece) const {
        return knows_royal_identity(color) ||
               (enemyRoyalCandidatesSpecified &&
                !is_enemy_royal_candidate(piece));
    }

    constexpr void set_enemy_royal_candidate(int piece) {
        enemyRoyalCandidatesSpecified = true;
        if (piece >= 0 && piece < 64)
            enemyRoyalCandidatesLow |= std::uint64_t{1} << piece;
        else if (piece >= 64 && piece < Position::MaxPieces)
            enemyRoyalCandidatesHigh |= std::uint64_t{1} << (piece - 64);
    }
};

// Allocation-light, collision-free equivalents of the textual keys below.
// Search uses these structured records in its inner loop; protocol diagnostics
// and standalone proof artifacts retain the stable textual spellings.
struct InformationPieceKey {
    std::uint64_t intrinsic = 0;
    std::int16_t linkClass = -1;
    std::int16_t hostClass = -1;

    friend bool operator==(const InformationPieceKey& lhs,
                           const InformationPieceKey& rhs) {
        return std::tie(lhs.intrinsic, lhs.linkClass, lhs.hostClass) ==
               std::tie(rhs.intrinsic, rhs.linkClass, rhs.hostClass);
    }
    friend bool operator<(const InformationPieceKey& lhs,
                          const InformationPieceKey& rhs) {
        return std::tie(lhs.intrinsic, lhs.linkClass, lhs.hostClass) <
               std::tie(rhs.intrinsic, rhs.linkClass, rhs.hostClass);
    }
};

struct InformationViewKey {
    std::uint64_t state = 0;
    std::vector<InformationPieceKey> pieces;

    friend bool operator==(const InformationViewKey& lhs,
                           const InformationViewKey& rhs) {
        return lhs.state == rhs.state && lhs.pieces == rhs.pieces;
    }
    friend bool operator<(const InformationViewKey& lhs,
                          const InformationViewKey& rhs) {
        return lhs.state != rhs.state ? lhs.state < rhs.state
                                      : lhs.pieces < rhs.pieces;
    }
};

struct InformationObservationKey {
    std::uint64_t action = 0;
    InformationViewKey view;
    std::vector<std::uint16_t> decisionMarkers;

    friend bool operator==(const InformationObservationKey& lhs,
                           const InformationObservationKey& rhs) {
        return lhs.action == rhs.action && lhs.view == rhs.view &&
               lhs.decisionMarkers == rhs.decisionMarkers;
    }
    friend bool operator<(const InformationObservationKey& lhs,
                          const InformationObservationKey& rhs) {
        return std::tie(lhs.action, lhs.view, lhs.decisionMarkers) <
               std::tie(rhs.action, rhs.view, rhs.decisionMarkers);
    }
};

[[nodiscard]] InformationViewKey compact_view_key(
  const Position& position,
  const DisclosureContext& disclosure,
  const std::vector<Move>* legalMoves = nullptr);

[[nodiscard]] std::vector<std::uint16_t> compact_decision_markers(
  const Position& position,
  const DisclosureContext& disclosure,
  const std::vector<Move>* legalMoves = nullptr);

[[nodiscard]] InformationObservationKey compact_transition_observation_key(
  const Position& before,
  const Move& move,
  const Position& after,
  const DisclosureContext& disclosure,
  bool includeDecisionObservation,
  const std::vector<Move>* afterLegalMoves = nullptr);

// Return a collision-free, canonical description of everything visible to one
// observer at a decision boundary.  Equal strings mean equal observations; the
// function intentionally returns the complete serialization rather than a
// fixed-width hash so an exact solver cannot merge states through a hash
// collision.
//
// Privacy rules:
//   * the observer's King/Jester identities are concrete;
//   * enemy King/Jester identities are "royal" silhouettes unless disclosed;
//   * an invisible enemy Ghost retains its public state but has no square;
//   * the observer's Ghosts, and every visible Ghost, retain exact squares.
//
// Piece relationship IDs and Angel attachment sequence numbers are normalized
// before serialization.  They therefore describe the rule-relevant public
// relationship graph without exposing Position's allocation order.
[[nodiscard]] std::string view_key(const Position& position,
                                   const DisclosureContext& disclosure);

// Return the mover-private observation available before choosing an action.
// The shipping client lets the player select each of their pieces, inspect all
// rendered legal-move dots, cancel, and repeat.  Consequently the complete dot
// frontier is information available to the side to move even when the
// ordinary board view still hides a King/Jester assignment or Ghost square.
//
// Each marker is the rendered source/destination pair. Multiple engine Moves
// that share one UI dot (promotion choices, auxiliary model IDs, or distinct
// internal action kinds) deliberately collapse to that one marker: those
// fields are not separately displayed before the dot is chosen. The ordinary
// view is embedded in the key, so equality means equality of both the board
// presentation and every inspectable dot. Pass is represented explicitly.
//
// This observation is private to the mover. Calling it for a non-moving
// observer is a logic error and throws std::invalid_argument rather than
// accidentally disclosing the mover's frontier to the opponent.
[[nodiscard]] std::string decision_observation_key(
  const Position& position,
  const DisclosureContext& disclosure,
  const std::string* ordinaryView = nullptr);

// Return the complete public observation of one legal transition.  The key
// includes the public action animation and the resulting view.  In particular,
// an already-invisible enemy Ghost's quiet move exposes neither endpoint; a
// visible Ghost exposes its source; and a capture/reveal exposes its
// destination.  This is the equivalence key an exact epistemic retrograde
// solver should use when partitioning successor worlds after an action.
//
// No belief bound, sampling, or lossy digest is used here.  The caller remains
// responsible for enumerating every concrete world consistent with a view.
[[nodiscard]] std::string transition_observation_key(
  const Position& before,
  const Move& move,
  const Position& after,
  const DisclosureContext& disclosure,
  std::string* resultingView = nullptr);

}  // namespace Stockfish::Ultimate

#endif
