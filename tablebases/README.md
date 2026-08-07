# Ultimate Fish tablebases

These bundled files are exact WDL/DTW retrograde solutions for the native
8x10 board. Position-only single-character classes use a dense 985,920-state
codec with both Kings and the named Ivory character on distinct anchor squares,
either side to move, and every model `moved=true`. The moved-state restriction
makes the class closed by excluding castling; the probe rejects state not
represented by that class.

| File | Class | In-class edges | First material owner to move W / L / D | Second owner / bare King to move W / L / D | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `krk.uftb` | King+Rook vs King | 11,970,912 | 492,960 / 0 / 0 | 0 (41,808) / 414,300 / 36,852 | `abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44` |
| `kqk.uftb` | King+Queen vs King | 15,744,492 | 492,960 / 0 / 0 | 0 (41,808) / 413,304 / 37,848 | `1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3` |
| `kninjak.uftb` | King+Ninja vs King | 12,610,592 | 492,960 / 0 / 0 | 0 (41,808) / 413,376 / 37,776 | `4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776` |
| `kdragonk.uftb` | King+Dragon vs King | 11,904,156 | 492,960 / 0 / 0 | 0 (41,808) / 414,164 / 36,988 | `28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6` |
| `kjesterk.uftb` | King+Jester vs King | 9,169,752 | 492,960 / 0 / 0 | 41,808 / 414,344 / 36,808 | `3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa` |
| `kbombk.uftb` | King+Bomb vs King | 9,761,788 | 492,960 / 0 / 0 | 0 (41,808) / 450,360 / 792 | `3d4f44035652e486cbd72c59e8247cfbba355b748107ee2b9d06abfe4674b864` |
| `kparasitek.uftb` | King+Parasite vs King | 8,602,716 | 492,960 / 0 / 0 | 0 (41,808) / 451,120 / 32 | `f08b2676a703259ef638bc4ab72b9cd7e6b2000afc72dd70b0ac17e03ea858ab` |
| `kgiantk.uftb` | King+Giant vs King | 4,747,504 | 85,776 / 0 / 407,184 | 0 (30,868) / 1,460 / 460,632 | `eb52f2c08cf88e1e3682d0c72dfde191d9009e79779ad7eee23ca82fdcade591` |
| `kberserkerk.uftb` | King+Berserker vs King, power 0–8 and 9+ | 92,321,028 | 4,929,600 / 0 / 0 | 0 (418,080) / 4,143,200 / 368,320 | `f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1` |
| `kghostk.uftb` | King+Ghost vs King, both visibility states | 17,761,144 | 985,920 / 0 / 0 | 0 (83,616) / 865,512 / 36,792 | `3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31` |
| `ksniperk.uftb` | King+Sniper vs King, cooldown 0–3 | 24,180,908 | 199,566 / 0 / 1,772,274 | 0 (167,232) / 2,276 / 1,802,332 | `473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5` |
| `kprincek.uftb` | King+Prince vs King, both action phases | 14,322,440 | 122,400 / 38,536 / 824,984 | 0 (41,808) / 44,792 / 899,320 | `3b1b965e31878b19e9c930ee8c8bdf256c1445c1010ab4e6b53b991120b28b0f` |
| `kpawnk.uftb` | King+Pawn vs King, moved/unmoved | 12,759,470 | 678,502 / 0 / 307,418 | 0 (83,616) / 459,272 / 443,032 | `42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844` |
| `kpenguink.uftb` | King+Penguin vs King, cooldown and aura | 39,165,400 | 289,648 / 8,992 / 5,616,880 | 19,576 (261,408) / 10,560 / 5,623,976 | `bec15406a45c4a955e5ca35a0190e3447abd7c68f621de254b9fc6afe242f8f3` |
| `kcopycatk.uftb` | King+linked Copycat pair vs King | 10,685,864 | 480,408 / 0 / 12,552 | 0 (40,776) / 374,136 / 78,048 | `98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb` |
| `kknightknightk.uftb` | King+2 Knights vs King | 196,460,680 | 1,964,822 / 0 / 7,524,658 | 0 (804,804) / 68 / 8,684,608 | `5c95ba0ed74d95e4d2c2fa4d1a98c1dddb121a9d11406853c75d4d5e1ba15138` |
| `kturtleturtlek.uftb` | King+2 Turtles vs King | 167,494,620 | 1,579,316 / 0 / 7,910,164 | 0 (804,804) / 416 / 8,684,260 | `bbb146a2f252eaf0eb40d23e49d0ddda56de183fb202c515ff64a0fd172037b1` |

Parentheses are illegal adjacent-King states excluded from the preceding legal
count. A live Jester intentionally permits its real King to remain threatened,
so the 41,808 bare-King-side wins in `kjesterk` are legal and are not parenthesized.

Bundled files use packed format version 4 (or v5 when a character has a second
linked/material model): a two-bit WDL plane, one-byte DTW
plane, and a sparse exact exception stream for distances of 255 or more. This
is 37.5% smaller than the former 16-bit records before entropy coding. Every file
was validated after retrograde propagation by
regenerating all legal successors and checking the Bellman equations for every
state. Giant anchor tuples whose 2x2 footprint is off-board or overlaps a King
are explicit draw sentinels and can never be produced by the probe.

The state dimensions preserve Ghost visibility, Sniper cooldown, both Prince
action phases, Pawn moved state, and the Penguin's cooldown plus exact reachable
freeze aura. Pawn promotions cross-probe the exact Queen table. A double-step
en-passant marker is quotient-equivalent here because no opposing Pawn exists
to use it.
Copycat is one deployable character with two board models; in this material
class its clone remains the exact horizontal mirror of the selected half, so
the invariant is encoded directly rather than storing unreachable arbitrary
clone coordinates.

`tools/plan_ultimate_tablebases.py` is the authoritative class and storage
inventory for the expansion. It applies horizontal-reflection canonicalization
and budgets separate two-bit WDL and byte DTW planes. Future large planes are
split at 64 MiB, below GitHub's regular 100 MB per-file limit; the repository
does not use Git LFS. The planner treats entropy compression as extra margin,
not as a speculative assumption when enforcing the 10 GiB total cap.

Regenerate them with `tools/generate_ultimate_tablebases.sh`. Checkpoints are
piece-tagged and resumable. Lone Bishop, Knight, and Turtle classes are not
bundled because the recovered native insufficient-material rule makes every
such position an immediate draw.
