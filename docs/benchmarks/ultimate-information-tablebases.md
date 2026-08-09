# Ultimate information tablebases

The ordinary `.uftb` files solve concrete Chess Ultimate positions. A concrete
position names the real King, every Jester, and the square and visibility of
every Ghost. Those files remain the exact transition and terminal-value oracle
for the information-tablebase layer; they are not rewritten or weakened.

## Canonical starting information

README W/L/D statistics for every material class containing a Jester or Ghost
use the `fresh-maximal-public-view-v1` convention:

- Each counted root is a fresh analysis snapshot. No move or draft history is
  assumed beyond state that is itself represented in UPN.
- Enemy King and Jester models are indistinguishable royal silhouettes. Every
  assignment of the real King that passes native necessary-reachability checks
  belongs to the initial information set. The convention deliberately does not
  assume that Ranked draft timing previously disclosed the King.
- A visible enemy Ghost has its exact public square. An invisible enemy Ghost
  ranges over every causally admitted square consistent with the public board,
  material, state fields, and necessary-reachability audit.
- A player always knows the concrete identity and state of their own pieces.
  Both players therefore retain their actual private information even in the
  classes where each side owns a Jester or Ghost.
- Results are concrete-realization weighted. Every admitted concrete world is
  counted once, preserving the existing row denominators and making the public
  and concrete summaries directly comparable. A hidden-piece owner may have a
  different result in two actual worlds that share its opponent's public view:
  the owner knows which world is real, while the opponent must keep one uniform
  observation-based strategy across its complete information set.

This convention is intentionally a statement about a fresh queried position.
The same hidden-Ghost UPN can occur after histories that imply a smaller belief
set—for example, a previously visible Ghost's quiet move restricts its possible
destination to legal neighbors of the known source. Such history-specific
positions are solved exactly when reached inside the information game, but are
not separate roots in the README population.

## Observations and strategy

Strategies use perfect recall of public observations and each player's own
private facts. Beliefs are updated only by shipping-game observations:

- visible moves and actions expose their public actor and source plus any
  target that remains public; when a previously visible Ghost makes a quiet
  move and hides again, its source stays known but its destination does not;
- a quiet invisible-Ghost move conceals the coordinates that the opponent does
  not see;
- a reveal exposes the newly visible destination;
- public captures, deaths, Bomb effects, pulls, cooldown/freeze changes, forced
  continuations, and the side to move are observed;
- continuation after a royal capture removes hypotheses in which the captured
  silhouette was the real King.

The solver uses **non-signaling observation semantics**. It may eliminate a
world because an observed action or result is impossible there, but it may not
eliminate a world merely because a chosen strategy would have preferred a
different legal move there. This matches the controller: private information
is learned from game behavior, never from an assumption about the opponent's
evaluation or policy.

For each concrete root and its two player-specific information sets, a side
wins only if it has a pure observation-based strategy that forces victory with
the private facts it actually owns. An uninformed player therefore must win in
every world it considers possible; an informed player may condition its moves
on its real private state, but not on the opponent's. A side loses only if the
opponent can force victory under the opponent's own information. Every
remaining actual-information state is a draw, including infinite observation-
preserving cycles and states for which neither player has a forced win.

## Exactness contract

The offline solver is separate from the latency-bounded live engine search.
It must:

1. enumerate every admitted initial royal/Ghost hypothesis;
2. retain every successor consistent with each public observation;
3. use no belief limit, representative sampling, depth limit, probability, or
   evaluation cutoff;
4. solve cycles by retrograde fixed point/SCC analysis rather than a search
   horizon;
5. filter worlds with the native necessary-reachability predicate before
   grouping them;
6. cross-probe exact lower-material information tables after captures;
7. emit a summary SHA-256-bound to the concrete tables, observation projection,
   move generator, and proof-kernel sources, plus zero Bellman/rank residuals;
8. conserve the admitted concrete-world count independently for both starting
   sides.

The checked-in summary catalog is small and reproducible. Temporary belief
graphs, predecessor files, and checkpoints are local generation artifacts and
are not committed. Lower-stratum `.ufiw` overlays carry both the logical source
table SHA-256 and the complete solver/move-generation model SHA-256; a larger
stratum refuses a stale overlay before constructing its graph.

The four-model concrete codec folds horizontal reflections according to the
real Ivory King's square. Swapping a hidden King/Jester assignment can therefore
change the encoded reflection even though the physical public board did not
flip. The information solver reconstructs both hypotheses in one shared
physical coordinate frame before comparing actions and observations, then maps
successors back through the folded codec. A 20,000-state involution/round-trip
self-test guards this boundary.

## First exact stratum

The exhaustive `K+Jester versus K` solve is the lower-material oracle for the
larger Jester classes. It contains 452,668 paired information sets and checked
8,239,208 transition observations without a belief cap or sampling. Its fixed
point has zero Bellman residual:

| Side to move | Public-information W / L / D | Concrete unreachable W / L / D |
| --- | ---: | ---: |
| Jester owner | 412,616 / 0 / 0 | 80,344 / 0 / 0 |
| Bare King | 120 / 417,496 / 75,344 | 0 / 0 / 0 |

The bare-King result differs substantially from perfect information because a
move must be legal under every retained royal assignment. For example, at
concrete index 492,966 (`King a1, Jester a2 versus King b1`, bare King to move),
`b1-a1` captures the real King and wins with perfect information. In the
swapped hypothesis it captures the Jester and would land next to the real King
on a2, so that move is illegal there and cannot be selected as a uniform public-
information action. This is a rules-derived information effect, not a belief
cap or heuristic reclassification.

There are also 124 paired bare-King-to-move sets with no action legal under
both royal assignments. The first is concrete index 492,967: the public royal
silhouettes are a1 and b2 and the bare King is b1. If a1 is the King, `b1-a1`
is its only move; if b2 is the King, `b1-b2` is its only move. Under the same
uniform-action rule used by the live belief search, the information set has no
legal action and is a forced timeout/soft-lock loss. This convention is tested
explicitly because vacuous AND semantics here materially affects W/L/D.
