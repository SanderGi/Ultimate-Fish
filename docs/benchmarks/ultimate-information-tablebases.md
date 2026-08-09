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
`af8d6187577e6c0fedc0b9643020f843e9b61cab03af63ef2cd2138e0ca41c7b`.
It contains all 985,920 dense concrete records; unreachable records remain
zero-valued and are accounted for separately rather than silently dropped.

The `af8d6187...` model also treats an observation bucket containing two
different residual material worlds as a terminal constant when both worlds
have ended with the same publicly announced winner. A terminal/ongoing mix or
different winners remains a hard projection error. This case arises when
capturing an indistinguishable royal silhouette leaves a Jester in one
hypothesis and a King in the other, but both hypotheses have already awarded
the same win. It must not be mistaken for a continuing canonical
`K+Jester versus K` pair.

The completed K+K+2 primary-Jester strata are regenerated with an explicit
lower-class cardinality split. A still-ambiguous two-world royal pair probes
the exact K+Jester information overlay; a history-refined singleton probes the
concrete perfect-information WDL. Their certified legal-root results are:

| Material class | First material owner starts W / L / D | Second material owner / bare King starts W / L / D | Concrete unreachable W / L / D by starting side |
| --- | ---: | ---: | ---: |
| K+Jester+Knight vs K | 14,798,084 / 0 / 0 | 242,136 / 16,055,072 / 2,681,752 | first: 4,180,876 / 0 / 0; second: 0 / 0 / 0 |
| K+Jester vs K+Knight | 2,898,404 / 0 / 12,987,312 | 334,432 / 121,472 / 18,523,056 | first: 3,093,244 / 0 / 0; second: 0 / 0 / 0 |
| K+Jester+Bishop vs K | 13,965,588 / 0 / 0 | 316,578 / 16,113,394 / 2,548,988 | first: 5,013,372 / 0 / 0; second: 0 / 0 / 0 |
| K+Jester vs K+Bishop | 2,506,936 / 0 / 13,378,780 | 488,456 / 10,706 / 18,479,798 | first: 3,093,244 / 0 / 0; second: 0 / 0 / 0 |
| K+Jester+Queen vs K | 10,877,572 / 0 / 0 | 611,994 / 17,307,456 / 1,059,510 | first: 8,101,388 / 0 / 0; second: 0 / 0 / 0 |
| K+Jester vs K+Queen | 2,489,288 / 7,339,356 / 6,057,072 | 16,116,718 / 2,682 / 2,859,560 | first: 3,093,244 / 0 / 0; second: 0 / 0 / 0 |
| K+Jester+Rook vs K | 12,797,700 / 0 / 0 | 425,372 / 17,360,968 / 1,192,620 | first: 6,181,260 / 0 / 0; second: 0 / 0 / 0 |

The owner-Knight, owner-Bishop, owner-Queen, and owner-Rook graphs respectively
exercised 650,906, 673,462, 650,826, and 656,846 canonical lower royal pairs
and no singleton lower transitions. Both opposing Knight and opposing Bishop
exercised 1,353,848 pair probes and 5,216 singleton probes. Opposing Queen
exercised 1,353,780 pair probes, 5,216 singleton probes, and 3,426 two-world
same-public-winner terminal groups. Every completed graph has zero
owner-overlay-versus-concrete differences, empty uniform-action sets, Bellman
residuals, and rank residuals. Results that happen to match a withdrawn run are
accepted only from this cardinality-correct recomputation.

The Knight, Bishop, Queen-owner, and Rook-owner payloads were first completed
under model `095d2301...`, whose exhaustive graph construction threw on every
two-world lower bucket that was not the exact continuing royal pair. Successful
completion therefore proves that the new same-winner terminal branch had zero
executions in those strata. Migration through `e117f735...` and then to the
complete `af8d6187...` source inventory changed only UFIW2
header bytes 96 through 159; bytes 0 through 95 and every payload byte from 160
onward are identical, all payload SHA-256 values are preserved, and every
source/model/header/count/conservation check was rerun. `K+Jester versus K`
itself was independently recomputed after the terminal-group correction; its
payload was then rebound to `af8d6187...` because the only further change was
adding the already-linked, unchanged `tablebase_probe.{h,cpp}` dependency to
the certificate's source inventory.

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

The committed proof kernel and arbitrary-belief probe are bound to model
`4a2d9d7b503b29204cf9af08985345771b9046c07bd2116e592fab40ee12e430`;
the concrete table SHA-256 is
`3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31`.
The bundled UFIW2 fresh-root overlay is 1,972,000 bytes with SHA-256
`8b16fe51e279b031510dda47347b391a64306c193e5f4c2734b8028cdb384144`.

The bundled UFGM1 sidecar retains all 5,477,899 reduced decision nodes plus
the force roots for 6,320 public geometries and 12,790 mover-private legal-dot
strata. It therefore probes an exact history-preserving Ghost-location mask
after a larger tablebase captures down into this class; it never replaces that
mask with the fresh maximal README root. The 54,865,711-byte sidecar has
SHA-256
`400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b`
and observation-model SHA-256
`af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf`.
Its independent loader rejects stale hashes, malformed or non-reduced nodes,
beliefs crossing a private dot stratum, visible non-singleton beliefs, and
terminal masks mixing different public winner observations. The export was
round-tripped against all 1,971,840 concrete singleton worlds and 492,952
non-singleton masks with zero residuals.

## Invalidated v1 results

The earlier `K+Jester versus K` overlay intersected legal actions before giving
the mover its private legal-dot observation. Its published W/L/D counts and
its claimed uniform-action soft locks therefore solve a strictly less informed
game and are not v2 results. They are retained only in the archived
`pre-legal-dots-v1` artifacts for provenance. No Jester/Ghost row may be
published as exact public-information W/L/D until it has a complete v2
certificate; validation never falls back to the old counts.
