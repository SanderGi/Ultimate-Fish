# Native tablebase rules repair, 2026-09-12

The preceding rule commits were `a8393fd6` (flying Checker Kings) and
`204c3ee5` (native turn-start check classification). This campaign recomputed
the affected closures and root plots and restored probes for the repaired formats. The rules,
evaluation, and search heuristics are unchanged by this repair commit.

## Scope and proof

- 80 concrete files: 37 containing Checker and 45 containing Sniper, with two
  shared classes; 26,574,487,680 dense indexed states.
- Eight Jester/Ghost information overlays and four Ghost arbitrary-mask
  sidecars. Concrete hidden-piece worlds are dependencies, not public WDL
  claims. The Ghost results retain exact transition and fixed-point proofs.
- Twelve stateful Devil partitions: 34,981,631,519 causal states. Eight are
  directly hosted; four use two 32-GB-or-smaller transport parts and a manifest.
- 112 managed dataset objects are verified for the repair. Byte-identical
  transport chunks reuse existing LFS blobs after full recomputation;
  unaffected payloads are preserved.

Historical Devil files supply only their authenticated, sorted position keys.
All old WDL/DTW values are discarded. The native solver rebuilds terminal seeds,
successors, reverse edges, and retrograde outcomes. Missing successors fail
closed, and the exhaustive final verifier checks terminal winners and Bellman
WDL/DTW equations. The native root classifier includes Minion turn-start mates
instead of dropping them as trivial stalemates. Across all twelve partitions, the corrected census has 1,237,899 fewer
draws, 849,194 more wins, and 388,705 more losses (net changes). Maximum
DTW increases from 19 to 34. Every new logical Devil file
also passes a streamed S3 restore/hash check.

Concrete files are generated and exhaustively verified under the corrected
rules. Native root audits provide both aggregate and piece-substate counts.
Prince classes use ordinary turn boundaries for plotting. Ghost information
summaries explicitly transpose the logical piece-substate/visibility layout
and bind that interpretation to the exact source and model hashes. The full
Devil ledger conserves dead-Devil continuation states in its excluded counts,
while the Minion plot displays only living-Devil roots. The Berserker audit
manifest and downloader accept immutable Hugging Face payload bindings for
repaired classes alongside the historical archive bindings.

## Publication

The final [Hugging Face dataset revision](https://huggingface.co/datasets/SanderGi/Ultimate-Fish-Tablebases/tree/847eb02da6cd3a0879226bac293464c0e72763dd)
is `847eb02da6cd3a0879226bac293464c0e72763dd`. Its payload commit is `a93b3a0cb917a2d899a65353e8e1c4899fb2a90f`;
the following commit binds the catalog manifest and repair proof to those
payloads. All 112 affected paths were verified, with 110 changed
objects and two recomputed, byte-identical transport chunks. The final catalog
contains 588 managed objects. The five repaired piece slices,
Devil Minion slice, reachability summary, README ledger, and
[grid plot](../../tablebases/ultimate-tablebase-grid.svg) use the final revision.

## Runtime formats

Affected UFTB files use version 12 and a 64-byte header. The tag at offset 56
is the underlying layout tag XOR `0x3132393036524655`; ordinary dense layouts
use zero before the XOR. Existing Giant and Angel layout semantics remain
intact. A lone Sniper uses the `PieceType::Count` secondary sentinel.

Corrected sparse Devil files retain their ten-byte records and 32-byte header,
with format version 2. Version 1 files cannot override search results. The
engine likewise rejects old Checker/Sniper UFTB versions while accepting new
compatible files. A cached header inventory bypasses unavailable repaired
classes before piece scans or Minion-vector allocation. It follows the engine
reload lifecycle used by the local tablebase manager and conservatively defers
legacy split manifests to the full decoder. New downloads require an updated
engine.

## Validation

The native rules, information, and Devil sidecar tests pass. The full external
payload suite still requires tablebases not installed in this checkout. The
Android controller suite passes 191 tests (one skipped), local conformance
passes 15, and army evolution passes seven. The broader tablebase Python suite
passes 580 tests (24 skipped) against the final published metadata, including
the plot and ledger checks and authenticated Hugging Face audit downloads.

A direct repaired Devil witness is:

```
b;king,w,c5;king,b,a6;devil,w,a1;minion,w,a4;minion,w,a5;minion,w,b6
```

At Devil cooldown zero, key `0x194154a003855` changed from historical draw to
loss in eight plies. Runtime probes reproduce the repaired result in both
color orientations. Checker/Checker King and Sniper probes also pass against
fresh downloads from the final Hugging Face revision. A native-versus-specialized Devil root-classifier comparison
passes 1,591,553 states, including the reported Minion checkmate, with fixed
random seed `0x4d595df4d0f33173`.

The repository benchmark completes all 28 hidden-information fixtures. Its
classic depth-6 control visits 12,033 nodes; Ultimate depth 5 visits 17,660;
the recovered Unranked depth-7 fixture visits 258,791. This is validation,
not an Elo estimate.

A paired baseline/candidate benchmark enabled probes with an empty local
catalog and alternated AB/BA for nine measured runs per fixture and mode,
after warmup. At one million nodes and at depth eight, every node count,
score, and principal variation matched the preceding rule-fix commit. Final
median throughput ratios ranged from 0.9678 to 1.0561; the cooldown node-limit
fixture was 3.2% slower, while the other nine comparisons were within 1.4%
slower to 5.6% faster. The cached availability guard removes repeated material
scans and Minion allocations when repaired files are missing. These numbers
measure fallback overhead, not the strength gained from exact probes.
Machine-readable summaries are in
[the performance results](tablebase-rules-repair-performance-20260912.json); the complete paired log is
preserved under the campaign bucket’s `repaired/validation/` prefix.

## Infrastructure and retention

The [resource inventory](ultimate-fish-repair-resources-20260912.json) and
[destructive cleanup commands](ultimate-fish-repair-cleanup-20260912.sh) are
adjacent to this report. The cleanup commands have not been executed. All created names begin with `ultimate-fish`; the private S3 bucket,
IAM role/profile, security groups, instances, network interfaces, and retained
encrypted EBS volumes are enumerated explicitly. The existing VPCs, subnets,
AMIs, and account credentials are not campaign-created resources.

Both independently computed Ghost/Checker concrete orientations match byte
for byte. Their source/model bindings remain those of the exact generating
snapshots preserved in the resource inventory; later changes to the Sniper
lower-table reader do not relabel the earlier Checker proofs. Information
receipt `workers` values describe transition and root-audit parallelism; the
Ghost fixed-point phase uses 32 workers, recorded in its native proof logs.

The main and Ghost workers are `r7i.16xlarge` (64 vCPUs, 512 GiB RAM,
$4.2336/hour); the Devil worker is `r7i.24xlarge` (96 vCPUs, 768 GiB RAM,
$6.3504/hour). The independent opposite Sniper/Ghost and Checker/Ghost proofs each use an
`r7i.8xlarge` (32 vCPUs, 256 GiB, $2.1168/hour) and a retained 500 GiB disk.
The Checker/Ghost worker uses existing Ohio quota; all other workers are in
Oregon. The requested Oregon increase closed with the enforced limit unchanged.
Prices were checked with AWS Pricing in Oregon and Ohio. The two 32-worker Ghost campaigns share one worker after measured resource
gates showed sufficient spare capacity. Stop deadlines bound unattended compute. Completed reverse-edge scratch can be reclaimed
under disk pressure only after authenticating the retained result and restore
receipt; no AWS resource is deleted.

There was a local monitoring gap between the last recorded 09:50 UTC
snapshot and the next local observation at 18:58 UTC on September 13.
The Devil batch preserved its final log at 09:52:02 UTC and immediately
requested automatic shutdown. Other completed workers were subsequently
stopped after their remaining uploads were authenticated. The compute
estimate includes idle time; the precise Devil stop time is inferred from
its final batch log rather than an EC2 transition timestamp. The last
Sniper/Ghost worker has a completion guard that authenticates and preserves
its proof before shutdown, so its final upload can run locally.

Retained storage totals 9,192 GiB of gp3. Its base rate is $0.08/GiB-month,
about $735.36/month, plus S3 storage and any retained extra disk performance.
Stopping EC2 does not stop storage charges. The supplied cleanup commands are
for the owner to review and execute; they are not run by this campaign.

## Final resource state and cost estimate

All five EC2 instances are confirmed stopped. The last worker preserved its
complete proof at 2026-09-14 01:26:16 UTC and requested automatic shutdown;
the stopped state was confirmed at 01:29:05 UTC. No AWS resource was deleted.
All five retained volumes now use 3,000 IOPS and 125 MiB/s, the included gp3
performance tier, so no extra retained performance is requested.

Estimated on-demand compute is **$336.30**, including idle time and shutdown
margin. This is an estimate, not an invoice; storage and transfer are additional.
The campaign remained inside the $2,000 authorization, with a conservative
$1,100 reserve for up to 30 days of storage and transfer. Retention beyond that
window continues to bill until the owner executes the cleanup commands.
