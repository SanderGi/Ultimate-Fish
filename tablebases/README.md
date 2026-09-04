# Ultimate Fish tablebases

These tablebase classes are exact WDL/DTW retrograde solutions for the native
8x10 board. Position-only single-character classes use a dense 985,920-state
codec with both Kings and the named Ivory character on distinct anchor squares,
either side to move, and every model `moved=true`. The moved-state restriction
makes the class closed by excluding castling; the probe rejects state not
represented by that class.

The packed files store concrete worlds, including exact King/Jester identities
and Ghost squares. Public-information W/L/D for every row containing either
piece requires a separate uncapped observation-game solution under the
documented [`fresh-maximal-public-view-v2`](../docs/benchmarks/ultimate-information-tablebases.md)
convention, including the mover-private pre-decision legal-dot channel.
Concrete tables remain its exact transition/value oracle rather than being
treated as public-information results directly. Version-1 information results
are invalid and are never accepted as a fallback. An affected material is
omitted from the generated W/L/D table until its current observation-game
artifact is fully certified; retaining its concrete `.uftb` dependency does
not make its concrete outcomes publishable.

All tablebase implementation code is contained in
`src/ultimate/tablebases/`, its tests in `tests/tablebases/`, and generation,
certification, scheduling, and ledger tools in `tools/tablebases/`. These are
the canonical paths; the former flat locations were removed and are not kept
as compatibility shims. Immutable S3 certificates and already installed AWS
units retain their original path strings as historical provenance, while every
new package is built from this layout.

AWS work is scheduled by material class from immutable source and input
bundles. Each class compiles and verifies its transition shards once, merges
them, runs the exact fixed point directly, and writes a result-only artifact
manifest for content-addressed S3 archival and a version-pinned restore check.
The ordinary Ghost runner deliberately has no throwaway measurement pass.
Failed expensive phases are retained and resumed through authenticated
manifests; they are never silently deleted or blindly restarted.

> **Fleet decommissioned (2026-08-29 23:34 PDT):** all five authorized EC2
> instances are terminated and no tablebase class is currently computing or
> preserving. Unfinished checkpoint narratives below are retained as historical
> provenance only; their ledger rows are **PLANNED** and expose no uncertified
> result cells. The final downloadable catalog is authenticated at Hugging Face
> dataset revision `c574ae2347e2b324f0e90888478fb498776cf372`: 462 certified
> ordinary `.uftb` files, two exact auxiliary lower-domain `.uftb` files, and
> the complete twelve-partition stateful Devil class, with 586 managed
> payload/sidecar/transport files totaling 498,327,627,605 bytes. The former
> versioned S3 preservation bucket was deleted after migration; Hugging Face is
> the downloadable tablebase authority.
>
> The two exact lower-domain dependencies are part of the Hugging Face catalog:
> `kcopycatlinkedk.uftb` covers arbitrary intact linked
> Copycat/clone placements, while `kghostk-tracked.uftb` covers a lone Ghost
> whose location remains permanently tracked after a Parasite interaction.
> They are auxiliary codecs rather than ordinary material-ledger rows and are
> therefore counted separately from the 462 ordinary `.uftb` files above.

> **Corrected Devil authority (2026-08-29):** `kdevilk.uftb` is an authenticated
> **causal entry-root projection**. Its starting positions may already contain
> Minions when those Minions could have been placed by earlier spawns from the
> indexed Devil; only arbitrary starting Minions without such a causal history
> are excluded. The projection is not a complete King+Devil+causally spawned
> Minions vs King tablebase and must not be presented as proving that the
> stateful endgame is a forced draw. `single:devil` is **CERTIFIED** from
> canonical logical keys and final eight-byte WDL/DTW nodes for all twelve
> fixed-square partitions, each preserved by exact S3 VersionId and exposed
> through an independently restored searchable stateful sidecar. The complete
> class contains 34,981,631,519 states across the twelve legal fixed-square
> partitions. Every partition passed exhaustive Bellman verification; every
> primary plane, searchable sidecar, and census was uploaded and restored by
> exact S3 VersionId. The class certificate has
> sha256:`95649cf36a9f6287379e9d29ee80b67f7af9c8ca6dff0298e73977f458bd3e0f`
> and VersionId `vzZ.mORQVSqa.0cfr665BEmMugKyGfav`, with zero coverage and
> conservation residuals. The first preservation attempt incorrectly treated
> v6's 16-byte proof keys as v29's seven-byte keys; those receipts remain
> rejected, while the original planes were retained and subsequently preserved
> with generation-aware exact-prefix handling.
> `same:bishop+devil` remains distinct and is lower priority.
> The machine-readable authority for recovery is
> [`ultimate_devil_stateful_recovery.json`](../tools/tablebases/ultimate_devil_stateful_recovery.json);
> it requires exactly twelve fixed-square partitions and cannot count the
> causal entry-root projection or any Bishop+Devil artifact as stateful
> lone-Devil evidence.
> As of 2026-08-29 19:48 PDT, all twelve primary planes (A1, B1, C1, D1, A2,
> B2, C2, D2, A3, B3, C3, and D3) survive or have been recomputed and are
> exact-VersionId restore-authenticated. Searchable, independently restored
> stateful sidecars and censuses are complete for all twelve
> partitions. C2's 51,769,877,022-byte sidecar passed exact-VersionId restore,
> and its census found 191,798,229 wins, 875,539 losses, and 4,984,313,931
> draws with zero conservation/sort residuals and maximum DTW 17.
> C3's receipt authenticated its legacy checkpoint as version 2 with 16-byte
> keys. B1 completed and exhaustively verified its 1,270,009,248-state,
> 8,125,179,009-edge graph and passed exact primary preservation. D1 has
> completed and exhaustively verified its 3,873,307,130-state,
> 24,345,340,979-edge graph; its primary planes, searchable sidecar, and census
> subsequently passed exact-VersionId restore authentication.
> C2 completed and exhaustively verified its 5,176,987,699-state,
> 33,045,156,534-edge graph before exact primary preservation. D3 was the final
> solve. Its retained 5,625,801,166-state reverse spool adopted the
> already-complete bucket 1 and resumed under the exact v31 binary. All
> remaining reverse buckets completed under the authenticated v31 continuation,
> including the final 8,969,638,320-record bucket. Exact retrograde and exhaustive
> Bellman verification completed over 35,818,054,648 edges; its primary planes,
> 56,258,011,692-byte sidecar, and census all passed exact-version restore.
> B3 completed its
> 1,710,246,028-state solve, passed exact primary preservation, and its
> 17,102,460,312-byte sorted sidecar passed exact-version restore authentication.
> An exhaustive census of that complete B3 sidecar found 59,213,715 wins,
> 404,942 losses, and 1,650,627,371 draws (conservation and sorted-key
> residuals both zero, maximum DTW 17). In particular, its Minion-bearing
> states include decisive outcomes, directly disproving the old entry-root-only
> presentation that made the full stateful class look like a forced draw.
> D2 independently confirms the same result: its 5,194,192,187-state census
> contains 186,874,833 wins, 133,292 losses, and 5,007,184,062 draws with
> zero conservation/sort residuals and maximum DTW 13. Both census receipts
> are immutable, VersionId-bound, and restore-authenticated.
> Twelve completed censuses now cover 34,981,631,519 searchable states:
> 1,271,264,798 wins, 4,444,400 losses, and 33,705,922,321 draws, with maximum
> DTW 19. Every census is bound to an exact
> searchable-sidecar VersionId and passed sorted-key and conservation checks.
> The released B1/B3 CPU lanes expanded
> D3 to 32 workers, while D1 remains at 32, without restarting either job.
> On i098, direct cgroup evidence found 1,725,552 D2 and 72,988 B3
> `memory.high` throttles despite 242.7 GB available. Their live exporter gates
> were safely raised to 64/80 GiB and 32/48 GiB respectively without restart;
> the checked-in launcher now accepts explicit reproducible memory gates.
> C3 later showed 1,790,365 `memory.high` events while i03 had 241.1 GB
> available; its live gates were therefore raised safely from 16/24 GiB to
> 64/96 GiB without restarting or discarding any external-sort work.
> D3 likewise accumulated `memory.high` events after completing its
> 5,625,801,166-state reverse spool on the resized 512 GiB host. Its live gate
> was raised without restart from 128/160 GiB to 256/320 GiB; the counter then
> remained constant while resident memory rose and the retained merge
> continued. When retrograde propagation later reached the 320 GiB soft gate,
> its live gates were raised without restart to 384/448 GiB, retaining 64 GiB
> for the host. The checked-in v31 continuation has a repeatable
> `--raise-memory` path for this correction, while the recompute launcher keeps
> its fail-closed `--tune` path. The v31 continuation
> processes disjoint reverse buckets concurrently, binds all 64 CPUs, and uses
> the persistent 3 TiB gp3 volume at its measured ceiling. The live retrograde
> has zero max/OOM events.
>
> **2026-08-29 15:27 UTC sprint handoffs:** the obsolete certified-Devil merge
> timer is disabled and its already-imported table and merge receipt remain
> nonempty. Opposed Bomb/Ghost completed iteration-13 compaction and handed off
> without losing its checkpoint from the 16-worker v9 unit to the 30-worker v16
> unit on disjoint CPUs `0-15,32-45`. The v16 runner is exact-version staged as
> sha256:`f822aceee41acd2e78c01cc4b21d499aa482ebcd8e21caf037e2020e705d4a9c`
> (VersionId `Op4AY4LfdAiV6O8_9S56NNsw4YYU4wDE`) and its service as
> sha256:`a2afeb51f522779ea448ac245b2168cba3a4a887624bcdb7d8d7712bdb845e87`
> (VersionId `bnO8bbjTINDldBlcDqvEmUlcKqip28HF`); exact-VersionId restores
> reproduced both hashes. Same-side Sniper/Ghost reached a zero-change
> iteration 49 and entered exhaustive Bellman verification. Its CPUs are the
> result-gated successor set for opposed Berserker/Ghost, so the Berserker path
> trigger is temporarily stopped until Sniper's verifier releases them. The
> replacement Berserker service is already deployed behind that gate on the
> Bomb-disjoint set `16-31,46-48,51-61`, sha256:
> `2cf7c7f249dbb741b4b3942bbe388c74da9c9e24a7345d0227e50ab85dcec122`,
> VersionId `KIXSoQqz2DcwRdK.qqkw3KKhxbJ3Be.T`; the prior unit file is retained.
>
> **2026-08-28 certification sprint:** through 2026-08-29 20:42 PDT the fleet
> is optimizing for complete certifications rather than speculative graph
> census work. Opposed Checker/Ghost v39 failed closed because its physical
> source order did not match the information state. No v39 result was
> published. The corrected v40 build made that order a compile-time contract
> and retained Checker primary/Ghost secondary. Its v22 binary is
> sha256:`b5da74fa4f53cc4d2a6b692061063bf500cf6e7aaef8041e85e91855e3f4f478`,
> S3 VersionId `C9Q7lGKAjf3xAmw.SmBGohpcTUIlaXFy`; the authenticated concrete
> oracle is sha256:`5a9cbeced03db4db2b4f6e66d87b0198da32dfd0c635cade3c84a06d320aed4d`,
> VersionId `Dm4BZe6YQue3Nt46dUWf.LSgvGTuo7zI`. Exhaustive normalization
> covered 303,663,360 states with remap/count residual zero and produced
> sha256:`8c9220d46609b8e1a4fc8610492420a49d9efa69bdee504fb1496ebf12c38e8e`.
> V40 completed its fresh 32-worker fixed point without reusing the
> semantically wrong v39 roots; its 27,291,934,464-byte transition payload,
> Bellman equality, symbolic proof, exhaustive singleton comparison, and every
> lower/source/model/observation binding passed with residual zero. Wrapper
> sha256:`562d911b9220f20a7503d464c99add84f161c468d31bcd6050a1229819157023`
> is VersionId `EaxxvublZhqrWxfcqNLx8RwdgpOC2e0R`. An exhaustive
> 303,663,360-state color-swap canonicalizer then proved a bijection and exact
> concrete-WDL equality against the Ghost-primary oracle, including the rank
> reflection required by unpromoted Checker movement. The canonical archive,
> arbitrary-belief sidecar, result certificate, and eight-substate trivial
> audit are now version-pinned and imported with all conservation residuals
> zero. Corrected opposed
> Penguin/Ghost v34 has now completed that entire chain: Bellman,
> monotonicity, singleton, dominance, source-remap, grouping, conservation,
> archive-restore, and result-import residuals are all zero. Its version-pinned
> information-v2 audit also imported explicit admitted, excluded, and trivial
> counts with full 16-substate conservation. The canonical ledger is therefore
> **460 certified, 0 preserving, 16 computing, and 0 failed**. Same-side
> Prince/Ghost is the newest certification. Its retained 16-iteration exact
> fixed point, UFIW2 overlay, and arbitrary-belief UFGD sidecar were restored
> from the deterministic 112,000,220-byte archive with every proof and binding
> residual zero. The independent four-substate information-trivial-v2 audit
> then restricted that algebraic domain to ordinary Prince turn boundaries:
> 37,957,920 concrete realizations per side, zero unknowns, and exact admitted,
> excluded, and trivial conservation. Result certificate sha256:
> `aa3b0c3665003be1fbee081629cb50777f50e7cafcf25bb7f9cdd313b393380b`
> is S3 VersionId `QjA.mhcnhpi1Grtxon3w0zGs9Nfmt9rS`; its authenticated
> audit sidecar sha256:
> `d39a1ea4e99e553584d3e200d51ada8fb4524a6aeb1329c5e2542a842e350bb4`
> is VersionId `LF9BerMPcoPXapJHaFfXclheN4f.7kwW`. The plot remains
> 6,126 x 2,904 pixels, matching the most recently committed resolution.
>
> The 2026-08-29 02:46 UTC deadline audit measured 158.764/224 busy vCPUs
> (70.9%). I024 and i08 were already at 62.924/64 and 62.810/64,
> respectively. An earlier pass had found sixteen genuinely unallocated CPUs
> on i024 beside the fully busy 16-worker opposed Berserker/Ghost transition
> rebind. Rebind v16 now uses the
> exact non-overlapping CPU set `32-48,51-63` and 30 workers; CPUs 49 and 50
> remain exclusively assigned to the retained opposed Ghost-pair and
> Parasite/Ghost jobs. The immutable wrapper is sha256:
> `63b12fe0e0abd1fe8be4820f806b1878beb29547bbd49910d60bc4d1470fb40d`,
> S3 VersionId `_DlvPmm9d1y8ixPpdFeMsY7B.1VhFhCC`. Its first process sample
> accumulated about 29.8 CPU-seconds per wall-second, nearly doubling the
> rebind phase without weakening any proof or storage gate. A superseded
> 32-worker launch was stopped immediately after the supervisor found its
> accidental overlap on CPUs 49-50; no result was published from that interval.
> A version-pinned systemd path now closes the remaining rebind-to-solve gap:
> the appearance of the rebind's six-component SHA manifest triggers a fresh
> 30-worker certifying solve only after the completion residual is zero and
> `sha256sum --check --strict` authenticates every rewritten component. The
> solve wrapper is sha256:
> `67d40f5996afd01243e896d439823f1c5764e65b98626ec236b1a7e36bc46ec1`
> (VersionId `3LRzJ52QNA2G05MWFvhhYGiU3YwHqvur`); its service and path are
> sha256:`f4c977bc18eaf6c144351c3a39b105e8e2f53e0b7febf04a135fa8a3aa020909`
> and sha256:`7e3c5d558fdcded1f4c6e61f7103f49f7ab0bc10c3a4c711dae0188972f47094`.
> I024's preservation manifest is also version-pinned and now names the v17
> terminal unit and proof log, so a successful solve proceeds automatically to
> archive/restore authentication instead of waiting for the next hourly audit.
> The first complete non-overlapping interval measured i024 at 62.821/64,
> i08 at 62.746/64, and i0b at 31.716/32; the fleet reached 163.511/224
> (73.0%). I03's serial Queen/Ghost tail and i098's storage-bound Devil merge
> account for nearly all remaining idle capacity. The i08 preservation manifest
> was separately corrected from obsolete Prince v14 to the active v19 unit and
> version-pinned as sha256:
> `1740debb4591c0938834669579a0c1daf193f28c4a3184abefc487c3b576f69c`,
> VersionId `ADHPlHpEBuqML1QEtHhaWyFxvBP_l.OW`; opposed Sniper remains bound to
> the same authenticated manifest. Dragon/Ghost iteration 64 ended with zero
> owner changes, 132 observer changes, 22 visible-root changes, and zero
> compaction residual. During iteration 67 its expensive tail used only 10.86
> of 16 assigned CPUs, while Ninja/Ghost saturated all 16 of its CPUs in the
> last 2,960 geometries of iteration 16. Without restarting either solver, the
> sprint allocation was therefore changed to eight CPUs for Dragon and 24 for
> Ninja. A subsequent exact ten-second cgroup delta measured 8.1 and 24.2 busy
> CPUs respectively, filling i0b while prioritizing the nearer deadline result.
> The 03:14 UTC checked-in supervisor pass measured 161.250/224 busy vCPUs
> (72.0%): i024 63.012/64, i08 62.799/64, i0b 29.221/32, i03 6.000/32,
> and i098 0.218/32. The apparent i098 CPU number excludes the storage-bound
> Devil child process from the unit delta, but fresh solver evidence shows its
> 19,568,521,291-state reverse spool complete and reverse merge at bucket
> 12/16. No ready certifying class fits the remaining memory reservations, so
> the deadline policy keeps the six nearest Ghost fixed points resident rather
> than evicting their authenticated roots for speculative work.
>
> The certification watcher was hardened before those terminal writes. Its
> manifest now separates each canonical ledger stem from the physical retained
> result stem, so Bomb (`kbombkghost-1.5b-v8`), same Sniper
> (`kghostsniperk-compositional-v3`), and Dragon
> (`kghostkdragon-memory-v5`) are preserved and imported under their canonical
> filenames. Watcher sha256:
> `d1fe13e5232f46fe10edbce6f55cbc34dd4c32b94632fbcaaea463f9165c8e0e`
> is VersionId `9NFz75b0G9OCpG.VSRTPSX_syM0Q_KPf`; preserver sha256:
> `2a1c80958515b5d0318aae631d823577c39585a59e97ae2e5e1a2e02eefbe3ee`
> is VersionId `Z8Bk1u5BKSyo41.eB1pNmaag7KHfDlx1`. I024 and i0b now run
> the exact files from persistent two-minute systemd timers, and an immediate
> invocation completed successfully without touching any active solver.
> I0b also has a one-minute result-gated CPU handoff: only after a producer is
> inactive and both of its terminal `.ufiw` and `.ufgd` files are nonempty does
> it expand the surviving 32-worker Dragon or Ninja unit to CPUs `0-31`. It
> neither starts a unit nor treats those files as certification evidence; the
> independent watcher still performs all proof and preservation checks. The
> deployed script is sha256:
> `e589bba0d929cd69e8920c01eca6ec4243074893051041ffa2c9d9e6c8b95376`,
> S3 VersionId `izIpwk9RlFUv6a51N3LxAIfe5ldhnuGh`; its service and timer are
> also exact-hash/version bound in supervision. The 03:40 UTC audit
> authenticated those remote files and S3 versions, measured i0b at
> 31.677/32 busy vCPUs, and measured 164.021/224 (73.2%) fleet-wide.
>
> The 2026-08-29 08:00 UTC deadline rebalance found nine CPUs genuinely idle
> while Ninja/Ghost's final 2,960 geometries had only six live worker threads.
> Without restarting either solver, Dragon/Ghost was expanded from CPUs
> `8-12,21-23` to the disjoint set `0-13,21-23`; Ninja retained
> `14-20,24-31`.  A direct 20.013-second cgroup delta measured 16.88 busy CPUs
> for Dragon and 4.99 for Ninja, increasing i0b from 13.45 to 21.87 busy CPUs
> while preserving both in-progress fixed points.
>
> **Historical progress note (superseded by the 13:21 UTC certification
> record below):** the 2026-08-29 09:45 UTC checked-in supervisor pass reported no source,
> certificate, overlap, or failed-unit errors. It measured 149.062 busy vCPUs
> out of 224 (66.5%), with the i03 interval explicitly incomplete because the
> lone-Devil C1 reverse merge is performed by child processes outside the
> sampled main-unit delta; the conservative upper bound was 78.7%. The
> authentic `single:devil` C1 job was then the priority-1000 v28 run on i03:
> its 3,861,213,174-state closure, 24,608,033,758-edge graph, and complete
> 512-shard reverse spool are unchanged and authenticated, and 13 of 16 reverse
> buckets are complete while the remaining three workers saturate the target
> NVMe. The volume has 419,139,178,496 bytes free. The distinct
> Bishop+Devil A1 job remains stopped with its checkpoint intact and consumes
> no CPU or RAM. Opposed Sniper/Ghost is advancing through iteration 46 with
> 32 workers (2,295,000/3,943,680 geometries at the fresh probe).
> Berserker/Ghost's authenticated lower-table rewrite reached all
> 4,929,600/4,929,600 geometries with 30 workers and emitted a zero-residual
> 131,488,840-edge lower-table probe certificate. The strict handoff then
> authenticated the restored marker and launched the distinct v22 rebind;
> the 2026-08-29 11:10 UTC supervisor saw it RUNNING with all six source/input
> bindings exact and 30 assigned CPUs. Its downstream v17 certifying solve is
> blocked on the not-yet-emitted rebind manifest, rather than being mislabeled
> as a failed restore. Same Ghost/Ghost now runs the
> immutable v6 parallel continuation from its restored transition archive on
> i098. A 30-worker production benchmark processed its first 50,000 Bellman
> geometries in 5.40372 seconds with all assigned CPUs near 100%; the prior
> one-core scratch and log were retained verbatim before handoff. The first
> implementation correctly failed closed when concurrent workers exposed a
> shared transition-stream cursor; v4 replaces it with immutable read-only
> memory-mapped block ranges. A second failed-closed launch exposed and fixed
> solve-existing's inability to atomically install an authenticated replacement
> binary. The active v6 unit is hash-bound to the corrected runner, binary,
> wrapper, transition marker, concrete table, lower Ghost sidecar, and
> certification manifest. Its result watcher is bound to the exact
> UFIW2/UFGG1 layout and automatically verifies, preserves, restore-checks,
> and stages a successful result for import. Opposed Bomb/Ghost is also
> correctly running the authenticated 3.0-billion-node continuation despite
> the historical `solve-1.5b-v8` pathname: both sparse node arenas are
> 27,000,000,000 bytes, the live wrapper hash matches supervision, and no
> 1.5-billion capacity failure is pending.
> A direct Ninja/Ghost thread and I/O profile also corrected the earlier
> generic "serial compaction" diagnosis. The v19 binary already contains the
> level-ordered parallel compactor and passes all 32 workers into it. The live
> iteration-16 process is instead waiting on its final two geometry tasks:
> exactly two worker threads were runnable at about 63% of one CPU each, the
> main thread was in a futex wait, RSS was 10,860,476 KiB, and a simultaneous
> sample showed essentially zero storage traffic. Restarting from iteration 15
> would repeat this same exact tail. Future continuations need sub-geometry
> work splitting and intra-iteration checkpoints; the retained partial run is
> left intact during the deadline because it can still finish, while the four
> nearer certification paths above receive operational priority.
>
> **Historical progress note (superseded by the 13:21 UTC certification
> record below):** the 2026-08-29 12:18 UTC bookkeeping and performance audit
> reauthenticated the then-live Devil split. `devil-spawned-square-2-v29` was
> the only C1 job
> for `kdevilk.uftb`, had scheduler priority 1000, and was active on i03 at
> the exact 228-GiB memory gate. The complete 512/512-shard reverse spool is
> retained. V28 completed buckets 3, 5--16 except 4, but those thirteen buckets
> contain only 9,514,861,633 of 24,608,033,758 edges; the unfinished buckets 1,
> 2, and 4 contain 15,093,172,125 edges (61.3%), so the bucket count alone was
> not a truthful work estimate. The old one-million-record scatter merge had
> already issued 25.9 TiB of physical writes while repeatedly dirtying the same
> mmap pages. V29 was built from exact source sha256:
> `368e7230e5e2c82604b8be1d019311eaa4c344e96c04bc08db249fe9fe24915a`,
> self-tested, version-preserved, and exact-restored as binary sha256:
> `acc758ed81e56fba827c1e6ecfccfb464bfcdf8823ce35540cbb7cd3144c7df6`.
> The v28 service was stopped only after a durable sync; v29 retains and
> validates every graph/degree/reverse extent and every completed-bucket cursor
> and payload, then sorts each small shard file once and performs a buffered
> k-way merge into monotonically written predecessor planes. The distinct
> Bishop+Devil B1 and C1 records are now explicitly **PAUSED**, joining its
> already-paused A1 record; none is runnable or consuming fleet resources.
> Supervision rejects any one-row `superseded_by` edge whose replacement tracks
> different ledger material, and the focused regression test now also requires
> every current Bishop+Devil record to remain paused behind lone-Devil C1.
>
> The finalization audit repaired stale watcher manifests before any result was
> lost. Immutable i024 manifest v38 now binds opposed Bomb/Ghost to its v16
> post-compaction continuation and same Sniper/Ghost to its active v12 unit,
> while retaining Checker and the downstream Berserker v17 solve;
> sha256:c07dc36e862d73cabbcd78e601ef21ab85dcfce7cfe505f4df423c6585caf5f9
> is S3 VersionId `ki4PSBI_IlfCI96C1T_B3Ce2YcjDlQAt`. Immutable i08 manifest
> v30 names Prince/Ghost's actual v19 unit, retains Penguin/Ghost, and adds
> opposed Sniper/Ghost; sha256:cfb01b277f4d0e85e8e7dd5d88c80934ccdf2f5dbdf18fd9e0479c2d34f1bc23
> is VersionId `lvHmOPcU.Bhx4HrJk4sI3A26qB9jUEd5`. Both were exact-version
> downloaded, hash-checked, atomically installed with their predecessors
> retained, and exercised by clean watcher passes. Their current supervisor
> source bindings and per-job S3 certificates now point at those exact versions.
>
> The 2026-08-29 certification sprint also armed a phase-aware i0b CPU handoff
> without restarting either solver.  Ninja/Ghost retains CPUs 4--31 during its
> parallel Bellman sweep.  After the exact iteration-17 492,960/492,960 marker,
> the watcher first shrinks Ninja to 4--7 and only then expands Dragon/Ghost to
> 0--3,8--31 for Ninja's compaction; it shrinks Dragon before restoring Ninja
> as soon as iteration 18 emits its first 5,000-geometry milestone.  An exit
> trap restores the original sets, so interruption cannot leave an overlap.
> Script sha256:`67fe3456c8f287553f19e24e35a43b362013b4210a3a5177c0bbb732bbca702a`
> is S3 VersionId `saoWzKT2Vq7jcr6XVnrd0HmUN9upc.SG`; service
> sha256:`f0cbe713da1bc383d25b11ffbe78e222173a9eafb114dc324591d2e912a019c0`
> is VersionId `CvE68GOfSsXG70LPWNoH3OCVQQW7bwoi`.  Both deployed hashes match,
> and the initial state was the expected Dragon 0--3 / Ninja 4--31 allocation.
>
> The 2026-08-29 11:38 UTC Devil final-mile audit found that those completed
> fragments were only partially retained across live host disks, despite the
> earlier ambiguous claim that all eleven were restored locally. All eleven
> exact S3 VersionIds now coexist on i03 under
> `/mnt/ultimatefish/devil-spawned-c1-v28/fragments-restored-v1`; every file is
> exactly 690,196 bytes and its downloaded SHA-256 matches supervision. This
> also corrected the prose-only D3 hash typo to sha256:
> `f81dd61ca01c641f8757e96b9098102835a27fb003599d2d2d2d959f236aad35`;
> the supervisor key and VersionId `6GlrBTjRWRcVDzW13YCxRb3XHdwRikzB`
> were already correct. An idempotent two-minute finalization timer now waits
> for a nonempty C1 fragment, rehashes those eleven files, and invokes the
> fail-closed twelve-square merge immediately. Merger sha256:
> `60b029808ffad9daa3df8555f40f430ff2a2e112fc633451b01b208d0f69a001`
> is S3 VersionId `7yZBMWmE7w08gv70a0buartyxAdAlcrv`; runner sha256:
> `6e0bd3f61f129409a97dc19d193affa8e6646a77229f9bc6015b470440a31154`
> is VersionId `7UgpmvPwLle1rm9EjNdW0f5DcSXwoOVx`. Both systemd files are likewise
> version-pinned and all four deployed hashes match. The first invocation was
> a clean no-op because C1 roots do not yet exist; no completion is inferred.
>
> Opposed Penguin/Ghost has an independently complete 3,943,680-geometry,
> 553,681,140-edge transition graph across 64 authenticated shards; every merge,
> byte-range, regeneration, payload-binding, and conservation residual is zero.
> The first continuation exposed and then fixed missing worker/resume CLI
> plumbing. The corrected result subsequently passed the full proof,
> preservation, import, and trivial-reachability chain described above. Its
> immutable information archive, preservation receipt, and result certificate
> are now explicit per-row supervisor certificates, so the obsolete failed v29
> unit cannot misclassify this already certified class as a live failure.
>
> Two large nonterminal Ghost tails remain intentionally **PAUSED**, not failed
> or preserving: same Jester/Ghost and crossed Jester/Ghost. Opposed
> Sniper/Ghost's complete retained transition graph is now authenticated on
> i08 and its fresh corrected fixed point is running on CPUs 32-63, disjoint
> from Prince/Ghost on CPUs 0-31. Opposed Bomb/Ghost was selected as the
> closest retained result:
> iteration 11 left only 4,865 owner and 75,467 observer root changes, and its
> iteration-12 checkpoint had reached geometry 325,000/492,960. That 59.5 GB
> tree was streamed into a 19,157,903,097-byte archive, SHA-256
> `918b1122428a29b537450bb37c669ea3f9651bacdeb89733d2822b777689b1f1`,
> S3 VersionId `.j1Dy1Ydu8FfX2ma7LJd_SDjldO24PB7`, then hash-verified and
> restored to i024's persistent 3 TiB volume. The first v9 invocation failed
> closed operationally when it authenticated the scratch but omitted the
> explicit fixed-point resume flags and began a fresh iteration 1; it was
> stopped immediately and retained as evidence. The frozen Bomb adapter then
> exposed a second fail-closed defect: unlike the shared solver it did not
> accept or propagate resume metadata. The tested current adapter adds that
> plumbing. Its ARM64 binary is sha256:
> `7e66fa2b0a7c42342dbf834c86053a727ff98129965dadb105110d6cd4e74967`,
> S3 VersionId `pBgheyagyrJjXlQuWv9bEuiEJDtfKTtT`, and passed all
> 151,831,680 codec states. Corrected resume v15 binds completed iteration 11
> to physical root slot `next` and ROBDD slot `b`, enables 16 Bellman workers,
> and authenticates the binary, concrete oracle, lower Ghost model, and every
> transition component before reopening iteration 12. The first v12 attempt
> reached geometry 375,000/492,960 but had already grown to 1,225,325,472
> nodes against its 1.5-billion cap; it was stopped at the safe iteration-11
> boundary. V15 raises the authenticated node and unique-table limits to
> 3,000,000,000 and 4,294,967,296 and sparsely extends both fixed-capacity
> ROBDD node slots from 13.5 to 27.0 GB. It deliberately retains the disposable
> unique tables at 8 GiB so the solver rebuilds a correct 2^32-slot index in
> parallel under the new hash mask. An earlier zero-padding attempt failed
> closed after admitting duplicate cache nodes; the exact iteration-11 root
> arrays remained intact. The authenticated archive was restored again, the
> corrected v15 continuation completed its 2^31-to-2^32 unique-table rehash
> with 837,617,027 old nodes, and iteration 12 is now advancing with 16 workers
> without duplicate nodes or a transition residual. Same
> Sniper/Ghost is the
> second retained candidate. Its 174.6 GB tree was streamed without consuming
> the source host's disk reserve into an 11,795,218,969-byte immutable archive,
> sha256:`2261b5bed83f0ead28e78fb68bdb4cf65d0fbbfadd89b15d465f29b5874faffc`,
> S3 VersionId `F9Yi0d6S4bEyr9pzHYANdZ7sDXoqNxcd`, then hash-verified and
> restored to i024. A current ARM64 binary, sha256:
> `d067036152298e71e00fef4f620c7779dc65a8d1dcb00dc202962df994909be1`,
> VersionId `JTMG8Rau2spftyMYcWmkULa7ZB77czF3`, passed the exhaustive
> 607,326,720-state codec/remap test. Resume v11 binds completed iteration 7 to
> physical root slot `next` and ROBDD slot `b`; it is authenticating the retained
> 3,943,680-geometry transition graph before continuing iteration 8 with 16
> Bellman workers on CPUs 16-31, disjoint from Bomb on CPUs 0-15. The shared
> host now also has an authenticated two-minute watcher that automatically
> drives a successful Bomb, same Sniper, or corrected Berserker solve through
> independent proof, immutable preservation, receipt upload, and import-ready
> evidence instead of leaving it in bookkeeping limbo.
> Opposed Berserker/Ghost's failed v9 never entered a fixed point: its frozen
> binary was compiled without the required Berserker-primary source-order flag
> and correctly rejected a header whose authenticated fields are primary=7,
> secondary=11, opposing=1. The complete 363.9 GB transition tree is preserved
> as a 29,076,339,287-byte immutable archive, sha256:
> `10709917e47875e13c3e8f873df0f9fff9c507a1546f692ff4d552fc193930ea`,
> S3 VersionId `B2A.X4sRblLBMfwZl7nrFiHO4QNE19Fg`, and was restore-authenticated
> on i024. A corrected current binary passed all 1,518,316,800 codec states
> with remap residual zero. Its first solve failed closed on stale exact
> lower-table flags. The retained graph is therefore being rebound and
> exhaustively reverified before solving. The rebind implementation already
> supported parallel geometry rewriting, but its initial wrapper omitted the
> worker environment and ran on one core; v14 sets 16 workers and restores the
> original authenticated graph before replay so a partially rewritten marker
> can never be trusted. Wrapper sha256:
> `91e3e0fac1232f17f21ff3707e0619a8372a17ddddb7f547e8fa3ff2422e8a16`
> is S3 VersionId `FFCxobpBZ_bmtvpjEY7w0pvAKWYbNH.1`. The first restore helper
> successfully restored the graph but then failed operationally because the
> stopped transient unit had been collected; v15 now recreates the v14 unit
> explicitly and is sha256:
> `908b28ab448d4de0ff3a10864426fbb3a89dccb780433aad95b0ede3b48f86ff`,
> S3 VersionId `_NEhvAyRbACTqdOgfJ_w1a42fPucP.Jb`. Pawn/Ghost's now-certified
> v17 source also
> authenticates exactly:
> its 19,712,000-byte source bundle now has an explicit hashing bound rather than
> failing the supervisor's generic 16 MiB probe limit. The v17 reload has now
> authenticated all 1,971,840 geometries, 138,954,816 worlds, and
> 1,405,040,664 edges with metadata, transition, geometry-start, and
> conservation residuals zero, then reopened and completed the iteration-21
> fixed-point checkpoint. Its automatic preservation chain is bound to corrected watcher
> sha256:`996009080b36eed172887d321b72bf9903a49eabf659497291f2566a00127e3d`,
> preserver sha256:`9522ecb8d76991c9c0327b5861daf1ca6a7ccd780a882f99678f35602d45a47c`,
> and manifest sha256:`ba51c827faf7408a38e5d5da46e708ed072656c43b073b0fbe9099a1656479db`
> (S3 VersionId `18BNe5eJG8So08Xm0wwe1Qbtq2Zz_6Oc`). A continuous receipt
> poll is active, but no result is counted before solve, independent proof,
> preservation, and ledger import all succeed.
> After v17 cleared its authenticated reload and one-time lower-ROBDD import,
> it re-entered iteration 22 with 32 Bellman workers. The serial Devil reverse
> merge on the same host was therefore confined to CPU 31 and Pawn/Ghost was
> expanded from CPUs 0-15 to CPUs 0-30, preserving a disjoint allocation and
> both unchanged checkpoints. The first complete supervisor interval after the
> live reassignment measured i098 at 25.327 busy vCPUs and the fleet at
> 134.464/224 (**60.0%**); this is an immediate mixed-phase measurement, not a
> terminal utilization claim.
> Prince/Ghost already had 32 Bellman workers, but its live unit unnecessarily
> confined them to 16 CPUs. The unit was expanded in place to CPUs 0-31 without
> restarting or discarding its active iteration; the supervisor now reserves
> the same range. The complete opposed Sniper/Ghost transition graph was reduced
> to its authenticated 95.4 GB minimum by deliberately excluding the known
> truncated partial roots, then preserved as a 6,026,797,111-byte immutable
> archive, sha256:`70ccd4b4b757afc5b6ecf951fab137f4eae6558d40ecb2252afbac8053c64692`,
> S3 VersionId `BDet.ZYR8kAAi_wGG8QDrI32vAhGElus`. It is being restored to
> the same host for a fresh 32-worker fixed point on CPUs 32-63, with automatic
> preservation already registered in the host watcher.
>
> Same Ghost/Ghost is no longer paused. Its authenticated transition graph and
> retained work tree occupy only 76,736,659,049 bytes on i03's separate
> `/mnt/ultimatefish` NVMe, with 838,137,307,136 bytes free when audited. The
> stopped v2 binary has no fixed-point resume interface, so its partial roots
> cannot safely seed a continuation. Sprint v3 first SHA-authenticated and moved
> every partial root plus the old solve log into the retained
> `retained-partial-solve-v2-20260828T1455Z` evidence directory, then began a
> fresh solve over the unchanged authenticated graph. It is confined to CPU 0;
> Queen remains confined to CPUs 1 and 3-31 on a different NVMe. Wrapper
> sha256:`3faf57cbefde08214bb76cca1ddfe8d62bbad4ad98708d4d2b605e123f8e6b20`
> is S3 VersionId `aQQrrBJOmITcdD7T2NiLUSx6PK25bL6V`. The exact supervisor
> authenticates the new unit as running; no launch is counted as completion.
>
> A proposed solve-only v4 continuation for opposed Sniper/Ghost was rejected
> after a four-minute canary exposed an older-runner incompatibility. Although
> `--solve-existing` correctly retained the complete transition graph, that
> runner did not forward explicit fixed-point resume metadata; the legacy solver
> therefore opened its root arrays with `O_TRUNC`, invalidating the retained
> iteration-30 roots. The service was stopped immediately, and an exhaustive
> mounted-storage search found no independent root copy. No result or proof was
> emitted. The authenticated transition graph remained intact and is now the
> input to the fresh v12 fixed point described above; no invalid partial root is
> included in its archive. The checked-in current
> runner now refuses any solve-existing invocation over root files unless
> `--resume-fixed-point`, iteration, current slot, and BDD slot are all explicit
> and their required files exist. Guarded runner
> sha256:`0b3219072d994650e0b556986a7f443e161081f2032cdd3859bf63e1f7bf59cb`
> is S3 VersionId `9stH053i3cURLVJUCJ9jj7PqaVwGrhEX`; the unsafe remote v4
> wrapper was replaced by an authenticated rejection guard while its original
> bytes remain retained as incident evidence.
>
> The sprint preservation watcher also now fails closed on systemd's
> `activating` and `reloading` states. Long-running `Type=oneshot` solvers remain
> `activating` until their complete solve and proof command returns, so treating
> merely "not active" as terminal could have observed both result files before
> the final proof phase. No candidate had emitted result files when this race was
> found, and nothing was prematurely preserved. The watcher now also
> authenticates any existing receipt and its local archive/certificate hashes;
> a failed or partial preservation attempt is moved to a timestamped retained
> quarantine before an automatic clean retry, instead of stranding the solved
> result forever. Corrected watcher
> sha256:`996009080b36eed172887d321b72bf9903a49eabf659497291f2566a00127e3d`
> is S3 VersionId `o0GbcdplkutybEQptXyemxHWkvCxeyNy`, is installed identically
> on all five instances with both prior executables retained, and is now an
> exact supervisor source binding for the active Penguin, Checker, and Prince
> certification chains.

> **2026-08-25 opposed Angel/Ghost correction:** the former information solve
> reached its Bellman fixed point with zero equality and monotonicity residuals,
> but its exhaustive singleton comparison correctly rejected 529,216 positions.
> A traced witness proved that the information recurrence and retained
> 985,920-geometry transition graph were consistent while the concrete
> `kghostkangel.uftb` source was stale under the current generator. A clean
> current-source rebuild completed 151,831,680 states and 1,406,960,512 edges,
> independently verified W/L/D 57,751,024 / 31,069,668 / 63,010,988, and
> produced table sha256:`55c131bb34977996b086111dbc23094c02a0d175f363de9796d59b37488062db`.
> Deterministic archive
> sha256:`d965ca736d388256a1a9aa7a4490a3c02bf7dc19acd4437aacb0966c945edce7`
> is S3 VersionId `l7yrxHEKyFQExLOHOfDAkbfua.rIb.Hi`; exact-version download,
> full-SHA, archive restore, and native verification residuals are zero.
> Current-source verification v23 preserved the old rejected run, cloned its
> converged iteration-43 roots and immutable transition payload, and rebound
> only the authenticated source-provenance field with payload residual zero.
> Its disjoint 28-worker certification pass completed with zero Bellman,
> monotonicity, singleton, rank, grouping, conservation, source-remap, and
> structural residuals. The now-certified public-information result has sides
> `45,096,416 / 25,816 / 24,481,148` and
> `57,050 / 31,004,114 / 35,448,972` (W/L/D). Deterministic information archive
> sha256:`44b3dafa5c85997601a9c70f6a1dbc0bbf85c47a9cef00ffef28a0d6c6ad2b4f`
> is S3 VersionId `bghKvWB8mBgv.o85DRh8QE4kgq9jAdtA`; result certificate
> sha256:`127dac280cfd8410a8a40ae440c57c31d334206951692f5026a0cf350f23221e`
> is VersionId `1.c6zrpLh19U6hb5ADG.pTgO2XP1C4y7`. The authenticated trivial
> sidecar sha256:`c7ef86c588d9ab26a90bf3a7426da8fa072334ac74eea584af1fc930505715c2`
> is VersionId `Rv2pWgmApu9ya25Z8i0.6AgtRUZtLQed`; the ledger and plot subtract
> its explicit W/L/D subsets.

> **Current AWS fleet audit (2026-08-27 18:23 PDT / 2026-08-28 01:23 UTC):**
> Same-side Bishop+Devil C1's retained checkpoint was independently copied and authenticated on the
> approved persistent 3-TiB gp3 volume (receipt
> sha256:`0ca103bc4babb4f6b27e98925f33a4009b916a3c08299e7c80bf4bab6233c7bf`).
> Instance `i-08c0f44a1776cb34a` was then resized from `r8gd.8xlarge` to
> `r8gd.16xlarge`; both EC2 health checks passed before any workload resumed.
> The 48-worker v27 run reconstructed all 16,638,596,268 retained keys and
> committed closure ply 14 at 27,209,034,909 states with a
> 10,570,438,641-state frontier. Its first 32-billion disposable-hash envelope
> then failed closed before committing the partial next layer. The checkpoint
> remains intact and the same exact source/binary has resumed with the reviewed
> 40-billion proof and hash gates. Source
> sha256:`de523aad058723e70820d7cd645fc4ec3a23a0422e7348c836bac29b8f06f214`
> is S3 VersionId `VlBg9ai6PixKEldf0wlYAIM0USJwvmfk`; binary
> sha256:`6b8fe2bfab9f76fcbfdedfbf67119cae418a749c86b615b40004be3e5b36ea68`
> is VersionId `Mh9h30fIAXagqVQUWn51_xmhQtNrJvMO`.
>
> The B1 scaling gate was also positive: after resizing
> `i-024a2073283e4336e` to `r8gd.16xlarge`, its 48-worker retained-index pass
> initially advanced 900 million of 11,462,218,346 keys in about one minute.
> The persistent gp3 recovery copy then saturated at about 702 MiB/s and left
> most workers in I/O wait, so B1 was stopped at the retained boundary and its
> current payload was copied to the host's new two-device XFS RAID0 scratch.
> Independent full-file gp3 and RAID manifests matched; receipt
> sha256:`57727f4eaf38ca7f007d90c353229042a002016bec0a19f621e8798cd2936b2b`
> binds manifest
> sha256:`ee880ec034c37ecf0c2ef864bd710f21462ac46e8aea879c5ee2f6aabd21838b`.
> The gp3 source is retained unchanged. The v29 RAID run then advanced 3.7
> billion of 11,462,218,346 retained keys in its first minute with workers at
> about 96% CPU each and 935,200 KiB/s from RAID. Four co-resident Ghost jobs
> were restored from authenticated resize receipts on disjoint CPUs. This
> storage move is an active utilization repair, not an accepted low-CPU steady
> state.
>
> This is a superseded 2026-08-27 snapshot: at that time the plot remained
> 6,126 x 2,904 pixels and eleven lone-Devil root fragments were authenticated.
> Authentic lone-Devil C1 subsequently completed, but the resulting 12/12
> statement certified only the causal entry-root projection, including starting
> Minions that could have been placed by that Devil, and is retracted
> as authority for the full stateful class. The 27.209-billion-state C1 checkpoint discussed
> nearby belongs only to `same:bishop+devil` and is not lone-Devil evidence.
> The eleven historical lone-Devil partition records are explicitly paused;
> C1 v29 is the sole S3-only certification authority. This prevents a missing
> retained local fragment from making an obsolete shard appear runnable after
> the merged result has certified. The distinct Bishop+Devil records remain
> paused and cannot supersede a lone-Devil job.
> The first complete post-RAID supervisor interval measured
> i024/i03/i08/i098/i0b at 37.095, 5.797, 27.851, 22.420, and 19.249 busy
> vCPUs respectively: **112.412/224, or 50.2% fleet CPU**. This materially
> improves the pre-repair 61.580/224 (27.5%) sample but remains below the 75%
> engineering threshold, so later storage-bound and serial phases remain
> subjects for continued optimization. The supervisor has 59 certified support
> jobs, 19 running jobs, zero current failed jobs, and one newly completed
> transition-support job awaiting preservation/solve staging. The canonical
> ledger remains **456 certified, 0 preserving, 19 computing, and 0 failed**
> rows.
>
> **2026-08-27 19:14 PDT / 2026-08-28 02:14 UTC continuation repair:** the
> completed opposed Berserker/Ghost transition graph is no longer stranded.
> The obsolete 4.2-billion-state duplicate C1 service on i0b was stopped only
> after its empty frontier and retained 67,200,000,000-byte exact-key file were
> authenticated; no checkpoint file was removed. Its ten-CPU allocation now
> contains the certifying Berserker/Ghost solve, whose exact source bindings
> passed the checked-in supervisor and whose fresh solve process is advancing.
> The supervisor now rejects transition jobs that declare a mandatory
> continuation unless their replacement is a certifying job for exactly the
> same ledger material. The first complete post-change interval measured
> i024/i03/i08/i098/i0b at **50.439, 5.989, 12.225, 22.428, and 21.789 busy
> vCPUs**, respectively: **112.870/224, or 50.4% fleet CPU**. This remains an
> engineering failure below the 75% threshold; the active repair is a
> model-compatible parallel Ghost benchmark plus persistent/compact Devil
> indexing, not acceptance of the serial and memory-bandwidth tails.
>
> **Earlier audit detail:**
> opposed Copycat/Ghost completed its 52-iteration information fixed point and
> passed Bellman equality, monotonicity, singleton-dominance, rank, grouping,
> conservation, source-remap, structural, and transition verification with
> zero residuals. Its deterministic archive
> sha256:`9f33560e7c5a23604f32a630646bce69393504dc7bb071f5e29b549734bfcff7`
> is S3 VersionId `_3YYFiavaNSS3dcn3qUzzNAO2P8fLFAM`; result certificate
> sha256:`30d4d8e33a491eb418ec028258b846293a2bf86ffaed9fb4fdd3fd32b0a4ca41`
> is VersionId `Sb6e6zm1nFZYwwDyjlwD8Zgl3H2PSFB1`. The preservation helper's
> initial journald-only proof lookup failed closed because this solve retained
> its proof in a file; v2 now accepts an exact proof-log path, retained the
> failed attempt, and completed exact-version restore authentication. A
> nine-worker information-trivial v2 audit then produced exhaustive bracketed
> subsets; sidecar
> sha256:`641ae128975689e2e4089a3c622ddc2a2a8e33679b3d17e524d29c74aa7e016c`
> is VersionId `nlXOUFjdJ808e18fofZRvjsjc2UvYltT`. Every certified row now has
> bracketed trivial counts, and the regenerated plot subtracts those counts.
>
> The exact checked-in supervisor authenticated 76 completed support jobs and 20 running
> jobs, with zero completed-unpreserved jobs. C1 is not counted as running:
> v24 failed closed after its retained 16,638,596,268-state closure and
> 11,541,132,580-state frontier exceeded the configured exact-graph limit.
> Its source checkpoint remains intact while the checked-in v26 migration
> copied and independently hashed it onto the approved persistent 3 TiB gp3
> volume before the larger-memory resume. Eleven of twelve
> fixed-square Devil fragments are version-preserved and exact-restore
> authenticated. That older reverse-state progress marker is superseded by the
> current closure state documented above.
> C3 fragment sha256:`64386789206e5e82e3c5cf06cf8c8f1419f3e83fbc2e0d8905b9b820c0feec2f`
> is S3 VersionId `nkZoxSS1sWPOkCAa2imptFtEENWt1bPT`; D3 fragment
> sha256:`f81dd61ca01c641f8757e96b9098102835a27fb003599d2d2d2d959f236aad35`
> is VersionId `6GlrBTjRWRcVDzW13YCxRb3XHdwRikzB`. Both exact-version restores
> authenticated before the supervisor promoted them from completed-uncertified
> to certified support artifacts.
> The active opposed Prince/Ghost solve was incorrectly reported as a source
> mismatch because its exact 37,118,976-byte source bundle exceeded the
> supervisor binding's default 16-MiB hash limit. Its S3 and retained hashes
> were identical; the explicit 40,000,000-byte binding gate now authenticates
> it as source-exact RUNNING without restarting or changing the solve.
> The stateless-companion codec is now running genuine
> retained-edge, retrograde same-side Bishop+Devil square-A1 and square-B1
> partitions on 22 and 28 workers. A1 has completed closure ply 16 at
> 7,870,840,299 committed states; B1 has completed ply 13 at 7,222,922,911.
> Both crossed the original 6-billion guard, failed closed,
> and were repaired and restarted from those retained checkpoints at a
> 12-billion guard. They have now migrated from v10 to the exact-version v11
> binary without deleting the committed closures, frontier files, or dense
> keys. Its exact 34-bit
> disposable hash slots remove a further 12 GiB from the 2^34-slot geometry
> compared with v9, while leaving proof keys and game semantics unchanged. V11
> replaces the non-restartable random-write reverse pass with 512 fixed parent
> shards, 16 child-locality spool buckets, atomic per-shard completion markers,
> exact extent and rolling-hash validation, parallel sequential merge, and
> edge-conservation checks. The shard geometry is independent of worker count,
> so a service may safely resume with a different non-overlapping CPU allocation.
> A built-in synthetic restart test proves that a second run reuses all completed
> shards without enumerating a successor again. Source bundle
> sha256:`bb04e578ce251a304bca3eafd329ed49a04aed7656a29f7431fef8a1d7cca400`
> is S3 VersionId `vKTPMe4bbaWdgkmQ6ms8CkP9AIK7g0Fi`; binary
> sha256:`db53f32f74d536eaa07fc514cafd1ae330eb3747c619e5b5c73058827fc1feee`
> is VersionId `lAif3Ta98i75vM4crpnL5rQv_8PEDfwU`. Both retained indices rebuilt
> successfully from the committed checkpoints. These
> are certifying tablebase partitions, not capped censuses. They are also
> self-contained: the exact key retains the Bishop after Devil capture and
> continues through surviving-Minions and Bishop-only states, so this class
> does not depend on the merged `kdevilk.uftb` result. The plot's **COMPUTING**
> state is therefore literal, not an inference from the unfinished single-Devil
> row. V11 restart profiling found the parallel dense-index rebuilds saturating
> the retained EBS volumes at about 0.7--0.9 GB/s: all 22/28 rebuild workers
> were active at about 60% CPU each while the hosts spent 27--36% in I/O wait.
> Both 7.87-billion- and 7.22-billion-key indices nevertheless rebuilt in about
> two minutes. The subsequent 9--37 GiB frontier restore is deliberately one
> sequential read and is a short storage-throughput interval, not a serial
> solver phase. No checkpoint was deleted. The
> canonical ledger is **456 certified, 0 preserving, 19 computing, and 0
> failed** rows, and the >1-KiB trivial-backfill inventory is empty on every
> host. After the NVMe migration and measured working-set gate correction, the
> supervisor's first post-migration interval measured only 29.135/160 busy
> vCPUs and was incomplete because it straddled the service restarts. This is
> below the automation's 75% engineering-action threshold and is not accepted
> as a steady-state result or excused by `ready_jobs=[]`: the checked-in solver,
> checkpoint layout, sharding, scheduler reservations, and instance/storage mix
> must be profiled and improved until the active workload either reaches the
> threshold or is moved to a demonstrably better-matched host. The immediate
> repair is the restartable reverse spool above plus parallel ROBDD compaction
> marking for Ghost jobs; deterministic compact-node copying remains serial.
> C1 emitted no new 10-million-state milestone during this
> accounting interval. Its slow milestone
> cadence remains storage-bound rather than failed or idle. A direct C1 sample
> found 25 workers in I/O sleep, 0.12 busy CPU, and its local overflow NVMe at
> 100% utilization with queue depth about 65; the unit and checkpoint remain
> healthy, but this last fragment is the critical storage-layout bottleneck.
> No current run failed.
>
> The explicitly approved utilization repair added encrypted gp3 volume
> `vol-0ba9c99f9f8302fbf` to i08: 3 TiB, 16,000 IOPS, and 1,000 MiB/s,
> mounted persistently at `/mnt/ultimatefish-devil-v11` with a 150-GiB free
> gate. Same-side Bishop+Devil square C1 is a third disjoint certifying
> partition on i08 CPUs `0-7,16-31`; opposed Prince/Ghost retains CPUs `8-15`,
> so i08 is fully allocated without overlap. A matched resource profile then
> found the Devil cgroup pinned to its former 190-GiB soft limit while gp3 was
> pinned at exactly 16,000 5--6-KiB random reads/s, 100% utilization, queue
> depth 77--80, and 57% host I/O wait. Prince used only 14 GiB under an obsolete
> 128-GiB hard cap. Its live gates are now 20/24 GiB and Devil's are 205/215
> GiB; their combined hard limit is 239 GiB on 247 GiB physical RAM, retaining
> an 8-GiB OS margin. Neither PID changed. The first post-change profile showed
> 0.9% I/O wait and 97% CPU busy; the following complete 95.7-second supervisor
> interval measured Devil at 11.531/24 and Prince at 7.986/8 busy vCPUs, versus
> 4.991/24 and 7.992/8 across the preceding mixed interval. That improvement
> did not persist through the next cold/random portion: the complete 53-minute
> interval measured Devil at 0.370/24 while Prince remained 8.000/8. A fresh
> device sample again measured 100% utilization, queue depth 82--85, 17,100
> reads/s at 11 KiB each, and 53% host I/O wait while the Devil cgroup sat
> exactly on its new 205-GiB soft limit. The checked-in cache correction is
> retained. The explicitly approved online increase to 40,000 IOPS completed
> at 05:12 UTC without detaching the 3-TiB volume or changing its 1,000-MiB/s
> throughput. The first complete post-change supervisor interval measured
> i024/i03/i08/i098/i0b at 4.078, 5.014, 8.671, 1.213, and 12.875 busy vCPUs:
> **31.851/160, or 19.9% fleet CPU**. Within i08, Prince/Ghost used 8.030/8
> while Devil used only 0.641/24. A direct five-sample device profile proved
> that the control-plane change was effective but not sufficient: the volume
> delivered 39,969--40,005 reads/s and about 185 MiB/s in 4.74--4.75-KiB
> requests, with about 1.8-ms read latency, queue depth 72, and 100% device
> utilization. Twenty-three of 24 Devil workers were in uninterruptible I/O
> sleep, full I/O pressure was about 65%, and full memory pressure about 10%.
> Thus 40,000 IOPS was not accepted as the end of the diagnosis; the compact
> key-layout repair below is required to remove the random-read working-set
> miss rather than merely buying more IOPS. As an intermediate check,
> the live Devil soft cache gate was widened from 205 to 212 GiB while retaining
> its 215-GiB hard cap and Prince/Ghost's 24-GiB hard cap, so the combined hard
> bound still leaves the reviewed 8-GiB host margin. The original PID and every
> checkpoint were retained. Memory immediately filled the extra 7 GiB, but the
> measured Devil rate remained 0.454/24 busy CPUs over 202 seconds, confirming
> that capacity misses are still gated by random-read IOPS rather than the
> former soft throttle. The idempotent launcher also now authenticates and
> reuses an already-running immutable executable instead of attempting to
> overwrite it and failing with `ETXTBSY`; mismatched binaries are downloaded
> to a staging path, hashed, and atomically installed. C1 has committed
> closure ply 12 at 5,097,463,688 states; this is progress, not a launch-only
> inference. The latest complete CPU-delta interval measured i024/i03/i08/i098/i0b
> at 3.937, 4.950, 30.478, 1.169, and 12.295 busy vCPUs:
> **52.829/160, or 33.0% fleet CPU**. Fleet utilization remains below the 75%
> engineering-action threshold because the other large Devil closures are in
> checkpoint/writeback intervals and most remaining Ghost tails are serial;
> it is not considered an acceptable steady state.
>
> Fresh Devil root seeding was also identified as a serial initialization
> defect and parallelized across the configured workers. The immutable v12
> source bundle
> sha256:`826f6c86ba32bad33a1b197b91fbf0f70e936dc62cc11d844d0b0dfd9ee89537`
> is S3 VersionId `cVreVCH42Td2TcMOWz2V2GK7oDkMohxz`; binary
> sha256:`6deb5153d6b5ecc717d93f205904e5ec0b13c4875f847febecb47cbc217ec09e`
> is VersionId `WQzYc5pFuT3_gATtuRrTKhNyZfMOyYDj`. Square C1 finished seeding and
> entered its 24-worker BFS before that binary was ready, so it was not
> restarted merely to force a version change; v12 applies to subsequent fresh
> partitions. V13 additionally packs the persistent proof key from 16 to 14
> bytes after proving that its high word uses only 48 bits. At the 12-billion
> state guard this reduces the maximum key plane from 192 to 168 GB, saving 24
> GB (12.5%) of RAM, checkpoint storage, and memory/storage traffic without
> changing the hash or game semantics. Its checkpoint schema is deliberately
> version 3 and rejects the older 16-byte v1/v2 layout; the live v11 C1 process
> and all of its retained data are therefore untouched, while fresh partitions
> use the smaller format. A fresh ten-million-state capped construction and an
> exact restart from its committed layer both passed, with a byte-exact
> 140,000,000-byte key file. Immutable v13 source bundle
> sha256:`7ecf1254fb3a349c77480b0b945320a26ccaca9c0cf5a35519e352c41358d911`
> is S3 VersionId `62_BTTbiHptflCYIklHJfckNLtFxlwJV`; binary
> sha256:`4d434e1d32cc7c82a1c059c6ea9b75e2c71781caf271295d1f9e0e19c8cf5cc4`
> is VersionId `RSD77tGXkSro0zsbp0GbRtkO8HGMl_v.`. V14 adds an explicit,
> fail-closed, nondestructive v2-to-v3 migration: it snapshots identical
> metadata around already-open source descriptors, validates the full source
> key/frontier extents, rejects any high key outside 48 bits, converts only the
> immutable committed prefix, copies the exact frontier, fsyncs both payloads,
> and installs v3 metadata last as the destination commit marker. The original
> checkpoint is never renamed or deleted. Its built-in fixture verifies every
> converted key and both sparse extents. Immutable v14 source bundle
> sha256:`4044755f9a2530ecd529f47856d9fecef4762a6295e3bcd8c30e4fb922ad5231`
> is S3 VersionId `8_FQj_Org_G642sz312G3eXb5UybUAq5`; binary
> sha256:`d4619620ad7c4718f07cf8f3e8b818494d4416fa9b29db4ce8f489792761fe50`
> is VersionId `UfLeaYlMybeKNtsw37qdU7vqN.IRIXP2`, and exact-version restore plus
> the native migration self-test passed on i08. C1 was quiesced at its retained
> ply-12 boundary only after the separate migration was active. It converted
> 5,097,463,688 keys and 1,444,998,022 frontier entries with canonical key hash
> `977519b260fe8dcd`; the committed destination is 179,559,984,220 logical and
> 93,392,302,080 allocated bytes, versus 203,559,984,220 logical bytes in v11.
> The entire v11 checkpoint remains inactive and intact as rollback. V14 then
> authenticated and rebuilt the disposable index from all committed keys,
> restored the frontier, and resumed the same non-overlapping C1 shard. Its
> first complete 73.9-second parallel-solve interval measured 22.371/24 busy
> Devil CPUs and 8.107/8 Prince/Ghost CPUs: i08 reached 30.478/32 (95.2%),
> materially above v11's 8.343/32 host rate without additional recurring cost.
> Busy-host response compaction previously dropped active
> checkpoint aggregates before trimming diagnostic text. The supervisor now
> preserves a hash-bound compact recovery summary through both transport
> compaction levels. Its live C1 observation authenticates three checkpoint
> files, 179,559,984,220 logical bytes, 93,392,302,080 allocated bytes, and the
> newest-write timestamp; no service restart was needed. The supervisor now
> records 21 running services, 456 certified
> ledger rows, 0 preserving, 19 computing, and 0 failed.

> **2026-08-27 utilization and restart repair:** the approved 3-TiB gp3 volume
> `vol-0ba9c99f9f8302fbf` is complete at 40,000 IOPS and 1,000 MiB/s. The two
> 2-TiB retained-checkpoint volumes `vol-0f61279e8acdfeade` and
> `vol-000eed5cee33af507` were also raised online from 16,000 to 40,000 IOPS
> without detaching them or deleting a checkpoint; all three volume
> modifications are now 100% complete. Merely raising IOPS was insufficient, so
> Devil v16 replaces the power-of-two dense hash with an exact stripe-aligned
> non-power-of-two table and wrapped modulo probing. At the 16-billion-state
> gate this removes roughly 68 GiB of disposable hash address space while
> retaining packed-slot locking and exact key comparisons. The v16 source
> bundle sha256:`ac4e0a7c496f57efae814f467a91e2137a036117e2e14e1f1b2dfd519671198d`
> is S3 VersionId `mAKreQKZ84JLzaYZN_uc9dVJZcALxV3o`; the self-tested binary
> sha256:`e5dedeaf255ba2194f9fe31de421cd98a1e572ac617905758a3a9a4e3a74ad79`
> is VersionId `WBpKAeql5FCmnAU0vx5QeEh8U9mPtkhD`. All three same-Bishop/Devil
> partitions authenticated and resumed from their retained v15 committed
> keys/frontiers. Their live resident sets fell from roughly 202--225 GiB to
> 150--200 GiB during index reconstruction, and square C plus its disjoint
> Prince/Ghost solve now sustain 31.6/32 busy vCPUs.
>
> V17 removes the remaining random-key-read bottleneck rather than treating
> the 40,000-IOPS ceiling as an acceptable steady state. Each occupied compact
> hash slot now carries a packed four-bit key fingerprint, so restart lookup
> reads the exact 14-byte key only for the matching one-sixteenth candidate
> subset; exact key equality remains authoritative and fingerprint collisions
> cannot merge states. The native reverse-spool and checkpoint-migration tests
> pass. Immutable source bundle
> sha256:`7cc0dc6b577ab4de550283119bf92b5f1740fa8fe507dcfb085987e41a9a5aef`
> is S3 VersionId `sKIOGsHVNnFBlPcDX6jI5vLNoEmoD6AG`; self-tested binary
> sha256:`8bd1e462edc860939070b1c55aea47b02f64421188777bcc7532d043d9857602`
> is VersionId `5Gq_NNldsnFS74SNbgT496sXR.KwB1Ky`. A fail-closed migration
> helper quiesces only the named v16 partition, copies closure/frontier/keys to
> `.stage` files on instance-local NVMe, hashes the gp3 source and NVMe copy in
> parallel, installs only matching files, and leaves the original checkpoint
> untouched. Large sequential node/reverse-spool outputs remain on durable gp3
> through explicit symlinks, so the scarce local device is reserved for the
> random-access key plane. On square B1, the authenticated local copy resumed
> all 11,462,218,346 retained states: the v17 index completed, all 28 closure
> workers then ran at about 85% CPU each, and local NVMe was only 7--10% busy,
> replacing the prior four-busy-vCPU/40,000-IOPS stall without recomputation.
> Square C1 received the same authenticated split layout at 5,097,463,688
> retained states. Its restart index completed and all 24 workers ran at about
> 97% CPU; together with the disjoint eight-worker Prince/Ghost solve, the next
> complete supervisor interval measured i08 at 31.506/32 busy vCPUs. Square A1
> then committed ply 18 at 15,624,220,303 states and correctly stopped before
> exceeding its 16-billion allocation. Its 1,801,715,175-state frontier is
> contracting, so only that partition was raised to a bounded 18-billion gate.
> The 15.624-billion-state checkpoint was hash-authenticated onto local NVMe,
> its original gp3 closure/frontier/keys remain intact, and v17 resumed on the
> same non-overlapping 22-CPU allocation. The v17 cgroup keeps the reviewed
> 215-GiB hard cap but no longer triggers reclaim three GiB early at the former
> soft gate.
>
> V18 addresses the remaining square-B1 retained-key cache pressure. It keeps
> v17's exact four-bit fingerprint filter and authoritative full-key equality,
> but raises the disposable anonymous hash table from 80% to 90% occupancy.
> The denser table saves about 10 GiB at a 16-billion-state gate for the hot
> retained-key cache; it changes neither a proof key nor a checkpoint byte.
> Native reverse-spool and checkpoint-migration self-tests pass. Immutable
> source bundle
> sha256:`ab8f3616b1687be61402f6ebb8cabb6dd0175da9e59e894f9f35b5231f6544c8`
> is S3 VersionId `rRA2WfMgxGhDwx8jVk3cB9gZBVh1gXgg`; self-tested binary
> sha256:`6bafa0eba96cde82ee3469998fc9e36f11907d983a79a806ab4bd240a23c7dd2`
> is VersionId `y84eqN5fRo38hu9yOvAuBPm3ZqpBhhFn`. All three partitions
> authenticated those exact versions and resumed the retained v15 checkpoints
> on their unchanged, non-overlapping CPU sets. B1 and C1 rebuilt their exact
> 11,462,218,346- and 5,097,463,688-state indexes at 183 and 124 GB resident;
> A1 continued rebuilding its 15,624,220,303-state index under the unchanged
> 215-GiB hard cap. No original gp3 checkpoint was modified or removed.
>
> The next sustained closure phase showed that v18 still sized its anonymous
> hash cache from the much larger fail-closed proof-state limit: B1 and C1
> reached the 215-GiB cgroup cap, incurred 45--48% full memory pressure, and
> spent worker time in file-page waits. A1 also proved that its next complete
> layer exceeds 18 billion states; it failed before committing a partial layer,
> leaving the 15,624,220,303-state checkpoint intact, and was safely raised to
> a bounded 22-billion proof gate. V19 fixes the cache/proof coupling: a new
> `--devil-spawned-hash-capacity` independently bounds only the disposable
> restart index. Exceeding it fails closed without changing the durable
> checkpoint, while the separate proof-state limit remains authoritative.
> Current reviewed hash envelopes are A1/B1/C1 = 20/15/8 billion versus proof
> gates 22/16/16 billion. Immutable source bundle
> sha256:`947e5cc0bae36c3bbd9a0f33a57bad18b5f6543e4fd4e7f63fbd2a6da0016018`
> is S3 VersionId `yEJqVf9r66Z1ZjxrS0TJ5cNApKFDj9Rq`; self-tested binary
> sha256:`cd98aaa27d8879ce0ddf9b6ee9068e68ad493d3f3649ca0ae61eccff0b82b3a3`
> is VersionId `VMo8OwaPFFRINRjljNXIjqLI0J1.J9wy`. All three exact-version
> units are active on their prior non-overlapping CPU sets and are rebuilding
> only the disposable index from the retained checkpoint keys.
> The first complete v19 interval at 10:14 UTC measured
> i024/i03/i08/i098/i0b at 30.562, 20.957, 30.854, 22.593, and 20.554 busy
> vCPUs: **125.520/160, or 78.5% fleet CPU**, with complete measurements and
> no CPU-set overlaps. C1 used 22.884/24 Devil workers while its memory
> reservation fell by roughly 43 GiB relative to the v18 capped phase.
> A later steady expansion interval exposed a second bottleneck: the retained
> seven-byte key mapping was still using default readahead despite sparse
> fingerprint-qualified equality probes, saturating the key NVMe devices while
> workers slept in file-page waits. V20 now switches the VM and Linux file
> advice from sequential during exact-index rebuild to random during closure
> expansion without changing any proof key, checkpoint, hash envelope, or
> certificate format. Immutable v20 source
> sha256:`ae5169e3293dad81853c36287ed08c21e12805ecb1b6fcb0d66fd00674dcc60b`
> is S3 VersionId `qcU70r2ldOxWFmBPHTiEhIFF7xZX.L54`; the self-tested,
> exact-restore binary
> sha256:`77eb2637a04e797b052527dd9308f8983a9cc180d80ae143bd306ae3a96f658c`
> is VersionId `_WM.KLlc_HW9GAEVRFN_7h9ye2yoZAwY`. All three Bishop+Devil
> partitions resumed from their retained complete-layer checkpoints on the
> same disjoint CPU sets and 215-GiB hard caps. The first complete rollout
> interval measured i024/i03/i08/i098/i0b at 30.965, 13.983, 28.520, 17.390,
> and 20.854 busy vCPUs: **111.712/160, or 69.8% fleet CPU**, up from 81.033/160
> (50.6%) immediately before the fix. B1 and C1 individually reached 27.055/28
> and 20.525/24 Devil-worker equivalents during phase-advised index rebuild;
> A1's larger 15.6-billion-key index remains the principal Devil bottleneck.
> The following steady expansion interval showed that 4-bit fingerprint
> matches still generated enough 4-KiB proof-key faults to saturate each local
> NVMe device: fleet CPU fell back to 78.063/160 (48.8%). V21 packs a 6-bit
> exact fingerprint, rejecting 63/64 rather than 15/16 unrelated hash
> candidates. Its extra 0.25 byte per hash slot is bounded by a reviewed
> 224-GiB Devil-only cgroup cap; measured host headroom exceeded 73 GiB before
> rollout, and durable checkpoint/proof formats are unchanged. Immutable v21
> source sha256:`dfe20d51bc1a8a3caf4d4bb74eb79119ff4731e2a084e34d097bf3b4826031d3`
> is S3 VersionId `UewGSNkdUreKcD84onUvq.QiB_GOa91n`; self-tested binary
> sha256:`2a4c0b9330f368a54f19e4deb8a7de3e305c3f352268036d4101efee01f6ba4f`
> is VersionId `U3OdWUzXzb5XDamAVmMcNSaSyy6c742L`. All three partitions
> authenticated their retained checkpoints and resumed on unchanged CPU sets.
> The first complete v21 rebuild interval measured i024/i03/i08/i098/i0b at
> 30.604, 13.062, 28.818, 22.190, and 20.825 busy vCPUs: **115.499/160,
> or 72.2% fleet CPU**. The six-bit filter's steady expansion effect remains
> fail-closed pending completion of these disposable index rebuilds. Those
> rebuilds then proved the old B1 and C1 hash gates exhausted: B1 stopped
> before mutation at its retained 11,462,218,346-state layer and C1 stopped at
> its retained 5,097,463,688-state layer. Their reviewed restart/hash envelopes
> are now 20/20 and 18/18 billion states respectively; the durable checkpoints
> were not rewritten and the same immutable v21 solver resumes them. The first
> complete repaired interval measured i024/i03/i08/i098/i0b at 30.126, 11.022,
> 28.699, 9.513, and 20.687 busy vCPUs: **100.047/160, or 62.5% fleet CPU**.
> B1 and C1 were again in parallel exact-index rebuilds; no retained layer was
> treated as complete merely because its replacement unit launched. Their
> following expansion profiles still saturated local NVMe at 97--99% while
> workers slept in proof-key page faults. V22 therefore uses a byte-aligned
> eight-bit exact fingerprint: it removes the packed cross-word fingerprint
> load and admits only 1/256 rather than 1/64 unrelated proof-key reads. The
> additional two bits per slot fit the unchanged reviewed 224-GiB cap. B1 and
> C1 resumed from the same untouched committed layers; A1 remains on v21 so
> its already-started retained-edge pass is not discarded. Immutable v22
> source sha256:`9397ee8c61ee304537501f41de0dc5ea9c8f865ceefee629467189d3fa8cee66`
> is VersionId `3cCJGxMgPPAFSBoDkxbVQfMthZnLAirD`; restore-authenticated,
> self-tested binary sha256:`6395376493f785f0a1b1bf7b7c0d6d1db2fd91432533f8466f35ba22c3f861c6`
> is VersionId `fzyzRvFd..D03LOsKap4UT4EYpuB0ZK2`. The first complete v22
> interval measured i024/i03/i08/i098/i0b at 10.744, 9.965, 29.628, 22.886,
> and 20.538 busy vCPUs: **93.761/160, or 58.6% fleet CPU**. C1 reached
> 21.648/24 workers beside the eight-core Prince/Ghost solve; B1's first
> post-rebuild interval remained true-duplicate/key-page bound, so v22 is a
> material C1 improvement but not the final B1 lookup fix. V23 batches 4,096
> frontier parents per worker, hash-sorts their nonterminal children, and
> removes exact duplicates before touching the shared disk-backed index. This
> preserves the exact closure and changes only disposable dense insertion
> order while coalescing repeated true-key page reads. Immutable v23 source
> sha256:`f28762c32e7cec9f5f1915bbb6c38829a0165a9e853e49b4e12ef1211642a29c`
> is VersionId `0k.FM0BUk_BHC1_Ct_qeWVLOCTXWtax5`; restore-authenticated,
> self-tested binary sha256:`3cb5f69ccf1e541eb2d6a93140d01e1bdc01dacc712e68989c39b53359337a75`
> is VersionId `Im_h0xxncZxKbYYuACz0wuiPEguFqngE`. B1 and C1 again resumed
> their unchanged committed layers under the same CPU, memory, and disk gates.
> The first complete post-rollout interval measured i024/i03/i08/i098/i0b at
> 9.512, 9.971, 29.210, 10.604, and 20.775 busy vCPUs: **80.072/160, or
> 50.0% fleet CPU**. C1 used 21.200/24 workers; B1 was still performing its
> one-time serial 33.9-GB retained-frontier restore, so this interval does not
> measure the new batched closure loop and no performance conclusion is drawn
> from service launch or restore alone.
>
> Queen/Ghost now uses 28 parallel Bellman workers, Ninja/Ghost uses 16, and
> the old single-Devil reverse-edge build retains a disjoint nine-CPU set while
> it serially writes its already-complete closure. A complete exact supervisor
> interval after warm-up measured i024/i03/i08/i098/i0b at 30.281, 30.929,
> 31.632, 22.815, and 19.903 busy vCPUs: **135.560/160, or 84.7% fleet CPU**,
> with complete measurements and no CPU-set overlaps. A later interval that
> crossed a service restart was intentionally marked incomplete and is not
> substituted for that complete sample.
> The first complete steady-state v18 interval at 08:56 UTC measured
> i024/i03/i08/i098/i0b at 30.408, 24.943, 31.462, 19.360, and 20.440 busy
> vCPUs: **126.613/160, or 79.1% fleet CPU**, again with complete measurements
> and no CPU-set overlaps. The immediately preceding 73.9% interval crossed
> the v18 index-to-closure phase boundary and is retained as an honest sample,
> not treated as the steady state. A fresh >1-KiB reachability-v3 inventory was
> empty on all five hosts, so no tablebase was recomputed merely to inflate CPU.
> The later sustained 08:59 UTC interval improved further to 31.431, 24.994,
> 31.588, 22.746, and 20.533 busy vCPUs respectively: **131.292/160, or 82.1%
> fleet CPU**, with complete measurements and no overlaps.
>
> Same-side Sniper/Ghost was the one current failed supervisor job: its
> transient unit had a one-week `RuntimeMaxSec`, reached that limit during
> iteration 35, and was terminated with SIGTERM despite a healthy retained
> checkpoint. The v4 continuation wrapper authenticates the source, binary,
> concrete and lower-material inputs, merged-transition receipt, and exact
> current-root extents, skips fresh-only merge gates, and resumes those roots.
> Wrapper sha256:`3d8e1a0c1156db2763758201333168fd606c5ab5acc203e46f8a9663ccff31f6`
> is S3 VersionId `DcqmKeT6du9_bfzEIbzUUKIsYWYCiBDu`; the replacement unit has
> no runtime cap, is active on non-overlapping CPU 21, and the supervisor has
> promoted it from FAILED back to RUNNING. The canonical ledger remains
> **456 certified, 0 preserving, 19 computing, and 0 failed rows**; no launch
> is counted as certification.

The established exact completion scope contains 521 of the 624 ledger rows. It
excludes the original arbitrary-Minions Devil rows and every Sludge row, Angel pairings that also require
a Ghost information state, and the six
separator-asymmetric Copycat pairings
(`same:mage+copycat`, `same:penguin+copycat`,
`same:copycat+fisherman`, and their opposed counterparts). The other visible
one-Angel classes and symmetric Copycat classes are in scope. Both Angel/Angel
cells are exact closed-form draws: with no attacking non-King material, ordered
attachment/rescue history cannot change the insufficient-material outcome and
does not need an indexed codec. Deferred rows record the remaining exclusions
explicitly and are not counted as unfinished tablebases.

> **Devil/Minion spawned-only closure (2026-08-21):** the new Devil campaign
> deliberately assumes that no Minion is present unless it was spawned by the
> indexed Devil in that tablebase. A starting position may therefore contain
> Minions when their placements are causally reachable from earlier spawns by
> that Devil; arbitrary setup or externally created Minions are not admitted.
> The Devil itself is sufficient
> material because it can spawn Minions; the earlier app/engine shortcut that
> adjudicated King+Devil versus King as an immediate draw was incorrect and is
> not used by this campaign. Because a Devil cannot move, indexed starting
> positions restrict it to its first three ranks. Within that boundary the
> native radius-two spawn action, cooldown three, automatic forward movement,
> collisions, captures, and far-edge removal are modeled exactly. The earlier
> all-ranks Queen+Devil census and its depth-three continuation are superseded;
> the latter was stopped as soon as the corrected boundary was supplied. Devil
> cooldown-aware v5 source bundle
> sha256:`9429b8bbd1001acf49f26b6085d709796ab2388b320ed9ae46947479858634c9`
> VersionId `5Xxzz52cxOKFol0Hiok8UvYyOEIjyD2l` and compiled binary
> sha256:`1965bc7e828672b9dcc1d9247546271d67e735bd48ab8018cd9498b917325db6`
> VersionId `Z8qMKzKA6vMZyMYFzDY0pexmzdwV0vpW` bind the corrected semantics and
> all four cooldown substates. Version 5 replaces variable-length owning state
> strings with an exact 16-byte key containing the complete 80-square Minion
> bitboard and all King, Devil, cooldown, secondary-piece, and side-to-move
> state. It therefore does not depend on the sizing run's observed two-Minion
> maximum. A deterministic one-million-state local census reproduced the v4
> counts and reduced elapsed time by about 29%. The authenticated distributed
> sizing campaign uses 992 deterministic shards, depth 12, and a 50,000,000-
> state per-shard ceiling with measured deep-frontier memory gates. All 992
> records completed with distinct shard identifiers and zero duplicates. Every
> record reached the ceiling at depth 9, totaling 99,200,000,000 visited states
> and 15,976,602,847 boundary transitions, with a maximum of three spawned
> Minions. The v6 source bundle
> sha256:`eeb0ef6c2c9fc1f7e89724012e3b57f5c8c7de653034e3cf0d4cb6f93f344e83`
> VersionId `Fcog2hi.OHbq9CAD0yAe_iMhdxHs4e8m` replaces the hash-table frontier
> with a flat exact set and 32-bit frontier slots. Its binary
> sha256:`90e33f98201f43a2ba1f78d836395e7cb22d11ca72264dc385099c85bb5052e7`
> VersionId `5WDv1T8jpmTvSwr0Jsq8Xf50_zDHaUIc` completed all 992
> 200,000,000-state seed partitions at about 4.57 GiB peak RSS per worker.
> Every worker independently followed its successors, however, so the reached
> closures overlap and are not disjoint graph shards. Every partition again
> reached its ceiling and none retained complete edges or ran a W/L/D fixed
> point. These runs are sizing evidence only, not reusable tablebase work.
>
> The replacement certifying pipeline was then running. It partitions the graph
> by the Devil's immutable canonical starting square (files a-d, ranks 1-3),
> retaining that square as latent provenance after a Devil capture. The twelve
> partitions are disjoint under every transition. Each uses an exact concurrent
> 16-byte-key index, finishes its complete uncapped closure, replays every edge
> into retained predecessor storage, solves W/L/D and DTW by retrograde, and
> exhaustively Bellman-verifies every state before emitting its root fragment.
> Cooldown three permits a spawn only every second White turn, while a spawned
> Minion survives at most ten White advances; therefore at most five spawned
> Minions coexist. The codec enforces that proved bound and continues to model
> surviving Minions after the Devil is captured. On 2026-08-23 the authenticated
> square-a1 run completed its exact 305,073,281-state, 1,960,550,125-edge
> solve and authenticated root fragment; this is certifying
> tablebase work, so `single:devil` was **COMPUTING** at that time. It is now
> **CERTIFIED** by the final campaign record and ledger row below.
>
> The completed a1 v1 source is sha256:`c215555de92a12715a4778af71196790e01dcbc7455a85ff44f5b6961b6c6db0`
> VersionId `ZxGnjvhJ.6IfHEIUP0h_kJ47MiMBUGIv`; its restored binary is
> sha256:`2298603bdc59b7f3ddff73b9fbd2a053370b99fbc24372bba9d91b7516c332c3`
> VersionId `xtallXZUBr52vxDIAtZ87K_K.9Be7dfb`. Profiling also removed a
> layer-checkpoint stall for subsequent partitions: v2 synchronizes only the
> committed dense-key prefix and reconstructs its disposable hash index after
> a restart. The a1 partition then completed its 305,073,281-state,
> 1,960,550,125-edge solve, exhaustive replay, and 98,592-root fragment at
> 06:13 UTC. Fragment
> sha256:`b5fb00cb344575bbc7ebf34bb866ebee6000939eab71558ad1221f3b37d0cf27`
> is S3 VersionId `_W7NtJCenLHcfAaPAlpG1KZ5jIPGtq60` and passed an exact-version
> restore comparison. Its eight disposable graph scratch files were then
> removed while the local root, versioned root, and proof log were retained,
> restoring overflow free space to 541 GiB. The b1 v2 run committed
> ply 18 at 972,499,341 states and then failed closed at its one-billion initial
> allocation; no partial layer was admitted. Growable-checkpoint v3 reopened
> the same dense prefix with a two-billion capacity and completed the exact
> 1,270,009,248-state, 8,125,179,009-edge solve and its 98,592-root fragment.
> Fragment sha256:`12eb3e9ba73b134c538399fa251ed708366598b2d7b93637bf506b7dcc2d6232`
> is S3 VersionId `rCoZIOpJVZV1hMJNoMm5mepk4IrFBgHA` and passed an
> exact-version restore comparison. Its disposable graph scratch was removed
> only after that comparison; the local root and proof log remain retained.
> The superseded v2 deterministic source is
> sha256:`6b1b2892ef4e6b521d432b2d7af0391a215aa860ee91888819649f5319ccd59b`
> VersionId `0vMmukbRHDvEZDqWHMBxu7GXdkK0UFYx`, and its exact-version-restored
> x86-64 binary is
> sha256:`1710d241ea7ba3177ac2be8f0cfde8fd11195b5952a0712c6f976886a2b282e1`
> VersionId `9yzbOJnfKNK7AdsV9d7uLlez6eMEioob`. Current v3 source
> sha256:`c897fbe45ea46f9277943f436cf0a97422fffaafa2f80a2e84b5f13f61430d4a`
> is VersionId `1SU26aPTGw_cB.zGJ_dgiZAj2SQW5ctO`; its exact-version-restored
> binary sha256:`28098024b5330d370a20f0c4c00a957f14fe4201ac8b2321fbd90cda24148bf5`
> is VersionId `R7aTmMBo32FlIVdU_k49pVRoWEu7yONp`. Full-uint32 v4 is staged
> for the wider remaining squares: it separates the predecessor side flag from
> the 32-bit dense index (five bytes per edge, rather than an eight-byte packed
> edge) and raises the safe allocation beyond two billion states. Source
> sha256:`bc59229d2788031778661f939a735d9e6ac358747c0ec4e75d59307a6dd33c3f`
> is VersionId `p.M.9kCjhRcHAnZjaM7CWqEzLD0CWBvX`; exact-version-restored
> binary sha256:`a013541e5ae252341503a08ea8496bcf25abb043791829495fe08f3973ef51d0`
> is VersionId `fRRiwxv8KnCDjpQgqTtDJysCrs.qlgd9`. The c1 partition committed
> ply 18 with 2,855,634,175 states and a 512,747,335-state frontier. It then
> failed closed at v4's arbitrary three-billion allocation without admitting a
> partial layer. A path-unit defect repeatedly relaunched that expected failure;
> the loop was disabled and the committed checkpoint was preserved. V5 made
> exact resume-index reconstruction parallel, while authenticated v6 additionally
> moves the disposable 32-GiB hash-slot map to anonymous memory so it cannot
> throttle on filesystem writeback. V6 source
> sha256:`b3a358fed09b92d90f43aa23616f3f81eec9589747c70b217681771fc196ed00`
> is VersionId `2DWETScmIcUD8S24KxUWZ3vH.02U5SzZ`; its strict-build,
> self-tested, exact-version-restored binary
> sha256:`16f1f86d3ef70fdae3c229bb9102f274a5631999ec694c288ae5bcd682e5cd0f`
> is VersionId `4x7cBA5XTjJ3vQd1RVQwEo3l5KgxYe2R`. D1 completed and
> exhaustively verified 3,873,307,130 states, 24,345,340,979 edges, and its
> 98,592-root fragment; sha256:`c1299e51b7d5d11be2f6f9eb7201e30c187eb3d314ded94ea9f6c8df0451c9a6`
> is S3 VersionId `bAok4KgyRlwVF0LvHd9lekMT6JkF.Idv` and passed an exact-version
> restore comparison. A2 then completed and verified 364,746,583 states,
> 2,346,760,900 edges, and another 98,592-root fragment;
> sha256:`bf6c6a16476336bd711cd11dcd49ff82ad2fde5c2fd61d7bea3f03abcf890344`
> is VersionId `JT1p83evKMhUf1INYbNGlz0nbn6p4ODN` and also passed exact restore.
> B2, C2, A3, B3, and D2 are now complete. C2 exhaustively verified
> 5,176,987,699 states and 33,045,156,534 edges; its root fragment
> sha256:`87fd0b18664b90d6f7354c0ada9f3c1d14a3d3e23c155c58da401b681c7b050d`
> is S3 VersionId `7bDNlSPFIs7f6LdwZBKM41.PZHj8_ENb`. A3 exhaustively
> verified 361,229,977 states and 2,344,674,058 edges; its root fragment
> sha256:`3b3d90ef2f9c932f5b6da3383256997af4cfe9a5e627b77468fbd3b58594b2b8`
> is VersionId `poO5Tm4gzOuVbNePcFVCTbXcNYkyuJBO`. B3 verified
> 1,710,246,028 states and 11,084,029,559 edges; fragment
> sha256:`b34ff12132cbbb8879b9bb8737bf3f799b0134246099ad82c46887c1a2380340`
> is VersionId `sqKiKtkFo8mOX8UoErVdoWYPH4szr3GW`. D2 verified
> 5,194,192,187 states and 32,687,418,336 edges; fragment
> sha256:`22fb6d5df071ffb0c33d8eff717d71e14fe94cc73017965371dd3e4cfb37c8eb`
> is VersionId `23tYo5y_FWcdxPUYLEbj6cenJ63zvx8x`. All four named v7
> fragments passed exact-version restore comparison. C3 and D3 subsequently
> completed, were version-preserved, and passed exact-version restore comparison;
> only C1 continues from its retained checkpoint under the wide-index v7 solver.
> Future partition launchers use authenticated
> builds and do not use a
> failure-retriggering path unit. All 48 same-team and opposed companion Devil rows are
> **PLANNED** under the same documented simplification: indexed Devils start on
> ranks 1-3, and both the starting frontier and sparse closure admit only
> Minions causally placed by an indexed Devil, including Minions already present
> at the start when earlier Devil spawns can account for them. Arbitrary starting
> Minions remain excluded. Their position totals are exact
> indexed starting-frontier counts, not dense spawned-closure size claims.
> A frontier census alone is never presented as a tablebase result, and no W/L/D
> cell will be published before the exact fixed point and verification pass.

![Ultimate tablebase computation and outcome grid](ultimate-tablebase-grid.png)

Diagonal hatching means the class is currently computing. The image is
generated exclusively from the canonical ledger below by
`python3 tools/tablebases/plot_ultimate_tablebases.py`. Exact Berserker-radius,
Giant-start-class, lone-Devil current-Minion, and Checker root-type rows
additionally consume their authenticated compact slice summaries; the renderer
never infers those rows from filenames.

> **Ghost rules invalidation (2026-08-13):** every result containing a Ghost
> from an earlier model generation is superseded. Old concrete tables,
> information sidecars, transition/normalized-parent graphs, certificates,
> checkpoints, and retained/rebound graphs must not be probed or resumed.
> Each in-scope Ghost class below is rebuilt from an empty work directory using
> the corrected blind-capture/collision and public-observation rules. Historical
> S3 versions remain only as invalidated provenance.
>
> A move begun by a visible Ghost publicly reveals its destination even if the
> Ghost fades there. Parasite/Ghost possession is a separate, permanent-tracking
> lower domain in both capture directions; it is authenticated against the
> tracked-Ghost concrete oracle and never approximated by an ordinary
> visible-singleton Ghost information probe.
>
> **Persistent-substate Ghost invalidation (2026-08-14):** the generic
> same-class child encoder omitted the companion's tablebase substate when it
> selected the canonical child geometry. This is why the opposing Prince/Ghost
> solve failed closed: a public Prince move entered the second-move continuation
> (`substate=1`), while its edge was incorrectly attached to the otherwise
> identical `substate=0` geometry. The encoder now carries the exact child
> substate, and an exhaustive pre-build regression requires a real nonzero-child
> witness (`witness_geometry 0`, `child_substate 1` for Prince). Every current
> transition graph, normalized source, overlay, checkpoint, or proof for Ghost
> with Pawn, Berserker, Sniper, Prince, Checker, or Penguin is therefore
> invalidated and cannot be resumed, even if an older per-row note below says
> that its transition or solve had completed. The two Berserker transition
> units, two Sniper merge units, and same-side Prince solve were stopped; no
> affected unit remains active. A clean same-side Prince rebuild is active as
> `ultimatefish-info-substate-v1-kghostprincek-transitions-v3` with 24-way shard
> fan-out after its self-test, under model
> sha256:8924969e0734de9fc9246745191fe09c40e79753e7467116832e9178c261c0ae.
> Clean opposing Prince (24 CPUs on i08), opposing Checker (20 CPUs on i024),
> same-side Penguin (18-CPU quota on i098), and opposing Sniper (14 CPUs on
> high-memory i03) rebuilds have also started from empty work trees after
> independently proving a nonzero child substate. Their superseded per-row
> transition/solve notes remain historical only until each new graph is
> exactly verified.
> The attempted opposing Berserker/Ghost rebuild stopped at its mandatory
> source-normalization gate before producing any shard: the bound concrete
> source is a valid v6 table whose WDL plane begins at byte 56, but the staged
> information reader still used the fixed v5 byte-48 offset and interpreted
> the v6 `exactEdges` header field as WDL. Both concrete copies match
> sha256:d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55;
> the version-aware reader is being exact-self-tested before any clean
> transition retry. The failed attempt produced no shard. The independent
> same-Berserker concrete solve subsequently certified successfully; its exact
> output, proof bindings, and pinned S3 VersionIds are recorded in the
> `same:berserker+berserker` row below.
>
> **Singleton-domain correction (2026-08-17):** the Prince witness run reduced
> the remaining singleton failure to 6,191,376 dense-table entries.  Every one
> of its first 20 exact witnesses had adjacent real Kings (the first was White
> King a1, Black King b1, Prince d1, visible Ghost c1).  Such placements cannot
> occur at a legal turn boundary, and an extra-piece continuation cannot create
> one because neither real King moves during the continuation.  Their packed
> concrete WDL bits are therefore padding, but the old certification loop
> treated them as oracle states.  Certification now excludes only this proved
> unreachable padding domain, reports the excluded and checked counts, and
> continues to compare every admissible singleton exhaustively.  The same
> signature was audited across the open same-side Prince, Dragon, opposing
> Copycat, opposing Knight, opposing Turtle, in-flight opposing Prince, and
> in-flight same-side Checker classes.  All seven now rebuild fresh
> transition graphs under class-specific corrected model hashes; fail-closed
> promoters require a successful service result and exact
> `transitions-ready.json` binding before starting a 1-billion-node
> `--solve-existing`.  The deployed source archive is
> sha256:c01f874b0ad5420b3f11ff3592399bb1f5558a01df563448d4d59b0293ca8657,
> S3 VersionId `CyM9nmyoZWXGGjjUl1rnm4UJfs8omQjD`.  At the 2026-08-18 03:32 UTC
> supervision snapshot, opposed Knight, opposed Turtle, opposed Copycat, and
> same Dragon had each completed the fresh transition service successfully and
> written an exact `transitions-ready.json`; their class rows below record the
> solve state and marker binding.  Same Prince, opposed Prince, and same Checker
> remain active fresh transition rebuilds with fail-closed promoters.  The
> exact-PID same-Berserker/Ghost merge remains a live 99.7%-CPU PID 519977 child
> of wrapper PID 428088 in the same systemd cgroup, with no readiness marker
> yet.  Failed graphs, witnesses, startup logs, and stopped partial restores
> remain preserved, and none is eligible for certification.
>
> **Current corrected-substate status (2026-08-17):** the same two-Berserker
> concrete table is now certified and version-pinned in S3. Both corrected
> Penguin/Ghost transition graphs passed exact authenticated reload against the
> privacy-corrected observation model and are in Bellman solving on i0b. Fresh
> v4 Pawn/Ghost graphs have all 64 verified shards in each orientation; the
> same-side graph is solving on i098, while the opposing solve remains gated on
> its fresh Queen/Ghost dependency. The opposing Berserker/Ghost build's ten
> verified shards were migrated with a full-hash inventory and are resuming on
> i0b under the corrected reader and model. These jobs use only
> post-invalidation graphs; none resumes a pre-correction Ghost graph or
> checkpoint.

> **Opposing extra-primary source-remap invalidation (2026-08-19):** the
> Knight/Ghost fixed point converged after 40 iterations but then failed its
> admissible singleton proof on 20,460,072 of 69,477,408 checked worlds.  The
> first witness (White King a1, Black King c1, White Knight b1, Black Ghost
> d1, White to move) exposed a source-normalization error rather than a hidden
> information effect: opposing extra-primary tables already store the public
> extra as White and the Ghost as Black, but the normalizer swapped side to
> move and the two King identities as if the source were Ghost-primary.  It
> did not swap extra/Ghost ownership, so its normalized WDL plane described a
> different physical position.  The remap now color-swaps only Ghost-primary
> opposing sources, with an exhaustive physical-identity regression for every
> extra-primary source state.  This invalidates every prior opposing ordinary
> extra-primary information attempt for Knight, Ninja, Queen, Rook, Turtle,
> Pawn, Berserker, and Copycat; same-orientation runs and Ghost-primary
> opposing runs are unaffected.  The active old Turtle solve, Berserker
> transition resume, and Copycat witness diagnostic were stopped without
> deleting scratch.  Corrected source
> sha256:ca679a7817bf03160f9fe9c53335aea6e82b5b653a0501b850b5168d45705900
> is S3 VersionId `o_wh_k7TO6mC4hqSZr7Hf4L5lzzNW2LA`.  Fresh Knight
> transition unit `ultimatefish-info-remap-fix-v1-kknightkghost-transitions-v9`
> completed all 64 shards and its 492,960-geometry merge from an empty work
> tree under staged model
> sha256:be268fc43e27fc69c39af5e4e98dc6a9cb40e2fbbb491af8b9e3bd7435659049;
> its wrapper sha256:f02e7a99c078f7e582201384469f179461280fbf7943904e167f74b822766948
> is S3 VersionId `Py81KQZg5aLtgQridWqw6YzNhOJLDqGH`.  The fresh marker
> sha256:32c81f2b49a53efdc804260e852713b467fe4f6f4b2e0407d33f811b673819b2
> exactly binds the Knight material, opposing orientation, corrected source,
> and corrected model.  Solve-existing unit
> `ultimatefish-info-remap-fix-v1-kknightkghost-solve-v11` is active against
> that graph with observation
> sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23;
> its wrapper sha256:82ed8ccdc547af180462c8ad613c254f8ae93ace82030f8bccfd9d80c55401d4
> is S3 VersionId `2yeaJh1hGAzByJ6gsKHvbWsyeo162vpB`.  The predecessor
> v10 launcher failed closed on unsupported optional runner arguments before
> solve scratch was created and remains preserved as S3 VersionId
> `Z7aUrA.hN1fpaZ9Fi1N1DGEBbUM7xeUc`.  None of the
> invalidated graphs, normalized sources, solve scratch, or outputs will be
> reused for certification.
>
> **Ghost-primary stateful source-plane correction (2026-08-19):** same-side
> Penguin/Ghost's converged fixed point disproved the prior padding diagnosis:
> 41,914,906 singleton mismatches included visible worlds.  The concrete
> generator packs a primary piece's substate before the secondary piece's, so
> Ghost-primary Sniper, Prince, Checker, and Penguin source tables physically
> use `[Ghost visibility][extra substate]`.  The information codec always uses
> `[extra substate][Ghost visibility]`; its source reader incorrectly treated
> those indices as identical and transposed every stateful WDL plane.  The
> reader now applies an exhaustive bijective logical-to-physical transpose.
> This affects the normalized concrete oracle, singleton certificate, and
> published W/L/D report only: transition compilation and the symbolic fixed
> point are generated directly from legal moves and observations, so all exact
> shards, strong merged graphs, and in-flight fixed-point iterations for these
> eight same/opposed classes remain valid and must not be rebuilt.  Their
> current old-binary solves continue to convergence as authenticated
> checkpoints, then use the explicit persisted-ROBDD resume path with a
> corrected normalized source.  Exact-self-tested native restart binaries are
> already pinned for Sniper (sha256:61dc6b4895db98961e88693c0f7c8b4030bee47f9d96c7bbbe3da07b864afe0b,
> VersionId `PgZkjujybQ0dotqJXfpElkmPn9gkoeFP`), Prince
> (sha256:87e359ff191fc2c187a599b1d3431596e83fab7940122af07629c1fb6f305070,
> VersionId `92SwOXklHO3rDr1NfNAYJZFKYgxuiM9R`), and Checker
> (sha256:cd3ca096dc8a9609e63c1eb6dd292943822aa618b3d435092a893bb5108c0ed3,
> VersionId `Kq8x7lEBmqebTAFhUN7Em6tyJ1KIN26J`).  The common source bundle is
> sha256:a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064,
> VersionId `WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3`.  Same Penguin/Ghost is the
> first live corrected resume and its row records the exact bindings below.
>
> The independent crossed Jester/Ghost graph reached 19,300,000 expanded
> roots and a durable 34,207,599,166-byte checkpoint before its 48-GiB
> resident gate failed closed at 52,150,984,704 bytes.  Because i024 has only
> 64 GiB physical RAM, the checkpoint is being SHA-256 hashed and copied to
> the authorized versioned bucket for authenticated migration to high-memory
> i0b; the original checkpoint and five-day log remain intact.  The upload
> completed with checkpoint sha256:84aa2c2b4d8416b8b8d109a8e270a9dc15c34b627971be7e74b9efc7789663a3,
> S3 VersionId `GCYyV19fVUJpL8RyVODsmXydHYdPbOOc`, and executable
> VersionId `Xn2BNCXhPvTKSgyFycR2Urnv6w2Kk5ji`.  Authenticated restore unit
> `ultimatefish-info-crossed-jester-ghost-migrated-v2` is active on i0b CPU
> 29 with 210/230-GiB cgroup gates and will resume only after exact local
> size/SHA verification.  Migration
> wrapper sha256:b2e632fa1210c31526c80a4ed8c2e6a9fa1cf26d49b727e89e11075fdd383b04
> is S3 VersionId `UOxQUik1dy9i4VV.WIiPXT1bJ6PDiuuA`.

> **Concrete reachability ledger audit (2026-08-17):** every one of the 354
> certified concrete rows was replayed against its preserved reachability
> evidence: 154 rows from the exact-version legacy receipt, 186 from individual
> content-addressed sidecars, and 14 from exact-output-bound sidecars fetched by
> their recorded S3 VersionIds. All per-side totals equal half of the indexed
> state count and the final replay residual is zero. The audit corrected 21
> published rows: 13 manual admitted/unreachable inversions (including opposed
> Berserker/Ninja and Berserker/Sniper), six stale Penguin-substate rows, and two
> stale legacy-receipt rows. The Dragon/Penguin row's columns were also restored
> to its authenticated file/header owner order within those six. One additional
> manual row was already numerically correct and received an exact output
> binding. The underlying `.uftb` WDL/DTW payloads and Bellman proofs were valid
> and unchanged; the defect was confined to the publication ledger and its
> generated plot inputs. The grid catalog now also uses the ledger's canonical
> record precedence and owner-order-neutral same-team keys, restoring the
> authenticated Dragon/Penguin record and 16 same-team Copycat mappings without
> changing any plot labels or legend semantics.
>
> The exact opposed Berserker/Ninja power audit also separates the aggregate
> ten-bucket Berserker result from the initial radius-1 (`power=0`) matchup. In
> that initial slice, Ninja-to-move has 9,866,198 wins, 5,680 losses, and
> 3,847,982 draws (71.912% / 0.041% / 28.047%). With Berserker to move, the
> Ninja-side result is 6,238,816 wins, 2,499,580 losses, and 7,147,320 draws
> (39.273% / 15.735% / 44.992%). Thus the power-0 behavior closely matches the
> expected Jester comparison; the previous zero-draw publication was false,
> while the all-power aggregate is legitimately much harsher for Ninja. The
> diagnostic is sha256:faac32e8ec568feb386c027ba38da4df74c9bb0741e84b0071bfba388fbf060f,
> S3 VersionId `.nr3tzspic8TXWi5SKV31uiY0rf2eDuC`, bound to concrete output
> sha256:b436e4a2f00c76cda8854c2262f79c3ed0daf362436e5a205770c5d1f5cb7796.
>
> **Berserker radius plot audit (2026-08-21):** the three rows immediately
> below aggregate Berserker are exact initial-radius 1, 2, and 3 slices
> (`power` substates 0, 1, and 2), not relabeled aggregate values. The refreshed
> schema-2 input covers all 43 certified Berserker records: 40 concrete tables
> were scanned, two Jester information overlays were decoded directly, and only
> the exchange-folded same-team two-Berserker class is excluded because it has
> no distinguished row piece. Opposed Berserker/Berserker remains included: its
> owners are color-distinguished, and an orientation regression test now rejects
> the former over-broad exclusion. Every concrete slice carries admitted,
> authenticated per-substate trivial, and displayed W/L/D counts; displayed
> counts equal admitted minus trivial, and all ten displayed power slices sum
> exactly to the current bracket-subtracted ledger aggregate. The two Jester
> information overlays now also carry authenticated per-substate trivial
> counts, so their radius cells use the same admitted-minus-trivial semantics
> as the concrete slices rather than inheriting an aggregate classification.
>
> The audit binary is
> sha256:9cc3c5b38ac072ca39ecf2401c3e54d15427c90bdfac03f48698739cface7667.
> Its deterministic 16-source bundle is
> sha256:577ebc7424484933bc9cd57d09756dd2ad9cc2a11e1ef126b4ff2013a218f29a,
> VersionId `_QHckJ9Q9NigQgdiF2eJdssQPwJYa7LR`; the corrected exact-artifact
> manifest is
> sha256:034a90d5ec9ef9d57af0d2160ba5ace1ae1c785bc1b6c873bf625f0d05b3effe,
> VersionId `T9tXNtGTs.c9evrgI7oORdayAD1Shizk`; and the worker-configurable final
> runner is
> sha256:6edec0d5380167b86d95032e9e25819dbd0754f6092fb15a7d9bf967d83def81,
> VersionId `4ytHzLKrIb49dFYm2HuMOCoLvKFJfNBo`. The complete 43-record,
> 126-radius-cell plot input is
> sha256:c241542ac1da43a45139193a970ded50e2b20911bf27625cab6c378304910b56,
> VersionId `gBzvIV77tProYXmQ0l_HCvYn76Ni7GIz`. All 40 concrete logs are preserved
> in deterministic node evidence archives: i08 has 6 logs at
> sha256:4e1198d9ae3f43614d865241d56e51559034dfa2b36541af0e18c5b81fab1f6b
> VersionId `Wr7bQS3BinFODGnR.OX6pTgsHWFGa.SN`; i098 has 18 logs at
> sha256:3301aad34779dea185d3fd697f61af751af34ed3b767216254a7e4090fdcbfb7
> VersionId `lhU3aoAiv7McZK4.MWjbO6cBDWGoRHQU`; and i024 has 16 logs at
> sha256:8d35cc379c9785ff4b97fe11d3576515f17d70bad6d6043f835f118f76f5696a
> VersionId `BZzQ5KxW4vO.eLTBGaBHxOI2YAkbXvkI`. Publication receipt
> sha256:079011cded676096fa97e15903ae5cd27a87ae7953b3bf4f972614adf7be1540
> is VersionId `_JQrhjDC5nWBO6PXxlHo40qvYb38QC5N` and binds every shard,
> source, runner, evidence archive, and final summary version.

> **Giant start-class plot audit (2026-09-03):** the four rows immediately
> below aggregate Giant are exact root-anchor parity components on the 7×9
> lower-left-anchor grid: Giant-20 is even-file/even-rank, Giant-16 is
> even/odd, Giant-15 is odd/even, and Giant-12 is odd/odd. A Giant's own
> two-square orthogonal moves preserve this component; Fisherman pulls, Mage
> swaps, and other external effects remain fully represented by the solved
> W/L/D and may move the Giant between components after the root. The summary
> covers all 45 certified Giant material records. Forty concrete tables and
> four Jester/Ghost information overlays were downloaded from immutable public
> Hugging Face revision `ed8375c43f26e42c5b14ac79ff41e77d4317b97f`,
> SHA-256 verified, audited serially with at most two native workers, and
> deleted one material group at a time. The exchange-folded same-team
> Giant/Giant record repeats its aggregate cell because it has no distinguished
> row Giant; opposed Giant/Giant remains exactly sliced by owner. Every class
> stores admitted, authenticated trivial, and displayed W/L/D, with displayed
> equal to admitted minus trivial and the four displayed classes conserving
> the ledger aggregate for both starting sides. The 176-class compact input is
> `tablebases/giant-start-class-summary.json`,
> sha256:c68814092c4246f4dcc5a1d233e84dc1b5e520e2da0f19d55ac37e4bfbf44004;
> its audit binary is
> sha256:cb9fda10d5e5aeec3f4d5bcf7a6ec9c10ecb6f21a7c7153cd2d7e32b664725eb.

> **Devil current-Minion plot audit (2026-09-04):** the six rows immediately
> below aggregate Devil are exact lone-Devil root slices with the Devil alive
> and 0 through 5 currently surviving Minions. Each slice applies the same
> ordinary-predecessor reachability admission and immediate-stalemate/material-
> simplification subtraction as the established plot; every solved successor
> remains unrestricted, so the underlying W/L/D values still include later
> Devil spawns. The zero-Minion slice reproduces the authenticated historical
> entry-root audit exactly and is a draw for either starting side. The other
> five slices contain no Devil wins after filtering. Companion cells are
> intentionally blank until their own material classes are audited. The
> complete certified 34,981,631,519-state class was streamed serially from
> immutable public Hugging Face revision
> `ed8375c43f26e42c5b14ac79ff41e77d4317b97f` and
> SHA-256 verified without retaining tablebase payloads on disk. The six rows
> cover 33,631,448,390 alive-Devil roots; 1,350,183,129 dead-Devil continuation
> states remain in the solved class but are excluded from these root cohorts.
> Of the alive roots, 50,240 predecessor-unsafe states and 1,220,336,641
> immediate trivial states are excluded, leaving 32,411,061,509 displayed
> roots. A native verifier exhaustively checked all 591,552 zero-Minion compact
> states and deterministically sampled another 250,000 Minion states, including
> compact Minion-code round trips, before the long stream was allowed to run.
> The compact input is `tablebases/devil-minion-start-summary.json`,
> sha256:f00535c2d05b9b70f7e6c0488eebf38f3a3829c0f995e880734b4f40c0782a5f;
> its audit source bundle is
> sha256:9181f97e8209def10013b9e86fe4d123a9f4b7105f4fe75d51eb20d8c840dfc9,
> and its native classifier-verifier source bundle is
> sha256:3f2e10fc0e0e0bad623a58d9fbcc27baa74189ceac4d826af5aa3dae994b6b2b.

> **Checker root-type plot audit (2026-09-04):** the two rows immediately
> below aggregate Checker split root positions into normal Checker substates
> 0/1 and Checker King substates 2/3. Ordinary and forced-jump roots remain in
> their respective type row, while all legal successors are unrestricted, so
> promotion and continued play retain the original tablebase W/L/D. The
> summary covers all 37 certified Checker material records: 32 concrete tables
> and four Jester/Ghost information overlays were downloaded from immutable
> public Hugging Face revision
> `ed8375c43f26e42c5b14ac79ff41e77d4317b97f`, SHA-256 verified, audited
> serially with two native workers, and deleted one material group at a time.
> The exchange-folded same-team Checker/Checker record repeats its aggregate
> cell because it has no distinguished row Checker. The opposed Ghost/Checker
> overlay's historical substate transpose is accepted only for its exact
> certified source/model hash pair. Every distinguished class stores total,
> excluded, admitted, authenticated trivial, and displayed W/L/D; displayed
> equals admitted minus trivial, and the normal plus king rows conserve the
> ledger aggregate for both starting sides. The compact input is
> `tablebases/checker-start-state-summary.json`,
> sha256:238c17ed175356bc9c6c8ad1e079d2761a243b458b6c31f9f4b45cd79a404687;
> its audit binary is
> sha256:ad757c852b2efed98a8331eb291f8567bd65c414a68c8c879e030786fbdb99eb.

> **Prior AWS fleet audit (2026-08-20 17:36 PDT / 2026-08-21 00:36 UTC):** i03 was
> externally stopped with EC2 reason `User initiated` at about 21:04 UTC. Its
> 500-GB `ufpenguin` EBS checkpoint volume survived and was identity-checked
> before remount; its 1.9-TB instance-store scratch was correctly treated as
> erased and rebuilt from version-pinned inputs. Five interrupted Ghost fixed
> points (opposed Penguin, Queen, Rook, Turtle, and Knight) are again active
> from authenticated EBS checkpoints. The erased opposed Ghost/Sniper and same
> Ghost/Ghost transition graphs were rebuilt fresh across 26 i03 CPUs and 30
> i08 CPUs respectively; neither old graph is accepted as current. The
> supervisor authenticates both replacements as **RUNNING** with exact wrapper,
> runner, binary, and model-source bindings. Same Ghost/Ghost is now in the
> merged graph's required exhaustive native regeneration check on one i08 CPU.
> Same Pawn/Berserker passed the former 64-GiB false-failure point, completed
> its eight-worker frontier/reverse build, and is in its disk-backed exact
> post-propagation verifier; its retained gp3 volume was raised online from
> 3,000 IOPS / 125 MiB/s to 4,000 IOPS / 1,000 MiB/s after measured I/O proved
> to be the limiter. Opposed Ghost/Sniper completed all 64 shards and entered
> its serial merge, so its reservation was narrowed to CPU `1`; exact-source
> opposed Berserker/Penguin remains fully busy on CPUs `10-13,15-18`; it has
> completed frontier and reverse construction and entered propagation. The
> supervisor authenticates that new unit as **RUNNING**. The checked-in 00:36
> sample measured 36.685/160 busy vCPUs: 10.184 on i03, 16.506 on i0b, 3.000
> on i08, 2.997 on i098, and 3.998 on i024. The i03 sample is explicitly
> incomplete while its multi-process jobs change phase, so the fleet's reported
> 22.9% is a lower bound; the most recent complete short-interval view of the
> busy workers was 43.412/160 at 00:27. Remaining idle
> CPUs are chiefly serial Ghost Bellman/verification tails rather than missing
> parallel workers.
>
> **Prior AWS fleet audit (2026-08-23 02:42 PDT / 09:42 UTC):** all five
> authorized hosts are `r8gd.8xlarge` (32 vCPUs and 256 GiB RAM). A fresh,
> complete supervisor CPU-delta measurement found 31.838, 11.053, 28.980,
> 31.710, and 29.891 busy vCPUs on i03/i0b/i024/i08/i098 respectively:
> 133.472/160, or **83.4% fleet utilization**. I03's 29-way corrected
> same-Ghost/Ghost graph rebuild now saturates the CPUs released from obsolete
> overlapping Devil censuses. I0b's deficit is real: its wide Devil C1 resume is
> in a storage/algorithm-bound checkpoint phase while its remaining Ghost
> Bellman solves are single-core; assigned affinity is not presented as CPU
> utilization. There are no CPU-set overlaps or incomplete host probes.
> The opposed Parasite/Ghost lane on i024 CPU 2 failed closed on a solve-time
> lower-table transition residual before changing its compaction-6 checkpoint.
> A second independent merge from all 64 authenticated shards completed all
> 492,960 geometries and 441,820,932 edges with zero transition, conservation,
> relation, gap, offset, or byte-identity residual across all six outputs. The
> source-pinned v4 wrapper (sha256:b2495176829eff15fed21711acd20faa5f197888d94ead890581d904a1d1eea4,
> S3 VersionId `Ilt7h5TVvR8kzPVlFL.BKsvxLLuzxyad`) is now reopening the retained
> compaction-6 checkpoint against that clean `recovered-v2` prefix on CPU 2.
> i0b's Berserker/Ghost transition volume reached 100%
> local-NVMe utilization in its write-heavy phase, but CPU-delta and PMU
> profiling proved its current merge is serial. Its affinity was narrowed from
> stale `0-9` to CPU 8, and nine existing modulo-133 Devil streams were split by
> alternating campaigns onto the released CPUs. The split preserves each
> stream's seed/receipt ownership without duplicating a shard identifier; it
> does not make successor closures disjoint. Available RAM
> after the split remained 57 GiB on i0b and at least 106 GiB on every other
> host; swap remains disabled. The deterministic split manifest is
> sha256:d5ce62122afcaec8e750443ab109df1f92857e890e682f782d02043782ea60b1,
> S3 VersionId `JHJNX0BAGe2U49pvnhsGuaFaFA_ByG7i`. The checked-in supervisor
> now accounts for all split services and targets
> 98% rather than 90%, so an empty ready queue cannot silently ratify the old
> conservative utilization ceiling. Six completed campaign lanes later released
> CPUs 13-18; the active odd-campaign lanes were stopped, their immutable logs
> hash-bound, and each remaining campaign-residue-1 shard sequence was divided
> by ordinal parity. The v3 manifest
> sha256:cce3bad40d43c41b0d6ffd471524439ab99b8db8f6208da9dcb0a9671728d104
> (S3 VersionId `0FuyKywoNVbEcUM4SelAV_CYojSyJJng`) assigns twelve disjoint
> shard lanes to CPUs 0-2,5-7,13-18. Six already-complete parity lanes then
> released CPUs 0-2,5-7 again; seven unsplit modulo-133 workers were stopped at
> their receipt boundary and divided by campaign parity under v4 manifest
> sha256:12f003f2ee02eedc2b4e3f6bd148eeafe4199508047e29dfdc75c905eed47bd7
> (S3 VersionId `h0MRXOsoEQ_qw5MFCiDrDHANdYJVPNBq`), which assigned 31/32 CPUs
> without overlap. When earlier split tails completed, eleven active
> campaign lanes were stopped at receipt boundaries and divided by assigned-
> shard ordinal parity. The v5 manifest
> sha256:16dd64e341f7640bac5cc53c5707549b788e56b4e98f7b10eaa0d2eb5913e168
> (S3 VersionId `8VIUOGvy4OGyRt..axe6jkBzXbJsmJLB`) fills the last eleven
> CPUs with disjoint work; all 32 i0b CPUs are assigned and about 54 GiB RAM
> remains available.
>
> **Prior AWS fleet audit (2026-08-23 04:11 PDT / 11:11 UTC):** the exact
> checked-in supervisor authenticates 50 completed jobs, 22 running jobs, and
> zero current failed jobs; the canonical ledger remains 451 certified, zero
> preserving, and 23 computing rows. Its process-delta sample was incomplete
> while short Angel shards changed PID, so a complete five-second host-counter
> sample measured i024 27.025/32, i03 4.003/32, i08 7.656/32, i098 7.052/32,
> and i0b 6.343/32 busy vCPUs: **52.079/160, or 32.5% fleet CPU**. I08 and
> i0b simultaneously spent 71.4% and 66.0% of their host time in I/O wait;
> their current Devil square-3 and square-2 retained-edge phases are storage-
> bound rather than compute-starved. I03 has three single-core Ghost fixed
> points plus the inherently serial same-Ghost/Ghost transition merge; the
> completed parallel shards are preserved and the merge cannot be safely
> repartitioned in place. I024 is now 84.5% busy after both repaired Angel/Ghost
> orientations entered eleven-worker transition phases. The backfill inventory
> contains no retained >1-KiB table awaiting a trivial audit. The remaining
> idle CPU is therefore concentrated behind two storage-bound Devil graphs and
> serial Ghost phases, not an unlaunched ready job; `ready_jobs=[]` is recorded
> as scheduler evidence, not accepted as a performance proof.
>
> **Prior AWS fleet audit (2026-08-23 04:46 PDT / 11:46 UTC):** a complete
> exact-supervisor CPU-delta sample measured i024 26.000/32, i03 3.998/32,
> i08 5.910/32, i098 2.000/32, and i0b 6.266/32 busy vCPUs:
> **44.174/160, or 27.6% fleet CPU**. It authenticates 50 completed jobs,
> 22 running jobs, and zero current failed jobs; the canonical ledger remains
> 451 certified, zero preserving, and 23 computing rows. Both repaired
> Angel/Ghost services remain healthy across later shards and account for 22
> non-overlapping transition workers on i024. I098's Pawn/Ghost shards have
> entered their authenticated single-threaded merge; i03 likewise has the
> serial same-Ghost/Ghost merge plus three serial Ghost fixed points. Devil
> square 2 and square 3 remain in retained-edge phases on i0b and i08; their
> wide worker allocations are storage-limited, while RAM and checkpoint gates
> prevent colocating another high-memory Devil root safely. A fresh >1-KiB
> backfill inventory was empty on all five hosts. Thus no current failure or
> unaudited trivial row can consume the idle cores; the actionable blocker is
> serial merge/fixed-point design plus Devil retained-edge I/O, not an empty
> scheduler queue by itself.
>
> **Prior AWS fleet audit (2026-08-23 05:53 PDT / 12:53 UTC):** the exact
> supervisor found opposed Angel/Ghost v10 failed at shard 43 because the
> generic runner scheduled the same orientation's three-substate geometry
> count for an opposed two-substate model. The solver correctly rejected the
> first start outside its 985,920-geometry domain; no partial graph was merged.
> Orientation-bound geometry regression coverage, deterministic source bundle
> sha256:0772149134ff300d67bf7b0decbda5b972c76f73e5f8d4ec743f3216b06f9c1a
> (S3 VersionId `.DHfnYDiZJ3lI6fb9Epvq1UM9XfUZs_W`), and wrapper
> sha256:447368b22af61871c796c0fa2598f5736fb333571aaf436adf91d59058d14a29
> (VersionId `Z7yad_b0CBONb8Evr4stB62sHOY2FqR5`) now bind a fresh v13 graph.
> Its exhaustive gates passed and seventeen non-overlapping workers are active
> on i024 CPUs 15-31, raising that host to a directly measured 32.000/32 busy
> vCPUs. A complete five-second host-counter sample measured i03 4.193/32,
> i08 4.260/32, i098 2.000/32, and i0b 6.116/32, for **48.569/160, or 30.4%
> fleet CPU**. I08 and i0b simultaneously spent 76.9% and 63.1% in I/O wait;
> i03 and i098 remain in serial merge/fixed-point phases. The reconciled state
> is 50 completed jobs, 22 running, and zero current failed jobs; the ledger is
> unchanged at 451 certified, zero preserving, and 23 computing rows. The
> >1-KiB trivial-backfill inventory is empty on all five hosts.
>
> **Prior AWS fleet audit (2026-08-23 07:22 PDT / 14:22 UTC):** both repaired
> Angel/Ghost transition campaigns completed all 64 shards, exhaustive replay,
> and exact merge authentication: same covers 1,478,880 geometries in a 33-GiB
> graph; opposed covers its exact 985,920-geometry two-substate domain in a
> 22-GiB graph. The supervisor's terminal label for these transition-only jobs
> is not a tablebase certification, so both were advanced immediately into
> separate authenticated fixed-point solves. Both solves then failed closed
> before emitting an output because a hidden same-class child had no decision
> stratum. A version-pinned diagnostic replay over the retained graph isolated
> the exact same-team witness: parent geometry 469 (`Ka1`, `Kc1`, Angel attached
> to the white King, Halo `b1`) legally reaches geometry 925,243 with the
> protected King on `b2`. The shared padding gate incorrectly discarded that
> adjacent-King child even though Angel protection makes it a legal turn
> boundary. The scoped fix admits adjacent real Kings only in Angel substate 1,
> adds that exact witness to native startup self-test, and retains the expanded
> geometry/relation diagnostic. Both old graphs and failed solve logs remain
> preserved and non-certifying. Corrected same and opposing binaries passed
> the full 455,495,040- and 303,663,360-codec-state self-tests respectively,
> including exact geometry 469→925,243 and 469→616,829 witnesses. Deterministic
> source bundle sha256:159eea065bd196755b0142966d1872eaf6265888900be4e28579b03788e7df7b
> is S3 VersionId `rnkYas1uV9A8_SvYZSHFDIR8lEOlZhrS`; fresh v15/v16 graphs
> now own non-overlapping i024 CPUs 4-14 and 15-31. A complete exact-supervisor
> sample measured i024 32.208/32, i03 4.008/32, i08 32.484/32, i098
> 2.000/32, and i0b 5.965/32: **76.665/160, or 47.9% fleet CPU** (the two
> slight over-32 readings are interval-boundary process-turnover noise). The two
> retained failed solve attempts are explicitly superseded by their corrected
> graph jobs; supervision reconciles 52 completed jobs, 22 running jobs, and
> zero current unsuperseded failures. The canonical ledger therefore
> remains 451 certified, zero preserving, and 23 computing rows. A fresh
> authenticated >1-KiB trivial-backfill inventory is empty on all five hosts.
>
> **Current fleet intervention (2026-08-24 04:55 PDT / 11:55 UTC):** the
> serial Bellman tails for opposed Queen/Ghost, opposed Ninja/Ghost, and both
> Angel/Ghost orientations were migrated from retained checkpoints to the
> authenticated eight-worker striped-ROBDD build. Same Checker/Ghost was
> likewise migrated to eight workers after repairing its explicit four-substate,
> horizontal-only, Checker-king, Ghost-primary, and implicit-draw lower
> bindings; all rejected attempts stopped before changing the retained
> iteration-31 checkpoint. Opposed Prince/Ghost remains on eight workers until
> its in-flight iteration-8 checkpoint is durable, after which its older broad
> provenance binding can be replaced safely.
>
> Devil C2 completed and verified 5,176,987,699 states and 33,045,156,534
> edges; A3 completed and verified 361,229,977 states and 2,344,674,058 edges.
> Their content-addressed root fragments passed exact-version S3 restore
> comparisons. Disjoint certifying B3, C3, and D3 partitions now run beside the
> retained C1 and D2 jobs; none is a census or overlapping shard. A six-second
> whole-host sample measured non-idle CPU of i024 31.85/32, i03 27.84/32,
> i08 29.52/32, i098 27.96/32, and i0b 22.52/32: **139.70/160 (87.3%)**.
> Excluding storage wait, **98.98/160 (61.9%)** was executing CPU work in that
> synchronized interval. The final non-overlapping allocation is i024 32/32,
> i03 32/32, i08 32/32,
> i098 29/32, and i0b 31/32: **156/160 (97.5%) assigned**. The four
> deliberately unassigned CPUs preserve headroom on the two storage-bound
> hosts. The remaining gap is concentrated in reverse-edge storage phases:
> i03, i08,
> i098, and i0b spent 24.3%, 29.5%, 44.2%, and 29.2% in I/O wait. Adding
> further graph builders there would increase
> contention rather than throughput; compute-bound Ghost work is overlaid on
> their non-overlapping CPU sets within the measured RAM gates. The canonical
> ledger remains **452 certified, 0 preserving, 22 computing, and 0 failed**
> rows; the newly preserved Devil fragments support `single:devil` but do not
> certify that row until all twelve partitions merge and verify. Exact
> supervision reconciles 24 current running jobs and zero current failed jobs.
>
> **Prior fleet audit (2026-08-24 01:30 PDT / 2026-08-24 08:30 UTC):**
> the exact checked-in supervisor authenticated all current source bindings and
> version-pinned S3 objects with no current failure, source mismatch, or newly
> terminal job. Opposed Penguin/Ghost v8 reached fresh iteration 5 at
> 1,405,000/3,943,680 geometries. Opposed Angel/Ghost reached iteration 14 at
> 945,000/985,920, same Angel/Ghost entered iteration 11, and opposed
> Checker/Ghost reached iteration 33 at 2,100,000/3,943,680. Copycat/Ghost
> completed iteration-6 external compaction with structural and root residuals
> zero and entered iteration 7. Quiet units again showed increasing CPU
> counters, so no health or completion status is inferred from launch state.
>
> Devil reverse-edge construction reached 5,140,000,768/5,176,987,699 states
> for square 10 (99.3%), 2,810,003,456/5,194,192,187 for square 11, and
> 1,330,003,968/3,861,213,174 for square 2. The canonical ledger remains
> **452 certified, 0 preserving, 22 computing, and 0 failed** rows; the
> supervisor has 23 current running jobs and zero current failed jobs. The
> authenticated >1-KiB trivial-backfill inventory is empty on all five hosts.
>
> The final supervisor interval measured i024 6.000/32, i03 4.000/32, i08
> 2.784/32, i098 4.756/32, and i0b 5.973/32 busy vCPUs: **23.513/160
> (14.7%)** measured fleet CPU. I098's interval was incomplete; the
> conservative upper bound remained 31.7%. The scheduler exposes no safe
> non-overlapping ready or CPU-rebalance job, leaving serial ROBDD work and the
> Devil reverse builder's memory/I/O locality as the active blockers.
>
> **Prior fleet audit (2026-08-24 00:29 PDT / 2026-08-24 07:29 UTC):**
> the exact checked-in supervisor reauthenticated every current source binding
> and version-pinned S3 object with no new terminal job, current failure, or
> source mismatch. Opposed Penguin/Ghost v8 advanced to fresh iteration 4 at
> 705,000/3,943,680 geometries. Opposed and same Angel/Ghost reached iteration
> 14 at 460,000/985,920 and iteration 10 at 1,450,000/1,478,880 respectively;
> opposed Checker/Ghost entered iteration 33, and Copycat/Ghost reached
> iteration 5 at 460,000/492,960. Quiet long-running units again had increasing
> CPU counters, so no completion or health claim relies on service state alone.
>
> Devil reverse-edge construction reached 4,940,001,280/5,176,987,699 states
> for square 10, 2,650,001,408/5,194,192,187 for square 11, and
> 1,310,003,200/3,861,213,174 for square 2. The canonical ledger remains
> **452 certified, 0 preserving, 22 computing, and 0 failed** rows; the
> supervisor has 23 current running jobs and zero current failed jobs. The
> authenticated >1-KiB trivial-backfill inventory is empty on all five hosts.
>
> The final supervisor interval measured i024 5.999/32, i03 3.999/32, i08
> 2.821/32, i098 4.606/32, and i0b 5.886/32 busy vCPUs: **23.311/160
> (14.6%)** measured fleet CPU. I098's interval was incomplete; the
> conservative upper bound was 31.7%. The scheduler exposes no safe
> non-overlapping ready or CPU-rebalance job, while serial ROBDD tails and the
> Devil reverse builder's memory/I/O locality continue to strand most cores.
>
> **Prior fleet audit (2026-08-23 23:28 PDT / 2026-08-24 06:28 UTC):**
> the exact checked-in supervisor authenticated every current source binding
> and version-pinned S3 object with zero current failures, source mismatches, or
> newly terminal jobs. Opposed Penguin/Ghost v8 advanced from fresh iteration 1
> into iteration 2 at 3,690,000/3,943,680 geometries under the corrected
> concrete dependency. Both Angel/Ghost orientations, opposed Checker/Ghost,
> Copycat/Ghost, same Checker/Ghost, and same Berserker/Ghost have fresh log
> progress. CPU counters for quiet Queen/Ghost, Ghost/Ghost, opposed
> Sniper/Ghost, opposed Prince/Ghost, Bomb/Ghost, Dragon/Ghost, Ninja/Ghost,
> and Parasite/Ghost all increased, so none is being mistaken for healthy from
> unit state alone.
>
> Devil reverse-edge construction reached 4,770,000,896/5,176,987,699 states
> for square 10, 2,500,001,792/5,194,192,187 for square 11, and
> 1,280,000,000/3,861,213,174 for square 2. The canonical ledger remains
> **452 certified, 0 preserving, 22 computing, and 0 failed** rows; the
> supervisor has 23 current running jobs and zero current failed jobs. The
> authenticated >1-KiB trivial-backfill inventory is empty on all five hosts.
>
> The final supervisor interval measured i024 6.000/32, i03 4.000/32, i08
> 2.906/32, i098 4.836/32, and i0b 5.880/32 busy vCPUs: **23.622/160
> (14.8%)** measured fleet CPU. I098's interval was incomplete and the
> conservative fleet upper bound was 31.7%. No non-overlapping job is exposed
> by the current scheduler, but this remains an `UNDERUTILIZED` failure rather
> than acceptable capacity use; serial ROBDD tails and the Devil reverse
> builder's memory/I/O locality are still the active architectural blockers.
>
> **Prior fleet audit (2026-08-23 22:27 PDT / 2026-08-24 05:27 UTC):**
> the exact checked-in supervisor reauthenticated all current source bindings
> and version-pinned S3 objects with no current job failure or source mismatch.
> Opposed Penguin/Ghost v8 has crossed both repaired scratch-parent boundaries
> and is advancing fresh iteration 1 at 1,195,000/3,943,680 geometries with the
> corrected concrete source; no stale fixed point is reused. Fresh file tails
> also show both Angel/Ghost solves, opposed Checker/Ghost, Copycat/Ghost, same
> Berserker/Ghost, and same Sniper/Ghost advancing. Quiet Queen/Ghost,
> Ghost/Ghost, opposed Sniper/Ghost, opposed Prince/Ghost, Bomb/Ghost,
> Dragon/Ghost, Ninja/Ghost, and Parasite/Ghost units were checked at the
> process level and are all consuming CPU rather than stalled.
>
> Devil fixed-square reverse-edge construction has reached
> 4,590,002,176/5,176,987,699 states for square 10,
> 2,350,002,176/5,194,192,187 for square 11, and
> 1,260,003,328/3,861,213,174 for square 2. Their process samples consumed
> about 3.91, 6.16, and 0.81 cores respectively despite 31-, 21-, and
> 25-thread allocations, confirming the retained-edge memory/I/O bottleneck.
> The canonical ledger remains **452 certified, 0 preserving, 22 computing,
> and 0 failed** rows; the supervisor has 23 current running jobs and zero
> current failed jobs. The authenticated >1-KiB trivial-backfill inventory is
> empty on all five hosts.
>
> The final supervisor interval measured i024 6.000/32, i03 3.999/32, i08
> 2.874/32, i098 5.203/32, and i0b 6.023/32 busy vCPUs: **24.099/160
> (15.1%)** measured fleet CPU. I098's interval was incomplete; even the
> conservative fleet upper bound is only 31.8%. There is no non-overlapping
> ready or CPU-rebalance job in the current configuration, but that does not
> make this utilization acceptable: parallel ROBDD solving and better-locality
> Devil reverse construction remain required scheduler/workload fixes.
>
> **Prior fleet audit (2026-08-23 22:14 PDT / 2026-08-24 05:14 UTC):**
> the exact checked-in supervisor caught opposed Penguin/Ghost v7 failing
> closed before changing transition or fixed-point bytes because its real-solve
> normalized-source parent was absent. This followed the independently retained
> v6 self-test-parent failure. Both failed units are superseded. V8 authenticates
> the corrected 607,326,720-state concrete table, the unchanged transition
> marker, and the exact v7 continuation; it retains the failed log, creates both
> scratch parents, and deliberately starts from an empty fixed point. It has
> crossed both former failure points and created a fresh 151,831,728-byte
> normalized solve source on i03 CPU 2. Wrapper
> sha256:181855dda649d7601e8d981801c2379a3a3c3b9ed84f020f190e9a0249c818bd
> is S3 VersionId `2JsxeR2CuhZH1EOYtwk8fkTO65lK8gyn`; unit
> sha256:8c4303f3879c0ef29055f57c1e79a1e2b020137dbafe1be036f48d58e971e8dc
> is VersionId `tpKgG8mBargS612gtDmFptqFv5ddTUJl`. No result is inferred from
> the healthy restart.
>
> The canonical ledger is **452 certified, 0 preserving, 22 computing, and 0
> failed** rows. The supervisor has 23 current running jobs and zero current
> failed jobs; the extra job is supporting work rather than a second ledger
> class. The authenticated >1-KiB concrete trivial-backfill inventory remains
> empty on all five hosts. The final sample measured i024 5.994/32, i03
> 3.922/32, i08 2.982/32, i098 4.881/32, and i0b 5.992/32 busy vCPUs:
> **23.771/160 (14.9%)** measured fleet CPU. I098's interval was incomplete;
> even its conservative utilization upper bound is only 31.8%. The supervisor
> correctly reports `UNDERUTILIZED`; `ready_jobs=[]` is not accepted as proof
> that the scheduler, serial Ghost tails, or Devil memory/storage locality
> cannot be improved.
>
> **Prior fleet audit (2026-08-23 20:29 PDT / 2026-08-24 03:29 UTC):**
> the exact checked-in supervisor caught Copycat/Ghost v5 failing closed during
> fresh iteration 54 with `reciprocal owner least fixed point regressed`. The
> failure is real and explains the earlier six Bellman residual roots: changing
> the action-conditioned observer recurrence made the pre-change iteration-53
> roots an unsafe warm start, not merely a stale-but-monotone checkpoint. V5
> emitted no result and is now explicitly superseded. Version-pinned v6 retains
> and reauthenticates the complete 492,960-geometry transition graph but starts
> mutable roots and ROBDD state from the empty least fixed point. It preserves
> the former failed `solve.log` under its authenticated SHA-256 and has
> demonstrably entered iteration 1 (5,000/492,960 geometries in the first fresh
> sample). Wrapper
> sha256:2be38a0baeeec77998de72e2d3f3b9a2cb95c67cd3d45645a1b9ce988d0e6209
> is S3 VersionId `kJQSZlfOoXE9NLXNbvz._V.wWeF_MH0B`; unit
> sha256:75e42bcc6f07d47a1f0f4337214de3cc660f7669bf290d717ea29c9142153c27
> is VersionId `wiGWD7n0Dhy_WWFcEAJgwvmtLs0cz88F`. Penguin/Ghost remains active
> after its zero-change iteration 83 while the independent certification tail
> runs. No completion is inferred from either launch or fixed-point convergence.
>
> The canonical ledger remains **452 certified, 0 preserving, 22 computing,
> and 0 failed** rows. There are again 23 current running supervisor jobs after
> the repair; the authenticated >1-KiB concrete trivial inventory is empty on
> all five hosts. The final post-repair sample measured i024 5.994/32, i03
> 3.000/32, i08 3.114/32, i098 4.897/32, and i0b 5.719/32 busy vCPUs:
> **22.724/160 (14.2%)** fleet CPU. I03 and i098 had incomplete intervals; even
> the conservative utilization upper bound is only 49.3%. The supervisor still
> reports `UNDERUTILIZED`; serial Ghost proof tails and memory/storage-bound
> Devil retained-edge construction remain the fleet-level blocker.
>
> **Prior fleet audit (2026-08-23 19:31 PDT / 2026-08-24 02:31 UTC):**
> the exact checked-in supervisor authenticated every current source binding and
> version-pinned S3 object. It caught a new Copycat/Ghost v4 certification
> failure: the independent Bellman equality check found six residual roots and
> emitted no result. As with Penguin/Ghost, v4 had reopened roots produced
> before the action-conditioned observer recurrence but incorrectly passed
> `--resume-converged`, so the corrected recurrence was never allowed to update
> them. V4 is now explicitly superseded by version-pinned v5. V5 preserves the
> authenticated iteration-53 roots and unchanged 492,960-geometry transition
> payload, removes the false convergence declaration, and is demonstrably
> advancing fresh Bellman iteration 54 (45,000/492,960 geometries in the latest
> sample). Wrapper
> sha256:b16dc6a276403508b1ed54caf2fa260e90d0e0af45132741b9aeefb6f8eba4a6
> is S3 VersionId `mgI5Nc8_Hn3bSPBEyyJ_6mGowPcWMa_H`; unit
> sha256:09999e45f7cedd8a38571ac04ae2e4e712370c85152170a4a2de9d3faa825fb1
> is VersionId `klpyFEuUPvquIZV5iC9xQfd1_iA6CjUB`. Penguin/Ghost v5 has now
> completed the full 3,943,680-geometry iteration 83 with zero changed owner,
> observer, or visible roots and is continuing through its certifying tail; a
> unit launch or zero-change iteration alone is not treated as completion.
> The canonical ledger remains **452 certified, 0 preserving, 22 computing,
> and 0 failed** rows. There are 23 current running supervisor jobs because the
> one Devil ledger class owns three non-overlapping retained-edge shards. The
> authenticated >1-KiB concrete trivial inventory is empty on all five hosts.
>
> The final post-repair supervisor sample measured i024 6.011/32, i03
> 3.003/32, i08 4.070/32, i098 5.738/32, and i0b 5.290/32 busy vCPUs:
> **24.112/160 (15.1%)** fleet CPU. I03 and i098 had incomplete measurement
> intervals; pessimistically counting both as 32/32 still bounds the fleet at
> 49.6%, below the configured 75% floor. There is no safe ready or CPU-rebalance
> job in the current configuration: most Ghost proof tails are serial and the
> Devil retained-edge shards are memory-traffic/storage bound. The supervisor
> therefore continues to report `UNDERUTILIZED`; `ready_jobs=[]` is not treated
> as evidence that the utilization is acceptable.
>
> **Prior fleet audit (2026-08-23 18:32 PDT / 2026-08-24 01:32 UTC):**
> the exact checked-in supervisor authenticated every current source binding and
> version-pinned S3 object without a binding or artifact error. It caught one new failure:
> opposed Penguin/Ghost v4 reached the exhaustive singleton comparison and
> reported 956,684 dominance violations. The retained iteration-82 roots had
> been produced before the action-conditioned observer recurrence, but the v3
> and v4 wrappers incorrectly passed `--resume-converged`, preventing the
> corrected Bellman iteration from updating them before certification. No result
> was emitted. The failed verifier is now superseded by version-pinned v5, which
> preserves the authenticated graph and roots, removes the false convergence
> declaration, and is demonstrably advancing Bellman iteration 83 (960,000 of
> 3,943,680 geometries in the first fresh sample). Wrapper
> sha256:f5d3d47758494afd1e0f7ce7363a051e1220a9fa7c5fd7c457a692bb2efe4d84
> is S3 VersionId `r5XfkvjW8E5wYCGmvkKfZ.yKTW9Cgn5p`; unit
> sha256:49e5e8b39ee4979c6f649f56c57887bc3b5ef7d6e4d0936d21769e60e06cc0e3
> is VersionId `UuWfRa.FSgy_8wE7C1S1Xh4YJ_BgXCuh`. The canonical ledger remains
> **452 certified, 0 preserving, 22 computing, and 0 failed** rows, and the
> authenticated >1-KiB concrete trivial inventory is empty on all five hosts.
>
> The final post-repair supervisor sample measured i024 5.988/32, i03 2.994/32,
> i08 4.362/32, i098 6.317/32, and i0b 6.025/32 busy vCPUs:
> **25.686/160 (16.1%)** fleet CPU; i03's interval was marked incomplete.
> Independent process/resource profiling confirms that this is real
> underutilization rather than a supervisor-only count error: the 31-thread i08
> Devil worker used about 4.5 cores at 80.9% RAM, the 21-thread i098 worker about
> 7.0 cores at 47.1% RAM, and the remaining Ghost/Jester fixed-point and graph
> tails are almost entirely single-core. RAM is plentiful on i024 and i03, while
> the Devil workers are limited by retained-edge memory traffic and storage
> flushing rather than thread allocation. The supervisor no longer requires a
> nonempty ready queue before recording underutilization and no longer lets one
> incomplete host probe suppress an obvious fleet-wide failure: pessimistically
> counting incomplete i03 as 32/32 still bounds the fleet at 34.2%, versus the
> configured 75% floor. After two samples this is now a first-class
> `UNDERUTILIZED` supervisor error despite `ready_jobs=[]`. Parallel Bellman
> partitioning and Devil retained-edge locality remain the active utilization
> defects.
>
> **Prior fleet audit (2026-08-23 17:00 PDT / 2026-08-24 00:00 UTC):**
> Rook/Ghost completed its exhaustive singleton-dominance certificate with
> zero Bellman, monotonicity, and dominance residuals. Its deterministic
> 20,624,394-byte archive (sha256:852a08e3af1ee3bd85855484b91ae77855b5f7c18769cae052a91bcb88a386b7,
> VersionId `eUKHKHvIO8vF418a1zAY27tWBpXen6_r`) passed both local and exact-version
> S3 restores and has been imported. A fresh ten-worker, 16-source
> information-trivial v2 audit then supplied exhaustive per-substate brackets;
> its sidecar is sha256:d7b3d2e5df9fe6ed10a6a7f1a2cd9f98d133fd6c8049810587945a300addde31,
> VersionId `o8iDBE.wnQ7uErABfAQIZuvLYUU8RJax`. The ledger is now **452 certified,
> 0 preserving, 22 computing, and 0 failed** rows, and the >1-KiB concrete
> trivial-backfill inventory is empty on all five hosts.
>
> Penguin/Ghost's v3 dominance pass exposed a real source-normalization defect:
> opposed Ghost-primary color swapping exchanged the physical Kings without
> exchanging Penguin freeze bits 1 and 2. The codec now swaps those bits in
> both normalization directions and exhaustively checks the involution. The
> deterministic replacement bundle is
> sha256:cb96a41474a62fe48f5d3464a78f42c2a5ccbd3977aeb2d79bbafb082bdff94a
> (VersionId `kQIv_4q0A7hy0jfcC2.3MqAmXcgNwVgd`); its v4 verifier is active from
> the retained iteration-82 checkpoint. Copycat/Ghost subsequently finished
> iteration 53 but failed its combined symbolic certificate before singleton
> comparison. The failed unit is superseded, not restarted unchanged: a
> version-pinned v4 diagnostic is active from the same retained checkpoint and
> now reports Bellman and monotonicity residuals separately so the failing
> invariant can be repaired without discarding the solve.
>
> A simultaneous five-second host-counter sample measured actual compute
> (user+nice+system, excluding I/O wait) at i024 6.138/32, i03 4.026/32, i08
> 5.203/32, i098 7.296/32, and i0b 6.246/32: **28.909/160 (18.1%)** fleet CPU.
> I08, i098, and i0b simultaneously spent 74.98%, 43.53%, and 23.22% in I/O
> wait. Current idle capacity is still stranded behind serial Ghost proof tails,
> storage-bound Devil retained-edge construction, and overstated disk/RAM
> reservations; `ready_jobs=[]` is scheduler evidence, not a justification for
> the low utilization.
>
> **Prior repair audit (2026-08-23 14:18 PDT / 21:18 UTC):** opposed
> Ghost/Penguin reached a genuine zero-change iteration-82 fixed point with
> 232,995,187 BDD nodes, then the obsolete singleton-equals-concrete oracle
> rejected 12,793,334 information differences. No output was written. This is
> the same invalid proof contract already demonstrated by the retained
> Rook/Ghost five-destination witness, not a fresh Bellman failure. The old unit
> is superseded. The version-pinned eight-Penguin-substate replacement passed
> its exhaustive 1,214,653,440-state preflight with zero codec, remap, count,
> payload-authentication, and transition-replay residuals, reopened the
> authenticated iteration-82 current-root/BDD-b checkpoint, reproduced the
> independent Bellman fixed point, and is now running the sound information-
> dominance verification. Rook/Ghost and Copycat/Ghost remain active in their
> independent Bellman verification passes.
> The canonical ledger remains **451 certified, 0 preserving, and 23 computing**
> rows; no certified row is awaiting a retained >1-KiB trivial audit on any of
> the five hosts. A simultaneous exact five-second host-counter sample measured
> actual compute (user+nice+system, excluding I/O wait) at i024 6.211/32, i03
> 5.184/32, i08 4.317/32, i098 4.608/32, and i0b 6.010/32: **26.330/160
> (16.5%)** fleet CPU. I08, i098, and i0b separately spent 74.97%, 46.72%, and
> 30.15% in I/O wait. The scheduler still exposes no ready/rebalance work despite
> this load; serial Ghost proof phases, storage-bound Devil scans, and
> over-reserved RAM/disk remain unresolved utilization defects.
>
> **Prior repair audit (2026-08-23 13:57 PDT / 20:57 UTC):** opposed
> Rook/Ghost and Copycat/Ghost both reconverged under the action-conditioned
> canary with zero changed roots (iterations 23 and 53 respectively), disproving
> the earlier recurrence diagnosis. A retained Rook edge witness instead proved
> the old singleton-equals-concrete oracle invalid: one private Ghost move from
> a singleton at square 56 has five publicly indistinguishable destinations.
> The certifier now exhaustively checks the sound information-dominance relation
> while retaining independent Bellman equality and sidecar reproduction. Both
> version-pinned replacement verifiers are active against unchanged authenticated
> edge payloads and retained fixed points. The first Rook v3 launch selected the
> dormant undersized BDD-a file and failed closed before verification; v3c
> authenticates and has reopened the log-proven current-root/BDD-b pair with
> 910,532,134 nodes. No output is inferred from either launch. The canonical
> ledger remains **451 certified, 0 preserving, and 23 computing** rows, and a
> fresh >1-KiB trivial-backfill inventory is empty on every host. The latest
> supervisor interval measured i024 6.024/32, i03 4.018/32, i08 3.884/32,
> i098 3.097/32, and i0b 5.891/32, or **22.914/160 (14.3%)**; the i03 and i098
> deltas were incomplete, so this is a lower-bound rather than an exact fleet
> measurement. This utilization remains unacceptable. The scheduler still
> reports no ready or rebalance jobs despite the low measured load, confirming
> that stale disk/RAM reservations and serial Ghost/Devil phases remain an
> active scheduling defect rather than proof that no utilization improvement is
> possible.
>
> **Prior repair audit (2026-08-23 10:54 PDT / 17:54 UTC):** the corrected
> same Angel/Ghost transition campaign completed all 64 shards and authenticated
> its exact 1,478,880-geometry merge under marker
> sha256:`2aac70e770a0321f78183404f0e9baad3e140f4fcd74662b78023c378b8d6b64`.
> Its first solve wrapper failed closed before creating a log, result, or solve
> checkpoint because it supplied the obsolete pre-fix model hash; that attempt
> is superseded. Corrected v19 wrapper
> sha256:`961830514022796d61136a070fc9f796bec7c1249ee9f8e95eea84f10adcd37a`
> is S3 VersionId `rZXXbSFJ1hjAN38qMG2Zc9RIERUN6.OL`, binds the marker's
> exact model hash, and is making real iteration-2 progress on i024 CPU 4
> (315,000/1,478,880 geometries at the fresh audit). Opposing v17 is likewise
> healthy at iteration 3, geometry 700,000/985,920 on CPU 15.
> Both completed Angel/Ghost graphs were also deterministically archived while
> the solves continued, uploaded to exact content-addressed versions, downloaded
> by VersionId, and compared byte-for-byte: same graph
> sha256:`c3f99a9f3f5b7d35425cd37cc0f2c81085b18aadd6e3a2ca1f2234ec2bdcd0d6`
> is VersionId `KoTiCmXBwNArv4lsOfKKtGf5ZAZof.Q0`; opposing graph
> sha256:`e576fe5782514fac077fec3394dc89168519717c66ee7d362352e9f45fb3c63e`
> is VersionId `bcus8n2gY7mH9NIgUfboR7L3N_MDtgD1`. Supervision now
> authenticates **60 completed support jobs, 23 running jobs, zero preserving,
> and zero unsuperseded failed jobs**; the canonical ledger remains **451
> certified, 0 preserving, and 23 computing** rows. Devil c2 has completed its
> 5,176,987,699-state graph and is at 540,000,256/5,176,987,699 in its
> retained reverse scan; disjoint d2 is building the layer after
> 4,130,632,905 states. The fresh >1-KiB trivial inventory is empty on all five
> hosts. A complete exact-supervisor interval measured i024 6.000/32, i03
> 3.997/32, i08 3.855/32, i098 12.802/32, and i0b 5.981/32:
> **32.635/160, or 20.4% fleet CPU**. This remains unacceptable utilization:
> c2 is RAM/disk-bandwidth-bound at 150 GiB, d2 is memory-gated while constructing
> a multi-billion-state layer, and the live Ghost fixed points/merges are serial;
> the completed graph-preservation work consumed idle i024 CPUs but finished
> within minutes, so it does not conceal the remaining algorithmic bottleneck.
>
> **Prior repair audit (2026-08-23 09:10 PDT / 16:10 UTC):** corrected
> opposing Angel/Ghost v16 completed all 64 transition shards and its exact
> 985,920-geometry merge. The authenticated marker is
> sha256:`1a0e5e47675cd25339471146339a7d5d46d2da5f9bcd20c91e463c43bdbeafff`;
> certifying solve v17 is active on i024 CPU 15 against that retained graph and
> the exact lower Ghost/concrete inputs. The source, binary, transition wrapper,
> and solve wrapper were restaged as SHA-metadata-bearing, version-pinned S3
> objects; no result is inferred from the launch. Devil c2 exposed a genuine
> v6 implementation ceiling after committing 4,117,172,392 exact states: its
> next layer crossed 32-bit dense indexing. Wide-index v7 imports the v6
> checkpoint, uses 64-bit frontier/hash/reverse-edge indices, and is active on
> 31 i08 CPUs under 150/160-GiB memory gates. Deterministic 16-source bundle
> sha256:`8d9fc6dfc8b648c343f442a4cf7a0c0eb8c0d118cd826584c65b22f93bc28f24`
> is S3 VersionId `LSgUY04193XBQ4mA4SDpZjCwb9xxjO2b`; Linux binary
> sha256:`9e3788b73537ba879bc7b6c9a2f3444cfd521d5c18c74d6b1bd952f04d7b7a0f`
> is VersionId `r8lnVHNE8kAH6745DhT0187b52EDdC_5` and passed exact-version
> restore comparison. Disjoint b2 then completed 1,632,071,820 states,
> 10,456,064,298 edges, and exhaustive Bellman replay; fragment
> sha256:`7d1cb299af6e3022d4063102f6074917a9fe6e974574094eadf0929e5f130108`
> is S3 VersionId `NbMpdvO9TIgUlV62cQRkJVAkCobb8CZF` and passed exact restore
> comparison. Its freed 21-CPU set immediately started disjoint d2 under v7.
> The authenticated >1-KiB trivial-backfill inventory remains empty on every host.
> A complete final supervisor interval measured i024 15.844/32, i03 3.976/32,
> i08 31.253/32, i098 22.751/32, and i0b 7.072/32: **80.896/160, or
> 50.6% fleet CPU**. Supervision has 57 authenticated completed support jobs,
> 23 running, and no unsuperseded failure or preserving-stage job; the canonical
> ledger remains **451 certified, 0 preserving, and 23 computing** rows.
>
> **Current AWS fleet audit (2026-08-23 08:14 PDT / 15:14 UTC):** exact
> supervision authenticates 55 completed/certified support jobs, 23 running
> jobs, and no current failed or preserving-stage job; the canonical ledger remains
> **451 certified, 0 preserving, and 23 computing** rows. Same Ghost/Ghost's
> authenticated 9,739,120-root transition rebuild completed successfully; its
> merged 62-GiB sparse graph is now in the actual solve through version-pinned
> solve-existing runner sha256:422f23cf58972b80875f41e6105f8814e421c6116256381ff7bf11ba48ddb97a
> (S3 VersionId `31PBnXl6U68W7J1eSfOCCItKTzm4Q1GU`) and wrapper
> sha256:4f85dd68092e20c9e85b8ad398bbc8842913b8a7de472fa10b0146dc87769256
> (VersionId `Q0M.8TnAjmEntpWMHp_W_rJltITvuryc`). The first launch failed
> closed at argument parsing because the retained runner predated
> `--solve-existing`; it touched no result or checkpoint and was replaced by
> that tested exact runner. The transition archive itself is preserved at
> sha256:5f740b452a493601737cbd2cd9620ec271305191643eaf8d36cf2ebd5b0201df,
> S3 VersionId `1zb2JYTbHrOQjg88g_HJdHJ6ujs37FEw`; restaging the identical
> source with SHA metadata closed the last uncertified preservation stage.
> Devil d1 completed and exhaustively verified
> 3,873,307,130 states and 24,345,340,979 edges; a2 then completed and verified
> 364,746,583 states and 2,346,760,900 edges. Both root fragments were uploaded,
> authenticated by exact S3 versions, and byte-compared after restore. Their freed CPUs immediately
> started disjoint b2 and c2 partitions from the same exact v6 source/binary.
> An initial b2 allocation overlapped Pawn/Ghost's reserved shard CPUs; live
> affinity was corrected without restart to the complementary 21-CPU set, and
> the final allocation has no overlap. A complete checked-in supervisor sample
> measured i024 27.320/32, i03 4.008/32, i08 31.734/32, i098 23.066/32, and
> i0b 6.472/32: **92.600/160, or 57.9% fleet CPU**. I024's short Angel shard
> turnover accounts for its interval-to-interval variation. I03 is constrained by
> four serial, high-memory Ghost fixed points and i0b by 116 GiB of live Ghost
> state plus storage-bound Devil c1; adding another 150-GiB Devil root on either
> would violate the retained jobs' RAM gates. The fresh authenticated >1-KiB
> trivial-backfill inventory is empty on all five hosts.
>
> **Failure-repair and certification audit (2026-08-22 19:27 PDT / 2026-08-23
> 02:27 UTC):** a fresh supervisor reconciliation and recent-object S3
> audit found no missed result certificate or manifest, so the ledger total was
> exactly **449 certified** at that timestamp. It is superseded by the current
> **451** total after authenticated Rook/Ghost and Turtle/Ghost imports. The two current
> failed units were repaired rather than merely diagnosed. Same Checker/Ghost
> v16 reopened the authenticated iteration-17/BDD-b checkpoint under corrected
> 36/40-GiB controls and is advancing Bellman iteration 18; at 18:16 UTC it had
> reached geometry 1,375,000/3,943,680 with about 9.90 GB peak RSS. Same
> Pawn/Ghost v5 exposed the unimplemented existing-ROBDD reopen path; v6 fixed
> that path, then its full transition audit failed closed on adjacent-real-King
> dense padding. v8 excluded that padding but found a second exact residual at
> geometry 493,272 (`Ka1`, `Kc1`, `Pb1`, hidden Ghost `d1`): the retained graph
> stored twelve moves where the current generator produces two. The v9 witness
> proved the retained v4 graph invalid outside the padding domain, so its
> iteration-42 fixed point is preserved as evidence but will not be resumed.
> Fresh empty-tree v10/v11 attempts then exposed the root build defect rather
> than producing a result: their prebuilt Pawn binaries omitted the required
> `ULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY` specialization, incorrectly enforced
> vertical D2 symmetry, and exposed only 985,920 of 1,971,840 public geometries.
> Their partial shards are non-certifying and superseded. The version-pinned v12
> binary was rebuilt from authenticated source with both the horizontal-only
> Pawn domain and shared adjacent-King padding exclusion, passed the exhaustive
> 303,663,360-state self-test/source normalization, and reports the correct
> 1,971,840-geometry domain. Its new empty transition tree is advancing on i098
> CPUs 0 and 2: shards 00-11 have now passed full transition/conservation and
> exact lower-edge patch/reload certificates; shards 12-13 are active.
> No old graph, shard, or fixed point is reused, and no failed
> attempt produced a result. Opposed Copycat/Ghost converged at iteration 52
> but the former verifier rejected 3,462,776 admissible singleton differences.
> A retained-edge witness proved those differences are expected information-game
> behavior, not Bellman failures: after a currently singleton belief, a private
> Ghost move can choose several publicly indistinguishable destinations and
> expand the next belief. The old perfect-information equality assertion was
> therefore an invalid certification contract. The replacement verifier keeps
> exhaustive singleton coverage but enforces the sound dominance relation
> (hidden play may add Ghost-owner forces or remove observer forces, but may not
> remove a concrete Ghost-owner force or create an observer force absent with
> perfect information). The iteration-53 zero-change checkpoint and unchanged
> edge payload are retained and the corrected verification is active. It remains
> uncertified until independent Bellman, dominance, and serialization checks
> pass. For
> opposed Parasite/Ghost, an isolated
> reflink diagnostic completed a full 4,429,716-lower-edge pass with zero
> residual before entering the already-proved exhaustive graph replay. The
> independent authenticated 64-shard remerge subsequently completed and proved
> its six files byte-identical to the first recovered bundle. v4 passed the old
> 4,429,716-edge failure point and completed the exhaustive reload of all
> 492,960 geometries, 37,957,920 worlds, and 441,820,932 edges with zero
> transition or conservation residual. It is authenticating the native relation
> partition before reopening the unchanged compaction-6 checkpoint. No unit
> launch, convergence message, or partial output was promoted as a certificate.
> At 04:40 UTC the corrected opposed Parasite/Ghost v4 replay completed its
> second full 492,960-geometry transition reload with all transition,
> conservation, lower-probe, and observation residuals zero, then failed closed
> before touching compaction 6 because its binary still lacked existing-ROBDD
> reopening. The v5 repair rebuilds the same source with authenticated resume
> patch sha256:0316dac2ad23df78bb6e41101ed6fde0535bbd17faaf2aef1453d16ba7eb72a2
> and open patch sha256:7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af.
> Its restore-verified binary sha256:79f3e9ec4f08e5cfc7df4012e87cb8ad8bb55d8b8a7a08ecf53c61193e948548
> is S3 VersionId `B0FvCeJnftZZfzI2IMqsHPGMXJol2ALd`; the source-pinned v5
> wrapper sha256:44c425d95b488acd7cb2873b57ee41b922761a2437d6b31c6a8d5f54a8609586
> is VersionId `fQ0K3Ga2pJbHwON_WydZfbWuTfRcxthq`. The replacement unit is
> active on i024 CPU 2 and is sequentially validating the persisted dense node
> prefix before admitting any checkpoint root; no result is inferred from that
> launch.
>
> Ten-second hardware-counter samples distinguish the remaining bottlenecks.
> The optimized Devil worker sustained 2.79 instructions/cycle with 22.4% of
> cycles reported as backend-memory stalls, 0.43% L1D refills, and about 19.2%
> L2 refill/access ratio; it is CPU/RAM-latency bound and scales across
> independent shards, not disk-bound. Checker/Ghost and Berserker/Ghost were
> heavier on memory latency at 1.91/2.10 instructions/cycle and 32.4%/30.2%
> backend-memory stalls, while each Bellman phase remains algorithmically
> single-threaded. Their measured resident sets are about 14-15 GiB; the Devil
> workers use about 4.3-4.8 GiB each. Disk throughput is material only in merge,
> compaction, and checkpoint phases, where local-NVMe utilization is explicitly
> gated rather than mistaken for a RAM shortage.
>
> The optimized Devil v6 flat exact-set census now has all **992/992** distinct
> 200-million-state single-Devil receipts (533/262/64/65/68 on
> i03/i0b/i024/i08/i098), zero missing shards, and no duplicate shard binding.
> Its 26 stateless companion/orientation census campaigns are active across all
> otherwise idle CPUs. Each completed task fsyncs its local marker and uploads
> a SHA-bound, versioned worker-log snapshot to S3 before advancing; the four
> pre-upgrade host logs also have deterministic versioned recovery archives.
> These are closure-sizing receipts, never W/L/D completion claims. Stateful
> Devil companions remain **PLANNED** until their extra substate codecs are
> added; all use the documented first-three-ranks/spawned-Minions-only model.
> A later graph-partition audit found that the 992 identifiers partition only
> initial seeds: each worker's breadth-first successor walk crosses that
> boundary, so the retained counts overlap and cannot be merged into a closure.
> The remaining census tails are not represented as COMPUTING tablebases.
>
> i024's 579,184,277,703-byte checkpoint tree was copied to the replacement
> 1.9-TB local NVMe, followed by a zero-residual `rsync -anHAXci --delete` pass
> and persistent UUID verification. Berserker/Ghost resumed from iteration 2,
> Checker/Ghost from authenticated compaction 18 and has advanced beyond the
> former slot-mismatch regression, and the rebuilt opposed-Ghost binary is
> reopening the retained 66.2-million-root graph. Parasite/Ghost found a
> transition-file residual before touching fixed-point roots; its second clean
> 64-shard remerge is certified byte-identical and v4 is reopening compaction 6
> on CPU 2. Opposed Rook/Ghost's retained roots passed the independent Bellman
> equality pass, and the action-conditioned canary converged again with zero
> changed roots at iteration 23. A concrete trace through geometry 246,650 then
> showed five hidden Ghost moves from square 56 sharing one public observation
> and reaching squares 57, 64, 48, 65, and 49. That trace proves a singleton
> belief need not remain singleton, so the former exhaustive equality oracle and
> its 4,341,148 differences were invalid as a cross-model certificate. The
> retained checkpoint is now being reverified under the exhaustive information-
> dominance contract; every failed attempt remains preserved and non-certifying.
> Supervisor severity also retains historical reservations that exceed
> conservative resource budgets.
>
> Both Jester/Angel classes are now **CERTIFIED**. Opposed archive
> sha256:d2337ca267697790ff854744a8de9877a6a733a805b5eb75d293738886c074ca
> and same archive
> sha256:6a9fa1cb3be32b881c1f1d3f68eba84a0ef5221a38c92020c8e2436738f7f7ac
> passed zero Bellman/rank residual checks and exact upload/restore verification.
> The corrected same result retains 9,279,732 draws in its first mover row and
> 5,870,480 in its second; these are not zeroed by reachability filtering. The
> parallel model is bound to deterministic 16-file source bundle
> sha256:20db30fcf7fd2dbc19bbbd1a1a860c3b06765551150ef8b0736b78d15b2dfe97,
> S3 VersionId `XJuPcn2ij3cr2OWIApeI0B9ORQP.Z5WJ`. A one-worker/eight-worker
> K+Jester cross-check was byte-identical to the certified lower overlay.
>
> A fresh online ledger audit downloaded every current reachability object by
> exact S3 VersionId and SHA-256, reconstructed admitted W/L/D from total minus
> explicit exclusions, and compared it with both supervision and this README.
> Its offline cache replay also passed: 66 artifacts, 66 files, zero missing
> configured results, and dense-state conservation across all 446 certified or
> preserving exact rows. Legacy native `reachability side` records continue to
> be interpreted as excluded states; certification rejects ambiguous or
> nonconserving rows. Result mappings cannot certify a row from source-only or
> dependency-only S3 objects, and completed jobs remain version-checked after
> their local scratch is retired.
>
> The suspicious opposing Berserker/Berserker `Win*` result was a real ledger
> bug. A distant power-0 position with only quiet in-class moves supplied a
> concrete contradiction: it and every child cannot all be wins for each new
> mover. The native sidecar's legacy `reachability side` record means states
> *excluded* by the causal predecessor predicate, but its admitted and excluded
> buckets had been reversed during manual import. The exact table remained
> Bellman-correct. Its corrected admitted W/L/D is now
> 262,700,200 / 271,396,830 / 31,227,730 per side and the plot classifies the
> cell as mixed. New native audits emit explicit `reachability_excluded` and
> `reachability_admitted` records, and the finalizer independently checks their
> conservation before publication, so the ambiguous legacy label cannot cause
> this reversal again.
>
> Twenty-six visible one-Angel classes are now exact or actively computing. The
> Bishop/Angel graph legitimately has zero in-class edges; its failure was a
> zero-length predecessor `mmap`, now fixed and covered by a full 1,971,840-state
> zero-edge audit. Its completed output and opposed Prince/Angel were recovered
> with a packaging-only path that authenticates the retained table, proof,
> archive, fresh restore, and S3 download without replaying either graph. The
> finalizer now pins and re-HEADs the exact reachability object version and uses
> S3 `ContentLength`, correcting the former one-byte trailing-newline mismatch;
> every finalized Angel certificate now verifies exact. Same Prince/Angel and
> same Checker/Angel, opposed Sniper/Angel, same Sniper/Angel, and both
> Penguin/Angel orientations have since certified. Same Penguin's 910,990,080
> states and 1,290,571,084 edges stayed below its 40-GiB cap. Both Pawn/Angel
> orientations are now certified as well. Opposed Pawn's 151,831,680 states
> completed in about two minutes with 6.21 GB peak RSS; same Pawn's 227,747,520
> states and 1,549,543,400 edges completed in 319.655 seconds on six otherwise
> idle i03 CPUs, staying near 10 GB cgroup memory. Fleet occupancy reached
> 40.823/160 vCPUs during that safe backfill. Its first launch failed closed
> before creating a work tree because the stage held opposed Queen/Angel but
> not the distinct same Queen/Angel promotion table. Both exact archives are
> now restored by VersionId, SHA-bound in separate manifests, and included by
> the corrected complete fresh stage wrapper
> sha256:7b2f10b734895d48530b031085add04006da9afaa2ebfd8e4292d49d2fe05799,
> S3 VersionId `aJckkhbzU_Kr7r88caeVMd.7sdY8aaqf`; regressions require both
> Queen/Angel promotion orientations and the exact Berserker lower table. The
> failed preflights and earlier partial stage attempts remain preserved.
> Opposed Berserker/Angel completed all 759,158,400 states, including its large
> reverse graph, propagation, and exhaustive Bellman verification, and is now
> independently reachability-certified. Its exact archive is
> sha256:c36ac8cfadbeb6da85de5d565c0f3cf5863f7fab3575656b187004565a5a4083
> VersionId `aY6aIFked6U.IPLCDQlB2hcFyQQUde7q`; generation certificate
> sha256:9c90cadf23aee9e62de9758ce7a8484f8379236a49ca496d5ccca8becfec7efd
> is VersionId `dsoMEPH60BdpBFwtvsleYMyeH6n7eKjV`, and reachability
> sha256:d658b2f4721190292fe7da453f5b8df55bc8b50a19161cc60f1471d9135730ce
> is VersionId `.hFDjJNTqmgy7FfqX9VLhEw6DSPWrNow`.
> The next Angel generator removes a duplicate successor simulation: legality
> filtering and graph-child construction now share the same applied child.
> One million opposed Berserker/Angel states fell from 10.48 to 8.05 seconds
> (23.2%) with the same 40,565,571 edges, and a complete 11,970,912-edge Rook
> table remained byte-for-byte identical. The Angel differential covered 58,390
> transitions, including 600 attachments. The authenticated worker count is no
> longer fixed at four: that same complete Rook table fell from 1.77 seconds on
> four workers to 1.13 seconds on seven (36.2%) and again remained byte-for-byte
> identical. Static contiguous verifier quarters were still badly imbalanced
> for attached Angel substates, so v6 schedules read-only Bellman checks in
> atomic 4,096-state blocks. A complete same Pawn/Angel comparison produced the
> exact same 284,684,464-byte table
> sha256:d1296b7db6862c2306c2f693c22357c49b5fe7d1a3d46cb05aad1b248cfb9741,
> including all 1,549,543,400 edges and W/L/D/DTW bytes, with zero residual.
> Total elapsed time fell from 319.655 to 283.745 seconds (11.2%), and the
> verifier tail from 96.161 to 85.177 seconds (11.4%). The exact source diff,
> both pinned certificates, and timing logs are bound by equivalence certificate
> sha256:f8164bc51f44cc9168241ee66be562567188028fd5076337c794493491c8cd5b,
> S3 VersionId `2B2MYXXe6trzooU.aHXjArJVFgC3WFSd`. The v6 source bundle is
> sha256:7fbebde72e91fa82396826417582200bba2bb970a2941ca7a10032fe265c1b66,
> model sha256:ac737cb47c399bd8cabbd02f191d07c86fb028eab7bb19f5068c61942c521b68,
> VersionId `4Vn2x3lCZzTB4EjhKxI7uNGk6ZV6LwTs`.
> Same Berserker/Angel v2 completed its full 1,138,737,600-state frontier, then
> reached 150,000,000 reverse states before its 64-GiB resident monitor stopped
> it at 68,781,654,016 bytes, only 62,177,280 bytes over the underestimated
> gate. The exact 9,109,900,800-byte node plane and 4,554,950,400-byte degree
> plane remain untouched. An authenticated frontier resume exposed a second
> generic-tool bug before computation: the manifest builder used the ordinary
> Berserker/Angel factor 10×2=20, while the exact same-team graph has 30
> substates because attachment to the companion is a third Angel mode. The
> native generator rejected that malformed checkpoint. The builder now shares
> `angel_pair_state_factor` with UFTB verification and has same-team, opposed,
> and non-Angel regression tests. Corrected builder
> sha256:783183cf2680b89d18e5041749950563433f0eefc7f79a3d76b542ddfe9108ba
> is S3 VersionId `_Xhh5UiVy3qkgGPzOBgnbMiM1OhGFPcu`; the exact 30-substate
> resume manifest is
> sha256:c1544365463bab93fb63388d8565f8d38f7052b8a0b654337fbadd9f9c35b243,
> VersionId `PqmsNcdhyevHRB.mgWi.kz3n6hDJ39FP`.
> The v5 resume accepted that checkpoint and allocated the complete
> 129,941,643,456-byte predecessor plane, but its wrapper failed to forward the
> requested worker count. v6 forwarded `--workers 7`, yet an independent native
> scheduling bug still forced reverse replay to one core: the scan used the
> same `!checkpointEvery_` condition as restartable frontier construction, even
> though a complete-frontier resume necessarily supplies `--checkpoint-every`.
> Exact process inspection found one native task and about one busy CPU while
> six assigned cores sat idle. v5 and v6 were stopped only after their work
> trees and journals were retained.
>
> v7 separates the two policies: a checkpointed frontier remains serial and
> restartable, while the disposable reverse replay may use deterministic
> worker partitions. From the exact same 985,920-state Rook checkpoint, reverse
> replay fell from 2.546940 to 0.681959 seconds (3.735×), total solve time fell
> from 2.998220 to 1.283280 seconds, and both outputs were byte-identical at
> sha256:abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
> with zero verification residual. Equivalence certificate
> sha256:2f9a6672b28210075be1220eb0cc96967f0e457c59b55e4186880c07852de501
> is S3 VersionId `jtcIvswTaH_kedmqmQhC1jLIwGb6LjHb`; the execution source
> bundle sha256:8f9139d08e1769e31bf435a1de4eb8c86aa7cf5713e6836bf18f7116892a166c
> is VersionId `IAasgQeGwjoQAWXgNE1Qas92zl451fQF`, and the exact remote binary
> is sha256:2662cb223b79c8d815276f27e282bb6846c01c0016354857f09c12a185c27b5c.
> Frontier reuse now records the historical frontier model and inventory
> separately from the replacement execution model, inventory, binary, and
> equivalence proof, and authenticates an old bundle using its own pinned
> inventory rather than today's expanded class inventory.
>
> The first v7 service invocation failed closed before creating its class work
> tree because the disk preflight called `disk_usage` on a deliberately new
> campaign parent. Its unit and journal are retained; the generic runner now
> probes the nearest existing filesystem ancestor without first mutating the
> destination. The fully downloaded replacement runner is
> sha256:ea5e7c284c1130613cdc7c1f82d01c8c313d8f62b979d73ebc4f647b93e1d794,
> 39,996 bytes, S3 VersionId `e7JKsAiPsPaIvtDjX8K0zguzYOrZ8r5o`.
> After creating only the empty campaign parent, the exact already pinned v7
> service restarted under its 180-GiB resident gate, 220-GiB cgroup ceiling,
> 200-GiB scratch gate, 140-GiB reverse gate, and 128-GiB free-disk floor.
> Seven native threads used i03 CPUs 3-9 without memory pressure. The complete
> 1,138,737,600-state resume finished its reverse replay in 1,703.93 seconds and
> its verified solve in 3,213.58 seconds, covering 32,485,410,864 edges with
> raw W/L/D 601,560,960 / 508,116,840 / 29,059,800 and zero Bellman residual.
> Output sha256:ffe00f661c4ebff9882af420e6ceb00893a57a30885020496455459847211c3d
> is preserved in archive
> sha256:54236276a8fbd87436bc0637998a4d9f1aef6a5633f146ac8ce328f85b55f532
> VersionId `oIA6f7w4yJtUTiiA9ZpIuVA941EUoSVM`; generation certificate
> sha256:24d09aa2a47c2fa3c16e3dd181870d3fa5272802877174d229bc3d0640e4f75f
> is VersionId `WncHsioVbqsGqZU3E3RNDh1yXgWoar4E`, and independent full-causal
> reachability sha256:1d0722783e5f1bf491e54eb25c27a2b126400eeb54e62ec9198161a330c03d2b
> is VersionId `tZ5QLqYc1KmYDcMQjNKxnDMTs12nd42y`. The supervisor now reports the
> row **CERTIFIED**, exact sources, and no blocked resource. Launch wrapper
> sha256:95829700c202c8fc0221ed530188cff57975b7b928bd48c1e29e92ef4d217002
> is S3 VersionId `NTjl_E7Ca8YP5S5RqeZ3IIEGX6wrCNAR`; its pinned execution
> runner sha256:1bf7afa741c5cfc73543344cc862f70f2a89ba71f9a3d569a6f177f46f697099
> is VersionId `RLts0ceiz0a.xh5AIWIrXp7Qqh.JQc1v`.
> The v2 frontier, the parent-directory-only v3 preflight, the v4
> rejected-checkpoint work tree, and the v5/v6 serial resumes are all retained
> and superseded rather than deleted. The v1 launch stopped before graph
> allocation because its 200-GiB scratch cap was 2.54 GiB below the explicit
> static-plus-reverse allowance; its failed unit, work tree, and run plan
> sha256:1852c2d8c585eca2600246efc5e25d3d1a72403915d10879819e6f920036a0a4
> remain preserved. The complete failure tree and journal are restore-verified
> in archive
> sha256:b70ab50f5e9bda52a2254c096d4af6182e0a1328d14dec94e78a5a5ac6521f91,
> S3 VersionId `jr0MPzkGAI20VhQZMZJ1.ntgJZHJo._i`.
> The launcher also now accepts the advertised one-to-eight worker range; its
> stale local validator had rejected every value above four even though the
> remote runner and CLI already supported them. The complete tablebase test
> suite passes 384 tests with 24 intentional skips, and the native Angel target
> passes its codec, differential, reachability, exhaustive probe, and
> parallel-resume self-tests. Fresh optimized-source
> rebuilds of tracked Ghost and the Parasite lower table remained byte-for-byte
> identical to their pinned artifacts, while their model fingerprints and the
> current Ghost-package fingerprints are now asserted explicitly.
>
> Same Copycat/Angel is no longer deferred. Its exact v10 codec has four
> deployment modes (deployed, attached to the King, attached to the primary
> Copycat, and attached to the clone). A rescue can separate the formerly
> mirrored pair, so the 303,663,360-state main class depends on a distinct
> 37,957,920-state K+arbitrary-linked-Copycat-pair-v-K lower table rather than
> truncating the native topology. The lower codec stores both reciprocal link
> endpoints explicitly; terminal orphan-Angel cases remain closed-form draws.
> Native differential tests exercised 23,595 transitions with 120 split-rescue
> and 88 orphan-draw witnesses, while probe tests reject legacy v9 headers,
> require the two material-specific v10 tags, cover clone-host attachment and
> displaced pairs, and fixed an older probe bug that had accepted arbitrary
> linked pairs as intact mirrored Copycats. The exact model is
> sha256:a04cc96d0b907eec3fa87f3daf6d51738b39923ccff32f9e77195aa2352b30ab
> with inventory
> sha256:1bd9a88cccac9f086b48f010b3e6faec134309efc04fe7ad6d8f085a9abce199.
> Source bundle
> sha256:393507ea88fdec8ee68336927e0166363143d80fb0b494bd78f9950c3c420ad8
> is S3 VersionId `GurygO1.0H3Zwbd.4MwYbuRDIIgVrjhZ`; stage wrapper
> sha256:65c7d02ba544b312560f3ca079af50fe4c339ba6e4f46897a2639e870989fca6
> is VersionId `ebiPxX9cahFYyCaCnMsOe7uoGty5PCQh`, and launch wrapper
> sha256:f663f58a18110b2329b80e708bab6aa0485e37cff27bb587dc2dca9c08c026f1
> is VersionId `izC_zY5ZBy23mDJTeDgvzJdG53h78OYg`. The authenticated lower
> bootstrap used all seven i098 workers and completed in 45.0568 seconds:
> 390,931,848 edges and 8,588,176 / 2,392,288 / 26,977,456 W/L/D with zero
> verification residual. Output
> sha256:e55924a8b97acedcbc9876b95a23837030d5112a35d2b06ec0cb85ff4ab16dd2
> is preserved in exact archive
> sha256:815cbabc73491f874735ec3c4ffd7bea6eef95e3dfbbeb19f9a20d0f75c4317b,
> S3 VersionId `EExdSq2sJnK0GLE1z256zGS39_r4pehV`, after fresh download and
> archive restore. Dependency manifest
> sha256:51385018e8c7ad83c0ad9e357de31a0f9a6cbedeb4b78618ab64905b42bb3fb7
> (VersionId `hlr5e8raVpFcIjbmmFJAtSvBxfFomflp`) binds it with intact Copycat
> sha256:98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb.
> The seven-worker main solve completed all 303,663,360 states in 562.488
> seconds, covering 3,346,729,632 edges with raw W/L/D
> 154,538,824 / 116,861,304 / 32,263,232 and zero Bellman residual. Output
> sha256:c3c37dd46156c4041fdf9576d8f3297583c51e368b5a8cf33f1b4836679d1f0c
> is preserved in archive
> sha256:0694fe873f88c5d6a3faf67f104747e8199e1442b001c0747fcfb93c055e2995
> VersionId `YVJNUdcgSIPcQPf1r0gcRMe_3IVeDIMu`; generation certificate
> sha256:e0069c5abdbd028d1cd0b0554f2d00ace46a0617fbe2e5929173e9aa5129e37c
> is VersionId `iB9C7zKd.r6UKKy7_hhvn6PvdvVDpe4T`, and independent full-causal
> reachability sha256:2a95789f431b2cb1484d79894b1b27a100b6efe5555000bf03f1ca18584b3f6f
> is VersionId `OEK0NgYvRzwlcyWBnRyvHd8kU1kPvQHK`. The supervisor reports the row
> **CERTIFIED** with no blocked resource. Main-stage
> wrapper sha256:a6e04678f287619c8d1666e3b7b21d148576f2ceb5901ca71025e8fc5c5c10f2
> is VersionId `luR6OoVRxQPMuGJ9bCkSK7wp0GLVUmfa`; launch wrapper
> sha256:dc66eb467d044478d98d364ef92cd21f8a8ff3a8033c6b470837b0753ab281eb
> is VersionId `.mntbzg.sYJdJ8n0kt3ICNO_L6_QDOmL`. A launch-time supervisor
> sample labeled the job `SOURCE_MISMATCH` only because its bounded probe
> deliberately refuses to hash individual files above 16 MiB. All eight paths
> independently matched; the 47,447,464-byte lower is now authenticated through
> its small manifest plus exact S3 archive, while the runner again performed its
> own full local SHA check. Finalization then exposed a distinct native audit
> bug: the v10 generator and runtime probe accepted both exact v10 codecs, but
> the reachability auditor's header whitelist stopped at v9. The first audit
> failed closed without changing the table or ledger. The whitelist now
> distinguishes and authenticates both v10 tags, with compile-time acceptance
> and wrong-tag rejection checks. The generic finalizer also accepts an
> explicit 1-32-worker audit pool; this proof used seven previously idle CPUs.
> Patched auditor source
> sha256:584cf62d4fdfaf8ae5cf7f252df97c963c9234283d7f9e16b3724b56e32ecaa7
> is VersionId `km6nlo8Dhy5RB9_vpX6iC0rQP.9A.zbT`; the separately built Linux
> binary sha256:c74caaf90125e131175f7ad581487d63588ca5b94f9f9fcc1bd564dd59e013e8
> is VersionId `hIsg5EDNtF9wh55YrmG3AsjmgvfGqJ9W`, and its stage wrapper
> sha256:c56bd07e035e26e8f66805df7f9be8c387b3ae4bcc94c65ec7587af564bc93a2
> is VersionId `z3pYfdI8uf5sbXAsRgllpfoYuxgaD4Bu`. All three are now exact
> remote/S3 supervision bindings; the original frozen solver binary was not
> overwritten.
>
> Opposed Ninja/Ghost v7 and Queen/Ghost v8 resume authenticated partial BDD
> sweeps with a read-only implication test that prevents proof-only node
> allocation. At 13:10 UTC Ninja had crossed the reused 500-million-node
> boundary, completed iteration 4, and reached geometry 90,000/492,960 in
> iteration 5 with 67,540,236 live nodes; Queen iteration 4 was at
> 195,000/492,960 with 218,102,364 nodes. Both stayed near 10.1 GB peak RSS,
> and their CPU counters, logs, and geometry checkpoints continue advancing.
> Opposed Penguin/Ghost completed all shards and its merge and is in
> Bellman iteration 4; its stale nineteen-core reservation was reduced live to
> CPU 2, releasing eighteen cores without a restart. Rook/Ghost, Copycat/Ghost,
> same Prince/Ghost, and Ninja/Ghost use authenticated compositional merges that
> skip redundant full graph replays. Same Penguin/Ghost reopened the exact converged
> iteration-62 arena under the corrected source-plane transpose, completed its
> independent full Bellman verification without another mutating sweep, and is
> now version-pinned and restore-certified over all 80 archived artifacts.
> Queen/Ghost likewise replaced its remaining replay with an exact
> compositional merge and is in Bellman iteration 1. Pawn/Ghost stopped its
> replay at 1,430,000/1,971,840 geometries and completed the equivalent exact
> compositional merge, but remains **PLANNED** until the corrected Queen/Ghost
> promotion sidecar is available; a dependency-gated, non-running class is not
> preservation work. The two rows previously labeled **PRESERVING** were
> stale: same Jester/Ghost and opposed Bomb/Ghost are both active on i0b CPUs
> 10 and 11, respectively, with exact processes and current checkpoint growth.
> On i03, the two superseded same-Berserker memory-gate retries were archived
> and their replay-only reverse graphs removed after all retained planes passed
> their hashes, recovering 255,301,967,872 bytes. The same Ghost/Ghost solve's
> six-day domain-root initialization was also replaced after an exact cache
> showed only three distinct masks: all 9,739,120 roots now build in 2.66408
> seconds, and the replacement is in its fixed point on CPU 30. The stopped
> 324,035-root baseline remains intact and restore-certified as evidence.
> Four completed Berserker rows that supervision had incorrectly left
> **COMPLETED_UNCERTIFIED** are now recognized as **CERTIFIED** after exact
> VersionId checks of their archives, certificates, and bound reachability.
> The supervisor's immutable remote probe is now zlib-compressed before base64
> transport, preserving identical fail-closed checks while keeping the growing
> job inventory safely below SSM's request-size limit.
>
> The i0b disk warning was cleared safely. Failed opposed Berserker/Checker v2
> and v4 plans, binaries, logs, and resource records were archived as
> sha256:086df8714e681e60f30b8c14f60d1f0c34677d7e8b12d3f771f21d4a763bb42c,
> S3 VersionId `yq.BRN4_7WfdqUviXwFxDULJuo4p5yPj`, and restored by that exact
> version before ten disposable reverse-graph scratch files were removed.
> Their 235,163,754,496 logical bytes shared CoW extents with retained
> frontiers, so the deletion released 33,161,863,168 physical bytes and left
> 347,427,078,144 bytes free, above the configured floor. No output,
> certificate, active file, or unique failure evidence was deleted.
> Separately, the certified opposed Berserker/Checker v5 success scratch on i03
> was independently restored from its exact table, certificate, and
> reachability VersionIds, every archive member was rehashed, and exact open
> file, mmap, cwd, and process-root scans were empty. Removing only that
> 130,977,916,151-byte disposable scratch raised
> `/mnt/ultimatefish-penguin` free space from 74,587,963,392 to
> 205,443,473,408 bytes; all outputs, logs, certificates, sources, and failed
> evidence remain retained.
> Older notes below are historical provenance only where a row's final sentence
> records a newer unit or a non-running status.

> **Historical Devil entry-root campaign (corrected 2026-08-29):** the former
> `single:devil` certification applies only to causal entry roots. Those roots
> may contain starting Minions when earlier spawns by the indexed Devil can
> account for their placement; arbitrary starting Minions are excluded. Its
> authentic lone-Devil C1 (`square 2`) independently verified
> all 3,861,213,174 closure states and 24,608,033,758 edges, emitted 98,592
> roots, and merged with the other eleven restore-authenticated fixed-square
> fragments. The merged root projection explicitly excludes Devil placements
> outside the legal first three ranks and admits starting Minions only through
> authenticated causal histories from that Devil. The result archive,
> certificate, and causal
> reachability sidecar were each uploaded, exact-VersionId downloaded, hashed,
> restored, and atomically imported.  The distinct 27,209,034,909-state
> checkpoint belongs only to `same:bishop+devil`; all Bishop+Devil jobs remain
> paused and were never dependencies or completion evidence for
> `single:devil`.  The checked-in plot remains at the approved 6126 x 2904
> pixel resolution.
> That result remains useful as an agreement slice, but the internal stateful
> key/WDL planes—not this projection—are now the primary result. No
> Bishop+Devil checkpoint may supersede or supply progress for lone Devil.

> **Same Sniper/Ghost normalization repair (2026-08-29 16:08 UTC):** the
> iteration-49 Bellman fixed point passed equality but rejected publication
> after the exhaustive singleton gate found 166,185 dominance mismatches.
> Exact witness replay proved that the reused normalized concrete plane was
> stale: one legal Ghost-capture child is an authenticated lower-material
> draw, while the stale plane labeled its parent a loss.  A fresh exhaustive
> remap of all 303,663,360 states completed with zero remap/count residual and
> sha256:`6250560a341019f4a0f86a2f73b6c52acde24a593241aca0b8e73dba34df0a73`.
> The solver now compares every reused normalized value against the current
> deterministic source remap before opening the retained fixed point.  The
> corrected v14 binary passed its 607,326,720-state self-test, was restored
> from exact S3 VersionId `2MIjvgPR8vbmI.bd93fAJMBvIrl9Hzjb`, and is running
> independent Bellman and singleton certification from the retained converged
> roots; no result is inferred from launch.

> **Certification sprint acceleration (2026-08-29 14:48 UTC):** the original
> opposed Ghost/Dragon solve remains intact on i0b and has reached iteration
> 88.  A second fixed-point solve was started from the independently retained,
> sha256:`679efb649622592a9d5034b56ea101623d169ed1a6f6843a6fa0c223ea029030`
> authenticated transition archive on i03 CPUs `2-15`.  Queen/Ghost was first
> reduced to the disjoint `0-1,16-31` CPU set; its iteration-5 compaction state
> was not restarted.  The race runner is sha256:`900f8dba335e71b4742898e1c5858840c3d4f04160f526d0d21004a4a6f313a6`
> (VersionId `Vov5xcBfIW0p81su9kp9PaJHiU65Ar1g`), the systemd unit is
> sha256:`367b4e95231d912da331d3afa7cb5e4d35c87c8781311b85adc13acc116c52ce`
> (VersionId `GdT4tPzsdWquTOuoGJ7f6mjzYCpFYslV`), and i03 preservation
> manifest v6 is sha256:`1ebb340357f0d8b7192e675bdd01ee73f28198a60767fac31aedc4d7489fb848`
> (VersionId `.DjjxQmT1XaYr9uoEUPkJ58ftPaOKnZq`).  Exact-VersionId restores
> matched all three local files.  The draft's unsupported worker-count option
> failed closed before a sweep, was removed, and the corrected run reused the
> normalized source with residual zero.  Launch and serial transition reload
> are not treated as solve progress.  After rebinding i03's watcher to its
> exact authenticated v2 source, the supervisor reported complete,
> non-overlapping measurements at 175.305/224 busy vCPUs (78.3%).

<!-- COMPUTATION_LEDGER_START -->
Ledger totals: **463 certified**, **0 preserving**, **0 computing**, **47 exact draws**, **61 planned**, **0 blocked**, and **53 deferred**; 624 unique material classes.

| Key | Class | Domain | File | Status | Indexed states | Result domain | First starts W / L / D | Second starts W / L / D | Reachable / unreachable (first; second) | Canonical storage |
| --- | --- | --- | --- | --- | ---: | --- | ---: | ---: | ---: | --- |
| `single:jester` | King+Jester vs King | single | `kjesterk.uftb` | **CERTIFIED** | 985,920 | information v2 | 412,616 [0] (80,344) / 0 [0] / 0 [0] | 3,272 [3,272] / 414,344 [0] / 75,344 [75,344] | 412,616 / 80,344; 492,960 / 0 | S3 information archive sha256:3ebb4a62e4bdc0f43ea08b4fa384c404d1e0b1c36ec3ffa494d3f6b0d1cb674b VersionId 0a6LR_SjcIGVDKoCZxJdlMt7D9dO5cx1; certificate sha256:f54eb05406a552db0878734fdc99947770e0bfdf875effb6f4670c9ec335eb2c VersionId DXfFBdFKRySqiSDcv51UqqkHgE.6h0sv; information trivial v1 sha256:dbeeea12434e7e62c8dc1703ee3bb9246302efd2079bb0ed67c491889f009e00 VersionId QFr9sIVmP38MJfhYmVHfX0JhpyImnrsU |
| `single:knight` | King+Knight vs King | single | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `single:pawn` | King+Pawn vs King | single | `kpawnk.uftb` | **CERTIFIED** | 1,971,840 | concrete | 576,806 [124,802] (101,696) / 0 [0] / 217,258 [2,790] (90,160) | 0 [0] (83,616) / 459,272 [60] / 352,872 [68,464] (90,160) | 794,064 / 191,856; 812,144 / 173,776 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:0abd68aaf766d780bed28d1f383bd241a79855658338b0c671778de8cdece589 VersionId ZYsrVqNUnl9V0b_6Xh2lbKfBDv6q9aQm |
| `single:queen` | King+Queen vs King | single | `kqk.uftb` | **CERTIFIED** | 985,920 | concrete | 306,404 [0] (186,556) / 0 [0] / 0 [0] | 0 [0] (41,808) / 413,304 [0] / 37,848 [37,848] | 306,404 / 186,556; 451,152 / 41,808 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:d6d74ea296391540d16e13162b4a19f51f38ea4f322ca67fde560f01bb250323 VersionId q4p0MBlPwCnrqrV4gJXKjLEPSAV3WeAw |
| `single:rook` | King+Rook vs King | single | `krk.uftb` | **CERTIFIED** | 985,920 | concrete | 361,648 [0] (131,312) / 0 [0] / 0 [0] | 0 [0] (41,808) / 414,300 [0] / 36,852 [36,852] | 361,648 / 131,312; 451,152 / 41,808 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:2b99b8df3756285ce7eed338127b92d5947f18b61f7f106aa3b1ae3c759bc384 VersionId mzoGV1GuvrFJ2jAvxYppMFnVTEgoZCrn |
| `single:bishop` | King+Bishop vs King | single | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `single:berserker` | King+Berserker vs King | single | `kberserkerk.uftb` | **CERTIFIED** | 9,859,200 | concrete | 1,468,376 [0] (3,461,224) / 0 [0] / 0 [0] | 0 [0] (418,080) / 4,143,200 [0] / 368,320 [368,320] | 1,468,376 / 3,461,224; 4,511,520 / 418,080 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:6fd702d3f668d8c4b8ed65e3fd31840b533a1b9a41de4ce321c837923751be25 VersionId _Y98jh2bL4km7DNVBnF7DwTgwO.kZ4XH |
| `single:bomb` | King+Bomb vs King | single | `kbombk.uftb` | **CERTIFIED** | 985,920 | concrete | 394,988 [0] (97,972) / 0 [0] / 0 [0] | 0 [0] (41,808) / 448,600 [0] (1,760) / 792 [792] | 394,988 / 97,972; 449,392 / 43,568 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:00deabc89c80a69e41c636986b9b5c09bb839cac158297e9326f872dbe6dcd43 VersionId 0wiQYMWFBUdCod5DtvHPKJ6o29KY6sxo |
| `single:ninja` | King+Ninja vs King | single | `kninjak.uftb` | **CERTIFIED** | 985,920 | concrete | 356,360 [0] (136,600) / 0 [0] / 0 [0] | 0 [0] (41,808) / 413,376 [0] / 37,776 [37,776] | 356,360 / 136,600; 451,152 / 41,808 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:e18e3f4c672001a89d93559ccded77a01b234a8960b60e84534322153ddbbc61 VersionId YKrYkxR6WuXGKAZJnmpA0aKIiLbYeIho |
| `single:turtle` | King+Turtle vs King | single | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `single:ghost` | King+Ghost vs King | single | `kghostk.uftb` | **CERTIFIED** | 1,971,840 | information v2 | 863,768 [38,536] (122,152) / 0 [0] / 0 [0] | 0 [0] (83,616) / 826,992 [0] (38,520) / 36,776 [36,776] (16) | 863,768 / 122,152; 863,768 / 122,152 | S3 table sha256:11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 VersionId EdXubcuAb7lALuR44pQWvHUW7jCNXTnz; Ghost model sha256:472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb VersionId vDqz0JIv0WdSiCtqX0j5mQpuqPS0rOAY; overlay sha256:010cbad9a68fe81596218bdaa94c96baed4fb62942a328a2e5529e89c6e86f33 VersionId h0XZmP3ODkrkjWxEWKOW.HbeyrvecLT2; certificate sha256:1ddd190bf1681882bd544ea832be4de55506c542be619641c5f464ffabb85088 VersionId 7WhyCs3YqsiAU9pFMkG7LEYDROFfDDDz; information trivial v1 sha256:5ffa1698c54324cfb3bcf6b01649ca9fa8c3a8ad269c55313b385b8f3710a0a9 VersionId pU8D1rjwCgS_7SOC2GPI0adLnK4QKfCR |
| `single:mage` | King+Mage vs King | single | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `single:penguin` | King+Penguin vs King | single | `kpenguink.uftb` | **CERTIFIED** | 3,943,680 | concrete | 1,264 [0] (52,024) / 192 [0] / 491,504 [0] (1,426,856) | 796 [0] (45,080) / 3,272 [0] / 530,700 [78,584] (1,391,992) | 492,960 / 1,478,880; 534,768 / 1,437,072 | S3 archive sha256:7bb7da4d5ee6bbe7cb74fd6d65c5d318e79e4ac41492c424960d3934335d22c7 VersionId n.2zykICTUqovc8HRIl7A_JUBWcOK12t; result sha256:5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd; certificate sha256:a2c9023d0fba7eb60b5daa8a4afbfd76cc649cbf26b3472c94fca170285c4629 VersionId OJUUS1OstRF50J2LZ_o74FLgzbUnyNH9; reachability v3 sha256:8fbaa3ca8952d6e95313715e924efe5ead6e7a1e7baba014923b9a1505b6d686 VersionId b_EAC6ZjKQuOS_70aXudVEQyWbH.lSrh |
| `single:parasite` | King+Parasite vs King | single | `kparasitek.uftb` | **CERTIFIED** | 985,920 | concrete | 412,616 [0] (80,344) / 0 [0] / 0 [0] | 0 [0] (41,808) / 451,120 [0] / 32 [32] | 412,616 / 80,344; 451,152 / 41,808 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9ad18b1d8bc389268f03b27af73dab30711fd71fbc2c518767820eed3636d856 VersionId byjhKm2Sz411VGDXy94mE4wjYudqzLmT |
| `single:devil` | King+Devil+causally-spawned-Minions vs King | single | `ultimate-devil-stateful-class-certificate.json` | **CERTIFIED** | 34,981,631,519 | stateful concrete | 25,120 / 4,444,400 / 8,124,244,746 | 1,271,239,678 / 0 / 25,581,677,575 | 8,128,714,266 / 0; 26,852,917,253 / 0 | Class certificate sha256:95649cf36a9f6287379e9d29ee80b67f7af9c8ca6dff0298e73977f458bd3e0f VersionId vzZ.mORQVSqa.0cfr665BEmMugKyGfav; all twelve primary planes, searchable sidecars, and censuses are exact-VersionId restore-authenticated; complete twelve-square causal closure; historical `kdevilk.uftb` retained only as an excluded causal entry-root projection that admits causally placed starting Minions |
| `single:sludge` | King+Sludge vs King | single | — | **DEFERRED** | — | concrete | — | — | — | — |
| `single:sniper` | King+Sniper vs King | single | `ksniperk.uftb` | **CERTIFIED** | 3,943,680 | concrete | 5,014 [0] (194,552) / 0 [0] / 872,222 [16] (900,052) | 0 [0] (167,232) / 1,210 [0] (1,066) / 901,094 [73,572] (901,238) | 877,236 / 1,094,604; 902,304 / 1,069,536 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5591ad8af93e5596bc029a375d95b549eba9385af0992320f93f7758184433e4 VersionId kUwnb4.rFQYJA7OEQF_zTQQHNkhfEtjL |
| `single:prince` | King+Prince vs King | single | `kprincek.uftb` | **CERTIFIED** | 1,971,840 | concrete | 412,616 [0] (80,344) / 0 [0] / 0 [0] | 0 [0] (41,808) / 414,344 [0] / 36,808 [36,808] | 412,616 / 80,344; 451,152 / 41,808 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:31191d85346d514e2a660d5209b1570a1a2cc3c963a405a59724819c924bd38b VersionId 8mhcEgM2D4WLXQMjy_gDh7WtIbz9xnkx |
| `single:checker` | King+Checker vs King | single | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `single:giant` | King+Giant vs King | single | `kgiantk.uftb` | **CERTIFIED** | 985,920 | concrete | 3,300 [0] (82,476) / 0 [0] / 273,324 [0] (133,860) | 0 [0] (30,868) / 1,460 [0] / 326,772 [43,572] (133,860) | 276,624 / 216,336; 328,232 / 164,728 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:e85e422d0ed3d45011e17128387d6fa0688a0f274afdcc81cc3a801b3c8953c4 VersionId M6Xgt4MhTQMmS7JRunVFaR5krOh_B0Uo |
| `single:copycat` | King+Copycat vs King | single | `kcopycatk.uftb` | **CERTIFIED** | 985,920 | concrete | 372,224 [0] (108,184) / 0 [0] / 72 [0] (12,480) | 0 [0] (40,776) / 374,136 [0] / 65,568 [65,352] (12,480) | 372,296 / 120,664; 439,704 / 53,256 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:58135bbfb34896edb543144d7fa876d1d1559e3dc3a551990d717c6a84646094 VersionId S6uZNa3qRc3gQRsEU_ZP3BpYk_oxy40p |
| `single:angel` | King+Angel vs King | single | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `single:fisherman` | King+Fisherman vs King | single | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `single:dragon` | King+Dragon vs King | single | `kdragonk.uftb` | **CERTIFIED** | 985,920 | concrete | 364,756 [0] (128,204) / 0 [0] / 0 [0] | 0 [0] (41,808) / 414,164 [0] / 36,988 [36,988] | 364,756 / 128,204; 451,152 / 41,808 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5d495cef3eacb8b1c5b0042b4e8444146748024a2b384acf68e8ddd7de4078f0 VersionId fT2ldpVXh8Wx8e5Ye8MVbBAKHA7p6a7o |
| `same:jester+jester` | King+2 Jesters vs King | same | `kjesterjesterk.uftb` | **CERTIFIED** | 18,978,960 | information v2 | 7,259,568 [0] (2,229,912) / 0 [0] / 0 [0] | 122,242 [122,242] / 8,682,564 [1,275,812] / 684,674 [684,674] | 7,259,568 / 2,229,912; 9,489,480 / 0 | S3 raw sha256:31bfe84a VersionId lYyE49iRP0WafZ4oGBBkqBNAbUDrbhts; arbitrary sha256:5da56e0c VersionId y7GdR.MtNb_mLQ_nRgj9L.0r6yFCuZtM; certificate sha256:536632ab VersionId Z07XOZA77U_9fLhNKU_lL3dltu3fBrHh; information trivial v1 sha256:d7701a119b79fa3956b6acfc26a18a35b84a242a24e619ca3fabbac133b24a53 VersionId l7QgxVtjrqn3cMfzZ5mRczrbQKlmwMev |
| `same:jester+knight` | King+Jester+Knight vs King | same | `kjesterknightk.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 14,798,084 [0] (4,180,876) / 0 [0] / 0 [0] | 242,136 [242,136] / 16,055,072 [1,275,868] / 2,681,752 [2,681,752] | 14,798,084 / 4,180,876; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:a4e9bafec28d96e09e5ed42261904c00e1592da794c136f2e291834cf5014df5 VersionId CWhs.vS.odmrKT2vk8zBX169mwpkBf7j |
| `same:jester+pawn` | King+Jester+Pawn vs King | same | `kjesterpawnk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 31,131,656 [4,405,844] (6,826,264) / 0 [0] / 0 [0] | 310,538 [310,538] / 33,723,331 [4,291,402] / 3,924,051 [3,924,051] | 31,131,656 / 6,826,264; 37,957,920 / 0 | S3 information archive sha256:4bb767f1ca306c8f649a4b66075b9a072163646529e0ff596d5dc307789fe44e VersionId rPGSPtqkzyhV5AsK.QA72iscfDLL8QIS; certificate sha256:97cf2f9e63b0271c2a362ee10e675cac10901728b33553e01155218873a9aa1f VersionId jP68epcV9j6hQAbAF.sVHiDKqcgUEg.T; information trivial v1 sha256:e94fb85d4bae7fc53cfa4e7e42afd9cf4569f70ba6ca6e91371817c72a0cd7fa VersionId D4knoIv1Ub1xoptTo_p7wy2lzcWyIOSu |
| `same:jester+queen` | King+Jester+Queen vs King | same | `kjesterqueenk.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 10,877,572 [0] (8,101,388) / 0 [0] / 0 [0] | 611,994 [611,994] / 17,307,456 [2,241,402] / 1,059,510 [1,059,510] | 10,877,572 / 8,101,388; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:8ece6864ce073b5accfc2c8bd06003dd085d94997d44054f16fdc5932dcd40e6 VersionId 1ymgMdtLTBrRJ__4Sxq3f.3uLhHcOCIO |
| `same:jester+rook` | King+Jester+Rook vs King | same | `kjesterrookk.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 12,797,700 [0] (6,181,260) / 0 [0] / 0 [0] | 425,372 [425,372] / 17,360,968 [2,409,632] / 1,192,620 [1,192,620] | 12,797,700 / 6,181,260; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:e2329441143bfabf65f57aba19a8749726800099b562bd7ead8b769d509e26fa VersionId rddooFz5SSpe7ejzUt6JpjZ5LCmuPcpE |
| `same:jester+bishop` | King+Jester+Bishop vs King | same | `kjesterbishopk.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 13,965,588 [0] (5,013,372) / 0 [0] / 0 [0] | 316,578 [316,578] / 16,113,394 [1,253,408] / 2,548,988 [2,548,988] | 13,965,588 / 5,013,372; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:ffbfe24d97d4f4870549f3fdd1fbc2e7de1e7120462960bf3d2050149277f32d VersionId x0b7ttSnEurSLpDN2mIrwz9pjXVgjanK |
| `same:jester+berserker` | King+Jester+Berserker vs King | same | `kjesterberserkerk.uftb` | **CERTIFIED** | 379,579,200 | information v2 | 51,853,836 [0] (137,935,764) / 0 [0] / 0 [0] | 11,458,444 [11,458,444] / 173,641,360 [17,454,944] / 4,689,796 [4,689,796] | 51,853,836 / 137,935,764; 189,789,600 / 0 | S3 information archive sha256:c395d3786f07e2ec55588af306a79545a09fce372cfa6d8797cff4a9a4c1f558 VersionId UPmRhIYEtW6PVlLTVMk15j355ZKz1fhf; certificate sha256:9c076660d1ffaf3f80884916ee7dc30fdb6f06f4cc8cf0ddc95562e2c3d3e932 VersionId j.bcA44wXBOogPSpJYUCS.n1ix7IjfeN; information trivial v1 sha256:2359d97d5eb3cf14b42221c19db2fdb5f56b19bf11144c67872291624e392d38 VersionId R6VhCOTsstAkE0Rjfi7KiRc6aE.t7dvq |
| `same:jester+bomb` | King+Jester+Bomb vs King | same | `kjesterbombk.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 13,901,148 [0] (5,077,812) / 0 [0] / 0 [0] | 309,574 [309,574] / 17,331,368 [1,246,130] / 1,338,018 [1,338,018] | 13,901,148 / 5,077,812; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:30622121919744fb2a46edb7ecb91615198b9577594955da4c81493b167d6e13 VersionId BFNtS0qczVotyY9UpxGgYuF.ueJnLRdt |
| `same:jester+ninja` | King+Jester+Ninja vs King | same | `kjesterninjak.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 12,548,400 [0] (6,430,560) / 0 [0] / 0 [0] | 446,484 [446,484] / 17,313,736 [2,393,936] / 1,218,740 [1,218,740] | 12,548,400 / 6,430,560; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:53cbf9f2bc6a4ca9d5a10c93aaa80acf9edc5acc84da30b1691a8af7223953b6 VersionId Vl5AkbpkoFmywdZxeMnqtxzTzC4CDdp3 |
| `same:jester+turtle` | King+Jester+Turtle vs King | same | `kjesterturtlek.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 15,158,912 [0] (3,820,048) / 0 [0] / 0 [0] | 189,426 [189,426] / 16,006,830 [1,247,388] / 2,782,704 [2,782,704] | 15,158,912 / 3,820,048; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:c211150d5cc717cc7e9c9b764d8faf4e519dae91f6dca6db9ece3c26b3de20da VersionId mvQGOXQGkw0hqOKKaHYlUGMBzSBk2iV9 |
| `same:jester+ghost` | King+Jester+Ghost vs King | same | `kjesterghostk.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. The fresh transition build remains retained with zero codec, action, decision, transition, and symmetry residuals: 38,450,880 raw states, 9,612,720 canonical states, 37,957,920 worlds, 480,139,352 edges, and payload sha256:286100c2704c36592127d494606bc94241bea8f66eb9e2708734dd1a10a61a15. Exact lower-binding unit `ultimatefish-info-kjesterghostk-lower-binding-v2` is currently inactive and intentionally paused for the certification sprint; its authenticated 110,149,313,248-byte solve checkpoint remains retained on i0b. Before pausing it passed the complete 9,612,720-canonical/37,957,920-world input preflight with source, lower-Jester overlay, singleton-Ghost, and transition residuals zero and entered the 1.5-billion-upper/500-million-lower-node solve. Wrapper sha256:4f6d315683d940c83870ce47ca9aa8650e938a4bba19b8cd5bd3c09354941ffc is S3 VersionId `3SKu6Hgsoyd0yvIXMW8qErrXUR385GeP`. |
| `same:jester+mage` | King+Jester+Mage vs King | same | `kjestermagek.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 125,972 [125,972] / 15,952,244 [1,247,388] / 2,900,744 [2,900,744] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:d0adc51693eda8b87b03efdc0bbdce79401205b109637da6f9f2716bbc591be1 VersionId SMfOzVcmgxAVLLidiso.rWfV.gk1HCJG |
| `same:jester+penguin` | King+Jester+Penguin vs King | same | `kjesterpenguink.uftb` | **CERTIFIED** | 303,663,360 | information v2 | 20,261,420 [0] (3,628,184) / 620 [304] / 304,392 [7,900] (127,637,064) | 140,840 [140,648] / 18,789,824 [1,250,700] / 5,263,952 [5,008,896] (127,637,064) | 20,566,432 / 131,265,248; 24,194,616 / 127,637,064 | S3 information archive sha256:d98cd7fbb300e296e720c9b578c416b4ca90a1dde8a277ef4ba330581429698a VersionId pq2Jmb5sSgTvPXWWurZQRG139liTZF0K; certificate sha256:61cd4c94f72ca03471f41af5e3c9da65be8252f644d3f5cf5d10507d5df5791b VersionId 1fk1aIBq1KAL0Az_0kKCxr3izxrYL1hE; information trivial v1 sha256:d7da893eb0f79dc3fc1ae1fe2fff7cd7c64aa045591414175636d5c3b2db0614 VersionId t14qJSE3IOI2npR92OyZedBVYpJeBNyL |
| `same:jester+parasite` | King+Jester+Parasite vs King | same | `kjesterparasitek.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 14,519,136 [0] (4,459,824) / 0 [0] / 0 [0] | 247,372 [247,372] / 17,365,128 [1,304,236] / 1,366,460 [1,366,460] | 14,519,136 / 4,459,824; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:4d5bb54e5f6f75d94b19723bd12a17fc52ab448ac3fd2983c533236ccf7f4d78 VersionId rXuQqWspUdy0ie7AWN27KQOLyUG415Jx |
| `same:jester+devil` | King+Jester+Devil vs King | same | `kjesterdevilk.uftb` | **PLANNED** | 45,549,504 | information required | — | — | — | — |
| `same:jester+sludge` | King+Jester+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:jester+sniper` | King+Jester+Sniper vs King | same | `kjestersniperk.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 62,681,510 [0] (13,234,330) / 0 [0] / 0 [0] | 672,896 [672,896] / 63,972,952 [5,020,294] / 11,269,992 [11,269,992] | 62,681,510 / 13,234,330; 75,915,840 / 0 | S3 information archive sha256:9d0a4e86e4c89a4ca0bdd6278a9d15b6e1fe7bacb827a36274c9b96dd945e049 VersionId z1_Oo_glS2gZhrVqVtPvYW3gepFCJ5Rn; certificate sha256:5e4ad08a96bcf14bbd29106f1aa95bacbc6b6c2e0e9ec228819697add48cb4fc VersionId Nu4ggz5QU7a6PH7cerh4KqeUHQ5O0AlS; information trivial v1 sha256:6df81f87e621f60b184e86bd38b861c7f7614e78abc88f3c96a682c7c1b03cdd VersionId Lm5PTtbQU7Mui2yIJb0Bfj2_rUuBcTUK |
| `same:jester+prince` | King+Jester+Prince vs King | same | `kjesterprincek.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 14,519,136 [0] (4,459,824) / 0 [0] / 0 [0] | 247,372 [247,372] / 17,365,128 [2,551,624] / 1,366,460 [1,366,460] | 14,519,136 / 4,459,824; 18,978,960 / 0 | S3 information archive sha256:5666698d3453fa11c60e53d605f7e9c09e75d3b511f744f79668139bba9a5b35 VersionId Qg2z7jtVMRXrkDmKcaXBkFV5JAO0bfCb; certificate sha256:7bbffbf28d14867cd5ccb9ccbd90f0747770d2f9e724d6f339714c2d29a3369c VersionId Lykqllu9hBZhtnMcP0dR0VCbBx4cv7wu; information reachability v1 sha256:d3f72751471ef7493969388fab238d658ec8d0f7bd2dd330b1aa1619a3dff5c1 VersionId ZKD3_WipK76T0bV4P0EaeNoJd7GODfO. |
| `same:jester+checker` | King+Jester+Checker vs King | same | `kjestercheckerk.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 31,055,752 [0] (7,744,856) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 326,443 [326,443] / 31,972,189 [2,494,776] (6,902,168) / 5,659,288 [5,659,288] (31,055,752) | 31,055,752 / 44,860,088; 37,957,920 / 37,957,920 | S3 information archive sha256:4be02ec360751350f94e54cf327e44fb65df02957d303787d1e87a8cad4b61ba VersionId x8iSCbqmyQKVL9Z3lHSgQIKUlgKqjb0d; certificate sha256:7107f8a80d6edb0707fd80309310a557477d021df48811f252c8b2340cd4ad1f VersionId TU9P5HDbtvHiB1_Uf09Pa05GKafK1MLg; information trivial v1 sha256:7327e00dede1963059ab2ed6a7d9ca6f8da7cf985ee5b3bd936a4d14d87994ad VersionId GPqa.4vPeCpiUsXKkPPAOZ5aZlJjD2NL |
| `same:jester+giant` | King+Jester+Giant vs King | same | `kjestergiantk.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 9,347,536 [0] (3,939,164) / 0 [0] / 0 [0] (5,692,260) | 260,316 [260,316] / 11,300,932 [1,534,228] / 1,725,452 [1,725,452] (5,692,260) | 9,347,536 / 9,631,424; 13,286,700 / 5,692,260 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; result sha256:36fce15f18bf2d78f16e9599dc351b2cecf66b9147ccb315eb1e10af364ffebc; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:2ab293a7b6ad0c42c1ace0fa8ce0ebf11584c3deabe1517011204a2671409169 VersionId mSoyYbNqQI.Axfk36YALlFyTuFnMs_vj |
| `same:jester+copycat` | King+Jester+Copycat vs King | same | `kcopycatjesterk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 25,893,408 [0] (10,623,072) / 0 [0] / 0 [0] (1,441,440) | 657,112 [657,112] / 33,403,112 [6,757,336] / 2,456,256 [2,456,256] (1,441,440) | 25,893,408 / 12,064,512; 36,516,480 / 1,441,440 | S3 information archive sha256:313e11fb19d643b072e92acb54d11ccddc7b92692f6c2b44fc489ebf7c7338a3 VersionId milQX8s6nAizS48Gm8lhgc3HBanXVtD0; certificate sha256:37f49d86a01e58f3987436b3852083add9070e3c67dc171a6f5e90c8263eeb0b VersionId YzeeGeGa9wyn9wAKG8U.bJrhMx8TaAI6; information trivial v1 sha256:de1b42e19d60582b051d18b68edcd6e079318e9484621da33438ce4214d469c7 VersionId G.5WQd5l1JpT_Lw49gmDgep0bPmmCDE. |
| `same:jester+angel` | King+Jester+Angel vs King | same | `kjesterangelk.uftb` | **CERTIFIED** | 113,873,760 | information v2 | 47,657,148 [0] (9,279,732) / 0 [0] / 0 [0] | 251,944 [251,944] / 50,814,456 [6,354,980] / 5,870,480 [5,870,480] | 47,657,148 / 9,279,732; 56,936,880 / 0 | S3 information archive sha256:6a9fa1cb3be32b881c1f1d3f68eba84a0ef5221a38c92020c8e2436738f7f7ac VersionId V343loE_urhguZR1JTE12lZZAla.TS00; certificate sha256:e96aeab184afaca44577414eb293f48062bec8de47e6996dfa0154ddc7046513 VersionId hAWYLibnjGo1D5kswkBQ0bQfoelW987i; information trivial v1 sha256:0604ebf0c3d0075fdf98de05182fb314c87fc5556b33e14710b0bb189754556d VersionId CZo1MpRfVSRLdIrvKFojxdWVkOchmPC5 |
| `same:jester+fisherman` | King+Jester+Fisherman vs King | same | `kjesterfishermank.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 125,972 [125,972] / 15,952,244 [1,247,388] / 2,900,744 [2,900,744] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:2f1a04e5fd90f92d35adda3680dd085b63d2796c646afcd0e293943af67e60b4 VersionId n8_LGfoESs05cHdDdIZzdngIR3DRoxq. |
| `same:jester+dragon` | King+Jester+Dragon vs King | same | `kjesterdragonk.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 12,877,956 [0] (6,101,004) / 0 [0] / 0 [0] | 431,414 [431,414] / 17,350,088 [2,416,786] / 1,197,458 [1,197,458] | 12,877,956 / 6,101,004; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:97e021b2a0e245d43f3f63dd08220d41fff3790768b23da59ba465654d65b157 VersionId juWFIMl6EyqbvLiVA5CHAldsTo8AcfYh |
| `same:knight+knight` | King+2 Knights vs King | same | `kknightknightk.uftb` | **CERTIFIED** | 18,978,960 | concrete | 360 [0] (1,964,462) / 0 [0] / 7,524,658 [0] | 0 [0] (804,804) / 68 [0] / 8,684,608 [1,272,198] | 7,525,018 / 1,964,462; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:d7d403f8eda87beea141a5d64e1ab705e535186474db7e8c902893c6f73cbb4a VersionId oXxETg0t5qQXM5kCMEomFE2tXTzBq51d |
| `same:knight+pawn` | King+Knight+Pawn vs King | same | `kknightpawnk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 27,144,042 [4,428,933] (6,260,188) / 0 [0] / 1,324,696 [58,940] (3,228,994) | 0 [0] (3,219,216) / 25,739,111 [1,999,338] / 5,528,433 [3,118,658] (3,471,160) | 28,468,738 / 9,489,182; 31,267,544 / 6,690,376 | S3 table sha256:822a9a16902d55a14bc6f655dd000a71f0312bd748dcac8f3f77861428b498d1 VersionId HTHCmCRpRLSfKzbUHDlRNAOcM81vf43y; certificate sha256:24fd254e87fafcff00fcdcd1fa261ead6c312efb55cc32c7afa96837f76d29d1 VersionId fpMpHBUO1m5J98opuxGExZns.bPUeh1W; reachability v3 sha256:3aef38270b34b16a22fd87b5e20d44fa7da5e3f4c500530d6b769f4abe7bff5d VersionId vPqKz2PXDAtnp.nDUpzrLac1SvDKKXLs; result sha256:a1fa8d3eefe2d3244fe223721014b5c59b43cf0c80a129f2d724380b8623490f |
| `same:knight+queen` | King+Knight+Queen vs King | same | `kknightqueenk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,144,698 [0] (7,834,262) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,991,680 [965,646] / 1,377,672 [1,377,672] | 11,144,698 / 7,834,262; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:92d5b7335039ff57c54c597d3c1fb8e661c940cfd783ae86f212384ba110db89 VersionId QQs0QN02HyHk6jWoekOPkkCRpXChD_Xw |
| `same:knight+rook` | King+Knight+Rook vs King | same | `kknightrookk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,069,652 [0] (5,909,308) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,041,948 [1,117,060] / 1,327,404 [1,327,404] | 13,069,652 / 5,909,308; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5fe9881eb903468f60f509ab6b8ebd2987834a985566750a9eeb687e92819f4b VersionId D_F9T6SmRDjSu9ef2m9XtVHhsSfOrw0O |
| `same:knight+bishop` | King+Knight+Bishop vs King | same | `kknightbishopk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,199,500 [0] (4,733,914) / 0 [0] / 45,546 [0] | 0 [0] (1,609,608) / 14,751,040 [0] / 2,618,312 [2,503,582] | 14,245,046 / 4,733,914; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1e6a838326ebf5e624d02c29dc4273e7f75aadfa043fa968952e09fc8dcb2cb8 VersionId z1GF6QqoTUakNr_w9l0exU7XqFztZxdt |
| `same:knight+berserker` | King+Knight+Berserker vs King | same | `kknightberserkerk.uftb` | **CERTIFIED** | 379,579,200 | concrete | 52,880,628 [0] (136,908,972) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 160,497,732 [4,441,064] / 13,195,788 [13,195,788] | 52,880,628 / 136,908,972; 173,693,520 / 16,096,080 | S3 table sha256:224fcdf06bae1dde98e85f33826d25d44c0a208bf8c2cb4222778df9373051ff VersionId lE64lNu6hk2.aJcOcSiKygMe7ZMur869; certificate sha256:f2661513a143932909bcf1ca3c387aeb003b29b6950492aea1747f6849ac5780 VersionId yfcaEKHoCWmdRLHrCGVSUzUDQO9NU0EH; reachability v3 sha256:63d78020b73ce17f3abeb0d94cdd06cf4d39571325626953be5c6c69cdaeda7b VersionId hrDWqYcZSmGhBb.SuzVjwt_CHb37_LK8; result sha256:c389380f6b2606721a30d44d57c01d76182d4bf4bb08eaf01bf85116c01b5689 |
| `same:knight+bomb` | King+Knight+Bomb vs King | same | `kknightbombk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,179,200 [4] (4,799,760) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,269,780 [1,244,268] (67,760) / 31,812 [31,812] | 14,179,200 / 4,799,760; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:c4e0460780698d2435db94e3cba0c099c2bef7e7a1c378a0e586b7cb1ca8aaf0 VersionId TsTqtMlCjtMuIvzMzKB45EARFj6Zfi5x |
| `same:knight+ninja` | King+Knight+Ninja vs King | same | `kknightninjak.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,797,612 [0] (6,181,348) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,000,796 [1,118,180] / 1,368,556 [1,368,556] | 12,797,612 / 6,181,348; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:4de6aa094d84884232fae8ef9d5c4e70e38a1d554e7327af26b764a12d8ac8de VersionId FPY2IvQFA1HjVpK29Kfm2A.FmpTtLdY9 |
| `same:knight+turtle` | King+Knight+Turtle vs King | same | `kknightturtlek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,093,700 [0] (3,538,608) / 0 [0] / 1,346,652 [0] | 0 [0] (1,609,608) / 12,261,896 [0] / 5,107,456 [2,626,112] | 15,440,352 / 3,538,608; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:6f32d7333b8bdac1534ff712e028080ebc57bbc3eb35b0fd250ad2d1bc26a2a8 VersionId lpDvuO.t9.C0zvm9rgJAfnDdhj.IzuQE |
| `same:knight+ghost` | King+Knight+Ghost vs King | same | `kknightghostk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 30,968,084 [1,371,916] (6,989,836) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 1,312,168 [1,312,168] (1,309,208) / 31,942,900 [2,641,532] (174,428) | 30,968,084 / 6,989,836; 33,255,068 / 4,702,852 | Exact 41-iteration corrected-rule solve from source sha256:56ad1b4479a5e6fff7aa8defa02a82de17ab8f117ccba8f23fcf58f8584175ca, normalized source sha256:117f0f8595fca8c5fa3b3d83de81d2eb067c7c3cf31dc7e9f718d2782daf5a62, model sha256:08bc4db46666b7524b54fd6a9725a13a291cc6c1cf64b463369a141994d5e171, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Information output sha256:8327c3d551e6285b4378db8baa1b87c46974dd002f47ed312cef0db809006754 and arbitrary sidecar sha256:6a10b52e4f6eed6868c6cdda5063403543f5bd0b1e292acd062267c1a94c45d9; Bellman, rank, monotonicity, singleton, compaction-root, grouping, conservation, dual-force, structural, and source-remap residuals are zero. The pre-binding manifest is retained; its rebound manifest passed exact source/model/observation/lower header and inventory verification. S3 information archive sha256:0b0781483d92868e6554c58f5bd092b539d7824c5b33a100067ead551fce9b8c VersionId `v048Lj26s1xzut93PD2whHk431Tg.z0D`; certificate sha256:6b526615a11725d94eab042e9b6fc38f38be8213e53924087a139b45c663c840 VersionId `SsEacPBC.brfcGEdx60nnItKmMZYqx3Y`. HEAD, exact-version download, inventory, local restore, and S3 restore residuals are zero across 70 artifacts; source and superseded evidence remain retained.; information trivial v1 sha256:e9d6b89e9cb9cf7add21268183e79e1dd421d05bc0c583c4c8a0c337f45c9f45 VersionId TXY52adY4c1nbM_kYFTCQ5Fif2xBoO2a |
| `same:knight+mage` | King+Knight+Mage vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:knight+penguin` | King+Knight+Penguin vs King | same | `kknightpenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 110,448 [0] (3,923,778) / 9,014 [0] / 19,147,574 [0] (128,640,866) | 34,258 [3,914] (1,881,536) / 256,896 [12] / 22,021,926 [4,564,334] (127,637,064) | 19,267,036 / 132,564,644; 22,313,080 / 129,518,600 | S3 table sha256:f19df2d526d6afeab2f8208fd95fc7e395bb99a746c529d9affee8a0217c27e1 VersionId dKxogVY3k0TW9XE0I3Byfhj66CTax5SD; certificate sha256:5bc295875b4953d29198dd80c1d007017740928c7004c7702415a1b476a7d29b VersionId NTJ_tOQcO9E0KGDALmUHRUF.ZEkimea1; reachability v3 sha256:c42c8f6ae52605f19d87b873f7177fc4f8c0ed98c28c4055d92a2ce61507ea5c VersionId seQi9.vK0xCbyqqSXpUQKKJklytlAW35; result sha256:b3684bbafa918bd7ad79573b6c153850ed0aa115ffa124059bd72110ff27262a |
| `same:knight+parasite` | King+Knight+Parasite vs King | same | `kknightparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,798,084 [0] (4,180,876) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,364,220 [1,304,236] / 5,132 [5,132] | 14,798,084 / 4,180,876; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:a71dcb78c22fb3c2d6895c55cf06034557d66d57bfca1464c4d74a09c2824906 VersionId n9tHFnIH4LHPdt_vdYBK.rJ60DKWc3GA |
| `same:knight+devil` | King+Knight+Devil vs King | same | `kknightdevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:knight+sludge` | King+Knight+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:knight+sniper` | King+Knight+Sniper vs King | same | `kknightsniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 11,922,549 [0] (24,251,031) / 0 [0] / 19,552,645 [0] (20,189,615) | 0 [0] (6,438,432) / 9,835,988 [9,609] (9,831,108) / 24,902,716 [5,227,171] (24,907,596) | 31,475,194 / 44,440,646; 34,738,704 / 41,177,136 | S3 table sha256:d551f94e4f5d50b0bd486aa43ef8377355d06c107cf7f6885d28ccfd0e6fa0ca VersionId C1nq6qpMlYJKCrxy6wtYf3zNtiMt.T6w; certificate sha256:ffdbc6c0f11ef21f3231d7dca80bfaf108a207630e9544d0d909b593c92e8051 VersionId jr0p_5WSen5Ns_GxWQvGB85oarslR2XA; reachability v3 sha256:8a8b31505f152dcd8bbbd55112b31befc0a0a428966d3428cd04a9aced852a87 VersionId Ll4OyyK0gqtCAqYMQuv0pTFwFsTYV72v; result sha256:e570493039b0f839a9db5d9cd1ca2fa9437f07154a57536875029aae973947c4 |
| `same:knight+prince` | King+Knight+Prince vs King | same | `kknightprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 14,798,084 [0] (4,180,876) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,055,072 [1,275,868] / 1,314,280 [1,314,280] | 14,798,084 / 4,180,876; 17,369,352 / 1,609,608 | S3 table sha256:ff772d1abe4db670a21485e446cc791a854afd1ee8eee4dcb939efeb11f84acf VersionId J_pjk4CnpfljQq5WJpEEgPrvEV5jo5JK; certificate sha256:fe148b785a2197a674b86f5a08de1f82d4fd44c9f620411d5ed0046197ac297a VersionId nzYEHSRk4B.yUZPZc2DWIY39N6tVOo.9; reachability v3 sha256:3669b59aa73e9043c90ac27ab52a4ab38735dc1a226d29918232be0b4911a5a7 VersionId _MIw_brCN5FOc05OddY5wtGVkAe0wO8A result sha256:b7ba0c5a2614ec6ff2db5f2b9744fcb0f40a276557c343befbc2d43a8e35105c |
| `same:knight+checker` | King+Knight+Checker vs King | same | `kknightcheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 842,688 [842,688] (6,326,064) / 0 [0] (3,153,552) / 30,017,359 [0] (35,576,177) | 0 [0] (3,219,216) / 0 [0] (6,326,064) / 33,003,124 [5,014,439] (33,367,436) | 30,860,047 / 45,055,793; 33,003,124 / 42,912,716 | S3 table sha256:bb3cb223021378861d1217a26da97bfac997d35b2a3aba8843f27d4dd3e65f41 VersionId 8EMpS4__J6eSRqr86zuh4L45UYApSkMo; wave certificate sha256:0aafe01c90850536a616274e10269ec2e89454521054ccf9ac531eae8aa20bf3 VersionId ._hiPZGaD3TOyhBisXbSllIPqa_2nlgD; reachability v3 sha256:2d5bd845153bcb1bfb76ad15769df60e41f63ca79dc52aba46dd66ab9f46d2b4 VersionId WPQhJN4cABp4.x3tKdYarENoY1CK7bEQ |
| `same:knight+giant` | King+Knight+Giant vs King | same | `kknightgiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 7,589,222 [0] (3,665,112) / 0 [0] / 2,032,366 [0] (5,692,260) | 0 [0] (1,142,116) / 7,416,466 [4,796] / 4,728,118 [2,374,612] (5,692,260) | 9,621,588 / 9,357,372; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8e2113e33a203bd6dbd4e2b116e54516462c6fb6f1c9cde599ab96312992a9f9 VersionId E9uA.CqfC7BX7T76djKyKwX3v7OAAZCa; result sha256:190eeb250445920f4027a7d8a20421c5200b26bd44972d94b5a489d246a87f32 |
| `same:knight+copycat` | King+Knight+Copycat vs King | same | `kcopycatknightk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 26,419,624 [0] (10,088,568) / 0 [0] / 8,288 [0] (1,441,440) | 0 [0] (3,098,976) / 28,751,160 [2,242,800] / 4,666,344 [4,642,560] (1,441,440) | 26,427,912 / 11,530,008; 33,417,504 / 4,540,416 | S3 table sha256:ebfe222bfc049f275723a3b1081a02897f9890d31afae60665fcdf1066e55f3a VersionId 6sM3UASG7icQ2h9Vz39bW7hLzryR1xWH; certificate sha256:543642902f72d733740d37eb8905fc45981fcdd675c6f4acc74f529b72c65544 VersionId 2_eemCbVzERMiFuH97UfIPaKd2oKqoR6; reachability v3 sha256:9e25180f1ca516c29c25ae6cc03c9e9df1cd23cf72273863bd65693d32991c94 VersionId EtidhXuRRGHbqpIjVxtKfeHa3YdFVnxc; result sha256:d9739280d824e405b664bbeeea2911233aca9d336ff90671d9260271d1392486 |
| `same:knight+angel` | King+Knight+Angel vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:knight+fisherman` | King+Knight+Fisherman vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:knight+dragon` | King+Knight+Dragon vs King | same | `kknightdragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,125,078 [0] (5,853,878) / 0 [0] / 4 [0] | 0 [0] (1,609,608) / 16,039,998 [1,078,918] / 1,329,354 [1,329,346] | 13,125,082 / 5,853,878; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:7c67cd2d7b4381598876b92fa6edb879b49a37964b962a8b1d32b85a2d12474f VersionId p5VOEx_OjQBQBaeX.URKPzxrwVwoPqjh |
| `same:pawn+pawn` | King+2 Pawns vs King | same | `kpawnpawnk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 26,297,859 [7,932,270] (8,546,010) / 0 [0] / 562,073 [36,160] (2,551,978) | 0 [0] (3,219,216) / 25,844,400 [3,171,391] (3,140,228) / 2,259,284 [1,317,735] (3,494,792) | 26,859,932 / 11,097,988; 28,103,684 / 9,854,236 | S3 table sha256:6bebddd1c4f53f8ae74792b4b150119a84d4c8910ab457b10b629ac6c5d20a05 VersionId tswynE0N3ScviqSb9AlDufSFVImnNeDk; certificate sha256:e33c21aa1d5aad2d60d015c967fe5fba543b871cb1c16dac80634501317e0991 VersionId LyQPBf94bSyqs7SMv1tLsRrQEZGLpJsZ; reachability v3 sha256:cbd02af7001a80c0a2fe27b3a15909ffb84ebe81ebbff56c50c0ca1e52e93555 VersionId 0Fq07Jw2rhHyFXesFKQeC6FGJ.ZXLN.9; result sha256:6ea0b2c441b9abf87b7795018ab9587d924a59625006f66ca70e0f09d39e570a |
| `same:pawn+queen` | King+Pawn+Queen vs King | same | `kpawnqueenk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 21,031,966 [3,282,889] (16,925,954) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,456,915 [4,593,474] (3,177,968) / 810,629 [810,629] (293,192) | 21,031,966 / 16,925,954; 31,267,544 / 6,690,376 | S3 table sha256:f2ff441302e1734cfee7ac47d812c4df6d6df906135001218a2ab2ed56542c83 VersionId 1Mtu61_AVALHf1S1BpUEn12JafmvPyeb; certificate sha256:c57340cf21aa0fcb921ccdd5902184a67fe0d395612238d37ae11ddd7533178e VersionId 0FVsAKyhHVJLp33Colt__YCrOFHQ7KZM; reachability v3 sha256:f0b393c506d88493e581feaa1194ddb1fccc3b84630e7b1e30a0cc640e710220 VersionId GOPOa8QKARXuqP6EftlKGUlqtPnvRa4S; result sha256:3c4fcdc596ba06b4b52fbf3acc16a64876e2a66ec9b12600541f20e32c077680 |
| `same:pawn+rook` | King+Pawn+Rook vs King | same | `kpawnrookk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 24,708,550 [3,877,953] (13,249,370) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,532,459 [4,503,846] (3,185,418) / 735,085 [735,085] (285,742) | 24,708,550 / 13,249,370; 31,267,544 / 6,690,376 | S3 table sha256:17aee4f99a2ad9099f2a3f94c21a75ce4ea05944d7b788ae68843d5081e53c72 VersionId KV3HcJSoVGUFF7tukLEOj5NJOBP0EVQ9; certificate sha256:8bfc44d6561152ac090caf36a614c3486f12dfe3ad3be32918e768818418d041 VersionId cmmH8po.OfTPMGytAPEDhcdFKu3bwjDt; reachability v3 sha256:7423552b72e5a7d8cc49469fe197538a2ccff1d404306b4741c11cb65be8b4eb VersionId B5iWy7E6DTkrwSSrpfVo0KHePcojPgQU; result sha256:9d29e1d0f00c5c545bcb6e425ce3d9ad11fa2e8f7e8e74935f4df258b8679efa |
| `same:pawn+bishop` | King+Pawn+Bishop vs King | same | `kpawnbishopk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 25,729,337 [4,198,093] (8,019,984) / 0 [0] / 1,165,543 [34,135] (3,043,056) | 0 [0] (3,219,216) / 26,530,127 [2,045,064] / 4,737,417 [2,995,572] (3,471,160) | 26,894,880 / 11,063,040; 31,267,544 / 6,690,376 | S3 table sha256:6a46aa4768f7ad7b412676d342cf5cf1a9cb9eab5a96d223f572cac922cadd2f VersionId fTT.7NdgmM6nJNCj9yoBmDHDtGuA6ABK; certificate sha256:e953d7910d650d9c66034e7cdfec515de563576494b0b819b1b0cdfd5419d003 VersionId PLUfJVNCP_BLpkcRMxrcB8xMuP5eFVal; reachability v3 sha256:3aaaa437791caa45344cb8bd8b99209f345be7903f9cbf59e57019ae89b090df VersionId WjL9tKLQovk6VGYWAgDcc6uwEMHJ0xIJ; result sha256:62e6e742b4d594946bce257c5bd3f800ceaf4f73295d934ba62bfa6be1e60007 |
| `same:pawn+berserker` | King+Pawn+Berserker vs King | same | `kpawnberserkerk.uftb` | **CERTIFIED** | 759,158,400 | concrete | 99,694,388 [15,446,671] (279,884,812) / 0 [0] / 0 [0] | 0 [0] (32,192,160) / 305,345,192 [28,775,321] (31,855,902) / 7,330,248 [7,330,248] (2,855,698) | 99,694,388 / 279,884,812; 312,675,440 / 66,903,760 | S3 table sha256:6b30ff7bb044adfd08bc0e6aa3f76b9792db0f59121ed8f97e749f6a36c6f5a6 VersionId mvSaqUcKIB61bjkrZMJBrl8TeM4ODtcX; certificate sha256:42f3457dc31943715af4010060a088ef62f5cdb8fbcf351cc8319872b9d37301 VersionId x9tF00tcp_c02U3e3Azgwsxw_akfVSoK; reachability sha256:774382c1a5d178db2a9d67ffa23ad1649f3b3377aa1ebf72f9e3803405711e5c VersionId bkIj.UoSsRA6AZsxAemoxI0y7yyt3zcz |
| `same:pawn+bomb` | King+Pawn+Bomb vs King | same | `kpawnbombk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 26,783,928 [4,217,438] (11,173,990) / 0 [0] / 2 [0] | 0 [0] (3,219,216) / 31,088,884 [2,848,298] (3,586,976) / 56,952 [56,950] (5,892) | 26,783,930 / 11,173,990; 31,145,836 / 6,812,084 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:7ae58be48e6e9b22573428469816ade8d604c6446c47f6554a8058e12be7273e VersionId cdPXRaTOOFAa76BJZ_uDwc_VkEiLvJ7G |
| `same:pawn+ninja` | King+Pawn+Ninja vs King | same | `kpawnninjak.uftb` | **CERTIFIED** | 75,915,840 | concrete | 24,156,042 [3,793,267] (13,801,878) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,462,577 [4,394,022] (3,178,510) / 804,967 [804,967] (292,650) | 24,156,042 / 13,801,878; 31,267,544 / 6,690,376 | S3 table sha256:0f14d5fa2fe0d0d2102b5d92cdd1e77148a39571f6e94ec07702ee8c77eb8354 VersionId 5.qtRHG9yWl1B31SbclwwTjnIkgEEs8u; certificate sha256:23ec4da95ac59736bf7240b5d4a4fa63fac9c4cf6df512def59d6da3d190f8f6 VersionId 6jSjwbB7cN5Pejrdt6TqHKlFafVS2Yxg; reachability v3 sha256:dbae0377d61d01d124555e52218454704265d711d0493f966edb3aefcdd624e2 VersionId P76mBNtnfEnxq1IrufqUYmY.iKEdIta2; result sha256:ef92507b706fa54a3314f6cafbdebcfd83939e3463f6d8fcf054ea54e9c182f2 |
| `same:pawn+turtle` | King+Pawn+Turtle vs King | same | `kpawnturtlek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 25,786,808 [4,519,644] (5,456,624) / 0 [0] / 3,401,600 [83,907] (3,312,888) | 0 [0] (3,219,216) / 23,074,570 [1,593,720] / 8,192,974 [3,317,745] (3,471,160) | 29,188,408 / 8,769,512; 31,267,544 / 6,690,376 | S3 table sha256:0e6bff4e44b426c57f9da156356d885b188fe1fc19bc1a5b854a1e1825d876f0 VersionId HzK8M5Ae2QJuDMai.j387ks2Vqnfgfw3; certificate sha256:35d23bb598ca2e8c125ffa74731e599dbd11baadd31ff32563a0bb0cc8d9a7a4 VersionId 7EemDOVf0ha3YlSKPI.v86mFfFpowJNL; reachability v3 sha256:e06e70e118ced2e0d8ef56275dc769775949ba01377c0bb674da592708befa47 VersionId Da5yIqJmFblSU_cg_lG.lKLsaldeLSs3; result sha256:33fe22febff27c382de0e06f3f29e4c53439ac3c7f36af16245f9754891c04aa |
| `same:pawn+ghost` | King+Pawn+Ghost vs King | same | `kpawnghostk.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 65,174,280 [11,742,550] (10,741,560) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 65,497,819 [7,016,067] (1,958,215) / 1,012,317 [1,012,317] (1,009,057) | 65,174,280 / 10,741,560; 66,510,136 / 9,405,704 | S3 information archive sha256:9bfdfda02d7a1c6078e8610aa8abf073afdddefd6d643cc39a796f912a535e23 VersionId _Kl_OZ1DCKFWHuitTg33ZCd19vXu8Zmc; preservation certificate sha256:4e98945e64ba60c8d1526558381611f33ce016229fae96d09da69621e3f5df07 VersionId XB2mNaYhDuBdULQpdEz6aK73uGgB86_P; result certificate sha256:d62c44aaaaf5f21e021777f6ae39bb5aa5c7ec6c41f2b6ce6c090aa0b8adfc79 VersionId 5MDlfYr5doXgLZZsqr8UMsTDzDMPVfle; arbitrary sidecar sha256:f45467a69518a08a9ad3707fecd8ed912699c8e12b456468980a0bcddfabab17; information trivial v2 sha256:b4ffc47bcf9172f2f7692102b69603868fa8271dcb150e006119583aea6a21e6 VersionId yhm9oQNTn_xcr2YIyTtcjjm9qSLnSHLs |
| `same:pawn+mage` | King+Pawn+Mage vs King | same | `kpawnmagek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 29,847,888 [8,689,112] (7,048,084) / 0 [0] / 723,576 [478,593] (338,372) | 0 [0] (3,219,216) / 26,720,788 [1,631,410] (2,523,305) / 4,546,756 [3,318,678] (947,855) | 30,571,464 / 7,386,456; 31,267,544 / 6,690,376 | S3 table sha256:b8749d30ed8cd6bee26b603843e3036aa0e719bbad3dd2fa44234896516f2330 VersionId CAQMLhNIzBWY1GL.Rqds21KrH9EYA1RZ; certificate sha256:68d80146b73fdaef3484a1824c06c8c25ec72a1f396a0e67a9e322c0e76c8f53 VersionId 0UTkQEXSOdJjQjbOqCxgi3IwpHDxSfYT; reachability v3 sha256:b61d5aa7dc1cd78d54c8c3a71edd3cc32f95d3f18e10c27e0ebe58c865afd6a5 VersionId QN3p6bIlMsks4yESbS7AdDEaYeYTt5Jz; result sha256:2e9c96a7503d067ddaadce65477a01eab1117d88206d4a6d55f0a367811bdbdb |
| `same:pawn+penguin` | King+Pawn+Penguin vs King | same | `kpawnpenguink.uftb` | **CERTIFIED** | 607,326,720 | concrete | 29,664,972 [5,136,230] (6,702,807) / 17,052 [0] (1,712) / 6,705,334 [107,384] (260,571,483) | 65,730 [8,572] (3,769,736) / 24,660,200 [1,528,335] (26,692) / 15,552,370 [7,020,672] (259,588,632) | 36,387,358 / 267,276,002; 40,278,300 / 263,385,060 | S3 table sha256:7759fc51aa29ed9062e3345e6d6b9e46f76d9528252a8a0fb0d7889fe5191faa VersionId H_Enh7AWBytQ0U88zLQFF4ZoRmAXq75x; certificate sha256:17a8841ef5b5bfa1d31decfdeb5b7f7c38ab8236619731822c6e2b143a3a1804 VersionId n46DfmryRxp0cJKIzfkYzqkmXhPts_GC; reachability v3 sha256:4c241fd093851879cd12706b533118b9fa03f1ac69529906723796b6f6753d2b VersionId sl.1W5N5TXV._Idmtdju8BF9SynTEPRd; result sha256:9ab338d5214f417832333bf2b4aef88441b8849b7fca961ec4202de4d994678d |
| `same:pawn+parasite` | King+Pawn+Parasite vs King | same | `kpawnparasitek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 27,959,472 [4,405,844] (9,998,448) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 31,261,396 [2,797,676] (3,470,928) / 6,148 [6,148] (232) | 27,959,472 / 9,998,448; 31,267,544 / 6,690,376 | S3 table sha256:7aa90bad27723ba5c411e67604750ec32ddaa857713278d33dda806fd8b080e5 VersionId G0bJlyTT15mJzwA50F1s8vOpkV0fzXqk; certificate sha256:021ee5acfd57d36fb3da395de99542d1c8fea7e4fd5590166dcfa8c1f272e2dc VersionId bgURM.MiV8bMNA0YD1EwfFv1BzWCvsoX; reachability v3 sha256:b63903309ae379b8117c65d54a8dbd87982f95d2db6da59bcb0194aa5bec70c0 VersionId K.iP5XZsx0h7GZK4kAOmqKXbykkzPKhe; result sha256:acaa6f2bcd4dee7e0438bbdc5f607f0ea5baf57f5cd27c6f1c18509d91851311 |
| `same:pawn+devil` | King+Pawn+Devil vs King | same | `kpawndevilk.uftb` | **PLANNED** | 91,099,008 | concrete | — | — | — | — |
| `same:pawn+sludge` | King+Pawn+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:pawn+sniper` | King+Pawn+Sniper vs King | same | `kpawnsniperk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 50,411,293 [9,248,300] (68,735,776) / 0 [0] / 9,103,393 [143,015] (23,581,218) | 0 [0] (12,876,864) / 44,105,559 [3,252,705] (44,113,852) / 18,429,529 [6,719,938] (32,305,876) | 59,514,686 / 92,316,994; 62,535,088 / 89,296,592 | S3 table sha256:98604104e855f0366d798436a1e6f6f6e52d77f93e1d409e01c7ac2f6a9ee90f VersionId Mc1J6u3HdyM28cIXruFsAdMXPdSSUoRW; certificate sha256:4c060ea0bb6498bf0b38450839800ab1a3697396fd5213217271db5578eaa067 VersionId d3_6fTQnZN3.lvUl7cf4oBFP.gAtdEIB; reachability v3 sha256:718c96993467d6aa978424ed6878ab737b3ce41a6224a4d701ed80d779e0cc95 VersionId lzt3MdOpqjs661NwagcZoNMg5MZ6ifbQ; result sha256:57bbc99cfd8d856e699f5c3cc4c3f90b7fcb6333b668e2f87ae0845590703ffd |
| `same:pawn+prince` | King+Pawn+Prince vs King | same | `kpawnprincek.uftb` | **CERTIFIED** | 151,831,680 | concrete | 27,959,472 [4,405,844] (9,998,448) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,537,567 [4,110,610] (3,185,764) / 729,977 [729,977] (285,396) | 27,959,472 / 9,998,448; 31,267,544 / 6,690,376 | S3 table sha256:7a284191884b471b5567709d94b8683d7b3794ecf8631cb10e8eac7dc6ae8dcf VersionId kaCJG1_KZvq5iXdK_tByncr6FMkibLF5; certificate sha256:ff5e31acec7033f9543c72b5d2e507893f00d379f4cf5006000d92cf266779d9 VersionId ccyC8s6AqQHd8O8rdwheeSlv1LI0vw1c; reachability v3 sha256:db76549d302dce3dfa97392a05d28571b263b9fade3e345d1c87f5609f636c93 VersionId MZ7YPwAE7mPyo6WVYBP2X_0A_IaNg4zQ result sha256:022a767867632d6084ed83c101c0ae0c453b25ba21593b09398dfaee94c5bb44 |
| `same:pawn+checker` | King+Pawn+Checker vs King | same | `kpawncheckerk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 48,558,372 [10,350,877] (11,507,729) / 0 [0] (6,307,104) / 9,629,288 [155,957] (75,829,187) | 0 [0] (6,438,432) / 40,849,554 [2,999,292] (10,934,306) / 18,521,674 [6,357,739] (75,087,714) | 58,187,660 / 93,644,020; 59,371,228 / 92,460,452 | S3 table sha256:7c84d6120e05a58d0a1660bdb764400b8911288cf2885ddf8eba4f624617207e VersionId xPAKLQgVvu6YC5jsCpQXolgXJ4rC2RBj; certificate sha256:f937278870fe4a0233658d736317453841a22fac68449afb27998dee7bcfa510 VersionId DqL9XejAb.9jpwdeE923OpRr72K1XAuY; reachability v3 sha256:6a2dfdf80833424c6b3687d0069af006a486bf5042ae4fdf552adb342be1633f VersionId 0nJeaFrXylHSO1MCw2PlE3mwL9ICp9yT; result sha256:fad0546442174f87d973d9adcfc5d9dba6a40881b2a51f440098fdedf36bc976 |
| `same:pawn+giant` | King+Pawn+Giant vs King | same | `kpawngiantk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 15,978,839 [2,799,202] (6,404,886) / 0 [0] / 2,116,159 [40,269] (13,458,036) | 0 [0] (2,284,232) / 16,199,574 [2,065,785] (9,586) / 5,605,778 [2,535,361] (13,858,750) | 18,094,998 / 19,862,922; 21,805,352 / 16,152,568 | S3 table sha256:a2b5ea316d3e8a4623aa37f82ba28c9b01ab8fa4c7c94ed81bd5ba21e35d1dfc VersionId wZ6b.YA77HI_xKDF30Fxcque1NKIWsSr; certificate sha256:d61640096f3fc183bdb6fd3cb8a676605fe3fda279a497d1de026a09aebe5fc5 VersionId ybwLGv.PQSKVjDRvTfsGFxWO4FEQCEdQ; reachability v3 sha256:2cd5e74d943ae4f987ce9fa0cb67275026163feb670c4c72834ba28c6d7d6c35 VersionId XAgIuTW0U3_KK_cP7BXcnhASCVQhlxDo; result sha256:0e29753c5016a9d9a34b5be599a91c3269f08002ced595757b63a11ee0667144 |
| `same:pawn+copycat` | King+Pawn+Copycat vs King | same | `kcopycatpawnk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 49,924,420 [7,703,888] (22,721,336) / 0 [0] / 700 [0] (3,269,384) | 0 [0] (6,197,952) / 57,614,900 [10,301,920] (5,191,608) / 2,541,676 [2,539,772] (4,369,704) | 49,925,120 / 25,990,720; 60,156,576 / 15,759,264 | S3 table sha256:701e2b8ce146dcee29d4e7a683a0ca9607c7f0869e16a00c89521ea9ac65300a VersionId Xu9_mkQFlSXhYEXJlH8yKV8dwN4uVRts; certificate sha256:69ccbdba78e1eab01e696fdc0bb30f477910789b9fb35c55a5c7e1169c8b6516 VersionId osC6BIOJ8bcBTFaNDZ5LoOqfIZQ73lIC; reachability v3 sha256:4bf95ab4f31172a3a40ca3bde197e208e435db44a0dd3eff6402d27ab33efd5b VersionId ftSaPxXu2k.s3AGV1R6_WudSNiJsTUSM; result sha256:9922bb573ffcbb425f61da13f304db3016eaec39172a3504eea2e881f3a4a62e |
| `same:pawn+angel` | King+Pawn+Angel vs King | same | `kpawnangelk.uftb` | **CERTIFIED** | 227,747,520 | concrete | 81,072,227 [14,357,291] (11,759,065) / 0 [0] / 10,642,165 [124,945] (10,400,303) | 0 [0] (6,438,432) / 74,336,818 [7,957,724] (4,606) / 22,360,398 [9,455,087] (10,733,506) | 91,714,392 / 22,159,368; 96,697,216 / 17,176,544 | S3 table sha256:a288e13d35bd6b592890b4a7205eaac74debe8e90c1c84e901b8c6fdec4a0c71 VersionId VTwlaycP7aJC2TgLozFPuf_O55r9t.i1; certificate sha256:9ed02e3adfde548cc06db769285e9eac81ff810b7e7a48cc0fd16cbc6599ecf6 VersionId cwDIBtMSxIWIGodaiQiFld1wb3ZsD3eX; reachability v3 sha256:075b380e4f27068e3923868f42ae0e7c91713182708c336a123b8b96c0afd311 VersionId l_3mlHVFj7GkICi73jwIM9VPkP6UR9WP |
| `same:pawn+fisherman` | King+Pawn+Fisherman vs King | same | `kpawnfishermank.uftb` | **CERTIFIED** | 75,915,840 | concrete | 29,932,710 [4,782,493] (7,157,423) / 0 [0] / 638,754 [66,944] (229,033) | 0 [0] (3,219,216) / 26,628,630 [1,626,779] (2,668,726) / 4,638,914 [3,319,504] (802,434) | 30,571,464 / 7,386,456; 31,267,544 / 6,690,376 | S3 table sha256:ca515123dd7cf6fe77310a3a9e31e8be93cbbbf89dbeb7891d31ac9f2e9fec4d VersionId Xhz0VkoUPyzBfv0Yp5hf5oOJSTYmIvm0; certificate sha256:274da28259e28d3f5c56189b7481d04058bb366c969b4210fd5126073cd881e9 VersionId yguINx7x9VRh3ukSBhTWoqlvNDpNTR8J; reachability v3 sha256:ba17956225ed25493f57f5632808e28db22775d89eba9eba4efe7803bd7c01e6 VersionId LbJPZkjPfcQnO_3UtB1J2DuQI.VsKUzl; result sha256:85f47bc3962e658c78554f9b3e52eeb139fa99d62af40c98098b11610ab7b5ac |
| `same:pawn+dragon` | King+Pawn+Dragon vs King | same | `kpawndragonk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 24,792,154 [3,892,809] (13,165,766) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,520,715 [4,480,363] (3,184,406) / 746,829 [746,829] (286,754) | 24,792,154 / 13,165,766; 31,267,544 / 6,690,376 | S3 table sha256:d9f1cf522013f48d662d9558e98ccb850731dabbb29f4251abf50c16cefa8d24 VersionId zePQz.JG8fkJLCLyuKdkO9_h01E6bVk.; certificate sha256:9c170597586b6217bf618e4da343d9ea72dd797f4ca6e1106a8f3636eb5511fe VersionId T.tkKELoM2uuNorhCKW_uPrIJX.bqcmg; reachability v3 sha256:43815a13d05b1e12909aceecc250d86eb973358dc583757292d0e9f14f414248 VersionId 0a9nP7kwZYDHGlZvT1l8hNRUK.G.5YTC; result sha256:3e6ede973af86cf474916fa1035bb2c6535cb71c448e76bb470ab6ca84fe84f5 |
| `same:queen+queen` | King+2 Queens vs King | same | `kqueenqueenk.uftb` | **CERTIFIED** | 18,978,960 | concrete | 3,991,466 [0] (5,498,014) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,560,812 [951,406] / 123,864 [123,864] | 3,991,466 / 5,498,014; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5f43d74fcfcbf01ade04871d14bd05af92514539689630d460e14cc4c4298f86 VersionId Ti_UWP_2DPlwZUy.XMhAr67tmcJ1TBJl |
| `same:queen+rook` | King+Queen+Rook vs King | same | `kqueenrookk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,474,332 [0] (9,504,628) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,251,240 [2,077,062] / 118,112 [118,112] | 9,474,332 / 9,504,628; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:f14ab19e0f7a2db12837e8a6131f7a7d27722ad369405c57a995e16a3b5f0dc4 VersionId BdvdLojNMVfRKT9ic1Hgxw0iPbca2YWw |
| `same:queen+bishop` | King+Queen+Bishop vs King | same | `kqueenbishopk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 10,451,944 [0] (8,527,016) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,034,380 [937,166] / 1,334,972 [1,334,972] | 10,451,944 / 8,527,016; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:e8186bbe3bc86a6e1ec94729ca36f12ca7879a21d9b26977aedc836d18f98247 VersionId f2J68Yi0Ds9.C52XIk08lpfr7zpRt2XJ |
| `same:queen+berserker` | King+Queen+Berserker vs King | same | `kqueenberserkerk.uftb` | **CERTIFIED** | 379,579,200 | concrete | 38,850,936 [0] (150,938,664) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,225,060 [14,097,412] / 468,460 [468,460] | 38,850,936 / 150,938,664; 173,693,520 / 16,096,080 | S3 table sha256:188b85277444a880806efa7862ec58f1397e1d22249776edd2d6634d9afbaf09 VersionId 7ILiSnEFFu5zGlJjntp9Ucb6brvuUTtr; certificate sha256:c0f559b14c0f271ddeb043d59986810af7defb15f36513442ab02eed17d8121d VersionId eqK8ljhgPu7iBMjXJiX6F_95bO5.Gbpu; reachability v3 sha256:8ab9addfc87c5babd90d6a962b11006a0280b18ffd965cddeb7b55a880aa1b58 VersionId DrE3RhvUrYIKf4QxjC3nnqlhyJ4rpDnk; result sha256:0dcdc7fcc0f534cd8bcc8a6d8cb57117590cfa60fe984b7a5efb03e85eedcd1b |
| `same:queen+bomb` | King+Queen+Bomb vs King | same | `kqueenbombk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 10,388,902 [0] (8,590,058) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,201,128 [1,244,268] (67,760) / 100,464 [100,464] | 10,388,902 / 8,590,058; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:42fd3c8cbe11056c8adcb894976807090311085fcd33229715833dfd0cb5e885 VersionId g97TklR_4_IBsIjsq_jgZpLD8t5trRP7 |
| `same:queen+ninja` | King+Queen+Ninja vs King | same | `kqueenninjak.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,342,106 [0] (9,636,854) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,178,972 [2,055,346] / 190,380 [190,380] | 9,342,106 / 9,636,854; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:6c5f8cef652d70624ca4e672a6d95c18aa8e11ed08832c14591747b3de201bdd VersionId 1NrMD30WAMbyqa2RqnhO30GfTI3f4_Oj |
| `same:queen+turtle` | King+Queen+Turtle vs King | same | `kqueenturtlek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,369,834 [0] (7,609,126) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,954,050 [937,166] / 1,415,302 [1,415,302] | 11,369,834 / 7,609,126; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:2704ffbc93c29368932990e9ced1eeaded106bd1655e2965111942dbeca4db7c VersionId lC9ZMlfbwJEE9Jyd.UePjeCRYadbjs9S |
| `same:queen+ghost` | King+Queen+Ghost vs King | same | `kqueenghostk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 22,727,232 [1,065,772] (15,230,688) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 33,175,250 [3,695,848] (1,482,256) / 79,818 [79,818] (1,380) | 22,727,232 / 15,230,688; 33,255,068 / 4,702,852 | Exact solve completed after 12 Bellman iterations with zero Bellman, rank, structural, singleton, source-remap, and conservation residuals. It binds source sha256:7b0ec34fc2f00524b0bf731f46deb7182c2c50fb17d1188dfa76f2238a697bb0, model sha256:c7be59127e706959d44b8441ec308de3b1389b0405a595730c4daa3c584aef65, observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23, information payload sha256:6ad4b41e236baa5901d0db6f0689f7a142d38ad5f9766f98527895e4f223bd73, and arbitrary sidecar sha256:09dbca339054c1c9e425d992b3a7de21c42a70824e2f04ae3ccef88f5718067b. Deterministic archive sha256:eeb3f12834aa8ef0d94538e1e18e636834c06e59752e9c0b868d86721bf5a2eb is S3 VersionId `o3hwV7VUjL_CRVeNdgSiR7Jz57tcOliZ`; archive certificate sha256:369437e2ac9b9b3ceb225db715891a3e0bae95e35e91c9780ec5bcb342347be0 is VersionId `Qi6ffJz0rbpqIo2TPk3nthbiUFpHdIkt`. Both exact versions passed HEAD, download, full-SHA, inventory, and restore verification; source results and scratch remain retained.; information trivial v1 sha256:f80beee7f95e81d9c8641f3f28f8e222c97b245ffb98aec97a6e88aa62d2ba69 VersionId eVITYiRVhiW59urAHkVODgY3YZx2eMCT |
| `same:queen+mage` | King+Queen+Mage vs King | same | `kqueenmagek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,943,344 [280] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,912,942 [937,166] / 1,456,410 [1,456,410] | 11,943,344 / 7,035,616; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1f5bbdce3d25aaa16dc62587914f508ff0f0234e2be40871da22e479b4a5455c VersionId rM0vVtFnU3YOkHzR2z4l5jU3eiotvQgS |
| `same:queen+penguin` | King+Queen+Penguin vs King | same | `kqueenpenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 14,516,164 [0] (9,516,358) / 6,618 [0] / 129,830 [0] (127,662,710) | 25,886 [1,918] (1,881,536) / 19,100,638 [940,786] / 3,186,556 [2,983,230] (127,637,064) | 14,652,612 / 137,179,068; 22,313,080 / 129,518,600 | S3 table sha256:9755fb499c39e2d6764dc25ae29815a8f7023ad2ee27ca7f1a54bae656137d4f VersionId hZKBOlu2IUWfVex_6c83Yx1D8S9pdIcw; certificate sha256:f834c2688200169261485301c702702abd9c8963d561ca6866be0cf943a29cb7 VersionId LhnaMC9fGseOu_jVJfi7KdjDluazwEAN; reachability v3 sha256:5fa1cf226a3ad597a5cfcdf6e9e9be19ed4999c964c4f73f15cda776ab9958f0 VersionId BRFsPjIhFPEWPvCdOW2kl0T8aTWjx.fV; result sha256:8956777e9f6a621c1ec3435a94cf24d50ece7574ea3b2de6c6f088a78e0e7926 |
| `same:queen+parasite` | King+Queen+Parasite vs King | same | `kqueenparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 10,877,572 [0] (8,101,388) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,307,456 [1,304,236] / 61,896 [61,896] | 10,877,572 / 8,101,388; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9c1fbc8bdf1e8bc75cd4c510c7761b2b396fdbb01f43aed063b30962894017d1 VersionId hIJ1w56ACU1AP3MsQEY7q9z9nZPoEb5U |
| `same:queen+devil` | King+Queen+Devil vs King | same | `kqueendevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:queen+sludge` | King+Queen+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:queen+sniper` | King+Queen+Sniper vs King | same | `kqueensniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 23,192,071 [0] (52,723,769) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 31,894,241 [1,884,335] (31,869,235) / 2,844,463 [2,844,463] (2,869,469) | 23,192,071 / 52,723,769; 34,738,704 / 41,177,136 | S3 table sha256:1f238e603715cbece48e9107db2a9a7dbb176e9ee3a8ce63f736629312466515 VersionId vkwMs3HmVOil5hghWd.4PNXf6xy497OC; certificate sha256:a3de713606611c03190073be25e045ec10ad9b26f0b54d3a03f03f47154b1e05 VersionId jhwg208IBjW1EtzAAr8gR37KgVOk0wMM; reachability v3 sha256:b25cde61c84116aeca23bd213b4e2361e977f21a3c71b07fd5162fb2f641df28 VersionId PNd.rNNhkFN.JFtYHOWf48wLLRplhx7n; result sha256:be6ac47cdd07b5097f31fe547d758befdc799535973982c1fb0d50608403e298 |
| `same:queen+prince` | King+Queen+Prince vs King | same | `kqueenprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 10,877,572 [0] (8,101,388) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,307,456 [2,241,402] / 61,896 [61,896] | 10,877,572 / 8,101,388; 17,369,352 / 1,609,608 | S3 table sha256:bfd03b2ac8860cd9665b58d800880ea373ad711cb7ac9eb7f4fb9d51bd500c7b VersionId 1Z5qKev.m0YNjp.GEJirwmBanBKXV.q8; certificate sha256:9f15a0b46906b8be8327d87bdac531488bae8de62da904fe9393ea535107f72d VersionId N8DZzQCZ_R1OGJ2BFbTM2jsziv9wl.Py; reachability v3 sha256:c7a74f56dff21143424d25f057f758f4105bbf25a678b9fa1d5b36d51b5a6ca1 VersionId _frWCjb1Q_aEa4bvczbOIIo4rHPVcmhv result sha256:5788af4f33bb9f1c502a2baaca2d12f364076c92656226310f40241a3ec46aee |
| `same:queen+checker` | King+Queen+Checker vs King | same | `kqueencheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 23,003,190 [842,688] (15,797,418) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,293,838 [1,804,538] (16,205,172) / 2,709,286 [2,709,286] (23,488,328) | 23,003,190 / 52,912,650; 33,003,124 / 42,912,716 | S3 table sha256:6414fa2bd9c72ff7ec46d0c2d4d9ec01bcafb4c05dbb1bd746eb69e86a3ca225 VersionId SprjB7Kx2jhx3CEMXh50uQLKGoLfpt41; certificate sha256:a52c7b2545d2aa8cce82ca90102c76b3a1b34a58eba5c942b3daab42e4bea758 VersionId TvjNAhsxfjq9Y9HNnQ4rwTo8Y9pDA5wk; reachability v3 sha256:12d56edcf3e7ed206e7eb98032c564863853a5509f2cbb9f1a6c6d32a100647f VersionId JxU87dySJh2jVy8LUMaZ_0eCQPa7XLQC; result sha256:f5e53a9237688466508a8d3f30658faac37385c1057af1aee9b59c61d345db95 |
| `same:queen+giant` | King+Queen+Giant vs King | same | `kqueengiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 7,071,014 [0] (6,215,686) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,256,584 [1,309,252] / 888,000 [888,000] (5,692,260) | 7,071,014 / 11,907,946; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:d36256e730da6b065306ed22a39796cc677f7c56d3391213640dcf4bb90ef134 VersionId UxnBAsYNqIa80XG99DyuWOMvFpp74WNe; result sha256:a9b0135f35a657e5192f939a57f0f81a2af90cefaf34847d97e7b5e592550937 |
| `same:queen+copycat` | King+Queen+Copycat vs King | same | `kcopycatqueenk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 19,505,032 [0] (17,011,448) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,263,840 [5,756,552] / 153,664 [153,664] (1,441,440) | 19,505,032 / 18,452,888; 33,417,504 / 4,540,416 | S3 sha256:0937aa6503c03f2ba69acbef26e1ad09f3b3325ebaa242b1249e9ff17699d08c VersionId YkV3LC8Bq_C3ZRdrj6C45LH5i3YBP.Id; certificate sha256:3120e771730e3d857c202fe5675211a165bbe24dadb1b64a74dabb764d9453e2 VersionId 7jxFTeCCnXzEBFNXqLAIzEC3j8OwvxCM; output sha256:855d0c4d8a72cec4142c4c2b4a0ea4500710575fca4138a3c1d0e808bc6ae01f; reachability v3 sha256:7ee733614166fcfb7cf2060f23a3ccf6e5bd095542ff49fac1800b2907c2a1eb VersionId 9hA_id7AQ8OEnwvwbGsUKzoZsanEA7ta |
| `same:queen+angel` | King+Queen+Angel vs King | same | `kqueenangelk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 35,830,032 [0] (21,106,848) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 50,696,550 [4,773,518] / 3,021,114 [3,021,114] | 35,830,032 / 21,106,848; 53,717,664 / 3,219,216 | S3 table sha256:ed526cdeb043de2a86226d6629362758f1d0b3f06bcc8acdf368376fb5ebb020 VersionId 4QmWt4TC5PTxUfsr27j1s4mjxMGQ13VO; certificate sha256:4da6311b3ea78af2813aec116ba51f69766d78ee9fa1875fcdd454ce664711e2 VersionId ZtkNDs.z8pnyXIWltZ9cPczInqwRU3CA; reachability v3 sha256:4f8d735d654fc1f52677e3201f2c9835d81142d5a8e4cee444947051b46d1abe VersionId GRJWVEtBfSny1kNJwnE7o.ZHqg8orleh |
| `same:queen+fisherman` | King+Queen+Fisherman vs King | same | `kqueenfishermank.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,943,344 [0] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,912,942 [937,166] / 1,456,410 [1,456,410] | 11,943,344 / 7,035,616; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:a4c2ce7d24047213742eb6c81afe8827913b73c73b488ab68b4de37200dd5764 VersionId 2aUXzgFotG9Ao1SpseMdDzX8XWjm2omP |
| `same:queen+dragon` | King+Queen+Dragon vs King | same | `kqueendragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,653,298 [0] (9,325,662) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,238,014 [2,100,544] / 131,338 [131,338] | 9,653,298 / 9,325,662; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:0f5c32b3836772de915c169181709f3b9cb3583d5d82d8b01f68a985dccc9392 VersionId JMtfxe0AwQ1R7YRAROOUE0rlVTv869Td |
| `same:rook+rook` | King+2 Rooks vs King | same | `krookrookk.uftb` | **CERTIFIED** | 18,978,960 | concrete | 5,565,392 [0] (3,924,088) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,669,238 [1,114,238] / 15,438 [15,438] | 5,565,392 / 3,924,088; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:6b592e6eb003cdfdd43ce8fff1e71c735441898b9c38c28e89b92a5540189e65 VersionId DU2uLNXnZxD.A7wt2nKMd.fac38dsHgG |
| `same:rook+bishop` | King+Rook+Bishop vs King | same | `krookbishopk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,371,072 [0] (6,607,888) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,100,204 [1,111,416] / 1,269,148 [1,269,148] | 12,371,072 / 6,607,888; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:3f6ccfd4dc98f49996fbcf0736f4200a9fff3a40182ae745cbe3e8b2ee51669f VersionId Pyue6PT1W.QCyi2kBr0iVWLRyENWfGol |
| `same:rook+berserker` | King+Rook+Berserker vs King | same | `krookberserkerk.uftb` | **CERTIFIED** | 379,579,200 | concrete | 45,455,332 [0] (144,334,268) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,567,318 [15,833,892] / 126,202 [126,202] | 45,455,332 / 144,334,268; 173,693,520 / 16,096,080 | S3 table sha256:21ceb9c97bc044af5a1fb2424efbd6febcd355b33d8bea6684b8e18656ae28bc VersionId G1fkWEpdf2IGs.1SrAB8dAV5dT55X.y0; certificate sha256:c707a22b95b0145eb803e992cfd7f7b5591368962160d5a780c4a6f0709f5ee6 VersionId 4KEwk0yMSGIszyL4m2aTh9acsNQZ.aWw; reachability v3 sha256:31430477141abb8661ed8dcc57de83f98db841d16feb352eef5c69f336742ab0 VersionId lmYiY7og1EaY5GzoRtaJEZaX11MIbqlS; result sha256:275588d89d53fabaece72ea4c2b3f8ea50241da0de23dd055c1f09bdb00c49c7 |
| `same:rook+bomb` | King+Rook+Bomb vs King | same | `krookbombk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,221,516 [0] (6,757,444) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,242,972 [1,244,268] (67,760) / 58,620 [58,620] | 12,221,516 / 6,757,444; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:c73db0a7f04777f343bdd8d7327413a082ad4e2a45971ca656bf0c28f6108ac2 VersionId _L8ygVXEZtqodhaaQ03hOt2j_gztGFSt |
| `same:rook+ninja` | King+Rook+Ninja vs King | same | `krookninjak.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,008,382 [0] (7,970,578) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,271,872 [2,229,596] / 97,480 [97,480] | 11,008,382 / 7,970,578; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:ea585fc05c37fcd58a983d599b19427b840b0c0f283308bef3a0e53a9c99c12d VersionId 18yOe.texLbRLVVt7b.4qVDE.IQmj3Lj |
| `same:rook+turtle` | King+Rook+Turtle vs King | same | `krookturtlek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,353,634 [0] (5,625,326) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,000,700 [1,088,580] / 1,368,652 [1,368,652] | 13,353,634 / 5,625,326; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8056e5e85bc56667d7403ac01d711f7a7b9d79ad75c31df3eed868357ec94855 VersionId O0NEym.8uQn2VeJnN0_j44MNUeHQ3uEc |
| `same:rook+ghost` | King+Rook+Ghost vs King | same | `krookghostk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 26,755,672 [1,229,824] (11,202,248) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 33,249,378 [3,833,460] (1,483,560) / 5,690 [5,690] (76) | 26,755,672 / 11,202,248; 33,255,068 / 4,702,852 | S3 information archive sha256:67e53c4672e0b4293671ecabdc15ef9ba422c6db3099dac8a6c373734b10ec2f VersionId HNLZUdc.pYHu8KdvqUCwUQfheXf_eOHs; preservation certificate sha256:d3bb3d6cf83d638fe914037b65477b16f56a70e287a1bbb8f3309bd5d6fc376a VersionId Uod9Oozj658n9EAdKJ85yFpDIZNnMKJ7; result certificate sha256:35bba5e5059f77bab9add372e2227275a0096f376b71c70948d35209c19c6b4c VersionId PFwny1ajD02Rh1I2DKZagoSf6BBPXwtc; arbitrary sidecar sha256:a07868465e3957ff31e5d7efd5120a011921295051f0588b3dc9e999192bdd3f; information trivial v2 sha256:389036170bca39034ee060d10efec8203f9632ea9ec311a911630574590cbe10 VersionId ls2e.ZtAglihNjfj78Wo_xfWS5vLBpao |
| `same:rook+mage` | King+Rook+Mage vs King | same | `krookmagek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,027,524 [32,974] (4,951,436) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,950,608 [1,088,580] / 1,418,744 [1,418,744] | 14,027,524 / 4,951,436; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:53f66bbd4d8d5d3a25e5036a0d34019f68c094ea0d6a971885aaaaa7db764614 VersionId KDkhcXj3Yn56Qbu8kDEsZt2fx.JkD7K2 |
| `same:rook+penguin` | King+Rook+Penguin vs King | same | `krookpenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 16,749,144 [0] (7,223,250) / 7,554 [0] / 171,446 [0] (127,680,286) | 28,934 [2,406] (1,881,536) / 18,879,752 [1,118,246] / 3,404,394 [3,131,472] (127,637,064) | 16,928,144 / 134,903,536; 22,313,080 / 129,518,600 | S3 table sha256:88cdc6fd9da50de70781c068e55b5668c88649973388de006121ea469272ad41 VersionId V4TTGhRhlsJunNdnVP2NLVcSWwwMFa_L; certificate sha256:8b9dbd34fcd4d13659f88664eaa771611c983614a54e3fd954419b0d733c1986 VersionId gidgX7vTq0bKm.ubFY43qBFBsS3Y2oN9; reachability v3 sha256:38097a6377e5cfc759664a272dfeef6717bc750f9da37ee7293f8e498ce30947 VersionId QVkhidLr8M2tFLDlAbvDlP.Bu0vpn00U; result sha256:1b8ac4a9603394068f2044f64348254af44fc357f1f30ac0f2cc4957324e86ae |
| `same:rook+parasite` | King+Rook+Parasite vs King | same | `krookparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,797,700 [0] (6,181,260) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,360,968 [1,304,236] / 8,384 [8,384] | 12,797,700 / 6,181,260; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:392da78f981983cb3bfdf4b729fa89d6cc55a61d5fba339e48f4337f646564cf VersionId DeJWpXXEW9cSi.3fIiVMXt6NOl2gGhLU |
| `same:rook+devil` | King+Rook+Devil vs King | same | `krookdevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:rook+sludge` | King+Rook+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:rook+sniper` | King+Rook+Sniper vs King | same | `krooksniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 27,240,584 [0] (48,675,248) / 0 [0] / 0 [0] (8) | 0 [0] (6,438,432) / 31,982,751 [2,187,163] (31,959,260) / 2,755,953 [2,755,953] (2,779,444) | 27,240,584 / 48,675,256; 34,738,704 / 41,177,136 | S3 table sha256:01f32cf37d9422ee26cd2844c2b34b0c23c19f469aa00ba5840f7b82f9a10eb6 VersionId oyA40r31qcueF_JIcj9FKysGG0F0ItH9; certificate sha256:2068437bb526770fae6cdd9c117264fdb9bcecd8b49de31373a2a5ffcd881c99 VersionId MAJO3uhmd25kPm3qrccJbuFLxNjrncKg; reachability v3 sha256:3ea8e0844096103ab619787e6dca814e1727472b8e818239ac8193f687b57b3a VersionId uuj5WC8kM1px8O8vpo5ZrMkyMW7Cj6VS; result sha256:1e102a2f5711253a7ff319ca986cb235cee8a1830c02a0a7dfb97d12ddba8711 |
| `same:rook+prince` | King+Rook+Prince vs King | same | `krookprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 12,797,700 [0] (6,181,260) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,360,968 [2,409,632] / 8,384 [8,384] | 12,797,700 / 6,181,260; 17,369,352 / 1,609,608 | S3 table sha256:f5064e1ce1749959f9b2b4ac1ccfed866624c319e192cac6bac59f77772c9a96 VersionId SQkiGg_ai_EdmkgW5gHIBurlFi4MDpcu; certificate sha256:e645d8dda2357466ad89aa77da29acb118f4234e2fb9cc160530d91eb90c6758 VersionId DJhrEbug_Ff2S_DqxwZ0TWu2bKmrTUIG; reachability v3 sha256:a7ade4e7571df6384d22112554dc276eb5a7d2689140e10f92534a82c0098d4c VersionId ZgEG_zNP1NvSVVNiIy75o8Pn_w.vSMNy result sha256:75b23122e70727468159450026142e6975778dcbcc5db60c136836b29f0e73e3 |
| `same:rook+checker` | King+Rook+Checker vs King | same | `krookcheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 26,884,310 [842,688] (11,916,298) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,374,309 [2,119,457] (12,113,725) / 2,628,815 [2,628,815] (27,579,775) | 26,884,310 / 49,031,530; 33,003,124 / 42,912,716 | S3 table sha256:c3d1e17886e93b3ac994c5ee32e345107789c4d495ee1772a41d30a95544f055 VersionId Cy0.UUaNZ2.A8rqV6WdJQB7j0rjx3cEk; certificate sha256:b0a78d5529844420514a40ab5279e386ac823e921115c1a6244302d6853c279d VersionId E0tzJseHgkXwcCcf4bsdfgsFLe8q9PFX; reachability v3 sha256:fad2105a86312c9a66a6c6965a6defc415d2ffe3a637a92379ad4be6dee91df1 VersionId VGgbeOIyryxMLXD_nlnYa5863uqtD5R1; result sha256:ab318a12b1599eb3cc1bb9ac5cb6c1caa30f2e1c83f9e6784b5bb316c6c92ee3 |
| `same:rook+giant` | King+Rook+Giant vs King | same | `krookgiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 8,246,894 [0] (5,039,806) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,296,414 [1,380,932] / 848,170 [848,170] (5,692,260) | 8,246,894 / 10,732,066; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:34ac19b4b6a8d19b6a5679a4c0102a8b6607fc2899cd18e23a1051910f8fdf53 VersionId J6VXsASUQmJDzuOAWcKwX_NKaPZN126z; result sha256:0ac2b5d3a1ef4c6c0d39c2cf7b7b6fd857d38319dc4607e8183418f38979a4a8 |
| `same:rook+copycat` | King+Rook+Copycat vs King | same | `kcopycatrookk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 22,871,952 [0] (13,644,528) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,387,600 [6,310,120] / 29,904 [29,904] (1,441,440) | 22,871,952 / 15,085,968; 33,417,504 / 4,540,416 | S3 table sha256:b1f67c45778a01a8e2df4ee54acfe699e048fb66b11d5ea683e745c8f501d457 VersionId OelzfPBCPgGnoJ62nGUmCFq.C6AZhivq; wave certificate sha256:c3b893807a8cf664836133fe6bbc69e576efea2bad6b56098c2b31fae7d98d73 VersionId BuHi08QPfw75nQ9ehlxd5ns.cvS.dpOw; reachability v3 sha256:d490722be6a788972c4d8ad5955fcb791eff55cac200bd0fabe3de6791b71a36 VersionId cAU1cZ1jMhJD1jZgJV0hnqs6_6eCpgvN |
| `same:rook+angel` | King+Rook+Angel vs King | same | `krookangelk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 42,082,572 [0] (14,854,308) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 50,806,680 [5,573,662] / 2,910,984 [2,910,984] | 42,082,572 / 14,854,308; 53,717,664 / 3,219,216 | S3 table sha256:6fd321ef6bd565054ab1a138a82aac860d01c75dfeaebbe4be42305923dbbba0 VersionId rQfTg.CAjkxk3BlHwSspFURyw.MncwkS; certificate sha256:9e548d4226193bc5ebe24b7f515a84bb77e55aa3606d1f76dea0648b534665f8 VersionId QLTVTo.50AMl3hGDgNGyXhZTcPvYCoR3; reachability v3 sha256:98238bcd7c5fad5ddb5b07fc6f140d06239400165bf41f0bdfb45bb88c98a3a1 VersionId ZM1Xb3dRdjXbMxwrEoYiObEMKliu3ESz |
| `same:rook+fisherman` | King+Rook+Fisherman vs King | same | `krookfishermank.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,027,524 [2,704] (4,951,436) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,950,608 [1,088,580] / 1,418,744 [1,418,744] | 14,027,524 / 4,951,436; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:73b4a4fed69af5909105c59d05d4c30dd6ac44ca855a5505ad3e801574b825bf VersionId KsivsLMfwP6XjE3cJ0vXE.IYlujTJtnj |
| `same:rook+dragon` | King+Rook+Dragon vs King | same | `krookdragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,413,200 [0] (7,565,760) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,325,054 [2,274,794] / 44,298 [44,298] | 11,413,200 / 7,565,760; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9abda951610f4e02be8a782592c81c405864261f90c296c1d6c53e345e76a153 VersionId 8Uz5R9KUCNVyYHgMxWLkB9q.0M26NJpK |
| `same:bishop+bishop` | King+2 Bishops vs King | same | `kbishopbishopk.uftb` | **CERTIFIED** | 18,978,960 | concrete | 3,306,336 [0] (1,498,130) / 0 [0] / 3,376,686 [0] (1,308,328) | 0 [0] (407,502) / 3,708,210 [0] / 4,976,466 [689,058] (397,302) | 6,683,022 / 2,806,458; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:0bb414feaa7782ba3d949e646b49d2c2bac1a588bc0df5a90fcadb29f4021383 VersionId _gEaalfMtGUwf2PEgOUvAnlao4VTAaZF |
| `same:bishop+berserker` | King+Bishop+Berserker vs King | same | `kbishopberserkerk.uftb` | **CERTIFIED** | 379,579,200 | concrete | 49,928,080 [0] (139,861,520) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 161,099,700 [4,418,604] / 12,593,820 [12,593,820] | 49,928,080 / 139,861,520; 173,693,520 / 16,096,080 | S3 table sha256:37a11cbd5b420adcfac78138b7e605446a6899f0c5276476c6de85158c24a695 VersionId UJfZoGL.vqyp9isG_PBLEIsn5xKMmqiK; certificate sha256:696182701ea6da505f4f20b6b9ce589e899b0d4420230d9a9ffee0debf7e9275 VersionId tIb3g9BishAIhCG3GX9pZoa.ino2WbO9; reachability v3 sha256:364b4cac33dad28f45884028abf61dd77bab8812802db4766c3572a46e674ad2 VersionId ELT7BtzS_uemwjPotKZCLPL5ed1NdN_O |
| `same:bishop+bomb` | King+Bishop+Bomb vs King | same | `kbishopbombk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,383,238 [0] (5,595,722) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,265,800 [1,244,268] (67,760) / 35,792 [35,792] | 13,383,238 / 5,595,722; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:6a9e37879023734994f91d46d118cda635004dc48684f7247c059955cf60d76d VersionId lh1uR2Osn1yiIC3S1tl6viqcnFe8jbKG |
| `same:bishop+ninja` | King+Bishop+Ninja vs King | same | `kbishopninjak.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,053,584 [0] (6,925,376) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,048,930 [1,089,700] / 1,320,422 [1,320,422] | 12,053,584 / 6,925,376; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:338c9d7cf6d9dcaf76790229cc15c66ba092211d7047ea1220aaefad67c5ffdf VersionId 50t5tqne75Nfac6ysQbBQJGlqTw7P.vE |
| `same:bishop+turtle` | King+Bishop+Turtle vs King | same | `kbishopturtlek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,555,124 [0] (4,380,964) / 0 [0] / 42,872 [0] | 0 [0] (1,609,608) / 14,694,114 [0] / 2,675,238 [2,557,792] | 14,597,996 / 4,380,964; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:85e4a217d56a1f7742282cb8935ba5e449f5da6763b4d055486f1f0a2031334b VersionId smLm97XVhZXbzYPqaW19X0Y6xyqgEEGy |
| `same:bishop+ghost` | King+Bishop+Ghost vs King | same | `kbishopghostk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 29,226,628 [1,319,584] (8,731,292) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 32,002,462 [2,636,240] (1,482,452) / 1,252,606 [1,252,606] (1,184) | 29,226,628 / 8,731,292; 33,255,068 / 4,702,852 | Fresh corrected-rule exact solve completed 37 iterations with zero Bellman, rank, monotonicity, singleton, compaction-root, independent-grouping, realization, and conservation residuals, admitting 62,481,696 roots. Its authenticated 69-file manifest binds source sha256:1b5655ce18f31079d5c4d63d820ab74402b50ccffd4b1a01827fd7296ce22cd8, model sha256:845ae7b2699123ea44d665b96152a9f3b24cac9da6239958277bb069b0767a39, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. S3 archive sha256:95c8ac14280247b994df17a4c20660ee4242d54f9f2062c97f4ac6622a394e17 VersionId H4fGBBmMzs2Z1rnVG.UDRkyypYOvVmMn; preservation certificate sha256:5d974e2b2190b178180c69d5b41f9f0d8c478968b8c869bea95cd19aea94bf8e VersionId Wocq8qdxuhzPB1uCnwDVD3ibveVQ7ERF; local restore, S3 head, download, archive-restore, certificate-head, and certificate-download residuals all zero. No pre-fix transition or overlay was reused.; information trivial v1 sha256:64d39bccbd60bb8b608d547ff55ec9dcb178d0d6e2b8aa93f8c3cc40cec9885b VersionId Eh87Rjfag4qEH4xW5ubOGuc9TKf0EvRI |
| `same:bishop+mage` | King+Bishop+Mage vs King | same | `kbishopmagek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (3,693,788) / 0 [0] / 15,285,172 [605,244] | 0 [0] (1,609,608) / 0 [0] / 17,369,352 [2,578,596] | 15,285,172 / 3,693,788; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9e30ae81807379938862ddf216d0d52bbd32d8e08821eb2a5d1ca0894fd6be41 VersionId d_xMxErDLlH_S6CP_UjXzH99OtX3wneh |
| `same:bishop+penguin` | King+Bishop+Penguin vs King | same | `kbishoppenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 224,826 [0] (5,076,498) / 8,690 [0] / 18,068,924 [0] (128,452,742) | 33,710 [4,094] (1,881,536) / 337,642 [84] / 21,941,728 [4,401,624] (127,637,064) | 18,302,440 / 133,529,240; 22,313,080 / 129,518,600 | S3 archive sha256:509d5709f14ce2411de876f5615c510877ecaf8f0e8b72a4c4d09caf48c2ef00 VersionId nZ4OBHs_LXkXd5tMPjI5pmxdzn4mfHPJ; result sha256:2e959c0d8845e38ad9e304ce2b7f0d0b8036bca54a382b5ae6eb7146c40f7c59; certificate sha256:c9c1b14f8211e61d2ee00c60061dbfafcebf58e4831c89f5ef7795a8f7cccae4 VersionId Ew9sYhJoV2uKbqVBdr.RbjtGMRxf9YCB; reachability v3 sha256:bf679441545c42bbdefea40f799f17f5f6dfafd2c1d082bebe459123f5504bdf VersionId BcyZHN6iIA5xKCSHONaBIL_IqJ0HNwl0 |
| `same:bishop+parasite` | King+Bishop+Parasite vs King | same | `kbishopparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,965,588 [0] (5,013,372) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,355,020 [1,304,236] / 14,332 [14,332] | 13,965,588 / 5,013,372; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:ecf8b59905ed5c9cb335015e180a97ec1da2ddbcf0cad5d625ac08b7e60d59e4 VersionId SIG6fi_6Ud4NQ9JtjAzu87gOUjc3W3Ex |
| `same:bishop+devil` | King+Bishop+Devil vs King | same | `kbishopdevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | This family is deprioritized behind recovery of the full lone-Devil stateful class. All three current supervisor records are explicitly paused and non-advanceable. A1 completed all 512 reverse-spool shards and replayed 830,001,152 of 19,568,521,291 graph states before its one-core v25 continuation was stopped cleanly: its 154,000,000,000-byte key checkpoint, closure marker, and empty frontier remain intact, and no fragment exists. Stopping A1 released 240 GiB on i098 for the authenticated same-Ghost/Ghost certifying solve; it cannot be auto-restarted during the sprint. B1 retains its ply-16 checkpoint at 25.155 billion states. C1 v30 authenticated its local-NVMe copy, rebuilt the 27.209-billion-state exact index with 48 workers, then failed closed when continued expansion exhausted the 40-billion proof gate. These Bishop+Devil checkpoints are distinct from and never evidence for single:devil. The model admits only Minions spawned by the indexed immobile Devil on its first three ranks and retains the stateless Bishop through Devil capture. No completion is inferred from launch, index rebuild, spool, or graph-replay progress. |
| `same:bishop+sludge` | King+Bishop+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:bishop+sniper` | King+Bishop+Sniper vs King | same | `kbishopsniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 9,193,147 [0] (24,947,234) / 0 [0] / 20,564,602 [0] (21,210,857) | 0 [0] (6,438,432) / 7,794,178 [9,255] (7,789,011) / 26,944,526 [5,090,249] (26,949,693) | 29,757,749 / 46,158,091; 34,738,704 / 41,177,136 | S3 table sha256:d1a2ef8b56d19837eb6289ae552e046a77be83a4ee0c6526e543ddbb3f631b43 VersionId BsUs3M7FEPw8KOrAq79jnX8D2tkSqO8R; certificate sha256:ecb456b001e79e57575d5cad46619f2837a44283e19a3bf7b6431bd8ab18a536 VersionId ld3EjZepRPNQymYlGuB6EEPEzq79p7ob; reachability v3 sha256:54d0bea98d8e5e81078743c00af91ad8f8943c86cdd28c51f577e4dee98b1b15 VersionId b_m3gik5XJYnuoU2D4Q4pGflm5X2FtMk; result sha256:74781f36c5c28c632f2d843e302b71327a17bd6ac76b27ffa6802e9649aac58c |
| `same:bishop+prince` | King+Bishop+Prince vs King | same | `kbishopprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 13,965,588 [0] (5,013,372) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,113,394 [1,253,408] / 1,255,958 [1,255,958] | 13,965,588 / 5,013,372; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5da4a861ece551f54488b0ced93d504433a5aeb0703fe186df36e1883a9c98e1 VersionId Mv8gq7BWHIVEG.ezVsV3pi4a4NN9C.QP |
| `same:bishop+checker` | King+Bishop+Checker vs King | same | `kbishopcheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 437,760 [437,760] (4,215,040) / 0 [0] (1,596,024) / 28,344,980 [0] (41,322,036) | 0 [0] (1,630,008) / 0 [0] (4,215,040) / 33,003,124 [2,629,452] (37,067,668) | 28,782,740 / 47,133,100; 33,003,124 / 42,912,716 | S3 table sha256:277d28c06f2fef400e6e61045a33d2ea5dc6abe7d596a23acc467d53403d3c93 VersionId VzD77Yv4h3i7.pZ.LwjvoftO9VoPcrIO; certificate sha256:7f0162be1223ca566171ebe696b026af3975a8132df509e6ae2eb87fbe069a40 VersionId prDLC0M7vvOuhpr7fzIjslUI.T6JICB6; reachability v3 sha256:e3080c98079ab0914daea40a7312ffa103cd9c7845c363bead323b7e442c7b00 VersionId LokoBZm4hLc3fb8S_7yk05mF1RhZcukl |
| `same:bishop+giant` | King+Bishop+Giant vs King | same | `kbishopgiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,478,182 [0] (4,150,080) / 0 [0] / 4,658,438 [0] (5,692,260) | 0 [0] (1,142,116) / 4,160,272 [5,134] / 7,984,312 [2,356,398] (5,692,260) | 9,136,620 / 9,842,340; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:44802a9a81b5a776f7efcfc1ec686147d0fdd0ee126d0a7cd11aa18702f23cf5 VersionId 2RTl7gvKso8Gr5VIW33HAjC6jsE7biBq; result sha256:b0b370a4128b71835d8c55c149ad1e88ab6003e62c0eab1e96596d4361086fd7 |
| `same:bishop+copycat` | King+Bishop+Copycat vs King | same | `kcopycatbishopk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 24,990,968 [0] (11,521,496) / 0 [0] / 4,016 [0] (1,441,440) | 0 [0] (3,098,976) / 28,959,680 [2,180,672] / 4,457,824 [4,444,168] (1,441,440) | 24,994,984 / 12,962,936; 33,417,504 / 4,540,416 | S3 table sha256:ab9d2be90a977c50cdef22f921006e782005aec6482d09e22b08549989a5fb13 VersionId rudC.rFuGy7SCcH4oxSy2mhvq92QmJLI; wave certificate sha256:e7bd67b7eba1977fbe23a0082fec2235e747d7b6ea0a1d36a619fea75a16b379 VersionId S9NibcQK77cLU2pcH3p2_c20SQf534Lw; reachability v3 sha256:33d693185bb7f50e7c677a320b7785c9d63fc69d61b483cd9899e55ae1a820d7 VersionId 4f8.s4ZVaIuw5REqbf0XwjcCXk4vz2JX |
| `same:bishop+angel` | King+Bishop+Angel vs King | same | `kbishopangelk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 0 [0] / 0 [0] / 45,855,516 [0] (11,081,364) | 0 [0] / 0 [0] / 53,717,664 [0] (3,219,216) | 45,855,516 / 11,081,364; 53,717,664 / 3,219,216 | S3 table sha256:8a04c509a756926b2818ee24bd79ea52e23e08d04ed1dae025c94610d73f5d5d VersionId 4bHUzBzkgs8sI9R6Y1Kmt6XM0NKoAtsQ; certificate sha256:840c33f9393e1f681048cb802ee11ee8a26751423beef7352a3d74c78ae9f727 VersionId cXDnph7WjiJHKe8O2jMj6TIQZ9T6iDSt; reachability v3 sha256:987c3fe764d0f23d6a265ab6cc7fae430eee0f41f2a801da3d3f19ceb7faf1ea VersionId vFM5gX_WS21LG1kbBYNoxBTbjRag7bI. |
| `same:bishop+fisherman` | King+Bishop+Fisherman vs King | same | `kbishopfishermank.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (3,693,788) / 0 [0] / 15,285,172 [3,042] | 0 [0] (1,609,608) / 0 [0] / 17,369,352 [2,578,596] | 15,285,172 / 3,693,788; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:21bd0dbc9fda34d693fdf1cfe7f5121d069cf078a803f8b0927d0c830f1baae3 VersionId bANbfzvkl3eJdgd.QR6l6xPTiiBCi9.y |
| `same:bishop+dragon` | King+Bishop+Dragon vs King | same | `kbishopdragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,325,918 [0] (6,653,042) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,094,422 [1,078,918] / 1,274,930 [1,274,930] | 12,325,918 / 6,653,042; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:da0e326b3744293260118ed6d866a1864c546f9b44767a1396473626e4509560 VersionId OvB5PZn9wAA39gkUZVXKB2qMp1hhfIET |
| `same:berserker+berserker` | King+2 Berserkers vs King | same | `kberserkerberserkerk.uftb` | **CERTIFIED** | 1,897,896,000 | concrete | 95,908,728 [0] (853,039,272) / 0 [0] / 0 [0] | 0 [0] (80,480,400) / 868,188,006 [44,665,896] / 279,594 [279,594] | 95,908,728 / 853,039,272; 868,467,600 / 80,480,400 | Certified table sha256:a1f07eae1bc073927fe220cae1df17428b3b3b94183614c6f18698dafa3529ff (2,372,370,056 bytes) completed with 95,392,004,308 edges, zero exceptions, zero Bellman residual, and zero final verification residual. The version-pinned S3 artifact is VersionId `qO5pVM0cQJT91GqwRmxWcYKyhyFTbaF9`; result certificate sha256:d9ad3f8077a024881fc815f888cbd31439eb9c8b483cc12213acf438d0f42e61 is VersionId `ZPTiJIHsz8Inwr.PClouA8PbdvzGzKxx`. Both exact versions were downloaded back to i03, full-SHA verified, and compared byte-for-byte with the local certified outputs. Allocation-bound manifest sha256:afe2e24beb30c7a6703bc4811d2fae3f1c0505a7bfb82246a632f597606f4fc4 binds the immutable original frontier nodes sha256:2fdf0b96d9d14fdb65bfa63bdc7e9820787c1416c4a7df6e4e2d3c2f9e9d83d3 (15,183,168,000 bytes, S3 VersionId `_oWIZINuGQLoTFu6ALG5lAFzJkjY9_Q3`) and degrees sha256:992fa27219815c9f1a17a535a100fe201ca6e246e2dbb449616b671ba37f2523 (7,591,584,000 bytes, VersionId `pSJZzbK9T16_QkGb.Gzip79BX8dCMMJU`). The v12 post-solve working nodes are a distinct retained plane, sha256:fc7abddd9b1c5455afa21825e04694dd75b0fc5e6ca786ded770423d65d68118; its combined checkpoint is sha256:1591012d1eb14e29e2bdf06290ab422f471fb82e5106ef5a640e5ef8c598fb5f. Compact source/proof bundle sha256:87be5f98bb358a94d418583ffa181b926eaa8d448dce82d166f19a45f155a9da is VersionId `eUevE0Zv3jaXftHOKyEioBArbKupIUhe`. Memory-gate failures v9/v11 are preserved in exact restored archive sha256:a7ec5e8d6b763d1bf8b0b6f70bae58e95058914669a4571cc144ccc580f263ec VersionId `GEjrNYk9YX90eoPolqaaGyyJkuu6dGAc`, with certificate sha256:422b3503272160c82db0e5b9f458202276bf0e9c69b4bbf6393e212881b07034 VersionId `cg8nPDvNDr36aPdlcn0Fv4FI.SVu3_YS`. After zero-open-handle and retained-plane hash gates, only replayable v9/v11/v12 offsets and predecessors were removed; XFS recovered 255,471,980,544 bytes in the two measured stages. All original/post-solve checkpoints, outputs, resource records, and proof logs remain retained.; output sha256:a1f07eae1bc073927fe220cae1df17428b3b3b94183614c6f18698dafa3529ff; reachability v3 sha256:086f64af94e2507e40e1476886edd3f4bbc39904e5c9c24305d911f64e304b2b VersionId 8hgWy6ykYFjK7Cc5xs1QuP3UccPEJ4Fv |
| `same:berserker+bomb` | King+Berserker+Bomb vs King | same | `kberserkerbombk.uftb` | **CERTIFIED** | 379,579,200 | concrete | 49,719,592 [0] (140,070,008) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 172,804,228 [12,442,680] (677,600) / 211,692 [211,692] | 49,719,592 / 140,070,008; 173,015,920 / 16,773,680 | S3 table sha256:3addd3e350e38e9234eee0800c56cefbe7e89f403c5cd0288677fe29906a82ab VersionId wB7rq2TjG0O6HGb14C9Fq8G5zrsduq6u; certificate sha256:82e18a2b210253cb399c9091cda30f9aded9f295137c637170c3e7f33bf834ce VersionId tulbqrZAz.5MooPj5XUWFjphdAB2Buvl; reachability v3 sha256:a192d22af8dd9fda883fcd2e086b4ae2ff317d8579af9f419095ab7ba1d3217d VersionId eSlTd7TunL.O_uY17Mw5gXempq.ENYVB |
| `same:berserker+ninja` | King+Berserker+Ninja vs King | same | `kberserkerninjak.uftb` | **CERTIFIED** | 379,579,200 | concrete | 45,167,328 [0] (144,622,272) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,318,006 [15,622,752] / 375,514 [375,514] | 45,167,328 / 144,622,272; 173,693,520 / 16,096,080 | S3 table sha256:b11c08418b1ba4b9e49a04e10d867f70b8077f6552759daddc9c754a1a5ed212 VersionId chMgMSI9Kx6Ovr9X_8vL_p8GhQeu0gz1; certificate sha256:c199e4c739da77a53a05d9deb72022329fc1d56b43767f1cfbd1cc15dc80ce63 VersionId 6cTy4ybUUTRtZKkPn1KiAzl3dXqD6bjo; reachability v3 sha256:b52295e5d1e5b1452260ec079b15ad52de98df17d4eabb1958d6a89b68bcbd64 VersionId XVQghfhqLH8ZBaOrmX476ck2Z3l_uYXG |
| `same:berserker+turtle` | King+Berserker+Turtle vs King | same | `kberserkerturtlek.uftb` | **CERTIFIED** | 379,579,200 | concrete | 54,012,262 [0] (135,777,338) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 160,059,100 [4,412,584] / 13,634,420 [13,634,420] | 54,012,262 / 135,777,338; 173,693,520 / 16,096,080 | S3 table sha256:050b5173679ca8fc53cd5d08c18f082b30695596a0f31bfabfc99b6dfcb597e2 VersionId hJUSFXZwMPX5rfAQ2qNmOdU4dtti2Wfw; certificate sha256:21b3a0d027946adc22a13775c59ac3611561f6555c505d11a048dc83efb5d3e2 VersionId aeT7zj_MFj4XWc2hwD7veyrbFu9VA_Lm; reachability v3 sha256:33884ab882b25c15c2db355dcfb330ea4eb4ddc269fc5cd80ff4223407b0947d VersionId AEl7votqzfrkBvXDt611O0fRO79RAmfG |
| `same:berserker+ghost` | King+Berserker+Ghost vs King | same | `kberserkerghostk.uftb` | **PLANNED** | 759,158,400 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. All 64 fresh corrected-rule shards are exact. The compositional merge authenticated every legacy shard marker, proved gap-free coverage plus exact metadata/index/stratum rebasing and conservation, restored the full graph without the projected four-day native replay, and SHA-bound all five merged components: 4,929,600 geometries, 11,715,731,784 edges, 1,869,966 strata, and 189,048,898,944 block bytes with zero gap, offset, byte, or conservation residuals. V3 then failed closed before allocating solve state because the hard 97-GiB gate incorrectly counted that immutable 189-GB transition input as a new mutable allocation. The gate now limits only the two BDD arenas, compaction maps, and force roots that the solve can create, while separately reporting the total on-disk footprint and retaining the exact free-space and physical-memory gates. Solve-only unit `ultimatefish-info-substate-v1-kberserkerghostk-allocation-v4` authenticated the unchanged strong payload, skipped replay, and passed both corrected gates on i024 CPU 0: 24,407,290,184 bytes of new mutable scratch, 215,457,948,648 bytes total footprint, 255,633,002,496 bytes available, and 13,709,640,616 bytes estimated ordinary resident memory under a 33,071,248,179-byte limit. At 2026-08-20 03:47 UTC Bellman iteration 1 had reached 60,000/4,929,600 geometries at 23,666,665 BDD nodes and 12,973,907,968 bytes peak RSS. Fixed source bundle sha256:0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856 is S3 VersionId `QdBG6EaSlckJI_v1rCztxgmbr0fOYW0L`; native ARM64 binary sha256:52a9da514544d6879b45b0ec1b9461cef313920c53a33f4c7266fb81086fef1d is VersionId `MzEBuccF4St_JpmF86ya1VY773l3THOW`; wrapper sha256:80043effb55515ace8b72a83c72ce49dc4a9c31405194dd65bb8f47c38097236 is VersionId `15Vp5oTTLJfogMj54bwN0EBH6lZss2T6`. The old replay, v2 format failure, and v3 allocation-gate failure logs remain preserved and non-certifying. |
| `same:berserker+mage` | King+Berserker+Mage vs King | same | `kberserkermagek.uftb` | **CERTIFIED** | 379,579,200 | concrete | 56,532,476 [1,765,376] (133,257,124) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 159,513,200 [4,412,584] / 14,180,320 [14,180,320] | 56,532,476 / 133,257,124; 173,693,520 / 16,096,080 | S3 table sha256:bf40c3c72305878a6b759cde688ee78eb0f86edee6a1c1b2bc0f45b7a9b5eb79 VersionId IgTHom.i.XyOoETzBGXEPVkNLDovJjet; certificate sha256:73bbf1f8b7042b82ada6398fd9c527a3a07293c397e982fbecbe16b869be706c VersionId gY71X_Y_0QKolrKNbCx3d6hZK4CiDt3P; reachability v3 sha256:24b3333010393343787d59563b86e1b3e6768c3ce94a5b4d28761c49c2a2493d VersionId Wxr.tX1KpyrtzxDmHAllkYd8Lkp1gmk0 |
| `same:berserker+penguin` | King+Berserker+Penguin vs King | same | `kberserkerpenguink.uftb` | **CERTIFIED** | 3,036,633,600 | concrete | 76,725,372 [0] (163,761,180) / 36,608 [0] / 994,712 [0] (1,276,798,928) | 133,596 [13,904] (18,815,360) / 198,562,130 [4,448,476] / 24,435,074 [22,911,504] (1,276,370,640) | 77,756,692 / 1,440,560,108; 223,130,800 / 1,295,186,000 | S3 table sha256:d691626491801921a95ebcb8e977cde2cbd0f56b61b4264520deaf8c76837592 VersionId jslzQSNuqakvbgizwkyyMU39rBBFV0uH; certificate sha256:fe2cdef20fa64070c5220aa0aac349dac263fb1f69c4a6736fce6c2a7a55995d VersionId K.dOjWnVVCgFwfnF9AYitxYXSMpsMbvz; reachability v3 sha256:71feb586068fed320f233fe8caf0c552076f7980c454951d9fa7eec956f73297 VersionId D51EkN9WblWCV8M6gLkoZG8dmxJqNjQx; result sha256:5d17089a189b99acda411f805ae5ea0e2c90907d49ba7e3b5e7c10c38002d438 |
| `same:berserker+parasite` | King+Berserker+Parasite vs King | same | `kberserkerparasitek.uftb` | **CERTIFIED** | 379,579,200 | concrete | 51,853,836 [0] (137,935,764) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,641,360 [13,042,360] / 52,160 [52,160] | 51,853,836 / 137,935,764; 173,693,520 / 16,096,080 | S3 table sha256:ebc724e189304ee1879b62402d5325ae562c01988502761bef1f03eb9c87ad21 VersionId ud0Fxo9WLrUaW7etaDMuH3ZQ66NFxq6H; certificate sha256:c9298115c73d080535ba9ed814e8ef2099d4c499b0de529c8612a7dc56bf8f41 VersionId BCMEZFO9oOcwhJVbPXYiMBZhZiS98mv6; reachability v3 sha256:a232f2d766062fc05230f51cfb02938b39b42f8ed20ce4a0be6fa26a3957ea19 VersionId O3W7JsFPL4Bp2YFxKyZFQ.sEZfaHPaDu |
| `same:berserker+devil` | King+Berserker+Devil vs King | same | `kberserkerdevilk.uftb` | **PLANNED** | 455,495,040 | concrete | — | — | — | — |
| `same:berserker+sludge` | King+Berserker+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:berserker+sniper` | King+Berserker+Sniper vs King | same | `kberserkersniperk.uftb` | **CERTIFIED** | 1,518,316,800 | concrete | 109,978,888 [0] (649,179,512) / 0 [0] / 0 [0] | 0 [0] (64,384,320) / 319,869,415 [8,930,916] (319,770,586) / 27,517,625 [27,517,625] (27,616,454) | 109,978,888 / 649,179,512; 347,387,040 / 411,771,360 | S3 table sha256:7a7345b2d97cfd62d37ef6f7bb53297f20cdb93f07685b9157c051942dfe2713 VersionId Z45tfbDwMueoc8qvxq1Opatq4C3jGCqr; certificate sha256:d255fb40f5aa28c31ce1bd9105eb9718d2a2e8fe3bd51051097239dfd4d9150b VersionId DS5hcmpjZjQxlegEgTJ9TM9VbBKbuX3l; reachability v3 sha256:800fa839c6fc33a6d31aeae52b2513db961109bfed13472c0c27f5dc78c2dae4 VersionId yUGxzJErKsCl_cgwRRja2iosNUFxkilM; result sha256:7e852f5e1209f5a83dc7d27edb06c90ca2ad493e427b664bc2e71d8683d25fbc |
| `same:berserker+prince` | King+Berserker+Prince vs King | same | `kberserkerprincek.uftb` | **CERTIFIED** | 759,158,400 | concrete | 51,853,836 [0] (137,935,764) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,641,360 [17,454,944] / 52,160 [52,160] | 51,853,836 / 137,935,764; 173,693,520 / 16,096,080 | S3 table sha256:1ba9d98040c20330b77cf894c80cf5f65f3f287b8fcf803bbb45287f1cddbb1f VersionId i8OWISOcxzrrNC72eLViP44_5nNtH0vp; certificate sha256:bc7da787f6bb420b14197a77ca49fa97e80fcdad090078250ed1e6377c2e54b7 VersionId ikp8RRxe5sUAWfLAXSO4FaDwBor3VUMX; reachability v3 sha256:ae8e53d589523b586227ee0d3eb22c35b3d16d163f45e15243b6cf64209a58a4 VersionId 1IDNiaaN3s90v2UR2pmIIq13c1IljZPk result sha256:a9789c4518cc959aa3cd32229b98388697dde88d421d087a6d1922b7cb962867 |
| `same:berserker+checker` | King+Berserker+Checker vs King | same | `kberserkercheckerk.uftb` | **CERTIFIED** | 1,518,316,800 | concrete | 113,651,363 [8,426,880] (274,354,717) / 0 [0] (31,535,520) / 0 [0] (339,616,800) | 0 [0] (32,192,160) / 303,777,533 [8,411,072] (284,676,599) / 26,253,707 [26,253,707] (112,258,401) | 113,651,363 / 645,507,037; 330,031,240 / 429,127,160 | S3 table sha256:2fc4257245ca71d92890041f7a9839d4a9ce6fd86993a035d63688f333823d06 VersionId YwZ858F1ByDKjxXp0UyskTOyOlAZugo7; certificate sha256:00dcce4ef1706d9f9287a4de0b33e46f10c3b9795d21dc5b650bdc42e693611b VersionId ZL1EwaTjvN.1JKgoil_v4JEar6i1R4XL; reachability v3 sha256:41768112399ec8c08674de73a896d5fe61e7e50e7f207583e6c3c17846089b65 VersionId WK50azT1abieb5dh_meTWLZJekwsPJDM |
| `same:berserker+giant` | King+Berserker+Giant vs King | same | `kberserkergiantk.uftb` | **CERTIFIED** | 379,579,200 | concrete | 33,876,688 [0] (98,990,312) / 0 [0] / 0 [0] (56,922,600) | 0 [0] (11,421,160) / 112,998,120 [5,221,108] / 8,447,720 [8,447,720] (56,922,600) | 33,876,688 / 155,912,912; 121,445,840 / 68,343,760 | S3 `8a1b1797…` / `EXkOI6JOaP.5gTfjFTSBYEfx9cfFBtS0`; output sha256:ffa499e631d5a245dc289e5b728c89d1fd241b642037c740bd263869ebe21235; reachability v3 sha256:69101792cde19ebf8aa51830d97864863f3bfd48c11deff24f5623c6622cae64 VersionId zhnV1KCokD1q4KTIscsNiHGNzUZ4aEq8 |
| `same:berserker+copycat` | King+Berserker+Copycat vs King | same | `kcopycatberserkerk.uftb` | **CERTIFIED** | 759,158,400 | concrete | 92,601,592 [0] (272,563,208) / 0 [0] / 0 [0] (14,414,400) | 0 [0] (30,989,760) / 334,009,432 [39,172,584] / 165,608 [165,608] (14,414,400) | 92,601,592 / 286,977,608; 334,175,040 / 45,404,160 | S3 table sha256:0434d07979d8b0c1edd30e3e2b48b4c2fc929d52d7ed1170b01937db2eac8016 VersionId YZxTQFtPQQnCRVkbjQu4lSu6yVYH46Mv; certificate sha256:26f3587a7ce1e95681aeaa62299d4338ab6d603849be7984af22ecae80153898 VersionId CNrPq2kJUeT.RbHpliCxi2Z0_WBKF3Dn; reachability v3 sha256:fc787a81a62810880669a6981cc8634a9230f988d120c5c8f80e53a4e357e5fb VersionId cE_fEe8EkSEJPe9nWmtAaUeqPiS0Cfdk; result sha256:857585c32b786f183d2cfdf4b34b5ea330c7a64f1de7d41ebd7ceb769699a9d9 |
| `same:berserker+angel` | King+Berserker+Angel vs King | same | `kberserkerangelk.uftb` | **CERTIFIED** | 1,138,737,600 | concrete | 169,597,428 [0] (399,771,372) / 0 [0] / 0 [0] | 0 [0] (32,192,160) / 508,116,840 [22,364,484] / 29,059,800 [29,059,800] | 169,597,428 / 399,771,372; 537,176,640 / 32,192,160 | S3 table sha256:54236276a8fbd87436bc0637998a4d9f1aef6a5633f146ac8ce328f85b55f532 VersionId oIA6f7w4yJtUTiiA9ZpIuVA941EUoSVM; certificate sha256:24d09aa2a47c2fa3c16e3dd181870d3fa5272802877174d229bc3d0640e4f75f VersionId WncHsioVbqsGqZU3E3RNDh1yXgWoar4E; reachability v3 sha256:458165a466f731768835e8fc3fbc6fd3f154395e8ab033f0b5327aa47c2fdf6e VersionId 4a1wEukgCMztyOo9H_BnSWL__UAuW9Sb |
| `same:berserker+fisherman` | King+Berserker+Fisherman vs King | same | `kberserkerfishermank.uftb` | **CERTIFIED** | 379,579,200 | concrete | 56,532,476 [0] (133,257,124) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 159,513,200 [4,412,584] / 14,180,320 [14,180,320] | 56,532,476 / 133,257,124; 173,693,520 / 16,096,080 | S3 table sha256:2a5d58237cd156a8ecb8047c23de8f76eaed9511548b6bb38cba997d3ac816e8 VersionId Hx0CMMVSIiPkz0udYDOEXXffUz92xPsb; wave certificate sha256:b58423335f143ca3d9af616faf5d93edb42f7ca492ac54ee5c9cec496960a866 VersionId drDuTvlkKpmlYtrG48h0uC5cSCW1JGOQ; reachability v3 sha256:61c2f775e6930edab0f4854d942b8e87a7c8286644889bed5b2017ac20352037 VersionId 8Uq68QcIunYn5IfGasZVz37_C_L7fPxL |
| `same:berserker+dragon` | King+Berserker+Dragon vs King | same | `kberserkerdragonk.uftb` | **CERTIFIED** | 379,579,200 | concrete | 46,276,232 [0] (143,513,368) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,452,674 [15,796,064] / 240,846 [240,846] | 46,276,232 / 143,513,368; 173,693,520 / 16,096,080 | S3 `bbbfcb3d…` / `ziGJ3OQj8DQorqRiy9uVx07S5uk5Bial`; output sha256:df2ca64b8c46824d99b938ded4b9fb190f54ca590349359da758684302839c84; reachability v3 sha256:865b89e592bb3c3d345e3cd42aa27dbb0c81f608f4275a4c206d5fa36a721857 VersionId hW1q1cxbxiy0PlBfYEXOoca5EBCoV2eE |
| `same:bomb+bomb` | King+2 Bombs vs King | same | `kbombbombk.uftb` | **CERTIFIED** | 18,978,960 | concrete | 6,651,352 [0] (2,838,050) / 0 [0] (78) / 0 [0] | 0 [0] (804,804) / 8,582,770 [0] (70,112) / 31,794 [31,794] | 6,651,352 / 2,838,128; 8,614,564 / 874,916 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:50994105ecfee1296227ebb717230e392b43dd175eee143c057e03b99638670d VersionId 2fqVdPsyCTR32Z3fpcGFH0sLJZsaYvY_ |
| `same:bomb+ninja` | King+Bomb+Ninja vs King | same | `kbombninjak.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,012,790 [0] (6,966,170) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,219,704 [1,244,268] (67,760) / 81,888 [81,888] | 12,012,790 / 6,966,170; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:66de4c5ab241c1063bbc9744adaa321630dede4d191e9f704a862acc92aee5cd VersionId 1x2cTYmEtKcSyRSxyUNuNwL1CNEgRs9x |
| `same:bomb+turtle` | King+Bomb+Turtle vs King | same | `kbombturtlek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,512,116 [60] (4,466,844) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,269,840 [1,244,268] (67,760) / 31,752 [31,752] | 14,512,116 / 4,466,844; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:761edc20aab45753ab559f68e5ff35db4212eef96b60b0c05b157a83ca85aeb7 VersionId P7Hxo2vQZzAGyTPG2NsU4vAZSJ4fZBrR |
| `same:bomb+ghost` | King+Bomb+Ghost vs King | same | `kbombghostk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 29,117,000 [1,314,704] (8,840,920) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 33,065,732 [1,304,458] (1,612,756) / 59,232 [59,232] (984) | 29,117,000 / 8,840,920; 33,124,964 / 4,832,956 | Exact 17-iteration corrected-rule solve from source sha256:22a25e19e8e0a4b90dce62c155fdc32d50babd9dfad0a86861dfbb8f31583283, normalized source sha256:61be4448d7871521ae4eaf2a9623ab993af5428791201a6a0de77dbf30f14c81, model sha256:7eb6b902171543a43b989ce6a6e2cd6c604d332c474d3c6a01ccb0b7f30f2aa8, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Transition payload sha256:3310758597e79c0a5ba5546ba69f102d58ac3105be7e88e2768d32a73ae974f5 and arbitrary sidecar sha256:8e2a673c5570d8d38bc3d632b7a2e3d3b3e9cff3a012df15dd7925e15b68d71b; Bellman, rank, monotonicity, singleton, compaction-root, structural, grouping, conservation, dual-force, and source-remap residuals are zero. S3 information archive sha256:71e1dce41592228d162371959e3654c430546fc542bb752950fe707d3939a09a VersionId `GpFc9pY8kTI1xosGdxI6JYbi8UT.AbLJ`; certificate sha256:40d2439a6a1a6e1701a6bc17697b5019cecbacb54de360fa771fedf43ad2d5dd VersionId `.63RVuGWmQ_Q7ttIQrbvh.YaWrn0tEri`. HEAD, exact-version download, inventory, local restore, and S3 restore residuals are zero across 70 artifacts; source evidence remains retained.; information trivial v1 sha256:2bd93a7e2267c764adb86b07b48f12f9b75f22000856aa2bdaec814a34f7dfe3 VersionId FdQzLQbI3zIbS4vj8hGUi0CtcmM5LyJx |
| `same:bomb+mage` | King+Bomb+Mage vs King | same | `kbombmagek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 15,215,852 [0] (3,763,108) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,271,480 [1,244,268] (67,760) / 30,112 [30,112] | 15,215,852 / 3,763,108; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:3935c091091b4c3fed444fb4cf7645af58047a306a20a1f7571c365738325f2a VersionId rFEwJIY.D7fJFlkQHQXTzWUzd.MVAhZB |
| `same:bomb+penguin` | King+Bomb+Penguin vs King | same | `kbombpenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 18,139,270 [0] (5,865,152) / 8,124 [0] (1,730) / 78,798 [0] (127,738,606) | 30,604 [0] (1,882,424) / 20,497,518 [1,244,268] (77,916) / 1,704,502 [1,585,008] (127,638,716) | 18,226,192 / 133,605,488; 22,232,624 / 129,599,056 | S3 archive sha256:176c6f525638bdf2ef5dcc83e81bae8153f10ba53e6fb061b215314a1b54a12e VersionId Tlf97_M.fyHYIxJIa0XO3TaNSQqW2VaH; result sha256:0bdb20ab3ed299d903fc1a9921150c77fcc7d80f1983f51c3672dc597550145f; certificate sha256:e12fcf04e0d3977b568e85aa7ea82c4ab196444596334f0f074a150e97a70f3e VersionId 6bYrqwai11FbeziQw_lHpHj0Z9wwg2NP; reachability v3 sha256:1b11d855aa6d1c14b49bf6fe1dd902279735596cc94cb2b7c0f0d7a32871cd9d VersionId EUIRqrm_8WlmEEpupkRGpvfcCrB7r03Z |
| `same:bomb+parasite` | King+Bomb+Parasite vs King | same | `kbombparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,901,148 [0] (5,077,812) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,263,608 [0] (67,760) / 37,984 [37,984] | 13,901,148 / 5,077,812; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:d53709785c67ba132edfe1c8e02b05aeae00f65627e553f44d91dbe34d876d9d VersionId pW8tbOdnqpq1hJs5nIxDXlwkmxYSl77t |
| `same:bomb+devil` | King+Bomb+Devil vs King | same | `kbombdevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:bomb+sludge` | King+Bomb+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:bomb+sniper` | King+Bomb+Sniper vs King | same | `kbombsniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 29,596,516 [5,263] (46,319,320) / 0 [0] / 2 [0] (2) | 0 [0] (6,438,432) / 34,536,086 [2,488,536] (34,802,038) / 67,098 [67,096] (72,186) | 29,596,518 / 46,319,322; 34,603,184 / 41,312,656 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1360122906b3f46ad40427175fd9e77a0efa2bd850b629e3d9723e753ce68c07 VersionId WKycHk.w2SEQa_HvwNwNaMFZT3ob7UKb |
| `same:bomb+prince` | King+Bomb+Prince vs King | same | `kbombprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 13,901,148 [0] (5,077,812) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,263,608 [1,244,268] (67,760) / 37,984 [37,984] | 13,901,148 / 5,077,812; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:b5348d5e46d488e6226051e47f6d844b6534a54ac72ada6c06988d60cb1a19ca VersionId UcDZviIKdr9oqiiNCoFNbaMD4iS5BQeP |
| `same:bomb+checker` | King+Bomb+Checker vs King | same | `kbombcheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 29,074,749 [843,378] (9,725,859) / 0 [0] (3,286,402) / 0 [0] (33,828,830) | 0 [0] (3,219,216) / 32,817,112 [2,397,809] (10,068,704) / 57,398 [57,398] (29,753,410) | 29,074,749 / 46,841,091; 32,874,510 / 43,041,330 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:a798c14047e394dd7e05bbb7fd7012b7e537868951a4052a452e611f2a94e4c0 VersionId XdhbJMN1o8xOr91IyfsO8xGP5wITNKpG |
| `same:bomb+giant` | King+Bomb+Giant vs King | same | `kbombgiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 8,936,526 [256] (4,350,174) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 12,067,988 [1,515,792] (48,420) / 28,176 [28,176] (5,692,260) | 8,936,526 / 10,042,434; 12,096,164 / 6,882,796 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:4f2af47696a1ecd3cb639646f567130fedc0db9ea1b51bed78fe80ba4e5b69d7 VersionId gHzY7tg2hKmM54q0Wno8m7K243dattN6; result sha256:560dcad95f6939eebb225e0f1751b0315a94a4b7bc1e2eb1a48d93229059956a |
| `same:bomb+copycat` | King+Bomb+Copycat vs King | same | `kcopycatbombk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 24,791,896 [0] (11,724,584) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,204,928 [4,393,136] (130,464) / 82,112 [82,112] (1,441,440) | 24,791,896 / 13,166,024; 33,287,040 / 4,670,880 | S3 table sha256:2982ef9e9d6ef502e19473e6df03e159d2573fe1b3a41e81b0da2e134958d569 VersionId Cw269O.8NJV6DKd2mXeEDUAFGXsBsT0I; wave certificate sha256:1d669cf6b9ca13864824ec6b4d572d5c30c4f31564bff3df11e978e8d1676fed VersionId 57MXPXmTV7JERWkdwDi1zje4oN.Y7AwH; reachability v3 sha256:7a3efe5745f52b4cd7e6bf6d33506d7a9f7e863613d258ef085ad747c0f3449a VersionId DiDs1vrKvO5c2yeLnqfsKlRYlBH5cKDu |
| `same:bomb+angel` | King+Bomb+Angel vs King | same | `kbombangelk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 45,647,556 [4,220] (11,289,324) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 53,555,368 [6,259,904] (71,960) / 90,336 [90,336] | 45,647,556 / 11,289,324; 53,645,704 / 3,291,176 | S3 table sha256:6f1c06e25d5b414a4a9faafc7027f5dded8f18fd269ed0c88f0585be4acd06b2 VersionId wZhNtGQFE77lp09vKgayrkAdGUWvbuzs; certificate sha256:1105cbe55c61c78966ea3ba88ea70683879b58328ddf98224cdf6f9665d22fe7 VersionId djfwazTlUnSZzxcyCe_oUJm1OQK67gzg; reachability v3 sha256:89a79c854421922825d2cb1443cf7226632a03f35a2828f2102c92a57aee85b7 VersionId OmDRwJtOk5vLlZfQiRTT90UeIXEomOWx |
| `same:bomb+fisherman` | King+Bomb+Fisherman vs King | same | `kbombfishermank.uftb` | **CERTIFIED** | 37,957,920 | concrete | 15,215,852 [0] (3,763,108) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,271,480 [1,244,268] (67,760) / 30,112 [30,112] | 15,215,852 / 3,763,108; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:42309a06df2e8a88d82671e0d9b554bc1f26f48060f6525492facb5a435c6ca6 VersionId JkGfH9w8RSr2p.NRvgFzk_LpmJVA8Dwb |
| `same:bomb+dragon` | King+Bomb+Dragon vs King | same | `kbombdragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,346,586 [0] (6,632,374) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,256,790 [1,244,268] (67,760) / 44,802 [44,802] | 12,346,586 / 6,632,374; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:49170cad32387845e13b54f9f22d72eb79abadfe53cbf22242fe76ab4025d31d VersionId IM6enMewahuELqHDlc6QsL1r8YyWngCr |
| `same:ninja+ninja` | King+2 Ninjas vs King | same | `kninjaninjak.uftb` | **CERTIFIED** | 18,978,960 | concrete | 5,424,306 [0] (4,065,174) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,609,904 [1,103,940] / 74,772 [74,772] | 5,424,306 / 4,065,174; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:64b6d9231d2b25ff5912784b1733df3ab25d57e44e61469db790206bd454cc7c VersionId P_Y4cwhK1ZhRYD6HEjAFMYhoCkF7MsBz |
| `same:ninja+turtle` | King+Ninja+Turtle vs King | same | `kninjaturtlek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,095,300 [0] (5,883,660) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,961,482 [1,089,700] / 1,407,870 [1,407,870] | 13,095,300 / 5,883,660; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:c1531cc11a620bac4a3cb2c0f35fd6ce6353aba2a7280815a369f2fea9bf6c82 VersionId D4JpQKRNMdRCn03ueAL6LFc3PPQuKw13 |
| `same:ninja+ghost` | King+Ninja+Ghost vs King | same | `kninjaghostk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 26,268,260 [1,171,460] (11,689,660) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 75,164 [75,164] (1,836) / 33,179,904 [3,834,422] (1,481,800) | 26,268,260 / 11,689,660; 33,255,068 / 4,702,852 | Exact 12-iteration corrected-rule solve from source sha256:b9d82cbd6e2acf0092f546ffa9cf81658420657ea74cb3d69bdaf0bbc5cde1b0, normalized source sha256:cd92ca0d14c4e57bc478d542c26dca70aa13731580f6e29a34a8ecdc4956e3e7, model sha256:bfd63efbbcfd5ee79e352f330ba643c10184a3eaf9183ed69da1fff491416712, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Information output sha256:c8ba666a20b8f9b969b41603665264a295f89cf8fec7e0078374569909bae6ae and arbitrary sidecar sha256:6d7cbcc86edfbd00b1ad9efa8f6619e900a98a6195c3faac585b3ab5b7a5d312; Bellman, rank, monotonicity, singleton, compaction-root, grouping, conservation, dual-force, structural, and source-remap residuals are zero. The pre-binding manifest is retained; its rebound manifest passed exact source/model/observation/lower header and inventory verification. S3 information archive sha256:49c9ac06a0b9b2e18a671a9a52fffc6cb6232f68dcc6d6e9c2b08de5a45dc5e8 VersionId `hEXSa83qWuwf7QpWEOmDDAEi8Qkggxve`; certificate sha256:ecb6c791cea70501a59b8186166a5cde51949fd2ae6980e875da9114c7042275 VersionId `jvBt9plo9HKmjonD1g.2ti1SlSRvacY5`. HEAD, exact-version download, inventory, local restore, and S3 restore residuals are zero across 70 artifacts; source and superseded evidence remain retained.; information trivial v1 sha256:1afddd52677a3ffe5cd45844f8844e73507c28bd1769cadcf93fec0da56e34c6 VersionId fxd.byp4WAgVAK0.WimdVeCkSbw79LAO |
| `same:ninja+mage` | King+Ninja+Mage vs King | same | `kninjamagek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,719,860 [314,846] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,914,976 [1,089,700] / 1,454,376 [1,454,376] | 13,719,860 / 5,259,100; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:36fd65f2f795e3890ca98cc23452a3cc1d754303e1dfb5d16d1dd12d9d75fec2 VersionId rOuX_5ksONm_X53UoKcijDi7f4rhUSZI |
| `same:ninja+penguin` | King+Ninja+Penguin vs King | same | `kninjapenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 16,444,626 [0] (7,537,472) / 7,244 [0] / 138,826 [0] (127,703,512) | 28,356 [1,918] (1,881,536) / 18,976,524 [1,093,320] / 3,308,200 [3,093,134] (127,637,064) | 16,590,696 / 135,240,984; 22,313,080 / 129,518,600 | S3 table sha256:09ecbd396ac683592adc9007c39e1c1b36238fbbd8c8534363afbfa1ee960344 VersionId 0lf2_p8fES9FHry1DDBfMbj4_slTYvFt; certificate sha256:2a804ead801050908640a34dfcaa3c719f54c29cfe25be9fe1bd38f8dc58216e VersionId DXF01gs3QTZDNM9nN5b06AEcOrpsgh3c; reachability v3 sha256:1e68a0702f6175a7df3f73f385102176d1cf0164b8fdea7ca78852cac7041c3a VersionId A27S6lQx8f.NSXxEykl8hqgc1yra.7Qe; result sha256:a82383d2ce1927250d8907386d49cf647e284a74095cd273c98ecd5cc7b5826f |
| `same:ninja+parasite` | King+Ninja+Parasite vs King | same | `kninjaparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,548,400 [0] (6,430,560) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,313,736 [1,304,236] / 55,616 [55,616] | 12,548,400 / 6,430,560; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1e87dc7fa65537be4ec74632809af7b902d7cc43eae34a653ef8dae024c26519 VersionId CIARK3rMdjqdud36MQuwEM5PZL9fN5c2 |
| `same:ninja+devil` | King+Ninja+Devil vs King | same | `kninjadevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:ninja+sludge` | King+Ninja+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:ninja+sniper` | King+Ninja+Sniper vs King | same | `kninjasniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 26,683,127 [0] (49,232,713) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 31,901,245 [2,189,403] (31,890,547) / 2,837,459 [2,837,459] (2,848,157) | 26,683,127 / 49,232,713; 34,738,704 / 41,177,136 | S3 table sha256:2852e3afc9b8a7e3de530f23e18e0acd3857ed66fb6eed95a4a3c009d01d4282 VersionId zBo8W9BcrVE9xcrXinBsNH4EAn4IE1Mj; certificate sha256:adb1529bbfbe789add26b6ee84fff2f102b4bd7c1b084c525816cb25663e3e4d VersionId iyK1zJmBOxssq7zyR_iXKLcZ07VOyvus; reachability v3 sha256:ea19bda546f723629964bc7f3d0b4b383b23fb17c87ac90f0ed2f89332077522 VersionId wXT8izXQqtj4OCp.3phQoLcr4WxTrgV5; result sha256:dbeb3322a4e68b8253e8807185ab73e2b3c94c446c15706b9dd3f7c35943082a |
| `same:ninja+prince` | King+Ninja+Prince vs King | same | `kninjaprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 12,548,400 [0] (6,430,560) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,313,736 [2,393,936] / 55,616 [55,616] | 12,548,400 / 6,430,560; 17,369,352 / 1,609,608 | S3 table sha256:052d02e33db814e9d2e2f2142afc23a2515df6ce756656a1593287c50b8e7aa1 VersionId hdbitaTYp.RkL9kGLL2zuMScQMJTamA3; certificate sha256:3c2b9765294dd4cc8902f39aaddba13e8617f8fb8c296318fda8019b5cb43737 VersionId md6XY9CuOJkCEcanA.VH2uEKaCBg_rwr; reachability v3 sha256:a07c56ddb0f924872f3bcf5b5c883945c308ea9d27687fc323bb90bc6b6d6c7c VersionId XCEiOHkWYf_YJc3PGDMNjTvBJAJ7L6v9 result sha256:901ebe3a50367f0e30abba9bed37a69ad4c61b788dc98ed402336bf0d7d0a6fe |
| `same:ninja+checker` | King+Ninja+Checker vs King | same | `kninjacheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 26,310,899 [842,688] (12,489,709) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,299,302 [2,096,754] (12,710,575) / 2,703,822 [2,703,822] (26,982,925) | 26,310,899 / 49,604,941; 33,003,124 / 42,912,716 | S3 table sha256:8ec4c89e52b7628a5368db078ea3e68c49c92ce7c9834504e8cb53efd26758f4 VersionId YlR3mmorxQKPDlnyV1cquPwabdn.nDQO; certificate sha256:95090d95b3052f381feb1e6e5a1525b4867fbd76c8f34826b934b21e73f35f89 VersionId 9W7MQYr31XNtRMknO2MYVjKQnB7Ye4kr; reachability v3 sha256:aba0aff8f3786095f80856dcd157000081b2779bbfb4e70600747d6227ab1f2d VersionId 9aR6iNSlrSLNIeyCmmmMLpoQHUNytY2W; result sha256:b8f762b36f0d59af02a63a1a6719b6359c8809c92012a0bdff1b7bcb6451a0ca |
| `same:ninja+giant` | King+Ninja+Giant vs King | same | `kninjagiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 8,088,164 [0] (5,198,536) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,260,640 [1,414,934] / 883,944 [883,944] (5,692,260) | 8,088,164 / 10,890,796; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5e75ae1932b963aa33f8436a45a621d58db430c5d2e1cb58fe68262c196865eb VersionId G6q9_ng8uvPTsKH2wX.03pGLWBi6uqZA; result sha256:e2b422f8816df785d4a6e9644e65e5b73d5c3d15cb8ebf1d08aa767f8524e6a5 |
| `same:ninja+copycat` | King+Ninja+Copycat vs King | same | `kcopycatninjak.uftb` | **CERTIFIED** | 75,915,840 | concrete | 22,360,056 [0] (14,156,424) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,285,312 [6,266,576] / 132,192 [132,192] (1,441,440) | 22,360,056 / 15,597,864; 33,417,504 / 4,540,416 | S3 table sha256:6df4c6b6b27eaa97dce939032a730c2d6d61dece24e1c3f8100b76525e63ff8c VersionId nF3a2njTHXnDVx55InMRSOf9y2qSlbq5; wave certificate sha256:1a648375b569cee62c615fa47d76423fb30eb8a7ab2e9c3a383f014c41a7d018 VersionId j2qKOOKP28iDfNXBVA0bjJXTL10fbra9; reachability v3 sha256:bef9f939756ab355e9639a492bebb5ef0e95ebc11fc3aaa1c41a8d95e9b615b5 VersionId QAkj5hW_CMX9tnrZceqWkhjRZzL38PH1 |
| `same:ninja+angel` | King+Ninja+Angel vs King | same | `kninjaangelk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 41,159,580 [0] (15,777,300) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 50,702,652 [5,535,732] / 3,015,012 [3,015,012] | 41,159,580 / 15,777,300; 53,717,664 / 3,219,216 | S3 table sha256:347e06409ceaea2b2699bf1888a979380c87c60cdeeda0b159ee60f2978fc2e9 VersionId CS7DJ5cfxUMLH7PSUYFa.hUrqjbut5gr; certificate sha256:68067b3800f00b86896a1103a7479d4153da5e22565a3feda667105c54328370 VersionId g0JTS8WDqwm4FEp1o9Eoprn7smGzZNZs; reachability v3 sha256:a9b62199ef5820309202cf7d446c6fd0d4fece6469bf1efcf539c5420a87021c VersionId kSLKyfRdjlSkD07j9zC7esSyWTlRAULN |
| `same:ninja+fisherman` | King+Ninja+Fisherman vs King | same | `kninjafishermank.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,719,860 [0] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,914,976 [1,089,700] / 1,454,376 [1,454,376] | 13,719,860 / 5,259,100; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1112df063c3730834b2c3f271b08b2384601d1496513843df327b3e6f68d4093 VersionId amKB9Fj_Vv93kcYh2nkGHgNRYKd4Mybi |
| `same:ninja+dragon` | King+Ninja+Dragon vs King | same | `kninjadragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,131,336 [0] (7,847,624) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,252,356 [2,253,078] / 116,996 [116,996] | 11,131,336 / 7,847,624; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1ee42a7d2f3a092cdd5c7ad670a6249825d070dd0f27cbddfe85920126e7efd1 VersionId E5XTcb6zapvSOlFkoMkCcH1XmnGXkQPo |
| `same:turtle+turtle` | King+2 Turtles vs King | same | `kturtleturtlek.uftb` | **CERTIFIED** | 18,978,960 | concrete | 440 [0] (1,578,876) / 0 [0] / 7,910,164 [0] | 0 [0] (804,804) / 416 [0] / 8,684,260 [1,322,978] | 7,910,604 / 1,578,876; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:296373958df11fcb666e2f0b3b3a216a2bd7f861dc042e80070f78890fa2fff1 VersionId dmBgRDrzl2xzhZZ01.f2RBX3ZSjaEkxG; result sha256:bbb146a2f252eaf0eb40d23e49d0ddda56de183fb202c515ff64a0fd172037b1 |
| `same:turtle+ghost` | King+Turtle+Ghost vs King | same | `kturtleghostk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 31,740,708 [1,422,884] (6,217,212) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 31,894,294 [2,646,488] (125,546) / 1,360,774 [1,360,774] (1,358,090) | 31,740,708 / 6,217,212; 33,255,068 / 4,702,852 | Exact 47-iteration solve with Bellman, rank, singleton, compaction-root, grouping, conservation, structural, and source-remap residuals all zero. UFIW sha256:e498f23c808507b3281e9bacba666556f3cfc2e8fdc5c4269c153fe3f5d5b19f; arbitrary UFGD sha256:78353c13707c7e8807fefd40d37b80c6131ddfc241287dcaec6dfd56127dd707. Authenticated 70-artifact S3 archive sha256:1bda4376102d940e6fde49b57d53c99de04488ce92896599ecddd610dbd6febd VersionId `8nmu7oeFhORIL8f3mfv210k8UYDeowCv`; certificate sha256:9983e17caeb3d3e29ae8c8365efb86793abd521b0132d710652e19434337762f VersionId `34CJnarv90OYYgv7FqxRsPLrM1YKbYbT`; local archive, S3 HEAD/download, certificate download, and fresh restore residuals zero. The legacy manifest omission was repaired from authenticated UFGD/UFGM headers under observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23; the original manifest is retained as evidence.; information trivial v1 sha256:a545723a9c1c970f89dd1a6206bc02dd6977308c9753272dcfcc566637c78252 VersionId jXyc318092q7Bg3txUOEYmoXc3J4xLmz |
| `same:turtle+mage` | King+Turtle+Mage vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:turtle+penguin` | King+Turtle+Penguin vs King | same | `kturtlepenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 66,816 [0] (3,193,810) / 8,738 [0] / 19,642,426 [0] (128,919,890) | 33,770 [2,958] (1,881,536) / 200,974 [36] / 22,078,336 [4,650,770] (127,637,064) | 19,717,980 / 132,113,700; 22,313,080 / 129,518,600 | S3 table sha256:b21d2109978ea2cb67ad993450ba5fc31810e56e77fb93a309865cbb0f4f5499 VersionId 5cbD.qo6aMxE3El0BxdExOgbluLL6w4O; certificate sha256:cffb4f16b4e516050b9e054dfd04216f3fd91dcd2e20caea43361baab4c53baa VersionId EgX_29JXPfZFGjrfBEM9gD5rzB_NtvjU; reachability v3 sha256:19ab6d7251ad9f96776c5348425a4aa58dc97b91719dd3dab57fc8e0a4e63b0b VersionId eXkOVSXGWFV461dPLJhwWemFfD0_RjJW; result sha256:37aed9ce0e03c858ab9367de863ae49078b10887893a4ef84542a2eaa1a87bd4 |
| `same:turtle+parasite` | King+Turtle+Parasite vs King | same | `kturtleparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 15,158,912 [0] (3,820,048) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,364,876 [1,304,236] / 4,476 [4,476] | 15,158,912 / 3,820,048; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9be5b6ab0283edc147601e7a2698d66613741dc14f18675f99ff8ba4873c326f VersionId 96i8GLulwMyjl4d7bqO0mqh2k7QbGf6h; result sha256:eecd62d724dc6c09c12d13d3170a88c951b884bb7c6fed5c522c6085a763a0ae |
| `same:turtle+devil` | King+Turtle+Devil vs King | same | `kturtledevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:turtle+sludge` | King+Turtle+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:turtle+sniper` | King+Turtle+Sniper vs King | same | `kturtlesniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 9,670,663 [0] (20,241,467) / 0 [0] / 22,592,323 [0] (23,411,387) | 0 [0] (6,438,432) / 7,496,555 [8,779] (7,491,150) / 27,242,149 [5,325,140] (27,247,554) | 32,262,986 / 43,652,854; 34,738,704 / 41,177,136 | S3 table sha256:3e3332c5cfcc94777acf40dba9cacf98b218630124e176c7a014fca39263411f VersionId ByVwM727XPtocicfDshEB8b7RkKE5FDg; certificate sha256:79d770c035509b2816fd27f37a6a4e7678f7b94a2939ed1451d72db905eb0fc1 VersionId BfkBD6Gf9_poiLZwCKLgL_TS4rkE1J7t; reachability v3 sha256:5c893f952ea6298c7ba4d46f0fa48b32202d8e512d9b04fb305872150e76ea1b VersionId XeEDk9sa.UIU_ZkBZMr8tDvfRuAlIoCI; result sha256:ea0320d221feada81a3f3095da60d9db8c641cd997ffb000cb58e68538a6fc09 |
| `same:turtle+prince` | King+Turtle+Prince vs King | same | `kturtleprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 15,158,912 [0] (3,820,048) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,006,830 [1,247,388] / 1,362,522 [1,362,522] | 15,158,912 / 3,820,048; 17,369,352 / 1,609,608 | S3 table sha256:61167f42d866d03d4d38f7c83772fba991970126881dd81e062f0698a397b0cd VersionId 0Do40XtK_qF.lXl1mREbCddlyQ7NQ80W; certificate sha256:da81e03bde5432f853a4ae68442197d997ca3f564cc4bbba37882adf95c798fc VersionId 7HOdYANfyHvyub5AESEc_zpppawL5Mqi; reachability v3 sha256:e0321730f212dec909850632381abc1e83dffcaf42171fa13c089070aaacbf67 VersionId LPrG2whLmnj0ckpL6J88UIn8hCIC9_Di result sha256:e1b0d6848e81718bbc9ce4bc5ce2ad0d44fdf36b544987e62c348ccaedd14c5d |
| `same:turtle+checker` | King+Turtle+Checker vs King | same | `kturtlecheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 842,688 [842,688] (5,530,456) / 0 [0] (3,153,552) / 30,771,020 [0] (35,618,124) | 0 [0] (3,219,216) / 0 [0] (5,530,456) / 33,003,124 [5,120,816] (34,163,044) | 31,613,708 / 44,302,132; 33,003,124 / 42,912,716 | S3 table sha256:5e5ca563b48f72aec754be84af5c35b4d89899dccf704edf93dc433eb1b61651 VersionId SzHBpypk8dc2ZVrFz.jKRIo1KtDyhihb; certificate sha256:c02a4f12760400e70dae0895dbad92324bdf93e9059a83c4b081486a90a311d5 VersionId 1UNMX0SIuHGeac7EUQrcdK4n4dUraFaV; reachability v3 sha256:46649cc1abe548c789f1b23669d926dcf3a0419065df87c26dffddaa3600ad63 VersionId oR_0s3G9voF9b1kxZkPq6zKlBiMD7_sS; result sha256:6055a03990fa2a5526698d3c4c2c7af5db120258f36a08fb9a4c35cf454f9dc1 |
| `same:turtle+giant` | King+Turtle+Giant vs King | same | `kturtlegiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 7,109,108 [0] (3,504,624) / 0 [0] / 2,672,968 [0] (5,692,260) | 0 [0] (1,142,116) / 6,387,316 [4,486] / 5,757,268 [2,401,070] (5,692,260) | 9,782,076 / 9,196,884; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:58168084614535ee88ecabe8120e7eb34f5bdfd9b3c38e978d2107356e27478e VersionId vpCLwexVBQGeeHcIAogoZViTSdNvRdqw; result sha256:c613d3210037c0f9ba306e76e5e42531d0910c9ec7b0b387a9a9a73ab8d88eb8 |
| `same:turtle+copycat` | King+Turtle+Copycat vs King | same | `kcopycatturtlek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 27,028,608 [0] (9,475,216) / 0 [0] / 12,656 [0] (1,441,440) | 0 [0] (3,098,976) / 28,584,976 [2,160,712] / 4,832,528 [4,796,304] (1,441,440) | 27,041,264 / 10,916,656; 33,417,504 / 4,540,416 | S3 table sha256:6c51b975f1526a83d68d2eb2d1e6282f83996e9770193ea5dfa6d261603723d6 VersionId IjxJmD0RE3jpu7hOETD79OCcPpsU5BtR; wave certificate sha256:5ccc2162984ec13b9f3e2c1f2d7aefbd1d1918ccf6cf8a8c72523f996be18418 VersionId lEbkyLx5tHl32WbDf4HaFhMeXY365Qkp; reachability v3 sha256:e1ebc1c93976a08a8277d11fbf7b969b58f6113ce750cae2f2b6ea428ab8de5d VersionId teyoBUoC1sChV7MkNPHmIIMHZuz60IJk |
| `same:turtle+angel` | King+Turtle+Angel vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:turtle+fisherman` | King+Turtle+Fisherman vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:turtle+dragon` | King+Turtle+Dragon vs King | same | `kturtledragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,456,552 [0] (5,522,408) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,996,498 [1,112,550] / 1,372,854 [1,372,854] | 13,456,552 / 5,522,408; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:b4d3d426fb5bd27f8f7bc3ada8e79762a404af96b978482d712716cbc6d6c2f8 VersionId 2teXLifjm.pZELoiVEoKVJu8Aj.masSF |
| `same:ghost+ghost` | King+2 Ghosts vs King | same | `kghostghostk.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. Authenticated transition archive sha256:5f740b452a493601737cbd2cd9620ec271305191643eaf8d36cf2ebd5b0201df VersionId 1zb2JYTbHrOQjg88g_HJdHJ6ujs37FEw. The active 30-worker v6 solve reuses the complete 9,739,120-geometry graph and never rebuilds it. Parallel v4 source sha256:5a7751157d992769fb200b5ffb8e907cfad9ae1000c88396ab33e0a2ea72dcd6 VersionId jN6RxS2TgAplV1CkVRW147rEQFNVxwlJ; binary sha256:7dd5ce8fc2d143fa92db54a09e731c0c7cef73facf43dac6da656859c1e2d97a VersionId 9zJpsu1nLMbRlM50wMvQyqEGHtShJ_xK; atomic replacement runner sha256:9ff669a8a9bfe4b28898495d481f9f188dd8e0a457556544715cb4c7545083b4 VersionId Z4AtgFAW4WOBCHZw7qBeBQUcOxmrSbD5; v6 wrapper sha256:5aebc85005b6c19d80cf8fc96b125f584578e099861e15e7c186f4779b1177c1 VersionId N0RheBDyCb7JF1e201xFF0P611DoxZyj; certification manifest sha256:5121479bdb3f568a49afa2e80b19594fd715048d19be91d59110cbe2122b253c VersionId 6twekqLaxSLXPLbM4FO8kxCMPFYC18uA. The authenticated benchmark processed 50,000 Bellman geometries in 5.40372 seconds with 30 workers near full CPU. The v4 one-core scratch/log, failed v3 benchmark, and failed v5 prelaunch evidence remain retained. Automatic verification, preservation, exact-version restore, receipt publication, and import staging remain armed; completion is not inferred before every gate passes. |
| `same:ghost+mage` | King+Ghost+Mage vs King | same | `kghostmagek.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 33,255,068 [3,837,968] (4,702,852) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 31,901,536 [2,612,816] (5,416) / 2,720,112 [2,720,112] (111,640) | 33,255,068 / 4,702,852; 34,621,648 / 3,336,272 | Exact solve converged after 49 iterations with zero Bellman, rank, monotonicity, structural, singleton, compaction-root, source-remap, grouping, conservation, and dual-force residuals. It binds source sha256:5f4abb7b2aa314a1cc872de82113481bf299e84da9ae66fecb512b884c09c22d, normalized source sha256:ac6f995bff48345423e45939fb41f4d8c4a7f72fa2bda69641589bc5bdf557d3, model sha256:3dbb6aa97cb2c97a52e981f7b9078cd664fb23fd805b533ed23ad5cd6fdcaf62, observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23, transition payload sha256:d43963aced1398effd83b4592eeee46ae70cc853b2b728c5bf12cb2b51a12b07, and arbitrary sidecar sha256:d08f1c68a032b52ab3d5c9c7657fb7bef18d14b185edd40021fd5c88503150f1. Deterministic archive sha256:f1f7956714fd6a3ca49da097bb6be2b559f46a11b16b1013cd4f42c0cefa440d is S3 VersionId `OmmOVLX_inwoWB64jTfIjEtPBdKXDoc0`; certificate sha256:cf970d7dd77c4fa50af4caf97b386454cdf6cf8bec924d3a76d56fd6062247c3 is VersionId `GB_9j3_mZ.GyDPdqZm4K0pumIZJyPtx0`. HEAD, exact-version download, inventory, and restore residuals are all zero; source evidence remains retained.; information trivial v1 sha256:a2bc94897a9f3ba859c4446d4a9990fa642b2ee120a09044bbe7ea3f84b02a5b VersionId VO3vPP7QJxJ6CDsKPZGRmc0Xacy.RSIw |
| `same:ghost+penguin` | King+Ghost+Penguin vs King | same | `kghostpenguink.uftb` | **CERTIFIED** | 607,326,720 | information v2 | 42,008,518 [1,746,648] (5,539,142) / 15,612 [0] (812) / 671,198 [316] (255,428,078) | 35,050 [2,654] (3,765,726) / 37,025,220 [2,645,460] (88,476) / 5,634,266 [5,032,566] (257,114,622) | 42,695,328 / 260,968,032; 42,694,536 / 260,968,824 | The fixed point is generated from legal moves and public observations, independently of the concrete WDL oracle. A post-convergence singleton failure exposed the source-plane bug: the concrete generator physically packs a Ghost-primary table as `[Ghost visibility][Penguin substate]`, while the information codec indexes `[Penguin substate][Ghost visibility]`; the old reader transposed the sixteen WDL planes. Corrected source performs the exact bijective transpose, passed native and local exhaustive 1,214,653,440-state role-remap/self-tests, and renormalized unchanged raw source sha256:e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4 to corrected sha256:729f67aaa74d79c44fa375d78b5ef51a0e120f44a48bb67b8b2d0d1e01d5998a with zero remap/count residuals. The corrected resume authenticated all 64 shards plus the five-component compositional payload, skipped redundant replay and merge, reopened converged iteration 62 over all 3,943,680 geometries at 82,528,399 BDD nodes, and completed independent verification with Bellman, rank, monotonicity, singleton, compaction-root, grouping, conservation, dual-force, structural, source-remap, transition, and lower-binding residuals zero. Information output sha256:1a326424c3569058ccba57ef3da8c6ae36188ce481714abd5fd2c8ef17b925ef; arbitrary output sha256:6b2b85d2a80c2ed4d59de40f2acf5e5a5e057c1c60d59546b562cd6d701f45d6. Its 80-artifact allowlisted archive is S3 sha256:176e2d8b7a986b995f58e8eeb910fa1613811ad09ee42186aef66ef69168de17 VersionId `f2W2w_dQZ8SOl9688rkoy.4UnJwftpXH`; certificate sha256:cc197bbae1333823b66d251991ef343cc7be5867f9966b2c62066eab265590cb VersionId `CeD0DgUyxg7ossjEpm2JocyW5IyObgzQ`. The certifier completed a fresh version-pinned download and full 80-artifact restore with zero residual; an independent exact-version download reproduced both archive and certificate hashes and a complete canonical member inventory. Archive wrapper sha256:ebd26e50c90643f1e21b51e794b5f01cbce9e4d66a1a1f73533d4836458a5a8f is VersionId `wmvcYZfIof60kVqBvaTpgOsC.yJ7MbD6`; source bundle sha256:a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 is VersionId `WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3`; ARM64 binary sha256:c42240a49361a72d77c17e12bb7611dfb4ac74d677f341726abd6202083be98b is VersionId `ebPYbHSybdqnwqA7uC8nEDplLo_xnza9`. Every prior log and failed output attempt remains preserved and non-certifying.; information trivial v1 sha256:1e6ba58350cb6f05494526e2c12e5f4c1f0078169f278325ddf2587d7d5f881a VersionId RF4ndb1jV9tMcoW2o6A6GM_Do2QZgxeJ |
| `same:ghost+parasite` | King+Ghost+Parasite vs King | same | `kghostparasitek.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 30,404,852 [1,366,580] (7,553,068) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 33,252,652 [1,337,556] (1,483,588) / 2,416 [2,416] (48) | 30,404,852 / 7,553,068; 33,255,068 / 4,702,852 | Exact 19-iteration permanent-tracking solve from source sha256:d551ba980ab7fb51c63713524e2bf23a31758cad7629835c8d4bb339976b4a58, normalized source sha256:54c8d0bf2fa36254f1e1c4b9efa507c9128a5dc0030555857726fbef6551dd44, model sha256:dbf6e2777ac700693164f461be688ecedc3e58f4c65fbc967025cc3b860d4361, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Information sha256:54fc07d9867fc4061147b8c2b3df7a556eb1df21257896f2b0e9eb9debfedf91 and tracked arbitrary sidecar sha256:423cfb3dd8d2172b0cc34a2214247d132d1be8c5d8cacadd44bd19fd4a9bbc7d; Bellman, rank, monotonicity, structural, singleton, compaction-root, grouping, conservation, dual-force, and source-remap residuals are zero. S3 information archive sha256:caed1886a729dd2da102c375c35d1a9d9f5b2a4e32b74c6b7c1ff078e3446764 VersionId `rmlq4cx4aVO8S7GWV0E2vC5MIl31ZLVw`; certificate sha256:1a329362073b54198116eb99381fb872c022bb8e8c0ee8cb9c12fae099f2a12b VersionId `8xcgdPvWXI_Sx9W0bpq7HwjA_coXY42C`. HEAD, exact-version download, inventory, local restore, and S3 restore residuals are all zero across 77 artifacts; both capture directions use the tracked domain, and source evidence remains retained.; information trivial v1 sha256:c4b03a460f0c51553aa909e10fabb201f94e35dfe70c662203ded757505c5d23 VersionId 14LRAmE3FJj7dw7b2bqedl1br8tnKiAO |
| `same:ghost+devil` | King+Ghost+Devil vs King | same | `kghostdevilk.uftb` | **PLANNED** | 91,099,008 | information required | — | — | — | — |
| `same:ghost+sludge` | King+Ghost+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:ghost+sniper` | King+Ghost+Sniper vs King | same | `kghostsniperk.uftb` | **CERTIFIED** | 303,663,360 | information v2 | 131,203,740 [6,724,810] (20,627,940) / 0 [0] / 0 [0] | 0 [0] (12,876,864) / 127,521,797 [10,576,790] (445,222) / 5,498,475 [5,428,011] (5,489,322) | 131,203,740 / 20,627,940; 133,020,272 / 18,811,408 | S3 information archive sha256:e14b0fb2802d974e5380c8473bb40c1db89e93ab4c393d15095e83198c9b5680 VersionId zlf.7L.qJETCB5n0NMhN6RLaC3u77NxV; preservation certificate sha256:ac5953d700ae5a1d37784675d16b16eb09650089d7b19c66940f165714c079c6 VersionId vb4u0gV5DhcMV.49u8yZ0_vwt3vs.2zo; result certificate sha256:933ba3ffb67bf600079b3f1939f73bc5d32c6483b04feb59f7db96e261491330 VersionId J_kSFYjVt2qqKIzCKqSR3.sgaPMQkK8u; arbitrary sidecar sha256:d488e2bdcb8192c6c8320070ab646198fad092a29e6fe862dbea19ed07117df8; information trivial v2 sha256:ba6efc403a1d76b232269c7c285bac22d3a576be958b56d3af8ec4cded6ada52 VersionId eDhNcNziEdbuZ0yRDbrg62IoMxq3fLP8 |
| `same:ghost+prince` | King+Ghost+Prince vs King | same | `kghostprincek.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 14,519,136 [0] (23,438,784) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 15,884,532 [1,304,736] (18,851,708) / 1,184 [1,184] (1,280) | 14,519,136 / 23,438,784; 15,885,716 / 22,072,204 | S3 information archive sha256:78afa0329463f0d466c7779f626fea6f3c5c836e03c711f9123a8022121e48f4 VersionId k6OY.hL93yj3PW19PHsWivLnOcS9VRDu; preservation certificate sha256:a515e76262acc2df94206ad602a6db3b52ef295a092e54cb78e27549e8133eec VersionId RAtzKHVhl9JZSN76kAINCHQXeFAB.2tl; result certificate sha256:aa3b0c3665003be1fbee081629cb50777f50e7cafcf25bb7f9cdd313b393380b VersionId QjA.mhcnhpi1Grtxon3w0zGs9Nfmt9rS; arbitrary sidecar sha256:06b99a6671bb2648a239076fbeb6eb864ca49f973e2db4b2ed1239cc185bffe8; information trivial v2 sha256:d39a1ea4e99e553584d3e200d51ada8fb4524a6aeb1329c5e2542a842e350bb4 VersionId LF9BerMPcoPXapJHaFfXclheN4f.7kwW |
| `same:ghost+checker` | King+Ghost+Checker vs King | same | `kghostcheckerk.uftb` | **CERTIFIED** | 303,663,360 | information v2 | 65,017,432 [33,961,680] (12,583,784) / 0 [0] (6,307,104) / 0 [0] (67,923,360) | 0 [0] (6,438,432) / 63,746,685 [34,104,825] (8,196,301) / 2,763,451 [2,724,335] (70,686,811) | 65,017,432 / 86,814,248; 66,510,136 / 85,321,544 | S3 information archive sha256:36d9185b96f01a06d2ed71c4f844d311c2e02ed96285dc2740227d3cf93ef02b VersionId vUVpcRuAZvK1ldFIDRfGtp2vj_qCQBgd; preservation certificate sha256:18e7fa29d6270b00054255cb48479bfca4207197107b5dec4d7689c8d74406ef VersionId 1IOfdmaZj3cU7_2osSDaypliGl73MgId; result certificate sha256:3237fecef43165111daf86c779f006cc028340a97b2a5c8130a998eb9fffb504 VersionId EgJUzW31KGqYF.yHSa8AIO55dN_kVlQ0; arbitrary sidecar sha256:9c6d587c02a17c292d96cc1cf263ebe5e9d4b24b39d074d455ac798c525d487e; information trivial v2 sha256:1f472a5417ebb04d8ab20bbdb9fab0a9fe558be9a1d949e84cb47439e55363f0 VersionId 26413sxiA2K3Lyt02yFjsZQ.lUalsp6J |
| `same:ghost+giant` | King+Ghost+Giant vs King | same | `kghostgiantk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 19,660,036 [964,964] (6,913,364) / 0 [0] / 0 [0] (11,384,520) | 0 [0] (2,284,232) / 22,395,792 [3,077,148] (210,664) / 842,184 [842,184] (12,225,048) | 19,660,036 / 18,297,884; 23,237,976 / 14,719,944 | Exact specialized 35-iteration solve with all 53,146,800 singleton checks and Bellman, monotonicity, compaction-root, grouping, conservation, structural, and source-remap residuals zero. Result UFIW sha256:efdac7b79a43d11afd1b519d93a50b02f69a31299fa0119efc8b9238dd638659; arbitrary UFGD sha256:a56cd228f4f8bf4b50736d88f2ae838646272bbb543002c9095efebbe09de077; normalized source sha256:d9cfde27ba5eb9e4462112ab2340a20f55ba22013e59c6bb8233509130326338. Authenticated 70-artifact S3 archive sha256:9cd4580ac47f4c1c9ac6441b6e8a584a91d61812243e58295843218819f6838d VersionId `8EtVjGwuFeV6bB5Pe09HAn6Bj6nMbcNo`; certificate sha256:079c4f2c901c41dc700d9aa39dd66590c27ce1e9e23131f7467a5e801077e1bf VersionId `PcRlbR7cxaewp27boYi2fZoqSrrrbpAW`; local archive, S3 HEAD/download, certificate download, and fresh restore residuals zero.; information trivial v1 sha256:b2571396c7a25275b30d1faea27b33dc2b5ba24928803af4a1cd94363f026362 VersionId mLw5pT8z3Ihub0lpGcWUAh.r32BoDxeT |
| `same:ghost+copycat` | King+Ghost+Copycat vs King | same | `kcopycatghostk.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 54,255,312 [2,468,496] (18,777,648) / 0 [0] / 0 [0] (2,882,880) | 0 [0] (6,197,952) / 63,968,352 [11,700,656] (2,855,376) / 10,464 [10,464] (2,883,696) | 54,255,312 / 21,660,528; 63,978,816 / 11,937,024 | Audit found that the prior completed solve lacked an observation binding and used obsolete model sha256:b913b77e1c1d3e9ad6b447ea65bb0f2ecdcb3eaf49f15b5b3b0ff29e529cfb80. Its manifest, solve log, information output, and arbitrary sidecar remain hash-preserved under `evidence/pre-observation-bound-solve-20260816T0355Z/`; neither that graph nor its output is reused. Exact source sha256:7f0f6bca64fba3957f7fd0d5e7bdfb7e182495ea9b2577748f7fde969d3f8ff3 is version-pinned as S3 VersionId `doTAFsX1xDEYUjndwKcLAQz_G89HVahB`. The fresh corrected graph authenticated 64/64 shards, 492,960 geometries, 519,411,752 edges, and 337,052 strata with zero gap, offset, byte, and conservation residuals under model sha256:5f90a906211ad3d5d84c0be29d59254ed31294638a44ea5b25c561f3fbc99ed4 and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Its exact solve completed 20 iterations and admitted 118,234,128 roots with zero Bellman, rank, monotonicity, singleton, compaction-root, grouping, conservation, dual-force, structural, source-remap, and transition residuals; output sha256:cf2df8ee30e54b18cb5ffe018f0fa630545e71ff7081d844b50a2c5422a4f5f4 and arbitrary sidecar sha256:2167b83265d8a15363d2c8cd3d52d2bfec9a7f9ac02dd1473893ca9e8fba367e. The first successful v2 manifest omitted its CLI-supplied observation and lower-Ghost bindings, so it is retained as evidence rather than certified. Tested manifest-only runner sha256:ee805e9dc7f9c24c36b9dbd8cab172eed5dc9af574e75fc3df69b8acb1e29d44 (S3 VersionId `U644TE55NBsZembDSP3Fo0Ggs0pxzZHl`) regenerated manifest sha256:47b1c70629d1f7e9c2afff31a817f28c614fd155bf1a2bdb66287339cafa358f over the untouched outputs, explicitly binding the source/model/observation, lower Copycat sha256:98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb/model sha256:d94883c4fd100918a23947c3c71d5e461588778aefbbe037f260d48d5083095f, and lower Ghost sha256:472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb/source sha256:11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5/model sha256:ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5/observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. The 30,998,457-byte S3 information archive is sha256:3baf8f10da88c96b4e1c88bf5c1bf545207b98570c25a33b7bfe2a29e1a388c7, VersionId `lUcrnP2McNdCU8l0aCdt1axTSzojdcTn`; certificate sha256:d0543f016cd02302941afaac9f07ed886b863b3680cf2441f17d2fb264005bea, VersionId `R.WJO9ZQOLVoW0RA_WnrSw4qORczD1bp`. Pinned-version download, archive restore, head, and full-hash residuals are zero across 70 artifacts; the deletion gate remains false, all source evidence remains retained, and no pre-fix graph or checkpoint was reused.; information trivial v1 sha256:853fc8316b681709e910d999e475ab0683865e297487d5fa31b33a1892e8ab33 VersionId t5zH70A_rZ_SYH.Cly3fCvVRYn.F.esU |
| `same:ghost+angel` | King+Ghost+Angel vs King | same | `kghostangelk.uftb` | **CERTIFIED** | 227,747,520 | information v2 | 99,765,204 [4,450,908] (14,108,556) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 99,958,936 [12,279,808] (1,677,368) / 2,899,512 [2,899,512] (2,899,512) | 99,765,204 / 14,108,556; 102,858,448 / 11,015,312 | S3 information archive sha256:546271f1198541a6ad90c9ec3fa569bdcb2049cb035ad585274c928d8c13a6d3 VersionId P5wF843Ao97alTfGXTkqNjnjAmr9t9KV; preservation certificate sha256:8fca0e389446c7e9ec5c5b640b448ebffc20b89dcbf8d5c6bc434c91fc400cf5 VersionId _LQLcg8.fozyU4CKzdVu931lhH26kaB3; result certificate sha256:cdffb64f9f6d2cae744ee31ef00562d5c0b46b155c90ef4f881c156e68d8dbae VersionId lnDsni1KIiIbZPH2raiqZ1EDdEI4uzpE; arbitrary sidecar sha256:aa8542e0a277bbc95b9d337b5ab7f15c5dee24d5da327656a3492b2dd5c81e7b; information trivial v2 sha256:8ffee74a5739d4fcf2d17c3839a160426c4bdf3518abf5f3a3605e8fd50f89e1 VersionId 8m2FYu0a1JPcltszXsbepeRULB704_CV |
| `same:ghost+fisherman` | King+Ghost+Fisherman vs King | same | `kghostfishermank.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 33,217,578 [1,483,636] (4,740,342) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 31,843,036 [2,615,900] (63,856) / 1,495,832 [1,495,832] (1,335,980) | 33,217,578 / 4,740,342; 33,338,868 / 4,619,052 | S3 information archive sha256:014059be3716c77abfefd3528f5ca347198c399bc866e5717d633740486fb27b VersionId HvR1_x22mlqhETh6ZamMab0ROuHwAa1p; certificate sha256:641105a83d43ace1aa4205760cf13556f26e91c81fd47b89277ab17314a83eb2 VersionId GbNWjeBCdSXp2pYeSPZYGzMlazPVNFOZ; information trivial v1 sha256:3c8e683696bc24895df228c40fd4d7d002251e29568a84ab92f3a094156f0d91 VersionId 3T6CANBeXyD2BfusVTx.Ppvxkee4Sq3V |
| `same:ghost+dragon` | King+Ghost+Dragon vs King | same | `kghostdragonk.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 26,939,644 [1,207,864] (11,018,276) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 33,239,122 [3,821,530] (1,480,412) / 15,946 [15,946] (3,224) | 26,939,644 / 11,018,276; 33,255,068 / 4,702,852 | S3 information archive sha256:4506634d00af31e384962bbfd2bd6bb50bc1e1fdeb2e1dff5f85ff0897c0ba98 VersionId FjbH81sgPmsyhq2W3DuuEMLNAllLywJi; certificate sha256:b67d5cce458c8238b2a2ae93e65797a0db56ff59e4862952a774469038e68636 VersionId S.gN9bMgbrplnRjqo4aFfnOrYUdPHK9d; restore, overlay, proof, and conservation residuals zero; information trivial v1 sha256:dc3456c364c6be2795b43311274cc9896435e0cae053d265d708cd7de30920bf VersionId YJjIKkgKjnOkTRwRqCSuH7DgKlZ6ErUW |
| `same:mage+mage` | King+2 Mages vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:mage+penguin` | King+Mage+Penguin vs King | same | `kmagepenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 52,012 [0] (2,273,538) / 2,820 [0] / 20,523,140 [1,102,930] (128,980,170) | 8,998 [3,178] (1,881,536) / 135,360 [0] / 22,168,722 [4,763,078] (127,637,064) | 20,577,972 / 131,253,708; 22,313,080 / 129,518,600 | S3 table sha256:90eeb1aef3fbe48c084ae1c687cfde59c1e038a33306c227175844239d4590fc VersionId 83DZWNbTLdgPddvwEZCKMtPY2R32_4Zv; certificate sha256:8ef021fda5605adac11e6da2d4a1fc522b3aeca6010ee6edd261d4fd2f3ac9e3 VersionId ZtyfbqhzpInivp5adCjqOOE00otwnAev; reachability v3 sha256:7220531602a24759fd9ad6eba02370855a9cb5180acd6c71d7936599433da105 VersionId DnUWJSGZ8oQ5Xey3K01GsPLCg44hrEd7; result sha256:99b7ea7b8b08078292e0da1b569e68c9d0377b0558d0ec82b40706e2db3c5f16 |
| `same:mage+parasite` | King+Mage+Parasite vs King | same | `kmageparasitek.uftb` | **CERTIFIED** | 37,957,920 | concrete | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,368,120 [1,304,236] / 1,232 [1,232] | 15,885,716 / 3,093,244; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:7a7774f5ee58f113fdbab763d17d14cced16edb49b32013c9a99e47472249968 VersionId Ua_24FhzfWwk3ftVLC.HDsUjpT_Rembc |
| `same:mage+devil` | King+Mage+Devil vs King | same | `kmagedevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:mage+sludge` | King+Mage+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:mage+sniper` | King+Mage+Sniper vs King | same | `kmagesniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 3,175,958 [2,554] (10,385,352) / 0 [0] / 30,630,304 [2,336,264] (31,724,226) | 0 [0] (6,438,432) / 1,556,296 [1,145] (1,550,648) / 33,182,408 [5,385,091] (33,188,056) | 33,806,262 / 42,109,578; 34,738,704 / 41,177,136 | S3 table sha256:440ce6f3e6941469d93abce069048e6b3d4036a47d2bdbefb961c200b561fc2c VersionId an5IdPEedjbmM9RK_f0UBHuD06RGhe_J; certificate sha256:73bcb1225cea54dce7e774df6faeecaa2e0c657cbfa98f65d5a4565882e79507 VersionId 4IdFBbhdnhHq24MPFS.88L0pH52JgJag; reachability v3 sha256:6d9d633ad7417764f1d97ed2f4ed873c262d1e04effe555c51e3ae955a96f53c VersionId .hLKuakzJ4F5DtSSDx_ZKc_fayyB1Ea5; result sha256:1ee846ca23bd49187bd75bf8fc03bd27dfabdfee26b1ec1cfdafd277fe018636 |
| `same:mage+prince` | King+Mage+Prince vs King | same | `kmageprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 15,885,716 [28] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,952,244 [1,247,388] / 1,417,108 [1,417,108] | 15,885,716 / 3,093,244; 17,369,352 / 1,609,608 | S3 table sha256:fcf556b3e726643dac36d89b5b298cfb862d2616528b01ab102dcc78f4d5c865 VersionId G45OTluTnoxvfAameMd1iDpLI5bBmTqP; certificate sha256:d6740d74b7a7eb48b2b40f9a3245681758b675bee3e33749aa81b4289b41d4bc VersionId ClyrfXy53gootcv1u9nMxo3rJSJ1CFOI; reachability v3 sha256:1f320b8679ac594f85acc9785a755eeb5ee3381d707ed1813a6d6bf184147b26 VersionId 4reniwlRgWgGziaoBLlM_qDhDTarE459 result sha256:680116363bb4338774e391c8fdc14400c2f7282e9846c9d103cb9262e8de03d6 |
| `same:mage+checker` | King+Mage+Checker vs King | same | `kmagecheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 842,688 [842,688] (3,996,240) / 0 [0] (3,153,552) / 32,226,100 [2,276,157] (35,697,260) | 0 [0] (3,219,216) / 0 [0] (3,996,240) / 33,003,124 [5,164,863] (35,697,260) | 33,068,788 / 42,847,052; 33,003,124 / 42,912,716 | S3 table sha256:5f2d6a524e5793a728bc1a0dbb621735cd31c819b1288ef5a82e7c8321d78ed7 VersionId F0EVoFieUU1HF2sqRSr80S2QPR0wklb9; wave certificate sha256:ab54a5a0e288d50ec6e4479b9a25b5224309fa21c8599069b3a5e2b9b7a4fcff VersionId XROm8jKz3598eZwHNP3Cw8GdMhcnwWjR; reachability v3 sha256:30a569ccba7a1b517427b1284a0b65507f0a277b26664e456869bbb4a245e59b VersionId VswOawXm5jgUeEDkA28gTj.XeGlYFu9C |
| `same:mage+giant` | King+Mage+Giant vs King | same | `kmagegiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,344,456 [1,280,090] (3,939,164) / 0 [0] / 3,080 [1,550] (5,692,260) | 0 [0] (1,142,116) / 9,720,916 [127,820] / 2,423,668 [2,412,264] (5,692,260) | 9,347,536 / 9,631,424; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:ee8ece190540fb112a389ecddad781709389f443e38ffea5d64eef222e59724a VersionId oewu810Y8cIOTaZw3NknKGd.jBIqU05v; result sha256:cb818ab1353d97412601aa5aa770e54783ece724068effee931178ecc0021f8a |
| `same:mage+copycat` | King+Mage+Copycat vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:mage+angel` | King+Mage+Angel vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:mage+fisherman` | King+Mage+Fisherman vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:mage+dragon` | King+Mage+Dragon vs King | same | `kmagedragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,085,820 [159,446] (4,893,140) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,945,268 [1,078,918] / 1,424,084 [1,424,084] | 14,085,820 / 4,893,140; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:f7414de75267c33ba208a63b420c0d0848652838f755f08abbca3cebbcc04f24 VersionId c1G9pQWb1ct22u.6jY82vyGB.7iMMjnY |
| `same:penguin+penguin` | King+2 Penguins vs King | same | `kpenguinpenguink.uftb` | **CERTIFIED** | 303,663,360 | concrete | 48,472 [0] (1,223,088) / 5,784 [0] / 10,303,014 [0] (140,251,322) | 23,780 [796] (935,234) / 130,430 [0] / 12,027,894 [3,318,326] (138,714,342) | 10,357,270 / 141,474,410; 12,182,104 / 139,649,576 | S3 table sha256:ac2539c747e4b087416ce6dbcf264faf5414032d9d34a0d715797a2adaf6648f VersionId f5azSaLaEYpTJ3Icrj_vcYQAPhTFHKRC; certificate sha256:6199a7a8589313e2bd8b5d0eacff5fd03c2a256971f055a44de0e4032e03bd8e VersionId ajZ._SELy2aLSvdTfh8B7uoz3JYpn992; reachability v3 sha256:f298b251387b31b7330cfb4d1aefb39fb3c9d1cfafc0565a8b0316b85ba4cb2d VersionId JRdA_zHDQZ.x.9U2J6c8K.BDLTwuaBbi; result sha256:d390c1d335944be7b045eafacb831f2912cc5d9e0cbb3f834cce6b57f7016abb |
| `same:penguin+parasite` | King+Penguin+Parasite vs King | same | `kpenguinparasitek.uftb` | **CERTIFIED** | 303,663,360 | concrete | 18,869,468 [0] (5,122,930) / 8,220 [0] / 80,080 [0] (127,750,982) | 32,236 [0] (1,881,536) / 20,550,828 [1,304,236] / 1,730,016 [1,609,980] (127,637,064) | 18,957,768 / 132,873,912; 22,313,080 / 129,518,600 | S3 table sha256:2d75d79fa4a94dd2721c2e0b8fea59cd8b83bd4c8e406d8a5ee7b803e0698a3b VersionId _6Eqk5fA9jF74he8uXkfDDUqn6CDu_FM; certificate sha256:778bdf9219ae0474153439bbf1c8edae95901012d40e3d2f152f230983dbe26d VersionId Ybz6fanURcMgIJsf.KZppIqGf6iAiLpN; reachability v3 sha256:92e7ebd177d76693a65b331fe74c8e571bb63f2b142a8e81363d74fb5c2959d4 VersionId iM3b65IvfPVb7cCo_CeiTREeTMN3qKZg; result sha256:758419427e374e94ed01eabcccd6fcdeafe8c6283ccb257f979da07364242748 |
| `same:penguin+devil` | King+Penguin+Devil vs King | same | `kpenguindevilk.uftb` | **PLANNED** | 364,396,032 | concrete | — | — | — | — |
| `same:penguin+sludge` | King+Penguin+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:penguin+sniper` | King+Penguin+Sniper vs King | same | `kpenguinsniperk.uftb` | **CERTIFIED** | 1,214,653,440 | concrete | 667,643 [0] (10,243,176) / 18,704 [0] (19,300) / 39,451,139 [0] (556,926,758) | 71,284 [8,638] (7,597,428) / 430,869 [2,005] (340,890) / 44,124,007 [9,376,560] (554,762,242) | 40,137,486 / 567,189,234; 44,626,160 / 562,700,560 | S3 table sha256:dc3b1e94e34a940fd52d322f9e677841a6e30ea135f7d8bd24c89c5a8021fa91 VersionId TnJb2ZNqBYLPptK.0drZq9uB1a9QX65M; certificate sha256:40a5ef91051f09077e463f51ef0bdf35adfb52c041cb93f7477fcd9d346e1a8d VersionId FqbY_vla7Jrj.TE7ojRC2ifs1ywcSdPB; reachability v3 sha256:56b2dc5e4298434fefc3544cf4d3be37b60e02423cf098acf1670cce5c3cdd14 VersionId Mpgox84V4EOZYKM6269BR37jYKiK1qTR; result sha256:a69674a6b5b52ae5efa81b848f2fcfcf147c289ac8a7295dc4ebce189df2b26d |
| `same:penguin+prince` | King+Penguin+Prince vs King | same | `kpenguinprincek.uftb` | **CERTIFIED** | 607,326,720 | concrete | 18,811,198 [1,120] (5,112,674) / 7,100 [0] / 139,470 [0] (127,761,238) | 27,614 [1,250] (1,881,536) / 18,805,584 [1,251,004] / 3,479,882 [3,266,454] (127,637,064) | 18,957,768 / 132,873,912; 22,313,080 / 129,518,600 | S3 table sha256:9502eb93cf3ce74daf4fe9a706ab0bd547cf849ca49b9fc68eb1cd6f4b234a79 VersionId bfLwEuZxDdHzf04VNH9ALHgcb2MaodaL; certificate sha256:8d0e4c70121f001ef99e8533442e08fb240194e568686736d96c33ce6437b20e VersionId qgf_g4ckwR4HDUnCisra6qNMIJt_SgXH; reachability v3 sha256:54c3a1c067db31fa5cd1538e009ace3f3743e68e6e1ccb240f612ac1fd4ebfcc VersionId W0xccQks09SrBVRK4fNmmHOnBUFGy3bB result sha256:9ffd9eb292f826d5a1a427adc89a5931726f1cc7bf2ad22bc8abbcd028516606 |
| `same:penguin+checker` | King+Penguin+Checker vs King | same | `kpenguincheckerk.uftb` | **CERTIFIED** | 1,214,653,440 | concrete | 1,061,771 [914,028] (5,432,818) / 17,945 [0] (3,693,290) / 38,137,705 [0] (558,983,191) | 69,008 [8,804] (3,766,404) / 323,692 [0] (4,691,100) / 42,059,530 [8,936,858] (556,416,986) | 39,217,421 / 568,109,299; 42,452,230 / 564,874,490 | S3 table sha256:ebf0409b2cc35a611ab34d2695dcb836b1e082f5d76bc2a67bb1fcc5f5c31835 VersionId BLEC5RiZaNnj.nuvk89lxRT9IJGMzlc2; certificate sha256:12b7a541760e53b1938df13128bf3cc4fad0971c8b14ca03466effc64d693b72 VersionId uaiX9WDCr0JW0rTbYfDJMbfFpcFAJL8M; reachability v3 sha256:c43f12f960c9655843af1e0196c76b190f3f4d90c5e6ac455758b92e7c58601a VersionId QmpaFCJEa8pctEVfUr0CkFe9nLWNjPG7; result sha256:b34710578767b75e21c3e5790cf995d49bc28994964395e783d918d289a27e92 |
| `same:penguin+giant` | King+Penguin+Giant vs King | same | `kpenguingiantk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 279,824 [0] (3,856,022) / 6,320 [0] (56) / 12,730,524 [296] (134,958,934) | 22,846 [3,014] (1,393,332) / 267,788 [754] / 15,996,786 [3,973,456] (134,150,928) | 13,016,668 / 138,815,012; 16,287,420 / 135,544,260 | S3 table sha256:73f46298694cd4c75ad531c9f820bff25a628eaa73490656526acf33c257fcca VersionId sPpN0R8UYEnVXoTl6Ssu.o_RfKKvMXAa; certificate sha256:faa342cc38498c3e7fd78b50f2194f56ce5fb66f888ed0939276db4ec4793871 VersionId bb1hF_aW6F65EVjoQ4KM3Eky6wp9js3Z; reachability v3 sha256:57ed84f2d9e1a0617cf2a586583b9b9ac2d99681c967183e1837bccc76b1452e VersionId 1S5mMKAGVqSYjCsUkJbfNdkQP8ycR19F; result sha256:49436c4403f618be39a647aeb1afb944351d418f6cd476217187c7b5069a9b85 |
| `same:penguin+copycat` | King+Penguin+Copycat vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:penguin+angel` | King+Penguin+Angel vs King | same | `kpenguinangelk.uftb` | **CERTIFIED** | 910,990,080 | concrete | 234,438 [24] (6,504,590) / 11,078 [0] / 61,488,400 [7,820] (387,256,534) | 42,004 [6,450] (3,763,072) / 434,678 [100] / 68,344,094 [16,037,602] (382,911,192) | 61,733,916 / 393,761,124; 68,820,776 / 386,674,264 | S3 table sha256:2a1906585a21f1bd6665fa7b6a0820140b029ef929e11edbe3eb4551694bd2ef VersionId QOYs24xiXjsRtjAd16JkqhIxra80RzU6; certificate sha256:4aa6f6f16af4baeb592452a1d7d299e3d4d55056c2c0ad295c2f12451ad755e6 VersionId GqoTLKsQSu8WVFkwBfMS9yv7oRTtseSe; reachability v3 sha256:974d1d4c11c623cbacc83d57a536f7922c0dc2eb7bb192370811694e89e0d08d VersionId EIZdAFDJNvobsWjgntQFUjAAlCjCIHKM |
| `same:penguin+fisherman` | King+Penguin+Fisherman vs King | same | `kpenguinfishermank.uftb` | **CERTIFIED** | 303,663,360 | concrete | 52,012 [0] (2,181,788) / 7,184 [0] / 20,518,776 [1,246] (129,071,920) | 26,522 [4,010] (1,881,536) / 135,360 [0] / 22,151,198 [4,762,246] (127,637,064) | 20,577,972 / 131,253,708; 22,313,080 / 129,518,600 | S3 table sha256:8d414f7feeb6752e2ab01d5df49284ada0a5ee87ecfbe95d66105512b2fa2d24 VersionId PckrHE_nskYGXs0XDGChPcPpZiNrqowj; certificate sha256:06c9ee5d81685581a912b6bffc5eb6f47b88fc9b917b5d572d512cce956e6f78 VersionId JPvgIkmu4Uffy7RDSO4KsgfNf1Eo6iJK; reachability v3 sha256:4766a8ece0c0634d4513822a8792f4b76662d4c9223f776bfb4ee9cbdba69787 VersionId nbKokpFpuVJo2TZLZ8OUc3U4SY4TTwKg; result sha256:3f710fc10cc64268245b61b2f5f820a388ffdb43f0c4e7cb6be75e6130c33580 |
| `same:penguin+dragon` | King+Penguin+Dragon vs King | same | `kpenguindragonk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 16,823,836 [0] (7,140,970) / 8,078 [0] / 159,590 [0] (127,699,206) | 30,978 [3,426] (1,881,536) / 18,922,014 [1,105,952] / 3,360,088 [3,099,582] (127,637,064) | 16,991,504 / 134,840,176; 22,313,080 / 129,518,600 | S3 archive sha256:7e320677e00407cdebc1d11f649fb259cb73748468bcf2c4429628fe43be01b9 VersionId BDYHf4_ejViJWeb8MntfhZPuhLQQzzmM; result sha256:8732dfe2b06e742628fba37cb6e7bcb4a44ba1b587e2aa4c1fef41e2f67a270d; certificate sha256:2a0f1a3e66234f90b5ddc8a3ab48eaa2562ddcb9d81ed52980dfd578501926e2 VersionId w4kzfYbRsFWSzVvX4pWLhIC4QfNE326B; reachability v3 sha256:c74e316001312c2de2f4d4b58fcd3a520545c92a4db70e43f0ab991a003d91a1 VersionId YzOGevQ5mkVTg66JECRng7vUkoaUb4dY |
| `same:parasite+parasite` | King+2 Parasites vs King | same | `kparasiteparasitek.uftb` | **CERTIFIED** | 18,978,960 | concrete | 7,259,568 [0] (2,229,912) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,682,564 [0] / 2,112 [2,112] | 7,259,568 / 2,229,912; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:b53d8568316fdbb24ba74bc2b5aa308fb5df0a4e787184c814cfec820a6695e6 VersionId EuR6Eye9xSp9kaVMJ4HrjFpPUVFH6HT3 |
| `same:parasite+devil` | King+Parasite+Devil vs King | same | `kparasitedevilk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:parasite+sludge` | King+Parasite+Sludge vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:parasite+sniper` | King+Parasite+Sniper vs King | same | `kparasitesniperk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 30,910,078 [0] (45,005,762) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 34,734,402 [2,608,472] (34,727,644) / 4,302 [4,302] (11,060) | 30,910,078 / 45,005,762; 34,738,704 / 41,177,136 | S3 table sha256:2c62cc695134f58b78cfb044f937571e51ef05be23a36232fe3ce15b6ce510f5 VersionId _TmEFe5DTMzJfIaoeQdiiQda8F.LEuW8; certificate sha256:94f28509bd4ceaa88ca8b90c905b20c62cb3a14ece7f3c925b02a8f0928c6250 VersionId h6w_3.OB8NmdIFnzvwWif542ejFEde8Q; reachability v3 sha256:f6b3132486cbd3a3fa865cd7d3d6c1a27383abaec2417460342db0d74ea7da5d VersionId F6BacqwyFDVmH5QrGlynkubnN66L2DsF; result sha256:fec437c1303bf0dd682a416507ae22cb019df2dca7275be946e96d48d926db2f |
| `same:parasite+prince` | King+Parasite+Prince vs King | same | `kparasiteprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 14,519,136 [0] (4,459,824) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,365,128 [1,304,236] / 4,224 [4,224] | 14,519,136 / 4,459,824; 17,369,352 / 1,609,608 | S3 table sha256:4e3cbf836083ceb6371ecb98e6dcb168b88e9edb7ca629e25ec08be7c466ef5d VersionId 7s671.cc3suApED2vkblz68aVAHr8VZR; certificate sha256:a3b48b39486c9b7daca826942fcf414f6e68370e24630f2c891ec4a4f4adf142 VersionId DIJBCMOMK.aesSUKZE2npMEUrRlTJsHg; reachability v3 sha256:90301f1c6bebde559d19b59fbaecdf99092e831c9e914305c66a928540299def VersionId x0lngbl9qyEMTrHc5N.2JeGoURXP8WYm result sha256:cf0276fbfd9c64de20bfd5829b01c136c2375f789e759947772ebcbd23529fa1 |
| `same:parasite+checker` | King+Parasite+Checker vs King | same | `kparasitecheckerk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 30,312,348 [842,688] (8,488,260) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 33,000,176 [2,514,375] (8,637,632) / 2,948 [2,948] (31,055,868) | 30,312,348 / 45,603,492; 33,003,124 / 42,912,716 | S3 table sha256:1adc02919108c1c1fc5e3d21041eddd78a5d00942c75189cfd89c10b657f2546 VersionId VdQdJc4qD_IJjjdW6AOsiJCOCcuaxoao; certificate sha256:a688cd4167da31225eeb7bf36fcc6ea5914b0afb2dec36b2babcc02615d896a7 VersionId 4ka5.xhqHPw3LrQhHhXuI5MYfa1cUmcd; reachability v3 sha256:1269868900b56635cdaf3e04c24453e67656fd73ab6f031fbd08be15625a07f1 VersionId jU9zSUof.Vp6OM05vpLUfynwJDDML1Mx; result sha256:91b90fecaa05f7265a88a18e05591a99df2df994ffb7d424e9fee0aa46bd769a |
| `same:parasite+giant` | King+Parasite+Giant vs King | same | `kparasitegiantk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,347,536 [0] (3,939,164) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 12,141,428 [1,568,280] / 3,156 [3,156] (5,692,260) | 9,347,536 / 9,631,424; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:ca50c60721d2bee52a22de6041ea2d63b86a03cbdad7e5454d7a96ea304743be VersionId 6lsAgk8BAa8ZcAE2oKj2YPcJYMpj5uL6; result sha256:5e0bb56139965b70efddd21aa7929f4d829c101abce197357b5fc76e282faaf6 |
| `same:parasite+copycat` | King+Parasite+Copycat vs King | same | `kcopycatparasitek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 25,893,408 [0] (10,623,072) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,403,568 [4,595,168] / 13,936 [13,936] (1,441,440) | 25,893,408 / 12,064,512; 33,417,504 / 4,540,416 | S3 table sha256:0461b714bc0e185cd3382fa90f6095ca420ca3f65192ad94246d570e16cf15c8 VersionId 3DFdhaU2bA1am9_47UI6rvkH50dc9lTD; wave certificate sha256:ebb747f93ecf52de6bfb53c6a1e0e8de99c80cb99ff18e4dc081262c6f0b5526 VersionId g3gJ4LOWUh6jUxIxOV22KK8rjbkwiAEK; reachability v3 sha256:7cc48f3597f50969ea314b3ec0c8c2790850904787a8aaf35890a58affa17715 VersionId Tx6IakYX8BNWI1Op.wcmQVEP9FNDSCQq |
| `same:parasite+angel` | King+Parasite+Angel vs King | same | `kparasiteangelk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 47,657,148 [0] (9,279,732) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 53,713,968 [5,283,632] / 3,696 [3,696] | 47,657,148 / 9,279,732; 53,717,664 / 3,219,216 | S3 table sha256:3682818134600be77fc5f192b14c2a4184a5a91def59ca50d55fb80f0b3ccc4c VersionId QkRBqjeHtROvZLDaUJQRyi._NcdLxkOe; certificate sha256:456a8939419c4f201962a6a7ad320a2b19b0266101123c068dceb8f558972067 VersionId GO4LiiaoFSUjiMaRTp06tFsRbdZ8izki; reachability v3 sha256:a66537f91010a450486cfa121f221e5ecdfa3a2bbc47fb4c9ecdfcd964926ca7 VersionId ahKYnhreFZECIsF1q9qH9Fvc5yjor0mX |
| `same:parasite+fisherman` | King+Parasite+Fisherman vs King | same | `kparasitefishermank.uftb` | **CERTIFIED** | 37,957,920 | concrete | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,368,120 [1,304,236] / 1,232 [1,232] | 15,885,716 / 3,093,244; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:a3144c589bdd085d32587368aa065e6fd2e330ab71ddd22ba1005f81fafa9a63 VersionId QIh6FRXeIqZCvTIb3JbR09d1C2qJarEK |
| `same:parasite+dragon` | King+Parasite+Dragon vs King | same | `kparasitedragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 12,877,956 [0] (6,101,004) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,350,088 [1,304,236] / 19,264 [19,264] | 12,877,956 / 6,101,004; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:0a27e32c07e7df6603f200cb56f4243309573668f26c8aab928bf3e701254e06 VersionId tqcPlxWxsm5rJKl5Dbh2uUH0IyGMC19T |
| `same:devil+devil` | King+2 Devils vs King | same | `kdevildevilk.uftb` | **PLANNED** | 26,522,496 | concrete | — | — | — | — |
| `same:devil+sludge` | King+Devil+Sludge vs King | same | `kdevilsludgek.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:devil+sniper` | King+Devil+Sniper vs King | same | `kdevilsniperk.uftb` | **PLANNED** | 182,198,016 | concrete | — | — | — | — |
| `same:devil+prince` | King+Devil+Prince vs King | same | `kdevilprincek.uftb` | **PLANNED** | 91,099,008 | concrete | — | — | — | — |
| `same:devil+checker` | King+Devil+Checker vs King | same | `kdevilcheckerk.uftb` | **PLANNED** | 182,198,016 | concrete | — | — | — | — |
| `same:devil+giant` | King+Devil+Giant vs King | same | `kdevilgiantk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:devil+copycat` | King+Devil+Copycat vs King | same | `kdevilcopycatk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:devil+angel` | King+Devil+Angel vs King | same | `kdevilangelk.uftb` | **PLANNED** | 136,648,512 | concrete | — | — | — | — |
| `same:devil+fisherman` | King+Devil+Fisherman vs King | same | `kdevilfishermank.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:devil+dragon` | King+Devil+Dragon vs King | same | `kdevildragonk.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `same:sludge+sludge` | King+2 Sludges vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+sniper` | King+Sludge+Sniper vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+prince` | King+Sludge+Prince vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+checker` | King+Sludge+Checker vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+giant` | King+Sludge+Giant vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+copycat` | King+Sludge+Copycat vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+angel` | King+Sludge+Angel vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+fisherman` | King+Sludge+Fisherman vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sludge+dragon` | King+Sludge+Dragon vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:sniper+sniper` | King+2 Snipers vs King | same | `ksnipersniperk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 5,095,977 [0] (31,789,868) / 0 [0] / 27,777,843 [649] (87,167,992) | 0 [0] (12,876,864) / 3,519,792 [10,344] (10,470,915) / 31,218,912 [5,313,473] (93,745,197) | 32,873,820 / 118,957,860; 34,738,704 / 117,092,976 | S3 table sha256:a12eae5e95aa2cd3233cb5b668b1b8d95999cd2b1ec94fed46e77c788ecc6e30 VersionId eBU94UfK1f9PH7uQuzSsYR5tBYNFdiXl; certificate sha256:7ba878ad4686f44cdddcdc60ac0662d1f04b58822372b9716522582d4f83a51a VersionId inYJ9iQSfVJ9WoKuClnPNw2v_QMAIBVi; reachability v3 sha256:1417fd2a4cefec6e464e6826b25e1dd3b005e587a5dab3990952248e9e1a3663 VersionId XqGnmg7YbwoLq1RHgTS70i1nVPTNg3db; result sha256:da2c94e3b05fd63bd548fb669115e6ea22414c06a84ded5684b11afdd276f854 |
| `same:sniper+prince` | King+Sniper+Prince vs King | same | `ksniperprincek.uftb` | **CERTIFIED** | 303,663,360 | concrete | 30,910,078 [0] (45,005,762) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 31,990,205 [2,510,497] (31,982,747) / 2,748,499 [2,748,499] (2,755,957) | 30,910,078 / 45,005,762; 34,738,704 / 41,177,136 | S3 table sha256:b0a00607b5093d7f4b2121517c44ec2f0ddfb74b3d89613f72cc01599d0e3f10 VersionId 2ywCQ.OwruQT9n.Sx5cF8IfSuBqiFGC.; certificate sha256:ae4ea1dbf7c62f2afc3741e0bdff8a0aa27726b2ae9e23441885bad12ed034a9 VersionId NrjbEj47D53MzUO3sP.6Yo6mKFzFUpFf; reachability v3 sha256:7ef208db862ef184bb46f25116bad342d17097d25c9118b15c3a403ce70b68d7 VersionId XOc8bVZlXvWe2LIZp9DNq7shBE66NdBk result sha256:b97ff7a41961e9043d8e2095d82cbcb552958707eed6c90f0cc1954050ba47ff |
| `same:sniper+checker` | King+Sniper+Checker vs King | same | `ksnipercheckerk.uftb` | **CERTIFIED** | 607,326,720 | concrete | 9,111,471 [1,685,376] (27,007,943) / 0 [0] (12,614,208) / 55,302,198 [10] (199,627,540) | 0 [0] (12,876,864) / 4,981,208 [12,171] (22,783,839) / 61,025,040 [10,201,510] (201,996,409) | 64,413,669 / 239,249,691; 66,006,248 / 237,657,112 | S3 table sha256:22a199167270b422368848b97b996badd987ac35c732ea491a2ec6ab8da58333 VersionId nN8ehb2UNqDMdFDi1XnGk11Z3RCtdrxn; certificate sha256:40511f5943770863dae204b821a261711c9e4f0c3b76263d2e34c1db537103e1 VersionId VPVnv1.kZL06nSXM.f0Uox9NDl2_bxR3; reachability v3 sha256:d6c5df3fe699f3309b58ec891f78756659655bdb7b528b62cd932994355fcbc9 VersionId vQx5m6.Ekx3TD.GL.IPN5wIuy3PCAJ1V; result sha256:9f2a923d90dfbe52ec3d173a8f0d2755f7dbf33b2deb0840a62ae31c2d1015e0 |
| `same:sniper+giant` | King+Sniper+Giant vs King | same | `ksnipergiantk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 5,157,709 [0] (17,725,745) / 0 [0] / 14,895,190 [0] (38,137,196) | 0 [0] (4,568,464) / 4,166,945 [7,062] (4,161,800) / 20,122,223 [4,767,556] (42,896,408) | 20,052,899 / 55,862,941; 24,289,168 / 51,626,672 | S3 table sha256:60d49fa63f9350125b39c2a310e6041c335091c1efe74f44651185dcda4f734e VersionId .WOhrXgkw1wvQliOZR6IURRmcAvQAikm; certificate sha256:d048e31d830d367a075be1ab38cabc5e6bccc4364bfd93cce1b4704ef9b977e3 VersionId mQZdeE8QaftK7HCoBE.VLOjLET9BQt_K; reachability v3 sha256:4b5942b21bd86b52f02037f1b75dbe0b0e4c9d44066999d1834b5f476bceedbe VersionId rYQs4yyMnFWz.9JlfHzNSPEHSGjXMOih; result sha256:44aa3a95babcdbc943e38b989a85b098ad391bc7b4394a661228bff54df2423a |
| `same:sniper+copycat` | King+Sniper+Copycat vs King | same | `kcopycatsniperk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 55,208,768 [0] (90,789,700) / 0 [0] / 30,952 [0] (5,802,260) | 0 [0] (12,395,904) / 57,106,868 [4,377,424] (57,085,436) / 9,728,140 [9,646,164] (15,515,332) | 55,239,720 / 96,591,960; 66,835,008 / 84,996,672 | S3 table sha256:632889a8382562edbd61af149528d362e5346578c2821a9a3e273d7a207cb0eb VersionId GqSACrZPIZc4_vMQYR9j7C.jsNtvnJuZ; certificate sha256:c1f054f70ac4cfc441cd10f91861c097e2fd72bf6e16e21a1102cf751d1bccf5 VersionId 3Z0ei.v79cSAyzz.oyMk0SBkhATpDXbg; reachability v3 sha256:e4fde95666852cd86a59e4587288a154f2ccb94586104b80239a1bad089092d4 VersionId v7jnTUH3K2q.gJEaWCiAodpZW4TTctpe; result sha256:11d685a7d7673ef3f4458ef42913f04f2034be32f3432a31e64fcfa3c0403021 |
| `same:sniper+angel` | King+Sniper+Angel vs King | same | `ksniperangelk.uftb` | **CERTIFIED** | 455,495,040 | concrete | 6,632,997 [0] (28,152,383) / 0 [0] / 94,785,789 [1,279] (98,176,351) | 0 [0] (12,876,864) / 4,049,868 [15,244] (4,032,764) / 103,385,460 [18,986,583] (103,402,564) | 101,418,786 / 126,328,734; 107,435,328 / 120,312,192 | S3 table sha256:edeb540b97676f35d5982166b0a9b372543ff71161c2b21e1e6652f5389fefd8 VersionId 56hc6xR61Nd7m0FXechWzyoXJFxnCJL1; certificate sha256:e2ff3512a0f14333762f836ea6950355f2f8a64a3502aa414ba163c3398d237d VersionId o81G3cns4lGFSD.N9HZkUwEYE9rmLcle; reachability v3 sha256:24c8b87ee9f29988e61e86fc9c74d11bc49bfdd4e0ebc80ee1838c205b7221f1 VersionId f0NY_DidKNyaAx4NkiVNfVnu7DIp1j54 |
| `same:sniper+fisherman` | King+Sniper+Fisherman vs King | same | `ksniperfishermank.uftb` | **CERTIFIED** | 151,831,680 | concrete | 31,421,784 [4] (39,247,144) / 0 [0] / 2,384,478 [45,422] (2,862,434) | 0 [0] (6,438,432) / 25,674,843 [9,762] (25,670,975) / 9,063,861 [5,376,474] (9,067,729) | 33,806,262 / 42,109,578; 34,738,704 / 41,177,136 | S3 table sha256:29769ba07f5e913f6976a334d44daf96cbb437be57983b2e60ae5c4cfbdb4e1e VersionId wLLuFWyOg2Rd1AEAVCc_IgNMYMfBjSHT; certificate sha256:7560b8f06b07fe94e026a79ec093be141f9f9f9b33b78246c8222dea2e8a4033 VersionId qwZ0TOFMrikSUGzEz6P0Yy0p1CwMlQs1; reachability v3 sha256:4ba694ede7b7424f7be231c216cdea8654d42f3c0e466c2175a48c5421f2dfa3 VersionId gr63owJepwhsggc7N3ZglZqQsBhRsOyH; result sha256:5983afa0fb77c2b44f0fe61023931095ef328f367da50cb00fe074ced1bc315f |
| `same:sniper+dragon` | King+Sniper+Dragon vs King | same | `ksniperdragonk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 27,426,669 [0] (48,489,146) / 0 [0] / 12 [0] (13) | 0 [0] (6,438,432) / 31,968,687 [2,181,965] (31,963,501) / 2,770,017 [2,770,004] (2,775,203) | 27,426,681 / 48,489,159; 34,738,704 / 41,177,136 | S3 table sha256:8d7a247f0d627b2b7990adec42b8aa21ce3b42d3167160e7525797c20414a4d4 VersionId hHlx3fgrHxMvxJL8DGr7oXYi283ThPqh; certificate sha256:a059da3f5133d28f929d91591a79b9b2d570bcb51a8bf38080b4f1fd1afc0a02 VersionId A2FgUBtaZ0Q3xQvnJqdN88oO5mEUePeN; reachability v3 sha256:72fa547c50ce92698a08c7ab14c3114c2f57e494024c46dae9812e33bcb10fc4 VersionId mVpt82aH3j8xn1Xr6FCISKmegP8pjcAx; result sha256:3a23ffb178f24fec1bc8144087102091c23b5f9c50bcd5cd59c86c63f275fca3 |
| `same:prince+prince` | King+2 Princes vs King | same | `kprinceprincek.uftb` | **CERTIFIED** | 75,915,840 | concrete | 7,259,568 [0] (2,229,912) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,682,564 [1,275,812] / 2,112 [2,112] | 7,259,568 / 2,229,912; 8,684,676 / 804,804 | S3 table sha256:0b90eaab02e5e90acc86c779cba94ff5b720f810bccc085a79b4c71c89727795 VersionId 9A8ncC8aYJbcTggIXp66WNCFleIsSds4; certificate sha256:fb1ed46091d8cb3e9afa8450361095baba73fe6e8d1f61dbb5325201a6758f69 VersionId sBaFCdDxApnhSVfQePO11Q3GRxXy25DA; reachability v3 sha256:51fa032fa68f600b1370f567fce14a4a568bcdf148909b165884d06476d6de33 VersionId fQVKiS6U92GbMuCNjTSnGCPFOcAuRsdW result sha256:55bf68fafce963911eb13d508490b557e5f900468111cd3bc7e09cc0e1059662 |
| `same:prince+checker` | King+Prince+Checker vs King | same | `kprincecheckerk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 30,312,348 [842,688] (8,488,260) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,379,307 [2,404,380] (8,495,050) / 2,623,817 [2,623,817] (31,198,450) | 30,312,348 / 45,603,492; 33,003,124 / 42,912,716 | S3 table sha256:2a328d2d2f67ac89e8aa6acd1d3a7e8ddb6d506bcd6a4686fa22ed5bc5f6b078 VersionId N8OllFUx6Ikkm8WgEPerdI0JecwDhcEI; certificate sha256:b6ca7587d2e6d05c7dda68ff4e23a285b506883e15c41de57b084d75e73dc78a VersionId SqKrThYrL6d8ebZWkf5cGkFxwUbMlpcA; reachability v3 sha256:ea63515df0c987271fa3b9ae57b1d7d5e95e7bd7b8bf5279d1aff09c3f201b7e VersionId lJSDFHEVU4Dm_3KS3XLs36PIlKDOaZQh result sha256:1dec6cd88cbe574a08f849f7278214673d4fbfd2c468d2b1d677ef074b2d65b2 |
| `same:prince+giant` | King+Prince+Giant vs King | same | `kprincegiantk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 9,347,536 [0] (3,939,164) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,300,932 [1,534,228] / 843,652 [843,652] (5,692,260) | 9,347,536 / 9,631,424; 12,144,584 / 6,834,376 | S3 table sha256:0d4ebe4ab7c4be37c6e5297a1215f019dde6169cd0d9d8d1e1b1a6494185b1e0 VersionId O3x913Xho5Xr4w8427x6ycGfu2D6suth; certificate sha256:cd95800e1b9bd7f68f8846aa9d8450c5926ae1fe11c6012138b751d72b29002b VersionId 8mLUZhxdez5N6dUv6SB2NYonA8JF1WoS; reachability v3 sha256:12f74edf56bde6336afe9b1d92bb3bd2262f033135df2ae5c39fcba8749a4bd2 VersionId p5Aa1sttGVHPtCc9wCpe3BSDK.20KU0Y result sha256:c4763e0ede9256c0b495854c4daa429890076069515f58daef552c1c4fd8bbb5 |
| `same:prince+copycat` | King+Prince+Copycat vs King | same | `kcopycatprincek.uftb` | **CERTIFIED** | 151,831,680 | concrete | 25,893,408 [0] (10,623,072) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,403,112 [6,757,336] / 14,392 [14,392] (1,441,440) | 25,893,408 / 12,064,512; 33,417,504 / 4,540,416 | S3 table sha256:3dc72f45adc99942f7f514bb25c3a06d1a6e05b3b995746f5700ecb5c3b503b8 VersionId LZ3ZOpPWePVsaDE6ilxrkIPoKIUoGxik; certificate sha256:75a613f9961429bdedb2b6bb50fc4628d60af33414165a0d873b34efebae76f8 VersionId PgPuP_dGbqS9TSwGuH1CUnle0Mwd5M.R; reachability v3 sha256:04a3b42f0eda7a290191b43c369d63f7719b59c028d36093a0682ce9662534b9 VersionId xo0VKnT3aKuMIiwErnEPhnYjXAScDaeo result sha256:dcdd63f5420344fa0b1ab49a2c1a8d1c688a5025644871a3395ead8081c60a90 |
| `same:prince+angel` | King+Prince+Angel vs King | same | `kprinceangelk.uftb` | **CERTIFIED** | 227,747,520 | concrete | 47,657,148 [0] (9,279,732) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 50,814,456 [6,354,980] / 2,903,208 [2,903,208] | 47,657,148 / 9,279,732; 53,717,664 / 3,219,216 | S3 table sha256:9ce5f729c6bfd99b9443a832bd32f7babe40769b8ec04a1e3aa8aa4f537b8dcf VersionId aWnlizkwAVFtbwmbp_9py96yNgAemJYt; certificate sha256:f896dbc17acf619bc38456239a121616aa9268ea7da1b168070be0fed3c0f99c VersionId pXshxJTkOucIx_jW7FcMJcTAx3TX8xAg; reachability v3 sha256:da380a6d8519e04e277753748e5d5848a8e79cc9f18b749bcd845afbd86f4363 VersionId 7Hm011AkXJfEL1n9kX6Z10Pgit9rhhH4 |
| `same:prince+fisherman` | King+Prince+Fisherman vs King | same | `kprincefishermank.uftb` | **CERTIFIED** | 75,915,840 | concrete | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,952,244 [1,247,388] / 1,417,108 [1,417,108] | 15,885,716 / 3,093,244; 17,369,352 / 1,609,608 | S3 table sha256:3b6c046a9755c849b74a74a33f537585a78b7201776add43af29ffa3ae817006 VersionId zY68nn6ieP.iACxIjeC37RjbgIFtHJIy; certificate sha256:5563c3dcfcad72287aaa22f55d8f320b2b6cc30f53fe6408551f1f1f9bfbf8d8 VersionId NXxcJOyIgwo2thx4fv89lY0f7hrAaiq4; reachability v3 sha256:3cdea28907c039287a533c9921be0d16b2f7f815f0b026e3b233f32977a74546 VersionId CQ.SbaKwEgYTwUqaYLMiHoPUCw6oT35x result sha256:19f989d8b13e97448c2468447efedd7cfd9e6eb0b04cd8af571e75703f07f614 |
| `same:prince+dragon` | King+Prince+Dragon vs King | same | `kprincedragonk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 12,877,956 [0] (6,101,004) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,350,088 [2,416,786] / 19,264 [19,264] | 12,877,956 / 6,101,004; 17,369,352 / 1,609,608 | S3 table sha256:a890cd8b854328bd6882996df5cec77b851e7fd5bf778947c55d172e84c6fb72 VersionId pIPzVK.CnmED.L7Eyv0GnWdSQrzPD6pb; certificate sha256:6ea971938f8eed7ce3187a1a26ead7da59f6128b48380ab7f930156f75a3317a VersionId Rt_7WO0bfShdcHQqXrOVQSYVRco2QfFC; reachability v3 sha256:b1b4f61ec3dc1eb245081257d92e020a3b31efb8e8dac6d199b149bf5c71eafe VersionId fgNYo6Jonm.CIOPtN19kSwP_9B..Qzz1 result sha256:a36610733aca3706e8ef867e88b394b047bb40a8ab8ae5bb5131598c34588107 |
| `same:checker+checker` | King+2 Checkers vs King | same | `kcheckercheckerk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 831,744 [831,744] (2,481,336) / 0 [0] (3,192,048) / 29,885,073 [0] (115,441,479) | 0 [0] (1,630,008) / 0 [0] (4,875,120) / 31,344,369 [2,509,327] (113,982,183) | 30,716,817 / 121,114,863; 31,344,369 / 120,487,311 | S3 table sha256:a040fbe9db576fe12918c644d2e18fd91a14dd62996bad938c74ac9d8fab99b2 VersionId QwjckTt1CgxAsAkYMZXrQ49bo7JCsGwD; certificate sha256:cf8638d03541d8dfd98315ff88c502aa4bc44d7323f223558eef9e6a11ade542 VersionId enN1I.jc7xrFs7o.QnYeIdRdcQcykREX; reachability v3 sha256:07925bd5642c740e93bfb1d6c9dbed1f8e32cbb9d95fc7be8c1e1c02222d21e7 VersionId IR4e8iOGxfKNKJL.M89BDB8nozz6nPMQ; result sha256:a5460fb6e1e44825cc58b87fa911e1bd63262890f579fe0dfdac28f6c8dbd6bc |
| `same:checker+giant` | King+Checker+Giant vs King | same | `kcheckergiantk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 5,347,274 [568,962] (6,424,787) / 0 [0] (2,238,662) / 14,333,543 [0] (47,571,574) | 0 [0] (2,284,232) / 4,005,411 [7,082] (6,418,607) / 19,041,849 [4,569,147] (44,165,741) | 19,680,817 / 56,235,023; 23,047,260 / 52,868,580 | S3 table sha256:e955cc58842563f9ee028468fb4016182b119fce5cd0fef1affcefc562b5455e VersionId 5Ivo0q1t2RntTcyY1KJqTBHspLYewhP4; wave certificate sha256:ef6949360a9985b8ea9b425cb08cc7135881e9d4260c4965203ac1fa03d3cf29 VersionId tyKUu9isTE8UXz9VZ1wLDY2zS5oxzbFX; reachability v3 sha256:0027cb7d34717a61c6e579824604cc808e544cc28de9fb825dcf0310a2c0aaa5 VersionId e1hbcSVh54W3g7S_zO2ZOmOeo6XU10zG |
| `same:checker+copycat` | King+Checker+Copycat vs King | same | `kcopycatcheckerk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 54,198,828 [1,598,400] (20,215,428) / 0 [0] (6,073,152) / 23,852 [0] (71,320,420) | 0 [0] (6,197,952) / 54,204,764 [4,157,020] (20,179,068) / 9,291,028 [9,222,436] (61,958,868) | 54,222,680 / 97,609,000; 63,495,792 / 88,335,888 | S3 table sha256:c3e7ea0d81609031e92f23cb66afb28dba614ac53381ce01b97f66dfbcd407f1 VersionId TCD0nf8t8eHQ3rotzXOVUybMHGjwGXyy; certificate sha256:261e0617524c8f05816a24793ed68f8766c96b7d3eb53267a16331ab23445827 VersionId QyOtcAhWSzcUGbovRPz8sRfZM3QMR6YE; reachability v3 sha256:40e3c0c39f4ad0bcda56e5abc6b61ce91a63222eafcf795d0873f4365b94e71b VersionId 3_dN9LDNrEpTwd4KmOauxmqV0qbsJQmg; result sha256:e956dd2a0d2e44117e87c82008f5a4f1fc13b21de02101685b5a91b712ead6ce |
| `same:checker+angel` | King+Checker+Angel vs King | same | `kcheckerangelk.uftb` | **CERTIFIED** | 455,495,040 | concrete | 0 [0] / 0 [0] / 96,678,300 [0] (131,069,220) | 0 [0] / 0 [0] / 102,066,272 [0] (125,681,248) | 96,678,300 / 131,069,220; 102,066,272 / 125,681,248 | S3 table sha256:e46148b4562bb3b9a4d048b4033c872428e76a16b42bb0a5c5a8b207843fb5f1 VersionId qr5knrJqBW3Bhkd_0kDAUj7fGRYPzmIb; certificate sha256:1f517191ceae39d6ac97abebf11f324a775ae8a3cd6b4694b481e12e0dfa081d VersionId NAGtTz4FLl_khTTUwpinek3tk4LXMCrF; reachability v3 sha256:cb23d46f174c755016070c6e794870e7f96f6e3510296688076583dd65140405 VersionId LU9oCNkqV0KZvqWMfRrgHXZZygctvNy3 |
| `same:checker+fisherman` | King+Checker+Fisherman vs King | same | `kcheckerfishermank.uftb` | **CERTIFIED** | 151,831,680 | concrete | 842,688 [842,688] (3,996,240) / 0 [0] (3,153,552) / 32,226,100 [17,666] (35,697,260) | 0 [0] (3,219,216) / 0 [0] (3,996,240) / 33,003,124 [5,164,863] (35,697,260) | 33,068,788 / 42,847,052; 33,003,124 / 42,912,716 | S3 table sha256:5c9eed290325205b234736476d1e391a9432c4196ce63aa815314ffee4db9cb8 VersionId .2tMi25lNoJI_uA7RwLWmMFC5K20Nu_D; wave certificate sha256:6dc56dd9b3333f76d9bd6fcba5fdc8778d18be867ac9d24223a725893c33cf2c VersionId szBknmRhYfqneCUL3Tv1R6YnT71fYLzc; reachability v3 sha256:975132bd2ed84bbc2f43eaa4998af0feed223ab27324f8548d596e22ed476196 VersionId tlskNPEfMTbTzaoWSlG1f59SXzE2ZFV1 |
| `same:checker+dragon` | King+Checker+Dragon vs King | same | `kcheckerdragonk.uftb` | **CERTIFIED** | 151,831,680 | concrete | 26,978,919 [842,688] (11,821,681) / 0 [0] (3,153,552) / 8 [0] (33,961,680) | 0 [0] (3,219,216) / 30,358,998 [2,075,136] (12,013,439) / 2,644,126 [2,644,110] (27,680,061) | 26,978,927 / 48,936,913; 33,003,124 / 42,912,716 | S3 table sha256:7d5f0907eb359db903667921cf2b0e969f402ef1edd7f3447f515c3d48cf280f VersionId boX6FBCgjv1OazU7D_8hBA6fTiqhght4; wave certificate sha256:a57c2e65c3e0f33e969e1c0bdc0961f3eb669ec7962564c72abfd59183b812a9 VersionId PfVK9yxo19_ByAF3BURpRJFSDMQyR8JT; reachability v3 sha256:ad64650ec20c403d0b7eca2418f0edcd918c91f7e5b6b879634033c697765ba7 VersionId 19BrkGBh6QCRwsPQ_9XrhIhlICI5jPcl |
| `same:giant+giant` | King+2 Giants vs King | same | `kgiantgiantk.uftb` | **CERTIFIED** | 18,978,960 | concrete | 1,467,264 [0] (1,555,208) / 0 [0] / 1,442,860 [0] (5,024,148) | 0 [0] (389,094) / 1,480,138 [1,076] / 2,596,100 [965,860] (5,024,148) | 2,910,124 / 6,579,356; 4,076,238 / 5,413,242 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5ef65c167436546403e73f85178905eae9b5c8ca5725b3bc08ea669e292c18b4 VersionId UOR2KAtOMqXZgu0nrS0LXqU69hkb20PR; result sha256:9cfc00607ab58e3be4b4312c6eef4abd6a5f95cf427316d5802bf290284847a7 |
| `same:giant+copycat` | King+Giant+Copycat vs King | same | `kcopycatgiantk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 16,108,512 [0] (8,577,712) / 0 [0] / 11,720 [0] (13,259,976) | 0 [0] (2,124,768) / 19,634,576 [2,733,184] / 2,938,600 [2,905,392] (13,259,976) | 16,120,232 / 21,837,688; 22,573,176 / 15,384,744 | S3 table sha256:ecfce4efda4f2236744b61cd203cd3375136b024016ea176ce2dd27032653543 VersionId UYuKIDogJI9wKeXpNfz8pXmrUJjNWhfU; wave certificate sha256:69d678526217108832cbe515931c05553a191d2049873ed7403023168caa8695 VersionId vIc.iZWjVjsNE4avn61W3073iK.a0AE5; reachability v3 sha256:4461f8556da5252f9a08ad2aaccd040011551f3c37f82d250ec41e2a47c3e897 VersionId Omz4LhhsDGOasGwdbAl.CN2XdGvlkvh_ |
| `same:giant+angel` | King+Giant+Angel vs King | same | `kgiantangelk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 5,812,435 [0] (8,922,600) / 38 [0] / 25,125,027 [0] (17,076,780) | 53,082 [52,903] (2,284,232) / 4,225,298 [8,194] / 33,297,488 [7,916,186] (17,076,780) | 30,937,500 / 25,999,380; 37,575,868 / 19,361,012 | S3 table sha256:336550dd867d8e3bef22fd204f3184e5567f01dd2e5543f762434eb2b024a3e4 VersionId f2ViRddoRzrQNJtxGtLKLwSpJskyyaNM; certificate sha256:fdffd784954d5aa33ed5c923286fd9449535e2a502719492a8eccc6ce8e22ebb VersionId 3HlLKJlPozA5er_leVhLQcKiwq7efinx; reachability v3 sha256:43cc089186e7f370a33fec69da0859a90618726ac5c6bc4317aa23ce46394fbe VersionId 0QZVVdEGCyaYn_X7WiYFFdkulbUVmh.C |
| `same:giant+fisherman` | King+Giant+Fisherman vs King | same | `kgiantfishermank.uftb` | **CERTIFIED** | 37,957,920 | concrete | 10,077,466 [128] (3,097,480) / 0 [0] / 111,754 [6,196] (5,692,260) | 0 [0] (1,142,116) / 9,403,952 [4,818] / 2,740,632 [2,409,218] (5,692,260) | 10,189,220 / 8,789,740; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:4c398de085b3854f248dc852aaa1c20dfbdd246d1baeca103ad3e22474dd9b9c VersionId pniI1GgcmG3ggD6E7YaBlNPA_jH46ZzA; result sha256:56728fb7a48b9dd7dc0652050521c168d1023d6da2160a34158b0c7b3b256217 |
| `same:giant+dragon` | King+Giant+Dragon vs King | same | `kgiantdragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 8,445,404 [0] (4,840,992) / 0 [0] / 304 [0] (5,692,260) | 0 [0] (1,142,116) / 11,293,652 [1,405,500] / 850,932 [850,652] (5,692,260) | 8,445,708 / 10,533,252; 12,144,584 / 6,834,376 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:f03b9f3a773363b97f0f52e62a08fd6ff00646c38b83b6a70a9988faa49bbb19 VersionId FQ1zfr6.fn9pUDLItdVnXlj2lAIbYWRz; result sha256:2a53fa8edbbc5a68bce7f1e66a52867626fda3ecb474f177377afd2fc4579015 |
| `same:copycat+copycat` | King+2 Copycats vs King | same | `kcopycatcopycatk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,649,072 [0] (6,134,928) / 0 [0] / 0 [0] (1,194,960) | 0 [0] (1,510,272) / 16,262,368 [3,958,208] / 11,360 [11,360] (1,194,960) | 11,649,072 / 7,329,888; 16,273,728 / 2,705,232 | S3 table sha256:5fab24af1ade0ae296904c4ace99224dce7d37b1fb3a386cce4628534b1fc353 VersionId 5OL3kuxQN02gA20M0_A4Q32fDHWtnqgX; wave certificate sha256:cc9f92ac2e74b87b3ba9f73a1bd676f335df80ddfcd5787682d036cc6302d186 VersionId Jg6orHnq_0Ghawpfaomlt.kE1Zi.jt3G; reachability v3 sha256:a0450e85095b91ad9bb966627f141fbb5da8bf727925fa279be9f0c36846ffa5 VersionId xen.5G0GClE13zfEnV_h6FFqmDAd58zJ |
| `same:copycat+angel` | King+Copycat+Angel vs King | same | `kcopycatangelk.uftb` | **CERTIFIED** | 303,663,360 | concrete | 112,623,592 [0] (32,618,304) / 0 [0] / 824,024 [0] (5,765,760) | 0 [0] (9,296,928) / 116,861,304 [11,452,192] / 19,907,688 [18,836,048] (5,765,760) | 113,447,616 / 38,384,064; 136,768,992 / 15,062,688 | S3 table sha256:0694fe873f88c5d6a3faf67f104747e8199e1442b001c0747fcfb93c055e2995 VersionId YVJNUdcgSIPcQPf1r0gcRMe_3IVeDIMu; certificate sha256:e0069c5abdbd028d1cd0b0554f2d00ace46a0617fbe2e5929173e9aa5129e37c VersionId iB9C7zKd.r6UKKy7_hhvn6PvdvVDpe4T; reachability v3 sha256:bf896cbbb928008273662c15993d9f0a2b23294c5b7f6acc3055123deaaa1dfd VersionId P8Sc_NIgEWL3LsMXepTgi61lenaffRbj |
| `same:copycat+fisherman` | King+Copycat+Fisherman vs King | same | — | **DEFERRED** | — | concrete | — | — | — | — |
| `same:copycat+dragon` | King+Copycat+Dragon vs King | same | `kcopycatdragonk.uftb` | **CERTIFIED** | 75,915,840 | concrete | 23,060,992 [0] (13,455,488) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,359,904 [6,296,864] / 57,600 [57,600] (1,441,440) | 23,060,992 / 14,896,928; 33,417,504 / 4,540,416 | S3 table sha256:90b889417ef6e4d332bd3f4e1e14cb0e0e753cb3bc053fb5cee61b62f55b07b1 VersionId fC.lBXxgCsJIit40YKPG8b6N7IFkoleE; wave certificate sha256:0c1799c9ed42a1df258884e260062e7adbee81baada5ca0de8f24c8741591b81 VersionId Ja5gFFJ.UTjJioTamiiY9pdVD5kfMY8f; reachability v3 sha256:a2db7416b3808e2b6aece716d0e1efa36104049b0cb8fe5c19aa7aa667326396 VersionId H3ARIXIe_54IKOtkEVI8ZLfTbLHclYGp |
| `same:angel+angel` | King+2 Angels vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:angel+fisherman` | King+Angel+Fisherman vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:angel+dragon` | King+Angel+Dragon vs King | same | `kangeldragonk.uftb` | **CERTIFIED** | 113,873,760 | concrete | 42,257,460 [0] (14,679,420) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 50,793,410 [5,546,424] / 2,924,254 [2,924,254] | 42,257,460 / 14,679,420; 53,717,664 / 3,219,216 | S3 table sha256:8698c67a8c7e044e91c7dfbaf7847fcf2c9e88504d85ea8cdc9a5f9b087f65eb VersionId Xwi8tV.BH50xV9RqbnnLHMnuo20SBHIT; certificate sha256:6695eb8f469dc5525258f56c9d6042cba9490a50df69ffe8af88ffcca111c139 VersionId IV4nd9OqhPTTisgDRt0rpsznjjufSfJT; reachability v3 sha256:f4d4f832522ccc98ae70e7ff8fef372969b68def0ddbfd5d26a1174c7e231e51 VersionId xpp.czFOJD4BnPdfOq9IHdupHa6cmGTF |
| `same:fisherman+fisherman` | King+2 Fishermans vs King | same | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `same:fisherman+dragon` | King+Fisherman+Dragon vs King | same | `kfishermandragonk.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,085,816 [306] (4,893,140) / 0 [0] / 4 [0] | 0 [0] (1,609,608) / 15,945,260 [1,078,918] / 1,424,092 [1,424,084] | 14,085,820 / 4,893,140; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:196379d7675bb646c2c4d77a9fe342413d7efdc05dfff736da40d43caa583859 VersionId sdMlC8whXHg0XLnFmrZfR3l8aySkh6gg |
| `same:dragon+dragon` | King+2 Dragons vs King | same | `kdragondragonk.uftb` | **CERTIFIED** | 18,978,960 | concrete | 5,682,590 [0] (3,806,890) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,655,726 [1,106,908] / 28,950 [28,950] | 5,682,590 / 3,806,890; 8,684,676 / 804,804 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1d4efc819c3dd1cf2fab127f168fa68a3a7863d0255345ef66098401149dc410 VersionId OIgmo3qhwxs_LVL7bGJAqoeL5QemzZ3I |
| `opposed:jester+jester` | King+Jester vs King+Jester | opposed | `kjesterkjester.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 5,105,984 [5,105,984] / 0 [0] / 13,872,976 [590,268] | 5,105,984 [5,105,984] / 0 [0] / 13,872,976 [590,268] | 18,978,960 / 0; 18,978,960 / 0 | S3 information archive sha256:3b92db6eb23004b93a2a9b68915a7e49b9ae9e39fadf236e1852c3789f680df5 VersionId cvDT8RgI_WwJAF2aCIJGUTDfHduiAiqT; certificate sha256:8217ad3cf1ef26da6d5a55f31ea64359bf9570419b9099395da7bf37e83bb8a9 VersionId r53FBqYru6vGkAYmyeQ8urT7hQRxxQ2d; information trivial v1 sha256:bbe3dd6f52e649b3602cecd854485b4211769e1a65bd1821de7a778cefeaaf39 VersionId f4ct4c8aFKJP.xjcs7qScjjyrquBSgn3 |
| `opposed:jester+knight` | King+Jester vs King+Knight | opposed | `kjesterkknight.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 2,898,404 [2,487,458] (3,093,244) / 0 [0] / 12,987,312 [188,486] | 334,432 [334,432] / 121,472 [4,148] / 18,523,056 [4,823,412] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:2d47b6a4135befeb6ea07b5800ff66a7122ecbe7b279f3195f1b62fcb96d616c VersionId btn5MqaYK0QN1WQnEHW8eQYUGGD5Rfn. |
| `opposed:jester+pawn` | King+Jester vs King+Pawn | opposed | `kjesterkpawn.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 23,425,182 [5,139,545] (6,186,488) / 976,756 [0] / 7,369,494 [66,471] | 7,007,584 [6,308,404] / 18,754,533 [199,846] / 12,195,803 [5,131,586] | 31,771,432 / 6,186,488; 37,957,920 / 0 | S3 information archive sha256:8bbaf07dd571e0e7aba19aafc7d4fe9856feddae7780ae034c412c83d6a19bcf VersionId KYJhlCJ9nvEACHtefiDGCL5y44EOzQG_; certificate sha256:1e7fc52d9a75e17795780c621d388c6c3ac94c0a8b09b4e44287971652b2c30d VersionId slKj4aAUs8NEXSUTRR9tRg9lNXCZTMvv; information trivial v1 sha256:ec45b7b2a841d7edb3296656885971e1e565ef9c57bbbcd8f5af512f4723168d VersionId khyCkFJcjmtnUSNE78j0IMX9ao8p9Ct9 |
| `opposed:jester+queen` | King+Jester vs King+Queen | opposed | `kjesterkqueen.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 2,489,288 [2,483,440] (3,093,244) / 7,339,356 [639,948] / 6,057,072 [254,204] | 16,116,718 [10,906,806] / 2,682 [0] / 2,859,560 [730,212] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:5f401ec8264a891a2823c4bf5009eb9cc8af49f98e8ba1fbe4e45da32ab94193 VersionId uHDL6_OO8_SqdrXsx0LfADB797vK8KOq |
| `opposed:jester+rook` | King+Jester vs King+Rook | opposed | `kjesterkrook.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 2,491,348 [2,483,448] (3,093,244) / 518,380 [241,048] / 12,875,988 [174,092] | 9,387,182 [8,186,390] / 4,634 [916] / 9,587,144 [560,872] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:b4576549724df48aaba069bb4ecfc1a525326d656f7fc3b49b382913ee93d45a VersionId TG2ixJ4NM023Oc2o5CY7mIAjyPJFsx4w |
| `opposed:jester+bishop` | King+Jester vs King+Bishop | opposed | `kjesterkbishop.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 2,506,936 [2,485,492] (3,093,244) / 0 [0] / 13,378,780 [195,360] | 488,456 [488,456] / 10,706 [2,560] / 18,479,798 [6,212,808] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:af97f6af0112d6c2109d428b27e1becd7077eb4a0480c51b5295b5bba372c74c VersionId qzGAvXwR2slGUxjx14YRKfiPe17FwROo |
| `opposed:jester+berserker` | King+Jester vs King+Berserker | opposed | `kjesterkberserker.uftb` | **CERTIFIED** | 379,579,200 | information v2 | 25,953,792 [24,834,416] (30,932,440) / 84,727,112 [61,722,380] / 48,176,256 [20,115,250] | 158,515,092 [144,751,376] / 513,032 [0] / 30,761,476 [10,048,188] | 158,857,160 / 30,932,440; 189,789,600 / 0 | S3 information archive sha256:3a274b45129a509764c3fa117659d3bc08b93bd06ec9ef293ce65801aae01612 VersionId q4UpNUW7bJXUSl23wY4wV4ZsSyg2MQZ4; certificate sha256:1ce7ce9c9a7a6f640d1c11612d6dc70cc9fe5591617bc2efca633b364c143bf7 VersionId LDArCy72gXFxJXyx5MvxzfepReYTmuK1; information trivial v1 sha256:ad6fd3c04ac49dc74004cfa5230a4e164ad2e071d5aa26e77adff8b4b1b5d8ca VersionId i0C6U1c0bSfHWltV9YaIYk6K1131qasg |
| `opposed:jester+bomb` | King+Jester vs King+Bomb | opposed | `kjesterkbomb.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 95,410 [598] (3,152,980) / 160 [160] (62,344) / 15,668,066 [1,476,958] | 3,278,154 [3,269,368] / 29,024 [28] / 15,671,782 [3,478,812] | 15,763,636 / 3,215,324; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:f25dfd182107f4a4c9f0ec355f615a0d6ef912587fcd25a6facfb350f88ff3a3 VersionId HHWyUcFYsEVKsXm7JNABdlIblynpTliz |
| `opposed:jester+ninja` | King+Jester vs King+Ninja | opposed | `kjesterkninja.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 2,499,692 [2,483,440] (3,093,244) / 5,971,304 [170,168] / 7,414,720 [202,112] | 14,621,888 [8,538,104] / 5,680 [0] / 4,351,392 [469,224] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:dfe8bf06b446943866b41f44374041914a59fd0797d5afa98e74026fc290baae VersionId Fn4jjtEwM9V6rjiTNU1kSWNSBXFQF3r0 |
| `opposed:jester+turtle` | King+Jester vs King+Turtle | opposed | `kjesterkturtle.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 8,899,984 [2,484,656] (3,093,244) / 0 [0] / 6,985,732 [118,352] | 264,054 [264,054] / 5,356,036 [2,080] / 13,358,870 [4,181,524] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:92f2b5c740503dcc0a02ace5dac4ec18f9838c24b91da9b413e8fb873f5365b7 VersionId t4wQcu6QGVB9nD4tBv6w8J0Rq0nsfG_Q |
| `opposed:jester+ghost` | King+Jester vs King+Ghost | opposed | `kjesterkghost.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. The full-domain perfect-recall graph reached 19,300,000/38,450,880 expanded roots with 40,163,507 nodes and a durable checkpoint before its 48-GiB resident gate failed closed at 52,150,984,704 bytes peak RSS. The checkpoint passed SHA-256 as 84aa2c2b4d8416b8b8d109a8e270a9dc15c34b627971be7e74b9efc7789663a3 and is S3 VersionId `GCYyV19fVUJpL8RyVODsmXydHYdPbOOc`; executable VersionId is `Xn2BNCXhPvTKSgyFycR2Urnv6w2Kk5ji`. Authenticated high-memory unit `ultimatefish-info-crossed-jester-ghost-migrated-v2` is currently inactive and intentionally paused for the certification sprint; its 67,416,858,732-byte restored checkpoint remains retained on i0b. Model sha256:829d6dc14920682a9de9a353660817d525fd5c3e831feeab1d7ded9baf78315d and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 remain unchanged. |
| `opposed:jester+mage` | King+Jester vs King+Mage | opposed | `kjesterkmage.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 15,885,716 [2,603,008] (3,093,244) / 0 [0] / 0 [0] | 125,972 [125,972] / 15,953,304 [2,614,380] / 2,899,684 [2,899,684] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:7bf9ca7b81ba717b6b386db81f486d5f402213d1329d82981673a9656d98deed VersionId pQhXHrS_h_g8kV0pdVqpIDtFDKAd91Z7 |
| `opposed:jester+penguin` | King+Jester vs King+Penguin | opposed | `kjesterkpenguin.uftb` | **CERTIFIED** | 303,663,360 | information v2 | 3,049,794 [2,720,960] (3,628,184) / 9,420 [0] / 17,507,218 [482,588] (127,637,064) | 167,500 [161,324] / 154,220 [368] / 23,872,896 [3,383,032] (127,637,064) | 20,566,432 / 131,265,248; 24,194,616 / 127,637,064 | S3 information archive sha256:0277f94cc9a26666057f2c544c238d4b06863abf270a5d3307b252e68db998ff VersionId DJG.eqowHZxV.dTR55TyL8vLRiUA1KXU; certificate sha256:92bf3192c483acd5f92a5bad24016c7ff07e4b6a0ec01af57afb263cebdf813d VersionId 8OGxIZ9xa0KOhwu.8ORDeJB7C.SboBy7; information trivial v1 sha256:0fef39512efab40ec0637f17f77210e178fcb6de71bcaf3d0a3225faeb856cce VersionId vHXH0tC1nLgodHxgzEjODM18T_K081X9 |
| `opposed:jester+parasite` | King+Jester vs King+Parasite | opposed | `kjesterkparasite.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 256,428 [1,656] (3,093,244) / 4,476,908 [697,670] / 11,152,380 [764,870] | 10,452,700 [5,472,048] / 87,668 [0] / 8,438,592 [164,468] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:46a09f7a4715d9617c6d2b09b84d814de34a336cec33ab270476016430232168 VersionId GSfPivpWCRsTffYh_fdq6tyCzyiNFh96 |
| `opposed:jester+devil` | King+Jester vs King+Devil | opposed | `kjesterkdevil.uftb` | **PLANNED** | 45,549,504 | information required | — | — | — | — |
| `opposed:jester+sludge` | King+Jester vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:jester+sniper` | King+Jester vs King+Sniper | opposed | `kjesterksniper.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 63,060,342 [10,409,248] (12,372,976) / 0 [0] / 482,522 [27,033] | 694,272 [694,216] / 61,245,762 [27,924] / 13,975,806 [13,125,905] | 63,542,864 / 12,372,976; 75,915,840 / 0 | S3 information archive sha256:11217a6cc35382a8d4909ef408425883375e09c372af0dd95a6d8e1edacf0b1a VersionId Fli9sSZgQ.0AlD6DZSX5jEvpHu4Q.mKJ; certificate sha256:ee6ecba4f9d611ece5b096bdc006bd4451b401f7a34fb2f8b2f871e426000667 VersionId 36sGeIzj_akB_QaHcG_lEjOIzBvKDypB; information trivial v1 sha256:6aaf73896b16d74610199b5f7238d4147fdc20a27c687fdaedf6ece66455301f VersionId AEMrC2b_mcgsojtfsDdn3Z16v5iTaJPH |
| `opposed:jester+prince` | King+Jester vs King+Prince | opposed | `kjesterkprince.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 2,519,404 [2,483,440] (3,093,244) / 6,440,420 [0] / 6,925,892 [119,568] | 14,646,010 [5,466,216] / 10,750 [0] / 4,322,200 [170,996] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:c11d51a7cea0062bdee8e462e2aa578cfe2b37c370ebf6be5439cd42c098a1e3 VersionId mkF8Gq9x_42I_9xbmjT23d2NleAEvj1n; certificate sha256:80c934aa3ada989320245f1bfce167eb1ff61be85d948d0f4c5d641a4603fd85 VersionId 2o2PYfuwTD0V7lfq3G.IBM2OosGTqpyL; information reachability v1 sha256:9cf05f4ec33498f9fe12bd388e1493f0e8725bef4bf96ee5e7a4b6cdc6f5c0fa VersionId zcZthWyAiRxR_d4dpEwgfe8fCHgsPCAq |
| `opposed:jester+checker` | King+Jester vs King+Checker | opposed | `kjesterkchecker.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 31,771,432 [5,206,016] (6,186,488) / 0 [0] (3,981,336) / 0 [0] (33,976,584) | 404,122 [404,122] (842,688) / 30,510,410 [26,594] (5,995,864) / 7,043,388 [7,043,388] (31,119,368) | 31,771,432 / 44,144,408; 37,957,920 / 37,957,920 | S3 information archive sha256:5e517cb12ed39a57143b5ed281d33edf334a470a5ecab90a3802ec4e42b54a61 VersionId Y6IAws9rsMYyhpydNzHhTlAHd_wVXMNl; certificate sha256:e730d7230b179cf3d1f4812291ad31bab7a9c73bec6f111ecdae85d794d20977 VersionId ZJwajqCsNlBtbf0B0PCcBCcTroo99mul; information trivial v1 sha256:df45af78a25432bca1fc075116d9e74f743bd6553df701fac79251341a192f0f VersionId 5kqE2Z45lAjbVvs0Gbh4LW6m4dNZJZM0 |
| `opposed:jester+giant` | King+Jester vs King+Giant | opposed | `kjesterkgiant.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 10,988,108 [2,773,676] (2,193,308) / 32 [24] / 105,252 [81,116] (5,692,260) | 516,144 [516,092] / 7,775,374 [2,262] / 4,995,182 [4,850,872] (5,692,260) | 11,093,392 / 7,885,568; 13,286,700 / 5,692,260 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; result sha256:0c0d2aa723badd35e14708cfecfaeeb0c8039ef4c76f741a43b60709ff08c615; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:ca5763bc6e98eaadc551461b170fb786fb4772933a7eea541a942c6849e45f2b VersionId tuTE1XdPP8XiHui6MV8VV42.AKrjPXBK |
| `opposed:jester+copycat` | King+Jester vs King+Copycat | opposed | `kcopycatkjester.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 13,451,688 [13,451,688] / 355,480 [128] / 22,709,312 [1,025,328] (1,441,440) | 9,262,160 [8,369,040] (5,955,168) / 0 [0] / 21,299,152 [353,024] (1,441,440) | 36,516,480 / 1,441,440; 30,561,312 / 7,396,608 | S3 information archive sha256:74f04b912f6d6261c3ddc183f6a5dd58f43dbc2b682545cba73c4f9473bf7f65 VersionId LbFNB.TV.CnUlvJialCImd6s0m2TwjYX; certificate sha256:53d9234c2fb54dfaaa0be7885e6cc4bc77af06f35847c33949d31adbcb5a95f6 VersionId 9KbUPkprlvNO.s8M.XDAc1b8mjJMH73h; information trivial v1 sha256:965d65fb38f6e09fa6f6308addec11f756f972856428aff279e43d386f30bf77 VersionId f9jB9mtLL_XDiI0sFn0.T9dHDM1b.EPn |
| `opposed:jester+angel` | King+Jester vs King+Angel | opposed | `kjesterkangel.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 22,519,462 [7,710,492] (3,093,244) / 0 [0] / 12,345,214 [588,768] | 125,972 [125,972] / 15,518,030 [9,704] / 22,313,918 [5,995,160] | 34,864,676 / 3,093,244; 37,957,920 / 0 | S3 information archive sha256:d2337ca267697790ff854744a8de9877a6a733a805b5eb75d293738886c074ca VersionId ItTwp9vjpYEMyp7b79klFN3GiGBzpEFn; certificate sha256:83475c0d2ba81af9df246cd2b187cc06b24b564a6d6a1c47d6cfe9e15ac5107a VersionId M5LV10OOesWPgtJS8SQ9NnrtQNNiCPvN; information trivial v1 sha256:cb46d3f222e7ecb672e175e0bd7290ff723771c291b5372a39f073e480c83b19 VersionId 05l4Fhi6fCxzJ8FHFVzRBcu.Qvl7mnPV |
| `opposed:jester+fisherman` | King+Jester vs King+Fisherman | opposed | `kjesterkfisherman.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 2,499,754 [2,487,404] (3,093,244) / 0 [0] / 13,385,962 [115,604] | 125,972 [125,972] / 11,842 [6,796] / 18,841,146 [2,901,032] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:c9deb54d7042f7395aea0e1eee7a1b148fe5709ef6398ef4f88cbd8281c115f9 VersionId 4EKQS4.CbZn7jlJvGqJkS8.gSNQFCLn. |
| `opposed:jester+dragon` | King+Jester vs King+Dragon | opposed | `kjesterkdragon.uftb` | **CERTIFIED** | 37,957,920 | information v2 | 2,493,996 [2,484,272] (3,093,244) / 6,688,516 [270,664] / 6,703,204 [205,840] | 15,063,608 [7,968,902] / 5,426 [1,296] / 3,909,926 [567,110] | 15,885,716 / 3,093,244; 18,978,960 / 0 | S3 information archive sha256:465748d3658d0d899fe107ca94864b4fca6cd6ac27d9f8ad0a0ace0023e49a52 VersionId DKOqUaJDqnZthHXHZ_fIWKfL6j1qQB0p; certificate sha256:dcfd5627b4baadd75cecf80293037e92c067d14bfab563516d203a0945fe120d VersionId At6IWH72LNtfnkwSirSWgjQVdg.dcOm4; information trivial v1 sha256:1701bbd81233cc52f7c1565576a8aeed52ba6c6f044f080597a2edb6588004b3 VersionId HSNJiVgu3JYE2jE.NdLCZ_Jfm2_biVFq |
| `opposed:knight+knight` | King+Knight vs King+Knight | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:knight+pawn` | King+Knight vs King+Pawn | opposed | `kknightkpawn.uftb` | **CERTIFIED** | 75,915,840 | concrete | 423 [57] (5,617,946) / 4,013,654 [2,357] (498) / 25,096,929 [4,388,242] (3,228,470) | 9,176,340 [5,839,044] (4,023,065) / 71 [2] (6) / 21,395,053 [1,536,618] (3,363,385) | 29,111,006 / 8,846,914; 30,571,464 / 7,386,456 | S3 table sha256:942dfdbdc53214aaefd199e6663a378bdcda82dcdb1499864bbf3ab47ed41331 VersionId DxmbTE7fugjQ60xp4FwMM57sAukedOE_; certificate sha256:c4a8a517474c9f81746d168f2b7df904774984157e047d9e67ea2877f3c96992 VersionId bGJi2_w60HkZylhxx7noBVMn7qwETvRc; reachability v3 sha256:ee518b458bf39738aa6aa9fc54b586726de492930a81c4bb5e0be1dc660fdf5d VersionId 0lFaF4g4D.QfiOk3dCdWcfv13mc3LbxI; result sha256:003d4d3cad41c9a6ea94e22d789b10a96fae0d71a6ca55cf56c8d82b0572f074 |
| `opposed:knight+queen` | King+Knight vs King+Queen | opposed | `kknightkqueen.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (2,808,960) / 13,559,860 [1,399,772] / 2,610,140 [2,348,088] | 11,888,556 [4,601,386] (7,035,616) / 0 [0] / 54,788 [54,780] | 16,170,000 / 2,808,960; 11,943,344 / 7,035,616 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:e9554a2809e6784420f16a915eefc0325f746d5ad70017e62df39dd810265fa7 VersionId U5WsJSeRU.nTQ0r1B_dGZ7KV.DO1ze78 |
| `opposed:knight+rook` | King+Knight vs King+Rook | opposed | `kknightkrook.uftb` | **CERTIFIED** | 37,957,920 | concrete | 16 [0] (2,808,960) / 1,871,092 [468,980] / 14,298,892 [2,507,944] | 6,652,526 [3,546,108] (4,951,436) / 4 [0] / 7,374,994 [344,664] | 16,170,000 / 2,808,960; 14,027,524 / 4,951,436 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5221c27513385b01a0e7343dfe571126fcd331a30219f292832fd1367abdb44c VersionId uhLLb9pMQg3mfLZzHsZWDSxoZboOy5Nd |
| `opposed:knight+bishop` | King+Knight vs King+Bishop | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:knight+berserker` | King+Knight vs King+Berserker | opposed | `kknightkberserker.uftb` | **CERTIFIED** | 379,579,200 | concrete | 20 [4] (28,089,600) / 124,506,824 [5,381,514] / 37,193,156 [23,481,420] | 43,541,844 [21,481,008] (133,257,124) / 4 [0] / 12,990,628 [134,364] | 161,700,000 / 28,089,600; 56,532,476 / 133,257,124 | S3 table sha256:f175cbafda61360a74b7569e1fec2abced06b9557e0327d58e53aaedb77c5a31 VersionId LVJqqQ2Rtoov7I4FlwhvSbOlAGZMS7SS; certificate sha256:b4c610c20e4bd6d03e2e274f607949e520e4296bf9a4e889dd3b4f6583431794 VersionId R5MNWo8AtiJFQHbaoMY6xZbfExLGig_5; reachability v3 sha256:f1d0488dd1eff23633cc089987478909375c15a72fa35816d6d94d252d5f493e VersionId T7CXiYDT1DEan9fKkSGZHA0MV83j1ban; result sha256:d24d5594074f02965001f4e7d7b7fc313443d2cc422fabf64bf3074318eb62aa |
| `opposed:knight+bomb` | King+Knight vs King+Bomb | opposed | `kknightkbomb.uftb` | **CERTIFIED** | 37,957,920 | concrete | 264 [0] (2,903,040) / 14,748,000 [362,658] (62,768) / 1,264,888 [930,332] | 15,062,940 [2,903,796] (3,846,020) / 52 [0] / 69,944 [69,932] (4) | 16,013,152 / 2,965,808; 15,132,936 / 3,846,024 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9693ea31fb14e7c73765eb111a5cb8900b36ec5a329fbe76a0f870f1cb11de12 VersionId 5M48oCfufJrRi561BC5j14bZwAcRbr2F |
| `opposed:knight+ninja` | King+Knight vs King+Ninja | opposed | `kknightkninja.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (2,808,960) / 13,522,364 [560,982] / 2,647,636 [2,348,060] | 13,657,124 [3,748,212] (5,259,100) / 0 [0] / 62,736 [62,724] | 16,170,000 / 2,808,960; 13,719,860 / 5,259,100 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:fba133a0a0b599aa48f1050aebb7771399b965941936429691fe6c86100fcd0f VersionId NB0hmXDHSlyMrkpE1XV3tP5PsNaiWZSc |
| `opposed:knight+turtle` | King+Knight vs King+Turtle | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:knight+ghost` | King+Knight vs King+Ghost | opposed | `kknightkghost.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 20 [4] (5,617,920) / 232,358 [0] (10,626) / 30,725,994 [3,372,468] (1,371,002) | 7,280,492 [6,452,236] (4,702,852) / 8 [0] / 25,974,568 [263,808] | 30,958,372 / 6,999,548; 33,255,068 / 4,702,852 | S3 information archive sha256:42fa05acd616973d212ddfe70f4860605b679db39d92d756bf6046a84e675882 VersionId _r_Elrcjl159cHgitIN50KaVtZOwwHLY; certificate sha256:b4fcc1344ff82fd5f0f2b32c36473da15e1d0fd280686ca829f80cd72b2169f1 VersionId SStfNZVMcPeWOlg0c9WtyvYJuHBWR31o; arbitrary sidecar sha256:ab895ddaa444bc2e8057154a860f43f10ebf19bce357cd983b6b0efa53ef9155; information trivial v1 sha256:c3e9cc0270494c39a1df4cb9ce9f02eaf18aa6fdc2aa723c28a623fcdf8491e0 VersionId n_6K4TrjBoa6xK4e8bHfpWP9H23AsKzU |
| `opposed:knight+mage` | King+Knight vs King+Mage | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:knight+penguin` | King+Knight vs King+Penguin | opposed | `kknightkpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 254,522 [27,724] (3,313,768) / 1,521,148 [0] / 19,105,178 [2,766,452] (127,637,064) | 4,862,070 [363,102] (1,941,024) / 95,978 [0] (228) / 17,229,532 [1,365,778] (127,702,848) | 20,880,848 / 130,950,832; 22,187,580 / 129,644,100 | S3 table sha256:cd1d0742be656efb3fc4ae231e80439025d8420ea1579840b790246a81668ac6 VersionId YNEg1baHbFa4qyjy4IoeyYwmNKVhRaUq; certificate sha256:a4e1f45eb6d8742cf5efac9bc54cafac52efd7c1ab7e0a528e9cd5d43220d1a9 VersionId CJn6p5QSw2QOlZk4qPcHFqAtO5emORvm; reachability v3 sha256:cd0dc678729c21c27e41c7b448ef73ca528fa3f8b2baa4293f89170857e1e7c8 VersionId oz0Qr_RDUFPeHhG4_DMDQdeqpXzqTqho; result sha256:78ae50654659bee099698857a92afd43267e01db4b86cee530e0242f6741738b |
| `opposed:knight+parasite` | King+Knight vs King+Parasite | opposed | `kknightkparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 20 [0] (2,808,960) / 14,791,096 [195,644] / 1,378,884 [1,054,596] | 15,817,696 [2,543,260] (3,093,244) / 4 [0] / 68,016 [67,996] | 16,170,000 / 2,808,960; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:575d4bfe5c726ea0e35e6215d1e60a0cced8a8404bebaf4b142ed7d6717d11e8 VersionId mNblaKSQEe6_JFzMJVxyUQnYnspMx2T5 |
| `opposed:knight+devil` | King+Knight vs King+Devil | opposed | `kknightkdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:knight+sludge` | King+Knight vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:knight+sniper` | King+Knight vs King+Sniper | opposed | `kknightksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 1,375 [132] (11,237,215) / 2,887 [0] (203) / 32,335,738 [4,800,681] (32,338,422) | 23,892 [8,805] (7,376,677) / 210 [0] (302) / 33,782,160 [3,908,529] (34,732,599) | 32,340,000 / 43,575,840; 33,806,262 / 42,109,578 | S3 table sha256:3c0661065296c0541ae17a218fcf01ab2a64b584d452b90ea92fa0db75febd50 VersionId OHoBQzYD6o5pZrZlCnsRkkbg3DKPqNN8; certificate sha256:70e96beac0d9cf892ff3a75d57aa790026b7682760487f1631b99e38c25c4c76 VersionId aHsw8ZCqiIWJqx8Wh8tfDx.ZQn2ygOrM; reachability v3 sha256:2fcccee18820f1863a5202eaeb5c4393d38d7a3a612e9cfb5a2ca14cb194c9c4 VersionId 1fy5St9zxK7Kno0q68Q4ix5IPrcEd4it; result sha256:7ca4cbeb56cec95bd22e0fc4787abc2e90b67bde75b2fb472f94c12c89cc5802 |
| `opposed:knight+prince` | King+Knight vs King+Prince | opposed | `kknightkprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 0 [0] (2,808,960) / 13,810,336 [4,780] / 2,359,664 [2,348,060] | 15,883,456 [2,613,948] (3,093,244) / 0 [0] / 2,260 [2,260] | 16,170,000 / 2,808,960; 15,885,716 / 3,093,244 | S3 table sha256:81cc89d1dfd6f45b332a18f1dadf1ee3e5c5727a8e00f3c00fbb75c7a7375599 VersionId 1MXp.5R.B5Ffui7JfPWcNVk3e8bV0n2L; certificate sha256:10780a3dbdeea93f890328e0f72059bd5dbb57fdb4b73178577ebb3da5bf6516 VersionId URa5SjOxqASK2EQvJlRMA4im8qEp.4Kt; reachability v3 sha256:5d723e5818170c8a86b57c840d411bb25e28dd9e67aa059e3869d41645cc9703 VersionId 5MTrbWu3krF9FQdbjkFtCVF4km9rbP2U result sha256:d5d481cdcaa3d22cfad03e11c29eb40a9a71596c72e1c972488c6339d918bace |
| `opposed:knight+checker` | King+Knight vs King+Checker | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:knight+giant` | King+Knight vs King+Giant | opposed | `kknightkgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (1,968,832) / 19,388 [24] / 11,298,480 [4,289,964] (5,692,260) | 37,890 [13,190] (3,051,612) / 0 [0] / 10,197,198 [2,410,594] (5,692,260) | 11,317,868 / 7,661,092; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:fe790a952af57272b00f7301882acf1ed927f7e6c83b933514987beca26a34c0 VersionId AuwWfuJkZx4FeRC_43qVptfqo2jR2dzA; result sha256:79d484eae6e15ed424647a3c1e1501ab06bb88d502be6b98b3a6c62cd2bc38ce |
| `opposed:knight+copycat` | King+Knight vs King+Copycat | opposed | `kcopycatkknight.uftb` | **CERTIFIED** | 75,915,840 | concrete | 8,216,824 [5,917,440] (8,221,984) / 16 [0] / 20,077,656 [555,000] (1,441,440) | 80 [16] (5,405,568) / 987,552 [16,728] / 30,123,280 [8,338,416] (1,441,440) | 28,294,496 / 9,663,424; 31,110,912 / 6,847,008 | S3 table sha256:5b87c5f4247094e8ad7a4c94a7066f7a07d6d3406ebbe8f6950a07b8f0236664 VersionId ARMwqigipNMDJWuvHMzO.9yhmOXjlCNQ; certificate sha256:df9f1a4ac449f1ec557ffa32988bd6a203908dd5795b25ada8f08f3ff606792c VersionId jYtnf8EYp.m2MRLsv4MKv9qXTz5A21yu; reachability v3 sha256:c315c756f497152903cf386853c75592cddf2e51e23942c305df5fdbbcfc2aa8 VersionId 2bEAs_VggOiC.7_WghR.aXqGeywhDwJH; result sha256:0be57fdeb8826649cbc8910da56cd9c57cae84dacee24caa06c245b9d257ba99 |
| `opposed:knight+angel` | King+Knight vs King+Angel | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:knight+fisherman` | King+Knight vs King+Fisherman | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:knight+dragon` | King+Knight vs King+Dragon | opposed | `kknightkdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (2,808,960) / 13,805,686 [467,032] / 2,364,314 [2,348,254] | 14,041,436 [3,637,916] (4,893,140) / 0 [0] / 44,384 [44,192] | 16,170,000 / 2,808,960; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:0a55874434ca5803800d0dd0ac12a149fd7bc825721fb35457f224461a07ab94 VersionId MQMje_ZdMXA_GC0Q8SvExkxARcp39NtV |
| `opposed:pawn+pawn` | King+Pawn vs King+Pawn | opposed | `kpawnkpawn.uftb` | **CERTIFIED** | 151,831,680 | concrete | 23,572,279 [11,162,115] (12,257,356) / 13,995,050 [138,662] (4,166,425) / 17,476,447 [2,306,947] (4,448,283) | 23,787,547 [11,367,389] (13,235,607) / 14,713,643 [110,159] (3,519,756) / 16,542,586 [2,130,176] (4,116,701) | 55,043,776 / 20,872,064; 55,043,776 / 20,872,064 | S3 table sha256:f3e91244513e245bcf7a9dddf4a3a35ff66458e18291e2b4fc272588ac3ec7c7 VersionId awBCUXNtJ9tPFn4WLTKTUN6MpIkSz9sr; certificate sha256:a1855f9ab86b866332ddab8bc1a9b0761a58c8c3a54aed760e12106b8a9f0c58 VersionId _SEw_VoCijSA.WS1xXZkb1u.VbGzNI9J; reachability v3 sha256:17610fa6870baa211fa35c6b41a31ec31886089d48133b7c44e2a38c31de8ed9 VersionId Me9llk.j3QZRi5ENYTzhebubcg0gJP8g; result sha256:8a40455ab36dd47c70f756c5da78aa22b0b4251b6cd81a2b24bf79122afdcb89 |
| `opposed:pawn+queen` | King+Pawn vs King+Queen | opposed | `kpawnkqueen.uftb` | **CERTIFIED** | 75,915,840 | concrete | 1,863,518 [1,863,452] (3,915,296) / 25,084,238 [3,308,287] (3,175,716) / 3,623,708 [3,483,344] (295,444) | 21,436,907 [8,399,223] (16,433,692) / 66 [66] / 87,255 [20,468] | 30,571,464 / 7,386,456; 21,524,228 / 16,433,692 | S3 table sha256:98a0ea2801bfd2ac4c7fe30e35c7bb35605bc599210308a1041dc7f727d80cb0 VersionId K0P3_m_CHy4Qmx6l84aWvaGO8Uvm.v6f; certificate sha256:9dffb0fd63142bda56a8f704692b1033b3995154fb9afa4b32a8551a9bb20b1c VersionId MtwXKLFCldRXr7jf30yY8GJDnx6qj5Ms; reachability v3 sha256:b267925ae2e37264fceb4191cd0f049a0b7a4624bbeb0ba9eaddc9ecfe23a161 VersionId N07Nt0FKXO1e5E4GFL_GuKGcy6NbZ8Jc; result sha256:9b937fa688d11089874a5cdad01ce7c474a18182d14ca394249613362b68af9e |
| `opposed:pawn+rook` | King+Pawn vs King+Rook | opposed | `kpawnkrook.uftb` | **CERTIFIED** | 75,915,840 | concrete | 4,625,746 [4,585,057] (3,915,296) / 21,929,710 [1,546,308] (3,183,976) / 4,016,008 [1,593,929] (287,184) | 23,671,433 [6,750,250] (12,693,436) / 26,202 [994] / 1,566,849 [155,345] | 30,571,464 / 7,386,456; 25,264,484 / 12,693,436 | S3 table sha256:ae577842ce9cf8b4dcb84ba376b660c1ce7db657394593da3d14d233a358209d VersionId jfR9H.zgQ0ZGua9W1lWIrAA6U5WEdSbL; certificate sha256:cad840d05991e0fd08ab44fbb3a0d4da026c6b0c798133945eb1fbf241ef2af4 VersionId ygC4699T_fvXE3yEApGdF8tQtSw0GteM; reachability v3 sha256:c911e9352db8171fd9a2510bee61531a961836405dcc32dd34444051c4bcf433 VersionId ZNFfpmB_sljF546M7qJkfPGjnY110N4Z; result sha256:4b193d764d1eeb5be729154e28f87739b5ceae01a796e2c4078a9cd746263d08 |
| `opposed:pawn+bishop` | King+Pawn vs King+Bishop | opposed | `kpawnkbishop.uftb` | **CERTIFIED** | 75,915,840 | concrete | 6,641,333 [5,361,314] (3,915,296) / 60 [0] (28) / 23,930,071 [2,066,663] (3,471,132) | 270 [0] (7,387,708) / 1,707,770 [1,673] / 25,819,248 [5,591,010] (3,042,924) | 30,571,464 / 7,386,456; 27,527,288 / 10,430,632 | S3 table sha256:9463883155544d1a8301ab0eddf92587dfe4b54595caf25ee41e3d47d503e0f6 VersionId H_uMipxCbe1i8yPZl1HF6qrCU2faSMVi; certificate sha256:123df905e4cdb3f9594a69e3f99b14ac01728b1808abe11d07357bf4da5567a0 VersionId lfNheZfGKUJyNj1bhk6WxD4znB9Fjrvh; reachability v3 sha256:b834bb848346e4a40ce8bca1a00ada817c1476d09ca6cc3731830c4d64b1b99b VersionId MD1F7M9B8t1cssXCyzuPrz_hBaRlFDcM; result sha256:e6cd72176ae656179adfbaf2bcff65c8c7eae1cf870af3a4d7f1955e3c8391fd |
| `opposed:pawn+berserker` | King+Pawn vs King+Berserker | opposed | `kpawnkberserker.uftb` | **CERTIFIED** | 759,158,400 | concrete | 22,818,110 [21,617,312] (39,152,960) / 258,732,582 [19,000,232] (31,847,016) / 24,163,948 [17,069,858] (2,864,584) | 92,795,246 [40,000,022] (277,726,386) / 1,514,284 [2,618] / 7,543,284 [35,639] | 305,714,640 / 73,864,560; 101,852,814 / 277,726,386 | S3 table sha256:b08c7324423452d1af0b442d77207e8a2789301e45f4cfcc0848533a6ec54989 VersionId ruHlTz0zj8eA5LaTUe249rvflj4QjPDV; certificate sha256:5dda525419c1822177f767c030033d4e328944a236727f3acc2e55c1c8769dee VersionId YNmWUtu1udIxKDP1beYKFFBBWbhxA3j1; reachability v3 sha256:eaa13b8b9b7e9d314d576dace035d014263d7032353a4825547e3a6005c6121b VersionId dlZtJLtISu_SjMcmZS.yrwsedox6v8Cs |
| `opposed:pawn+bomb` | King+Pawn vs King+Bomb | opposed | `kpawnkbomb.uftb` | **CERTIFIED** | 75,915,840 | concrete | 194,062 [168,124] (3,969,106) / 24,596,582 [1,481,981] (3,584,448) / 5,607,796 [3,691,488] (5,926) | 24,725,970 [5,229,463] (10,722,710) / 33,717 [398] (21) / 2,475,277 [156,077] (225) | 30,398,440 / 7,559,480; 27,234,964 / 10,722,956 | S3 table sha256:4520221e6fe723786fd1cca88e0619ae068e3999a0c9f328f911aee88f0fd78c VersionId e3l1NOhun8xgb09qZHsiRUZQF8xle0ep; certificate sha256:1c6d364eb9af182981f5a700552c93812b694441bd0ea2c4eb6e4580d3ad1b60 VersionId F9boVx_5rQAOPtbd3VrYjBKsOr6YXYfY; reachability v3 sha256:629f9f79156f5273e3adf478c0a819d5a4e8b08ef83e4986e592750d7cc82ca8 VersionId bON1nCmHEy0uV.TC98zdycph3QRknuev; result sha256:1b9a77a1082abc5d7c6016e232e40ea1a33df3889ecfea36a587711141340f08 |
| `opposed:pawn+ninja` | King+Pawn vs King+Ninja | opposed | `kpawnkninja.uftb` | **CERTIFIED** | 75,915,840 | concrete | 2,172,582 [2,168,729] (3,915,296) / 23,490,501 [1,478,375] (3,176,984) / 4,908,381 [4,150,537] (294,176) | 23,868,117 [6,929,869] (13,254,978) / 5,152 [275] / 829,673 [9,284] | 30,571,464 / 7,386,456; 24,702,942 / 13,254,978 | S3 table sha256:8cd08c55647660a7b54987ad7bf237c049ec8c6b9d92dbc318f81d812cf6b762 VersionId gcmQtNb1ux97TCqD8CsfwZNFTKnUNvTt; certificate sha256:da751f76bdbf9272cd1f4c7c020c8150448b57de9c59330c4cbe74caad6eafc5 VersionId bdUEpXs6cARbwjBB1BcjogYerGGKdVB5; reachability v3 sha256:9b95333c95709277ded346f5c6da30d4639bdbff05a74bb3fad8cfd912be9960 VersionId YaA6zbG8TSXtToigOyXfey1.FKH.u5jn; result sha256:6106bc0ffac808fb713f240873ea09af882214893b21eba180592ab214ea24dd |
| `opposed:pawn+turtle` | King+Pawn vs King+Turtle | opposed | `kpawnkturtle.uftb` | **CERTIFIED** | 75,915,840 | concrete | 16,009,417 [6,169,115] (3,915,296) / 4 [0] (4) / 14,562,043 [1,273,292] (3,471,156) | 6 [4] (4,794,334) / 10,898,578 [2,160] / 18,952,120 [3,795,693] (3,312,882) | 30,571,464 / 7,386,456; 29,850,704 / 8,107,216 | S3 table sha256:7cff193adc95fb56b0f122afc5e738a347d6353fa1ece49f360e55b9479b051e VersionId .Dnn.9ogCuBJwgy8VFWN368MeJVo_RPo; certificate sha256:322f68aa3099c76cb67f93efca1867a6abc462e8dbf4a947c6a03494d66976e9 VersionId eteSKOZostfJtb5NM0G3BbkdtMSQX.M4; reachability v3 sha256:a3a3cb91196e567f975845a2a62bbeb75c4bdc1b17df6ddb7a0efb8ed1e25302 VersionId LXsPM6yioPeNovSEr5P3QRJJzCoeCtOR; result sha256:aa8d282fabc538742ea79bb630156dea715e3935b04b8303e6909f4492cc24fa |
| `opposed:pawn+ghost` | King+Pawn vs King+Ghost | opposed | `kpawnkghost.uftb` | **PLANNED** | 151,831,680 | information required | — | — | — | All 64 corrected empty-tree shards are complete and retained. The old serial replay was stopped at 1,430,000/1,971,840 geometries after 14,874 seconds; only its five non-certifying partial graph files were removed. An exact specialization composed the 1,971,840-geometry, 1,458,198,008-edge, 985,912-stratum graph from all 64 exhaustive shard certificates with gap, offset, byte, conservation, payload, authentication, and compositional residuals zero and `full_replay_skipped 1`; blocks sha256:82ea57d0c94c863e2ef50432a3027087c98302e8be3bc55020f9296c62b781f7. The pending solver now uses an exact read-only ROBDD implication walk for every monotonicity proof instead of materializing `old & !new`, avoiding proof-only arena writes while retaining exhaustive semantics. Optimized source sha256:0e36f439827f02d88a21c0beee516c0a7b104fe49834a542498c60270437cb1f is S3 VersionId `1fFkLKovK9u97bC8P1EezyRkh4wjMaRO`; ARM64 binary sha256:69f3903f723b3dddcd2e94711d75c3830482e191b248ab0d0d124503fd46ff1d is VersionId `pSPDKiUQwgLFgODDxarpq69cpTO2tnqW`. It exhaustively checked 303,663,360 codec states, both Pawn substates and the promotion child witness, reproduced normalized source sha256:87cdf78459ad1eaa90cb36c2dc8e8ded1b704d61fcf807c1d739eef687dc9105, and passed a fresh exact-version binary restore; build certificate sha256:cb7b549080f644b7df336f5a22efdcddd014a9884a485e837e7bf590aabd7020. No Pawn solver is running yet because promotion requires the corrected opposed Queen/Ghost sidecar now being computed on i03; a dependency-gated supervisor stage will fire as soon as Queen/Ghost certifies, and launching against the invalidated older sidecar remains forbidden. The failed v7 compile log is retained and explicitly superseded. Merge wrapper sha256:26abbf224dffbd46a24e3563db6d169fbe8aed6d7d19725323ceaf9887e59205 is VersionId `k6UayJ8h.Gk7PKlASkrX7hTo_lgc9wBI`. |
| `opposed:pawn+mage` | King+Pawn vs King+Mage | opposed | `kpawnkmage.uftb` | **CERTIFIED** | 75,915,840 | concrete | 21,929,849 [6,757,635] (3,915,296) / 0 [0] / 8,641,615 [689,337] (3,471,160) | 0 [0] (3,219,216) / 16,912,650 [1,997,903] / 14,354,894 [3,389,739] (3,471,160) | 30,571,464 / 7,386,456; 31,267,544 / 6,690,376 | S3 table sha256:f87b4b20aaf6fa0e12ff1033f70fd5263f31c9a19dbf5c0e539ce7b868b5e3a9 VersionId XVZi9DYe0myOFsTSubtEKrzShBAN9N3_; certificate sha256:ae4b1f984c49902b963996781a2e7d72f38e23e1cc3f5b5af9f108ddff3be8fb VersionId CV0FcScjyqTRipev1TDNssd1cSd2DyRm; reachability v3 sha256:eeb610d0c9fb59d2fb94f1d4305509a594a2728651763609708cbdd51c76ba4a VersionId R3i87m21qujs7_QRPU8HzDABQ8s.KdOz; result sha256:9210d586bae8a151c6d17a3828360bf35d12256340340679cb659dd46f968602 |
| `opposed:pawn+penguin` | King+Pawn vs King+Penguin | opposed | `kpawnkpenguin.uftb` | **CERTIFIED** | 607,326,720 | concrete | 11,832,030 [6,469,242] (4,590,874) / 14,424,714 [75,614] (26,708) / 13,200,398 [2,417,755] (259,588,636) | 21,775,987 [1,768,283] (3,992,056) / 6,175,609 [15,037] (1,841) / 12,090,558 [1,330,185] (259,627,309) | 39,457,142 / 264,206,218; 40,042,154 / 263,621,206 | S3 table sha256:9cb581ecdfa1ce69206926be507fc26b6a97cd9d818c502f028cde96efba53d0 VersionId xvvqzfkZMPBLUloZYKxrkmy_oufNctqv; certificate sha256:e4ac9a4e6ccbba060e97df5ba987e8dd5d28e7df129fe22fddb5f446d0bf3f2a VersionId 6zdFXeK.sIK.xEr9kgNOPzE2J7zOJlNW; reachability v3 sha256:817c143ea80417fd68d1688c947018f7c302f3447836585436857477b11b526d VersionId qupydsp50ybCbeOlh3PqhvyC9BsPFee8; result sha256:a5bcd9c68346eeb9db9e968ff1aee112311b76c1ffa8a41796d4d90f183e37f3 |
| `opposed:pawn+parasite` | King+Pawn vs King+Parasite | opposed | `kpawnkparasite.uftb` | **CERTIFIED** | 75,915,840 | concrete | 497,022 [398,513] (3,915,296) / 23,399,121 [1,414,644] (3,470,252) / 6,675,321 [3,466,305] (908) | 24,637,692 [4,730,099] (9,358,672) / 127,859 [535] / 3,833,697 [3,031] | 30,571,464 / 7,386,456; 28,599,248 / 9,358,672 | S3 table sha256:7010366e158c34dc919f60ec31e711d6e120694c45000d52c05d42c10a79adb2 VersionId a5HuKB4iMAHA3kQ24xhO4CmG0MMaLD0k; certificate sha256:480e651382d5811c9736d8b7ead8fba096a6cc3f35522d5b07f69030730aae0b VersionId QPZHOzQ1Cib__67KXc7._8YA2H43Fldo; reachability v3 sha256:417daaf19229676b9ac51a0ccf248dcc07c51ce10a53b5f4348859f837db9dbd VersionId KHLq69bpZ6Qk6A.iF99.YM7tOka7f3Ah; result sha256:738e8d01a49355a2af55920857f29bc30349cfbd250e58ab4e2531008bfa1a18 |
| `opposed:pawn+devil` | King+Pawn vs King+Devil | opposed | `kpawnkdevil.uftb` | **PLANNED** | 91,099,008 | concrete | — | — | — | — |
| `opposed:pawn+sludge` | King+Pawn vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:pawn+sniper` | King+Pawn vs King+Sniper | opposed | `kpawnksniper.uftb` | **CERTIFIED** | 303,663,360 | concrete | 33,472,406 [12,448,985] (49,133,590) / 7,362 [65] (17,349) / 27,663,160 [2,363,887] (41,537,813) | 56,090 [20,101] (14,810,196) / 24,565,735 [4,758] (26,522,672) / 36,241,229 [6,822,621] (49,635,758) | 61,142,928 / 90,688,752; 60,863,054 / 90,968,626 | S3 table sha256:0e7d57234e600379b65d620310e657e396120e6d2ca9b9d7eb42e64b0ff25f92 VersionId usQzhVVdTdEXiqItxPE6fXe.w0awcrfk; certificate sha256:73bec4d102031e070989779b3cfac0c99cac7cbeaef766dd21d420c86d6e5edf VersionId yvLLP5_Nh6cdXuyni9M8JMipE6WyRM2e; reachability v3 sha256:9b1256b407ad3a9fdfd780d224899006c93f49bb2ca6e7ce9a59299b3fb5731f VersionId XN9mygG7EMbwCmPWdiibIcsPEGH172e4; result sha256:8cc3f328fdf589387bad9c452eaac1632ff8fc9d685bae6c68a239b4fca904fb |
| `opposed:pawn+prince` | King+Pawn vs King+Prince | opposed | `kpawnkprince.uftb` | **CERTIFIED** | 151,831,680 | concrete | 2,563,910 [2,525,732] (3,915,296) / 24,816,148 [2,190,553] (3,185,088) / 3,191,406 [2,721,245] (286,072) | 27,928,283 [4,732,262] (9,358,672) / 52,974 [756] / 617,991 [647] | 30,571,464 / 7,386,456; 28,599,248 / 9,358,672 | S3 table sha256:b822ef82354e7486a662b2f50132bb465b744d2250a04e483500b3b231dc23b8 VersionId had6zZDR98voFZTMDzBesN3QH0MUqKtb; certificate sha256:9112b92fbc856f9c2e609534a17401a19f62967753c6f89d6839a258e74c1e48 VersionId J3mOwn1HbrJqWpy6irnOptQyzPn5KWqo; reachability v3 sha256:6f921a0cf6ee97c44b90165cb9a4ff0192d8da85c3d400fee44e918bbc69634c VersionId 03riIxyopNICn_pu4uLRtf68P.C68xff result sha256:94dd0fcfadc972c6dc8b62154de61f92ebb30965047385bba0135202fab42faf |
| `opposed:pawn+checker` | King+Pawn vs King+Checker | opposed | `kpawnkchecker.uftb` | **CERTIFIED** | 303,663,360 | concrete | 31,563,353 [11,974,656] (10,049,616) / 0 [0] (7,962,672) / 26,529,999 [2,269,071] (75,726,040) | 1,513,008 [1,513,008] (8,164,848) / 22,643,668 [16,395] (9,414,334) / 36,854,940 [7,741,595] (73,240,882) | 58,093,352 / 93,738,328; 61,011,616 / 90,820,064 | S3 table sha256:d593d6930253c80d1708442a28a0d3447d148710d1e3f3411a4095b6cc2eb6d1 VersionId 5WOltkWm_v3V5oQZFd84o3LmsmNHmpn.; certificate sha256:089d237a2eb994abc025bb327a22bf4c0313d59bbca1c52a138c4817fa0706bc VersionId ozfyvGj1QAPtWaOnG8fWD9j8Lq9d26Jv; reachability v3 sha256:95eb1649fa454ef6bc12be19660c0d0a71e273d88fe9286f8c207131191025f7 VersionId JzHpjh.gRCr6eLrlzdcTs2ZIVlV0n1Ph; result sha256:0df247f65f30956c260fe1ded6ea75c5980bb15750129ba0364b3eeaab3feeb0 |
| `opposed:pawn+giant` | King+Pawn vs King+Giant | opposed | `kpawnkgiant.uftb` | **CERTIFIED** | 75,915,840 | concrete | 9,065,903 [5,011,218] (2,772,100) / 50,175 [481] (10,834) / 12,201,406 [2,021,179] (13,857,502) | 94,351 [29,122] (6,128,192) / 4,264,979 [1,437] / 14,022,468 [4,343,474] (13,447,930) | 21,317,484 / 16,640,436; 18,381,798 / 19,576,122 | S3 table sha256:ce91b596f1937c5f0080142edb5ec324f94e4fde054542cf8b6c435e046623b8 VersionId 4ggNk5fDAyDVIN2UBN24B2J8WxOqfZYH; certificate sha256:a7ab46dcac699bf7c45b46bfb1bf1ad6c8f0a8fb0d229002cd2a80adc8658239 VersionId fZFzKLn45bTEwOs5WRQ7C3a71Fl4f53C; reachability v3 sha256:0391a82962be4dfe4a7f6283c198c858dbe5a78844156be022b1df2e1426bce9 VersionId 9vUZoBEKUhyweP7Kdsh4tu9btu8WaGB3; result sha256:fb00809bfbbec185e10b43691bd64264050a7a95da659ee2772788c8a6e6d166 |
| `opposed:pawn+copycat` | King+Pawn vs King+Copycat | opposed | `kcopycatkpawn.uftb` | **CERTIFIED** | 151,831,680 | concrete | 38,167,160 [11,424,968] (22,084,440) / 8,982,700 [3,620] (104) / 3,797,596 [212,608] (2,883,840) | 21,266,620 [13,313,660] (7,901,188) / 28,728,548 [471,624] (5,668,856) / 8,822,696 [4,616,640] (3,527,932) | 50,947,456 / 24,968,384; 58,817,864 / 17,097,976 | S3 table sha256:682db8cc96ddef13858d892017d650d31ba9308000c0359345dd3e888f163c5f VersionId M4xTNTxbabaxx2Z8.HSTvQFXZoL4YTg4; certificate sha256:a24969b73beb6ec457cc6dd69af98bd5712a34380f82ef15dfb84ae2e4531ae4 VersionId kv2runRp_iQdB7EVSc6syi8XQqSTLBD6; reachability v3 sha256:791e676077bb940bfffc9b729df5a421f8cda5010d96bdb4fbef9940decfe8bc VersionId CtuU.kpeme3OTrgvD4aKStnQNAn5HE47; result sha256:ec2e86807cad1a0e48a35fef8320f1234b846ca6cc5db6d2e2521800c4cecb29 |
| `opposed:pawn+angel` | King+Pawn vs King+Angel | opposed | `kpawnkangel.uftb` | **CERTIFIED** | 151,831,680 | concrete | 26,415,915 [12,711,074] (3,915,296) / 31,763 [1,466] (4,858) / 38,285,914 [5,561,540] (7,262,094) | 82,851 [6,452] (6,452,194) / 16,404,078 [1,843] / 46,048,159 [5,396,588] (6,928,558) | 64,733,592 / 11,182,248; 62,535,088 / 13,380,752 | S3 table sha256:2cb40adf85bdf5f6900b4b96d89572edc7c9f293caffa6274d2e1779f1222198 VersionId NxXYWLiuaV7xdzU72A320X9oKzl1aCQH; certificate sha256:cf0f3915fd93c28fd1849972a83bf9afc2f416eed863511a883040fa3c419797 VersionId poxAJpCB8iF10ouoZEN_ltC5UXDs092d; reachability v3 sha256:831f15cea3b273079bc5b9c71bc842d680323589427dc5e30e53d5934441af2b VersionId YfotZdu9b9_XylTUXFO514geyPqj26fb |
| `opposed:pawn+fisherman` | King+Pawn vs King+Fisherman | opposed | `kpawnkfisherman.uftb` | **CERTIFIED** | 75,915,840 | concrete | 8,325,772 [6,146,233] (3,915,296) / 0 [0] / 22,245,692 [1,300,739] (3,471,160) | 0 [0] (3,219,216) / 3,521,082 [118,619] / 27,746,462 [2,643,796] (3,471,160) | 30,571,464 / 7,386,456; 31,267,544 / 6,690,376 | S3 table sha256:4850ec9b5fe6204a0ab1de252ec750ba32018be777063670352eb9f50a16faa5 VersionId r4hkFLN7MYlh2EPFwzxeDfc5Rt.aVe78; certificate sha256:cbbb0f1eb5308ec39d5a7a0e06c485d73ebac572521b308ed9b3156c40f93a93 VersionId LQWXMFMnU_edCIPCMsqKVRZPU.J2XHin; reachability v3 sha256:2958d2938221b4c7a7a08f305c7ebd0e8e52f9b9ff98ca8a8284c98163a88c32 VersionId 7Ny6ZFYXBAykIWITS0Ygj_D2QDNQ_D5K; result sha256:9a6828c3e5a21875c77b6d2e6d00ffad1c95736f92f05eb0db506d743261f2cd |
| `opposed:pawn+dragon` | King+Pawn vs King+Dragon | opposed | `kpawnkdragon.uftb` | **CERTIFIED** | 75,915,840 | concrete | 2,144,900 [2,144,201] (3,915,296) / 24,362,176 [1,721,244] (3,183,656) / 4,064,388 [3,904,740] (287,504) | 25,183,197 [6,763,796] (12,587,170) / 775 [229] / 186,778 [26,841] | 30,571,464 / 7,386,456; 25,370,750 / 12,587,170 | S3 table sha256:6226145824602f0547875394df0ca04a1379cd18511697e27ac2135db5bd2adb VersionId hyv1Y.41GDcr_CJJClB.F9GJypruTN_1; certificate sha256:efad443a7d106fd5fefd71f9df6753fa23edf29c4e60adf1b696504b465c5065 VersionId 8t9LAelp7KI54UR92bhkUj4tphtJMyh.; reachability v3 sha256:f16c5d0b5a926695f38c932b3dfebad9a9c58566b5fa21dcb42a1d4fc6537b3e VersionId FVEhqisd6VGW.iHzcP9dPuHKPWNGrU2V; result sha256:2332bba62a6c7ba2935cec3faf337c14c9ba08ea11b7a338b6c54dfa46ca3018 |
| `opposed:queen+queen` | King+Queen vs King+Queen | opposed | `kqueenkqueen.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,619,002 [4,275,534] (7,035,616) / 47,930 [31,796] / 7,276,412 [369,904] | 4,619,002 [4,275,534] (7,035,616) / 47,930 [31,796] / 7,276,412 [369,904] | 11,943,344 / 7,035,616; 11,943,344 / 7,035,616 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:96dcc0e1b25008cb1c917b3b57a1f4423b3c030e26ee9988fe99709c7be23ec8 VersionId OtwGr0l.dZ39SOwz.2RPri5209TNkEjx |
| `opposed:queen+rook` | King+Queen vs King+Rook | opposed | `kqueenkrook.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,847,368 [4,577,736] (7,035,616) / 24,998 [21,462] / 70,978 [59,018] | 3,696,724 [3,522,564] (4,951,436) / 9,667,734 [556,738] / 663,066 [275,288] | 11,943,344 / 7,035,616; 14,027,524 / 4,951,436 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:607a9cdbba8894ae1c2be6b6551bcded55f57145b789e30ad00fe462886b8cc7 VersionId 8eFuJbozRn6Ic5d9bR6MJmbd1us4YRvD |
| `opposed:queen+bishop` | King+Queen vs King+Bishop | opposed | `kqueenkbishop.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,915,826 [4,592,488] (7,035,616) / 0 [0] / 27,518 [27,518] | 0 [0] (3,693,788) / 12,200,676 [841,966] / 3,084,496 [3,007,772] | 11,943,344 / 7,035,616; 15,285,172 / 3,693,788 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:dea9b4630082992adb0502964859650b70ed97362ebf89f0270c065c2f9423b5 VersionId uhk4b6yIQjQ4Q3WHC6B85FUxAC2aeuIy |
| `opposed:queen+berserker` | King+Queen vs King+Berserker | opposed | `kqueenkberserker.uftb` | **CERTIFIED** | 379,579,200 | concrete | 55,908,752 [42,743,894] (70,356,160) / 49,276,058 [2,289,660] / 14,248,630 [3,288,486] | 32,009,490 [21,373,322] (133,257,124) / 11,339,090 [1,426,458] / 13,183,896 [634,662] | 119,433,440 / 70,356,160; 56,532,476 / 133,257,124 | S3 table sha256:c3f098ef1fc68325ae6d067a16f97af4492ada03c78e5bd5eeda4c8cfb2a5112 VersionId sDIopTwfRhRosUrc4iJ8YHNbWRiur_Kd; certificate sha256:e4c2aa223719232634bbecb36075b568e6d79d457f7cebba9f8fb56941d40d4f VersionId v20uqHUK0v4MHRnWgxH6B.yza5s8u0AF; reachability v3 sha256:3f629b504f67d1238754d9077d8e0a3f2873abdabe9f7b1dd512c81eecfd44f9 VersionId HroIyvyo8R7RJ3rEMJrnRcRBMhtmsDxu; result sha256:87fb45d7076234f070a3c1e14a4516fbb1dffb7cf28c87736da03ea95a96e2d7 |
| `opposed:queen+bomb` | King+Queen vs King+Bomb | opposed | `kqueenkbomb.uftb` | **CERTIFIED** | 37,957,920 | concrete | 1,421,172 [423,388] (7,353,252) / 1,282,090 [100,374] (46,244) / 8,873,510 [2,884,652] (2,692) | 3,611,678 [1,484,816] (3,842,796) / 670,572 [46,234] (528) / 10,850,686 [2,053,184] (2,700) | 11,576,772 / 7,402,188; 15,132,936 / 3,846,024 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8c7ed878d071579dde1e9714f699bd7436bf36844ccc873935392b0addd16fa9 VersionId POBXG9raJDp9WZpg2yUU3Xla0Gsjv6qG |
| `opposed:queen+ninja` | King+Queen vs King+Ninja | opposed | `kqueenkninja.uftb` | **CERTIFIED** | 37,957,920 | concrete | 7,529,430 [4,275,858] (7,035,616) / 9,456 [304] / 4,404,458 [335,746] | 3,622,556 [3,581,154] (5,259,100) / 1,365,734 [278,624] / 8,731,570 [251,206] | 11,943,344 / 7,035,616; 13,719,860 / 5,259,100 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:ab7427b34c26932369c13b57b886bba0950e22a1df8df60ea23c8a7ad0c9dca9 VersionId JL6h.zxNXrrhkMfBgW9wdxVeSQ.zGG7S |
| `opposed:queen+turtle` | King+Queen vs King+Turtle | opposed | `kqueenkturtle.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,943,312 [4,601,370] (7,035,616) / 0 [0] / 32 [32] | 0 [0] (2,397,164) / 14,534,402 [1,159,532] / 2,047,394 [2,047,314] | 11,943,344 / 7,035,616; 16,581,796 / 2,397,164 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8cfa72c769e7e67fd78f2f8fd4b9b28644a1bcf4a315a9f222d26a91bd9573b7 VersionId ZQtoLNWJl5IvMmz4wqfAi3dS5Udfpktc |
| `opposed:queen+ghost` | King+Queen vs King+Ghost | opposed | `kqueenkghost.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. All 64 corrected empty-tree shards remain authenticated. The old serial replay was stopped after 435,000/492,960 geometries and only its five non-certifying partial graph files were removed; its merge log and all shards remain retained. Successor `ultimatefish-info-kqueenkghost-source-order-v6` is active on i03 CPU 14 under 40/48-GiB gates. Its native binary exhaustively checked 151,831,680 codec states, normalized the exact source to sha256:8bd2029be1f63502b80db7a0e173fa00d441723ce31170b4b998c85aa2b3511a, composed the 492,960-geometry/690,119,718-edge graph from the 64 exhaustive shard certificates with zero gap, offset, byte, conservation, payload, and authentication residuals, explicitly skipped the redundant full replay, passed its disk/RAM gates, and entered Bellman iteration 1. Source bundle sha256:a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 is S3 VersionId `WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3`; ARM64 binary sha256:d305be9bbb8ad8401d3be31c8ba6daf3c56efc306b5def0111f21a8ae492699c is VersionId `4Wanayq8gd18cwrEfNgbAj6YGeM1Asme`; wrapper sha256:ae571bd528600a300fc689517341e9323aaa8ce75b9f0b3ba887229a429e3e5c is VersionId `AoVmlQduwsRrWOD9Zk6y0ccBUqlojCuZ`. Old graphs and failure evidence remain retained and non-certifying. |
| `opposed:queen+mage` | King+Queen vs King+Mage | opposed | `kqueenkmage.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,943,344 [4,601,402] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,938,592 [5,918,152] / 1,430,760 [1,430,760] | 11,943,344 / 7,035,616; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:fa56cdb89e1583f16824be38c1334c07b0d5441911aca918669cbc8d255d52cd VersionId nOs9NpPuRZr59mbWqa_u8txSkMidvQp7 |
| `opposed:queen+penguin` | King+Queen vs King+Penguin | opposed | `kqueenkpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 14,124,912 [5,359,504] (8,257,358) / 110,644 [0] / 1,701,702 [126,786] (127,637,064) | 294,258 [8,974] (1,900,844) / 14,096,292 [1,033,564] (384) / 7,797,030 [1,646,724] (127,742,872) | 15,937,258 / 135,894,422; 22,187,580 / 129,644,100 | S3 table sha256:69ad126479bf180c0985cc86e4eb6775fed8665cb0a0cf92c50146d5f4d9a344 VersionId G4GuJ5JbagtoWyMS9Vm88noG.0W2vXaf; certificate sha256:db5057753bc255ac0fd8b3f0895d9616cf91bf88c52b8358f3578ce104afdd48 VersionId d4_BNzBbraUeBHqhLq8oRaK6T5mruE_G; reachability v3 sha256:874df5000e182cf7495613748304f5aee2e0fac051a94efbbb02bb6806220280 VersionId H0zocSaV_PsMfafN4p9uanzqCNDOR22t; result sha256:4b88a0fc26abf45532c892cee3a6ac6826dc9aa177cf2dbac82ad3cfc63c614a |
| `opposed:queen+parasite` | King+Queen vs King+Parasite | opposed | `kqueenkparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 2,353,096 [585,500] (7,035,616) / 794,236 [61,178] / 8,796,012 [2,192,962] | 3,740,612 [2,510,592] (3,093,244) / 1,561,098 [82,446] / 10,584,006 [572,746] | 11,943,344 / 7,035,616; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:2aef41cbe509704d365a07f12d41b6c04ff10396446ea8558809225818501e3d VersionId e7xaCb.eo.VtG1r5.i1PQUE_J_H11vKt |
| `opposed:queen+devil` | King+Queen vs King+Devil | opposed | `kqueenkdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:queen+sludge` | King+Queen vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:queen+sniper` | King+Queen vs King+Sniper | opposed | `kqueenksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 23,861,263 [9,200,785] (52,003,727) / 639 [0] / 24,786 [20,688] (25,425) | 11,695 [8,138] (7,374,850) / 30,068,292 [2,559,973] (31,767,334) / 3,726,275 [3,651,252] (2,967,394) | 23,886,688 / 52,029,152; 33,806,262 / 42,109,578 | S3 table sha256:ef2a72acbc9b3b04c32faf375eda00693f20f232fc475416e420ab9ef311d23a VersionId DjvFTbtj8ITPqM1sv3c5ppY5Z6CNhRoy; certificate sha256:cb4ed282616a28fa1a7e144d86a378987267154465529377dd52bcbcb86ea9dc VersionId BFIks9PV_t3sNFqZ_yXdrdutRcQ9Y1_A; reachability v3 sha256:d0a630d6e38f104cbe740d54c08180a233d98ee17de8e11ffacdcde4d306d0f7 VersionId y92KwDrAzoPa4b5MPFq4orRicGFWSrf4; result sha256:15960e29f126027de6aa80081361c6a1274d90e8e7d9e9aca49d9f87153e6d32 |
| `opposed:queen+prince` | King+Queen vs King+Prince | opposed | `kqueenkprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 8,281,508 [4,274,320] (7,035,616) / 839,582 [0] / 2,822,254 [327,082] | 8,540,824 [2,744,162] (3,093,244) / 2,678,044 [378,616] / 4,666,848 [45,662] | 11,943,344 / 7,035,616; 15,885,716 / 3,093,244 | S3 table sha256:07fcfe50364cc717cb08bb69aac4ae61278208352b826eb3c798c7ae2a7811ab VersionId wJcayFSuXR6KuiGNvE249xe1aU0_y1Lw; certificate sha256:7ea4279b52e5f1318155c5dd34a74400e1cb317b7701abcf2e4ccca4d3ca9081 VersionId YLVk6NULZwTWyxHRGTuOB.DHK5Thqx7P; reachability v3 sha256:3509d47b66c0343d9c47434cbc578c6c1377b910fba42b5f46742d01aab36463 VersionId y18asyiczyo0x43y0RPx6aQimD.9xtyY result sha256:47686caa18bd36c29906b81a4470bc346fcbfe65b5031c127afadfb0fbd4a274 |
| `opposed:queen+checker` | King+Queen vs King+Checker | opposed | `kqueenkchecker.uftb` | **CERTIFIED** | 151,831,680 | concrete | 22,705,453 [8,810,659] (15,252,462) / 0 [0] (3,981,336) / 5 [5] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,546 [2,570,087] (15,131,060) / 4,047,886 [4,047,876] (22,980,420) | 22,705,458 / 53,210,382; 33,808,120 / 42,107,720 | S3 table sha256:f05cb79909e7194a072f133b81f179c2c1a9098162eb1f6f68b6021ca3348c58 VersionId Z_DsqW0Ge_gbiZm_5YzfkAFCGLin79F7; certificate sha256:4b050ac8112c4b71a11e8b7c03410fa53da210bc5d0fbb67c95b79785692f4ee VersionId YurdfBMIODUg7_Pp9nazcBF4Lkagsksg; reachability v3 sha256:33e79955ec66f98506e55392b25be670e16bbd56b1b588f72377536c3d0b86dc VersionId YXJxcW5Tp7iK.3IDY.IvFL5MfdXeClls; result sha256:3f93dbd960fe79d2fedffc2df55c8a7df4f074c8154f44926a1443b08d0b77df |
| `opposed:queen+giant` | King+Queen vs King+Giant | opposed | `kqueenkgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 8,456,120 [5,941,250] (4,821,246) / 7,090 [0] / 2,244 [2,218] (5,692,260) | 20,818 [8,038] (3,051,612) / 7,904,922 [1,215,392] / 2,309,348 [2,307,354] (5,692,260) | 8,465,454 / 10,513,506; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:988bf7645a60d838bd4cfafc2b3f14f396e2db1218f7c710454ebdc10ec8706a VersionId E6T5E1CxvNwYgm1zQpaAjgLSp8WpXEPS; result sha256:34a72a3b8cf5660a35905e91dd4d311bf0738b035f3889c57f577d0a8606c93b |
| `opposed:queen+copycat` | King+Queen vs King+Copycat | opposed | `kcopycatkqueen.uftb` | **CERTIFIED** | 75,915,840 | concrete | 5,928,304 [5,904,928] (8,221,984) / 22,013,424 [2,465,504] / 352,768 [350,168] (1,441,440) | 23,184,848 [13,947,056] (13,318,456) / 11,304 [0] / 1,872 [1,808] (1,441,440) | 28,294,496 / 9,663,424; 23,198,024 / 14,759,896 | S3 sha256:2c37e428063f1917bd2a92c682ebc318e3c86c1b70cfde4f44148ec731471162 VersionId wwrkJam6HgCpGUJF_Yr9c4.KAFyquczy; certificate sha256:14ac858039d343bdcac06e9ad73e9b79fdbb0f35b155d1f3411afa65d46e57d6 VersionId jsMKAP_Dd3QPfpdk8xSFokb1QzcmmhiH; output sha256:e279ecddd46c0e050b1ce11c473f56ec944002dcbef8cb83738f046590a82a2d; reachability v3 sha256:4e4af83d83084f299cf4c1c9030f1fc911ab3f6a01e584e353c429c4aedcb5b3 VersionId LmqBd0VJcg.el9sjD7xr4ihEjgPqMQ1R |
| `opposed:queen+angel` | King+Queen vs King+Angel | opposed | `kqueenkangel.uftb` | **CERTIFIED** | 75,915,840 | concrete | 28,060,204 [15,351,792] (7,035,616) / 10,324 [20] / 2,851,776 [767,106] | 26,556 [1,708] (3,219,216) / 25,622,336 [1,281,902] / 9,089,812 [2,998,384] | 30,922,304 / 7,035,616; 34,738,704 / 3,219,216 | S3 table sha256:0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c VersionId PhoLsJIi0SeWiAS8NAoFMPE7lqc6lBt6; certificate sha256:d3008009ff683a3d13034a6539b35eb64a4d151d8f5c408d631162ef47c63e7e VersionId R8XCxOGR7UVxfi6vEJ7CWxdVupkWEHqe; reachability v3 sha256:ecce223327e95fa925b2b4417031542722745d951177bac77ed47315d4decbc8 VersionId MFMBIIWFrn2EBNBRpxlsDp9wqoayyiBz |
| `opposed:queen+fisherman` | King+Queen vs King+Fisherman | opposed | `kqueenkfisherman.uftb` | **CERTIFIED** | 37,957,920 | concrete | 11,943,344 [4,601,402] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,953,468 [1,514,416] / 1,415,884 [1,415,884] | 11,943,344 / 7,035,616; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:83744781a630a38550f886a7729ee1b76a7230303fc5a0a50da7d2d116fefead VersionId jEx3NAs5ZD9WNH_Dxpc3HV3doOEmoatg |
| `opposed:queen+dragon` | King+Queen vs King+Dragon | opposed | `kqueenkdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 7,965,806 [4,276,854] (7,035,616) / 108,042 [98,604] / 3,869,496 [372,202] | 4,290,082 [3,407,090] (4,893,140) / 1,437,880 [536,946] / 8,357,858 [469,644] | 11,943,344 / 7,035,616; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:d3a43a735746fd57828bc702aa185226619561233526354b9424ce63f0e1fab9 VersionId UKCkqwnDqzWZfBKWTOuvMTaaGi.nSTtK |
| `opposed:rook+rook` | King+Rook vs King+Rook | opposed | `krookkrook.uftb` | **CERTIFIED** | 37,957,920 | concrete | 3,748,496 [3,522,794] (4,951,436) / 85,386 [75,764] / 10,193,642 [336,990] | 3,748,496 [3,522,794] (4,951,436) / 85,386 [75,764] / 10,193,642 [336,990] | 14,027,524 / 4,951,436; 14,027,524 / 4,951,436 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:76f403327714bfd4b85b188c2108000873e49902bf9bf3297e41f4a0321c0a4a VersionId Ckh3vYyk4GDhCoGCM4_BzyW0PEsbzpU7 |
| `opposed:rook+bishop` | King+Rook vs King+Bishop | opposed | `krookkbishop.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,582,954 [3,527,016] (4,951,436) / 0 [0] / 9,444,570 [437,554] | 0 [0] (3,693,788) / 423,770 [357,544] / 14,861,402 [3,173,920] | 14,027,524 / 4,951,436; 15,285,172 / 3,693,788 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:f29339ae6a09243c6e280fe7cc3b7e3273c9050c901f4e641fb248261d960a93 VersionId kC8I5qaOrorLL0GQlf03dRdX0zRTNpLV |
| `opposed:rook+berserker` | King+Rook vs King+Berserker | opposed | `krookkberserker.uftb` | **CERTIFIED** | 379,579,200 | concrete | 38,318,464 [35,226,490] (49,514,360) / 89,794,560 [3,815,268] / 12,162,216 [2,725,650] | 42,548,358 [21,471,642] (133,257,124) / 1,645,304 [576,320] / 12,338,814 [325,482] | 140,275,240 / 49,514,360; 56,532,476 / 133,257,124 | S3 table sha256:f2acd468a40525a09933dd1d3c0248a19848212512c3f86727f2be79339a9536 VersionId mpJO63EULZ2BYc.2gNxj6tZAohSMKHm_; certificate sha256:218f38f5855caf2868a1f3073a1d463395c5e46ccb027b143cb49ffb70c10d49 VersionId doZR1fXUiqeMmUR82zs2k3ey0deXLLys; reachability v3 sha256:88d4d3c9df6a1cad9cb26e7e114c92bdd59e309c90f34be284aec7772d03a9cd VersionId swnIksbX6kh6sMW_K5pOuxJP0iXPUsFX; result sha256:993cc0cd421fbc0d161bde2dd1dc8a366358052d8ad70cb51ec4aa33f62e8e4b |
| `opposed:rook+bomb` | King+Rook vs King+Bomb | opposed | `krookkbomb.uftb` | **CERTIFIED** | 37,957,920 | concrete | 91,214 [14,144] (5,176,766) / 2,816,466 [204,232] (54,846) / 10,838,228 [2,410,652] (1,440) | 6,144,700 [1,763,914] (3,844,836) / 15,106 [0] (8) / 8,973,130 [1,396,440] (1,180) | 13,745,908 / 5,233,052; 15,132,936 / 3,846,024 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9fcf48db944a7d34685fb9909d1ce3d8fe000dd8c191e21f7215acdf47efb2c5 VersionId THsuklJlRP_rK.Kw2kTSVlyfIZfzQ9Ez |
| `opposed:rook+ninja` | King+Rook vs King+Ninja | opposed | `krookkninja.uftb` | **CERTIFIED** | 37,957,920 | concrete | 3,833,886 [3,522,988] (4,951,436) / 275,140 [172,410] / 9,918,498 [335,076] | 4,742,502 [3,585,318] (5,259,100) / 131,520 [118,418] / 8,845,838 [215,140] | 14,027,524 / 4,951,436; 13,719,860 / 5,259,100 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:f30ba3463e16635204ce54e7db335b8bdb1ccee1499bfdd9abf4aa8fe0c679ce VersionId 7MzKVWqcsEkCUDWd26jho9Be1Rj3MntM |
| `opposed:rook+turtle` | King+Rook vs King+Turtle | opposed | `krookkturtle.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,003,804 [3,771,810] (4,951,436) / 4 [0] / 23,716 [23,688] | 4 [4] (2,397,164) / 14,534,220 [447,420] / 2,047,572 [2,047,322] | 14,027,524 / 4,951,436; 16,581,796 / 2,397,164 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:9525539e86bf1213fe7990cefc414f3bcb52c01068337014f42e471b1b2537f1 VersionId hS5oFN4leeuXh5Rl7TFF35FKpPosFqhQ |
| `opposed:rook+ghost` | King+Rook vs King+Ghost | opposed | `krookkghost.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 4,642,892 [3,523,802] (11,103,138) / 4,806 [0] (4,494) / 22,114,242 [2,819,420] (88,348) | 6,466,580 [6,450,708] (4,702,852) / 439,876 [258,912] / 26,348,612 [571,864] | 26,761,940 / 11,195,980; 33,255,068 / 4,702,852 | S3 information archive sha256:852a08e3af1ee3bd85855484b91ae77855b5f7c18769cae052a91bcb88a386b7 VersionId eUKHKHvIO8vF418a1zAY27tWBpXen6_r; preservation certificate sha256:17ab53663f0615d4196e11d8e01898c9e21bf0768981be2bd90c40a723edaf7b VersionId aDp4y3ZLtQb5harNEr41.JIItGCF2shM; result certificate sha256:b4f3d67ec0e2eb5df0ba04a244af99c5088bc9cfa3458caff599789af65a44cb VersionId BzKzIfXrRkmGE2lDPkqU5xTEeUqGcFWU; arbitrary sidecar sha256:87e76d1aee3238605c4e64e2808b78ea6ef82550cec4ccb0ebd864de4ccc837d; information trivial v2 sha256:d7b3d2e5df9fe6ed10a6a7f1a2cd9f98d133fd6c8049810587945a300addde31 VersionId o8iDBE.wnQ7uErABfAQIZuvLYUU8RJax |
| `opposed:rook+mage` | King+Rook vs King+Mage | opposed | `krookkmage.uftb` | **CERTIFIED** | 37,957,920 | concrete | 14,027,524 [3,795,146] (4,951,436) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,952,876 [4,121,264] / 1,416,476 [1,416,476] | 14,027,524 / 4,951,436; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:4d16e9d2fbe4642c728341d9296db24469c6b09cab0ea23ccce1aa72b020b37b VersionId 2L_xja878HkdhUg1BpVlM0Quqkvnxgmi |
| `opposed:rook+penguin` | King+Rook vs King+Penguin | opposed | `krookkpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 6,137,850 [4,054,806] (5,804,210) / 154,864 [0] / 12,097,692 [442,306] (127,637,064) | 406,384 [13,712] (1,901,252) / 958,538 [444,468] (468) / 20,822,658 [1,748,018] (127,742,380) | 18,390,406 / 133,441,274; 22,187,580 / 129,644,100 | S3 table sha256:a577722ae194d3bcd2f46aaf3f8972f84dc6826e44a973b7cf5ac08ef78e0cc3 VersionId nuHhAnUc8IEsgkMunIYwFCGje1xg1XeC; certificate sha256:8edb3fc9a4b5cfbd6b0af10a0da1db822e6454e8b7d0406abe8af9231bdadef8 VersionId 73wS9UAd4mnkuiwlJXAvvCBQWB1WXvDt; reachability v3 sha256:1967323976175a0641b69e83134b252c2f722254b42a3092df8b43fab169d58d VersionId 4ld6_WjrhtiAtOKuZvqaeFQOE04RAmzz; result sha256:5b5187b6b54267abec83bd56a01adae9315e94e8db71780f39aa461cbe91c3ef |
| `opposed:rook+parasite` | King+Rook vs King+Parasite | opposed | `krookkparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 100,882 [14,972] (4,951,436) / 13,479,612 [2,402,244] / 447,030 [81,460] | 15,715,312 [2,836,440] (3,093,244) / 17,690 [0] / 152,714 [3,442] | 14,027,524 / 4,951,436; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:240068444612889ff1c0c12f5b9958ad619b6b137e7fbb412d4041dc11e6f0d4 VersionId XNL3McmS2UHhDu25IoRl6Kf_.r3IzBFE |
| `opposed:rook+devil` | King+Rook vs King+Devil | opposed | `krookkdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:rook+sludge` | King+Rook vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:rook+sniper` | King+Rook vs King+Sniper | opposed | `krookksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 27,994,313 [7,580,471] (47,800,057) / 1,345 [0] / 59,390 [52,712] (60,735) | 15,553 [8,411] (7,374,850) / 29,996,075 [1,044,702] (31,743,450) / 3,794,634 [3,615,394] (2,991,278) | 28,055,048 / 47,860,792; 33,806,262 / 42,109,578 | S3 table sha256:2ef29fdfaf870beb9dad46d1ff852d4c8bd5ee8265e0c3dc9b3f77fa1467a4e6 VersionId DzHBxFq7OjfaheK0M_JyOYazH4dubozN; certificate sha256:8980c0f36ed36d58a0485d7b08bdabf9c443401f818a2fda3b0e7e871a60aff5 VersionId EAjii8lP6RCRQYEJoCd_2wUZabXHcUhb; reachability v3 sha256:f8e75ecdc5a412b535028c119edbced3df4fcbcbc8c55b0795209d816529e3f8 VersionId _8tN_LFT8.SkhiIR0ARy25mqmZSw78xo; result sha256:82f0700a00398b917923388ddfd3bbce6fb650ff116cc16011159244c3457b47 |
| `opposed:rook+prince` | King+Rook vs King+Prince | opposed | `krookkprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 3,945,892 [3,522,596] (4,951,436) / 9,120,690 [1,596] / 960,942 [272,550] | 15,595,226 [2,636,544] (3,093,244) / 181,862 [155,926] / 108,628 [17,584] | 14,027,524 / 4,951,436; 15,885,716 / 3,093,244 | S3 table sha256:209586e851d9ddf23136fe17546be98880029d84d9f0bbd3bcdb0ad894b9d628 VersionId 24v1plNYh9DipQT_CyodQKD4iZprKi1f; certificate sha256:ba9a0990660d5ec099dbf677036df22adbe3e8cfc02a3841c60e54d7e0810418 VersionId zSqi4oO8wwotqAIkLyVoKl263I5beTAL; reachability v3 sha256:8d4fe5be0ed8e23582704613b1163fd841126291b27d6286292c11ecc6822aba VersionId 8DKPfrjQBwENcfxrYhzbRUCQamb4Evz7 result sha256:2e4650da37f8101cfae58edcbd08aa8c9f5e69b1ef0590112844bef9ff8a8708 |
| `opposed:rook+checker` | King+Rook vs King+Checker | opposed | `krookkchecker.uftb` | **CERTIFIED** | 151,831,680 | concrete | 26,642,046 [7,230,317] (11,298,154) / 0 [0] (3,981,336) / 17,720 [17,720] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,319 [1,177,847] (11,128,832) / 4,048,113 [4,047,681] (26,982,648) | 26,659,766 / 49,256,074; 33,808,120 / 42,107,720 | S3 table sha256:4738efd5277a35f0f89b30b484730c4b015a844b575a31f857183fdbbd6661fd VersionId bJOo5D3V9cTu_yQwd5brv1wZBKbf5ypY; certificate sha256:fe8f6c341848b6ed0b23fa38cd35868de32214ad68448b416cbdaaf8ab4a4de5 VersionId maD7UgUB9LLJsvAV8DDpoXwptFlDLy2c; reachability v3 sha256:06f99284495a2664ed3b44a9b8bc8eea5113d88205e68bf8c623b8233599f364 VersionId afvHG2b_GMNJBwZEmW6UwERA3bE8yBRb; result sha256:9d11eac17ac6c84271eea4ee1559712cc88e362db9913ca8e8e96a29581c87d2 |
| `opposed:rook+giant` | King+Rook vs King+Giant | opposed | `krookkgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,775,420 [4,480,382] (3,443,644) / 13,840 [4] / 53,796 [53,044] (5,692,260) | 30,694 [11,182] (3,051,612) / 7,895,702 [362,660] / 2,308,692 [2,304,166] (5,692,260) | 9,843,056 / 9,135,904; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:b0371714125ceea34fe5d59e7f15303f08f07cb7d7ac5128c99ad4bc58f85dce VersionId l_ZewBOpEf3lgzuqfWWStRq8Bu5mHhmN; result sha256:6e099716aa2a1115445af30f913b8e502f7e66ec8b63046da19b1f9f90c242fb |
| `opposed:rook+copycat` | King+Rook vs King+Copycat | opposed | `kcopycatkrook.uftb` | **CERTIFIED** | 75,915,840 | concrete | 5,943,888 [5,905,056] (8,221,984) / 8,664,984 [725,256] / 13,685,624 [546,784] (1,441,440) | 19,628,232 [10,700,040] (9,396,328) / 22,976 [3,712] / 7,468,944 [562,056] (1,441,440) | 28,294,496 / 9,663,424; 27,120,152 / 10,837,768 | S3 sha256:fd7a72056cfe167b2d53cf04cf269bd64ee3f9166c01327cb17b06b72690be2a VersionId Hvnps5dshDBsT_4Ti7CeJPqJGSR7SrV_; certificate sha256:b82ed9ad43b0c3d227520d5c4da2423578ab5b81cd3fa7340e4d999738c90cba VersionId wvY0MEFLM2w_lsqGhIdV2lxtyBuf7zo6; output sha256:f3a0e57e40d2cdf2a7a484986c724e63045c384f68a896fcfaaa9fa155113267; reachability v3 sha256:f0ac95dc42fb9f505bd8c9f7e803253d7f594d5bff96a99a0c83c220c4b19f38 VersionId WjNOpk93KfCxqFZvrHo2SP6glB.Wjar2 |
| `opposed:rook+angel` | King+Rook vs King+Angel | opposed | `krookkangel.uftb` | **CERTIFIED** | 75,915,840 | concrete | 27,658,594 [11,704,934] (4,951,436) / 11,110 [56] / 5,336,780 [717,328] | 28,312 [1,908] (3,219,216) / 21,970,406 [512,476] / 12,739,986 [2,964,112] | 33,006,484 / 4,951,436; 34,738,704 / 3,219,216 | S3 table sha256:491c82b0790c3442691f83a33d6afd1ef180736ee8d15a14e74dcfcd03986233 VersionId KO28PtpRrv05RYs_cWzCkA9LgHSSoOus; certificate sha256:1a86d6b335e67453181f52d149872faefb54be6ef7a3cf370f1d2b9f3acae544 VersionId WcDbeqm6Syweie3D0YQgV0YuaGfeCUrx; reachability v3 sha256:c801f79bf39876853dc2bdbad5bdbbd0367710eef8ffc320dd1aaeddd56487a4 VersionId fsBz.cR_G07Q7ANn_1soy0z9FI1S2MX6 |
| `opposed:rook+fisherman` | King+Rook vs King+Fisherman | opposed | `krookkfisherman.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,422,744 [3,529,374] (4,951,436) / 0 [0] / 9,604,780 [265,772] | 0 [0] (1,609,608) / 461,572 [400,922] / 16,907,780 [1,687,846] | 14,027,524 / 4,951,436; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:458a6c298fc2a71375b05c4b85b7754008ad19d25c486333963713245f2a8d9d VersionId 2ShPB71PE9ZNPVjl2ipImXlyI7gzvh7L |
| `opposed:rook+dragon` | King+Rook vs King+Dragon | opposed | `krookkdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,376,160 [3,523,156] (4,951,436) / 437,638 [381,108] / 9,213,726 [363,058] | 5,782,264 [3,460,060] (4,893,140) / 324,630 [313,032] / 7,978,926 [350,324] | 14,027,524 / 4,951,436; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:32662cdfbba319c694921f605ced1dfb2b26b7336894082f2db3be4abc292f0a VersionId iMUNX5uUd03Nv.2A6kWFWzC2GB_ZIhTT |
| `opposed:bishop+bishop` | King+Bishop vs King+Bishop | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:bishop+berserker` | King+Bishop vs King+Berserker | opposed | `kbishopkberserker.uftb` | **CERTIFIED** | 379,579,200 | concrete | 0 [0] (36,937,880) / 110,296,472 [4,589,036] / 42,555,248 [30,069,936] | 43,049,192 [21,477,988] (133,257,124) / 0 [0] / 13,483,284 [266,830] | 152,851,720 / 36,937,880; 56,532,476 / 133,257,124 | S3 table sha256:ca6d1034b539770a380fd410f28e8af44aecc35c34dc29e859f977e71ff0bed7 VersionId yCClnIFKpUMsV2mN4rWpj_d2D0E0upY1; certificate sha256:db1dcdf1bbf492139b2997c311afaff8e70935541d6347e480a3355a3883d116 VersionId ApqEgQu0nhBuMdiR42ifwBctDTMJno3.; reachability v3 sha256:20e008b172cb97396312ea2ae9dd9f9f44968f75d8a24bed2da8ecbb969e44e6 VersionId _3__uQfuPjhGKmWOdDzUDwAyo46Fn7_b; result sha256:13f9c6b0b2cc8c78e26b49e0de6d8ccbd044e570178023c047f9daf6151f9e6b |
| `opposed:bishop+bomb` | King+Bishop vs King+Bomb | opposed | `kbishopkbomb.uftb` | **CERTIFIED** | 37,957,920 | concrete | 116 [0] (3,850,520) / 13,139,856 [246,048] (59,240) / 1,928,028 [1,599,610] (1,200) | 14,983,656 [2,853,360] (3,845,632) / 24 [0] (16) / 149,256 [148,476] (376) | 15,068,000 / 3,910,960; 15,132,936 / 3,846,024 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1bd8b06981005fc93e3d35e7ab59ef17263d31bf51d679d0fd33211a094c8668 VersionId USePveidPAacN8_W4VS.JhfGQQ2XOCb6 |
| `opposed:bishop+ninja` | King+Bishop vs King+Ninja | opposed | `kbishopkninja.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (3,693,788) / 12,195,462 [302,314] / 3,089,710 [3,007,110] | 13,679,960 [3,741,216] (5,259,100) / 0 [0] / 39,900 [39,900] | 15,285,172 / 3,693,788; 13,719,860 / 5,259,100 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:97885763f597790933b0cdd783a03d4c25fb7065b9d59d8e9353b610be4b329d VersionId zDOqwDKXtor48I1T54vTh4LZoha4o9Bq |
| `opposed:bishop+turtle` | King+Bishop vs King+Turtle | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:bishop+ghost` | King+Bishop vs King+Ghost | opposed | `kbishopkghost.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 0 [0] (7,430,290) / 14,200 [0] (7,212) / 29,210,746 [4,722,622] (1,295,472) | 6,497,540 [6,452,276] (4,702,852) / 0 [0] / 26,757,528 [324,620] | 29,224,946 / 8,732,974; 33,255,068 / 4,702,852 | Exact exhaustive reciprocal solve with Bellman, rank, dual-force, structural, singleton, grouping, conservation, and realization residuals zero. UFIW sha256:420d4e6f906aad8cb38fab1a3d175c6a88eac3409255b0771d834669f050620d; arbitrary UFGD sha256:1145a77cd1eabcb35c9ea5429228488644e88846846548a391a2b331b407e27a. Authenticated 38-artifact S3 archive sha256:f30f3d6647e5616c50d7f7fa5de8975bf3fd3c30a4d46de933a0a27c979b8020 VersionId `Y6du5Uy3GO5J4SnJDftq0_h1XO10Xg72`; certificate sha256:8c9c54c686709f778f607a1b78fa2bbdc539a9872492a8c65a0a842c960681d3 VersionId `6DPmX2PEHRTha8w0f0ZEU8ZKxtsDlCYQ`; local archive, S3 HEAD/download, certificate download, and fresh restore residuals zero.; information trivial v1 sha256:6c66a1a89eb7711a2e95bad141dba0ca2092c815700f91ada10271c78de86ba9 VersionId Fd8spwpjBLx7B8bk2r9BqNz.Oal0gB_3 |
| `opposed:bishop+mage` | King+Bishop vs King+Mage | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:bishop+penguin` | King+Bishop vs King+Penguin | opposed | `kbishopkpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 351,036 [28,528] (4,334,684) / 1,795,034 [0] / 17,713,862 [3,535,658] (127,637,064) | 6,177,308 [388,768] (1,972,862) / 150,056 [2,288] / 15,860,216 [1,446,406] (127,671,238) | 19,859,932 / 131,971,748; 22,187,580 / 129,644,100 | S3 archive sha256:ac95f005b0d7bbd094986ff54e0d43c013308ec056c8352c5d0a3a69ecee21dd VersionId hZEwbO4sp20YnYFqUAOaPH4hY.hmJ9gt; result sha256:2e272def31054bfc62cda5f81fce85cdabc37597dd0d65ff737c7861381c8fe3; certificate sha256:180e24aca77b28b28a28ccc2cea1a2f85d928ede9c0ebc247924b61b74a91b94 VersionId keO6hngPUNcCR_XNsvm0Te2RULdnDAaq; reachability v3 sha256:9aae04484972702d7f915667b049daef753f2ae8f1af85a2415a82e79720a641 VersionId SvgEiqIjBzM4jH9kVFaBfoENAqQhwEU_ |
| `opposed:bishop+parasite` | King+Bishop vs King+Parasite | opposed | `kbishopkparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (3,693,788) / 13,243,670 [121,738] / 2,041,502 [1,714,306] | 15,738,520 [2,492,632] (3,093,244) / 0 [0] / 147,196 [144,172] | 15,285,172 / 3,693,788; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8879f0bcacf01406ac42653327fc7eb7f3c7730c3f5ce6ffc52560e8128f736f VersionId FkCSe1UOruD5ODcqP4Qd9rPVpjGldgix |
| `opposed:bishop+devil` | King+Bishop vs King+Devil | opposed | `kbishopkdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:bishop+sludge` | King+Bishop vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:bishop+sniper` | King+Bishop vs King+Sniper | opposed | `kbishopksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 365 [0] (14,775,517) / 1,516 [0] / 30,568,463 [6,089,878] (30,569,979) | 16,501 [8,428] (7,374,850) / 80 [0] (112) / 33,789,681 [4,072,198] (34,734,616) | 30,570,344 / 45,345,496; 33,806,262 / 42,109,578 | S3 table sha256:bd9cf7a0d182b2671586ba84c4beb639f49524282c75a5e66bc095bc0e9cea50 VersionId 9BwyXwXHHOPxveFmNeueiQo0MCLzaZJi; certificate sha256:ba55957531d37bbddae6633d18a4570a5d66932c3a9818ffb0b5d1115038201f VersionId hqakyDYBQtFZAZWn85GeZkaHcU322uAW; reachability v3 sha256:ce095bb6baadcc54c605c426bdbc98a9421a82d8be1a38c905d0fdbe373e93ce VersionId db0BKRl5jIZwwAA3urKcdyHU4UkIzc7q; result sha256:8f5c296efc9164db658870de11cc4ad61222e82cc2315de7f378b81aed34185d |
| `opposed:bishop+prince` | King+Bishop vs King+Prince | opposed | `kbishopkprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 0 [0] (3,693,788) / 12,149,366 [3,096] / 3,135,806 [3,006,940] | 15,826,140 [2,574,822] (3,093,244) / 0 [0] / 59,576 [49,388] | 15,285,172 / 3,693,788; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:4f5bc121d7fbbbad69cef64d22a635d6a61fbca011ac17f226eac17963c66064 VersionId jfmzKaEWMdu8rrZHLKbJjZ1Q6rQBiqv. |
| `opposed:bishop+checker` | King+Bishop vs King+Checker | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:bishop+giant` | King+Bishop vs King+Giant | opposed | `kbishopkgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (2,519,718) / 15,926 [108] / 10,751,056 [5,080,962] (5,692,260) | 32,848 [11,384] (3,051,612) / 0 [0] / 10,202,240 [2,567,328] (5,692,260) | 10,766,982 / 8,211,978; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:4778f0358975ee51c73b4ac2a6d62117966b323d8ac5a5aef8b53e755d9c18d3 VersionId VpUWt1QasR6rNqAUfeA5FdmoKUPDJW6y; result sha256:996ff93dba83521f5d211c03639b30c68caeb96a662a181e78f9b7e30127d2ad |
| `opposed:bishop+copycat` | King+Bishop vs King+Copycat | opposed | `kcopycatkbishop.uftb` | **CERTIFIED** | 75,915,840 | concrete | 5,967,160 [5,909,584] (8,221,984) / 0 [0] / 22,327,336 [727,224] (1,441,440) | 0 [0] (7,021,104) / 36,168 [11,128] / 29,459,208 [10,718,984] (1,441,440) | 28,294,496 / 9,663,424; 29,495,376 / 8,462,544 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:2925413c1b8df234e1b632c5d5242f4ee5666f58c5967414b9085c1a011b7ffe VersionId .ESekT1cRMho6Q.ypi_Ulq4TWMBPCcGy |
| `opposed:bishop+angel` | King+Bishop vs King+Angel | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:bishop+fisherman` | King+Bishop vs King+Fisherman | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:bishop+dragon` | King+Bishop vs King+Dragon | opposed | `kbishopkdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 8 [0] (3,693,788) / 12,191,210 [273,664] / 3,093,954 [3,007,140] | 13,981,252 [3,608,198] (4,893,140) / 4 [0] / 104,564 [103,694] | 15,285,172 / 3,693,788; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:c2a86bd4d75160b1fc619021593457f0a8047047999fd7d81b5d0a6b9f09a908 VersionId j.jTu6X0X1GIXUoGzhf_0CA43aVc9Qqv |
| `opposed:berserker+berserker` | King+Berserker vs King+Berserker | opposed | `kberserkerkberserker.uftb` | **CERTIFIED** | 3,795,792,000 | concrete | 262,700,200 [211,530,242] (1,332,571,240) / 271,396,830 [9,450,088] / 31,227,730 [3,996,402] | 262,700,200 [211,530,242] (1,332,571,240) / 271,396,830 [9,450,088] / 31,227,730 [3,996,402] | 565,324,760 / 1,332,571,240; 565,324,760 / 1,332,571,240 | S3 archive sha256:177c0e68a38e733b58998b576a1ef6040dcd664e25b3b94d9738b7c982929d15 VersionId vIaZ9yyi6GlqgldU_NTr0UcSma09abog; certificate sha256:d7f4c8f0e15ed15c496aaa98e16774e71f89d7a5771e1829d8337d46fb423fe1 VersionId dxsjNUCyF73SC12Tk5wj45MhnpVzzFH9; output sha256:d7b3fd554638042c855374f3e38f898226f9884d06e6c6d4167fbd182ad2209c; output-reachability v3 sha256:f5a21caceab1e42e591f33be9087c247f1286c6dad6a577c18497657dd2f3e20 VersionId CqPMPqppXfdymsZbU6HC.Vz8d8VlgpBq corrected 2026-08-20 after a concrete distant power-0 witness proved that the earlier ledger entry had reversed the native sidecar's excluded and admitted buckets. The exact table itself remained Bellman-correct. |
| `opposed:berserker+bomb` | King+Berserker vs King+Bomb | opposed | `kberserkerkbomb.uftb` | **CERTIFIED** | 379,579,200 | concrete | 29,873,516 [13,308,408] (133,738,716) / 15,397,290 [491,502] (209,208) / 10,570,870 [3,091,556] | 33,274,166 [14,604,986] (38,428,588) / 96,367,532 [4,284,004] (29,344) / 21,687,662 [14,481,812] (2,308) | 55,841,676 / 133,947,924; 151,329,360 / 38,460,240 | S3 table sha256:7ab66f110ab737afb73f8516dd9d321cfac71b002bcf9e1985bb6f809e912de0 VersionId nLWuIst1EalItDtUSqUvWv.SbkJPv5Pq; certificate sha256:1e952eb829fefd1e3b53fa19407d44e4964bffb1281123d1d9ea6611bc883d4a VersionId CfGYuI2VbcMDmkdNGUa7zDP0MpOW12YP; reachability v3 sha256:602cc7a2eaaa1727b1bab3ea062f0f1ae52a3519694a2c7c9e185cfa7f979952 VersionId iCOWdfGBQrNXjT1pC426wKrOlDEK2nss |
| `opposed:berserker+ninja` | King+Berserker vs King+Ninja | opposed | `kberserkerkninja.uftb` | **CERTIFIED** | 379,579,200 | concrete | 37,634,442 [21,426,156] (133,257,124) / 6,593,328 [259,288] / 12,304,706 [208,544] | 43,467,936 [35,800,204] (52,591,000) / 80,669,688 [3,051,264] / 13,060,976 [1,700,148] | 56,532,476 / 133,257,124; 137,198,600 / 52,591,000 | S3 archive sha256:7d0bd43fc1fc2abd30514b84932388de40d6bba34bc6ab37309606bd419fe9ec VersionId CTTIeVnWFCVB2Ttq9nufgWmzkgiskjtj; contained table sha256:b436e4a2f00c76cda8854c2262f79c3ed0daf362436e5a205770c5d1f5cb7796; certificate sha256:dfea8c060f8fb5bd95e7b4e4836e4cfe36bc26b2dd6a54d04693e99a12d5999b VersionId 808YPwo15yJZMihsnIEoIQ74BlvC7QkA; reachability v3 sha256:267900cb005c12fdf6760acf65e387390473dc9a7e0a17ccb54ffa896a45d0bf VersionId xnvweIE_FIw8q8y2X1BbxC1HF1V8DT7A power-slice diagnostic sha256:faac32e8ec568feb386c027ba38da4df74c9bb0741e84b0071bfba388fbf060f VersionId .nr3tzspic8TXWi5SKV31uiY0rf2eDuC |
| `opposed:berserker+turtle` | King+Berserker vs King+Turtle | opposed | `kberserkerkturtle.uftb` | **CERTIFIED** | 379,579,200 | concrete | 49,547,276 [21,483,264] (133,257,124) / 0 [0] / 6,985,200 [59,176] | 0 [0] (23,971,640) / 136,166,764 [5,565,484] / 29,651,196 [20,474,076] | 56,532,476 / 133,257,124; 165,817,960 / 23,971,640 | S3 `bc27862a…` / `ewEfzR2SHgsWtETshq9OVYLiwhdNzvHh`; output sha256:7b38de854d35a8d8b94bb2ddae3edf69202ce5cbd8d3a58f329b3d12b5f977c1; reachability v3 sha256:6149b6e667c96803d4cab75fcbcfd15618684f72d39ef9145156ee1d14d23768 VersionId RgKNEkl7cDHvCvrZHT_LGl21JlITNn30 |
| `opposed:berserker+ghost` | King+Berserker vs King+Ghost | opposed | `kberserkerkghost.uftb` | **PLANNED** | 759,158,400 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. Authenticated v6 concrete source sha256:d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55 (S3 VersionId GL3OL9ZoYDb4NxPqHlqPllMLdJQ9Ryue) is valid, not mislabeled: direct retained-file inspection reads version 6, primary piece id 7 Berserker, 759,158,400 states, 20 combined substates, secondary piece id 11 Ghost, opposing orientation 1, and the WDL plane begins at byte 56. The older failure was a stale fixed-v5-offset reader defect, not a Bishop-source defect. Fresh v5 retained a complete 4,929,600-geometry transition graph under ready-marker sha256:58cf814a568ff1c92f16835c472e4d5dc3f1a255e460f52aac30797e25933e7d. An exact v12 diagnostic proved unresolved lower-material force placeholders (first witness geometry 1,580: stored 0, expected observer force 2). V21 exhaustively restored all 4,929,600 lower-edge geometries with 30 workers and passed its zero-residual 131,488,840-edge probe. Its strict handoff authenticated the unchanged original marker and launched v22; the checked-in supervisor now tracks that rebind separately as RUNNING with exact source, binary, concrete-input, lower-table, and marker hashes. The downstream v17 certifying solve remains gated on v22's authenticated rebind manifest. No result is published until Bellman, exhaustive singleton, preservation, and restore authentication pass. |
| `opposed:berserker+mage` | King+Berserker vs King+Mage | opposed | `kberserkerkmage.uftb` | **CERTIFIED** | 379,579,200 | concrete | 56,532,476 [21,542,440] (133,257,124) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 159,518,996 [35,774,112] / 14,174,524 [14,174,524] | 56,532,476 / 133,257,124; 173,693,520 / 16,096,080 | S3 table sha256:3fe501362b15c1840240982a155a415f046e3fa47749678640c8b2fff23993e3 VersionId PAJ_E9hsY2pTX0Oxm62uhqkwvvc_kuVz; certificate sha256:df14670b3fac29576c6be6c1e042bdef408e79236bc2df96c0b15919ccdcea83 VersionId 62ow2uCxg_jcO18N4c4J508XbNiTSHiB; reachability v3 sha256:56c662af2864d8d6f357986ef67027a9b7c4666c597f9bdb9646b3cb6722a5b5 VersionId MBk3VgnbSa2zpN1Smiz0v4ksbx54Prie |
| `opposed:berserker+penguin` | King+Berserker vs King+Penguin | opposed | `kberserkerkpenguin.uftb` | **CERTIFIED** | 3,036,633,600 | concrete | 47,115,022 [24,693,340] (157,864,088) / 2,037,592 [0] / 34,929,458 [1,472,096] (1,276,370,640) | 5,196,140 [55,262] (19,008,796) / 124,701,220 [4,388,134] (2,112) / 91,978,440 [16,491,710] (1,277,430,092) | 84,082,072 / 1,434,234,728; 221,875,800 / 1,296,441,000 | S3 table sha256:78675fdb4ce0007b98ddf7f87dd71823b8618fc9846b0c3552f56a7d3e22f665 VersionId 8_jsHPDzlqX9nO1OoQBANO3r8JRiaEFf; certificate sha256:e0f6f929a8e0ebd5211a0aa400355f23e4f379291ebf8badf306cbe1f9b1ab00 VersionId OOIBNdQIHhNqG4MpL7X.uU4PSaguiaTD; reachability sha256:c8a915bfe5a28cfe73d91fb06d199e3874de77c38b52d44429c07deef7e99995 VersionId LtniinPrAjBoSCMgQmah54ON4u9RPxZF |
| `opposed:berserker+parasite` | King+Berserker vs King+Parasite | opposed | `kberserkerkparasite.uftb` | **CERTIFIED** | 379,579,200 | concrete | 34,618,806 [4,516,896] (133,257,124) / 20,085,416 [1,380,746] / 1,828,254 [65,214] | 44,625,046 [24,996,378] (30,932,440) / 111,319,144 [939,932] / 2,912,970 [439,418] | 56,532,476 / 133,257,124; 158,857,160 / 30,932,440 | S3 table sha256:2a78ca7e06432a55f1f76eddb0d7747a2c15099d6ba6fd42640feb606ed8b6b9 VersionId lHLgYW8VDAF5enXgBE7rWGQuq0MJMo7A; certificate sha256:c4915e74d5d7558280eebe635f3bd5b3450960da40194b3e1a0a901da8bf822f VersionId LhEAfbiRcswtbCIxGUCSpiPU3DBLzjgk; reachability v3 sha256:5f91fec66168392b6650c1cf08787a9325df8c27a449d1b0705a05335f239257 VersionId r5tSrmPSz_88qwkjG_Mmo.5eujc9oR2r |
| `opposed:berserker+devil` | King+Berserker vs King+Devil | opposed | `kberserkerkdevil.uftb` | **PLANNED** | 455,495,040 | concrete | — | — | — | — |
| `opposed:berserker+sludge` | King+Berserker vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:berserker+sniper` | King+Berserker vs King+Sniper | opposed | `kberserkerksniper.uftb` | **CERTIFIED** | 1,518,316,800 | concrete | 112,834,132 [43,082,802] (645,862,628) / 8,413 [0] (479) / 222,407 [119,288] (230,341) | 126,266 [80,882] (73,752,811) / 301,329,375 [12,310,654] (318,640,610) / 36,606,979 [36,051,389] (28,702,359) | 113,064,952 / 646,093,448; 338,062,620 / 421,095,780 | Native resume completed and Bellman-verified on i098. Output sha256:4d514728bb05af84079c2b35eeaf0fdba7c673573a414db191f4f95df876651f; content archive sha256:0bf91cecaa27507bba56c5bc845eeefd7c29f161f8913e63170b497856f79ec0 (S3 VersionId `hQVvUvV1fU22o8DBz5NpL8sp_eqdwV7X`); certificate sha256:e6ca6fcc4b52af96a259e3e9669dc936d4b1c77a2b9fd960abbea67ff936a5c9 (VersionId `0ctc8DBF7CU8_bpLOJcvSJ79KMiBn71A`); superseded unbound causal reachability v2 sha256:d76dc3ac6cea34657673de862b577a72e9396cfb612a555903442744e8328efa (VersionId `vN3635hUEZCL.iOHDz0X244_OsKKztWX`); fresh exact-output-bound reachability sha256:590ebdc7051d453605f5153024986c02718f5af22973ab99fccd22373c04167c (VersionId `_hMfz2ZVWApSl3FSYoqfFGIZeDmgHr4U`).; reachability v3 sha256:3eb5719205688ab9c980db29ef18f6f3b4773e5d8aef153674bc9eb39fe2153c VersionId UBjnx2yIBAnShXQCU0G876fpUdOn41VW |
| `opposed:berserker+prince` | King+Berserker vs King+Prince | opposed | `kberserkerkprince.uftb` | **CERTIFIED** | 759,158,400 | concrete | 34,642,930 [21,333,644] (133,257,124) / 14,340,720 [0] / 7,548,826 [208,796] | 76,168,028 [48,180,252] (30,932,440) / 74,070,652 [2,756,524] / 8,618,480 [664,256] | 56,532,476 / 133,257,124; 158,857,160 / 30,932,440 | S3 `e4bbd8bb…` / `vp9jH5pdw2s8jBiiSTeI5WqtSfvHhytf`; output sha256:5cffc0d8c782f6d28801d9cfc9cd3e8da77ac2710720ddc0e37c7e7885a476a2; reachability v3 sha256:f57882ca1a6aaff24c248089c586265d06542ed55d34598836d5738593234928 VersionId Jn_yaq2Hib5kmaXK4El.P48IHa6ABE4j |
| `opposed:berserker+checker` | King+Berserker vs King+Checker | opposed | `kberserkerkchecker.uftb` | **CERTIFIED** | 1,518,316,800 | concrete | 107,458,883 [41,557,797] (272,120,317) / 0 [0] (39,813,360) / 0 [0] (339,765,840) | 8,426,880 [8,426,880] (39,962,400) / 289,178,657 [11,779,988] (271,305,622) / 40,475,663 [40,475,663] (109,809,178) | 107,458,883 / 651,699,517; 338,081,200 / 421,077,200 | S3 table sha256:ebdd707c7f1d1bd999c29da99f4b9888586361f38707e4be7f11832456660868 VersionId lsmkwgnUZO5mVSOcJ_8aOiIyAWm61tnV; certificate sha256:6bed5fb85fcd2fc92299e517238726e8740c47558d96ea14e4123693bab6ce9f VersionId et3UvH2bzJUofMqJ4W6OJ9F.iFYtYgRp; reachability v3 sha256:65e7755fba31175b9b92e177f7fe1191c1334f4c11da323d3c0bb98e061e21e9 VersionId .WEckRm.t0B9V0d6bf8ZYpzFM46j2.3g failed v2/v4 memory-gate evidence archive sha256:086df8714e681e60f30b8c14f60d1f0c34677d7e8b12d3f771f21d4a763bb42c VersionId yq.BRN4_7WfdqUviXwFxDULJuo4p5yPj was version-pinned-restored before disposable scratch reclamation. On 2026-08-20 the successful v5 table, certificate, and reachability objects were independently fetched by those exact VersionIds, the 1,897,896,056-byte table restored with sha256:6919e29cda2593b591baa7d9ae0332990e63c4af74a14aa03cb8eeeae84c1311, and every archive-manifest member rehashed successfully; only then was the inactive, handle-free 130,977,916,151-byte success scratch directory removed. Outputs, logs, certificates, sources, and all failed-run evidence remain retained; `/mnt/ultimatefish-penguin` free space increased from 74,587,963,392 to 205,443,473,408 bytes. |
| `opposed:berserker+giant` | King+Berserker vs King+Giant | opposed | `kberserkerkgiant.uftb` | **CERTIFIED** | 379,579,200 | concrete | 39,561,654 [19,685,876] (93,165,442) / 56,756 [78] / 83,148 [59,026] (56,922,600) | 167,220 [86,642] (30,516,120) / 79,001,406 [3,410,746] / 23,182,254 [23,066,842] (56,922,600) | 39,701,558 / 150,088,042; 102,350,880 / 87,438,720 | S3 table sha256:de9d945fc7a048c1d9ffe1284d88e9026264893b33c28327453ec2a8cd2c8262 VersionId h4HqEbPggva7N4qCMe2fb6VcP5ap6w_x; certificate sha256:74c3224d7d964f9e14b43f7a78af91fdb2279df9a24b5b9abede828b4c2575ff VersionId D.duvgEt38w2gwMWkCHRVCSMZgoVwZNB; reachability v3 sha256:45c7876a4d5f251b83311fc5de66b55041c88b2faf8369ff4cbbbcaa9750d3ac VersionId gMlCfdmWjvFd5oCyIRlCZaQB8KWUWPMH; result sha256:bd0871671df1098f7a5a2028c08efa284fb8f8a4f7454f32cdbc67a401d9df3b |
| `opposed:berserker+copycat` | King+Berserker vs King+Copycat | opposed | `kcopycatkberserker.uftb` | **CERTIFIED** | 759,158,400 | concrete | 61,771,024 [59,049,360] (82,219,840) / 198,470,656 [9,322,008] / 22,703,280 [3,500,968] (14,414,400) | 87,371,472 [57,076,040] (256,401,208) / 1,490,328 [0] / 19,901,792 [177,328] (14,414,400) | 282,944,960 / 96,634,240; 108,763,592 / 270,815,608 | S3 table sha256:f9017fd7b869a40946cd32dac298cdbebf911e82563900b2682cdffb626c6d05 VersionId wQFKp6LuKIiZsCh1AzwTAcXzjrq7VMz4; certificate sha256:83561ac2c81514fe66c702ca81b56320b92acf5688aba52671e57df3d6f33d84 VersionId i9HgLKGwe3TT5fNqu_brUqIScLTxasj6; reachability v3 sha256:a652e11bdeb64b978f57fbcfcba14af4ae4be1da0b66a276e90863fbdbce499e VersionId tLNW2FSIuA6KzpZWYL4PchQ6OP_Mk45Y; result sha256:3a3850b5ba940f648db042c67de241e497ceb0ee68a7b162861af2fde920ab82 |
| `opposed:berserker+angel` | King+Berserker vs King+Angel | opposed | `kberserkerkangel.uftb` | **CERTIFIED** | 759,158,400 | concrete | 218,952,532 [167,665,590] (133,257,124) / 86,258 [32] / 27,283,286 [7,904,050] | 231,220 [13,636] (32,192,160) / 264,545,860 [92,190,448] / 82,609,960 [34,927,170] | 246,322,076 / 133,257,124; 347,387,040 / 32,192,160 | S3 table sha256:c36ac8cfadbeb6da85de5d565c0f3cf5863f7fab3575656b187004565a5a4083 VersionId aY6aIFked6U.IPLCDQlB2hcFyQQUde7q; certificate sha256:9c90cadf23aee9e62de9758ce7a8484f8379236a49ca496d5ccca8becfec7efd VersionId dsoMEPH60BdpBFwtvsleYMyeH6n7eKjV; reachability v3 sha256:0f22f1df0264010c5de9c53225f1a24a1e08b56faea661b7bdd8553dda6d26a7 VersionId 3qnfqkcdFYa8lMH0HszCSzY24kPZdnD6 |
| `opposed:berserker+fisherman` | King+Berserker vs King+Fisherman | opposed | `kberserkerkfisherman.uftb` | **CERTIFIED** | 379,579,200 | concrete | 43,145,968 [21,483,818] (133,257,124) / 0 [0] / 13,386,508 [58,622] | 0 [0] (16,096,080) / 143,592,952 [7,694,834] / 30,100,568 [14,160,284] | 56,532,476 / 133,257,124; 173,693,520 / 16,096,080 | S3 `4e28c28b…` / `ALz0nmRU6g9NnmsxX9Z.MF4WV2HOMY8F`; output sha256:31d4fccb2fa110188d7fd20ba5b6493ba30428e20b9dce5305121c4f4fffd560; reachability v3 sha256:8b240a739885f8cb660d1c5dd355754cb00519995dc80d7e1647c2fedf17fd7d VersionId eC_NvQVGMc3KJf8rH4Nr0pbWTPU0XsAE |
| `opposed:berserker+dragon` | King+Berserker vs King+Dragon | opposed | `kberserkerkdragon.uftb` | **CERTIFIED** | 379,579,200 | concrete | 41,739,206 [21,463,178] (133,257,124) / 7,362,054 [420,078] / 7,431,216 [210,660] | 42,958,590 [33,954,920] (48,931,400) / 88,075,900 [3,604,702] / 9,823,710 [2,472,108] | 56,532,476 / 133,257,124; 140,858,200 / 48,931,400 | S3 archive sha256:3b1917db24e5c05810e42e1cf0000636350bddd91eaad564d39debadfad546bc VersionId A7NUg91DR6U8MyBmJ2yi2heWnDtI8w9Y; table sha256:463cca983d48f134bf94576fc29ce2fa3b48590a2ed22579e433baa3194ce8e9; certificate sha256:6bb136f9bc766e410ee59bda572501258cdd7f3afc87c1959c98d6c11355381d VersionId sJ4f3hH7lB7zWT0XlgfJNrNFWhO63DMq; Bellman and archive-restore residuals 0.; output sha256:463cca983d48f134bf94576fc29ce2fa3b48590a2ed22579e433baa3194ce8e9; reachability v3 sha256:0178df990b56ddd4a59fc212ed0936a85b363e4879bf38c5f020f9c90b90d64d VersionId J7SEUSlGGwsYstBobadIqP_70W6HBgSy |
| `opposed:bomb+bomb` | King+Bomb vs King+Bomb | opposed | `kbombkbomb.uftb` | **CERTIFIED** | 37,957,920 | concrete | 6,440,274 [681,974] (3,841,414) / 4,746,170 [129,570] (60,466) / 3,889,740 [983,130] (896) | 6,440,274 [681,974] (3,841,414) / 4,746,170 [129,570] (60,466) / 3,889,740 [983,130] (896) | 15,076,184 / 3,902,776; 15,076,184 / 3,902,776 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:80565e57018d522cc9a91d561b9898f22a63704cae79bf40c7a166a2c609ddb1 VersionId WtbPY.WEDwVT4dXXn.dTCQeG0cg5X7Fd |
| `opposed:bomb+ninja` | King+Bomb vs King+Ninja | opposed | `kbombkninja.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,900,960 [1,591,850] (3,842,796) / 489,142 [5,962] (528) / 9,742,834 [1,466,396] (2,700) | 1,190,298 [210,638] (5,421,572) / 2,177,708 [117,310] (53,280) / 10,136,102 [2,248,270] | 15,132,936 / 3,846,024; 13,504,108 / 5,474,852 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:719432f91cbabee9fc63f8793a0c52ac9ac0bbfa4f6c313c6295c3b85dd122d1 VersionId KmdessEUaKkfPS9BOYx1cQ363lhjnUuy |
| `opposed:bomb+turtle` | King+Bomb vs King+Turtle | opposed | `kbombkturtle.uftb` | **CERTIFIED** | 37,957,920 | concrete | 15,132,776 [2,903,640] (3,845,820) / 0 [0] (4) / 160 [160] (200) | 0 [0] (2,437,986) / 15,822,016 [251,228] (64,838) / 654,120 [653,744] | 15,132,936 / 3,846,024; 16,476,136 / 2,502,824 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:1de0d54c263cb90aeb54c974d19c28251489f9062179a4faa3c69e749edbbb89 VersionId iTDpPOCsRzxJHXMEk_BaNYrjZ6YxApa8 |
| `opposed:bomb+ghost` | King+Bomb vs King+Ghost | opposed | `kbombkghost.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. The fresh strong graph remains verified under concrete sha256:fc10b2a5ee22ca9ec8aecb1caa89a9de03a6431804a594cf95626f63a20eb1db, model sha256:d87ecb37913bbc5c67653a9742ee3e18cc823e97ecacd58aa0472008d7739f68, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. The 500-million-node and 1-billion-node failures remain retained. Memory-correct `ultimatefish-info-kbombkghost-strong-solve-v8` is active on i0b CPU 11 against the S3-restored compositional-v7 graph with a 1.5-billion-node budget. At 18:29 UTC Bellman iteration 7 had advanced through geometry 490,000/492,960 at 461,986,828 BDD nodes and 13,596,336,128 bytes peak RSS. Strong graph archive sha256:9e03643ba0dec2705755ccbc850a075767b622973915222acb3248270fc4a953 is S3 VersionId `pAzUOJxjD4WTvFft7X5r.sjmKIu9WqLs`; v8 wrapper sha256:04d2b4ba4514ec9376f1e5549e1d2843760a10c10736656bdfcfbceb7c90f2e1 is VersionId `g9pM7FHoSRJY1hW.1Kc4AkB14mYmGTOy`. |
| `opposed:bomb+mage` | King+Bomb vs King+Mage | opposed | `kbombkmage.uftb` | **CERTIFIED** | 37,957,920 | concrete | 15,132,936 [2,903,800] (3,846,024) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,296,088 [3,325,170] (67,760) / 5,504 [5,504] | 15,132,936 / 3,846,024; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:0926ddff438762ecb6774f5d0c827f712cfc2dddd8d400299310556421957cfe VersionId 9m_KNEM71VVebosHxXq3nEfC0O5Tabjx |
| `opposed:bomb+penguin` | King+Bomb vs King+Penguin | opposed | `kbombkpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 6,721,592 [1,720,940] (4,563,760) / 819,348 [0] (19,288) / 12,058,892 [1,413,384] (127,648,800) | 2,471,876 [0] (1,925,280) / 3,970,504 [133,952] (33,150) / 15,667,080 [492] (127,763,790) | 19,599,832 / 132,231,848; 22,109,460 / 129,722,220 | S3 archive sha256:1746070975c8ec23d716c274de58b008bcb9d7c0f7115442f1f74c62c67e940f VersionId DBtMEP0ECD3kEXfxo.BQQB4rZmM.2.zH; result sha256:f6546ea7233d72a4e904170f5113b2021dab7235c5c9916e796a169ff57db294; certificate sha256:84d3b9a2a7889faf54daa2b6b976ab826881ff5608e31349bfdcee1a0a9ab466 VersionId .8052JHu8lyWbhCITi9LHa2la.CygqMK; reachability v3 sha256:f93916171408753245504cd91e4e9cf8cb3b065c6d8b2fabd9ab26b1f995aba4 VersionId MCrqNa42iL5H__JaE5BZiANbKmqH2Vav |
| `opposed:bomb+parasite` | King+Bomb vs King+Parasite | opposed | `kbombkparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 6,976,158 [843,608] (3,842,796) / 3,007,200 [95,062] (3,224) / 5,149,578 [821,496] (4) | 5,374,364 [1,352,612] (3,093,268) / 5,237,688 [54,332] (59,260) / 5,211,320 [5,672] (3,060) | 15,132,936 / 3,846,024; 15,823,372 / 3,155,588 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:5e3e1d019eae22479f8d327c0368fd2baab6d2c18312f44c0606f9f0bfc1c70e VersionId ixI0v7hce5x_Uxg02UvGIzaU2X2WXTFM |
| `opposed:bomb+devil` | King+Bomb vs King+Devil | opposed | `kbombkdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:bomb+sludge` | King+Bomb vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:bomb+sniper` | King+Bomb vs King+Sniper | opposed | `kbombksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 30,195,199 [5,788,960] (45,579,259) / 3,103 [0] (246) / 67,570 [52,837] (70,463) | 17,159 [113] (7,440,953) / 32,601,128 [788,509] (34,711,234) / 987,815 [817,789] (157,551) | 30,265,872 / 45,649,968; 33,606,102 / 42,309,738 | S3 table sha256:d45f12015a074a4d3028d94e0f1aa7eb646ef90bb05363d0997162632505cd9a VersionId AUcq2DmUv5gD.YMzrGzGWSUDYQNl_QTp; certificate sha256:39a72dae4d9f87e7245a2f124783ea191ac2bba798b8ec7596291482abd5b52c VersionId pqpvXLuisBeZtZ.Q4ThuRnT5G_dcGOF8; reachability v3 sha256:690491347dc2a14e2b8ecf23a53f860d2bb1bdd8a204e5e8858f06e59443fde8 VersionId hlDeLxkcUvCHApyvR.3PXmFOIm9BGfcj; result sha256:da5ae52691b37e45faf8e846021158542f88fce980291c2d16229103d045ebaf |
| `opposed:bomb+prince` | King+Bomb vs King+Prince | opposed | `kbombkprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 5,503,540 [1,624,552] (3,842,796) / 4,450,992 [8] (3,228) / 5,178,404 [1,279,248] | 9,444,906 [847,724] (3,162,508) / 2,718,824 [118,044] (52,464) / 3,599,906 [393,926] (352) | 15,132,936 / 3,846,024; 15,763,636 / 3,215,324 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:ca3a9f90da6219c6146e2ed5a45b4263575d043da14009851b0a56f08016f376 VersionId zeZEuYmcaIeUnPznWp.qc3nEEf6sp3DQ |
| `opposed:bomb+checker` | King+Bomb vs King+Checker | opposed | `kbombkchecker.uftb` | **CERTIFIED** | 151,831,680 | concrete | 28,750,394 [5,593,343] (9,268,558) / 0 [0] (4,170,190) / 24 [24] (33,726,674) | 904,032 [904,032] (4,054,914) / 31,403,069 [659,741] (9,325,271) / 1,276,453 [1,276,420] (28,952,101) | 28,750,418 / 47,165,422; 33,583,554 / 42,332,286 | S3 table sha256:44d2277b10b2892d4d3bca4be8329f9facf7af9c438f2fecf8b82e2727d57c67 VersionId cg1f9I2LK1oXqInvKW7megIov0UCnKfX; certificate sha256:e69e9aa47d6a3bb57d8e27fad0d014d76c68a5690777d555d34e04f2adf14dc0 VersionId FYY8pAQk0Spqeb9VoMHlsvXEKYhKgCzP; reachability v3 sha256:0d3446d4c88814cada2b0adf92db5e43641dd66c429a5cf8980948caba83ef6c VersionId RcPskQakUZKAVaGcF1QGcwgUz_8HdrSN |
| `opposed:bomb+giant` | King+Bomb vs King+Giant | opposed | `kbombkgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 10,070,952 [3,002,922] (2,726,186) / 19,504 [0] / 470,050 [299,746] (5,692,268) | 30,656 [4,482] (3,121,728) / 8,037,238 [229,674] (43,188) / 2,052,834 [1,392,516] (5,693,316) | 10,560,506 / 8,418,454; 10,120,728 / 8,858,232 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8b18afacb45f47f76e77cd86aaccfbc557763efc64cd05e1a3328c34d7babc84 VersionId t_Mm2nodx1yhXs_xx1FfUJuEp2mzdDHR; result sha256:a933018cdcc7a230eb2613e12b512bc36063a672cd33188fb8d7f30ae727dace |
| `opposed:bomb+copycat` | King+Bomb vs King+Copycat | opposed | `kcopycatkbomb.uftb` | **CERTIFIED** | 75,915,840 | concrete | 248,056 [3,672] (8,405,592) / 13,861,752 [468,400] (116,968) / 13,883,856 [3,752,096] (1,441,696) | 21,361,760 [7,963,352] (7,510,168) / 88,816 [0] (1,504) / 7,547,032 [1,608,536] (1,448,640) | 27,993,664 / 9,964,256; 28,997,608 / 8,960,312 | S3 table sha256:07f204048979078b351b700efdcd347a168bb25eebd23c5a1821badb99fc1452 VersionId lT0q95zeZHS.I8SNzU7golLURvOgw51F; wave certificate sha256:e258fbf5f32cfa84ff2eca8a64c0fb0f5a5a5e15974c5e5cf0d443c5f5154fc6 VersionId WGcMMMkUw_YdlaADZxk8rtZ6hQuX1OL8; reachability v3 sha256:6e4b527f71d0b33dd8e94444d4d98c2305868c8516f5ce23a4df6f3ba7ce38f7 VersionId AWkP1Ja7LMsbwoB3m5fGYsBdmX29GLro |
| `opposed:bomb+angel` | King+Bomb vs King+Angel | opposed | `kbombkangel.uftb` | **CERTIFIED** | 75,915,840 | concrete | 33,712,614 [9,201,194] (4,115,036) / 14,574 [0] (1,100) / 112,848 [64,850] (1,748) | 37,570 [272] (3,282,400) / 32,884,298 [686,924] (71,112) / 1,681,316 [1,186,842] (1,224) | 33,840,036 / 4,117,884; 34,603,184 / 3,354,736 | S3 table sha256:c3699acc66cae32160ee1a3e5a86432cf212471d9282a0b07f28f1900308944e VersionId CHlzN5XPUngDIaL0iETVcNGdtcFm.ZFi; certificate sha256:2b51532d253450a003554cf5ac3585f205bb917570d37c14759f9084ffeec9e6 VersionId _bpeN3iPIdGxfoDNRNdQ3SbHtj9x.AKq; reachability v3 sha256:621aa284ee60b6a7ecc275362212bca50d2f50e0a31a291ce9ffa59996f082fd VersionId HtDsF9Qi75SgkNxS1_BdBma0lkpl4ih4 |
| `opposed:bomb+fisherman` | King+Bomb vs King+Fisherman | opposed | `kbombkfisherman.uftb` | **CERTIFIED** | 37,957,920 | concrete | 3,883,058 [1,540,246] (3,845,724) / 0 [0] / 11,249,878 [1,363,554] (300) | 0 [0] (1,609,608) / 1,468,590 [294,004] (42,504) / 15,833,002 [83,762] (25,256) | 15,132,936 / 3,846,024; 17,301,592 / 1,677,368 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:f673c619fce20efc68336a46b010ea3f34a0944aa16d90e34eb2c11a9bb31a87 VersionId L9Vd4_vSSRWAWdOUdQjmFxCH0dHMG3oe |
| `opposed:bomb+dragon` | King+Bomb vs King+Dragon | opposed | `kbombkdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,442,892 [1,559,026] (3,843,440) / 116,256 [3,568] (264) / 10,573,788 [1,688,658] (2,320) | 510,846 [76,862] (5,087,512) / 1,891,844 [227,150] (54,248) / 11,433,310 [2,220,442] (1,200) | 15,132,936 / 3,846,024; 13,836,000 / 5,142,960 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:303fdb051e36cf76ae5e6dd27cd4dbf33c0905f45dcc078393ff99fa0c89e872 VersionId pFPJRoelVSKWTt5HoxVP7FW93h.IdQ.B |
| `opposed:ninja+ninja` | King+Ninja vs King+Ninja | opposed | `kninjakninja.uftb` | **CERTIFIED** | 37,957,920 | concrete | 3,731,190 [3,581,168] (5,259,100) / 42,886 [0] / 9,945,784 [167,060] | 3,731,190 [3,581,168] (5,259,100) / 42,886 [0] / 9,945,784 [167,060] | 13,719,860 / 5,259,100; 13,719,860 / 5,259,100 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:a98b4436daa7b6bb6ae88698f48c3213bf0c6383467c338f9ca2085bf814acb9 VersionId GL12YBrevFhQrmxAfdZnlm1r9KKcHCpF |
| `opposed:ninja+turtle` | King+Ninja vs King+Turtle | opposed | `kninjakturtle.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,719,744 [3,748,112] (5,259,100) / 0 [0] / 116 [116] | 0 [0] (2,397,164) / 14,534,186 [416,380] / 2,047,610 [2,047,314] | 13,719,860 / 5,259,100; 16,581,796 / 2,397,164 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:064a1ba31fec601df7df81eb273c0a18c585efcdb148534648f0157e22f11473 VersionId R1FcG.XISG.yzc7e1PEkHJb.Maw9UZW0 |
| `opposed:ninja+ghost` | King+Ninja vs King+Ghost | opposed | `kninjakghost.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. V7 exhausted its exact 750M-node arena partway through iteration 12 after a clean iteration-11 fixed point and compaction. V8 resumed that retained fixed point and collision-safe partial node prefix without rebuilding transitions, with a 1.25B-node arena. Unit ultimatefish-info-kninjakghost-source-order-v8 is active on i0b CPU 12; wrapper sha256:76dceaeaa7031f40a6278d80d57ffda7bd08aaa44ac3993485556efff1a151be is S3 VersionId wnNWjZ6zRCTi3gvkN9zlxpX1XI0xdaTB. |
| `opposed:ninja+mage` | King+Ninja vs King+Mage | opposed | `kninjakmage.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,719,860 [3,748,228] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,942,740 [4,275,730] / 1,426,612 [1,426,612] | 13,719,860 / 5,259,100; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:744442dcb95f2d224e0db7685e4098376f9996b988fd7edf6e3a141b28ba7c79 VersionId _uwZtfu8yDeZv69jppMmgU8Xbg1kkOvn |
| `opposed:ninja+penguin` | King+Ninja vs King+Penguin | opposed | `kninjakpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 9,451,316 [4,106,378] (6,207,336) / 200,870 [0] / 8,335,094 [308,270] (127,637,064) | 665,546 [8,590] (1,900,844) / 3,287,800 [232,944] (384) / 18,234,234 [1,667,434] (127,742,872) | 17,987,280 / 133,844,400; 22,187,580 / 129,644,100 | S3 table sha256:bfb1d0406ffce4d89cdd39fb38788299360f6050fc4c97bd793c42faaec8d0ee VersionId Vlxjc6YPUVMx8tcLwtqLiLmgyd5JO9T3; certificate sha256:27bdd7ed0c1a8cbbeadd4c4cfaa31128601875dba40a267831e248b106c39cfc VersionId qyzxTfIMD0SZGd7i0cYpFDNixTPrUxOw; reachability v3 sha256:2ad7a69df46b5e98bd736097636a7607f770d8e95c21bb5ccd0dcedd9a889c4a VersionId W08lUQQ.F9UlBKr5pBIZfKs4f_KfcWSj; result sha256:9229d9cbe019449cf2cd5cceb7518dc6b1e4334426424371b4c3b629e98d91e5 |
| `opposed:ninja+parasite` | King+Ninja vs King+Parasite | opposed | `kninjakparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 2,199,160 [401,896] (5,259,100) / 1,828,856 [161,690] / 9,691,844 [1,773,932] | 5,070,022 [2,536,168] (3,093,244) / 1,201,524 [11,782] / 9,614,170 [217,364] | 13,719,860 / 5,259,100; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:f84b3177cf301545d875fdc439a65e1ef22d3f0c476a291aa70864cc0632b8ce VersionId dKnB.S_jALK1A1PqG6AqjkbCm9SJ1q3T |
| `opposed:ninja+devil` | King+Ninja vs King+Devil | opposed | `kninjakdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:ninja+sludge` | King+Ninja vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:ninja+sniper` | King+Ninja vs King+Sniper | opposed | `kninjaksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 27,403,612 [7,495,255] (48,440,012) / 1,986 [0] (34) / 34,122 [26,939] (36,074) | 18,986 [8,280] (7,375,156) / 30,048,895 [1,108,924] (31,764,666) / 3,738,381 [3,646,465] (2,969,756) | 27,439,720 / 48,476,120; 33,806,262 / 42,109,578 | S3 table sha256:f9baf2eb758644a3cabd2c45be8f8508ad7f343ef3d858d3c5006c0e0fcd07ec VersionId WhXOW4LQZE2hwjb4Z0kx.O3__shuF3RY; certificate sha256:f1e06ed5c76ec17baf427cbfd782e69595d00a93c8c119c97e52b72a2cdceb52 VersionId 2ZQ0hfTqPIwWHt4.YMhyoS1k3oCkORWP; reachability v3 sha256:1c56eadc5da0dc1277c868f104632ad3a2a287373e184d7fe1eb52c556b7be30 VersionId Kg__f2IC8W9XCVtUyJfVVM_QkGyI1Fye; result sha256:93de17e41dfa1e34e41ef52e4335693b0eba1d40167b27b5e714d5a116fc630a |
| `opposed:ninja+prince` | King+Ninja vs King+Prince | opposed | `kninjakprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 4,846,014 [3,579,644] (5,259,100) / 2,281,212 [0] / 6,592,634 [168,584] | 10,544,172 [2,692,252] (3,093,244) / 323,642 [75,468] / 5,017,902 [33,324] | 13,719,860 / 5,259,100; 15,885,716 / 3,093,244 | S3 table sha256:698b37e79698784db6c95b18b89f8666b40abf3222be3362a12d47961ffb42ad VersionId JcCKYFMZJdqQNXC41my9si3f9jlbVagu; certificate sha256:ec5efd7e8d94e8c46c67794205b745526254065925ba0b629ff58bbafa3a0673 VersionId 05TlhPEJEfhzZXuool.MkEHMHIV1huac; reachability v3 sha256:724bf21aa9dfebcf3dd5990fc86b16afdf8b5a217443c8e8e205c83c1511e631 VersionId snhOoaeMFTeD9.8_D5bzeYNOc.beykWa result sha256:871b3ff462a17422b10bacb898a90f23f4815651ac7b785ec30f1030299b2436 |
| `opposed:ninja+checker` | King+Ninja vs King+Checker | opposed | `kninjakchecker.uftb` | **CERTIFIED** | 151,831,680 | concrete | 26,071,326 [7,217,091] (11,886,589) / 0 [0] (3,981,336) / 5 [5] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,651 [995,446] (11,713,745) / 4,047,781 [4,047,771] (26,397,735) | 26,071,331 / 49,844,509; 33,808,120 / 42,107,720 | S3 table sha256:70f5a58ff1f505dbfd7320eb3871fdc9b4181b0439404ea709f8b939b796fac8 VersionId 2o08h2tpK93EqxW1XEyt9PQZvVp9RqNz; certificate sha256:f3e458dda5408f0b544f9001bcfbee61ef6185bc9111c16cfed67832c159cbc7 VersionId bf08AzJEfuZA_2.fD8KtaIue2.yFkuo.; reachability v3 sha256:ea286e868ef23ff33c97ff0d1ba98bb2fcc65ce06265a0b2cb89b5a514c90196 VersionId oURj9f7i.xB45Jw3ft3ijoXoTJ50z3ld; result sha256:ca368a02f935119311cbae502451ae5bab45cd7a38dd85ccef0ba9d5050635f4 |
| `opposed:ninja+giant` | King+Ninja vs King+Giant | opposed | `kninjakgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,576,164 [4,899,106] (3,695,184) / 13,504 [0] / 1,848 [1,766] (5,692,260) | 29,382 [9,054] (3,051,612) / 7,897,616 [426,314] / 2,308,090 [2,306,294] (5,692,260) | 9,591,516 / 9,387,444; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:6fdbad248e3e633efbd2708553e03108e455cff774f40fa8eac685fe96a2b851 VersionId rUr5V_aMzjxXun7eJPYY8Tu7Yu2YkXzT; result sha256:c4b9a0a3ca1b5da47be4d557b2003d0a264c906a72c62d95ba1e0e219de42c85 |
| `opposed:ninja+copycat` | King+Ninja vs King+Copycat | opposed | `kcopycatkninja.uftb` | **CERTIFIED** | 75,915,840 | concrete | 5,972,976 [5,904,928] (8,221,984) / 21,965,624 [797,248] / 355,896 [350,104] (1,441,440) | 26,362,888 [12,113,296] (10,123,424) / 26,648 [0] / 3,520 [3,336] (1,441,440) | 28,294,496 / 9,663,424; 26,393,056 / 11,564,864 | S3 table sha256:f11af55dc81e6bf06c182859f9f15e8bdbf5fd99a40eb2a9310f1285b9eef608 VersionId QdJ5qkcIC17buas7dRlcOYs_OQNqMB_D; wave certificate sha256:8d918ed5792a315d5072a6d85c4ae1ced1bca3a8f1ff9271b0bdd0b83c738cd2 VersionId zPGSIX_XnLMfMehFu46sGNlD8ZFxWkUK; reachability v3 sha256:492794510cfb26cb2cc3d25c3c46dbb19533af328d43979d52f87de108485daf VersionId OV3u1E.eodJTrbHgwNyt9by2zeXLEJtD |
| `opposed:ninja+angel` | King+Ninja vs King+Angel | opposed | `kninjakangel.uftb` | **CERTIFIED** | 75,915,840 | concrete | 28,643,322 [12,112,030] (5,259,100) / 12,764 [12] / 4,042,734 [527,618] | 33,388 [1,588] (3,219,216) / 24,306,480 [518,818] / 10,398,836 [2,997,468] | 32,698,820 / 5,259,100; 34,738,704 / 3,219,216 | S3 table sha256:d56e1b33688aeda30dbd65f561b37c1070eab2eb09b209eb8dd6eec4e902add6 VersionId RFIfUOhCzhog.T_cnSsndkdtp_wo._JO; certificate sha256:38bd7ee2746fff13fbeec41fc3b5ad82342ac13b03b41d73d63824f44607de6f VersionId iQkRKhjfHlwUIDqwnQw6eJ3Dv9uFUnxX; reachability v3 sha256:f832422118d31ce54ba49dbca8ada9b4a49589775d70d366282fd878f711161f VersionId lL1R_s6sjgEULsO1IO.P18FL21HfNtGw |
| `opposed:ninja+fisherman` | King+Ninja vs King+Fisherman | opposed | `kninjakfisherman.uftb` | **CERTIFIED** | 37,957,920 | concrete | 13,719,860 [3,748,228] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,953,476 [604,752] / 1,415,876 [1,415,876] | 13,719,860 / 5,259,100; 17,369,352 / 1,609,608 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:fcd040e9c89cdbd0dddfc8b37366a596c633a6d4f0683833cf870e6e94ca07b1 VersionId xr_INlJ1wi7XyP25PodKjrsUqyR4Idyt |
| `opposed:ninja+dragon` | King+Ninja vs King+Dragon | opposed | `kninjakdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,589,520 [3,581,936] (5,259,100) / 186,156 [147,588] / 8,944,184 [224,462] | 4,806,436 [3,409,056] (4,893,140) / 276,566 [214,900] / 9,002,818 [294,408] | 13,719,860 / 5,259,100; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:47eb9dad53a2c2cf770ba9ce7cf5d1f10a1c1b553d31c655a30968dac68c09fe VersionId SXau1txaT_GeLvK65RfWKbGHObE7rNFO |
| `opposed:turtle+turtle` | King+Turtle vs King+Turtle | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:turtle+ghost` | King+Turtle vs King+Ghost | opposed | `kturtlekghost.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 0 [0] (4,794,328) / 10,686,114 [0] (27,342) / 21,061,532 [2,741,872] (1,388,604) | 19,285,540 [6,451,948] (4,702,852) / 0 [0] / 13,969,528 [118,232] | 31,747,646 / 6,210,274; 33,255,068 / 4,702,852 | S3 information archive sha256:0911049dbd6d99d36beb9123a6f789cf563cae1848be4cb988b764d117991393 VersionId 6azYjlEeY3krsYccmBnlZ5O6kctgJYtY; preservation certificate sha256:b62b46fc08cd618d12de8e1214f0266d59c620eaf44d0d5d3c22d318fe0da6a7 VersionId aQSSANepz3zDQVgN.GrYcY6lGpnST9yI; result certificate sha256:c978b6a3968ecc23082b425de3e5b157782e40696a232ef1c010f46353c82986 VersionId bQn64IPhUcuTJ_p8w8dgsmTZuymB6Z4X; arbitrary sidecar sha256:e936d847d9995cea76569d22ef787e9a973634825fc4f8b3c9359c8e07c5b888; information trivial v2 sha256:8f9452a3ff5763f0ed163ccd0f7d7e740b65dcb81ae6ef6c4ef80885db9e26e2 VersionId 0sO622mlQgtUS.cQKprei9nrCN9DIXTY |
| `opposed:turtle+mage` | King+Turtle vs King+Mage | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:turtle+penguin` | King+Turtle vs King+Penguin | opposed | `kturtlekpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 85,838 [6,036] (2,807,026) / 4,314,716 [0] / 16,987,036 [2,381,536] (127,637,064) | 8,233,738 [453,460] (1,912,312) / 55,292 [0] (468) / 13,898,550 [1,201,652] (127,731,320) | 21,387,590 / 130,444,090; 22,187,580 / 129,644,100 | S3 table sha256:642390fcd354587ca70ab3b39dd03f96809c62257a0d901c0f34fdca28bd8199 VersionId 4c9alOxfL6T7Uo7qRDGpbxI9NxHS6aX2; certificate sha256:bb40d14f0efd56df082444947c7389899b690b9f9a5e1732f6d78135b6ffe55b VersionId JkTjlVd7Ro3DAH24abUdMSLeXbTJ2sAq; reachability v3 sha256:993dc83f53292173ab4454d2c95576de09bc78b0a7702682d245dc6552e4d080 VersionId 6BOkTFf9Qep_.BDkepzXtBzIQdNvAYf2; result sha256:6c82aa7c28d28e967675f8010248aae9ee53935eea19866e1f9685d5c7b4076b |
| `opposed:turtle+parasite` | King+Turtle vs King+Parasite | opposed | `kturtlekparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (2,397,164) / 7,602,610 [105,364] / 8,979,186 [739,046] | 9,995,776 [2,171,402] (3,093,244) / 0 [0] / 5,889,940 [371,870] | 16,581,796 / 2,397,164; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:e7633e2e07fd4fb1745b19130c1e0b5459b2180108346c213bc5849477a7cd0f VersionId upFcD2VwlzIAtIaJK5GKR4BNVrl1jbuP; result sha256:7e174a2eaa35113df49bbd5b8dd89e16eac9359a96e93b9032d7c1622de8d878 |
| `opposed:turtle+devil` | King+Turtle vs King+Devil | opposed | `kturtlekdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:turtle+sludge` | King+Turtle vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:turtle+sniper` | King+Turtle vs King+Sniper | opposed | `kturtleksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 22 [15] (9,588,678) / 3,637 [0] (380) / 33,159,933 [4,163,976] (33,163,190) | 28,231 [9,079] (7,378,264) / 15 [0] (16) / 33,778,016 [3,605,910] (34,731,298) | 33,163,592 / 42,752,248; 33,806,262 / 42,109,578 | S3 table sha256:19a0b72298fb5247a38b087e53f3ae51968dc9ec6bfbeeb147bbcd22c76c70e2 VersionId L9oLonj2Fd.awgS_MDLE4RkWumKdakhc; certificate sha256:4d654a360f60fc6c3da8819262da8a0cf774aaea2d09a79f2ff76fa9b351a80a VersionId aQONc2znkpgExHMp4OI3x4sTZCCyucrm; reachability v3 sha256:8956ad050170c00dcb89e169abb128198d31df61d365f3b3391408395eb4facc VersionId uHIltL3Qjd64NAHTQV8MCVhuWxcMFlfr; result sha256:1711bff02a6020dac0023006338e7ccb8039c3e7e9db1bf6cdec854d48097a94 |
| `opposed:turtle+prince` | King+Turtle vs King+Prince | opposed | `kturtlekprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 0 [0] (2,397,164) / 14,534,482 [2,988] / 2,047,314 [2,047,314] | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | 16,581,796 / 2,397,164; 15,885,716 / 3,093,244 | S3 table sha256:6456d886591d9e961187e187b172b8a77cecf3de94bfe99a96d0efee3921a4eb VersionId PHfeDyIrpCUmGLywBwEQq1DDyhQ..Ttw; certificate sha256:f64225e56f1a32727eaf01516ea9ad1f8660c34d3bd85bda48cd9d79576d16cb VersionId 37ezMhB6n0rb_07AZg.d_TgExh6XJpVT; reachability v3 sha256:9b09c619483cf2bf90d2d678db6af1ba986ede0f2f6e1e6be093bd01625279b5 VersionId csDOn6nI4xMFyntCSS5G9VfCuq7vSif4 result sha256:0eb410311239b82c8846b71cc251304047540ff6d5b336f7c405106e53347fd7 |
| `opposed:turtle+checker` | King+Turtle vs King+Checker | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:turtle+giant` | King+Turtle vs King+Giant | opposed | `kturtlekgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (1,705,440) / 23,990 [86] / 11,557,270 [2,562,686] (5,692,260) | 42,798 [13,218] (3,051,612) / 4 [0] / 10,192,286 [2,302,838] (5,692,260) | 11,581,260 / 7,397,700; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:0902862e3107b4ff6c5db148ca5e1f03d4bf1e4c030c84e8dbd0d9749bdac115 VersionId 3xhoRqGs_Uzp_pSWxfIm8oLdOrrltefW; result sha256:5e013243f6e49a8a2db0c969d117709183d2c8e0c95ce720cb19b4a23f2210a4 |
| `opposed:turtle+copycat` | King+Turtle vs King+Copycat | opposed | `kcopycatkturtle.uftb` | **CERTIFIED** | 75,915,840 | concrete | 16,119,432 [5,948,088] (8,221,984) / 0 [0] / 12,175,064 [307,008] (1,441,440) | 0 [0] (4,616,456) / 9,036,896 [10,216] / 22,863,128 [7,063,776] (1,441,440) | 28,294,496 / 9,663,424; 31,900,024 / 6,057,896 | S3 table sha256:b64d26d188618396a5b5a67c6b647309c90c7f87f51e0839aee5509b4c045dec VersionId Qd.Tuy.Man9Xuf4gRnI3I8mJh1nKxb_.; wave certificate sha256:78878b279615ffe4a167c7d8fddcbe593c76145a2bf647d6d0fa6d818bca3e60 VersionId R4ayDHJpNBECULRYdePIO7yVJ9OzYK3R; reachability v3 sha256:e8babb5ddf4d4cfee923a3fca99de333dcee6487f5e0a7dc1612ff501dff8b87 VersionId Km_4r5enxx5zYCradxuQP54a1MOfE386 |
| `opposed:turtle+angel` | King+Turtle vs King+Angel | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:turtle+fisherman` | King+Turtle vs King+Fisherman | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:turtle+dragon` | King+Turtle vs King+Dragon | opposed | `kturtlekdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (2,397,164) / 14,530,158 [564,036] / 2,051,638 [2,048,292] | 14,056,448 [3,613,858] (4,893,140) / 0 [0] / 29,372 [29,372] | 16,581,796 / 2,397,164; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8297ddb40f36bbf17767f4b73c2109e4240cd68f908d6e9ab8304c534b30c293 VersionId CazIfNcCGOOR_RXaM8RtCDMgptvjL06Q |
| `opposed:ghost+ghost` | King+Ghost vs King+Ghost | opposed | `kghostkghost.uftb` | **PLANNED** | 151,831,680 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. Fresh full-domain perfect-recall graph build `ultimatefish-info-opposed-ghost-pair-current-v1` is active serially on i024 CPU 1 from current model sha256:5b815fa030b6c4b5274c13b316ee3fa4324df5f4785a3ac20b3549bb69070026 and destination-aware observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Corrected concrete sha256:7ccf53a0dc5477404b4d966b39c5d7d1acfdd91958567857fedc22db5485a4ab is preserved at S3 VersionId `_p6GHftcdIodFczn41rW3qwyD5fK52MW`. The obsolete pre-fix graph stopped at 12,240,000/77,900,320 roots and is not reused; the current job starts at root zero in `/mnt/ultimatefish/opposed-ghost-pair-current-5b815-v1`, uses 50,000-root durable batches, and is bounded by 48 GiB measured RAM, 500 GiB checkpoint size, and a 100 GiB free-space floor. |
| `opposed:ghost+mage` | King+Ghost vs King+Mage | opposed | `kghostkmage.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 34,501,620 [7,816,732] (3,456,300) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 31,839,192 [2,366,896] (67,760) / 1,415,876 [1,415,876] (1,415,876) | 34,501,620 / 3,456,300; 33,255,068 / 4,702,852 | Fresh corrected-rule specialized solve completed 37 exact iterations with zero Bellman, rank, monotonicity, singleton, compaction-root, structural, grouping, conservation, and dual-force residuals, admitting 67,756,688 roots. Its authenticated 70-file manifest binds source sha256:a69ec5b7dfc6710575f3ba8ee43c44ee28830c15f78094b35a6d50fd2eff2ad1, normalized source sha256:8ee3fe6c859f50de9ebed545a287ee7ac70a00f81d863abe79169d7d69d333b6, model sha256:fe61002db93b3614b1331f7a44daf41c6edc07e3d5b17b5d80613bb4470dfcc7, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. S3 archive sha256:7b9421ed49eacb4f061aea63192a2d007177d69969623d3ab18fff2f3dae0058 VersionId 2tiOOAJFrvaCS1vjoWw8R9sQCMvNrVPp; preservation certificate sha256:7b34ecfb2dd54171eefac24710468b530595e89975d9272954696120e27d0856 VersionId vEMJxAOPkiqMyi9A7jYwKzyhCLwj2QS3; local restore, S3 head, download, archive-restore, certificate-head, and certificate-download residuals all zero. No pre-fix graph or checkpoint was reused.; information trivial v1 sha256:174f7d941e652a547a63753518d6b32be635579b16c6561e3dabdf659518ac3a VersionId lmlWfd.TinSTZVHozCCyr4jyywvTW9KW |
| `opposed:ghost+penguin` | King+Ghost vs King+Penguin | opposed | `kghostkpenguin.uftb` | **CERTIFIED** | 607,326,720 | information v2 | 7,827,900 [7,171,552] (5,520,348) / 1,646,398 [0] (5,144) / 33,221,030 [470,700] (255,442,540) | 4,071,644 [90,446] (3,841,676) / 256,928 [9,352] (33,154) / 38,365,964 [4,634,186] (257,093,994) | 42,695,328 / 260,968,032; 42,694,536 / 260,968,824 | S3 information archive sha256:813b7c79c6f26429588c3dd995dd6a83ef592cfdc41fcce57c4125fd6dc76d25 VersionId P5naba5zoZJGYhnHKp5nsgv4lvtlYsHJ; preservation certificate sha256:54fb099569b4ac1ffc411ef059a48644affa83af6ceab689dda0e335c6110130 VersionId o1LX295qsTlNfLuP3Pi0HeFCe_slhqR_; result certificate sha256:b9fc1fc0506a1157883b200f5f062c5a0ea3a8ce4083b5353e92f1f25ef088e8 VersionId V.Z5teqsANCVfXJABM3Ki4gRMEtjbMqZ; arbitrary sidecar sha256:d0a55bedae2dbe3a20ef2b0b8e596b5867ddaab0ac8ba40c875c40242fb6d09d; information trivial v2 sha256:bcdfba30c013561dce6d27fbf8c5c781bcf97a406236b928d96338ef99d92573 VersionId riHgksYrIHQDvCRidfaLx5T2L9ng5uO6 |
| `opposed:ghost+parasite` | King+Ghost vs King+Parasite | opposed | `kghostkparasite.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. The fresh 29-shard tracked-oracle graph, authenticated 64-shard transition merge, and compaction-6 fixed-point checkpoint are retained on i024. The version-pinned v3 resume correctly selected BDD slot A and the recovered transition prefix, but failed closed before changing the checkpoint because the solve-time transition re-audit reported `Parasite lower-table transition residual`; it has not been restarted blindly. An independent v2 remerge from the retained shards is active on CPU 2 under wrapper sha256:ac168d74400180b65089c92e17c0365b7780907a87fc9e8335cabb1b618fb401, S3 VersionId `iQo4dPyoLVYLSS3Pg9wdOfdUz6SxD4j9`; it writes a new prefix and byte-compares every component with recovered v1, without touching the tablebase checkpoint. Bundle sha256:2be6d67854696ea5a6af2ffc71ec629a43894afa61540feeb7f4f6356b527e65 VersionId `RaeHAm.2K6JFbVhB8yPu9.tuXMB_drZM` binds corrected concrete sha256:af888568f496353e4477d364f1652f5b2329b51820ab07a7d4b0c80bd362baf1 and permanent-tracking lower sha256:32dc7889506f4dd4948dbf1bed3f31c6c3ebdd4a8600c4381c7a8c710b9ec671. Resume wrapper sha256:ae151cb0dc1cf88d06dd81224e3126801b8e375e8a599e82af62667a3743f0ae is S3 VersionId `XYq22IZPa9c3pwtZFnATbtQhnO369SnG`. Both capture directions enter the tracked domain; ordinary singleton UFGM is rejected for possession edges. |
| `opposed:ghost+devil` | King+Ghost vs King+Devil | opposed | `kghostkdevil.uftb` | **PLANNED** | 91,099,008 | information required | — | — | — | — |
| `opposed:ghost+sludge` | King+Ghost vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:ghost+sniper` | King+Ghost vs King+Sniper | opposed | `kghostksniper.uftb` | **CERTIFIED** | 303,663,360 | information v2 | 132,891,644 [26,367,773] (18,811,408) / 6,906 [3,171] / 121,722 [2,040] | 35,487 [12,362] (14,786,856) / 124,139,016 [1,689,038] (266,198) / 7,030,679 [5,926,449] (5,573,444) | 133,020,272 / 18,811,408; 131,205,182 / 20,626,498 | S3 information archive sha256:7734575c27bddfd4afbeb0cf4fffd2b40d658bf70bbdd5d5129a398d9d5a84b3 VersionId 1jG5tCFhn.7AmdEN7Q2dagEDec4gjzpd; preservation certificate sha256:5c92367555039c66c71d9fd58aba2c90728cc9bac7bb587e2fcc9c984d406d1d VersionId aalriJafe9yB4NfuPJX_r1nnziM5q4Wz; result certificate sha256:85aa58e50c5c256a54f5be8b8732d87edf12785a8660086488054e046899a41d VersionId e1nz5KEuuP38NYE0miUtQIejR_l7Ef3f; arbitrary sidecar sha256:eb238fc1377ff7c7f4051bb3a38be884a1dccd1c566f4f6d174f91646b632abb; information trivial v2 sha256:0754994e929af7269752e2b32e9e66dba2d7e063543417d7bcd789c55d80aafb VersionId d9LmWHqwkiTAYzk5Jv2MzHVtG6_BPUnI |
| `opposed:ghost+prince` | King+Ghost vs King+Prince | opposed | `kghostkprince.uftb` | **PLANNED** | 151,831,680 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. The completely independent corrected graph is authenticated by marker sha256:08de4e98e854832a9449f694a14617ec01bd71e803440fffc5ec77f2ef7a055f and binary sha256:71fcb13099ae8862823b8a440fdfa59f19f35d37fc6c26afcc8b517e062c778e. V6 reached Bellman iteration 8 and 990,738,437 nodes before failing exactly at the 1-billion-node ROBDD budget, with peak RSS about 15.1 GiB; its graph, 47-GiB tree, and log remain intact. Solve-only unit `ultimatefish-info-solve-kghostkprince-2b-v7` is active on i08 CPU 14 against a separate copy-on-write graph/scratch tree with a 2-billion-node budget and 32/36-GiB cgroup gates. Iteration 6 completed at 888,278,099 transient nodes, changed-owner 1,824, changed-observer 77,788, and changed-visible 1,930,363, then compacted to 8,461,627 live nodes with structural/root residuals zero. At 18:29 UTC iteration 7 had reached geometry 10,000/985,920 at 14,439,602 nodes and 14,401,691,648 bytes peak RSS. Wrapper sha256:49a80b12e7d7155efa29e866a3d56a027778215b055409107a0a5020a925ce2e is S3 VersionId `wp6CDeAMqKTzG0F9J4YL0HfiMwguYuCy`. |
| `opposed:ghost+checker` | King+Ghost vs King+Checker | opposed | `kghostkchecker.uftb` | **CERTIFIED** | 303,663,360 | information v2 | 66,510,136 [13,140,360] (9,405,704) / 0 [0] (7,987,800) / 0 [0] (67,928,040) | 0 [0] (9,699,168) / 60,624,674 [15,976] (6,435,153) / 4,388,153 [4,388,153] (70,684,532) | 66,510,136 / 85,321,544; 65,012,827 / 86,818,853 | S3 information archive sha256:5f9321d19ab86a17d302b4ab45983289ccf40ac33cd119ebbcaaee1bd55e7a03 VersionId mFxNTgWIxia9aN8jamtlvRHvbCGkg59K; preservation certificate sha256:1a444dc77e985d069f359c748380687f19b96498ed9163bbf838f3db55deb2da VersionId _Y6KGaUfYVB5ACl6nT6lp2k9DL_S68vo; result certificate sha256:7494fddda2a37f2943a92d1105d32be634f9bfe50f08d11f07b7ce406c991be6 VersionId D9TtjHvn7c.62mxcomz3VlOA2BPzpBNP; arbitrary sidecar sha256:7764deee6a452d986ae501aaf5093eebb13b3348d448085aad1b63cf673b4398; information trivial v2 sha256:9e12e57bb7e715f9e0a5826a90bbf3ae5ad7e40f5567954aa10cf9af930c611e VersionId 2O1rm_YoQGh0O5TrQJn0hiKImxOOhjEM |
| `opposed:ghost+giant` | King+Ghost vs King+Giant | opposed | `kghostkgiant.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 23,027,424 [6,574,512] (3,335,424) / 44,316 [78] / 166,236 [117,954] (11,384,520) | 40,538 [11,516] (6,104,934) / 15,510,940 [0] (39,824) / 4,033,270 [3,773,612] (12,228,414) | 23,237,976 / 14,719,944; 19,584,748 / 18,373,172 | Exact 59-iteration corrected-rule specialized solve from source sha256:98d084a83f2a8e6c3aa070849028cddbfc6b46181fc083eea86411b675b219b4, normalized source sha256:a541e48414b9dba0808b34b39053fd3a0f95a90ccfc167716c35eb0de81177e0, model sha256:32467999e767ec9bc5c4bfa781f478da3055e334bbf084486a99cbf839a3643f, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Result UFIW sha256:e50f913fd9771d6c3fa95886a644a324f1b89a073c269c5d748c7ad6d3125107; transition payload sha256:cca876e18013b5d2480fdcd47139c09a84b5af59c99d746bc69afa22ba2f6e0a and arbitrary sidecar sha256:61107912c1c6d33da1338ba467270fdd800beb67b66245ee8fea32e4c01d3a13; Bellman, rank, monotonicity, singleton, compaction-root, structural, grouping, conservation, dual-force, and source-remap residuals are zero. S3 information archive sha256:1c798df4a376745fcb25f4ce590ae18349e9eb878aa98539e480a823beed44e8 VersionId `IJ7.u8DX7IMF3_JrVlGpwUMVcJuFGCh1`; certificate sha256:491795e0aeb7612965f220fc276d02f188dc2a798cd488a07c287e6f0d5e863a VersionId `ZpItq93ijLeslbOkfAI44wo1PK_aTtyN`. HEAD, exact-version download, inventory, local restore, and S3 restore residuals are zero across 70 artifacts; source evidence remains retained.; information trivial v1 sha256:e55f5f7b89e511c13f871ed31529ab0d825f47fbba596f7d5927d60adc675ad4 VersionId fMnx2I2ozsyHcGKpOfFD2Ii6vEBCSLV8 |
| `opposed:ghost+copycat` | King+Ghost vs King+Copycat | opposed | `kcopycatkghost.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 7,381,648 [5,905,008] (18,749,800) / 696,168 [0] (15,208) / 46,094,704 [4,292,800] (2,978,312) | 21,382,176 [19,594,880] (9,054,144) / 752,248 [0] / 41,844,392 [352,864] (2,882,880) | 54,172,520 / 21,743,320; 63,978,816 / 11,937,024 | S3 information archive sha256:9f33560e7c5a23604f32a630646bce69393504dc7bb071f5e29b549734bfcff7 VersionId _3YYFiavaNSS3dcn3qUzzNAO2P8fLFAM; preservation certificate sha256:c340e333c2b7dc597e64a95fb05464f0f9f80cc25c3bd68ee547abaac445bc6d VersionId GX7mwvd_TKAYi56vnyZh35t8_XsrkcP4; result certificate sha256:30d4d8e33a491eb418ec028258b846293a2bf86ffaed9fb4fdd3fd32b0a4ca41 VersionId Sb6e6zm1nFZYwwDyjlwD8Zgl3H2PSFB1; arbitrary sidecar sha256:80b72ae88963752773bfe3e79af3c7df85ef8d7e53dbf899ecf03dd151c898aa; information trivial v2 sha256:641ae128975689e2e4089a3c622ddc2a2a8e33679b3d17e524d29c74aa7e016c VersionId nlXOUFjdJ808e18fofZRvjsjc2UvYltT |
| `opposed:ghost+angel` | King+Ghost vs King+Angel | opposed | `kghostkangel.uftb` | **CERTIFIED** | 151,831,680 | information v2 | 45,096,416 [15,457,756] (6,129,436) / 25,816 [9,168] (436) / 24,481,148 [643,536] (182,588) | 57,050 [2,468] (6,440,900) / 31,004,114 [32,194] (32,194) / 35,448,972 [2,932,610] (2,932,610) | 69,603,380 / 6,312,460; 66,510,136 / 9,405,704 | S3 information archive sha256:44b3dafa5c85997601a9c70f6a1dbc0bbf85c47a9cef00ffef28a0d6c6ad2b4f VersionId bghKvWB8mBgv.o85DRh8QE4kgq9jAdtA; preservation certificate sha256:81e4133376918de969a308599ac64cd4d16ab4e5c79e40a99861e02ab009fa32 VersionId D7y4jyqpaSazisTkSoXsCIlNPYypN1U6; result certificate sha256:127dac280cfd8410a8a40ae440c57c31d334206951692f5026a0cf350f23221e VersionId 1.c6zrpLh19U6hb5ADG.pTgO2XP1C4y7; arbitrary sidecar sha256:e7bb0fa3fea2867cfe2e797e32e5c7042cc5b207f1cc051ffcff0b54a2a8c76e; information trivial v2 sha256:c7ef86c588d9ab26a90bf3a7426da8fa072334ac74eea584af1fc930505715c2 VersionId Rv2pWgmApu9ya25Z8i0.6AgtRUZtLQed |
| `opposed:ghost+fisherman` | King+Ghost vs King+Fisherman | opposed | `kghostkfisherman.uftb` | **CERTIFIED** | 75,915,840 | information v2 | 6,562,206 [6,534,566] (4,620,900) / 0 [0] / 26,774,814 [117,706] | 0 [0] (3,272,322) / 12,160 [772] (5,532) / 33,193,224 [6,325,630] (1,474,682) | 33,337,020 / 4,620,900; 33,205,384 / 4,752,536 | Fresh corrected-rule exact solve completed 16 iterations and admitted 66,542,404 roots. Its authenticated manifest binds source sha256:9568af731a3b41e144defad62069ea901eb8f3ae3e1350bb978074ad3898ae3e, normalized source sha256:e7d54554d8456593442e3b0f1c4e39c41f8540770a4b2f2e27c6e780c96abf8c, model sha256:436b1277bbc88bb1f47d87d4926b32424da609abd6970cb09eed86a9ce5b9ed6, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. Transition payload sha256:20aa250edd7f0759ee5aa2e692655dbca08053ba2c0c471c00121f910c6aa95e and arbitrary sidecar sha256:d66471cc56b1526881db71e9e0eec90ba5cc897669bd896239419769fcf7a7d3; Bellman, rank, monotonicity, singleton, compaction-root, grouping, conservation, dual-force, structural, source-remap, transition, pinned-version download, and archive-restore residuals are zero. The 536,572,407-byte S3 information archive is sha256:e3ea7f024ba5286774322412e66a7074ba4b6aa49111cec894e1273073a94945, VersionId `85iOw57RGPwrAqO7prEp3dpxikjL1Xrv`; certificate sha256:813db65c94202538e24901b9990b80def18f0e9fbdb9c1f79570bc453143601b, VersionId `gFJWMfgd.qXMf_0hJdqqts8id.Cs9spa`. Source evidence remains retained and the certificate's deletion gate remains false. No pre-fix graph or checkpoint was reused.; information trivial v1 sha256:785b44619e10ca7fb3685ec890e576949e88a31f7df56a4f5175e2251ebeb663 VersionId dHxD8k1zPcoLp7WLRplsERateiR.9dNB |
| `opposed:ghost+dragon` | King+Ghost vs King+Dragon | opposed | `kghostkdragon.uftb` | **PLANNED** | 75,915,840 | information required | — | — | — | Fleet decommissioned; the unfinished progress below is historical only and is not a certified result. Fresh corrected transitions remain verified for all 492,960 geometries under concrete sha256:f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225, model sha256:c3ce5a68d38944b19a36594a6d3e1e3ad862d1c096902b350d4dadda19cf4de7, and observation sha256:890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23. The 500-million-node solve exhausted its budget; the 1-billion-node i08 retry failed its physical-memory gate. Authenticated graph archive sha256:679efb649622592a9d5034b56ea101623d169ed1a6f6843a6fa0c223ea029030 is S3 VersionId `u4cDO9VSrWR_gXYqqzU3B0FdfFnsBWfR`; its fresh extraction reproduced the source, lower tables, legacy marker, and all five transition components exactly, including the 9,082,299,584-byte block store and zero-residual 557,661,284-edge replay certificate. Memory-correct unit `ultimatefish-info-kghostkdragon-memory-v5` is active on i0b CPU 24, preserving the legacy marker separately while current source performs one final exhaustive replay before a 2-billion-node solve under 96/128-GiB gates. At 14:42 UTC Bellman iteration 4 had reached geometry 480,000/492,960 at 519,832,817 nodes and 23,076,405,248 bytes peak RSS; the process and checkpoint continue advancing under their gates. Native ARM64 binary sha256:86f1ca0fbf1555f4d6026f97a8424b1892e4f8d0c18aec96575b6c4ba6788f5a is S3 VersionId `S.iTuzESz5k8_1vwX.18qrs6FNIXpu9n`; wrapper sha256:66ac302af7847bb585ca51f66e1da11582a031d1273549d56636b27a97b20301 is VersionId `EydHEj.w5W5xn6ySs9Q87kWacwCsKamd`. All prior failure evidence remains preserved and non-certifying. |
| `opposed:mage+mage` | King+Mage vs King+Mage | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:mage+penguin` | King+Mage vs King+Penguin | opposed | `kmagekpenguin.uftb` | **CERTIFIED** | 303,663,360 | concrete | 46,148 [0] (1,881,536) / 22,416 [384] / 22,244,516 [3,062,676] (127,637,064) | 44,662 [12,490] (1,901,860) / 11,076 [0] / 22,131,842 [1,642,522] (127,742,240) | 22,313,080 / 129,518,600; 22,187,580 / 129,644,100 | S3 table sha256:6f8e47606ea7499cd376721e196cccc5a5d8f96e54a2f8986c653b2fb0bbea97 VersionId 02kKpSQcFjNEDFzlrzfEr8GaTDRPqLrt; certificate sha256:b39ecbc5d0e06ed4e53359a9254f8f50429b2398359b6c89cca6d8fe01abd608 VersionId 0NdVdO4oHVFxdaIaat3gDGGjRP5_3IAV; reachability v3 sha256:c1e458bafaa013cde7f1b018556ad60fc8f6de992a67d2d17c43e529f57e494d VersionId d3By2in50Vcc.oamJqYyUlaW_e13l76U; result sha256:9d62161db06f39194bb282289c56433c57caa5d7a64f07091a094383cc8609a2 |
| `opposed:mage+parasite` | King+Mage vs King+Parasite | opposed | `kmagekparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (1,609,608) / 17,369,180 [2,742,076] / 172 [172] | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | 17,369,352 / 1,609,608; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:426e019692a273b694ba71217524b0fafa88f54d0e419d85ffa98621191ad8aa VersionId GwvFNVH6yXdxvglWlVl12C.9_4BibI1c |
| `opposed:mage+devil` | King+Mage vs King+Devil | opposed | `kmagekdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:mage+sludge` | King+Mage vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:mage+sniper` | King+Mage vs King+Sniper | opposed | `kmageksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 0 [0] (6,438,432) / 600 [237] (195) / 34,738,104 [6,002,161] (34,738,509) | 11,188 [9,547] (7,375,992) / 0 [0] / 33,795,074 [3,600,362] (34,733,586) | 34,738,704 / 41,177,136; 33,806,262 / 42,109,578 | S3 table sha256:d4a6c1baf9e8577fdbcf7c1cc458391234a4e75bd6a113a411e5d561bac9cba4 VersionId uLTy2r2mLJ3fxkgFO.U10.MnEQKZzaQS; certificate sha256:15b95279ce3b85f47aad9a5d10f91831c6d8c9e04e6c1c3c62851e8637b43c27 VersionId Nr_yK9sU5QS_9ujbSSIPzkvH8_7XBckW; reachability v3 sha256:83b2d454c979486a71df2214ce66ba8576d2c4ead54441ce7c702ef5ac5622f4 VersionId AFfM3rqjJ1reG5KaPNFp1WFT4FLMBK9h; result sha256:4515cad800d4c94dd32abfac88efb2cd2b5cdde353ae7be5dcb51411b63f52cb |
| `opposed:mage+prince` | King+Mage vs King+Prince | opposed | `kmagekprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 0 [0] (1,609,608) / 15,953,304 [2,438,568] / 1,416,048 [1,416,048] | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | 17,369,352 / 1,609,608; 15,885,716 / 3,093,244 | S3 table sha256:2bade19d205899ce5b872886f1bc46d99a07332e281c55def3f29151d03fade7 VersionId iZ1MPkAtWX5I9N0rDQTERw6GgBBHEb7R; certificate sha256:6726ab0905f261b95dea9767476ae9631cdad7ffe78979418ad9f18def1b6ceb VersionId _S2VLc_pWlVYAobjdew4Xdv2hDwpac38; reachability v3 sha256:30a788eb4e37b7178da5545fae839c6c87788440a9e258b9401bedd6d85f03f5 VersionId sTVf_SCpZfMekeAGvdNFbRoAkfEXduUi result sha256:37d7fc822bc8e388184ca1e3eb0c69a9c3993946d9a459b07642f00a7de93468 |
| `opposed:mage+checker` | King+Mage vs King+Checker | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:mage+giant` | King+Mage vs King+Giant | opposed | `kmagekgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (1,142,116) / 11,632 [3,960] / 12,132,952 [3,927,392] (5,692,260) | 31,532 [17,696] (3,051,612) / 0 [0] / 10,203,556 [2,297,652] (5,692,260) | 12,144,584 / 6,834,376; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:dec4aed9514cf2034034b1018bb7c28b4e95cf639d87a97693846abc5731ddd9 VersionId yPOs5pp.7Lz66DSz2.lhJ4r8tnkth1Oi; result sha256:cf2dbfa1a1c73499fe682ee0a8a60f26b904382b6f59eb139bd7091ce5843fd7 |
| `opposed:mage+copycat` | King+Mage vs King+Copycat | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:mage+angel` | King+Mage vs King+Angel | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:mage+fisherman` | King+Mage vs King+Fisherman | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:mage+dragon` | King+Mage vs King+Dragon | opposed | `kmagekdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (1,609,608) / 15,951,230 [4,006,756] / 1,418,122 [1,418,122] | 14,085,820 [3,642,684] (4,893,140) / 0 [0] / 0 [0] | 17,369,352 / 1,609,608; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:3e4d7bf674a1079572c8958471c4a949a576c73b891bbaff1b08b4be7cd574ac VersionId 8QGPNbdUfTSKtXxsCVJ6ARF.HFAR4ChF |
| `opposed:penguin+penguin` | King+Penguin vs King+Penguin | opposed | `kpenguinkpenguin.uftb` | **CERTIFIED** | 607,326,720 | concrete | 2,316,414 [10,832] (1,870,468) / 871,218 [0] / 21,176,576 [1,538,660] (277,428,684) | 2,305,582 [0] (1,870,468) / 871,218 [0] / 21,187,408 [1,549,492] (277,428,684) | 24,364,208 / 279,299,152; 24,364,208 / 279,299,152 | S3 table sha256:779ce4abdd32ad0a8cd6c1607bc630940d9176f14d1c518e025ada0608798cf9 VersionId kHJxKAUZZfW.1OCcGdjakzy1TK3VRrau; certificate sha256:bdd77a7db29d122e0f4e65366b65e657e48b37414892e4a4a78adc6210920d46 VersionId eJV4vf.xjxhR99ZNeyEeWnfmZfvXXlN9; reachability v3 sha256:33c0feb538e1a9024e137d36844a64db29da6839a5bc1562395d925792ed670c VersionId xryis2_KNHhTl.geAn3MXnIiym9CZC1a; result sha256:659269ce29c4b499ef89947874f0f085e537d0cef662010628eb10148052cc0a |
| `opposed:penguin+parasite` | King+Penguin vs King+Parasite | opposed | `kpenguinkparasite.uftb` | **CERTIFIED** | 303,663,360 | concrete | 3,676,662 [0] (1,901,200) / 2,397,148 [89,708] (384) / 16,113,770 [0] (127,742,516) | 4,688,126 [1,411,064] (3,628,184) / 1,463,544 [0] / 14,414,762 [1,546,572] (127,637,064) | 22,187,580 / 129,644,100; 20,566,432 / 131,265,248 | S3 table sha256:51cf2c96609a1f333dd8a53fe2a03fd73c9b783a7fc34d6a294e957764205f2b VersionId Qr0ccQehPFqotlwCat1oQEKTQvCQr9to; certificate sha256:27cbf0f843af54f5becf3f27a9a0e9135d29da5c29afa1e09f15a3f5abc95faf VersionId g9bGHrWC6WAV.4MDU0Ob2hLxuIEs1sR1; reachability v3 sha256:f2df3fc366876bb91a12b14cba71e4b4d70168752a175c0a676d6d48afd6485c VersionId 5cJ9d6dk0ttymxrLy1TQbSMkBl6_oVy7; result sha256:23b3783c5dbde51d0e8cdba20ffb6cd506800dce9d33bf45be6d676132b41f19 |
| `opposed:penguin+devil` | King+Penguin vs King+Devil | opposed | `kpenguinkdevil.uftb` | **PLANNED** | 364,396,032 | concrete | — | — | — | — |
| `opposed:penguin+sludge` | King+Penguin vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:penguin+sniper` | King+Penguin vs King+Sniper | opposed | `kpenguinksniper.uftb` | **CERTIFIED** | 1,214,653,440 | concrete | 28,078,231 [2,258,675] (38,164,291) / 67,736 [1,809] (41,116) / 14,619,585 [1,037,018] (526,355,761) | 202,605 [30,522] (8,728,431) / 21,824,065 [0] (21,090,790) / 21,504,578 [5,822,875] (533,976,251) | 42,765,552 / 564,561,168; 43,531,248 / 563,795,472 | S3 table sha256:009b01304c23a041b4b0daf1e24c4c84f9cdb9cb50324f7f624395e9dc8a6b2e VersionId biWm4WdlUw8m8iFlSwWUp7pmK36C30So; certificate sha256:1a039408c4c3554e90d19c1a66e27ceb70bc9b9337ea40a0548459a7836982ea VersionId fQcZFooWUiMCZcHHWouoz0kw0.k9nwLB; reachability v3 sha256:9d660d6cd0778090cfc903630c68250864ded697e4926e2d1e5a35c6a88ccbf4 VersionId KUKGVr1AC14BgjCZ8aiLYQA7PG3UypQQ; result sha256:625b236cbd2c232d9111b714f822ac0a6c0a85ded6667d0bca176cde91003257 |
| `opposed:penguin+prince` | King+Penguin vs King+Prince | opposed | `kpenguinkprince.uftb` | **CERTIFIED** | 607,326,720 | concrete | 908,482 [5,416] (1,901,532) / 6,270,036 [368] (384) / 15,009,062 [1,649,280] (127,742,184) | 13,969,824 [2,838,042] (3,628,184) / 295,934 [0] / 6,300,674 [134,918] (127,637,064) | 22,187,580 / 129,644,100; 20,566,432 / 131,265,248 | S3 table sha256:a85e35bfd7445e3e0c3fc3dcb5b9611e46c2dcfc909a1daacb23e5bc43278dde VersionId vYuCmNcWCB_N34AWYblEdsgBCHNROBjt; certificate sha256:e62ac9ada0350184766e2d63912cf403a2cc24eb3cda1000ab3527bff4df402d VersionId qvWP0c8rlgVHfIn85.ky.sVKrM9L9RDL; reachability v3 sha256:355ce3f2d9a346ed2b7516b6b8f0aa40bc01a50801af50a663c41230cd7e1c7c VersionId TQ12NuQaXcXhmV1K8nwHkkC_aIRPOkvq result sha256:805c6087b9c4eb81d2aee2fbf30fae0269032eb0a9784df41f8613879dbc5e68 |
| `opposed:penguin+checker` | King+Penguin vs King+Checker | opposed | `kpenguinkchecker.uftb` | **CERTIFIED** | 1,214,653,440 | concrete | 38,588,465 [2,997,096] (4,028,623) / 37,362 [0] (4,662,760) / 3,582,830 [211,756] (556,426,680) | 1,065,566 [986,654] (4,681,076) / 31,851,132 [0] (3,705,788) / 10,466,148 [4,759,463] (555,557,010) | 42,208,657 / 565,118,063; 43,382,846 / 563,943,874 | S3 table sha256:f08c6602c9415246d9d05768a3caf1eb56eaf3438c9e8c29573f35568d285f0a VersionId j7F0qFr_XTjnyZRk5sZf1XY5I2zcTJX5; certificate sha256:ec5ae01acb783b855918309d941a95a4a125a9ab7f36226ad98e0ad7f97f4cf0 VersionId Q33s28wNK348y7xStAqCcquG.oz0g_Gi; reachability v3 sha256:785b5e298835f5b1d6545c54785a67d144b83510d585d1690062f8b36077345c VersionId .B17C.h3bhzrgBy_EIIEC_4EjLiBA1i7; result sha256:abfc9e093894b70a374e6eddd67575c73a30e5f8e26aa12d8c7add200c4034ed |
| `opposed:penguin+giant` | King+Penguin vs King+Giant | opposed | `kpenguinkgiant.uftb` | **CERTIFIED** | 303,663,360 | concrete | 2,438,842 [136,686] (1,434,972) / 93,748 [24] (336) / 13,627,494 [1,877,670] (134,236,288) | 161,494 [21,824] (3,649,580) / 809,970 [0] / 13,059,708 [2,685,300] (134,150,928) | 16,160,084 / 135,671,596; 14,031,172 / 137,800,508 | S3 table sha256:5e37f32304d4c133759ca50b2f6f89239ac62ddbd8e3cd91b038ade496c8a41e VersionId cgL647uK2L.rZvLu_1pAUzRl4hKLn1oS; certificate sha256:c44dd407efa24e7ed03a210d5dcb99fc488f4a7009f8187c398b1f1a3ca282cf VersionId ARzRrLK7zOVjUKDJr8fOHuNUZDvgrZ84; reachability v3 sha256:826c5bc4c25e8b5818a6a1ef0668d9dbb187fd1ef62a266b36d77e8fbf28f320 VersionId O2U75OsEvUnLclSXcXWKccQgA4bUb5Dk; result sha256:b157e48890e8294f457c7e6e5598502fd1dda2bb00b2130a772ce79ca41e9e4e |
| `opposed:penguin+copycat` | King+Penguin vs King+Copycat | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:penguin+angel` | King+Penguin vs King+Angel | opposed | `kpenguinkangel.uftb` | **CERTIFIED** | 607,326,720 | concrete | 56,376 [14,808] (1,906,332) / 33,488 [0] / 46,157,444 [5,027,628] (255,509,720) | 109,648 [0] (3,763,072) / 18,824 [3,972] / 44,497,688 [5,313,496] (255,274,128) | 46,247,308 / 257,416,052; 44,626,160 / 259,037,200 | S3 table sha256:b7d46b6df7f626b053da0d8cd457486787793c6fa23b6177d9bd2f12e96753ba VersionId OkcQVMKyrgHQvAsORCar3yPNOHgdy.FV; certificate sha256:da8506e424631abafa17f80f32f194bc06e1091ac8d200e76c56987153e78359 VersionId OtIBfhS5M7.h6MITxibjMSEOmjAWSWGG; reachability v3 sha256:7524d45b307f529cf8772b53ecf5e0e8abf2c95ccb52c50470a5be8c793383a4 VersionId fc1EKFnvoWPEzOx5tSEefCA.2YQXK0s_ |
| `opposed:penguin+fisherman` | King+Penguin vs King+Fisherman | opposed | `kpenguinkfisherman.uftb` | **CERTIFIED** | 303,663,360 | concrete | 277,430 [24,326] (1,902,856) / 9,676 [0] / 21,900,474 [1,630,686] (127,741,244) | 39,656 [0] (1,881,536) / 111,230 [5,274] / 22,162,194 [1,666,916] (127,637,064) | 22,187,580 / 129,644,100; 22,313,080 / 129,518,600 | S3 table sha256:9b39b813291bd890710ab0cc4fefecfa44215d831619016aa86f514cb67759ed VersionId FExts.38LhxFUuAx96DmkCeOSBqvHR7g; certificate sha256:e94b4f53b2ec5c2e0c806053ea97bc35478b1a92638e6a58d876fee5e0113235 VersionId h2av3E4PoLJlu6ubIZWfxEvmINAHtWMr; reachability v3 sha256:18a77a53f665c84edf533c1b278535fd5fe85f36b1b8c41cdf4c0f6309ef7cce VersionId bQyrqWclrorqUymaCNgZqTZUNwhse8VF; result sha256:62920549fe495d0157f7109648e9b8fca9bfd01316023c4c80243daeb3e4f13b |
| `opposed:penguin+dragon` | King+Penguin vs King+Dragon | opposed | `kpenguinkdragon.uftb` | **CERTIFIED** | 303,663,360 | concrete | 752,816 [34,664] (1,903,140) / 965,350 [287,284] (240) / 20,469,414 [1,652,154] (127,740,720) | 6,345,962 [3,952,822] (5,766,916) / 206,072 [0] / 11,875,666 [369,722] (127,637,064) | 22,187,580 / 129,644,100; 18,427,700 / 133,403,980 | S3 archive sha256:05d81d7b41898268579807b5443903171b15ea2910ce42ef539df1921dfa026a VersionId _8SYfiYg0KdSkoHbqlZ6bTMpVk1airvS; result sha256:53844af37522f54b3cc3c77b0be8db61ce5e5ba9eab4f50283b14abae5de9a91; certificate sha256:6e4fcfd40405e2a5040b29d84d0faf3fb81957dd82b365f7fb727a42d3791b93 VersionId mo_91HR7TFgpwMSXqdgH2uPbYs7lr7Dz; reachability v3 sha256:89142ba5cc27f66c7b9891ba24b2a9d234781d065a44b9183104cd1d492d5f2e VersionId sBiWZ4W0D1A9ABY1ttrv2L2bQBfLTm6Y |
| `opposed:parasite+parasite` | King+Parasite vs King+Parasite | opposed | `kparasitekparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 5,802,628 [1,356,396] (3,093,244) / 3,262,986 [0] / 6,820,102 [60] | 5,802,628 [1,356,396] (3,093,244) / 3,262,986 [0] / 6,820,102 [60] | 15,885,716 / 3,093,244; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:abeecb1b2dd8e5c9dfa9139938156bb2c7f3d2d5f62c076f7bc3754093804064 VersionId tHg.LZSDbdA4JS9C4CyWKlWWisHMtvFG |
| `opposed:parasite+devil` | King+Parasite vs King+Devil | opposed | `kparasitekdevil.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:parasite+sludge` | King+Parasite vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:parasite+sniper` | King+Parasite vs King+Sniper | opposed | `kparasiteksniper.uftb` | **CERTIFIED** | 151,831,680 | concrete | 30,827,161 [5,026,190] (43,200,137) / 3,362 [0] (324) / 940,909 [103,708] (943,947) | 18,837 [175] (7,373,790) / 31,459,030 [372,293] (33,426,461) / 2,328,395 [931,171] (1,309,327) | 31,771,432 / 44,144,408; 33,806,262 / 42,109,578 | S3 table sha256:dad63a2cc212b684a33c5f9df65ea1d3888900566a680a830f74e5162aa0dd39 VersionId Wp9qRyitJD.M75ZkC2XPQDG6dHhWLnqd; certificate sha256:74ac7f7af7d6fb7e868f35cea5b34882ac0c848686bc270c89eaae0f90e23494 VersionId Bk0nIuvDvab_YPLBQ2KjI3XlzGBGHtlb; reachability v3 sha256:dba702d6495987e72feaeb6f0cc232bc04f55b21b3c7b3168f39aa224d76ca42 VersionId yAv5328P92LPYogjTu2G_bXw9t.i9tPP result sha256:15f3fdcdb3fd40059ede627b69ad5a1abc6485601730d2d7632de0487363bc86 |
| `opposed:parasite+prince` | King+Parasite vs King+Prince | opposed | `kparasitekprince.uftb` | **CERTIFIED** | 75,915,840 | concrete | 4,652,674 [2,513,040] (3,093,244) / 8,407,008 [0] / 2,826,034 [30,232] | 12,315,894 [1,120,886] (3,093,244) / 1,456,090 [90,364] / 2,113,732 [153,608] | 15,885,716 / 3,093,244; 15,885,716 / 3,093,244 | S3 table sha256:d87d7cc6e993d9d66228e3b69e8fc799c4fd08801215e3d1bfc73e4ae19899e0 VersionId XtGHBcwegk1mYhFwAUOUPPthpDiiKfq1; certificate sha256:dec6c4f8c55fc1106760ea7f56ebc200ac1471a68afd430a333dceb106f4ed28 VersionId znsd25_fIYyHN09hwIwhcLPzZp0pqJ.Q; reachability v3 sha256:257052c867f442fc0dd8afa371f57599178ce7a59194343a93b985f23d0bfa10 VersionId RSxzpD7rKwKliWwyUQ1zK4fT0RNiYkuL result sha256:945fb573ed4d54a979916ad8a8f59314712bab3ce4c120ff91a6293b8495d637 |
| `opposed:parasite+checker` | King+Parasite vs King+Checker | opposed | `kparasitekchecker.uftb` | **CERTIFIED** | 151,831,680 | concrete | 30,184,807 [4,908,545] (7,772,580) / 0 [0] (3,981,336) / 533 [533] (33,976,584) | 842,688 [842,688] (3,996,240) / 31,486,080 [310,286] (7,730,990) / 1,479,352 [1,478,670] (30,380,490) | 30,185,340 / 45,730,500; 33,808,120 / 42,107,720 | S3 table sha256:b86e55ca8392040f78fd9a99c649c38969cfcd6c2af7b6fd1ce2fcf67935f9dc VersionId qGd_X1pG6hJ8ddwXaVWsCrrGAhjP_Ae1; certificate sha256:0faae310d9da157393a68f4e6c5d5daaf797197ef812e8facd8e552a07249a6d VersionId 9yJ3ynsHZeWTWBxYIZoAYVRSY_fcY5h2; reachability v3 sha256:75aa89c7b82baefc8ac13c3bc83fa8183d1a1e43276d04de3992e95c464fba94 VersionId Nn3HzM13mYKRzcx9Ar1w4c0NdBMdtvse; result sha256:23df48e4e7de77314af68271e030b635a3d2475399bd0af466a16670915f3bf4 |
| `opposed:parasite+giant` | King+Parasite vs King+Giant | opposed | `kparasitekgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 9,667,136 [2,259,216] (2,193,308) / 22,216 [78] / 1,404,040 [561,382] (5,692,260) | 39,092 [10,070] (3,051,612) / 6,932,524 [90,918] / 3,263,472 [1,552,590] (5,692,260) | 11,093,392 / 7,885,568; 10,235,088 / 8,743,872 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:c38864c18a0f49215f520f339f5f3e419fae56bebc73400764e3c524271180eb VersionId w.gsl64rpKcKcc4dc_i_q9lLisvx6b_J; result sha256:726d837a97ab80c74da4bfb4492dce77950c96351b36fbb6105b42e62171918f |
| `opposed:parasite+copycat` | King+Parasite vs King+Copycat | opposed | `kcopycatkparasite.uftb` | **CERTIFIED** | 75,915,840 | concrete | 906,120 [12,904] (8,221,984) / 27,368,096 [4,002,792] / 20,280 [704] (1,441,440) | 30,159,016 [8,545,680] (5,955,168) / 392,904 [0] / 9,392 [96] (1,441,440) | 28,294,496 / 9,663,424; 30,561,312 / 7,396,608 | S3 table sha256:67e72c03e95a77716e87ed1001dc615c27e0e2017a5a7caa396e085e71e69f58 VersionId fWRbRtVgucxDE6KPpMyCTw8Rlk.ThWiC; wave certificate sha256:8f38b16ccbc78ecb09e1df85265496b580c5380608182e11a445a92ee180fa9c VersionId 4jFT8slqIuthsjWHUrMHkiQBVVJZTr0R; reachability v3 sha256:8cd20a5e5b83a4d5615f5e854b10349c1140a300bd81ce4da953d7eadd3520c4 VersionId Z8nTzM3a8TjdljRk7gWqsVZ9ll3b_46T |
| `opposed:parasite+angel` | King+Parasite vs King+Angel | opposed | `kparasitekangel.uftb` | **CERTIFIED** | 75,915,840 | concrete | 33,229,226 [6,453,100] (4,702,852) / 16,192 [0] / 9,650 [24] | 42,198 [0] (3,219,216) / 34,669,604 [420,960] / 26,902 [616] | 33,255,068 / 4,702,852; 34,738,704 / 3,219,216 | S3 table sha256:3a30824c07ccf6c04b93aed4ac0249d875c8a5ce5d435d8776acd168c5c91d48 VersionId w5M1piYceL2FdPiwsAI3DMYNOqA8Csi7; certificate sha256:dc47c297a68c47cff5b0c75b4e6928dff65cc50442ef9ea9a95591a0a1cede60 VersionId Ya5xQKuZiiVzfp3UQ9e5Tpgp5HRvtMUD; reachability v3 sha256:180e67eba5ee04cba6ef7a1eadb03a8baaa470108b1a3c0b09b2c32ad1abbb75 VersionId 8ATTNIAvlIxrNnQcNfFSiJPIEwhA8pJf |
| `opposed:parasite+fisherman` | King+Parasite vs King+Fisherman | opposed | `kfishermankparasite.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (1,609,608) / 314,868 [115,894] / 17,054,484 [110,084] | 1,742,400 [1,338,528] (3,093,244) / 0 [0] / 14,143,316 [1,204,744] | 17,369,352 / 1,609,608; 15,885,716 / 3,093,244 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:b570d427fd6bd00f44f788b88d6f4794ec0f3cdb586a32870620fcf9ef8e7f51 VersionId g4bMWi8hGVolemZoZQQ8zXiUU8dCHzRt |
| `opposed:parasite+dragon` | King+Parasite vs King+Dragon | opposed | `kparasitekdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 4,898,546 [2,541,902] (3,093,244) / 314,622 [6,194] / 10,672,548 [296,902] | 887,716 [128,792] (4,893,140) / 1,657,392 [302,362] / 11,540,712 [1,897,188] | 15,885,716 / 3,093,244; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:55b19e6cbc39cd0e789da838e4b4d630491fc5952026b2111dc3a77438ef1eb0 VersionId Op1WKmZr4Ez.yLNWMjXTr4tnxvD9oRMQ |
| `opposed:devil+devil` | King+Devil vs King+Devil | opposed | `kdevilkdevil.uftb` | **PLANNED** | 53,044,992 | concrete | — | — | — | — |
| `opposed:devil+sludge` | King+Devil vs King+Sludge | opposed | `kdevilksludge.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:devil+sniper` | King+Devil vs King+Sniper | opposed | `kdevilksniper.uftb` | **PLANNED** | 182,198,016 | concrete | — | — | — | — |
| `opposed:devil+prince` | King+Devil vs King+Prince | opposed | `kdevilkprince.uftb` | **PLANNED** | 91,099,008 | concrete | — | — | — | — |
| `opposed:devil+checker` | King+Devil vs King+Checker | opposed | `kdevilkchecker.uftb` | **PLANNED** | 182,198,016 | concrete | — | — | — | — |
| `opposed:devil+giant` | King+Devil vs King+Giant | opposed | `kdevilkgiant.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:devil+copycat` | King+Devil vs King+Copycat | opposed | `kdevilkcopycat.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:devil+angel` | King+Devil vs King+Angel | opposed | `kdevilkangel.uftb` | **PLANNED** | 91,099,008 | concrete | — | — | — | — |
| `opposed:devil+fisherman` | King+Devil vs King+Fisherman | opposed | `kdevilkfisherman.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:devil+dragon` | King+Devil vs King+Dragon | opposed | `kdevilkdragon.uftb` | **PLANNED** | 45,549,504 | concrete | — | — | — | — |
| `opposed:sludge+sludge` | King+Sludge vs King+Sludge | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+sniper` | King+Sludge vs King+Sniper | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+prince` | King+Sludge vs King+Prince | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+checker` | King+Sludge vs King+Checker | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+giant` | King+Sludge vs King+Giant | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+copycat` | King+Sludge vs King+Copycat | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+angel` | King+Sludge vs King+Angel | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+fisherman` | King+Sludge vs King+Fisherman | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sludge+dragon` | King+Sludge vs King+Dragon | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:sniper+sniper` | King+Sniper vs King+Sniper | opposed | `ksniperksniper.uftb` | **CERTIFIED** | 607,326,720 | concrete | 82,139 [22,265] (29,633,213) / 9,976 [0] (20,535) / 67,520,409 [7,350,219] (206,397,088) | 65,701 [20,032] (29,600,117) / 14,004 [142] (38,876) / 67,532,819 [7,352,310] (206,411,843) | 67,612,524 / 236,050,836; 67,612,524 / 236,050,836 | S3 table sha256:7fa741e5cf0b5a5de3642832736cb45d9b1fd3870cde3793bcb6edde82d205a7 VersionId UqBPctDZhDraQHDY_EMd.kNxeIJh8zyX; certificate sha256:3cba99e66b8cf9a0092b3b914526736d20ad12af4300eb05fc5930146f4d98c9 VersionId _Lm5bQnmQpAIOPbrsi7J3dhOo.zttKTJ; reachability v3 sha256:6e6be95f06b78687052414aa6fa898f3b646054b9b305a40ed2fa842a62598cd VersionId feb0eGEHrDBhKjFfzrV3G8.xyqYWkN00; result sha256:40480cbb8019d8c8804f82fce7a036fed4f2afc88134bb3181d56c9e60b6291a |
| `opposed:sniper+prince` | King+Sniper vs King+Prince | opposed | `ksniperkprince.uftb` | **CERTIFIED** | 303,663,360 | concrete | 21,074 [7,950] (7,376,279) / 30,072,342 [12,859] (31,837,201) / 3,712,846 [3,604,026] (2,896,098) | 31,728,060 [5,088,903] (44,101,036) / 2,399 [0] (181) / 40,973 [12,540] (43,191) | 33,806,262 / 42,109,578; 31,771,432 / 44,144,408 | S3 table sha256:42381901343592708fb7e45ba555042aa8d8d1e6e101ce353edf4c2741a02fd6 VersionId P63uMUnoHCo9x4ZwUH8ID9.3aRWPWs_m; certificate sha256:8950e7f0056fdab80ed6f18f2b6d1a8b95354c2f87d8c15c00cdace3f5dc6d89 VersionId IgttE778M6xPyE4NPevnUNfQP4YESQ_c; reachability v3 sha256:be4cdc1c4c75d21d252f3c893e0700058e6ea6145bb5e278162f5addf7c2fe28 VersionId tkQ7aG4I2Nahl3Jgz6dkdlpundJoB_bm result sha256:13a44527959eabd4ad528de1171e80a3788874aee835e0ef2b5c6d28cd149c67 |
| `opposed:sniper+checker` | King+Sniper vs King+Checker | opposed | `ksniperkchecker.uftb` | **CERTIFIED** | 607,326,720 | concrete | 62,927 [19,466] (14,787,558) / 0 [0] (15,925,344) / 64,174,862 [7,014,267] (208,712,669) | 1,685,376 [1,685,376] (17,670,336) / 8,642 [368] (14,405,940) / 65,922,222 [8,282,737] (203,970,844) | 64,237,789 / 239,425,571; 67,616,240 / 236,047,120 | S3 table sha256:c76c6c2050a0275a7faa6617351b78e3e1112a5c0b1144d704c6875a619549a2 VersionId OW33UwbYrgrmA52bT0FO2rwSVAl8T9Wr; certificate sha256:30187812b2b48fe4920b8eff76ce2eee368d021b31ac62d564254926126b6fca VersionId nqjr7hEyVw0wscc7zRYHyTW5hUxgd18n; reachability v3 sha256:37acddb301cee6f9b9947cad9597b4b0d1c90d171d8c7caaa0f7b34aaac2ace2 VersionId ASdssWpJ4DwFCwTFUiZByYO2NR0BxD7F; result sha256:637555647c893a3e08cded3c521545607280d051ee9bb61cee78b3008f5e889b |
| `opposed:sniper+giant` | King+Sniper vs King+Giant | opposed | `ksniperkgiant.uftb` | **CERTIFIED** | 151,831,680 | concrete | 25,615 [13,122] (5,215,427) / 54,781 [482] (65,922) / 23,569,822 [4,527,764] (46,984,273) | 101,524 [33,273] (12,307,972) / 2,857 [52] (261) / 20,365,795 [4,658,586] (43,137,431) | 23,650,218 / 52,265,622; 20,470,176 / 55,445,664 | S3 table sha256:0b6d65c33e9622e030a7b4e63a55bfd945637a7aa3fdb8df7f5690891ed298c3 VersionId sYMTKNSwdxzRKz1dmN9QN3BtsWIlRaG2; certificate sha256:aa6d6df5c16968fafba997b10522864dbeca545d965b0f02eb8921a31b69d1e0 VersionId mbFf_HST3Z2TKD0WnJirkyPepwIKLJH_; reachability v3 sha256:aed7daf455a36c1c6bb1a0b2f502131203f5fb16228640c8709de092d4a6a85b VersionId oU81nkuUY3sROw0qK52JXy6q5fPDIYdh; result sha256:56b1361f43ec982bd77c7c3f8458bb5c4a84519667ca0f38643ac8b32f60c858 |
| `opposed:sniper+copycat` | King+Sniper vs King+Copycat | opposed | `kcopycatksniper.uftb` | **CERTIFIED** | 303,663,360 | concrete | 50,858,316 [12,440,844] (83,746,252) / 4,392 [0] (288) / 5,726,284 [227,644] (11,496,148) | 51,284 [28,288] (14,139,100) / 45,434,024 [61,756] (49,447,588) / 19,622,372 [12,724,808] (23,137,312) | 56,588,992 / 95,242,688; 65,107,680 / 86,724,000 | S3 table sha256:265345c7ddfecd74501287bbd2bd2c8646ddc09cf294e0f109c823d5c639c14c VersionId dhu8JGh1QRshmOv9CMPGkz.UTPx4NVMP; certificate sha256:48994dd4db103cdbaaaeb40d7d401a354b141ff0bdfd918b7a2c391fd5f72c3c VersionId 89w6lJfrjYI0R_jO16gE9AIcu58w1JVV; reachability v3 sha256:a11f00b7eb2b6dc7dcb56a722ec9e7037be0f7a4c414ac630c70b927bcb2a529 VersionId x3EqdV5_ZOBRNuR1zTacGW9v0ebU3Bcc; result sha256:62657cf05d173f3e63cb6a50c293a6bc7f3c0f70b4622aa640d0e6ab8daa8629 |
| `opposed:sniper+angel` | King+Sniper vs King+Angel | opposed | `ksniperkangel.uftb` | **CERTIFIED** | 303,663,360 | concrete | 34,682 [26,967] (7,387,161) / 36,130 [201] (40,575) / 71,693,370 [11,108,799] (72,639,762) | 97,477 [7,386] (12,974,341) / 2,097 [16] (2,090) / 69,377,834 [5,872,989] (69,377,841) | 71,764,182 / 80,067,498; 69,477,408 / 82,354,272 | S3 table sha256:1cd542d52dc1764f66086a14056223efc8b330ec272c3187041ebade532da287 VersionId Q0yUaXhzwzHcfbSEI.fG4cqwxjjcNF5c; certificate sha256:653211f294f3816c29c3f9d960614910e4d5bc9cc0d13f4373b5d9ce92d0298f VersionId cMyGrLBKUDmL_GIXVi3kT3QhQS82byoG; reachability v3 sha256:75e4ec9766ce166900d1e0608cb4cf53b1040de788aa0e249943395db951feb7 VersionId 0uvtKJ5eAMEsH.QHB9X53p2KwvlKW8pO |
| `opposed:sniper+fisherman` | King+Sniper vs King+Fisherman | opposed | `ksniperkfisherman.uftb` | **CERTIFIED** | 151,831,680 | concrete | 19,672 [8,981] (7,379,641) / 0 [0] / 33,786,590 [3,600,928] (34,729,937) | 0 [0] (6,438,432) / 2,171 [1,060] (1,102) / 34,736,533 [2,941,016] (34,737,602) | 33,806,262 / 42,109,578; 34,738,704 / 41,177,136 | S3 table sha256:49506fddb6ecf7d32b9d714ec0f7e89c61d9c17c30a6142f02f73fe102f52971 VersionId P7fOdMz.RoFc4oiaHuxZF007K0z8Mqtv; certificate sha256:35110c83210f79c8ae626575434815f274feb91ca437cf040e3d4faf6bf486ba VersionId ievylUml.OSxyjv2l_A3TNaDM_uITXbQ; reachability v3 sha256:2a491bd38eaf0dcb7c64de07e699f93bcb50d55b30f84c4f6ae11b1bc26cbc26 VersionId sgk65IH8_hVt9L.qB4mr5gMqzpe0w2xf; result sha256:3c28915b87c2a61479a4ed9e11a05dad6cafd7e4a01df5333897f5de07caa51b |
| `opposed:sniper+dragon` | King+Sniper vs King+Dragon | opposed | `ksniperkdragon.uftb` | **CERTIFIED** | 151,831,680 | concrete | 18,551 [8,451] (7,377,486) / 29,948,299 [1,243,184] (31,728,605) / 3,839,412 [3,624,913] (3,003,487) | 28,081,570 [7,276,246] (47,654,130) / 1,946 [618] (618) / 88,124 [78,376] (89,452) | 33,806,262 / 42,109,578; 28,171,640 / 47,744,200 | S3 table sha256:568fbc9cb8ea5007b2d503e3decdce673596c0aa9024d4b6f3efea29ef93e54f VersionId L6emW15asmHvpUJBC6fP.06GGY6.O_jo; certificate sha256:5566b0e45798c84340550f5558e1bd01979f0f3bac7e24c771787ab874309b5d VersionId iqA1Qd1_6od0xczhDR.RLPrKKLBXhJUr; reachability v3 sha256:53ce1cccdbeb085feda815c590a3ed0404313fa05aa6677c473684366774a3e0 VersionId aoDdF_uysSBmqfwoocKjTc4TfVlB0tdv; result sha256:061fdbf0fa706e3d905f862e6e2055175f5e6e9747a45d86fe5c1eb64a2a01b2 |
| `opposed:prince+prince` | King+Prince vs King+Prince | opposed | `kprincekprince.uftb` | **CERTIFIED** | 151,831,680 | concrete | 9,496,340 [2,537,522] (3,093,244) / 2,032,316 [0] / 4,357,060 [6,446] | 9,496,340 [2,537,522] (3,093,244) / 2,032,316 [0] / 4,357,060 [6,446] | 15,885,716 / 3,093,244; 15,885,716 / 3,093,244 | S3 table sha256:5b2ffb1ddcac85495556b75ae826ab1c02c090016c1170c95b054712584fa073 VersionId 2H6eBUBFy3ySuRK4_5JV5wtWuATPcNbC; certificate sha256:035ccb67b7d367df4a67e7eeee82740617cff3188ea89aab54fd21bb6bc8da57 VersionId z6WsKuUqoyyo52AMFmWaRJbDus27k7k5; reachability v3 sha256:1666d2cef0f82b6050d004fe5fee05de83407bc4c43da2e929c3aafeab0af4e3 VersionId vKSyFXtX.xgPZLUiASdaQBsplO_LaxHg result sha256:86458138107d33f578a26e31142b6248701454ced8dc89e3df58b218747f5b40 |
| `opposed:prince+checker` | King+Prince vs King+Checker | opposed | `kprincekchecker.uftb` | **CERTIFIED** | 303,663,360 | concrete | 30,185,340 [4,909,078] (7,772,580) / 0 [0] (3,981,336) / 0 [0] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,866 [26,083] (7,588,408) / 4,047,566 [4,047,566] (30,523,072) | 30,185,340 / 45,730,500; 33,808,120 / 42,107,720 | S3 table sha256:3a6cfb4fd829479694f5b2dc5662e225e53bd2d73c1e7edfaa7cbd63e65aa13d VersionId 9HkTRZfOcI9AmNW5c7wvreyMIz.GHepj; certificate sha256:35687a172d174938d0917ac752ef1ca2ebc7e8899aea5899f06d70c9563c1cfd VersionId l2vBSCaoNZPOc5wOMAAp1drXm70P_g7E; reachability v3 sha256:7b75f8bfaed37275a40bac7f6b291cd650de7119b5b7b45161176b65be81f2ec VersionId R_WGCs8A0WIAtcbdKruJMCE82iYg5jks result sha256:2d10c9908be3f336c416db2a654121d276e1a3cee496a0c2b60f40cad49c8178 |
| `opposed:prince+giant` | King+Prince vs King+Giant | opposed | `kprincekgiant.uftb` | **CERTIFIED** | 75,915,840 | concrete | 11,077,872 [2,827,256] (2,193,308) / 15,508 [0] / 12 [12] (5,692,260) | 32,332 [10,020] (3,051,612) / 7,897,424 [2,266] / 2,305,332 [2,305,328] (5,692,260) | 11,093,392 / 7,885,568; 10,235,088 / 8,743,872 | S3 table sha256:58bcd4864513d2bea9d7d76ec178d15e005fafa5b46528138e599bd3015606c8 VersionId AZ7fMGY5jAkiJwnxc7Q0qxvhXlz7RnZ2; certificate sha256:4440863ca38d47bc1b75f677dcc454cc883f67e06e2a012d5b690c2b2b9f2a7a VersionId wekMoJuvIXI9DLOQxpcZSevZgkCH3x5k; reachability v3 sha256:477be29b95ba5c78cc42da9142c6a14a05b1bbd79e8e84d2a022ca7928988e3c VersionId emJHQpctMC0RqyaR2h9OjLn6fsphagjB result sha256:7c975a34f8be98cadcffb0c0431aefd13c4df11aba4c54af3d316e6c3d2f14cb |
| `opposed:prince+copycat` | King+Prince vs King+Copycat | opposed | `kcopycatkprince.uftb` | **CERTIFIED** | 151,831,680 | concrete | 6,043,944 [5,904,928] (8,221,984) / 21,894,696 [128] / 355,856 [350,104] (1,441,440) | 30,510,808 [8,551,472] (5,955,168) / 48,928 [0] / 1,576 [472] (1,441,440) | 28,294,496 / 9,663,424; 30,561,312 / 7,396,608 | S3 table sha256:6af2e5cdb57f9158605b1b64df180bcafda4734ffff369ae6294c51337f7a041 VersionId ork8WgQhe85Zjkmx4EMp69x7H7BUgxkE; certificate sha256:c3e34ddaf76a3f34582459857c43a219f3c813e76b31efe7b900eb3b7f74746a VersionId sKzySH.A069CcBbTeFpXc6DleL0ZOsRK; reachability v3 sha256:e0aa5d50a1f5cd3023f7fb8dc15dacd60c531f24f4246b627635188c9b42152a VersionId 2pZdIi7ra7g1IIsRD5J_62lQjMZ_or7d result sha256:2a606ed0abb2544f0c59d11d5faf4537b803a4ea53129ea9efc6dfeea637bd06 |
| `opposed:prince+angel` | King+Prince vs King+Angel | opposed | `kprincekangel.uftb` | **CERTIFIED** | 151,831,680 | concrete | 30,787,966 [7,864,510] (3,093,244) / 14,038 [0] / 4,062,672 [195,260] | 36,778 [1,344] (3,219,216) / 24,628,176 [10,788] / 10,073,750 [2,899,296] | 34,864,676 / 3,093,244; 34,738,704 / 3,219,216 | S3 table sha256:f27415aee74b6f578688d0e930efaca0eba51e55cb1b03e54138cd9388379f6c VersionId YSZX_hixuafrwX94.UZXTg1Ht0EjhOpH; certificate sha256:5ddf6ebf09d55b42b404e43b894d42785a03ac3ec19bf2a6f3714ab8966f9d61 VersionId XmiNO0Y8vrQOzSGIdFokTOWEgm_TiiwA; reachability v3 sha256:38f8d26e15a5cff81d9281040ea7b08ea2dce8ed98805ef066b4401b95bce483 VersionId KgX5pMnSXAD.S7LdKqrNqRKWUhoa8.8O |
| `opposed:prince+fisherman` | King+Prince vs King+Fisherman | opposed | `kprincekfisherman.uftb` | **CERTIFIED** | 75,915,840 | concrete | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,953,476 [8,080] / 1,415,876 [1,415,876] | 15,885,716 / 3,093,244; 17,369,352 / 1,609,608 | S3 table sha256:1a28c7aca6f2f57deaaf85903798b1b863171c84ff23eeb0e920116627caf34a VersionId TAS8vAJmKQpnTAWIhxFzHFK31CN_NCuh; certificate sha256:520ca97f5c6bdafbe9b75668429d258a4ec0369af0bcb0ddbe3a98df0b38d471 VersionId _dOgsPwszoq4XS8QoFJZL8ld3FoFR63h; reachability v3 sha256:825ce212a777dca56427a442034f0e8b109f07d47c8f55e7c633d8e205c3af7d VersionId TnoPRLMiPeZyUmceCnB3i.T7uGdasPgm result sha256:865807122ae9a67559d398a98013eee2c4c61d427091acaf18662857a90738f6 |
| `opposed:prince+dragon` | King+Prince vs King+Dragon | opposed | `kprincekdragon.uftb` | **CERTIFIED** | 75,915,840 | concrete | 14,755,942 [2,730,756] (3,093,244) / 131,108 [78,374] / 998,666 [25,960] | 4,154,896 [3,392,696] (4,893,140) / 6,130,796 [1,484] / 3,800,128 [249,988] | 15,885,716 / 3,093,244; 14,085,820 / 4,893,140 | S3 table sha256:b1da44829ea0718c3a371276850034fad8022728a428ed6dc0d347e0f2475cf2 VersionId o36okCShN.BfYUqmeBzzTlOoojpp8HRU; certificate sha256:38b67286c116e943adc8c678c5ae09219c5eb34842dc7dd658f8dfafb05c739d VersionId tdo4bCha3nRUvIAflId44_oYTaOxxiF.; reachability v3 sha256:0077a0f0fb8202a0a8f6a42df92b271080a0a50ff8f10c071c40958fd989c120 VersionId ALuTVPhkrZoZPGl0.YNobwjnJr28wt3U result sha256:d68283d813726a389ff33e8c3571f012c89e80934c527428ea6a52b3e6bf6936 |
| `opposed:checker+checker` | King+Checker vs King+Checker | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:checker+giant` | King+Checker vs King+Giant | opposed | `kcheckerkgiant.uftb` | **CERTIFIED** | 151,831,680 | concrete | 568,962 [568,962] (2,807,624) / 50,459 [362] (5,780,640) / 23,832,789 [5,823,076] (42,875,366) | 89,909 [28,268] (6,115,708) / 0 [0] (2,778,776) / 19,336,078 [4,473,781] (47,595,369) | 24,452,210 / 51,463,630; 19,425,987 / 56,489,853 | S3 table sha256:96e835dd2c0b20bfc59e8ed141f0d32369b67b9c624691e53583bdeb33d4f8d5 VersionId Q6of5oD.FcqPGKxZp6ilm7odiOxZqTmu; wave certificate sha256:6ec681aaeda7a27665ff3fc7c5e0fad34178e3707dc8c3e287c74bf36dc69176 VersionId c270SIGgLesjkGhdkHcMm7Ty1DyNBgmS; reachability v3 sha256:0d22df862a63d8ae9782a065750209cdd059e6927cd30aaa657f7394951b532e VersionId B55dMgCgJIdMv.CLJxVBK6.rjyf6jjja |
| `opposed:checker+copycat` | King+Checker vs King+Copycat | opposed | `kcopycatkchecker.uftb` | **CERTIFIED** | 303,663,360 | concrete | 53,755,432 [12,070,144] (19,264,204) / 0 [0] (7,612,608) / 12,792 [3,668] (71,186,644) | 1,598,400 [1,598,400] (7,671,552) / 50,350,944 [105,116] (18,469,916) / 14,551,000 [14,519,304] (59,189,868) | 53,768,224 / 98,063,456; 66,500,344 / 85,331,336 | S3 table sha256:44ed1e42d75c4738879f6d330606979b59516eeaf5e047b5f5d55f5fcb7b3b19 VersionId 9mKWCcF1S4K2HxGCxNSYILfvYxfb3R82; certificate sha256:3f2a06ba4239d341c248ba6e18da1baa220fbe80995fce7fd011e3dc8d4a00b8 VersionId iHbDCg3k65rg_h1zjPYeNN28Y.UBoRWs; reachability v3 sha256:30a7622c1b3a2054c50830b5e2d640646f8039135270a2c4838bc428fe4df2e3 VersionId zlska3igpTSBwoG1gHmC5D2k2B5BhSvV; result sha256:d932ad3a788fad22af676487e3f73096ce40bdb06cb3f2200d9ccd5fda40aaf8 |
| `opposed:checker+angel` | King+Checker vs King+Angel | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:checker+fisherman` | King+Checker vs King+Fisherman | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:checker+dragon` | King+Checker vs King+Dragon | opposed | `kcheckerkdragon.uftb` | **CERTIFIED** | 151,831,680 | concrete | 842,688 [842,688] (3,996,240) / 28,917,719 [1,089,143] (11,031,756) / 4,047,713 [4,047,713] (27,079,724) | 26,748,599 [7,014,929] (11,186,725) / 0 [0] (3,981,336) / 22,596 [22,596] (33,976,584) | 33,808,120 / 42,107,720; 26,771,195 / 49,144,645 | S3 table sha256:aa69192d60b0ef8e7335268bdfab32eb4d74898de8b743661c86c5519a334db5 VersionId 91I06qiWt.iAtfTgp7BCepstxkJlc1Lp; wave certificate sha256:7a8f5800a0cf5313ea9c31f97f5c6d66d7039705d624518391a4bcb3263187c3 VersionId 6wOgGzrnGFzqxNXt0CAThoLU58r3GhuJ; reachability v3 sha256:6d36d0b364d9fc73f1c8980b718c31bff9e0945e258e6a68322c79cf404f8233 VersionId b5CHq5Ff3Q2GZe354bh3r_aN4NFXug1Y |
| `opposed:giant+giant` | King+Giant vs King+Giant | opposed | `kgiantkgiant.uftb` | **CERTIFIED** | 37,957,920 | concrete | 33,948 [15,698] (2,060,572) / 15,770 [28] / 6,820,374 [2,558,514] (10,048,296) | 33,948 [15,698] (2,060,572) / 15,770 [28] / 6,820,374 [2,558,514] (10,048,296) | 6,870,092 / 12,108,868; 6,870,092 / 12,108,868 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:20d086e84c75793bffe1910f54d7c7ce42e376d4d3caa112af14a24abc871743 VersionId Dn_RLSHQrXPYJLMLjYfmN.lLuNxf9PTT; result sha256:192e190c240441b5c1d461b39fe3f93f44377ba8f03391df6c2dfba230f42893 |
| `opposed:giant+copycat` | King+Giant vs King+Copycat | opposed | `kcopycatkgiant.uftb` | **CERTIFIED** | 75,915,840 | concrete | 14,914,280 [6,207,776] (5,611,880) / 34,832 [240] / 4,136,952 [223,616] (13,259,976) | 73,856 [32,824] (5,684,048) / 7,549,984 [7,880] / 11,390,056 [6,894,872] (13,259,976) | 19,086,064 / 18,871,856; 19,013,896 / 18,944,024 | S3 table sha256:a60cdf0c749d52a73aca1041bb33dcdf9161f3e4fd95d1f2ba157d9086133556 VersionId THey1fByFzwgwYuBvrHrpHoGwTSCyg0v; certificate sha256:c5484532bbf66558649449c96613321cd205fe78306ae6aef4f52377d6947f71 VersionId At1iuHa7Bf1EC6nbAVBxoO9JcbLmIrHq; reachability v3 sha256:ba9ed5104ec43b0331598e31f2001dceea89f3b826930fbe31c86f36cedfd106 VersionId .Hdhup8RQVDvHXy5A57CQBHG1MZJv5_n; result sha256:3e12e89432d45f030e68a843332a8757388bcc268228d7b25f27763a490a7f6e |
| `opposed:giant+angel` | King+Giant vs King+Angel | opposed | `kgiantkangel.uftb` | **CERTIFIED** | 75,915,840 | concrete | 41,298 [36,622] (3,132,828) / 11,854 [68] / 23,387,420 [7,481,246] (11,384,520) | 30,520 [2,184] (2,284,232) / 5,514 [2,590] / 24,253,134 [3,497,510] (11,384,520) | 23,440,572 / 14,517,348; 24,289,168 / 13,668,752 | S3 table sha256:ed5ae3e841a55bf481a2e8a97342977450d169d503693f80c8d8e669dee25861 VersionId pmy6u8H5gGPmv8an2tVwuLa62inlz1wQ; certificate sha256:34bb0ea5b4d2e2b587eae7f1e83f668227bb351846c442eafdb811ded7e2d6fa VersionId uDfw0XEBRlHMFxhnPkytTSoCdIF0dQq9; reachability v3 sha256:02f8f35353b91380401ec86022554841a1215866723492abba15909c20e169bd VersionId EKs085UK7vbhZqx86Yu3lmIWQDqKFjOA |
| `opposed:giant+fisherman` | King+Giant vs King+Fisherman | opposed | `kgiantkfisherman.uftb` | **CERTIFIED** | 37,957,920 | concrete | 28,410 [10,554] (3,051,612) / 0 [0] / 10,206,678 [2,308,718] (5,692,260) | 0 [0] (1,313,848) / 12,796 [524] / 11,960,056 [1,786,726] (5,692,260) | 10,235,088 / 8,743,872; 11,972,852 / 7,006,108 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:c6f5f618d59a9f21058fcd93826d64de61d2cb4897c31c2e2be080bf28c0ae8a VersionId ClS3qd_1ck_THm6PMJta9rxDNlNM2a1Y; result sha256:547b564ac58fff439ed090a9f765563f764228be6ae05b6434313707fe49aa32 |
| `opposed:giant+dragon` | King+Giant vs King+Dragon | opposed | `kgiantkdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 29,308 [11,108] (3,051,612) / 7,899,376 [487,698] / 2,306,404 [2,304,644] (5,692,260) | 9,876,212 [5,242,228] (3,346,434) / 12,978 [20] / 51,076 [50,870] (5,692,260) | 10,235,088 / 8,743,872; 9,940,266 / 9,038,694 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:812f3a88542ff0008047c65270205cffa4af9ab446186f830d6652b20a9b5452 VersionId VZHt4LeoEzqQnqKXhj92OzSvHjPaKAQj; result sha256:88b1b6bc867d11eb78cab27c599bbe94db8a32d97edb1a250624982d09a02c72 |
| `opposed:copycat+copycat` | King+Copycat vs King+Copycat | opposed | `kcopycatkcopycat.uftb` | **CERTIFIED** | 75,915,840 | concrete | 10,094,704 [7,403,456] (8,008,736) / 1,629,264 [0] / 15,835,296 [337,200] (2,389,920) | 10,094,704 [7,403,456] (8,008,736) / 1,629,264 [0] / 15,835,296 [337,200] (2,389,920) | 27,559,264 / 10,398,656; 27,559,264 / 10,398,656 | S3 table sha256:55772fcdcd3396d01cc3edf905d9dfc69715e66b5bc93336163a3e77ecb9622a VersionId ciKQoDkxwwFDvNPuOyCWOdf9k7qKXny7; wave certificate sha256:23cf885bdf68cf4d21ff1d61021e2e8baf211ab128ab5a1fc4ff539c3cc21894 VersionId JCSmfG6qwUgMCn4urvCv4ih2AHjAb9jz; reachability v3 sha256:38aec76e89477c55218c4d5c1bc6ce3740a71709123265c3fd9fc018fa81bd4c VersionId txutvcPwX6SnU5O2b8Xm5PJHRRO1bMeZ |
| `opposed:copycat+angel` | King+Copycat vs King+Angel | opposed | `kcopycatkangel.uftb` | **CERTIFIED** | 151,831,680 | concrete | 40,828,336 [18,620,704] (8,294,944) / 28,848 [152] / 23,880,832 [1,816,384] (2,882,880) | 74,496 [8,080] (6,197,952) / 25,877,360 [49,488] / 40,883,152 [10,150,184] (2,882,880) | 64,738,016 / 11,177,824; 66,835,008 / 9,080,832 | S3 table sha256:2b03a2dfccaf05a69f8f6529af33dafbe08bb647afe82c4ebe9d9e0683e6c95a VersionId MqOQP1BBvdLxwGRdcYUY3L6DfOnLd95w; certificate sha256:1da5992bb43a211f012c8a052fc151a09d70af7d611d5b51ec8a5e4d077a6043 VersionId 4daOOaSYnmcPmTzozlNszTBkYNe9_uFf; reachability v3 sha256:df05013a60be0dbf40ee78c825ab895241072c47139451b0cfac9e99eca05763 VersionId i.96LOzGoS.VYytPssbgC9w.LjLsJVci |
| `opposed:copycat+fisherman` | King+Copycat vs King+Fisherman | opposed | — | **DEFERRED** | — | concrete | — | — | — | — |
| `opposed:copycat+dragon` | King+Copycat vs King+Dragon | opposed | `kcopycatkdragon.uftb` | **CERTIFIED** | 75,915,840 | concrete | 5,950,664 [5,907,152] (8,221,984) / 21,954,560 [1,129,184] / 389,272 [352,720] (1,441,440) | 26,998,624 [11,898,808] (9,327,696) / 26,344 [7,640] / 163,816 [163,240] (1,441,440) | 28,294,496 / 9,663,424; 27,188,784 / 10,769,136 | S3 table sha256:485d392dfb731e1d2f5312a2f30a7ff21f2051afc510c0d74b8171539720591e VersionId FRSWhc2_SrDVHlVAJ1vjmFuyOZ2_e5TB; certificate sha256:d05cac549ad5e2c7618eb0d5d0bc67da26a7686b88091528e1e3c599032522f4 VersionId ZwGZdPGu83pJsa9R6FuPdYW_Vkt_SUzL; reachability v3 sha256:368f6107fe0b9241f6ae10140d7a84e53bf4ac7b8b4480c69a6b60b4e050d1a4 VersionId 5nuPoRe1DYMc8pCLJPnn6i_7h.p7e4JM; result sha256:9137d64862b6e6c611962039c061fe89770b08e24ade699500ecb9ee48e51014 |
| `opposed:angel+angel` | King+Angel vs King+Angel | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:angel+fisherman` | King+Angel vs King+Fisherman | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:angel+dragon` | King+Angel vs King+Dragon | opposed | `kangelkdragon.uftb` | **CERTIFIED** | 75,915,840 | concrete | 31,450 [1,828] (3,219,216) / 21,775,034 [548,418] / 12,932,220 [3,003,160] | 27,810,948 [11,369,466] (4,893,140) / 12,118 [28] / 5,241,714 [691,704] | 34,738,704 / 3,219,216; 33,064,780 / 4,893,140 | S3 table sha256:1eb3ca1b50c1aeb5a38f8cda04e550a14d093152e0abdd1c249b714d0eb4fe04 VersionId OvihXMzfMtLBFSvzCtpe2GikKcduOIXs; certificate sha256:b8fd427c4d02182465fb969ef5df9eb4ff8ca3f667cb936107ff3ce5392681cf VersionId s2_bQNKZq8nlMDO0syUTRyf1wECN.sto; reachability v3 sha256:79da05a29b66e4f1e55d05e4e417f03bb03f170733595f03df91c1c8764afd21 VersionId E7l2VZ.wQbQjeGr01j3uWruMEXHsGoXN |
| `opposed:fisherman+fisherman` | King+Fisherman vs King+Fisherman | opposed | — | **DRAW** | — | insufficient material | 0 / 0 / 1 | 0 / 0 / 1 | closed-form draw | — |
| `opposed:fisherman+dragon` | King+Fisherman vs King+Dragon | opposed | `kfishermankdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 0 [0] (1,609,608) / 15,953,310 [674,766] / 1,416,042 [1,416,042] | 14,085,820 [3,642,684] (4,893,140) / 0 [0] / 0 [0] | 17,369,352 / 1,609,608; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:4e08ce65065ac9e3336ec00983655e14811c84ec5f1700dc154605f23fb29ae0 VersionId sCn.7I.6MtB7f9TKY.1GFjLUZ.WEYLP. |
| `opposed:dragon+dragon` | King+Dragon vs King+Dragon | opposed | `kdragonkdragon.uftb` | **CERTIFIED** | 37,957,920 | concrete | 3,622,924 [3,420,502] (4,893,140) / 110,414 [93,236] / 10,352,482 [239,206] | 3,622,924 [3,420,502] (4,893,140) / 110,414 [93,236] / 10,352,482 [239,206] | 14,085,820 / 4,893,140; 14,085,820 / 4,893,140 | S3 legacy archive sha256:e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263 VersionId YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ; preservation certificate sha256:1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d VersionId 5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY; reachability v3 sha256:8ee2b68a70716b68f73e5599696fcefaea1434b79ee61ab584fd9e5eddd24564 VersionId ytf5ytyg0knbufo9seoDCi0y6EyduOUt |
<!-- COMPUTATION_LEDGER_END -->

## Certified result details

<!-- GENERATED_TABLE_START -->
| File | Class | In-class edges | First material owner starts W / L / D | Second material owner / bare King starts W / L / D | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `kjesterk.uftb` | King+Jester vs King | 9,169,752 | 412,616 [0] (80,344) / 0 [0] / 0 [0] | 3,272 [3,272] / 414,344 [0] / 75,344 [75,344] | `3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa` |
| `kpawnk.uftb` | King+Pawn vs King | 12,759,470 | 576,806 [124,802] (101,696) / 0 [0] / 217,258 [2,790] (90,160) | 0 [0] (83,616) / 459,272 [60] / 352,872 [68,464] (90,160) | `42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844` |
| `kqk.uftb` | King+Queen vs King | 15,744,492 | 306,404 [0] (186,556) / 0 [0] / 0 [0] | 0 [0] (41,808) / 413,304 [0] / 37,848 [37,848] | `1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3` |
| `krk.uftb` | King+Rook vs King | 11,970,912 | 361,648 [0] (131,312) / 0 [0] / 0 [0] | 0 [0] (41,808) / 414,300 [0] / 36,852 [36,852] | `abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44` |
| `kberserkerk.uftb` | King+Berserker vs King | 92,321,028 | 1,468,376 [0] (3,461,224) / 0 [0] / 0 [0] | 0 [0] (418,080) / 4,143,200 [0] / 368,320 [368,320] | `f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1` |
| `kbombk.uftb` | King+Bomb vs King | 9,827,604 | 394,988 [0] (97,972) / 0 [0] / 0 [0] | 0 [0] (41,808) / 448,600 [0] (1,760) / 792 [792] | `18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f` |
| `kninjak.uftb` | King+Ninja vs King | 12,610,592 | 356,360 [0] (136,600) / 0 [0] / 0 [0] | 0 [0] (41,808) / 413,376 [0] / 37,776 [37,776] | `4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776` |
| `kghostk.uftb` | King+Ghost vs King | 17,761,144 | 863,768 [38,536] (122,152) / 0 [0] / 0 [0] | 0 [0] (83,616) / 826,992 [0] (38,520) / 36,776 [36,776] (16) | `3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31` |
| `kpenguink.uftb` | King+Penguin vs King | 9,938,528 | 1,264 [0] (52,024) / 192 [0] / 491,504 [0] (1,426,856) | 796 [0] (45,080) / 3,272 [0] / 530,700 [78,584] (1,391,992) | `5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd` |
| `kparasitek.uftb` | King+Parasite vs King | 8,646,512 | 412,616 [0] (80,344) / 0 [0] / 0 [0] | 0 [0] (41,808) / 451,120 [0] / 32 [32] | `c53364f87ce4ff23372aa3565d279e9fadde706eb1aeca5695aecc1ea3e83047` |
| `ksniperk.uftb` | King+Sniper vs King | 24,180,908 | 5,014 [0] (194,552) / 0 [0] / 872,222 [16] (900,052) | 0 [0] (167,232) / 1,210 [0] (1,066) / 901,094 [73,572] (901,238) | `473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5` |
| `kprincek.uftb` | King+Prince vs King | 14,324,280 | 412,616 [0] (80,344) / 0 [0] / 0 [0] | 0 [0] (41,808) / 414,344 [0] / 36,808 [36,808] | `7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0` |
| `kgiantk.uftb` | King+Giant vs King | 4,747,504 | 3,300 [0] (82,476) / 0 [0] / 273,324 [0] (133,860) | 0 [0] (30,868) / 1,460 [0] / 326,772 [43,572] (133,860) | `eb52f2c08cf88e1e3682d0c72dfde191d9009e79779ad7eee23ca82fdcade591` |
| `kcopycatk.uftb` | King+Copycat vs King | 10,685,864 | 372,224 [0] (108,184) / 0 [0] / 72 [0] (12,480) | 0 [0] (40,776) / 374,136 [0] / 65,568 [65,352] (12,480) | `98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb` |
| `kdragonk.uftb` | King+Dragon vs King | 11,904,156 | 364,756 [0] (128,204) / 0 [0] / 0 [0] | 0 [0] (41,808) / 414,164 [0] / 36,988 [36,988] | `28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6` |
| `kjesterbishopk.uftb` | King+Jester+Bishop vs King | 504,293,320 | 13,965,588 [0] (5,013,372) / 0 [0] / 0 [0] | 316,578 [316,578] / 16,113,394 [1,253,408] / 2,548,988 [2,548,988] | `5816410c814d49a27dac924a0f5beb1b792b61b742a2fc83f0411bf10d15eae0` |
| `kjesterbombk.uftb` | King+Jester+Bomb vs King | 512,779,944 | 13,901,148 [0] (5,077,812) / 0 [0] / 0 [0] | 309,574 [309,574] / 17,331,368 [1,246,130] / 1,338,018 [1,338,018] | `5fdb70947be34ad506f893291c4657a4b0f850e39672caae6e564a1df71486fb` |
| `kjesterdragonk.uftb` | King+Jester+Dragon vs King | 596,943,520 | 12,877,956 [0] (6,101,004) / 0 [0] / 0 [0] | 431,414 [431,414] / 17,350,088 [2,416,786] / 1,197,458 [1,197,458] | `3553fad7d75c3c428bbf0d9333801d57cb3dae0a99bc505ef1bf45de1f7cc017` |
| `kjesterfishermank.uftb` | King+Jester+Fisherman vs King | 796,241,336 | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 125,972 [125,972] / 15,952,244 [1,247,388] / 2,900,744 [2,900,744] | `3f335332d457d184fc3a20b5ddddf2806911bb5e5bfd607872eb757a2a90a968` |
| `kjestergiantk.uftb` | King+Jester+Giant vs King | 259,948,596 | 9,347,536 [0] (3,939,164) / 0 [0] / 0 [0] (5,692,260) | 260,316 [260,316] / 11,300,932 [1,534,228] / 1,725,452 [1,725,452] (5,692,260) | `36fce15f18bf2d78f16e9599dc351b2cecf66b9147ccb315eb1e10af364ffebc` |
| `kjesterkbishop.uftb` | King+Jester vs King+Bishop | 489,530,528 | 2,506,936 [2,485,492] (3,093,244) / 0 [0] / 13,378,780 [195,360] | 488,456 [488,456] / 10,706 [2,560] / 18,479,798 [6,212,808] | `698f617b02ec063f481f57dac1b3677e7d9db618ca006dac59a3105c872dfd43` |
| `kjesterkbomb.uftb` | King+Jester vs King+Bomb | 494,687,920 | 95,410 [598] (3,152,980) / 160 [160] (62,344) / 15,668,066 [1,476,958] | 3,278,154 [3,269,368] / 29,024 [28] / 15,671,782 [3,478,812] | `b01b14d0a5d920e9c519c1c9a310882c7aec263c8a7aa71c2abbefb3202dc8cb` |
| `kjesterkdragon.uftb` | King+Jester vs King+Dragon | 573,650,240 | 2,493,996 [2,484,272] (3,093,244) / 6,688,516 [270,664] / 6,703,204 [205,840] | 15,063,608 [7,968,902] / 5,426 [1,296] / 3,909,926 [567,110] | `55477de8dd1263ab734582dea3409af5409fa704bf35088d4ab4300beee7a5fb` |
| `kjesterkfisherman.uftb` | King+Jester vs King+Fisherman | 723,303,740 | 2,499,754 [2,487,404] (3,093,244) / 0 [0] / 13,385,962 [115,604] | 125,972 [125,972] / 11,842 [6,796] / 18,841,146 [2,901,032] | `ce378df3a2be9b6fe0dcb82098a88abc7067ed1cde02821ca744af46dc461df0` |
| `kjesterkgiant.uftb` | King+Jester vs King+Giant | 265,285,348 | 10,988,108 [2,773,676] (2,193,308) / 32 [24] / 105,252 [81,116] (5,692,260) | 516,144 [516,092] / 7,775,374 [2,262] / 4,995,182 [4,850,872] (5,692,260) | `0c0d2aa723badd35e14708cfecfaeeb0c8039ef4c76f741a43b60709ff08c615` |
| `kjesterkknight.uftb` | King+Jester vs King+Knight | 432,640,208 | 2,898,404 [2,487,458] (3,093,244) / 0 [0] / 12,987,312 [188,486] | 334,432 [334,432] / 121,472 [4,148] / 18,523,056 [4,823,412] | `cf6d15681c176ff90cd7c862187b5d1ab4db217c6b00468f032f57c817155d69` |
| `kjesterkmage.uftb` | King+Jester vs King+Mage | 364,406,212 | 15,885,716 [2,603,008] (3,093,244) / 0 [0] / 0 [0] | 125,972 [125,972] / 15,953,304 [2,614,380] / 2,899,684 [2,899,684] | `705b00311fc61e20615b79e13472b96cf7705f8e80227303877ec0445326d274` |
| `kjesterknightk.uftb` | King+Jester+Knight vs King | 441,170,696 | 14,798,084 [0] (4,180,876) / 0 [0] / 0 [0] | 242,136 [242,136] / 16,055,072 [1,275,868] / 2,681,752 [2,681,752] | `4d9e7cd0ad349ef4ddc10f214ba3d5e63000d551b307a4e0e5266ee3950de9e6` |
| `kjesterkninja.uftb` | King+Jester vs King+Ninja | 602,269,760 | 2,499,692 [2,483,440] (3,093,244) / 5,971,304 [170,168] / 7,414,720 [202,112] | 14,621,888 [8,538,104] / 5,680 [0] / 4,351,392 [469,224] | `7255da99637afb9a015c284056b4be3431d602c6c0febddb7fd439157170a55b` |
| `kjesterkparasite.uftb` | King+Jester vs King+Parasite | 450,905,824 | 256,428 [1,656] (3,093,244) / 4,476,908 [697,670] / 11,152,380 [764,870] | 10,452,700 [5,472,048] / 87,668 [0] / 8,438,592 [164,468] | `45f0f26f182a2e231455df61931cf49d6a54d9b36f9be0e1fd9fd87117a7fd18` |
| `kjesterkqueen.uftb` | King+Jester vs King+Queen | 712,427,800 | 2,489,288 [2,483,440] (3,093,244) / 7,339,356 [639,948] / 6,057,072 [254,204] | 16,116,718 [10,906,806] / 2,682 [0] / 2,859,560 [730,212] | `1345e2b73a1978a6569821a379c236415997ff3b9cdb108563c982f458d88600` |
| `kjesterkrook.uftb` | King+Jester vs King+Rook | 571,417,768 | 2,491,348 [2,483,448] (3,093,244) / 518,380 [241,048] / 12,875,988 [174,092] | 9,387,182 [8,186,390] / 4,634 [916] / 9,587,144 [560,872] | `ba1a6ecd7e71b6e3fbe1bff9a1576c0d4a9d0cc2f241aefd48ea519959ef984d` |
| `kjesterkturtle.uftb` | King+Jester vs King+Turtle | 402,759,128 | 8,899,984 [2,484,656] (3,093,244) / 0 [0] / 6,985,732 [118,352] | 264,054 [264,054] / 5,356,036 [2,080] / 13,358,870 [4,181,524] | `7216f1e140c32fc3ed69addce02b651f64ec3fef5173f7b0c11259fcc38ccebd` |
| `kjestermagek.uftb` | King+Jester+Mage vs King | 386,478,416 | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 125,972 [125,972] / 15,952,244 [1,247,388] / 2,900,744 [2,900,744] | `9d48c76c568315e9ff533fe3d518f1c7fe528c421891b65b1e171657f2ed7c09` |
| `kjesterninjak.uftb` | King+Jester+Ninja vs King | 629,742,840 | 12,548,400 [0] (6,430,560) / 0 [0] / 0 [0] | 446,484 [446,484] / 17,313,736 [2,393,936] / 1,218,740 [1,218,740] | `33431727e988e9c2f48dc0f22e3418072fc8c6f741290b037fd1a5e28b0bbf97` |
| `kjesterparasitek.uftb` | King+Jester+Parasite vs King | 462,590,064 | 14,519,136 [0] (4,459,824) / 0 [0] / 0 [0] | 247,372 [247,372] / 17,365,128 [1,304,236] / 1,366,460 [1,366,460] | `50d5e2a3a35f7746b27395ebeb922dd9583b10800b59a6870c18750416ac5630` |
| `kjesterqueenk.uftb` | King+Jester+Queen vs King | 751,025,764 | 10,877,572 [0] (8,101,388) / 0 [0] / 0 [0] | 611,994 [611,994] / 17,307,456 [2,241,402] / 1,059,510 [1,059,510] | `b1e938905b645a70b2a2de433eb0ea12dc1374ad0bdef79738f115b65adfcb74` |
| `kjesterrookk.uftb` | King+Jester+Rook vs King | 595,252,940 | 12,797,700 [0] (6,181,260) / 0 [0] / 0 [0] | 425,372 [425,372] / 17,360,968 [2,409,632] / 1,192,620 [1,192,620] | `9e485cf5516add2e833527fbce24374380f444322b0db1e5a41e687bda500c82` |
| `kjesterturtlek.uftb` | King+Jester+Turtle vs King | 409,044,272 | 15,158,912 [0] (3,820,048) / 0 [0] / 0 [0] | 189,426 [189,426] / 16,006,830 [1,247,388] / 2,782,704 [2,782,704] | `496ab5eb6012ff5500b54b6589d153710839563621cbb4340e6f69534a0d3067` |
| `kknightknightk.uftb` | King+2 Knights vs King | 196,460,680 | 360 [0] (1,964,462) / 0 [0] / 7,524,658 [0] | 0 [0] (804,804) / 68 [0] / 8,684,608 [1,272,198] | `5c95ba0ed74d95e4d2c2fa4d1a98c1dddb121a9d11406853c75d4d5e1ba15138` |
| `kknightqueenk.uftb` | King+Knight+Queen vs King | 674,814,946 | 11,144,698 [0] (7,834,262) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,991,680 [965,646] / 1,377,672 [1,377,672] | `5a2143cee6221564e0acaf53e5d0f5fd764f2997445fc8bbc490deb79c5693ba` |
| `kknightkqueen.uftb` | King+Knight vs King+Queen | 616,478,182 | 0 [0] (2,808,960) / 13,559,860 [1,399,772] / 2,610,140 [2,348,088] | 11,888,556 [4,601,386] (7,035,616) / 0 [0] / 54,788 [54,780] | `c708a791064b0b9f45bd506b3cfc1657912890dd1fb080533d311db6c3418750` |
| `kknightrookk.uftb` | King+Knight+Rook vs King | 533,160,532 | 13,069,652 [0] (5,909,308) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,041,948 [1,117,060] / 1,327,404 [1,327,404] | `04513bc582ac66c262ce0a50aadc7dc4a103cf281b5ae66aa06a73fd4a78d457` |
| `kknightkrook.uftb` | King+Knight vs King+Rook | 497,289,662 | 16 [0] (2,808,960) / 1,871,092 [468,980] / 14,298,892 [2,507,944] | 6,652,526 [3,546,108] (4,951,436) / 4 [0] / 7,374,994 [344,664] | `4f1655eb31c7a335de951bcc217a2e01b777872b2fc867fead6d42ed3f255da8` |
| `kknightbishopk.uftb` | King+Knight+Bishop vs King | 450,616,302 | 14,199,500 [0] (4,733,914) / 0 [0] / 45,546 [0] | 0 [0] (1,609,608) / 14,751,040 [0] / 2,618,312 [2,503,582] | `e78a30ba8605cccbbc3c9198dd717843ef80115905b7ea05b03f130756d54f48` |
| `kknightbombk.uftb` | King+Knight+Bomb vs King | 457,027,800 | 14,179,200 [4] (4,799,760) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,269,780 [1,244,268] (67,760) / 31,812 [31,812] | `6c1e33d2b92fe411485a2370fb18621c86d5e26de45a3deb053f0a4547312d72` |
| `kknightkbomb.uftb` | King+Knight vs King+Bomb | 431,449,080 | 264 [0] (2,903,040) / 14,748,000 [362,658] (62,768) / 1,264,888 [930,332] | 15,062,940 [2,903,796] (3,846,020) / 52 [0] / 69,944 [69,932] (4) | `64f63059b0d87f4b28bf8d30c0a1c3354ed42441fd6b84c6a1be26263aa9afde` |
| `kknightninjak.uftb` | King+Knight+Ninja vs King | 564,591,464 | 12,797,612 [0] (6,181,348) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,000,796 [1,118,180] / 1,368,556 [1,368,556] | `ecb39a6f313de06083df9552b6b46e291fdfda4f04c4536744096eea5615a3fa` |
| `kknightkninja.uftb` | King+Knight vs King+Ninja | 524,358,488 | 0 [0] (2,808,960) / 13,522,364 [560,982] / 2,647,636 [2,348,060] | 13,657,124 [3,748,212] (5,259,100) / 0 [0] / 62,736 [62,724] | `c72d856b445ae577e4394e9b0fe4ec3589a3c0fd55bd991d037a709bad688e78` |
| `kknightturtlek.uftb` | King+Knight+Turtle vs King | 363,952,568 | 14,093,700 [0] (3,538,608) / 0 [0] / 1,346,652 [0] | 0 [0] (1,609,608) / 12,261,896 [0] / 5,107,456 [2,626,112] | `e51001e72d79078c3da96de0dab87d8752d5f7496621b4b3812f82624107c5df` |
| `kknightparasitek.uftb` | King+Knight+Parasite vs King | 412,737,272 | 14,798,084 [0] (4,180,876) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,364,220 [1,304,236] / 5,132 [5,132] | `80703fb82f8caf5589e4e3d07bec5f6b4ce78dec12ce44d7a7f7e07d5e127246` |
| `kknightkparasite.uftb` | King+Knight vs King+Parasite | 396,499,600 | 20 [0] (2,808,960) / 14,791,096 [195,644] / 1,378,884 [1,054,596] | 15,817,696 [2,543,260] (3,093,244) / 4 [0] / 68,016 [67,996] | `c9c65bc38098b46e30464eabc4816c9d4e68c498b0876c7dca2df59103257c04` |
| `kknightgiantk.uftb` | King+Knight+Giant vs King | 228,702,604 | 7,589,222 [0] (3,665,112) / 0 [0] / 2,032,366 [0] (5,692,260) | 0 [0] (1,142,116) / 7,416,466 [4,796] / 4,728,118 [2,374,612] (5,692,260) | `190eeb250445920f4027a7d8a20421c5200b26bd44972d94b5a489d246a87f32` |
| `kknightkgiant.uftb` | King+Knight vs King+Giant | 215,826,804 | 0 [0] (1,968,832) / 19,388 [24] / 11,298,480 [4,289,964] (5,692,260) | 37,890 [13,190] (3,051,612) / 0 [0] / 10,197,198 [2,410,594] (5,692,260) | `79d484eae6e15ed424647a3c1e1501ab06bb88d502be6b98b3a6c62cd2bc38ce` |
| `kknightdragonk.uftb` | King+Knight+Dragon vs King | 534,668,038 | 13,125,078 [0] (5,853,878) / 0 [0] / 4 [0] | 0 [0] (1,609,608) / 16,039,998 [1,078,918] / 1,329,354 [1,329,346] | `5a186985b6ca6e024562e2a0cff6dbe34003e30903761bae751400135abeb01c` |
| `kknightkdragon.uftb` | King+Knight vs King+Dragon | 498,929,176 | 0 [0] (2,808,960) / 13,805,686 [467,032] / 2,364,314 [2,348,254] | 14,041,436 [3,637,916] (4,893,140) / 0 [0] / 44,384 [44,192] | `22a14009f6d4f456eddcb659d3a5c1d99170ff4c706a517b786357abaecddabb` |
| `kqueenqueenk.uftb` | King+2 Queens vs King | 480,508,116 | 3,991,466 [0] (5,498,014) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,560,812 [951,406] / 123,864 [123,864] | `953ef23458f3681680b5d58736d854930218e59dea4961d8ddec5632108f1b21` |
| `kqueenkqueen.uftb` | King+Queen vs King+Queen | 701,876,100 | 4,619,002 [4,275,534] (7,035,616) / 47,930 [31,796] / 7,276,412 [369,904] | 4,619,002 [4,275,534] (7,035,616) / 47,930 [31,796] / 7,276,412 [369,904] | `41421585773f70182648370f85b441f6bb7ad39a43a1d9926b249a3c4f435fe1` |
| `kqueenrookk.uftb` | King+Queen+Rook vs King | 816,632,332 | 9,474,332 [0] (9,504,628) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,251,240 [2,077,062] / 118,112 [118,112] | `8674db75197f632ee8a9a0740b3569a49082929879fbd79ea70fe30efd10eaea` |
| `kqueenkrook.uftb` | King+Queen vs King+Rook | 656,954,204 | 11,847,368 [4,577,736] (7,035,616) / 24,998 [21,462] / 70,978 [59,018] | 3,696,724 [3,522,564] (4,951,436) / 9,667,734 [556,738] / 663,066 [275,288] | `e3616069bee07342fda4eed31660b8504ca8c0ac149038c8bbc35e10beb89a90` |
| `kqueenbishopk.uftb` | King+Queen+Bishop vs King | 732,953,056 | 10,451,944 [0] (8,527,016) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,034,380 [937,166] / 1,334,972 [1,334,972] | `8ce01962e4b7dde87d17de918bfe89ce50337bd353de2cd19ec8c2b99739774e` |
| `kqueenkbishop.uftb` | King+Queen vs King+Bishop | 633,491,052 | 11,915,826 [4,592,488] (7,035,616) / 0 [0] / 27,518 [27,518] | 0 [0] (3,693,788) / 12,200,676 [841,966] / 3,084,496 [3,007,772] | `3cb84ad6176d504554d46a67b62a03853d8590e6940ed9d66a6e983e0e2594f6` |
| `kqueenbombk.uftb` | King+Queen+Bomb vs King | 738,060,636 | 10,388,902 [0] (8,590,058) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,201,128 [1,244,268] (67,760) / 100,464 [100,464] | `a75bcbfedb3e5fc4f35d711e81b50db0b23d93ef232bdf1ee5b15c44787cf5d9` |
| `kqueenkbomb.uftb` | King+Queen vs King+Bomb | 629,403,948 | 1,421,172 [423,388] (7,353,252) / 1,282,090 [100,374] (46,244) / 8,873,510 [2,884,652] (2,692) | 3,611,678 [1,484,816] (3,842,796) / 670,572 [46,234] (528) / 10,850,686 [2,053,184] (2,700) | `7317a875224cf62e25de4b51c3bcabb3720fce8c0ebdab45e1db2cd2f4db9330` |
| `kqueenninjak.uftb` | King+Queen+Ninja vs King | 849,027,988 | 9,342,106 [0] (9,636,854) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,178,972 [2,055,346] / 190,380 [190,380] | `d9b0d5151b48d5137566ed0a0d7364af0122b1b68adfcc6d946afc9a31881f74` |
| `kqueenkninja.uftb` | King+Queen vs King+Ninja | 672,276,532 | 7,529,430 [4,275,858] (7,035,616) / 9,456 [304] / 4,404,458 [335,746] | 3,622,556 [3,581,154] (5,259,100) / 1,365,734 [278,624] / 8,731,570 [251,206] | `d406e996eabed529e140e6e31c93a5841561e412ce0350f8e36d169f0ddc8e4c` |
| `kqueenturtlek.uftb` | King+Queen+Turtle vs King | 644,511,080 | 11,369,834 [0] (7,609,126) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,954,050 [937,166] / 1,415,302 [1,415,302] | `c2a325eccaf941235c632d386beac95de4cdcc22b2005511b80e538f3353f39a` |
| `kqueenkturtle.uftb` | King+Queen vs King+Turtle | 606,531,770 | 11,943,312 [4,601,370] (7,035,616) / 0 [0] / 32 [32] | 0 [0] (2,397,164) / 14,534,402 [1,159,532] / 2,047,394 [2,047,314] | `353627128552df82c4eece2999e8e6805a11b29a418078c7f4f255f5e0ec55fe` |
| `kqueenmagek.uftb` | King+Queen+Mage vs King | 623,307,860 | 11,943,344 [280] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,912,942 [937,166] / 1,456,410 [1,456,410] | `de3e8f1a0b0d1d5f216c8be8d4ab9efccf8a7a09a4c803175a4e6651c451a22e` |
| `kqueenkmage.uftb` | King+Queen vs King+Mage | 600,512,500 | 11,943,344 [4,601,402] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,938,592 [5,918,152] / 1,430,760 [1,430,760] | `88a74961c77c7da83eded1c1d716037ffbe71a46746849651c3a308141d4b2f4` |
| `kqueenparasitek.uftb` | King+Queen+Parasite vs King | 694,110,316 | 10,877,572 [0] (8,101,388) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,307,456 [1,304,236] / 61,896 [61,896] | `6a29b00ab2a1edd510e94e67d5ca70d6da108b15bc9d9a3edd54b547d2b8327c` |
| `kqueenkparasite.uftb` | King+Queen vs King+Parasite | 622,639,914 | 2,353,096 [585,500] (7,035,616) / 794,236 [61,178] / 8,796,012 [2,192,962] | 3,740,612 [2,510,592] (3,093,244) / 1,561,098 [82,446] / 10,584,006 [572,746] | `f088d7d7937b6efed0cc69eec38fda1448bbb0351d135b9fb07027a9ad578d39` |
| `kqueengiantk.uftb` | King+Queen+Giant vs King | 411,446,456 | 7,071,014 [0] (6,215,686) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,256,584 [1,309,252] / 888,000 [888,000] (5,692,260) | `a9b0135f35a657e5192f939a57f0f81a2af90cefaf34847d97e7b5e592550937` |
| `kqueenkgiant.uftb` | King+Queen vs King+Giant | 358,244,540 | 8,456,120 [5,941,250] (4,821,246) / 7,090 [0] / 2,244 [2,218] (5,692,260) | 20,818 [8,038] (3,051,612) / 7,904,922 [1,215,392] / 2,309,348 [2,307,354] (5,692,260) | `34a72a3b8cf5660a35905e91dd4d311bf0738b035f3889c57f577d0a8606c93b` |
| `kqueenfishermank.uftb` | King+Queen+Fisherman vs King | 998,335,056 | 11,943,344 [0] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,912,942 [937,166] / 1,456,410 [1,456,410] | `d8999ba65bae1875637e5cc4e5b7ad857bb178533a7f41751cbdadce731f1c38` |
| `kqueenkfisherman.uftb` | King+Queen vs King+Fisherman | 870,038,166 | 11,943,344 [4,601,402] (7,035,616) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,953,468 [1,514,416] / 1,415,884 [1,415,884] | `c321eebe57da1b0b119a42001f06b98d1353528567fc9c6dffcd09b95c27e3b2` |
| `kqueendragonk.uftb` | King+Queen+Dragon vs King | 819,198,846 | 9,653,298 [0] (9,325,662) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,238,014 [2,100,544] / 131,338 [131,338] | `59dbc00aeff1770fdae48bb5ebeb6db85c4318e17d95f1fa531cc4fcf4c9f32e` |
| `kqueenkdragon.uftb` | King+Queen vs King+Dragon | 661,400,078 | 7,965,806 [4,276,854] (7,035,616) / 108,042 [98,604] / 3,869,496 [372,202] | 4,290,082 [3,407,090] (4,893,140) / 1,437,880 [536,946] / 8,357,858 [469,644] | `c4cf5caf8667b03528602cfa24f1b35e13bd1d038c5791fb028196965988d7cd` |
| `krookrookk.uftb` | King+2 Rooks vs King | 336,683,762 | 5,565,392 [0] (3,924,088) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,669,238 [1,114,238] / 15,438 [15,438] | `6ad45d2181b8bb5e7780e8bb87b654970778d108405c43d21c06954ed287a635` |
| `krookkrook.uftb` | King+Rook vs King+Rook | 574,799,708 | 3,748,496 [3,522,794] (4,951,436) / 85,386 [75,764] / 10,193,642 [336,990] | 3,748,496 [3,522,794] (4,951,436) / 85,386 [75,764] / 10,193,642 [336,990] | `74424fbc58c6b6e837780bed913ada2eb256cdcaf27fb53162c4b84e1e9d763e` |
| `krookbishopk.uftb` | King+Rook+Bishop vs King | 591,300,396 | 12,371,072 [0] (6,607,888) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,100,204 [1,111,416] / 1,269,148 [1,269,148] | `0c5930e4043e0581648739b7d5fc499ce18daec8d18e19f04836f27ca49cc2ad` |
| `krookkbishop.uftb` | King+Rook vs King+Bishop | 530,190,084 | 4,582,954 [3,527,016] (4,951,436) / 0 [0] / 9,444,570 [437,554] | 0 [0] (3,693,788) / 423,770 [357,544] / 14,861,402 [3,173,920] | `ce2f769cedcf748f0cf45fb78eb7f642c5df51e01f2aad7263a65238ab82bc56` |
| `krookbombk.uftb` | King+Rook+Bomb vs King | 596,543,578 | 12,221,516 [0] (6,757,444) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,242,972 [1,244,268] (67,760) / 58,620 [58,620] | `6af34f7ec80a32ea17a32a38a3ce217e620a3883a5d637e7077706c1b5260443` |
| `krookkbomb.uftb` | King+Rook vs King+Bomb | 529,230,264 | 91,214 [14,144] (5,176,766) / 2,816,466 [204,232] (54,846) / 10,838,228 [2,410,652] (1,440) | 6,144,700 [1,763,914] (3,844,836) / 15,106 [0] (8) / 8,973,130 [1,396,440] (1,180) | `8c427bef832972b822da38aedd9c2acbb86fe8f4e1a615c84644cf0e729f5121` |
| `krookninjak.uftb` | King+Rook+Ninja vs King | 705,763,068 | 11,008,382 [0] (7,970,578) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,271,872 [2,229,596] / 97,480 [97,480] | `a19e735be310cbf8caa6e8397d8202d134c16a51f9f5de486c3f4f56583584ea` |
| `krookkninja.uftb` | King+Rook vs King+Ninja | 597,070,432 | 3,833,886 [3,522,988] (4,951,436) / 275,140 [172,410] / 9,918,498 [335,076] | 4,742,502 [3,585,318] (5,259,100) / 131,520 [118,418] / 8,845,838 [215,140] | `f3b3c7caa225f800a73cdb302dec38601f02bd805700a1cbb001b41844b23492` |
| `krookturtlek.uftb` | King+Rook+Turtle vs King | 503,391,432 | 13,353,634 [0] (5,625,326) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,000,700 [1,088,580] / 1,368,652 [1,368,652] | `e2752a01a8c10a08344f8aa51373a2913a5c0175b48a476baccb907e7fc77526` |
| `krookkturtle.uftb` | King+Rook vs King+Turtle | 480,007,102 | 14,003,804 [3,771,810] (4,951,436) / 4 [0] / 23,716 [23,688] | 4 [4] (2,397,164) / 14,534,220 [447,420] / 2,047,572 [2,047,322] | `9250b5addb8e8b775422b8eb5d562337847f03ea08fe21cc660e2b7849045819` |
| `krookmagek.uftb` | King+Rook+Mage vs King | 482,774,292 | 14,027,524 [32,974] (4,951,436) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,950,608 [1,088,580] / 1,418,744 [1,418,744] | `b1704edd22b133cf0e3b036a69990353ce923d10f524aa3802d2ce54d03326ca` |
| `krookkmage.uftb` | King+Rook vs King+Mage | 462,063,112 | 14,027,524 [3,795,146] (4,951,436) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,952,876 [4,121,264] / 1,416,476 [1,416,476] | `f7f3e7b23902188d6c0e47673f8c59b483656de94486fa52a0affcb6ced4abd7` |
| `krookparasitek.uftb` | King+Rook+Parasite vs King | 552,591,676 | 12,797,700 [0] (6,181,260) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,360,968 [1,304,236] / 8,384 [8,384] | `91ca9a8f1a35291a384b84a2813e29433b331cb354039cd992d860cad7ac3c19` |
| `krookkparasite.uftb` | King+Rook vs King+Parasite | 508,640,722 | 100,882 [14,972] (4,951,436) / 13,479,612 [2,402,244] / 447,030 [81,460] | 15,715,312 [2,836,440] (3,093,244) / 17,690 [0] / 152,714 [3,442] | `05d30182c25ad00379ab4e5c1d51223cb108ee133c30d00efc89faa012e9aed2` |
| `krookgiantk.uftb` | King+Rook+Giant vs King | 321,003,062 | 8,246,894 [0] (5,039,806) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,296,414 [1,380,932] / 848,170 [848,170] (5,692,260) | `0ac2b5d3a1ef4c6c0d39c2cf7b7b6fd857d38319dc4607e8183418f38979a4a8` |
| `krookkgiant.uftb` | King+Rook vs King+Giant | 287,592,138 | 9,775,420 [4,480,382] (3,443,644) / 13,840 [4] / 53,796 [53,044] (5,692,260) | 30,694 [11,182] (3,051,612) / 7,895,702 [362,660] / 2,308,692 [2,304,166] (5,692,260) | `6e099716aa2a1115445af30f913b8e502f7e66ec8b63046da19b1f9f90c242fb` |
| `krookfishermank.uftb` | King+Rook+Fisherman vs King | 857,801,488 | 14,027,524 [2,704] (4,951,436) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,950,608 [1,088,580] / 1,418,744 [1,418,744] | `093944252b6b71ad34c31a006493a6bb6ab46fef02238e6f1d4714dacf0e6707` |
| `krookkfisherman.uftb` | King+Rook vs King+Fisherman | 778,458,154 | 4,422,744 [3,529,374] (4,951,436) / 0 [0] / 9,604,780 [265,772] | 0 [0] (1,609,608) / 461,572 [400,922] / 16,907,780 [1,687,846] | `bd7c952e4779e7b9fd1e035dc1e9eabcd2202c51f0e96b0dfbe38bdd20b713a9` |
| `krookdragonk.uftb` | King+Rook+Dragon vs King | 676,425,340 | 11,413,200 [0] (7,565,760) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,325,054 [2,274,794] / 44,298 [44,298] | `7705efb335912c0ed7c8f7dd7c1ac63ea6cce8f353ce5f8f4799d8a09aa4f70c` |
| `krookkdragon.uftb` | King+Rook vs King+Dragon | 579,444,158 | 4,376,160 [3,523,156] (4,951,436) / 437,638 [381,108] / 9,213,726 [363,058] | 5,782,264 [3,460,060] (4,893,140) / 324,630 [313,032] / 7,978,926 [350,324] | `59d1e0f5d1a560efbd24fde36c9d5d6947e64ec50509b5500b56cff1b28644d8` |
| `kbishopbishopk.uftb` | King+2 Bishops vs King | 130,467,458 | 3,306,336 [0] (1,498,130) / 0 [0] / 3,376,686 [0] (1,308,328) | 0 [0] (407,502) / 3,708,210 [0] / 4,976,466 [689,058] (397,302) | `18ee411f10c1c62e365b9e30359a1403f5d34e827bcc5509f20d8c0e73ded490` |
| `kbishopbombk.uftb` | King+Bishop+Bomb vs King | 514,356,290 | 13,383,238 [0] (5,595,722) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,265,800 [1,244,268] (67,760) / 35,792 [35,792] | `7551d93b34162827fa0b35578b4c6a4d9acd181bddcaa31582ab1361756764b1` |
| `kbishopkbomb.uftb` | King+Bishop vs King+Bomb | 471,646,156 | 116 [0] (3,850,520) / 13,139,856 [246,048] (59,240) / 1,928,028 [1,599,610] (1,200) | 14,983,656 [2,853,360] (3,845,632) / 24 [0] (16) / 149,256 [148,476] (376) | `8f3edd16e534811b61a4dcd792f2bb7a26e7c6f43fb0397c9c3696de56d0ddbe` |
| `kbishopninjak.uftb` | King+Bishop+Ninja vs King | 622,467,416 | 12,053,584 [0] (6,925,376) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,048,930 [1,089,700] / 1,320,422 [1,320,422] | `25afeb53b8ab30386bf683f20c1769220d7e3b1c1356e543c60f1f260ad58985` |
| `kbishopkninja.uftb` | King+Bishop vs King+Ninja | 554,408,596 | 0 [0] (3,693,788) / 12,195,462 [302,314] / 3,089,710 [3,007,110] | 13,679,960 [3,741,216] (5,259,100) / 0 [0] / 39,900 [39,900] | `cd948abb01bd00c51ffdf0c4aebb46394b4f3052e6b86073c8b4e327fa813f22` |
| `kbishopturtlek.uftb` | King+Bishop+Turtle vs King | 421,326,784 | 14,555,124 [0] (4,380,964) / 0 [0] / 42,872 [0] | 0 [0] (1,609,608) / 14,694,114 [0] / 2,675,238 [2,557,792] | `356dc50f942ccd939ea3a1364b4481691b4abf9bb4338c65fef015b567bea827` |
| `kbishopmagek.uftb` | King+Bishop+Mage vs King | 400,784,944 | 0 [0] (3,693,788) / 0 [0] / 15,285,172 [605,244] | 0 [0] (1,609,608) / 0 [0] / 17,369,352 [2,578,596] | `4b32c84e4e56475370f5439dcd28b75a743e86531c8f27c4b652d47b722b767a` |
| `kbishopparasitek.uftb` | King+Bishop+Parasite vs King | 470,086,096 | 13,965,588 [0] (5,013,372) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,355,020 [1,304,236] / 14,332 [14,332] | `f47108fdea572a895b49d0e1ac7f94e342f0029a753a7aa3cd99217ae4d275fb` |
| `kbishopkparasite.uftb` | King+Bishop vs King+Parasite | 442,566,648 | 0 [0] (3,693,788) / 13,243,670 [121,738] / 2,041,502 [1,714,306] | 15,738,520 [2,492,632] (3,093,244) / 0 [0] / 147,196 [144,172] | `4974a88a0665613f8097467df8bf39c67a1048046963e7be6f0cbbd802e879b0` |
| `kbishopgiantk.uftb` | King+Bishop+Giant vs King | 263,060,894 | 4,478,182 [0] (4,150,080) / 0 [0] / 4,658,438 [0] (5,692,260) | 0 [0] (1,142,116) / 4,160,272 [5,134] / 7,984,312 [2,356,398] (5,692,260) | `b0b370a4128b71835d8c55c149ad1e88ab6003e62c0eab1e96596d4361086fd7` |
| `kbishopkgiant.uftb` | King+Bishop vs King+Giant | 242,856,394 | 0 [0] (2,519,718) / 15,926 [108] / 10,751,056 [5,080,962] (5,692,260) | 32,848 [11,384] (3,051,612) / 0 [0] / 10,202,240 [2,567,328] (5,692,260) | `996ff93dba83521f5d211c03639b30c68caeb96a662a181e78f9b7e30127d2ad` |
| `kbishopfishermank.uftb` | King+Bishop+Fisherman vs King | 775,812,140 | 0 [0] (3,693,788) / 0 [0] / 15,285,172 [3,042] | 0 [0] (1,609,608) / 0 [0] / 17,369,352 [2,578,596] | `3e2b9df328f5ae439a9d4559b83e5d7d80ae35c07004c9f3f6abccff7cc254f3` |
| `kbishopdragonk.uftb` | King+Bishop+Dragon vs King | 592,268,962 | 12,325,918 [0] (6,653,042) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,094,422 [1,078,918] / 1,274,930 [1,274,930] | `e5441c998ebc7f4ef1a7be2079a7919b96a2e02238a21ccd4b0e342925bfc6fd` |
| `kbishopkdragon.uftb` | King+Bishop vs King+Dragon | 531,451,376 | 8 [0] (3,693,788) / 12,191,210 [273,664] / 3,093,954 [3,007,140] | 13,981,252 [3,608,198] (4,893,140) / 4 [0] / 104,564 [103,694] | `8c0fba9b456b74d6784bd9b48932a1ccabf44237792ee57413bfbf55a95d2be0` |
| `kbombbombk.uftb` | King+2 Bombs vs King | 260,111,166 | 6,651,352 [0] (2,838,050) / 0 [0] (78) / 0 [0] | 0 [0] (804,804) / 8,582,770 [0] (70,112) / 31,794 [31,794] | `3007861257e32343194410f89e6c446a904086162f15481d46d7a5337404a4ab` |
| `kbombkbomb.uftb` | King+Bomb vs King+Bomb | 477,943,752 | 6,440,274 [681,974] (3,841,414) / 4,746,170 [129,570] (60,466) / 3,889,740 [983,130] (896) | 6,440,274 [681,974] (3,841,414) / 4,746,170 [129,570] (60,466) / 3,889,740 [983,130] (896) | `c32ed66d65ba864590c1539a383f2e74d703d9e0b7a221226bf91bde30816736` |
| `kbombninjak.uftb` | King+Bomb+Ninja vs King | 628,229,260 | 12,012,790 [0] (6,966,170) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,219,704 [1,244,268] (67,760) / 81,888 [81,888] | `6b3ae1d52b88034772c3ecc8d59303dfe2b66c23f199d70b1613060f8f0017f6` |
| `kbombkninja.uftb` | King+Bomb vs King+Ninja | 554,146,022 | 4,900,960 [1,591,850] (3,842,796) / 489,142 [5,962] (528) / 9,742,834 [1,466,396] (2,700) | 1,190,298 [210,638] (5,421,572) / 2,177,708 [117,310] (53,280) / 10,136,102 [2,248,270] | `3c4bbaa19cd78e42cc8d5f74c14ba98052ae381c0b705123357a11fd364ca9d5` |
| `kbombturtlek.uftb` | King+Bomb+Turtle vs King | 427,769,788 | 14,512,116 [60] (4,466,844) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,269,840 [1,244,268] (67,760) / 31,752 [31,752] | `ee4dca71935ad3215f55f9647542ca737f86a6b092ef36923ae7662032b827de` |
| `kbombkturtle.uftb` | King+Bomb vs King+Turtle | 411,556,236 | 15,132,776 [2,903,640] (3,845,820) / 0 [0] (4) / 160 [160] (200) | 0 [0] (2,437,986) / 15,822,016 [251,228] (64,838) / 654,120 [653,744] | `b3ed8e16f550dc7325b87bf02578d8b949f758f070a7c7e98cddfd741c107f91` |
| `kbombmagek.uftb` | King+Bomb+Mage vs King | 407,442,416 | 15,215,852 [0] (3,763,108) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,271,480 [1,244,268] (67,760) / 30,112 [30,112] | `17d8e361f33ffa0b7e2ea366d9a68711ea96a2115aab8fd4de09b55800f412af` |
| `kbombkmage.uftb` | King+Bomb vs King+Mage | 387,466,920 | 15,132,936 [2,903,800] (3,846,024) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,296,088 [3,325,170] (67,760) / 5,504 [5,504] | `a7469c100c45ef088a63fb7016ea648a2b93e8ed3793054d83961f364e92fbe3` |
| `kbombparasitek.uftb` | King+Bomb+Parasite vs King | 476,419,868 | 13,901,148 [0] (5,077,812) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,263,608 [0] (67,760) / 37,984 [37,984] | `2af30741c6d6e9892aac5bd68b397454f3e351f0e459d04c274e47a7f80d9a30` |
| `kbombkparasite.uftb` | King+Bomb vs King+Parasite | 447,255,924 | 6,976,158 [843,608] (3,842,796) / 3,007,200 [95,062] (3,224) / 5,149,578 [821,496] (4) | 5,374,364 [1,352,612] (3,093,268) / 5,237,688 [54,332] (59,260) / 5,211,320 [5,672] (3,060) | `bc300fdbcb2945fd8fe494f09337a39f28fe7f6c41debac8535a7b124fca3d5f` |
| `kbombgiantk.uftb` | King+Bomb+Giant vs King | 273,239,456 | 8,936,526 [256] (4,350,174) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 12,067,988 [1,515,792] (48,420) / 28,176 [28,176] (5,692,260) | `560dcad95f6939eebb225e0f1751b0315a94a4b7bc1e2eb1a48d93229059956a` |
| `kbombkgiant.uftb` | King+Bomb vs King+Giant | 249,537,360 | 10,070,952 [3,002,922] (2,726,186) / 19,504 [0] / 470,050 [299,746] (5,692,268) | 30,656 [4,482] (3,121,728) / 8,037,238 [229,674] (43,188) / 2,052,834 [1,392,516] (5,693,316) | `a933018cdcc7a230eb2613e12b512bc36063a672cd33188fb8d7f30ae727dace` |
| `kbombfishermank.uftb` | King+Bomb+Fisherman vs King | 781,013,976 | 15,215,852 [0] (3,763,108) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,271,480 [1,244,268] (67,760) / 30,112 [30,112] | `c35a204b36baf78a0c823f857b370adeb4a22a2aa1b7eb159cface00da7e5b6b` |
| `kbombkfisherman.uftb` | King+Bomb vs King+Fisherman | 729,381,942 | 3,883,058 [1,540,246] (3,845,724) / 0 [0] / 11,249,878 [1,363,554] (300) | 0 [0] (1,609,608) / 1,468,590 [294,004] (42,504) / 15,833,002 [83,762] (25,256) | `afdd6e53d8f69c864f9c4dd29d44082319d434640b52157181cf078a31ddbf72` |
| `kbombdragonk.uftb` | King+Bomb+Dragon vs King | 598,544,858 | 12,346,586 [0] (6,632,374) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,256,790 [1,244,268] (67,760) / 44,802 [44,802] | `062163e51a367a6558d6cf520594ea40fcf3ba1f4d1572ea4a8eb62bcbd2f9ff` |
| `kbombkdragon.uftb` | King+Bomb vs King+Dragon | 531,583,888 | 4,442,892 [1,559,026] (3,843,440) / 116,256 [3,568] (264) / 10,573,788 [1,688,658] (2,320) | 510,846 [76,862] (5,087,512) / 1,891,844 [227,150] (54,248) / 11,433,310 [2,220,442] (1,200) | `6b1c65f252af9e836c6ccf51a368baaca815bffeeb2d1efca8034cf8c85a900e` |
| `kninjaninjak.uftb` | King+2 Ninjas vs King | 368,966,508 | 5,424,306 [0] (4,065,174) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,609,904 [1,103,940] / 74,772 [74,772] | `18fd0757d22df706de52658fc16ff2448be7ddb042e4c84b5561983c1031d6c8` |
| `kninjakninja.uftb` | King+Ninja vs King+Ninja | 616,537,904 | 3,731,190 [3,581,168] (5,259,100) / 42,886 [0] / 9,945,784 [167,060] | 3,731,190 [3,581,168] (5,259,100) / 42,886 [0] / 9,945,784 [167,060] | `f6decd9288f45723c055e89e2af71dee4a02865fbaea5b8256952131cfe74898` |
| `kninjaturtlek.uftb` | King+Ninja+Turtle vs King | 534,851,632 | 13,095,300 [0] (5,883,660) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,961,482 [1,089,700] / 1,407,870 [1,407,870] | `d74dad4d2f24688128956f8a6953f18bfadd5db0d3662844c3e8c99d0f888326` |
| `kninjakturtle.uftb` | King+Ninja vs King+Turtle | 508,867,664 | 13,719,744 [3,748,112] (5,259,100) / 0 [0] / 116 [116] | 0 [0] (2,397,164) / 14,534,186 [416,380] / 2,047,610 [2,047,314] | `5aa6d5de8a796b51006eccdaddb13ed8054a9339bc05c05371069365d7358a1a` |
| `kninjamagek.uftb` | King+Ninja+Mage vs King | 513,941,200 | 13,719,860 [314,846] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,914,976 [1,089,700] / 1,454,376 [1,454,376] | `47d3661b0f889b224609320c39b595ceb0acd7bf96b4842f41c73118ee7c50a9` |
| `kninjakmage.uftb` | King+Ninja vs King+Mage | 492,922,356 | 13,719,860 [3,748,228] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,942,740 [4,275,730] / 1,426,612 [1,426,612] | `8686c7bcc2093ef3812ced89ef620f6f01a876a72b8b928aae23fce957f510be` |
| `kninjaparasitek.uftb` | King+Ninja+Parasite vs King | 584,118,216 | 12,548,400 [0] (6,430,560) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,313,736 [1,304,236] / 55,616 [55,616] | `c37a2962c07c0e6317ff0db96a1e742c97a63259fe708a80f9423b013d857a60` |
| `kninjakparasite.uftb` | King+Ninja vs King+Parasite | 535,099,432 | 2,199,160 [401,896] (5,259,100) / 1,828,856 [161,690] / 9,691,844 [1,773,932] | 5,070,022 [2,536,168] (3,093,244) / 1,201,524 [11,782] / 9,614,170 [217,364] | `3b3eec23f0e5f96eb845c436bf35aabc5eda9b870d96411981f7e8023e5e6c6e` |
| `kninjagiantk.uftb` | King+Ninja+Giant vs King | 344,953,212 | 8,088,164 [0] (5,198,536) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,260,640 [1,414,934] / 883,944 [883,944] (5,692,260) | `e2b422f8816df785d4a6e9644e65e5b73d5c3d15cb8ebf1d08aa767f8524e6a5` |
| `kninjakgiant.uftb` | King+Ninja vs King+Giant | 306,835,628 | 9,576,164 [4,899,106] (3,695,184) / 13,504 [0] / 1,848 [1,766] (5,692,260) | 29,382 [9,054] (3,051,612) / 7,897,616 [426,314] / 2,308,090 [2,306,294] (5,692,260) | `c4b9a0a3ca1b5da47be4d557b2003d0a264c906a72c62d95ba1e0e219de42c85` |
| `kninjafishermank.uftb` | King+Ninja+Fisherman vs King | 888,968,396 | 13,719,860 [0] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,914,976 [1,089,700] / 1,454,376 [1,454,376] | `2adcd8f6d511a359b43a9dbb89650b0e2492d338d6d98538208796a182df0763` |
| `kninjakfisherman.uftb` | King+Ninja vs King+Fisherman | 803,024,716 | 13,719,860 [3,748,228] (5,259,100) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,953,476 [604,752] / 1,415,876 [1,415,876] | `de5c29df362dc35024077e4dcd380fffe6836e469d863e3871a7a1b52549be09` |
| `kninjadragonk.uftb` | King+Ninja+Dragon vs King | 707,856,384 | 11,131,336 [0] (7,847,624) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,252,356 [2,253,078] / 116,996 [116,996] | `de0501b3bef6a6abf8fc81c478c31af9a6c3f71f28d313473de954dc62beb52c` |
| `kninjakdragon.uftb` | King+Ninja vs King+Dragon | 599,564,588 | 4,589,520 [3,581,936] (5,259,100) / 186,156 [147,588] / 8,944,184 [224,462] | 4,806,436 [3,409,056] (4,893,140) / 276,566 [214,900] / 9,002,818 [294,408] | `b95ef8b87226cdbcbd0181443086fd4bb55033760d1e3cbc4fff0512141bdf5e` |
| `kturtleturtlek.uftb` | King+2 Turtles vs King | 167,494,620 | 440 [0] (1,578,876) / 0 [0] / 7,910,164 [0] | 0 [0] (804,804) / 416 [0] / 8,684,260 [1,322,978] | `bbb146a2f252eaf0eb40d23e49d0ddda56de183fb202c515ff64a0fd172037b1` |
| `kturtleparasitek.uftb` | King+Turtle+Parasite vs King | 383,586,096 | 15,158,912 [0] (3,820,048) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,364,876 [1,304,236] / 4,476 [4,476] | `eecd62d724dc6c09c12d13d3170a88c951b884bb7c6fed5c522c6085a763a0ae` |
| `kturtlekparasite.uftb` | King+Turtle vs King+Parasite | 373,105,856 | 0 [0] (2,397,164) / 7,602,610 [105,364] / 8,979,186 [739,046] | 9,995,776 [2,171,402] (3,093,244) / 0 [0] / 5,889,940 [371,870] | `7e174a2eaa35113df49bbd5b8dd89e16eac9359a96e93b9032d7c1622de8d878` |
| `kturtlegiantk.uftb` | King+Turtle+Giant vs King | 210,345,328 | 7,109,108 [0] (3,504,624) / 0 [0] / 2,672,968 [0] (5,692,260) | 0 [0] (1,142,116) / 6,387,316 [4,486] / 5,757,268 [2,401,070] (5,692,260) | `c613d3210037c0f9ba306e76e5e42531d0910c9ec7b0b387a9a9a73ab8d88eb8` |
| `kturtlekgiant.uftb` | King+Turtle vs King+Giant | 201,853,704 | 0 [0] (1,705,440) / 23,990 [86] / 11,557,270 [2,562,686] (5,692,260) | 42,798 [13,218] (3,051,612) / 4 [0] / 10,192,286 [2,302,838] (5,692,260) | `5e013243f6e49a8a2db0c969d117709183d2c8e0c95ce720cb19b4a23f2210a4` |
| `kturtledragonk.uftb` | King+Turtle+Dragon vs King | 505,144,192 | 13,456,552 [0] (5,522,408) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,996,498 [1,112,550] / 1,372,854 [1,372,854] | `5a40d96d2a1f2747a0e246d52add88cd99496a7a6d3e796fc1e11c0bec5a4f36` |
| `kturtlekdragon.uftb` | King+Turtle vs King+Dragon | 481,941,236 | 0 [0] (2,397,164) / 14,530,158 [564,036] / 2,051,638 [2,048,292] | 14,056,448 [3,613,858] (4,893,140) / 0 [0] / 29,372 [29,372] | `15fd11a4865807c48c1d47b508b5073f78efdf12332b34ab8ff7fd062d567fca` |
| `kmageparasitek.uftb` | King+Mage+Parasite vs King | 363,306,160 | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,368,120 [1,304,236] / 1,232 [1,232] | `d3d34227988697a1416e50aba326819bd14915ff18f9e5794846c477488ab076` |
| `kmagekparasite.uftb` | King+Mage vs King+Parasite | 344,453,172 | 0 [0] (1,609,608) / 17,369,180 [2,742,076] / 172 [172] | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | `7d8638ada4d6e2fb5314afc7b87bf25a4013f149b76525f44ba561a55cf147aa` |
| `kmagegiantk.uftb` | King+Mage+Giant vs King | 212,650,924 | 9,344,456 [1,280,090] (3,939,164) / 0 [0] / 3,080 [1,550] (5,692,260) | 0 [0] (1,142,116) / 9,720,916 [127,820] / 2,423,668 [2,412,264] (5,692,260) | `cb818ab1353d97412601aa5aa770e54783ece724068effee931178ecc0021f8a` |
| `kmagekgiant.uftb` | King+Mage vs King+Giant | 182,439,080 | 0 [0] (1,142,116) / 11,632 [3,960] / 12,132,952 [3,927,392] (5,692,260) | 31,532 [17,696] (3,051,612) / 0 [0] / 10,203,556 [2,297,652] (5,692,260) | `cf2dbfa1a1c73499fe682ee0a8a60f26b904382b6f59eb139bd7091ce5843fd7` |
| `kmagedragonk.uftb` | King+Mage+Dragon vs King | 484,234,160 | 14,085,820 [159,446] (4,893,140) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,945,268 [1,078,918] / 1,424,084 [1,424,084] | `d3733f70585208428185796973f80c171dc0f6b83adcc9923f46832f6fb73150` |
| `kmagekdragon.uftb` | King+Mage vs King+Dragon | 463,581,276 | 0 [0] (1,609,608) / 15,951,230 [4,006,756] / 1,418,122 [1,418,122] | 14,085,820 [3,642,684] (4,893,140) / 0 [0] / 0 [0] | `37521b0c37cfcd7cfb313f0532649eb0e9d4007a15c6d2524d34295b0c3790ae` |
| `kparasiteparasitek.uftb` | King+2 Parasites vs King | 216,128,928 | 7,259,568 [0] (2,229,912) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,682,564 [0] / 2,112 [2,112] | `000efc03710b9ed9771056a3c2d6490803739a163475094580fe499b39527e70` |
| `kparasitekparasite.uftb` | King+Parasite vs King+Parasite | 412,490,816 | 5,802,628 [1,356,396] (3,093,244) / 3,262,986 [0] / 6,820,102 [60] | 5,802,628 [1,356,396] (3,093,244) / 3,262,986 [0] / 6,820,102 [60] | `3d8117f1323b75f1b183355ca1eff44c122b40b7647d65ca24135c13be30169e` |
| `kparasitegiantk.uftb` | King+Parasite+Giant vs King | 243,180,684 | 9,347,536 [0] (3,939,164) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 12,141,428 [1,568,280] / 3,156 [3,156] (5,692,260) | `5e0bb56139965b70efddd21aa7929f4d829c101abce197357b5fc76e282faaf6` |
| `kparasitekgiant.uftb` | King+Parasite vs King+Giant | 227,572,076 | 9,667,136 [2,259,216] (2,193,308) / 22,216 [78] / 1,404,040 [561,382] (5,692,260) | 39,092 [10,070] (3,051,612) / 6,932,524 [90,918] / 3,263,472 [1,552,590] (5,692,260) | `726d837a97ab80c74da4bfb4492dce77950c96351b36fbb6105b42e62171918f` |
| `kparasitefishermank.uftb` | King+Parasite+Fisherman vs King | 738,333,356 | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,368,120 [1,304,236] / 1,232 [1,232] | `6b18865fd3c4e670c68a7ad5845239dece0fabbcbb3fad83c456018b5e274a03` |
| `kfishermankparasite.uftb` | King+Fisherman vs King+Parasite | 703,350,700 | 0 [0] (1,609,608) / 314,868 [115,894] / 17,054,484 [110,084] | 1,742,400 [1,338,528] (3,093,244) / 0 [0] / 14,143,316 [1,204,744] | `6032d4e60931298ce7fc31341cde1318a4fdcd19604c29a1a260a577d0f4c389` |
| `kparasitedragonk.uftb` | King+Parasite+Dragon vs King | 554,255,912 | 12,877,956 [0] (6,101,004) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,350,088 [1,304,236] / 19,264 [19,264] | `2b91e50eb9ff938d3f0419b34bff35c5496b5c1ee077949f09a46bb4d8090ca2` |
| `kparasitekdragon.uftb` | King+Parasite vs King+Dragon | 510,498,792 | 4,898,546 [2,541,902] (3,093,244) / 314,622 [6,194] / 10,672,548 [296,902] | 887,716 [128,792] (4,893,140) / 1,657,392 [302,362] / 11,540,712 [1,897,188] | `5259021302823eed33bf757a63b2c32caf8f07a45ad619e0fa9ab52af20df75e` |
| `kgiantgiantk.uftb` | King+2 Giants vs King | 62,426,868 | 1,467,264 [0] (1,555,208) / 0 [0] / 1,442,860 [0] (5,024,148) | 0 [0] (389,094) / 1,480,138 [1,076] / 2,596,100 [965,860] (5,024,148) | `9cfc00607ab58e3be4b4312c6eef4abd6a5f95cf427316d5802bf290284847a7` |
| `kgiantkgiant.uftb` | King+Giant vs King+Giant | 117,385,888 | 33,948 [15,698] (2,060,572) / 15,770 [28] / 6,820,374 [2,558,514] (10,048,296) | 33,948 [15,698] (2,060,572) / 15,770 [28] / 6,820,374 [2,558,514] (10,048,296) | `192e190c240441b5c1d461b39fe3f93f44377ba8f03391df6c2dfba230f42893` |
| `kgiantfishermank.uftb` | King+Giant+Fisherman vs King | 441,838,754 | 10,077,466 [128] (3,097,480) / 0 [0] / 111,754 [6,196] (5,692,260) | 0 [0] (1,142,116) / 9,403,952 [4,818] / 2,740,632 [2,409,218] (5,692,260) | `56728fb7a48b9dd7dc0652050521c168d1023d6da2160a34158b0c7b3b256217` |
| `kgiantkfisherman.uftb` | King+Giant vs King+Fisherman | 398,427,880 | 28,410 [10,554] (3,051,612) / 0 [0] / 10,206,678 [2,308,718] (5,692,260) | 0 [0] (1,313,848) / 12,796 [524] / 11,960,056 [1,786,726] (5,692,260) | `547b564ac58fff439ed090a9f765563f764228be6ae05b6434313707fe49aa32` |
| `kgiantdragonk.uftb` | King+Giant+Dragon vs King | 319,145,998 | 8,445,404 [0] (4,840,992) / 0 [0] / 304 [0] (5,692,260) | 0 [0] (1,142,116) / 11,293,652 [1,405,500] / 850,932 [850,652] (5,692,260) | `2a53fa8edbbc5a68bce7f1e66a52867626fda3ecb474f177377afd2fc4579015` |
| `kgiantkdragon.uftb` | King+Giant vs King+Dragon | 286,479,206 | 29,308 [11,108] (3,051,612) / 7,899,376 [487,698] / 2,306,404 [2,304,644] (5,692,260) | 9,876,212 [5,242,228] (3,346,434) / 12,978 [20] / 51,076 [50,870] (5,692,260) | `88b1b6bc867d11eb78cab27c599bbe94db8a32d97edb1a250624982d09a02c72` |
| `kfishermandragonk.uftb` | King+Fisherman+Dragon vs King | 859,261,356 | 14,085,816 [306] (4,893,140) / 0 [0] / 4 [0] | 0 [0] (1,609,608) / 15,945,260 [1,078,918] / 1,424,092 [1,424,084] | `0c860c29d234c4d0cb13106abe1fb3fb0281697533d8b6e1eb5c90cb9ea19f70` |
| `kfishermankdragon.uftb` | King+Fisherman vs King+Dragon | 782,325,302 | 0 [0] (1,609,608) / 15,953,310 [674,766] / 1,416,042 [1,416,042] | 14,085,820 [3,642,684] (4,893,140) / 0 [0] / 0 [0] | `949f749c991ae2901b0cc9bad4acb8fababef28a5e3e4b62e483211a1d467c41` |
| `kdragondragonk.uftb` | King+2 Dragons vs King | 338,720,772 | 5,682,590 [0] (3,806,890) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,655,726 [1,106,908] / 28,950 [28,950] | `fcbd96c4751483c15c31577d3850fd3270ddaac00d7a96fe5495a60f49ede102` |
| `kdragonkdragon.uftb` | King+Dragon vs King+Dragon | 580,885,096 | 3,622,924 [3,420,502] (4,893,140) / 110,414 [93,236] / 10,352,482 [239,206] | 3,622,924 [3,420,502] (4,893,140) / 110,414 [93,236] / 10,352,482 [239,206] | `a8740a9fc8683cd75bcd319081c544e1067796ad37b401a2259f2c6799847b56` |
| `kbombpenguink.uftb` | King+Bomb+Penguin vs King | 584,110,796 | 18,139,270 [0] (5,865,152) / 8,124 [0] (1,730) / 78,798 [0] (127,738,606) | 30,604 [0] (1,882,424) / 20,497,518 [1,244,268] (77,916) / 1,704,502 [1,585,008] (127,638,716) | `0bdb20ab3ed299d903fc1a9921150c77fcc7d80f1983f51c3672dc597550145f` |
| `kbombprincek.uftb` | King+Bomb+Prince vs King | 678,856,408 | 13,901,148 [0] (5,077,812) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,263,608 [1,244,268] (67,760) / 37,984 [37,984] | `74b48061b11795d274bcbb421019558cedc9fb1decb422c967020c9a8f9f827e` |
| `kpawnbombk.uftb` | King+Pawn+Bomb vs King | 779,725,448 | 26,783,928 [4,217,438] (11,173,990) / 0 [0] / 2 [0] | 0 [0] (3,219,216) / 31,088,884 [2,848,298] (3,586,976) / 56,952 [56,950] (5,892) | `804da1e2b87eac100ac6101ea2fe10fc2e83adeb8d0e4f50143f3a89719f3684` |
| `kbombcheckerk.uftb` | King+Bomb+Checker vs King | 818,127,549 | 29,074,749 [843,378] (9,725,859) / 0 [0] (3,286,402) / 0 [0] (33,828,830) | 0 [0] (3,219,216) / 32,817,112 [2,397,809] (10,068,704) / 57,398 [57,398] (29,753,410) | `013330f8184618b0d536da4a4e9417c8fbc1b49fcaab2a1842266e8246b7ad52` |
| `kbombsniperk.uftb` | King+Bomb+Sniper vs King | 1,509,762,618 | 29,596,516 [5,263] (46,319,320) / 0 [0] / 2 [0] (2) | 0 [0] (6,438,432) / 34,536,086 [2,488,536] (34,802,038) / 67,098 [67,096] (72,186) | `be04af3e927cf0a34219b0bd2b3663e659da16c86be7775ee8afb05d55150b9e` |
| `kberserkerbombk.uftb` | King+Berserker+Bomb vs King | 11,931,939,740 | 49,719,592 [0] (140,070,008) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 172,804,228 [12,442,680] (677,600) / 211,692 [211,692] | `b05efd1d29f32bd1647ee78672a7b5acd3468729a0b262e4370754a74216c1f6` |
| `kberserkerdragonk.uftb` | King+Berserker+Dragon vs King | 12,793,936,358 | 46,276,232 [0] (143,513,368) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,452,674 [15,796,064] / 240,846 [240,846] | `df2ca64b8c46824d99b938ded4b9fb190f54ca590349359da758684302839c84` |
| `kberserkergiantk.uftb` | King+Berserker+Giant vs King | 7,257,357,620 | 33,876,688 [0] (98,990,312) / 0 [0] / 0 [0] (56,922,600) | 0 [0] (11,421,160) / 112,998,120 [5,221,108] / 8,447,720 [8,447,720] (56,922,600) | `ffa499e631d5a245dc289e5b728c89d1fd241b642037c740bd263869ebe21235` |
| `kberserkerkfisherman.uftb` | King+Berserker vs King+Fisherman | 11,740,317,882 | 43,145,968 [21,483,818] (133,257,124) / 0 [0] / 13,386,508 [58,622] | 0 [0] (16,096,080) / 143,592,952 [7,694,834] / 30,100,568 [14,160,284] | `31d4fccb2fa110188d7fd20ba5b6493ba30428e20b9dce5305121c4f4fffd560` |
| `kberserkerkprince.uftb` | King+Berserker vs King+Prince | 11,434,096,778 | 34,642,930 [21,333,644] (133,257,124) / 14,340,720 [0] / 7,548,826 [208,796] | 76,168,028 [48,180,252] (30,932,440) / 74,070,652 [2,756,524] / 8,618,480 [664,256] | `5cffc0d8c782f6d28801d9cfc9cd3e8da77ac2710720ddc0e37c7e7885a476a2` |
| `kberserkerkturtle.uftb` | King+Berserker vs King+Turtle | 10,143,227,932 | 49,547,276 [21,483,264] (133,257,124) / 0 [0] / 6,985,200 [59,176] | 0 [0] (23,971,640) / 136,166,764 [5,565,484] / 29,651,196 [20,474,076] | `7b38de854d35a8d8b94bb2ddae3edf69202ce5cbd8d3a58f329b3d12b5f977c1` |
| `kbishopghostk.uftb` | King+Bishop+Ghost vs King | 958,964,780 | 29,226,628 [1,319,584] (8,731,292) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 32,002,462 [2,636,240] (1,482,452) / 1,252,606 [1,252,606] (1,184) | `61bc4f9865d4c217868da670e27e16e9a493f31ca15ae6a68f91469b8c48dcf2` |
| `kbishopkpenguin.uftb` | King+Bishop vs King+Penguin | 561,147,524 | 351,036 [28,528] (4,334,684) / 1,795,034 [0] / 17,713,862 [3,535,658] (127,637,064) | 6,177,308 [388,768] (1,972,862) / 150,056 [2,288] / 15,860,216 [1,446,406] (127,671,238) | `2e272def31054bfc62cda5f81fce85cdabc37597dd0d65ff737c7861381c8fe3` |
| `kbishopkprince.uftb` | King+Bishop vs King+Prince | 648,044,796 | 0 [0] (3,693,788) / 12,149,366 [3,096] / 3,135,806 [3,006,940] | 15,826,140 [2,574,822] (3,093,244) / 0 [0] / 59,576 [49,388] | `e3e2ae7d3c017eeaf0b0365b7bc1114170d0b076ea55dbc83ff66ebbbf318fb9` |
| `kbishoppenguink.uftb` | King+Bishop+Penguin vs King | 576,356,284 | 224,826 [0] (5,076,498) / 8,690 [0] / 18,068,924 [0] (128,452,742) | 33,710 [4,094] (1,881,536) / 337,642 [84] / 21,941,728 [4,401,624] (127,637,064) | `2e959c0d8845e38ad9e304ce2b7f0d0b8036bca54a382b5ae6eb7146c40f7c59` |
| `kbishopprincek.uftb` | King+Bishop+Prince vs King | 673,430,482 | 13,965,588 [0] (5,013,372) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,113,394 [1,253,408] / 1,255,958 [1,255,958] | `bdfb6056b073a1c261379d0e2d56ffdc30f3422cf6ed5de00a912cfe3a117116` |
| `kbombkpenguin.uftb` | King+Bomb vs King+Penguin | 567,847,282 | 6,721,592 [1,720,940] (4,563,760) / 819,348 [0] (19,288) / 12,058,892 [1,413,384] (127,648,800) | 2,471,876 [0] (1,925,280) / 3,970,504 [133,952] (33,150) / 15,667,080 [492] (127,763,790) | `f6546ea7233d72a4e904170f5113b2021dab7235c5c9916e796a169ff57db294` |
| `kbombkprince.uftb` | King+Bomb vs King+Prince | 648,908,976 | 5,503,540 [1,624,552] (3,842,796) / 4,450,992 [8] (3,228) / 5,178,404 [1,279,248] | 9,444,906 [847,724] (3,162,508) / 2,718,824 [118,044] (52,464) / 3,599,906 [393,926] (352) | `cd220ce57a5db672809cf550f82f9d06f2195e60a001f6fe2e6c5186fabf8b94` |
| `kghostgiantk.uftb` | King+Ghost+Giant vs King | 497,645,576 | 19,660,036 [964,964] (6,913,364) / 0 [0] / 0 [0] (11,384,520) | 0 [0] (2,284,232) / 22,395,792 [3,077,148] (210,664) / 842,184 [842,184] (12,225,048) | `efdac7b79a43d11afd1b519d93a50b02f69a31299fa0119efc8b9238dd638659` |
| `kghostkgiant.uftb` | King+Ghost vs King+Giant | 472,896,720 | 23,027,424 [6,574,512] (3,335,424) / 44,316 [78] / 166,236 [117,954] (11,384,520) | 40,538 [11,516] (6,104,934) / 15,510,940 [0] (39,824) / 4,033,270 [3,773,612] (12,228,414) | `e50f913fd9771d6c3fa95886a644a324f1b89a073c269c5d748c7ad6d3125107` |
| `kcopycatkbishop.uftb` | King+Copycat vs King+Bishop | 967,403,928 | 5,967,160 [5,909,584] (8,221,984) / 0 [0] / 22,327,336 [727,224] (1,441,440) | 0 [0] (7,021,104) / 36,168 [11,128] / 29,459,208 [10,718,984] (1,441,440) | `80879aedce245b17492c375236d447dfc011a96d56bbf749797ab398603dd8a7` |
| `kpenguindragonk.uftb` | King+Penguin+Dragon vs King | 675,982,732 | 16,823,836 [0] (7,140,970) / 8,078 [0] / 159,590 [0] (127,699,206) | 30,978 [3,426] (1,881,536) / 18,922,014 [1,105,952] / 3,360,088 [3,099,582] (127,637,064) | `8732dfe2b06e742628fba37cb6e7bcb4a44ba1b587e2aa4c1fef41e2f67a270d` |
| `kpenguinkdragon.uftb` | King+Penguin vs King+Dragon | 652,117,660 | 752,816 [34,664] (1,903,140) / 965,350 [287,284] (240) / 20,469,414 [1,652,154] (127,740,720) | 6,345,962 [3,952,822] (5,766,916) / 206,072 [0] / 11,875,666 [369,722] (127,637,064) | `53844af37522f54b3cc3c77b0be8db61ce5e5ba9eab4f50283b14abae5de9a91` |
| `kparasiteksniper.uftb` | King+Parasite vs King+Sniper | 1,317,072,056 | 30,827,161 [5,026,190] (43,200,137) / 3,362 [0] (324) / 940,909 [103,708] (943,947) | 18,837 [175] (7,373,790) / 31,459,030 [372,293] (33,426,461) / 2,328,395 [931,171] (1,309,327) | `15f3fdcdb3fd40059ede627b69ad5a1abc6485601730d2d7632de0487363bc86` |
| `kpawnkpawn.uftb` | King+Pawn vs King+Pawn | 1,033,823,420 | 23,572,279 [11,162,115] (12,257,356) / 13,995,050 [138,662] (4,166,425) / 17,476,447 [2,306,947] (4,448,283) | 23,787,547 [11,367,389] (13,235,607) / 14,713,643 [110,159] (3,519,756) / 16,542,586 [2,130,176] (4,116,701) | `8a40455ab36dd47c70f756c5da78aa22b0b4251b6cd81a2b24bf79122afdcb89` |
| `kbishopkberserker.uftb` | King+Bishop vs King+Berserker | 9,703,257,936 | 0 [0] (36,937,880) / 110,296,472 [4,589,036] / 42,555,248 [30,069,936] | 43,049,192 [21,477,988] (133,257,124) / 0 [0] / 13,483,284 [266,830] | `13f9c6b0b2cc8c78e26b49e0de6d8ccbd044e570178023c047f9daf6151f9e6b` |
| `kberserkerkgiant.uftb` | King+Berserker vs King+Giant | 6,031,224,106 | 39,561,654 [19,685,876] (93,165,442) / 56,756 [78] / 83,148 [59,026] (56,922,600) | 167,220 [86,642] (30,516,120) / 79,001,406 [3,410,746] / 23,182,254 [23,066,842] (56,922,600) | `bd0871671df1098f7a5a2028c08efa284fb8f8a4f7454f32cdbc67a401d9df3b` |
| `kberserkerpenguink.uftb` | King+Berserker+Penguin vs King | 13,810,633,928 | 76,725,372 [0] (163,761,180) / 36,608 [0] / 994,712 [0] (1,276,798,928) | 133,596 [13,904] (18,815,360) / 198,562,130 [4,448,476] / 24,435,074 [22,911,504] (1,276,370,640) | `5d17089a189b99acda411f805ae5ea0e2c90907d49ba7e3b5e7c10c38002d438` |
| `kberserkerprincek.uftb` | King+Berserker+Prince vs King | 12,956,507,116 | 51,853,836 [0] (137,935,764) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,641,360 [17,454,944] / 52,160 [52,160] | `a9789c4518cc959aa3cd32229b98388697dde88d421d087a6d1922b7cb962867` |
| `kberserkersniperk.uftb` | King+Berserker+Sniper vs King | 41,855,141,200 | 109,978,888 [0] (649,179,512) / 0 [0] / 0 [0] | 0 [0] (64,384,320) / 319,869,415 [8,930,916] (319,770,586) / 27,517,625 [27,517,625] (27,616,454) | `7e852f5e1209f5a83dc7d27edb06c90ca2ad493e427b664bc2e71d8683d25fbc` |
| `kbishopsniperk.uftb` | King+Bishop+Sniper vs King | 1,483,041,050 | 9,193,147 [0] (24,947,234) / 0 [0] / 20,564,602 [0] (21,210,857) | 0 [0] (6,438,432) / 7,794,178 [9,255] (7,789,011) / 26,944,526 [5,090,249] (26,949,693) | `74781f36c5c28c632f2d843e302b71327a17bd6ac76b27ffa6802e9649aac58c` |
| `kknightberserkerk.uftb` | King+Knight+Berserker vs King | 11,294,287,008 | 52,880,628 [0] (136,908,972) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 160,497,732 [4,441,064] / 13,195,788 [13,195,788] | `c389380f6b2606721a30d44d57c01d76182d4bf4bb08eaf01bf85116c01b5689` |
| `kknightkpawn.uftb` | King+Knight vs King+Pawn | 645,354,262 | 423 [57] (5,617,946) / 4,013,654 [2,357] (498) / 25,096,929 [4,388,242] (3,228,470) | 9,176,340 [5,839,044] (4,023,065) / 71 [2] (6) / 21,395,053 [1,536,618] (3,363,385) | `003d4d3cad41c9a6ea94e22d789b10a96fae0d71a6ca55cf56c8d82b0572f074` |
| `kknightpawnk.uftb` | King+Knight+Pawn vs King | 651,976,044 | 27,144,042 [4,428,933] (6,260,188) / 0 [0] / 1,324,696 [58,940] (3,228,994) | 0 [0] (3,219,216) / 25,739,111 [1,999,338] / 5,528,433 [3,118,658] (3,471,160) | `a1fa8d3eefe2d3244fe223721014b5c59b43cf0c80a129f2d724380b8623490f` |
| `kknightsniperk.uftb` | King+Knight+Sniper vs King | 1,254,109,954 | 11,922,549 [0] (24,251,031) / 0 [0] / 19,552,645 [0] (20,189,615) | 0 [0] (6,438,432) / 9,835,988 [9,609] (9,831,108) / 24,902,716 [5,227,171] (24,907,596) | `e570493039b0f839a9db5d9cd1ca2fa9437f07154a57536875029aae973947c4` |
| `kmagekprince.uftb` | King+Mage vs King+Prince | 560,208,356 | 0 [0] (1,609,608) / 15,953,304 [2,438,568] / 1,416,048 [1,416,048] | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | `37d7fc822bc8e388184ca1e3eb0c69a9c3993946d9a459b07642f00a7de93468` |
| `kninjakpenguin.uftb` | King+Ninja vs King+Penguin | 684,877,344 | 9,451,316 [4,106,378] (6,207,336) / 200,870 [0] / 8,335,094 [308,270] (127,637,064) | 665,546 [8,590] (1,900,844) / 3,287,800 [232,944] (384) / 18,234,234 [1,667,434] (127,742,872) | `9229d9cbe019449cf2cd5cceb7518dc6b1e4334426424371b4c3b629e98d91e5` |
| `kninjaksniper.uftb` | King+Ninja vs King+Sniper | 1,898,236,088 | 27,403,612 [7,495,255] (48,440,012) / 1,986 [0] (34) / 34,122 [26,939] (36,074) | 18,986 [8,280] (7,375,156) / 30,048,895 [1,108,924] (31,764,666) / 3,738,381 [3,646,465] (2,969,756) | `93de17e41dfa1e34e41ef52e4335693b0eba1d40167b27b5e714d5a116fc630a` |
| `kninjapenguink.uftb` | King+Ninja+Penguin vs King | 711,410,832 | 16,444,626 [0] (7,537,472) / 7,244 [0] / 138,826 [0] (127,703,512) | 28,356 [1,918] (1,881,536) / 18,976,524 [1,093,320] / 3,308,200 [3,093,134] (127,637,064) | `a82383d2ce1927250d8907386d49cf647e284a74095cd273c98ecd5cc7b5826f` |
| `kpawnbishopk.uftb` | King+Pawn+Bishop vs King | 766,191,332 | 25,729,337 [4,198,093] (8,019,984) / 0 [0] / 1,165,543 [34,135] (3,043,056) | 0 [0] (3,219,216) / 26,530,127 [2,045,064] / 4,737,417 [2,995,572] (3,471,160) | `62e6e742b4d594946bce257c5bd3f800ceaf4f73295d934ba62bfa6be1e60007` |
| `kpawncheckerk.uftb` | King+Pawn+Checker vs King | 1,113,858,720 | 48,558,372 [10,350,877] (11,507,729) / 0 [0] (6,307,104) / 9,629,288 [155,957] (75,829,187) | 0 [0] (6,438,432) / 40,849,554 [2,999,292] (10,934,306) / 18,521,674 [6,357,739] (75,087,714) | `fad0546442174f87d973d9adcfc5d9dba6a40881b2a51f440098fdedf36bc976` |
| `kpawndragonk.uftb` | King+Pawn+Dragon vs King | 933,442,172 | 24,792,154 [3,892,809] (13,165,766) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,520,715 [4,480,363] (3,184,406) / 746,829 [746,829] (286,754) | `3e6ede973af86cf474916fa1035bb2c6535cb71c448e76bb470ab6ca84fe84f5` |
| `kpawnfishermank.uftb` | King+Pawn+Fisherman vs King | 1,304,042,676 | 29,932,710 [4,782,493] (7,157,423) / 0 [0] / 638,754 [66,944] (229,033) | 0 [0] (3,219,216) / 26,628,630 [1,626,779] (2,668,726) / 4,638,914 [3,319,504] (802,434) | `85f47bc3962e658c78554f9b3e52eeb139fa99d62af40c98098b11610ab7b5ac` |
| `kpawnkchecker.uftb` | King+Pawn vs King+Checker | 1,106,164,412 | 31,563,353 [11,974,656] (10,049,616) / 0 [0] (7,962,672) / 26,529,999 [2,269,071] (75,726,040) | 1,513,008 [1,513,008] (8,164,848) / 22,643,668 [16,395] (9,414,334) / 36,854,940 [7,741,595] (73,240,882) | `0df247f65f30956c260fe1ded6ea75c5980bb15750129ba0364b3eeaab3feeb0` |
| `kpawnkninja.uftb` | King+Pawn vs King+Ninja | 973,048,459 | 2,172,582 [2,168,729] (3,915,296) / 23,490,501 [1,478,375] (3,176,984) / 4,908,381 [4,150,537] (294,176) | 23,868,117 [6,929,869] (13,254,978) / 5,152 [275] / 829,673 [9,284] | `6106bc0ffac808fb713f240873ea09af882214893b21eba180592ab214ea24dd` |
| `kpawnkparasite.uftb` | King+Pawn vs King+Parasite | 683,105,194 | 497,022 [398,513] (3,915,296) / 23,399,121 [1,414,644] (3,470,252) / 6,675,321 [3,466,305] (908) | 24,637,692 [4,730,099] (9,358,672) / 127,859 [535] / 3,833,697 [3,031] | `738e8d01a49355a2af55920857f29bc30349cfbd250e58ab4e2531008bfa1a18` |
| `kpawnkpenguin.uftb` | King+Pawn vs King+Penguin | 852,462,936 | 11,832,030 [6,469,242] (4,590,874) / 14,424,714 [75,614] (26,708) / 13,200,398 [2,417,755] (259,588,636) | 21,775,987 [1,768,283] (3,992,056) / 6,175,609 [15,037] (1,841) / 12,090,558 [1,330,185] (259,627,309) | `a5bcd9c68346eeb9db9e968ff1aee112311b76c1ffa8a41796d4d90f183e37f3` |
| `kpawnkqueen.uftb` | King+Pawn vs King+Queen | 1,183,125,489 | 1,863,518 [1,863,452] (3,915,296) / 25,084,238 [3,308,287] (3,175,716) / 3,623,708 [3,483,344] (295,444) | 21,436,907 [8,399,223] (16,433,692) / 66 [66] / 87,255 [20,468] | `9b937fa688d11089874a5cdad01ce7c474a18182d14ca394249613362b68af9e` |
| `kpawnkrook.uftb` | King+Pawn vs King+Rook | 912,779,220 | 4,625,746 [4,585,057] (3,915,296) / 21,929,710 [1,546,308] (3,183,976) / 4,016,008 [1,593,929] (287,184) | 23,671,433 [6,750,250] (12,693,436) / 26,202 [994] / 1,566,849 [155,345] | `4b193d764d1eeb5be729154e28f87739b5ceae01a796e2c4078a9cd746263d08` |
| `kpawnmagek.uftb` | King+Pawn+Mage vs King | 550,731,452 | 29,847,888 [8,689,112] (7,048,084) / 0 [0] / 723,576 [478,593] (338,372) | 0 [0] (3,219,216) / 26,720,788 [1,631,410] (2,523,305) / 4,546,756 [3,318,678] (947,855) | `2e9c96a7503d067ddaadce65477a01eab1117d88206d4a6d55f0a367811bdbdb` |
| `kpawnninjak.uftb` | King+Pawn+Ninja vs King | 993,011,116 | 24,156,042 [3,793,267] (13,801,878) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,462,577 [4,394,022] (3,178,510) / 804,967 [804,967] (292,650) | `ef92507b706fa54a3314f6cafbdebcfd83939e3463f6d8fcf054ea54e9c182f2` |
| `kpawnpawnk.uftb` | King+2 Pawns vs King | 518,468,624 | 26,297,859 [7,932,270] (8,546,010) / 0 [0] / 562,073 [36,160] (2,551,978) | 0 [0] (3,219,216) / 25,844,400 [3,171,391] (3,140,228) / 2,259,284 [1,317,735] (3,494,792) | `6ea0b2c441b9abf87b7795018ab9587d924a59625006f66ca70e0f09d39e570a` |
| `kpawnqueenk.uftb` | King+Pawn+Queen vs King | 1,212,077,088 | 21,031,966 [3,282,889] (16,925,954) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,456,915 [4,593,474] (3,177,968) / 810,629 [810,629] (293,192) | `3c4fcdc596ba06b4b52fbf3acc16a64876e2a66ec9b12600541f20e32c077680` |
| `kpawnrookk.uftb` | King+Pawn+Rook vs King | 930,610,960 | 24,708,550 [3,877,953] (13,249,370) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,532,459 [4,503,846] (3,185,418) / 735,085 [735,085] (285,742) | `9d29e1d0f00c5c545bcb6e425ce3d9ad11fa2e8f7e8e74935f4df258b8679efa` |
| `kpawnsniperk.uftb` | King+Pawn+Sniper vs King | 1,974,046,016 | 50,411,293 [9,248,300] (68,735,776) / 0 [0] / 9,103,393 [143,015] (23,581,218) | 0 [0] (12,876,864) / 44,105,559 [3,252,705] (44,113,852) / 18,429,529 [6,719,938] (32,305,876) | `57bbc99cfd8d856e699f5c3cc4c3f90b7fcb6333b668e2f87ae0845590703ffd` |
| `kpenguinkchecker.uftb` | King+Penguin vs King+Checker | 895,858,308 | 38,588,465 [2,997,096] (4,028,623) / 37,362 [0] (4,662,760) / 3,582,830 [211,756] (556,426,680) | 1,065,566 [986,654] (4,681,076) / 31,851,132 [0] (3,705,788) / 10,466,148 [4,759,463] (555,557,010) | `abfc9e093894b70a374e6eddd67575c73a30e5f8e26aa12d8c7add200c4034ed` |
| `kpenguinkprince.uftb` | King+Penguin vs King+Prince | 776,575,720 | 908,482 [5,416] (1,901,532) / 6,270,036 [368] (384) / 15,009,062 [1,649,280] (127,742,184) | 13,969,824 [2,838,042] (3,628,184) / 295,934 [0] / 6,300,674 [134,918] (127,637,064) | `805c6087b9c4eb81d2aee2fbf30fae0269032eb0a9784df41f8613879dbc5e68` |
| `kpenguinksniper.uftb` | King+Penguin vs King+Sniper | 1,642,298,428 | 28,078,231 [2,258,675] (38,164,291) / 67,736 [1,809] (41,116) / 14,619,585 [1,037,018] (526,355,761) | 202,605 [30,522] (8,728,431) / 21,824,065 [0] (21,090,790) / 21,504,578 [5,822,875] (533,976,251) | `625b236cbd2c232d9111b714f822ac0a6c0a85ded6667d0bca176cde91003257` |
| `kprincekdragon.uftb` | King+Prince vs King+Dragon | 709,512,276 | 14,755,942 [2,730,756] (3,093,244) / 131,108 [78,374] / 998,666 [25,960] | 4,154,896 [3,392,696] (4,893,140) / 6,130,796 [1,484] / 3,800,128 [249,988] | `d68283d813726a389ff33e8c3571f012c89e80934c527428ea6a52b3e6bf6936` |
| `kprincekfisherman.uftb` | King+Prince vs King+Fisherman | 919,105,884 | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,953,476 [8,080] / 1,415,876 [1,415,876] | `865807122ae9a67559d398a98013eee2c4c61d427091acaf18662857a90738f6` |
| `kqueenberserkerk.uftb` | King+Queen+Berserker vs King | 14,269,732,124 | 38,850,936 [0] (150,938,664) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,225,060 [14,097,412] / 468,460 [468,460] | `0dcdc7fcc0f534cd8bcc8a6d8cb57117590cfa60fe984b7a5efb03e85eedcd1b` |
| `kqueenkberserker.uftb` | King+Queen vs King+Berserker | 8,573,958,714 | 55,908,752 [42,743,894] (70,356,160) / 49,276,058 [2,289,660] / 14,248,630 [3,288,486] | 32,009,490 [21,373,322] (133,257,124) / 11,339,090 [1,426,458] / 13,183,896 [634,662] | `87fb45d7076234f070a3c1e14a4516fbb1dffb7cf28c87736da03ea95a96e2d7` |
| `kqueenkchecker.uftb` | King+Queen vs King+Checker | 1,206,580,272 | 22,705,453 [8,810,659] (15,252,462) / 0 [0] (3,981,336) / 5 [5] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,546 [2,570,087] (15,131,060) / 4,047,886 [4,047,876] (22,980,420) | `3f93dbd960fe79d2fedffc2df55c8a7df4f074c8154f44926a1443b08d0b77df` |
| `kqueenkpenguin.uftb` | King+Queen vs King+Penguin | 802,788,964 | 14,124,912 [5,359,504] (8,257,358) / 110,644 [0] / 1,701,702 [126,786] (127,637,064) | 294,258 [8,974] (1,900,844) / 14,096,292 [1,033,564] (384) / 7,797,030 [1,646,724] (127,742,872) | `4b88a0fc26abf45532c892cee3a6ac6826dc9aa177cf2dbac82ad3cfc63c614a` |
| `kqueenkprince.uftb` | King+Queen vs King+Prince | 810,849,480 | 8,281,508 [4,274,320] (7,035,616) / 839,582 [0] / 2,822,254 [327,082] | 8,540,824 [2,744,162] (3,093,244) / 2,678,044 [378,616] / 4,666,848 [45,662] | `47686caa18bd36c29906b81a4470bc346fcbfe65b5031c127afadfb0fbd4a274` |
| `kqueenpenguink.uftb` | King+Queen+Penguin vs King | 842,346,056 | 14,516,164 [0] (9,516,358) / 6,618 [0] / 129,830 [0] (127,662,710) | 25,886 [1,918] (1,881,536) / 19,100,638 [940,786] / 3,186,556 [2,983,230] (127,637,064) | `8956777e9f6a621c1ec3435a94cf24d50ece7574ea3b2de6c6f088a78e0e7926` |
| `krookkberserker.uftb` | King+Rook vs King+Berserker | 9,270,926,858 | 38,318,464 [35,226,490] (49,514,360) / 89,794,560 [3,815,268] / 12,162,216 [2,725,650] | 42,548,358 [21,471,642] (133,257,124) / 1,645,304 [576,320] / 12,338,814 [325,482] | `993cc0cd421fbc0d161bde2dd1dc8a366358052d8ad70cb51ec4aa33f62e8e4b` |
| `krookkchecker.uftb` | King+Rook vs King+Checker | 941,536,498 | 26,642,046 [7,230,317] (11,298,154) / 0 [0] (3,981,336) / 17,720 [17,720] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,319 [1,177,847] (11,128,832) / 4,048,113 [4,047,681] (26,982,648) | `9d11eac17ac6c84271eea4ee1559712cc88e362db9913ca8e8e96a29581c87d2` |
| `krookkprince.uftb` | King+Rook vs King+Prince | 707,127,324 | 3,945,892 [3,522,596] (4,951,436) / 9,120,690 [1,596] / 960,942 [272,550] | 15,595,226 [2,636,544] (3,093,244) / 181,862 [155,926] / 108,628 [17,584] | `2e4650da37f8101cfae58edcbd08aa8c9f5e69b1ef0590112844bef9ff8a8708` |
| `krookksniper.uftb` | King+Rook vs King+Sniper | 1,776,985,012 | 27,994,313 [7,580,471] (47,800,057) / 1,345 [0] / 59,390 [52,712] (60,735) | 15,553 [8,411] (7,374,850) / 29,996,075 [1,044,702] (31,743,450) / 3,794,634 [3,615,394] (2,991,278) | `82f0700a00398b917923388ddfd3bbce6fb650ff116cc16011159244c3457b47` |
| `krookpenguink.uftb` | King+Rook+Penguin vs King | 674,547,284 | 16,749,144 [0] (7,223,250) / 7,554 [0] / 171,446 [0] (127,680,286) | 28,934 [2,406] (1,881,536) / 18,879,752 [1,118,246] / 3,404,394 [3,131,472] (127,637,064) | `1b8ac4a9603394068f2044f64348254af44fc357f1f30ac0f2cc4957324e86ae` |
| `krooksniperk.uftb` | King+Rook+Sniper vs King | 1,810,956,004 | 27,240,584 [0] (48,675,248) / 0 [0] / 0 [0] (8) | 0 [0] (6,438,432) / 31,982,751 [2,187,163] (31,959,260) / 2,755,953 [2,755,953] (2,779,444) | `1e102a2f5711253a7ff319ca986cb235cee8a1830c02a0a7dfb97d12ddba8711` |
| `ksniperfishermank.uftb` | King+Sniper+Fisherman vs King | 2,558,434,808 | 31,421,784 [4] (39,247,144) / 0 [0] / 2,384,478 [45,422] (2,862,434) | 0 [0] (6,438,432) / 25,674,843 [9,762] (25,670,975) / 9,063,861 [5,376,474] (9,067,729) | `5983afa0fb77c2b44f0fe61023931095ef328f367da50cb00fe074ced1bc315f` |
| `ksnipergiantk.uftb` | King+Sniper+Giant vs King | 703,447,594 | 5,157,709 [0] (17,725,745) / 0 [0] / 14,895,190 [0] (38,137,196) | 0 [0] (4,568,464) / 4,166,945 [7,062] (4,161,800) / 20,122,223 [4,767,556] (42,896,408) | `44aa3a95babcdbc943e38b989a85b098ad391bc7b4394a661228bff54df2423a` |
| `ksniperkdragon.uftb` | King+Sniper vs King+Dragon | 1,783,570,522 | 18,551 [8,451] (7,377,486) / 29,948,299 [1,243,184] (31,728,605) / 3,839,412 [3,624,913] (3,003,487) | 28,081,570 [7,276,246] (47,654,130) / 1,946 [618] (618) / 88,124 [78,376] (89,452) | `061fdbf0fa706e3d905f862e6e2055175f5e6e9747a45d86fe5c1eb64a2a01b2` |
| `ksniperkprince.uftb` | King+Sniper vs King+Prince | 2,170,540,228 | 21,074 [7,950] (7,376,279) / 30,072,342 [12,859] (31,837,201) / 3,712,846 [3,604,026] (2,896,098) | 31,728,060 [5,088,903] (44,101,036) / 2,399 [0] (181) / 40,973 [12,540] (43,191) | `13a44527959eabd4ad528de1171e80a3788874aee835e0ef2b5c6d28cd149c67` |
| `kbombksniper.uftb` | King+Bomb vs King+Sniper | 1,483,640,998 | 30,195,199 [5,788,960] (45,579,259) / 3,103 [0] (246) / 67,570 [52,837] (70,463) | 17,159 [113] (7,440,953) / 32,601,128 [788,509] (34,711,234) / 987,815 [817,789] (157,551) | `da5ae52691b37e45faf8e846021158542f88fce980291c2d16229103d045ebaf` |
| `kcheckercheckerk.uftb` | King+2 Checkers vs King | 302,267,456 | 831,744 [831,744] (2,481,336) / 0 [0] (3,192,048) / 29,885,073 [0] (115,441,479) | 0 [0] (1,630,008) / 0 [0] (4,875,120) / 31,344,369 [2,509,327] (113,982,183) | `a5460fb6e1e44825cc58b87fa911e1bd63262890f579fe0dfdac28f6c8dbd6bc` |
| `kknightkberserker.uftb` | King+Knight vs King+Berserker | 10,000,878,252 | 20 [4] (28,089,600) / 124,506,824 [5,381,514] / 37,193,156 [23,481,420] | 43,541,844 [21,481,008] (133,257,124) / 4 [0] / 12,990,628 [134,364] | `d24d5594074f02965001f4e7d7b7fc313443d2cc422fabf64bf3074318eb62aa` |
| `kknightkpenguin.uftb` | King+Knight vs King+Penguin | 499,527,648 | 254,522 [27,724] (3,313,768) / 1,521,148 [0] / 19,105,178 [2,766,452] (127,637,064) | 4,862,070 [363,102] (1,941,024) / 95,978 [0] (228) / 17,229,532 [1,365,778] (127,702,848) | `78ae50654659bee099698857a92afd43267e01db4b86cee530e0242f6741738b` |
| `kknightkprince.uftb` | King+Knight vs King+Prince | 605,790,120 | 0 [0] (2,808,960) / 13,810,336 [4,780] / 2,359,664 [2,348,060] | 15,883,456 [2,613,948] (3,093,244) / 0 [0] / 2,260 [2,260] | `d5d481cdcaa3d22cfad03e11c29eb40a9a71596c72e1c972488c6339d918bace` |
| `kknightpenguink.uftb` | King+Knight+Penguin vs King | 508,183,960 | 110,448 [0] (3,923,778) / 9,014 [0] / 19,147,574 [0] (128,640,866) | 34,258 [3,914] (1,881,536) / 256,896 [12] / 22,021,926 [4,564,334] (127,637,064) | `b3684bbafa918bd7ad79573b6c153850ed0aa115ffa124059bd72110ff27262a` |
| `kknightprincek.uftb` | King+Knight+Prince vs King | 621,465,228 | 14,798,084 [0] (4,180,876) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,055,072 [1,275,868] / 1,314,280 [1,314,280] | `b7ba0c5a2614ec6ff2db5f2b9744fcb0f40a276557c343befbc2d43a8e35105c` |
| `kmagekpenguin.uftb` | King+Mage vs King+Penguin | 429,135,484 | 46,148 [0] (1,881,536) / 22,416 [384] / 22,244,516 [3,062,676] (127,637,064) | 44,662 [12,490] (1,901,860) / 11,076 [0] / 22,131,842 [1,642,522] (127,742,240) | `9d62161db06f39194bb282289c56433c57caa5d7a64f07091a094383cc8609a2` |
| `kmageksniper.uftb` | King+Mage vs King+Sniper | 986,983,732 | 0 [0] (6,438,432) / 600 [237] (195) / 34,738,104 [6,002,161] (34,738,509) | 11,188 [9,547] (7,375,992) / 0 [0] / 33,795,074 [3,600,362] (34,733,586) | `4515cad800d4c94dd32abfac88efb2cd2b5cdde353ae7be5dcb51411b63f52cb` |
| `kmagepenguink.uftb` | King+Mage+Penguin vs King | 449,539,868 | 52,012 [0] (2,273,538) / 2,820 [0] / 20,523,140 [1,102,930] (128,980,170) | 8,998 [3,178] (1,881,536) / 135,360 [0] / 22,168,722 [4,763,078] (127,637,064) | `99b7ea7b8b08078292e0da1b569e68c9d0377b0558d0ec82b40706e2db3c5f16` |
| `kmagesniperk.uftb` | King+Mage+Sniper vs King | 1,058,326,024 | 3,175,958 [2,554] (10,385,352) / 0 [0] / 30,630,304 [2,336,264] (31,724,226) | 0 [0] (6,438,432) / 1,556,296 [1,145] (1,550,648) / 33,182,408 [5,385,091] (33,188,056) | `1ee846ca23bd49187bd75bf8fc03bd27dfabdfee26b1ec1cfdafd277fe018636` |
| `kninjacheckerk.uftb` | King+Ninja+Checker vs King | 1,031,703,788 | 26,310,899 [842,688] (12,489,709) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,299,302 [2,096,754] (12,710,575) / 2,703,822 [2,703,822] (26,982,925) | `b8f762b36f0d59af02a63a1a6719b6359c8809c92012a0bdff1b7bcb6451a0ca` |
| `kninjakchecker.uftb` | King+Ninja vs King+Checker | 1,000,857,173 | 26,071,326 [7,217,091] (11,886,589) / 0 [0] (3,981,336) / 5 [5] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,651 [995,446] (11,713,745) / 4,047,781 [4,047,771] (26,397,735) | `ca368a02f935119311cbae502451ae5bab45cd7a38dd85ccef0ba9d5050635f4` |
| `kninjakprince.uftb` | King+Ninja vs King+Prince | 730,942,100 | 4,846,014 [3,579,644] (5,259,100) / 2,281,212 [0] / 6,592,634 [168,584] | 10,544,172 [2,692,252] (3,093,244) / 323,642 [75,468] / 5,017,902 [33,324] | `871b3ff462a17422b10bacb898a90f23f4815651ac7b785ec30f1030299b2436` |
| `kninjaprincek.uftb` | King+Ninja+Prince vs King | 778,235,364 | 12,548,400 [0] (6,430,560) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,313,736 [2,393,936] / 55,616 [55,616] | `901ebe3a50367f0e30abba9bed37a69ad4c61b788dc98ed402336bf0d7d0a6fe` |
| `kninjasniperk.uftb` | King+Ninja+Sniper vs King | 1,936,390,928 | 26,683,127 [0] (49,232,713) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 31,901,245 [2,189,403] (31,890,547) / 2,837,459 [2,837,459] (2,848,157) | `dbeb3322a4e68b8253e8807185ab73e2b3c94c446c15706b9dd3f7c35943082a` |
| `kparasitecheckerk.uftb` | King+Parasite+Checker vs King | 729,645,788 | 30,312,348 [842,688] (8,488,260) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 33,000,176 [2,514,375] (8,637,632) / 2,948 [2,948] (31,055,868) | `91b90fecaa05f7265a88a18e05591a99df2df994ffb7d424e9fee0aa46bd769a` |
| `kparasitekchecker.uftb` | King+Parasite vs King+Checker | 716,371,728 | 30,184,807 [4,908,545] (7,772,580) / 0 [0] (3,981,336) / 533 [533] (33,976,584) | 842,688 [842,688] (3,996,240) / 31,486,080 [310,286] (7,730,990) / 1,479,352 [1,478,670] (30,380,490) | `23df48e4e7de77314af68271e030b635a3d2475399bd0af466a16670915f3bf4` |
| `kparasitekprince.uftb` | King+Parasite vs King+Prince | 619,441,776 | 4,652,674 [2,513,040] (3,093,244) / 8,407,008 [0] / 2,826,034 [30,232] | 12,315,894 [1,120,886] (3,093,244) / 1,456,090 [90,364] / 2,113,732 [153,608] | `945fb573ed4d54a979916ad8a8f59314712bab3ce4c120ff91a6293b8495d637` |
| `kparasiteprincek.uftb` | King+Parasite+Prince vs King | 639,112,828 | 14,519,136 [0] (4,459,824) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,365,128 [1,304,236] / 4,224 [4,224] | `cf0276fbfd9c64de20bfd5829b01c136c2375f789e759947772ebcbd23529fa1` |
| `kpawngiantk.uftb` | King+Pawn+Giant vs King | 368,497,478 | 15,978,839 [2,799,202] (6,404,886) / 0 [0] / 2,116,159 [40,269] (13,458,036) | 0 [0] (2,284,232) / 16,199,574 [2,065,785] (9,586) / 5,605,778 [2,535,361] (13,858,750) | `0e29753c5016a9d9a34b5be599a91c3269f08002ced595757b63a11ee0667144` |
| `kpawnkbomb.uftb` | King+Pawn vs King+Bomb | 766,296,389 | 194,062 [168,124] (3,969,106) / 24,596,582 [1,481,981] (3,584,448) / 5,607,796 [3,691,488] (5,926) | 24,725,970 [5,229,463] (10,722,710) / 33,717 [398] (21) / 2,475,277 [156,077] (225) | `1b9a77a1082abc5d7c6016e232e40ea1a33df3889ecfea36a587711141340f08` |
| `kpawnkdragon.uftb` | King+Pawn vs King+Dragon | 915,700,531 | 2,144,900 [2,144,201] (3,915,296) / 24,362,176 [1,721,244] (3,183,656) / 4,064,388 [3,904,740] (287,504) | 25,183,197 [6,763,796] (12,587,170) / 775 [229] / 186,778 [26,841] | `2332bba62a6c7ba2935cec3faf337c14c9ba08ea11b7a338b6c54dfa46ca3018` |
| `kpawnkfisherman.uftb` | King+Pawn vs King+Fisherman | 1,287,701,472 | 8,325,772 [6,146,233] (3,915,296) / 0 [0] / 22,245,692 [1,300,739] (3,471,160) | 0 [0] (3,219,216) / 3,521,082 [118,619] / 27,746,462 [2,643,796] (3,471,160) | `9a6828c3e5a21875c77b6d2e6d00ffad1c95736f92f05eb0db506d743261f2cd` |
| `kpawnkgiant.uftb` | King+Pawn vs King+Giant | 362,044,641 | 9,065,903 [5,011,218] (2,772,100) / 50,175 [481] (10,834) / 12,201,406 [2,021,179] (13,857,502) | 94,351 [29,122] (6,128,192) / 4,264,979 [1,437] / 14,022,468 [4,343,474] (13,447,930) | `fb00809bfbbec185e10b43691bd64264050a7a95da659ee2772788c8a6e6d166` |
| `kpawnkmage.uftb` | King+Pawn vs King+Mage | 518,767,828 | 21,929,849 [6,757,635] (3,915,296) / 0 [0] / 8,641,615 [689,337] (3,471,160) | 0 [0] (3,219,216) / 16,912,650 [1,997,903] / 14,354,894 [3,389,739] (3,471,160) | `9210d586bae8a151c6d17a3828360bf35d12256340340679cb659dd46f968602` |
| `kpawnkprince.uftb` | King+Pawn vs King+Prince | 1,110,797,094 | 2,563,910 [2,525,732] (3,915,296) / 24,816,148 [2,190,553] (3,185,088) / 3,191,406 [2,721,245] (286,072) | 27,928,283 [4,732,262] (9,358,672) / 52,974 [756] / 617,991 [647] | `94dd0fcfadc972c6dc8b62154de61f92ebb30965047385bba0135202fab42faf` |
| `kpawnksniper.uftb` | King+Pawn vs King+Sniper | 1,968,257,270 | 33,472,406 [12,448,985] (49,133,590) / 7,362 [65] (17,349) / 27,663,160 [2,363,887] (41,537,813) | 56,090 [20,101] (14,810,196) / 24,565,735 [4,758] (26,522,672) / 36,241,229 [6,822,621] (49,635,758) | `8cc3f328fdf589387bad9c452eaac1632ff8fc9d685bae6c68a239b4fca904fb` |
| `kpawnkturtle.uftb` | King+Pawn vs King+Turtle | 590,011,881 | 16,009,417 [6,169,115] (3,915,296) / 4 [0] (4) / 14,562,043 [1,273,292] (3,471,156) | 6 [4] (4,794,334) / 10,898,578 [2,160] / 18,952,120 [3,795,693] (3,312,882) | `aa8d282fabc538742ea79bb630156dea715e3935b04b8303e6909f4492cc24fa` |
| `kpawnparasitek.uftb` | King+Pawn+Parasite vs King | 691,146,212 | 27,959,472 [4,405,844] (9,998,448) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 31,261,396 [2,797,676] (3,470,928) / 6,148 [6,148] (232) | `acaa6f2bcd4dee7e0438bbdc5f607f0ea5baf57f5cd27c6f1c18509d91851311` |
| `kpawnpenguink.uftb` | King+Pawn+Penguin vs King | 857,470,324 | 29,664,972 [5,136,230] (6,702,807) / 17,052 [0] (1,712) / 6,705,334 [107,384] (260,571,483) | 65,730 [8,572] (3,769,736) / 24,660,200 [1,528,335] (26,692) / 15,552,370 [7,020,672] (259,588,632) | `9ab338d5214f417832333bf2b4aef88441b8849b7fca961ec4202de4d994678d` |
| `kpawnprincek.uftb` | King+Pawn+Prince vs King | 1,118,502,692 | 27,959,472 [4,405,844] (9,998,448) / 0 [0] / 0 [0] | 0 [0] (3,219,216) / 30,537,567 [4,110,610] (3,185,764) / 729,977 [729,977] (285,396) | `022a767867632d6084ed83c101c0ae0c453b25ba21593b09398dfaee94c5bb44` |
| `kpawnturtlek.uftb` | King+Pawn+Turtle vs King | 594,296,668 | 25,786,808 [4,519,644] (5,456,624) / 0 [0] / 3,401,600 [83,907] (3,312,888) | 0 [0] (3,219,216) / 23,074,570 [1,593,720] / 8,192,974 [3,317,745] (3,471,160) | `33fe22febff27c382de0e06f3f29e4c53439ac3c7f36af16245f9754891c04aa` |
| `kpenguincheckerk.uftb` | King+Penguin+Checker vs King | 903,135,178 | 1,061,771 [914,028] (5,432,818) / 17,945 [0] (3,693,290) / 38,137,705 [0] (558,983,191) | 69,008 [8,804] (3,766,404) / 323,692 [0] (4,691,100) / 42,059,530 [8,936,858] (556,416,986) | `b34710578767b75e21c3e5790cf995d49bc28994964395e783d918d289a27e92` |
| `kpenguinfishermank.uftb` | King+Penguin+Fisherman vs King | 894,206,922 | 52,012 [0] (2,181,788) / 7,184 [0] / 20,518,776 [1,246] (129,071,920) | 26,522 [4,010] (1,881,536) / 135,360 [0] / 22,151,198 [4,762,246] (127,637,064) | `3f710fc10cc64268245b61b2f5f820a388ffdb43f0c4e7cb6be75e6130c33580` |
| `kpenguingiantk.uftb` | King+Penguin+Giant vs King | 313,208,608 | 279,824 [0] (3,856,022) / 6,320 [0] (56) / 12,730,524 [296] (134,958,934) | 22,846 [3,014] (1,393,332) / 267,788 [754] / 15,996,786 [3,973,456] (134,150,928) | `49436c4403f618be39a647aeb1afb944351d418f6cd476217187c7b5069a9b85` |
| `kpenguinparasitek.uftb` | King+Penguin+Parasite vs King | 531,557,176 | 18,869,468 [0] (5,122,930) / 8,220 [0] / 80,080 [0] (127,750,982) | 32,236 [0] (1,881,536) / 20,550,828 [1,304,236] / 1,730,016 [1,609,980] (127,637,064) | `758419427e374e94ed01eabcccd6fcdeafe8c6283ccb257f979da07364242748` |
| `kpenguinprincek.uftb` | King+Penguin+Prince vs King | 787,350,076 | 18,811,198 [1,120] (5,112,674) / 7,100 [0] / 139,470 [0] (127,761,238) | 27,614 [1,250] (1,881,536) / 18,805,584 [1,251,004] / 3,479,882 [3,266,454] (127,637,064) | `9ffd9eb292f826d5a1a427adc89a5931726f1cc7bf2ad22bc8abbcd028516606` |
| `kprincecheckerk.uftb` | King+Prince+Checker vs King | 1,156,532,442 | 30,312,348 [842,688] (8,488,260) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,379,307 [2,404,380] (8,495,050) / 2,623,817 [2,623,817] (31,198,450) | `1dec6cd88cbe574a08f849f7278214673d4fbfd2c468d2b1d677ef074b2d65b2` |
| `kprincedragonk.uftb` | King+Prince+Dragon vs King | 750,573,070 | 12,877,956 [0] (6,101,004) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,350,088 [2,416,786] / 19,264 [19,264] | `a36610733aca3706e8ef867e88b394b047bb40a8ab8ae5bb5131598c34588107` |
| `kprincefishermank.uftb` | King+Prince+Fisherman vs King | 954,088,540 | 15,885,716 [0] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,952,244 [1,247,388] / 1,417,108 [1,417,108] | `19f989d8b13e97448c2468447efedd7cfd9e6eb0b04cd8af571e75703f07f614` |
| `kprincekchecker.uftb` | King+Prince vs King+Checker | 1,143,982,528 | 30,185,340 [4,909,078] (7,772,580) / 0 [0] (3,981,336) / 0 [0] (33,976,584) | 842,688 [842,688] (3,996,240) / 28,917,866 [26,083] (7,588,408) / 4,047,566 [4,047,566] (30,523,072) | `2d10c9908be3f336c416db2a654121d276e1a3cee496a0c2b60f40cad49c8178` |
| `kprincekgiant.uftb` | King+Prince vs King+Giant | 364,607,296 | 11,077,872 [2,827,256] (2,193,308) / 15,508 [0] / 12 [12] (5,692,260) | 32,332 [10,020] (3,051,612) / 7,897,424 [2,266] / 2,305,332 [2,305,328] (5,692,260) | `7c975a34f8be98cadcffb0c0431aefd13c4df11aba4c54af3d316e6c3d2f14cb` |
| `kprincekprince.uftb` | King+Prince vs King+Prince | 827,645,648 | 9,496,340 [2,537,522] (3,093,244) / 2,032,316 [0] / 4,357,060 [6,446] | 9,496,340 [2,537,522] (3,093,244) / 2,032,316 [0] / 4,357,060 [6,446] | `86458138107d33f578a26e31142b6248701454ced8dc89e3df58b218747f5b40` |
| `kprinceprincek.uftb` | King+2 Princes vs King | 422,983,900 | 7,259,568 [0] (2,229,912) / 0 [0] / 0 [0] | 0 [0] (804,804) / 8,682,564 [1,275,812] / 2,112 [2,112] | `55bf68fafce963911eb13d508490b557e5f900468111cd3bc7e09cc0e1059662` |
| `kqueencheckerk.uftb` | King+Queen+Checker vs King | 1,250,844,256 | 23,003,190 [842,688] (15,797,418) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,293,838 [1,804,538] (16,205,172) / 2,709,286 [2,709,286] (23,488,328) | `f5e53a9237688466508a8d3f30658faac37385c1057af1aee9b59c61d345db95` |
| `kqueenksniper.uftb` | King+Queen vs King+Sniper | 2,319,211,428 | 23,861,263 [9,200,785] (52,003,727) / 639 [0] / 24,786 [20,688] (25,425) | 11,695 [8,138] (7,374,850) / 30,068,292 [2,559,973] (31,767,334) / 3,726,275 [3,651,252] (2,967,394) | `15960e29f126027de6aa80081361c6a1274d90e8e7d9e9aca49d9f87153e6d32` |
| `krookberserkerk.uftb` | King+Rook+Berserker vs King | 12,770,082,774 | 45,455,332 [0] (144,334,268) / 0 [0] / 0 [0] | 0 [0] (16,096,080) / 173,567,318 [15,833,892] / 126,202 [126,202] | `275588d89d53fabaece72ea4c2b3f8ea50241da0de23dd055c1f09bdb00c49c7` |
| `krookcheckerk.uftb` | King+Rook+Checker vs King | 969,285,252 | 26,884,310 [842,688] (11,916,298) / 0 [0] (3,153,552) / 0 [0] (33,961,680) | 0 [0] (3,219,216) / 30,374,309 [2,119,457] (12,113,725) / 2,628,815 [2,628,815] (27,579,775) | `ab318a12b1599eb3cc1bb9ac5cb6c1caa30f2e1c83f9e6784b5bb316c6c92ee3` |
| `krookkpenguin.uftb` | King+Rook vs King+Penguin | 650,198,952 | 6,137,850 [4,054,806] (5,804,210) / 154,864 [0] / 12,097,692 [442,306] (127,637,064) | 406,384 [13,712] (1,901,252) / 958,538 [444,468] (468) / 20,822,658 [1,748,018] (127,742,380) | `5b5187b6b54267abec83bd56a01adae9315e94e8db71780f39aa461cbe91c3ef` |
| `krookprincek.uftb` | King+Rook+Prince vs King | 748,288,326 | 12,797,700 [0] (6,181,260) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,360,968 [2,409,632] / 8,384 [8,384] | `75b23122e70727468159450026142e6975778dcbcc5db60c136836b29f0e73e3` |
| `ksnipercheckerk.uftb` | King+Sniper+Checker vs King | 2,127,916,160 | 9,111,471 [1,685,376] (27,007,943) / 0 [0] (12,614,208) / 55,302,198 [10] (199,627,540) | 0 [0] (12,876,864) / 4,981,208 [12,171] (22,783,839) / 61,025,040 [10,201,510] (201,996,409) | `9f2a923d90dfbe52ec3d173a8f0d2755f7dbf33b2deb0840a62ae31c2d1015e0` |
| `ksniperdragonk.uftb` | King+Sniper+Dragon vs King | 1,817,779,796 | 27,426,669 [0] (48,489,146) / 0 [0] / 12 [0] (13) | 0 [0] (6,438,432) / 31,968,687 [2,181,965] (31,963,501) / 2,770,017 [2,770,004] (2,775,203) | `3a23ffb178f24fec1bc8144087102091c23b5f9c50bcd5cd59c86c63f275fca3` |
| `ksniperkchecker.uftb` | King+Sniper vs King+Checker | 2,112,245,202 | 62,927 [19,466] (14,787,558) / 0 [0] (15,925,344) / 64,174,862 [7,014,267] (208,712,669) | 1,685,376 [1,685,376] (17,670,336) / 8,642 [368] (14,405,940) / 65,922,222 [8,282,737] (203,970,844) | `637555647c893a3e08cded3c521545607280d051ee9bb61cee78b3008f5e889b` |
| `ksniperkfisherman.uftb` | King+Sniper vs King+Fisherman | 2,514,088,462 | 19,672 [8,981] (7,379,641) / 0 [0] / 33,786,590 [3,600,928] (34,729,937) | 0 [0] (6,438,432) / 2,171 [1,060] (1,102) / 34,736,533 [2,941,016] (34,737,602) | `3c28915b87c2a61479a4ed9e11a05dad6cafd7e4a01df5333897f5de07caa51b` |
| `ksniperksniper.uftb` | King+Sniper vs King+Sniper | 3,740,495,424 | 82,139 [22,265] (29,633,213) / 9,976 [0] (20,535) / 67,520,409 [7,350,219] (206,397,088) | 65,701 [20,032] (29,600,117) / 14,004 [142] (38,876) / 67,532,819 [7,352,310] (206,411,843) | `40480cbb8019d8c8804f82fce7a036fed4f2afc88134bb3181d56c9e60b6291a` |
| `ksniperprincek.uftb` | King+Sniper+Prince vs King | 2,184,270,722 | 30,910,078 [0] (45,005,762) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 31,990,205 [2,510,497] (31,982,747) / 2,748,499 [2,748,499] (2,755,957) | `b97ff7a41961e9043d8e2095d82cbcb552958707eed6c90f0cc1954050ba47ff` |
| `ksnipersniperk.uftb` | King+2 Snipers vs King | 1,873,383,456 | 5,095,977 [0] (31,789,868) / 0 [0] / 27,777,843 [649] (87,167,992) | 0 [0] (12,876,864) / 3,519,792 [10,344] (10,470,915) / 31,218,912 [5,313,473] (93,745,197) | `da2c94e3b05fd63bd548fb669115e6ea22414c06a84ded5684b11afdd276f854` |
| `kturtlecheckerk.uftb` | King+Turtle+Checker vs King | 632,802,460 | 842,688 [842,688] (5,530,456) / 0 [0] (3,153,552) / 30,771,020 [0] (35,618,124) | 0 [0] (3,219,216) / 0 [0] (5,530,456) / 33,003,124 [5,120,816] (34,163,044) | `6055a03990fa2a5526698d3c4c2c7af5db120258f36a08fb9a4c35cf454f9dc1` |
| `kturtlekpenguin.uftb` | King+Turtle vs King+Penguin | 468,073,224 | 85,838 [6,036] (2,807,026) / 4,314,716 [0] / 16,987,036 [2,381,536] (127,637,064) | 8,233,738 [453,460] (1,912,312) / 55,292 [0] (468) / 13,898,550 [1,201,652] (127,731,320) | `6c82aa7c28d28e967675f8010248aae9ee53935eea19866e1f9685d5c7b4076b` |
| `kturtlekprince.uftb` | King+Turtle vs King+Prince | 584,501,740 | 0 [0] (2,397,164) / 14,534,482 [2,988] / 2,047,314 [2,047,314] | 15,885,716 [2,543,272] (3,093,244) / 0 [0] / 0 [0] | `0eb410311239b82c8846b71cc251304047540ff6d5b336f7c405106e53347fd7` |
| `kturtlepenguink.uftb` | King+Turtle+Penguin vs King | 473,840,192 | 66,816 [0] (3,193,810) / 8,738 [0] / 19,642,426 [0] (128,919,890) | 33,770 [2,958] (1,881,536) / 200,974 [36] / 22,078,336 [4,650,770] (127,637,064) | `37aed9ce0e03c858ab9367de863ae49078b10887893a4ef84542a2eaa1a87bd4` |
| `kturtleprincek.uftb` | King+Turtle+Prince vs King | 594,594,956 | 15,158,912 [0] (3,820,048) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 16,006,830 [1,247,388] / 1,362,522 [1,362,522] | `e1b0d6848e81718bbc9ce4bc5ce2ad0d44fdf36b544987e62c348ccaedd14c5d` |
| `kturtlesniperk.uftb` | King+Turtle+Sniper vs King | 1,138,521,016 | 9,670,663 [0] (20,241,467) / 0 [0] / 22,592,323 [0] (23,411,387) | 0 [0] (6,438,432) / 7,496,555 [8,779] (7,491,150) / 27,242,149 [5,325,140] (27,247,554) | `ea0320d221feada81a3f3095da60d9db8c641cd997ffb000cb58e68538a6fc09` |
| `kbishopksniper.uftb` | King+Bishop vs King+Sniper | 1,461,597,624 | 365 [0] (14,775,517) / 1,516 [0] / 30,568,463 [6,089,878] (30,569,979) | 16,501 [8,428] (7,374,850) / 80 [0] (112) / 33,789,681 [4,072,198] (34,734,616) | `8f5c296efc9164db658870de11cc4ad61222e82cc2315de7f378b81aed34185d` |
| `kknightksniper.uftb` | King+Knight vs King+Sniper | 1,241,344,106 | 1,375 [132] (11,237,215) / 2,887 [0] (203) / 32,335,738 [4,800,681] (32,338,422) | 23,892 [8,805] (7,376,677) / 210 [0] (302) / 33,782,160 [3,908,529] (34,732,599) | `7ca4cbeb56cec95bd22e0fc4787abc2e90b67bde75b2fb472f94c12c89cc5802` |
| `kmageprincek.uftb` | King+Mage+Prince vs King | 579,061,344 | 15,885,716 [28] (3,093,244) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 15,952,244 [1,247,388] / 1,417,108 [1,417,108] | `680116363bb4338774e391c8fdc14400c2f7282e9846c9d103cb9262e8de03d6` |
| `kparasitesniperk.uftb` | King+Parasite+Sniper vs King | 1,332,434,664 | 30,910,078 [0] (45,005,762) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 34,734,402 [2,608,472] (34,727,644) / 4,302 [4,302] (11,060) | `fec437c1303bf0dd682a416507ae22cb019df2dca7275be946e96d48d926db2f` |
| `kpawnkbishop.uftb` | King+Pawn vs King+Bishop | 755,071,473 | 6,641,333 [5,361,314] (3,915,296) / 60 [0] (28) / 23,930,071 [2,066,663] (3,471,132) | 270 [0] (7,387,708) / 1,707,770 [1,673] / 25,819,248 [5,591,010] (3,042,924) | `e6cd72176ae656179adfbaf2bcff65c8c7eae1cf870af3a4d7f1955e3c8391fd` |
| `kpenguinkfisherman.uftb` | King+Penguin vs King+Fisherman | 894,206,922 | 277,430 [24,326] (1,902,856) / 9,676 [0] / 21,900,474 [1,630,686] (127,741,244) | 39,656 [0] (1,881,536) / 111,230 [5,274] / 22,162,194 [1,666,916] (127,637,064) | `62920549fe495d0157f7109648e9b8fca9bfd01316023c4c80243daeb3e4f13b` |
| `kpenguinkgiant.uftb` | King+Penguin vs King+Giant | 299,536,632 | 2,438,842 [136,686] (1,434,972) / 93,748 [24] (336) / 13,627,494 [1,877,670] (134,236,288) | 161,494 [21,824] (3,649,580) / 809,970 [0] / 13,059,708 [2,685,300] (134,150,928) | `b157e48890e8294f457c7e6e5598502fd1dda2bb00b2130a772ce79ca41e9e4e` |
| `kpenguinkparasite.uftb` | King+Penguin vs King+Parasite | 520,782,820 | 3,676,662 [0] (1,901,200) / 2,397,148 [89,708] (384) / 16,113,770 [0] (127,742,516) | 4,688,126 [1,411,064] (3,628,184) / 1,463,544 [0] / 14,414,762 [1,546,572] (127,637,064) | `23b3783c5dbde51d0e8cdba20ffb6cd506800dce9d33bf45be6d676132b41f19` |
| `kpenguinkpenguin.uftb` | King+Penguin vs King+Penguin | 579,441,560 | 2,316,414 [10,832] (1,870,468) / 871,218 [0] / 21,176,576 [1,538,660] (277,428,684) | 2,305,582 [0] (1,870,468) / 871,218 [0] / 21,187,408 [1,549,492] (277,428,684) | `659269ce29c4b499ef89947874f0f085e537d0cef662010628eb10148052cc0a` |
| `kpenguinpenguink.uftb` | King+2 Penguins vs King | 289,720,780 | 48,472 [0] (1,223,088) / 5,784 [0] / 10,303,014 [0] (140,251,322) | 23,780 [796] (935,234) / 130,430 [0] / 12,027,894 [3,318,326] (138,714,342) | `d390c1d335944be7b045eafacb831f2912cc5d9e0cbb3f834cce6b57f7016abb` |
| `kpenguinsniperk.uftb` | King+Penguin+Sniper vs King | 1,655,885,080 | 667,643 [0] (10,243,176) / 18,704 [0] (19,300) / 39,451,139 [0] (556,926,758) | 71,284 [8,638] (7,597,428) / 430,869 [2,005] (340,890) / 44,124,007 [9,376,560] (554,762,242) | `a69674a6b5b52ae5efa81b848f2fcfcf147c289ac8a7295dc4ebce189df2b26d` |
| `kprincegiantk.uftb` | King+Prince+Giant vs King | 378,901,188 | 9,347,536 [0] (3,939,164) / 0 [0] / 0 [0] (5,692,260) | 0 [0] (1,142,116) / 11,300,932 [1,534,228] / 843,652 [843,652] (5,692,260) | `c4763e0ede9256c0b495854c4daa429890076069515f58daef552c1c4fd8bbb5` |
| `kqueenprincek.uftb` | King+Queen+Prince vs King | 877,396,168 | 10,877,572 [0] (8,101,388) / 0 [0] / 0 [0] | 0 [0] (1,609,608) / 17,307,456 [2,241,402] / 61,896 [61,896] | `5788af4f33bb9f1c502a2baaca2d12f364076c92656226310f40241a3ec46aee` |
| `kqueensniperk.uftb` | King+Queen+Sniper vs King | 2,374,625,846 | 23,192,071 [0] (52,723,769) / 0 [0] / 0 [0] | 0 [0] (6,438,432) / 31,894,241 [1,884,335] (31,869,235) / 2,844,463 [2,844,463] (2,869,469) | `be6ac47cdd07b5097f31fe547d758befdc799535973982c1fb0d50608403e298` |
| `ksniperkgiant.uftb` | King+Sniper vs King+Giant | 694,495,544 | 25,615 [13,122] (5,215,427) / 54,781 [482] (65,922) / 23,569,822 [4,527,764] (46,984,273) | 101,524 [33,273] (12,307,972) / 2,857 [52] (261) / 20,365,795 [4,658,586] (43,137,431) | `56b1361f43ec982bd77c7c3f8458bb5c4a84519667ca0f38643ac8b32f60c858` |
| `kturtleksniper.uftb` | King+Turtle vs King+Sniper | 1,130,420,832 | 22 [15] (9,588,678) / 3,637 [0] (380) / 33,159,933 [4,163,976] (33,163,190) | 28,231 [9,079] (7,378,264) / 15 [0] (16) / 33,778,016 [3,605,910] (34,731,298) | `1711bff02a6020dac0023006338e7ccb8039c3e7e9db1bf6cdec854d48097a94` |
| `kcopycatberserkerk.uftb` | King+Berserker+Copycat vs King | 23,638,091,312 | 92,601,592 [0] (272,563,208) / 0 [0] / 0 [0] (14,414,400) | 0 [0] (30,989,760) / 334,009,432 [39,172,584] / 165,608 [165,608] (14,414,400) | `857585c32b786f183d2cfdf4b34b5ea330c7a64f1de7d41ebd7ceb769699a9d9` |
| `kcopycatcheckerk.uftb` | King+Checker+Copycat vs King | 1,732,329,984 | 54,198,828 [1,598,400] (20,215,428) / 0 [0] (6,073,152) / 23,852 [0] (71,320,420) | 0 [0] (6,197,952) / 54,204,764 [4,157,020] (20,179,068) / 9,291,028 [9,222,436] (61,958,868) | `e956dd2a0d2e44117e87c82008f5a4f1fc13b21de02101685b5a91b712ead6ce` |
| `kcopycatkberserker.uftb` | King+Berserker vs King+Copycat | 18,155,307,736 | 61,771,024 [59,049,360] (82,219,840) / 198,470,656 [9,322,008] / 22,703,280 [3,500,968] (14,414,400) | 87,371,472 [57,076,040] (256,401,208) / 1,490,328 [0] / 19,901,792 [177,328] (14,414,400) | `3a3850b5ba940f648db042c67de241e497ceb0ee68a7b162861af2fde920ab82` |
| `kcopycatkchecker.uftb` | King+Checker vs King+Copycat | 1,685,726,452 | 53,755,432 [12,070,144] (19,264,204) / 0 [0] (7,612,608) / 12,792 [3,668] (71,186,644) | 1,598,400 [1,598,400] (7,671,552) / 50,350,944 [105,116] (18,469,916) / 14,551,000 [14,519,304] (59,189,868) | `d932ad3a788fad22af676487e3f73096ce40bdb06cb3f2200d9ccd5fda40aaf8` |
| `kcopycatkdragon.uftb` | King+Copycat vs King+Dragon | 1,071,866,264 | 5,950,664 [5,907,152] (8,221,984) / 21,954,560 [1,129,184] / 389,272 [352,720] (1,441,440) | 26,998,624 [11,898,808] (9,327,696) / 26,344 [7,640] / 163,816 [163,240] (1,441,440) | `9137d64862b6e6c611962039c061fe89770b08e24ade699500ecb9ee48e51014` |
| `kcopycatkgiant.uftb` | King+Giant vs King+Copycat | 501,307,408 | 14,914,280 [6,207,776] (5,611,880) / 34,832 [240] / 4,136,952 [223,616] (13,259,976) | 73,856 [32,824] (5,684,048) / 7,549,984 [7,880] / 11,390,056 [6,894,872] (13,259,976) | `3e12e89432d45f030e68a843332a8757388bcc268228d7b25f27763a490a7f6e` |
| `kcopycatkknight.uftb` | King+Knight vs King+Copycat | 901,111,056 | 8,216,824 [5,917,440] (8,221,984) / 16 [0] / 20,077,656 [555,000] (1,441,440) | 80 [16] (5,405,568) / 987,552 [16,728] / 30,123,280 [8,338,416] (1,441,440) | `0be57fdeb8826649cbc8910da56cd9c57cae84dacee24caa06c245b9d257ba99` |
| `kcopycatknightk.uftb` | King+Knight+Copycat vs King | 957,754,704 | 26,419,624 [0] (10,088,568) / 0 [0] / 8,288 [0] (1,441,440) | 0 [0] (3,098,976) / 28,751,160 [2,242,800] / 4,666,344 [4,642,560] (1,441,440) | `d9739280d824e405b664bbeeea2911233aca9d336ff90671d9260271d1392486` |
| `kcopycatkpawn.uftb` | King+Pawn vs King+Copycat | 1,630,018,804 | 38,167,160 [11,424,968] (22,084,440) / 8,982,700 [3,620] (104) / 3,797,596 [212,608] (2,883,840) | 21,266,620 [13,313,660] (7,901,188) / 28,728,548 [471,624] (5,668,856) / 8,822,696 [4,616,640] (3,527,932) | `ec2e86807cad1a0e48a35fef8320f1234b846ca6cc5db6d2e2521800c4cecb29` |
| `kcopycatkprince.uftb` | King+Prince vs King+Copycat | 1,309,937,520 | 6,043,944 [5,904,928] (8,221,984) / 21,894,696 [128] / 355,856 [350,104] (1,441,440) | 30,510,808 [8,551,472] (5,955,168) / 48,928 [0] / 1,576 [472] (1,441,440) | `2a606ed0abb2544f0c59d11d5faf4537b803a4ea53129ea9efc6dfeea637bd06` |
| `kcopycatksniper.uftb` | King+Sniper vs King+Copycat | 3,170,789,152 | 50,858,316 [12,440,844] (83,746,252) / 4,392 [0] (288) / 5,726,284 [227,644] (11,496,148) | 51,284 [28,288] (14,139,100) / 45,434,024 [61,756] (49,447,588) / 19,622,372 [12,724,808] (23,137,312) | `62657cf05d173f3e63cb6a50c293a6bc7f3c0f70b4622aa640d0e6ab8daa8629` |
| `kcopycatpawnk.uftb` | King+Pawn+Copycat vs King | 1,658,761,360 | 49,924,420 [7,703,888] (22,721,336) / 0 [0] / 700 [0] (3,269,384) | 0 [0] (6,197,952) / 57,614,900 [10,301,920] (5,191,608) / 2,541,676 [2,539,772] (4,369,704) | `9922bb573ffcbb425f61da13f304db3016eaec39172a3504eea2e881f3a4a62e` |
| `kcopycatprincek.uftb` | King+Prince+Copycat vs King | 1,374,716,520 | 25,893,408 [0] (10,623,072) / 0 [0] / 0 [0] (1,441,440) | 0 [0] (3,098,976) / 33,403,112 [6,757,336] / 14,392 [14,392] (1,441,440) | `dcdd63f5420344fa0b1ab49a2c1a8d1c688a5025644871a3395ead8081c60a90` |
| `kcopycatsniperk.uftb` | King+Sniper+Copycat vs King | 3,224,851,984 | 55,208,768 [0] (90,789,700) / 0 [0] / 30,952 [0] (5,802,260) | 0 [0] (12,395,904) / 57,106,868 [4,377,424] (57,085,436) / 9,728,140 [9,646,164] (15,515,332) | `11d685a7d7673ef3f4458ef42913f04f2034be32f3432a31e64fcfa3c0403021` |
| `kberserkerberserkerk.uftb` | King+2 Berserkers vs King | 95,392,004,308 | 95,908,728 [0] (853,039,272) / 0 [0] / 0 [0] | 0 [0] (80,480,400) / 868,188,006 [44,665,896] / 279,594 [279,594] | `a1f07eae1bc073927fe220cae1df17428b3b3b94183614c6f18698dafa3529ff` |
<!-- GENERATED_TABLE_END -->

Each W / L / D cell is from the perspective of the side to move named by its
column. Parentheses separate unreachable dense-index states and exclude them
from the preceding result count. They are proven causal impossibilities, not a
claim that every unparenthesized state is reachable from a particular Ranked
deployment. A missing parenthesized value means that outcome's certified
unreachable bucket is zero; it never means the reachability pass was skipped.
Square brackets identify the authenticated trivial subset already included in
the preceding admitted W/L/D count: immediate stalemates and positions forced
to simplify immediately through a hanging capture, check/fork/skewer, or pin.
For every Prince class, W/L/D, reachability, and bracketed trivial counts are
restricted to exact ordinary turn boundaries (`cont=0`). The packed tablebase
still contains and probes the Prince's required second-move continuations
(`cont=2`); those internal continuation records are not starting positions.
The plot subtracts bracketed subsets. The authenticated backfill is complete:
all 451 certified rows—389 concrete and 62 public-information—carry an explicit
bracket on all six W/L/D components, and `[0]` means the certified trivial
subset is empty.
For example, all 492,960 bare-King-to-move records in `K+Jester vs K` pass the
necessary-reachability audit, and `3,272 + 414,344 + 75,344 = 492,960`.
The private legal-dot partition changes their public-information outcomes, not
their reachability classification. Every generated row has a SHA-bound
native audit using the same royal-safety simulation as move legality: at an
ordinary turn boundary, the previous mover cannot have left its real King
threatened unless that side still owns a live Jester. The audit is
outcome-independent and includes indirect royal kills through Bomb chains and
other character effects. Stateful audits union that condition by state identity
with recovered causal failures such as impossible promotion squares, Sniper
cooldown/turn parity, an active Penguin aura with no
frozen model, an impossible forced-action turn, or a turn that could not survive
the frozen lone enemy King. A Jester exemption is color-specific; the 41,808
concrete bare-King-side wins in `kjesterk` remain reachability-admitted because
the previous mover owns the Jester. Under the information game only 3,272 are
uniformly forceable wins; 38,536 become draws rather than unreachable records.

Stored concrete objects use packed format version 4 (or v5 when a character has a second
linked/material model): a two-bit WDL plane, one-byte DTW
plane, and a sparse exact exception stream for distances of 255 or more. This
is 37.5% smaller than the former 16-bit records before entropy coding. Every file
was validated after retrograde propagation by
regenerating all legal successors and checking the Bellman equations for every
state. Giant anchor tuples whose 2x2 footprint is off-board or overlaps a King
are explicit draw sentinels and can never be produced by the probe.

The state dimensions preserve Ghost visibility, Sniper cooldown, both Prince
action phases, Pawn moved state, and the Penguin's inactive/active exact
geometry-derived freeze aura. Penguins have no native cooldown. Pawn
promotions cross-probe the exact Queen table. A double-step
en-passant marker is quotient-equivalent here because no opposing Pawn exists
to use it.
Copycat is one deployable character with two board models. The K+Copycat-v-K
and K+Copycat-v-K+Bishop classes index it as one compound piece: the linked
half is reconstructed at the exact horizontal mirror of the selected half
rather than storing arbitrary clone coordinates. The next expansion applies
that same explicitly requested unsplit-start simplification to 36 sufficient
K+K+2 Copycat material classes. Each pair has one indexed Copycat anchor and a
derived linked clone. Copycat+Copycat uses two such linked pairs and folds the
same-team pair exchange. Singleton Copycats and independently displaced halves
remain outside this planning domain. Both orientations of Copycat with Penguin,
Mage, or Fisherman are skipped because those pieces can split the linked pair
within the class; no out-of-domain successor is silently scored as a draw.

Angel format v9 indexes one exact Angel graph without enlarging the board
placement tuple. Its dense Angel slot names either the deployed Angel square or
the independent live Halo square. A two-state opposed substate distinguishes
deployed from attached-to-own-King; a three-state same-team substate also
distinguishes attachment to the companion. Decoding reconstructs the off-board
Angel, reciprocal Angel/Halo link, host identity, and canonical rescue order.
The generator rejects malformed graphs and verifies that sampled Link and
ordinary successors either remain in this exact domain or enter an authenticated
lower-material table. Although a double-Angel history can contain an ordered
rescue stack, both Angel/Angel material cells have no attacking non-King piece
and are exact insufficient-material draws. They therefore need neither a graph
codec nor computation. Ghost pairings remain deferred until their perfect-recall
information states carry the attachment graph without disclosure. Jester/Angel
is closed by this v9 graph and the primary-Jester public-observation solver.
Opposed Copycat/Angel is now also exact in v9: the Angel can attach only to its
own King, never to either enemy Copycat half, so the existing intact-pair codec
needs only the ordinary deployed/attached Angel bit. Same-team Copycat/Angel
remains deferred because attachment to one half plus linked death of the other
can leave a live off-board orphan Angel outside that domain.
The opposed codec passed 20,000 state round trips and 24,000 sampled legal
transitions, including 515 Link transitions, plus deployed and attached runtime
probes and an explicit rejection of a same-team file header. Its immutable v7
source bundle is
sha256:7da3ff80da46940d4fc7e2a3a2c0fd49b9968fa12e1a3bc673540a71c00b0e01,
S3 VersionId `XOJjiA7mcqd_jTn98ZgKaVyEV7_k6ObV`, under model
sha256:edb3c62023be556e8b87167419db14dcfee639b63ee3fd7c25b5d36162bf9e7a
and inventory
sha256:46dbf01ff6b8e80a7bc0b7ecce66340944fdec4aae197e46d81e95d1d5fa289d.
The exact 151,831,680-state run completed all 1,677,587,360 edges, propagation,
and exhaustive Bellman verification in 237.52 seconds. Its peak predecessor
extent was 6,710,349,440 bytes, well below the 16-GiB reverse gate. Pinned S3
download and restore verification certified table/archive
sha256:2b03a2dfccaf05a69f8f6529af33dafbe08bb647afe82c4ebe9d9e0683e6c95a
VersionId `MqOQP1BBvdLxwGRdcYUY3L6DfOnLd95w`, generation certificate
sha256:1da5992bb43a211f012c8a052fc151a09d70af7d611d5b51ec8a5e4d077a6043
VersionId `4daOOaSYnmcPmTzozlNszTBkYNe9_uFf`, and reachability
sha256:880f0fb3280c0801529b8003c3437d350b0cfecf78e15d0b3c6db252d3e9d751
VersionId `OiNdsgva1y6TSUERaOneFWH.LmGr2xV3`. The supervisor independently
reports the row **CERTIFIED** with all six exact S3 bindings and no errors.

`tools/tablebases/plan_ultimate_tablebases.py` defines the supported codecs and estimated
storage for the expansion; the ledger above is authoritative for operational
status and artifact identity. The planner applies horizontal-reflection
canonicalization and budgets separate two-bit WDL and byte DTW planes. S3
stores content-addressed compressed archives while certificates retain the
logical-table SHA and byte extent. The planner treats entropy compression as extra margin,
not as a speculative assumption when enforcing the original 10 GiB target.
The explicitly requested K+Copycat-v-K+Bishop and K+Dragon-v-K+Penguin tables
use a narrow documented exception, capped at 160 MiB beyond that target.

Production tables are generated by the committed AWS runners with piece-tagged,
resumable checkpoints. After generation,
`tools/tablebases/audit_ultimate_tablebase_reachability.py` records the
native turn-boundary audit against each immutable table SHA-256 before
`tools/tablebases/update_ultimate_tablebase_readme.py --full` rebuilds this summary. The
final publication pass additionally uses
`--concrete-certificates RESTORED_CERTIFICATES --require-certified-compression`.
That mode accepts sizes only from content-addressed, versioned-S3 certificates
whose HEAD, fresh download, full SHA-256, and archive restore checks passed,
and binds each compressed byte count back to the logical table SHA and extent.
Lone Bishop, Knight, Turtle, Mage, Checker, Fisherman, and Angel classes need no
file because the recovered native insufficient-material rule makes each an
immediate draw. Sludge and arbitrary-starting-Minions Devil domains remain
deferred spawning families; causally placed starting Minions are included in
the narrower Devil-spawned-only closure, which is the
authenticated campaign described above. The
remaining Angel deferrals are unsupported ordered or hidden-information graph
topologies, not claims that the material is insufficient.

Of the 182 possible stateless K+K+2 material classes, 160 have exact results and the
remaining 22 are omitted as immediate insufficient-material draws. The 22
omitted classes are King+Knight+Mage, King+Knight+Fisherman, King+Turtle+Mage,
King+Turtle+Fisherman, King+Mage+Mage, King+Mage+Fisherman, and
King+Fisherman+Fisherman versus a bare King, plus King+Knight versus
King+Knight, Bishop, Turtle, Mage, or Fisherman; King+Bishop versus
King+Bishop, Turtle, Mage, or Fisherman; King+Turtle versus King+Turtle, Mage,
or Fisherman; King+Mage versus King+Mage or Fisherman; and King+Fisherman
versus King+Fisherman.

The original 10 GiB budget admits 29 stateful classes: K+Bomb+Ghost,
K+Bomb+Penguin, K+Bomb+Prince, K+Pawn+Bomb, K+Bomb+Checker, K+Bomb+Sniper,
K+Berserker+Bomb, K+Bishop+Ghost, K+Bishop versus K+Ghost, K+Bishop versus
K+Penguin, K+Bishop versus K+Prince, K+Bishop+Penguin, K+Bishop+Prince,
K+Bomb versus K+Ghost, K+Bomb versus K+Penguin, K+Bomb versus K+Prince,
K+Ghost+Dragon, K+Ghost+Fisherman, K+Ghost+Ghost, K+Ghost+Giant, K+Ghost
versus K+Dragon, K+Ghost versus K+Fisherman, K+Ghost versus K+Giant,
K+Ghost versus K+Mage, K+Ghost versus K+Parasite, K+Ghost+Mage,
K+Ghost+Parasite, K+Jester+Ghost, and K+Jester versus K+Ghost. The narrow
approved overrun adds K+Copycat versus K+Bishop and K+Dragon versus K+Penguin.

The AWS expansion inventory separately contains all 232 closed stateful
K+K+2 classes (87,872,584,800 packed bytes) plus 36 unsplit-Copycat classes
(6,784,978,200 packed bytes), for 268 supported classes and 94,657,563,000
packed bytes. Its promotion dependency waves are 226 classes / 85,832,346,600
bytes without Pawns, 40 classes / 8,540,532,000 bytes with one
Pawn, and 2 classes / 284,684,400 bytes with two Pawns. Pawn waves depend on
the complete preceding wave; in particular, Copycat+Pawn depends on the
mirror-domain Copycat+Queen table.

The 32 one-Angel source graphs add 9,821,611,800 packed bytes, bringing the
current supported source inventory to 300 classes / 104,479,174,800 bytes.
The combined waves are 256 classes / 95,179,484,400 bytes without Pawns, 42
classes / 9,015,006,000 bytes with one Pawn, and the same 2 classes /
284,684,400 bytes with two Pawns.

The legacy K+Penguin table and five K+K+2 Penguin payloads used the former
geometry-derived aura bit: `kpenguink.uftb`, `kbishoppenguink.uftb`,
`kbishopkpenguin.uftb`, `kbombpenguink.uftb`,
`kbombkpenguin.uftb`, and `kdragonkpenguin.uftb`. Their immutable legacy
archive is retained as provenance, but they are **PLANNED**, not preserving or
certified: the current codec records exact causal freeze membership and has
twice as many indexed states for K+Penguin and four times as many for the
K+K+2 materials. They must be generated once under the current model; their
legacy W/L/D must not be imported.

The remaining deferred sufficient K+K+2 materials contain Sludge or one of the
six separator-asymmetric Copycat pairings named above. The complete
single-Devil spawned-only class is **CERTIFIED** by the twelve-partition
stateful certificate described above. Its companion Devil materials are
**PLANNED** under the first-three-ranks/causally-spawned-Minions-only
simplification; those companions await their additional codec dimensions rather
than being deferred. Starting Minions are included when earlier spawns by the
indexed Devil can account for them; arbitrary starting Minions and persistent
Goop remain outside
the indexed scope. The exact
one-Angel source graph now covers 32 material classes, including both
Ghost/Angel orientations. Those two classes remain `information required`
until their perfect-recall hidden-Ghost attachment overlays are certified;
their concrete graphs must not be published as public-information results.

## Computation operations

The computation ledger at the top of this document is the only authoritative
class-status inventory. It contains all 624 unique material cells shown by the
plot: 24 single-character classes, 300 unordered same-team pairs, and 300
unordered opposing pairs. Its `Indexed states` column is the exact dense codec
size where a supported codec exists. For a certified row, the two result cells
and the explicit admitted/unreachable totals are the latest exact
reachability-aware values. `information v2` means the values came from the
uncapped observation game; `information required` means a concrete dependency
must not be presented as a public result.

Statuses have strict meanings:

- **CERTIFIED**: exact W/L/D and reachability values are available and the
  storage column names their versioned-S3 preservation binding.
- **PRESERVING**: a completed, internally valid result is in the mandatory
  archive/restore, W/L/D extraction, reachability, or ledger-import tail. It is
  not a catch-all for dependency gates, active diagnostics, or a retained but
  invalid proof. Exact W/L/D cells are shown only after the table and its
  reachability sidecar are complete. There are currently no rows in this tail:
  Rook/Ghost and Turtle/Ghost were imported, while the five stale labels were
  reclassified according to their actual work state.
- **COMPUTING**: an authenticated current-model solver is actually consuming
  CPU; the plot renders the cell with diagonal hatching. A sleeping
  certificate/resource-gated wrapper remains **PLANNED**, or **PRESERVING**
  when it already owns an authenticated retained phase. A prior local result
  is not shown as current while this status is set.
- **DRAW**: the native insufficient-material rule closes the entire class
  without a table file.
- **PLANNED**: the class is supported but no current result is certified or
  running.
- **BLOCKED**: intervention is required; quiet progress is never classified as
  blocked.
- **DEFERRED**: excluded from this campaign. This covers Sludge/Goop plus the
  six Copycat pairings with Penguin, Mage, or Fisherman
  that can split its linked mirror. Ghost/Angel source graphs are in scope but
  remain information-required until their hidden-attachment overlays finish.

Dependency queues use artifact gates, not a transient unit's cached
`Result=success`: a concrete predecessor must have both its
`certificates/wave-certificate.json` and named `.uftb`, while an information
predecessor must have its result-only `work/artifact-manifest.json` and named
`.ufiw`. This is necessary because collected transient units can later report
the default successful result after a failed unit object has disappeared.

S3 is the canonical data store. The repository contains code, the ledger, and
the plot, not production table payloads. Every deletion requires a
content-addressed object in a versioned bucket plus HEAD, fresh version-pinned
download, full-SHA, and archive-stream restore certificates. A computation
must consult the ledger before staging; **CERTIFIED**, **COMPUTING**, and
**DRAW** rows must never be launched as new work. Checkpoints and uncertified
scratch remain resumable and are not deleted merely because a job is quiet.

For an ordinary concrete class,
`tools/tablebases/launch_ultimate_aws_concrete_class.py` preflights one exact
inventory index on one staged host, starts one bounded unit, and changes the
ledger only after launch succeeds. The AWS runner produces one retained work
directory and one versioned-S3 certificate. Finish that class exactly once
with `tools/tablebases/finalize_ultimate_aws_concrete_class.py`: it checks the
successful unit, runs the native full-causal reachability audit against the
retained output, uploads the content-addressed sidecar, and imports the exact
per-side W/L/D split here. The finalizer refuses information-required rows, so
a concrete Ghost or Jester dependency cannot be mislabeled as a public result.

The 217 legacy local payload files (10,936,783,935 physical bytes) were removed
after preservation under
`legacy-local-tablebases/v1/snapshots/sha256/e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263/`.
The archive SHA-256 is `e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263`
and its S3 VersionId is `YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ`. The separate
certificate SHA-256 is
`1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d`
with VersionId `5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY`; its local and fresh
version-pinned archive stream checks both authenticated every file with zero
residual. The corresponding 200-table native full-causal reachability receipt
has SHA-256
`52887f4e29eab71e92dd0aaec2019f4a5a3f59b8010ad4d4de7f0816559f889a`
and VersionId `dnNKI2aUC80gBjlcQWpvjNLGrcmH0fen`. Six legacy Penguin
payloads use the obsolete aura codec and are intentionally absent from that
receipt; the current K+Penguin result above replaces its single-character
member.

The AWS supervisor reports both CPU-set reservations and measured consumption.
For every continuously active systemd unit it differences cumulative
`CPUUsageNSec` across polls and records the sample interval, CPU-time delta,
average busy vCPUs, and utilization of the unit's allocated CPU set. Host
totals report measured busy vCPUs and percentage of physical vCPU capacity.
CPU-time samples are retained in supervisor state and included in regular
reports, but do not create five-minute change notifications by themselves.
The resource scheduler backfills every dependency- and source-certified job
that fits measured RAM and projected allocated disk until the host reaches a
70% measured-utilization target. Single-threaded solvers normally reserve one
disjoint CPU; fleet throughput comes from running independent classes in
parallel, not from assigning idle cores to a serial solver. Oversized live CPU
sets are shrunk in place without restarting the unit. Overlaps remain a
resource warning even when sampled utilization is low.

Every concrete class has an explicit queue record, unit, work directory,
per-class dependency list, and conservative resource envelope. There is no
operational "remaining wave" job. The five-minute collector certifies finished
work, immediately backfills newly freed capacity, and updates this ledger. Two
complete samples below 50% utilization while runnable or stageable work exists
raise **UNDERUTILIZED** and delegate preparation of another source-pinned batch
of 10–30 units. An idle instance may be stopped only when no safe runnable work
exists, all useful results are version-pinned and restore-certified in S3, and
no unarchived checkpoint or instance-store scratch would be lost.

Update current statuses with
`python3 tools/tablebases/update_ultimate_tablebase_ledger.py --set-status FILE=STATUS`
and require
`python3 tools/tablebases/update_ultimate_tablebase_ledger.py --check-launch FILE`
before staging new work (`--resume` requires an existing **COMPUTING** row).
and regenerate the checked-in plot with
`python3 tools/tablebases/plot_ultimate_tablebases.py`. The plot reads the
ledger plus the authenticated Berserker-radius, Giant-start-class, Devil
current-Minion, and Checker root-type summaries; it neither scans local
payloads nor guesses completion from filenames.

### Penguin/Ghost terminal-source correction (2026-08-27 18:36 UTC)

The `opposed:ghost+penguin` row's earlier “current-rule concrete rebuild”
narrative is superseded. That generator enumerated the legal frozen-King mating
capture at source index 318,855,161 but classified its already-terminal child
through a lower-material table because it did not test `child.game_over()`.
It consequently stored a draw where the mover has a win. The concrete
initializer and independent Bellman verifier now share an explicit
terminal-successor predicate, with a permanent self-test for this witness.

Immutable corrected source
sha256:a66ab20828a2f981737f68efcb41f8b08323319136f8273737cf215e1ed25ec0
is S3 VersionId `IJy9WzKY87DKDCpTcE.mo1SvOs1c.Zwi`; binary
sha256:94a5fafa0de8421cb61107bdc4560da039f999558a70cdcaf5c22d6d92df53c1
is VersionId `uen2kjPPiMW_mifeiX_z.981qKb31Fj8`. A fresh, non-checkpointed
607,326,720-state rebuild completed with 1,075,040,760 edges and W/L/D
25,194,856 / 3,394,442 / 578,737,422. It passed the full Bellman proof and
the exact witness-byte gate; output
sha256:06dc64a6ae06df68df65c9819de7a8abec06c124b59ea5cd5010a60769b84b64
is exact-restore authenticated as S3 VersionId
`LPA.Ol.SVqn5RvxaS.qA5DmoLQmyzxxn`. No Penguin/Ghost information result may
be published until the retained information fixed point passes its exhaustive
singleton audit against that corrected source.

At 18:57 UTC v9 failed before entering the retained solve because its wrapper
omitted one character from the 64-hex lower-observation hash. The source table,
transition rebind, and fixed-point arrays remained authenticated and unchanged.
V10 fixes that transcription, explicitly requires the v9 failure evidence,
and resumed the retained fixed point on i-03 CPU 2. Wrapper
sha256:56ee885dbee3a41475cd45792899fa3b1073a28cedc509dcd466798944e3a001
is S3 VersionId `nwLeIIG5jyAkZz2vBkng5wvUySQLzjub`; unit
sha256:0c6a39542f8d433c8703d34b49e0f7d9ffdd9bb7d30c198da9417f689806339f
is VersionId `ZlFhBWI8SfEdQM6DDXKhH_qxIA8_tHnn`.

At 18:19 UTC the C1 Bishop+Devil v23 process was OOM-killed immediately after
rebuilding the exact 5,097,463,688-key retained index: the 224-GiB cgroup was
full, while its checkpoint remained unchanged and no `.roots` fragment
existed. The explicitly approved 3-TiB gp3 volume now supplies a bounded
128-GiB swapfile at
`/mnt/ultimatefish-devil-v11/ultimatefish-devil-c1-v24.swap`; its mode is
0600, the cgroup swap gate is unlimited, and the volume retains more than
2.89 TB free. V23 was restarted on the same disjoint CPUs 0-7,16-31 with the
unchanged 18-billion proof/hash gates and immutable source/binary bindings.
This is a checkpoint resume, not evidence of completion; the host audit
separately verifies the durable checkpoint and the bounded gp3 overflow file.

The following sustained sample proved that overflow alone was not an adequate
fix: v23 filled its 224-GiB cgroup, swapped the anonymous 11,559,984,176-byte
retained-frontier copy through the gp3 device at 98% utilization, and left only
its main thread runnable. V24 removes that duplicate allocation. On resume it
maps the authenticated frontier read-only and sequentially advised, so its
clean pages remain reclaimable; newly generated layers retain the existing
atomic checkpoint installation and exact-key semantics. The reverse-edge and
checkpoint-migration self-tests passed. Immutable source
sha256:bf3c49ccca68a00cb3733c7105e25b3689e91dbd8f05a5d9127616369511ab7c
is S3 VersionId `ctQNJyTlyEf6vcOibC0vcnLC7QLNl.oE`; exact-restored binary
sha256:f655cd0ec7d3981b4f7da8e1f36bf792e1865b6e01f85208611f7cd8ef12c2bd
is VersionId `wJE2AURbjHO_OVBpYezSszX4wNyt_SeU`. C1 v24 resumed the unchanged
5,097,463,688-state checkpoint on CPUs 0-7,16-31. Its fresh rebuild sample has
24 workers at roughly 97.6% each, 121,955,143,680 bytes resident, negligible
memory pressure, and no active swap I/O. Completion still requires a new
complete closure layer, retained-edge retrograde, exhaustive Bellman proof,
and authenticated fragment preservation.

At 20:25 UTC A1 v21 completed its 19,568,521,291-state parallel graph scan but
failed before writing any reverse shard: `devil-0.reverse-spool` was a retained
symlink to a missing directory on the mounted checkpoint-migrate volume. The
closure checkpoint and key plane remained unchanged, and no fragment was
published. The exact target was created only after verifying the mount and
1.37 TB of free space. V25 now also fails fast unless the resolved spool path
is a writable directory, and reduces the multi-billion-entry degree plane in
parallel instead of leaving 31 CPUs idle during a serial sum. Its reverse and
migration self-tests passed. Immutable source
sha256:19fa50ebfa1f5b0319fac5d3f3957d0f37ff5bbe600a22619e6bb93eaf631728
is S3 VersionId `XJkADC_Sn4K6eVKFjaSe6qkD6XmEbbsP`; exact-restored binary
sha256:5732a5db776e2572e1401ce5c4dc2fda90e11ad84ddbffcb3ddd1efdd435e6ec
is VersionId `yZElOHwKruG670rGNILIsCK9gZQ2SVKh`. A1 v25 resumed the unchanged
checkpoint on CPUs 1-21,31; launch is not completion, and the repaired reverse
spool must still produce authenticated shards before retrograde can begin.

### Devil C1 64-vCPU resume and B1 scaling gate (2026-08-27 23:54 UTC)

C1's retained closure, frontier, and exact keys were copied byte-for-byte from
instance store to the approved 3-TiB gp3 volume before the host was stopped.
Independent source and destination SHA-256 manifests matched, and migration
receipt sha256:`0ca103bc4babb4f6b27e98925f33a4009b916a3c08299e7c80bf4bab6233c7bf`
binds closure
sha256:`179d1d0aed2bfbeed33cbefa96f5ce190227a6f1d40da64666a36348f8cf71fd`,
frontier
sha256:`681785795969fb04a2095c7ba274c5c5cb79317e0a391faf57065a5415b4e0f0`,
and keys
sha256:`ca7f2f57add074b89276a0c4667d6825608e542644e11e659cfbedd5653f451f`.
The co-resident Prince/Ghost checkpoint was separately preserved and restored
with matching manifests before its scratch device was replaced.

Instance `i-08c0f44a1776cb34a` is now `r8gd.16xlarge` with 64 vCPUs and
512 GiB RAM. V27 removes the obsolete 32-worker validation ceiling while still
rejecting requests above detected hardware concurrency. Immutable source
sha256:`de523aad058723e70820d7cd645fc4ec3a23a0422e7348c836bac29b8f06f214`
is S3 VersionId `VlBg9ai6PixKEldf0wlYAIM0USJwvmfk`; restore-tested binary
sha256:`6b8fe2bfab9f76fcbfdedfbf67119cae418a749c86b615b40004be3e5b36ea68`
is VersionId `Mh9h30fIAXagqVQUWn51_xmhQtNrJvMO`. C1 resumed with 48 workers and
40-billion proof / 32-billion exact-hash gates. Its parallel index phase
materially scaled, advancing roughly 4.7 billion entries in about 40 seconds;
the following exact-key pass then exposed a separate single-device IOPS limit,
with 48 workers blocked behind a 95%-utilized device. This evidence satisfies
the conditional gate for resizing B1, but also requires B1's two new
instance-store devices to be striped for scratch rather than repeating the
single-device layout. Completion is still not inferred from either launch or
index completion.

Prince/Ghost resumed on disjoint CPUs 48-55 and all eight Bellman workers are
active. Its restored lower-material inputs are now explicit supervision
checkpoints, closing the preservation gap that previously left those exact
dependencies outside the resize receipt.

### C1/B1 restart acceleration and A1 storage failure (2026-08-28 03:37 UTC)

C1's authenticated gp3 checkpoint was copied onto the resized host's local NVMe
only after checking the device model, serial, size, and exclusion of the gp3
source. Source and destination SHA-256 manifests match exactly. Migration
manifest sha256:`e7e378845b79bc7389779c7547049bc65b06f443308990a0bdfbed34d27418fe`
is S3 VersionId `A0vSrLt1NozCrqmu4qVqyvtedOg6WpK9`; receipt
sha256:`9afc3d413d6159047a3375229b0883e47d415b6ebb718a50243ca4f4055cdac0`
is VersionId `MzoPJ8fBAZ0SaR4GnNcgYCLLnY3FZoHP`. The gp3 source remains untouched.
The generic resume launcher was fixed to create a fresh log directory before
starting a new unit, so the first v30 launch failure did not touch the retained
checkpoint. C1 v30 now runs 48 workers with 40-billion proof and exact-hash
gates; a 30.111-second exact cgroup sample measured **40.910/48 busy vCPUs
(85.2%)** while its retained index advanced.

B1 v29 failed closed at its 32-billion hash-capacity gate after committing
closure ply 16 at 25,155,000,000 states. V31 resumed that exact retained layer
with 40-billion gates and 48 workers; a 30.010-second cgroup sample measured
**47.150/48 busy vCPUs (98.2%)**. This demonstrates worthwhile scaling without
resizing B1: its current 64-vCPU host already supplies the tested worker set.

A1 is not running its Devil reverse-edge pass. V25 stopped after
9,402,063,174/19,568,521,291 parent states because the separate 2-TiB persistent
EBS reverse-spool volume is full. The checkpoint and partial spool remain
retained, but the 16-byte record layout projects roughly 3.62 TB for the final
spool plus about 0.4 TB of inputs. Safe continuation requires expanding that
volume to 5 TiB, or completing and validating a compact restart-compatible
spool format. This failure is not represented as progress or completion.

The checked-in supervisor's complete 93-second fleet sample measured i098
0.999/32, i024 40.581/64, i03 4.990/32, i08 40.186/64, and i0b 21.461/32 busy
vCPUs: **108.217/224 (48.3%) fleet CPU**. The below-75% state remains an
engineering failure; A1's stopped 31-core allocation and the serial Ghost tails
are the remaining immediate deficits.

### Forty-eight-hour certification sprint and A1 recovery (2026-08-28 04:18 UTC)

The active objective through 2026-08-29 20:42 America/Los_Angeles is to maximize
fully certified canonical classes. Terminal solves awaiting verification,
preservation, reachability import, and ledger promotion take precedence over new
censuses or long speculative work; those stages are one mandatory continuation.

With explicit approval, A1's attached gp3 volume
`vol-000eed5cee33af507` was expanded in place from 2,000 to 5,000 GiB while
retaining 40,000 IOPS and 1,000 MiB/s throughput. Stable attachment identity
bound it to `i-0986ed3d272721f02` at `/dev/sdf`; the guest exposed it as
`/dev/nvme1n1` with serial `vol000eed5cee33af507`. XFS grew from 524,288,000 to
1,310,720,000 blocks and reported 3,198,741,401,600 free bytes afterward. V25
then resumed the unchanged A1 checkpoint on CPUs 1-21,31 with exact source
sha256:`19fa50ebfa1f5b0319fac5d3f3957d0f37ff5bbe600a22619e6bb93eaf631728`
and binary sha256:`5732a5db776e2572e1401ce5c4dc2fda90e11ad84ddbffcb3ddd1efdd435e6ec`.
All 22 workers were runnable at about 93.6-93.8% in the first authenticated
status sample, with zero cgroup memory pressure. Launch is not completion; the
reverse spool, retrograde solve, exhaustive verification, preservation, and
ledger import all remain required.

### Certification-sprint terminal continuation (2026-08-28 05:00 UTC)

The exact supervisor measured 59.736/224 busy vCPUs (26.7%). This remains below
the normal engineering threshold, but the sprint deliberately preserves the
advanced fixed-point state of the most certifiable jobs instead of restarting
their final uneven geometry chunks merely to improve the utilization number.
Prince/Ghost is in iteration 8 at 960,000/985,920 geometries with all eight
workers runnable; Queen/Ghost is beyond 490,000/492,960 in iteration 5;
Ninja/Ghost is beyond 490,000/492,960 in iteration 15; opposed Checker/Ghost is
at 3,875,000/3,943,680 in iteration 19. None has emitted a result, so none is
reported as complete or preserving.

To remove the former solve-to-preservation bookkeeping delay, tested watcher
v3 (`ultimatefish-certification-sprint-watch-v1.sh`) is installed on all five
authorized hosts from immutable S3 object
sha256:`8b71dc27a32f3afe1a4c545691ec20855d29a7729f52e73cdcc26c366e11fae2`,
VersionId `M3ZMrfl3vRAZd1KsWfshej3xDYlTajeH`. Each two-minute timer authenticates
the existing preservation tool
sha256:`9522ecb8d76991c9c0327b5861daf1ca6a7ccd780a882f99678f35602d45a47c`,
requires both nonempty information outputs plus a non-active solver. A
still-retained unit must report success; a transient unit collected before the
timer samples it may proceed only to the same exhaustive artifact verifier.
This closes the `systemd-run --collect` race without admitting a partial output
from a failed run. The verifier then creates, uploads, downloads, restores, and byte-verifies the
deterministic archive and preservation certificate. Existing preservation
directories are never overwritten. V3 also uploads the authenticated receipt
under a content-addressed `receipts/sha256/...` key, verifies its exact S3
version, size, and metadata hash, and retains a local upload receipt so an
interrupted final-mile pass resumes without repeating preservation. The timer covers the current Prince,
Queen, Ninja, Bomb, Dragon, same Sniper, opposed Checker, Penguin, and Pawn
targets; ledger import still authenticates the resulting receipt and creates a
separate result certificate before any canonical row is promoted.

The mislabeled opposed Berserker/Ghost v9 source remains quarantined. A corrected
Berserker oracle and exhaustive 1,518,316,800-state codec build are now
preserved, and the retained transition tree has been streamed into the
29,076,339,287-byte archive sha256:
`10709917e47875e13c3e8f873df0f9fff9c507a1546f692ff4d552fc193930ea`,
S3 VersionId `B2A.X4sRblLBMfwZl7nrFiHO4QNE19Fg`. Its authenticated restore to
i024 is active; only after table, Ghost sidecar, transition-marker, binary,
wrapper, and service hashes pass will the corrected fresh fixed point start.

### Saturated Ghost index repair and Checker handoff (2026-08-28 06:15 UTC)

`perf` on opposed Ninja/Ghost identified the fixed-point stall precisely:
38.56% of sampled cycles were in `pthread_mutex_lock` below
`ExternalRobdd::Impl::make/apply`, while its collision-checked unique index held
1,073,490,689 nodes in 1,073,741,824 slots (99.98%). Prince was still closer to
the same full-table boundary. This was not a large state-space tail or a memory
gate; open-addressing probe chains had effectively serialized every worker.

The checked-in ROBDD now treats the dense node arena as authoritative and the
unique table as a disposable acceleration index. Reopening with a larger
power-of-two index rebuilds that side index in parallel into a temporary file,
flushes it, and atomically renames it, leaving the original checkpoint index
untouched. Node publication and collision checks use striped locks; compaction
marking, copying, and replacement-index binding are parallel and preserve exact
structural/root residual checks. The exhaustive reopen test verifies every old
node ID and Boolean result after doubling the index.

Immutable Ghost v11 source
sha256:`2d2036ae1e0029a2341a11dda4548bfaf504c85bf918156e89910aeb18aec4b0`
is S3 VersionId `TL.n6_nbobhOgl8uC6UpC13K4TvikuRF`. Restore-tested Queen and
Ninja binaries are respectively
sha256:`2ce3c5651cc2fa07edfb45b6fb0dd69ee58a90c366f96c638719eee26368741b`
VersionId `EPgq4gm_eBLPwN7rnp_wzhSguMWQ0F5K` and
sha256:`9e6aadc7d14e4365de7fc519ba56912b16910ad53a75790ab4d76fe728221e3f`
VersionId `P24ZzGf_sZ0fBOaqtbUFz6lkVM3ugp75`. Ninja completed iteration 15,
compacted 1,089,007,074 nodes to 18,957,393 with zero structural/root residual,
and immediately advanced into iteration 16 with sixteen workers. Queen uses 28
workers and the expanded 2^31-slot index. Neither has emitted a result yet.

Prince must retain the exact v23 transition-marker semantics. A nominally
compatible v12 build failed closed on that dependency marker before opening the
solve arena. The exact v23 base plus the checked-in disposable-index rebuild is
preserved as v18 source sha256:
`29247cdf9878bd74a04535e83cde46105ee4c19a7d83737a7be303fe8e44fcd4`,
VersionId `uEooAJ6z8.HqB.vlZnuxWaANIVcaeMh6`; its binary sha256 is
`66215229b45b54a2fffef7788934fc33530de110b9bd54cb04f171c687919c7b`,
VersionId `k3QKM3OQ0CNMPIztfoKsHKl4e2e.e2fN`. V18 raises the legacy 97 GiB
scratch-admission ceiling to 192 GiB while retaining the independent free-disk,
physical-memory, and cgroup gates. Iteration 10 completed all 985,920
geometries and compacted to 18,092,847 nodes with zero structural and root
residuals. V19 now resumes exact parity `iteration=10,current,a` with a
3-billion-node arena and a parallel-rebuilt 2^32-slot unique index.

Iteration 8 later completed all 985,920 geometries with 3,608 changed owner
roots, 71,933 changed observer roots, and 1,007,455 changed visible roots. It
compacted 1,125,485,396 accumulated nodes to 12,441,688 with zero
structural/root residual. A newly observed unit-journal marker (the monitor
required that no iteration-8 marker existed when it started) then triggered the
authenticated v10 handoff: v9 stopped before doing material iteration-9 work,
the watcher moved from 8 to 32 threads, and v10 resumed exact parity
`iteration=8,current,a`. Resume wrapper
sha256:`7e5773e678b5421e33c5e8d36158ff1af1f9ca257851194f951ec7f5047ca8b9`
is S3 VersionId `6BrKzjzkijxkOJyPu9s0CFxHS__MjYh9`; handoff wrapper
sha256:`67f7f540304645192194c3cd4662efa18bdcae9a6227ff987b97fbd3a1a477ba`
is VersionId `nK_86KnlG5Wsll3NuX4_ke0eqiyI.aas`.

Opposed Checker/Ghost retained an exact iteration-19 compaction with
320,207,491 roots, 8,494,235 copied nodes, and zero structural/root residual.
The first parallel binaries failed closed before opening that checkpoint: v11
used a newer semantic base, v13 reversed the primary orientation, and v14
omitted Checker's four-substate, horizontal-only, and lower-draw compile
contract. The subsequently model-matched v15 run exposed a second code defect:
the reciprocal adapter duplicated the legacy serial Bellman loop and bypassed
the generic parallel sweep.

The checked-in v16 package routes that reciprocal adapter through the same
thread-safe, disjoint-geometry 32-worker Bellman and verification sweep.
Immutable source
sha256:`ed276d8278458ecf2d01e53740674cf0e065c159e302debc4d8878606461189b`
is S3 VersionId `k8dJlfvM3iBaaqzVYXW33vXWpo.jaJRY`; restored binary
sha256:`596fd34474e356bd4dddec00c82ed50a6160242b515e11dd298fe356fb23e3d4`
is VersionId `IXNerJkrcWvollvX8yi_HotmhxmoRUHd`. Unit
`ultimatefish-info-kghostkchecker-parallel-v22` resumed only the authenticated
iteration-19 parity (`next,b`) through wrapper
sha256:`98d8905ac3196a02c25ebc70aff9e279d8c30fb64bba58aaf5a6cfed0fba079a`,
VersionId `6UAilvq3DteRcTVSrWtan6PieWRqMK7O`; every partial iteration-20 attempt remains
non-certifying, and the certification watcher follows v22 with 32 threads. The
first production sweep reached 1,040,000/3,943,680 geometries in 66.1 seconds;
all 32 worker threads measured about 98.5% CPU, versus roughly 490 seconds for
only 325,000 geometries in the preceding serial attempt (about 24x higher
geometry throughput).

The same production continuation completed iteration 20 with 178,407 changed
owner roots and 179,757 changed visible roots, then compacted to 8,494,235
nodes with zero structural/root residual before entering iteration 21. This is
checkpoint progress only, not a completed or certified result.

V22 subsequently reached an exact fixed point at iteration 23 with zero owner,
observer, and visible changes, and its 32-worker independent Bellman equality
pass covered all 3,943,680 reciprocal geometries.  Profiling then exposed a
separate certification tail: the exhaustive concrete-singleton dominance
proof still ran serially.  V17 now partitions every placement, extra substate,
and visibility state into disjoint 1,024-placement chunks across the requested
workers; it retains every validity, adjacent-King exclusion, concrete WDL,
information-set evaluation, dominance, exception, and witness check.  There is
no sampling or changed proof contract.  Exact source
sha256:`9bfb45619bc35c8d67530a8b87f05362d47fd45c082c06126eaf84b6a17d13e6`
is S3 VersionId `dLVvNbEEHy0GlWKZaKRut1nhsFqzxsYI`; tested binary
sha256:`845edc1852e16e6f715e5df83cd78dcdcee0a361540cf170f214968b028ec0ae`
is VersionId `sPOPxmZTd8zG6srvIJS1aEXM.3Nm0Vkg`.  The first v23 launch rejected
an unsupported frontend-only `--resume-converged` option before opening the
checkpoint.  Corrected v24 wrapper
sha256:`2de381fe6344f5519aead75db992990da4ffe97b823638c8bdd6f4e66ddf9e6d`
(VersionId `IuAogra1fEOCD98kUsyhbNmTw7b4Zty0`) instead resumes the authenticated
iteration-22 `(current,a)` compaction, reproduces the already observed
zero-change iteration 23, and then runs the parallel equality and singleton
proofs.  Unit `ultimatefish-info-kghostkchecker-parallel-v24` and its
certification watcher are active; no result is inferred until both result
planes and the preservation receipt exist.

Direct process-delta profiling after v24's complete 3,943,680-geometry Bellman
proof found the remaining symbolic Bellman/monotonicity residual pass using
exactly one runnable solver thread.  V18 partitions its complete owner/visible
geometry proof into disjoint 32-geometry chunks and its complete observer
stratum proof into disjoint 256-stratum chunks; exact counters are reduced only
after all workers join, with no sampling or relaxed residual.  Source
sha256:`8a5cc9d4527059f24ebbbee79e115b433bfa2d052456dd61d10aaae8bd964a1c`
is S3 VersionId `vJRY8OeB2K.OX0KC3MFFkPEKKj_mgWtL`; its warnings-as-errors
build and external-ROBDD tests produced binary
sha256:`f93034ec5fc79d453b08620cc947c30f23f02b72dc3f30ca2c3a0155cc596b79`,
VersionId `jj4cS8rNVsxFAV77PxCnu_yL9bUsA4M_`.  With no result planes present,
v24 was stopped at that serial proof boundary and v25 resumed the unchanged
authenticated iteration-22 `(current,a)` checkpoint.  Wrapper
sha256:`879a714b5e2128b0dfce28568665583525e470bb9f5488bd0195d867d917303d`
is VersionId `Hjp14ORwKW5_3oNhwjAsgl97970cRAuD`; unit
`ultimatefish-info-kghostkchecker-parallel-v25` and the certification watcher
now follow it.  Completion remains unclaimed until the parallel symbolic and
singleton proofs, both output planes, archive restore, and preservation receipt
all authenticate.

V25 completed the exact parallel Bellman and symbolic equality proofs, but its
exhaustive singleton dominance gate correctly rejected the result with
66,367,025 violations; no output plane was published.  The witnesses isolated
an action-conditioning defect in owner-force propagation on observer turns:
the successor image was keyed only by transition observation, so two different
public actions rendering the same observation could be mixed into one belief.
This erased real owner wins, especially at forced Checker continuations.  V20
conditions owner successors by both the chosen action and observation, matching
the already action-conditioned observer recurrence.  Its exact source bundle
sha256:`99effde369bfa1a42b7fae1440827152fe30aac6187a674afb38335eeb52746a`
is S3 VersionId `PbDDTMXOMXAi9hgHUW8xEQAzaiQf0Hgk`; the warnings-as-errors build and
external-ROBDD suite produced Checker binary
sha256:`e7c136743ffcada8735ff9075b73de245b4e204290bcafbf7e40447a4f08e654`,
VersionId `3dL6UQDg0KQkjfEgBwm53IAvNTZKHF.9`.  V27 wrapper
sha256:`2c1d930af8ff3bb5a208997524c3ff6844402d8476218e635d6ae34c21b93cc8`
(VersionId `7PmuG799LDkmdfyMZKurQjC6xwkTz4Ca`) resumes the retained exact
iteration-23 roots in physical slot `next` and ROBDD arena `a`; unit
`ultimatefish-info-kghostkchecker-parallel-v27` is active on 32 CPUs.  The
failed v26 launch rejected an unsupported explicit worker flag before opening
the checkpoint.  Completion remains unclaimed until the corrected fixed point,
all proof gates, both outputs, preservation receipt, and ledger import succeed.

The identical v18 proof implementation was also compiled, tested against the
external-ROBDD exactness suite, uploaded, exact-version restored, and staged
without interrupting the live Prince, Queen, or Ninja solves.  The material
binaries are Prince sha256:`aaa993f77bc197f0219ebd99069872d63cff9304dd37db8177dcd77762245a0a`
(VersionId `QvXuHk36NLYTJFUlNGvyLDs6BbFJg6tX`), Queen
sha256:`82e28749759c9507af64af4a68526937eb10beabb8a059fc47b345c3852da923`
(VersionId `emT9SP9_R1bEd7Bx_MV3zYtaWnakuVM6`), and Ninja
sha256:`321d503533587df796ef19b95bdb7760ecf57c749291af73c6c4aa74e52741e9`
(VersionId `Z8diEVhMc.0nLV9nju74oCvyrMhLyRZS`).  These are standby verifier
binaries, not completion claims; a live solve is handed over only after an
authenticated fixed-point checkpoint makes rerunning the old serial proof
strictly wasteful.
