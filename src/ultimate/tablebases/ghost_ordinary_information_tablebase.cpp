/* Ultimate Fish exact ordinary-piece/Ghost information CLI. GPLv3+. */

#include "position.h"

#ifndef ULTIMATE_GHOST_ORDINARY_PIECE
#error "ULTIMATE_GHOST_ORDINARY_PIECE must name one PieceType enumerator"
#endif

#define Dragon ULTIMATE_GHOST_ORDINARY_PIECE
#define GhostDragonExact GhostOrdinaryExact
#include "ghost_dragon_information_tablebase.cpp"
#undef GhostDragonExact
#undef Dragon

