/*
  Ultimate Fish - exact opposed Ghost/Ghost graph solver specialization
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.

  The graph arena, closure, and Bellman construction are material-agnostic.
  Reuse the crossed perfect-recall implementation with the opposed Ghost/Ghost
  model types so both hidden-information domains share one tested algorithm.
*/

#include "opposed_ghost_pair_information_solver.h"

#define ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED
#define CrossedJesterGhostInformation OpposedGhostPairInformation
#define CrossedJesterGhostSolver OpposedGhostPairSolver
#include "crossed_jester_ghost_information_solver.cpp"
#undef CrossedJesterGhostSolver
#undef CrossedJesterGhostInformation
#undef ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED
