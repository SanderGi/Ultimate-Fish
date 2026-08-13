/* Ultimate Fish exact ordinary-piece/Ghost information instantiation. GPLv3+. */

// Include the engine model before defining the narrow token substitution so
// PieceType's declaration itself is never rewritten.  The audited Dragon
// kernel is material-generic apart from this compile-time piece token and its
// authenticated K+piece-v-K lower lookup.
#include "position.h"

#ifndef ULTIMATE_GHOST_ORDINARY_PIECE
#error "ULTIMATE_GHOST_ORDINARY_PIECE must name one PieceType enumerator"
#endif

#define Dragon ULTIMATE_GHOST_ORDINARY_PIECE
#define GhostDragonExact GhostOrdinaryExact
#include "ghost_dragon_information_solver.cpp"
#undef GhostDragonExact
#undef Dragon

