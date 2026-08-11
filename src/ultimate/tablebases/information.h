/*
  Ultimate Fish - Chess Ultimate public-information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_INFORMATION_H_INCLUDED
#define ULTIMATE_INFORMATION_H_INCLUDED

#include "position.h"

#include <string>

namespace Stockfish::Ultimate {

// Knowledge which is not recoverable from the current board alone.  A player
// always knows the concrete identities of their own King and Jesters.  The
// enemyKingKnown bit is for public draft/history disclosure (for example, the
// King was locked before a later Jester was drafted); it must never be inferred
// merely from the concrete Position supplied to the information-set solver.
struct DisclosureContext {
    Color observer = Color::White;
    bool enemyKingKnown = false;

    [[nodiscard]] constexpr bool knows_royal_identity(Color color) const {
        return color == observer || enemyKingKnown;
    }
};

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
  const DisclosureContext& disclosure);

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
  const DisclosureContext& disclosure);

}  // namespace Stockfish::Ultimate

#endif
