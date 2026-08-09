# Ultimate information tablebases

The ordinary `.uftb` files solve concrete Chess Ultimate positions. A concrete
position names the real King, every Jester, and the square and visibility of
every Ghost. Those files remain the exact transition and terminal-value oracle
for the information-tablebase layer; they are not rewritten or weakened.

## Canonical starting information

README W/L/D statistics for every material class containing a Jester or Ghost
use the `fresh-maximal-public-view-v2` convention:

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

Strategies use perfect recall of public observations, each player's own
private facts, and UI observations visible only to that player. Beliefs are
updated only by shipping-game observations:

- before choosing an action, the mover may select every selectable owned piece
  and privately inspect the complete set of legal destination/action markers
  (the UI's legal dots); this inspection does not consume the turn;
- the non-mover does not see those markers and must not refine its information
  set from the mover's private inspection;

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
evaluation or policy. A legal-dot difference is rule-generated private
evidence, not policy inference, and therefore does refine the mover's
information before its action is chosen.

The smallest confirmed King/Jester witness has the observer to move with its
King on e7 and indistinguishable enemy royal silhouettes on e6 and e5. If e6
is the real enemy King and e5 the Jester, selecting e7 shows an e6 destination
dot: `e7-e6` legally captures the King. If e6 is the Jester and e5 the real
King, the same move would capture the Jester and leave the moving King adjacent
to the real King on e5, so the e6 dot is absent. The ordinary hidden public
board is identical in both worlds, but the mover's pre-decision private marker
observation distinguishes them. The shipping-game experiment included six
Penguins per side plus Player 2's Turtle on b1 and Pawns on b2/b3; exhaustive
regression fixtures retain sufficient surrounding material so terminal or
insufficient-material handling cannot erase the local distinction.

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
2. before every mover decision, partition that mover's information set by the
   exhaustive per-owned-piece legal-marker signature, without exposing the
   signature to the non-mover;
3. retain every successor consistent with each public and owned-private
   observation;
4. use no belief limit, representative sampling, depth limit, probability, or
   evaluation cutoff;
5. solve cycles by retrograde fixed point/SCC analysis rather than a search
   horizon;
6. filter worlds with the native necessary-reachability predicate and any
   stronger causal hidden-state audit before grouping them;
7. cross-probe exact lower-material information tables after captures;
8. emit a summary SHA-256-bound to the semantic contract, concrete tables,
   observation projection,
   move generator, and proof-kernel sources, plus zero Bellman/rank residuals;
9. conserve the admitted concrete-world count independently for both starting
   sides.

README parentheses come from that exact admission certificate, bucketed by the
rejected concrete state's perfect-information W/L/D. The older native audit is
required to be an outcome-by-outcome subset. This matters for histories that a
dense codec can represent locally but that cannot occur in play—for example,
an enemy Ghost cannot remain hidden while adjacent to the observing King.

The checked-in summary catalog is small and reproducible. Temporary belief
graphs, predecessor files, and checkpoints are local generation artifacts and
are not committed. Lower-stratum `.ufiw` overlays carry both the logical source
table SHA-256 and the complete solver/move-generation model SHA-256; a larger
stratum refuses a stale overlay before constructing its graph.

Schema version 2 and solver version 2 bind `fresh-maximal-public-view-v2` into
both the catalog's observation SHA and every UFIW2 model SHA. Consequently, a
v1 catalog, partial checkpoint, or overlay is rejected even when its concrete
`.uftb` SHA is unchanged. Preserved v1 artifacts use the
`*pre-legal-dots-v1*` namespace; new generation defaults use disjoint
`*legal-dots-v2*` paths and never resume them.

The four-model concrete codec folds horizontal reflections according to the
real Ivory King's square. Swapping a hidden King/Jester assignment can therefore
change the encoded reflection even though the physical public board did not
flip. The information solver reconstructs both hypotheses in one shared
physical coordinate frame before comparing actions and observations, then maps
successors back through the folded codec. A 20,000-state involution/round-trip
self-test guards this boundary.

For the one-primary-Jester strata, a collision-free fixed-width projection
specializes the general relationship-aware text serializer to these closed
four-model classes (which contain no links, attachments, Ghosts, or forced
continuations). On `K+Jester versus K`, it produced all 985,920 overlay flags
byte-for-byte identical to the independent general projection while reducing
observation-graph construction from about 84 seconds to 17 seconds on the same
machine. The general projection remains the semantic oracle and regression
baseline.

## First corrected v2 stratum

The exhaustive `K+Jester versus K` solve is the first lower-material oracle
regenerated with the mover-private legal-dot channel. Its fresh roots contain
450,272 still-ambiguous royal pairs: 206,308 with the Jester owner to move and
243,964 with the bare King to move. A further 2,396 bare-King-to-move royal
pairs have different private dot frontiers and split into singletons before an
action is selected. The resulting action graph has no empty uniform-action
sets. Both force fixed points have zero Bellman and rank residuals.

| Side to move | Public-information W / L / D | Concrete unreachable W / L / D |
| --- | ---: | ---: |
| Jester owner | 412,616 / 0 / 0 | 80,344 / 0 / 0 |
| Bare King | 3,272 / 414,344 / 75,344 | 0 / 0 / 0 |

Concrete index 492,966 is a compact regression witness. In one world the
bare King on b1 can capture the real King on a1; in the swapped world a1 is the
Jester and the real King is on a2, so `b1-a1` is absent but `b1-a2` is offered.
Version 1 incorrectly intersected these actions and created a soft lock.
Version 2 first observes the distinct dot frontiers, chooses the winning dot in
the resulting singleton, and therefore classifies both concrete realizations as
bare-King wins.

The overlay is SHA-256-bound to concrete table
`3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa`
and legal-dot-v2 solver model
`095d2301b1cda56e6aa75db7663d7a4b811d36cf837c1482a6c491940b63d831`.
It contains all 985,920 dense concrete records; unreachable records remain
zero-valued and are accounted for separately rather than silently dropped.

K+K+2 primary-Jester results are temporarily withheld while their lower-class
transition is being regenerated. The first v2 implementation probed the dense
K+Jester overlay per concrete child after the extra piece was captured. That is
valid for a still-ambiguous two-world royal pair, but not for a history-refined
singleton, which must use the concrete perfect-information WDL. The solver now
distinguishes those cardinalities explicitly; all affected larger results are
treated as stale even when their earlier Bellman residual was zero.

## First exact Ghost stratum

`K+Ghost versus K` is solved over arbitrary hidden-location subsets rather
than enumerating a capped list of worlds. For each fixed public geometry, the
solver represents all 80 Ghost-membership bits in a canonical reduced ordered
binary decision diagram and applies the exact non-signaling observation image
symbolically. The completed least fixed point took 49 iterations and 5,477,899
canonical BDD nodes, with no sampling, depth bound, or belief cap.

| Side to move | Public-information W / L / D | Concrete unreachable W / L / D |
| --- | ---: | ---: |
| Ghost owner | 863,768 / 0 / 0 | 122,152 / 0 / 0 |
| Bare King | 0 / 826,992 / 36,776 | 83,616 / 38,520 / 16 |

The proof exhaustively matches all 1,971,840 concrete singleton states, checks
the monotonicity required by the symbolic antichains, and re-evaluates the
complete symbolic Bellman equations; all three residuals are zero. Its root
certificate conserves 1,727,536 admitted concrete roots. Four exact rectangle
symmetries reduce the 913,872 physical initial information sets to 228,468
canonical set orbits (114,234 per starting side) without changing their
realization weights.

The committed proof kernel is bound to model
`9c3700a9fab27c9971300db6f7d7f3af8edb16ed9af0f33753f4b5bf957de6ba`;
the concrete table SHA-256 is
`3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31`.
The generated UFIW2 overlay is 1,972,000 bytes with SHA-256
`53c313acae8207f4569b03082367589abd820ce4be4a6c1d11631c5b1cad6f66`.

## Invalidated v1 results

The earlier `K+Jester versus K` overlay intersected legal actions before giving
the mover its private legal-dot observation. Its published W/L/D counts and
its claimed uniform-action soft locks therefore solve a strictly less informed
game and are not v2 results. They are retained only in the archived
`pre-legal-dots-v1` artifacts for provenance. No Jester/Ghost row may be
published as exact public-information W/L/D until it has a complete v2
certificate; validation never falls back to the old counts.
