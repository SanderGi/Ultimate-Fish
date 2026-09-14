# Information overlay orientation correction, 2026-09-14

The opposed Ghost/Checker and Ghost/Sniper reporting audits in commit
`3d7a3fb8` reversed wins and losses. The solver outputs and published payloads
were correct. Four Ghost overlays were downloaded, hash checked, and audited
locally with the corrected reader in approximately 351 seconds. No solve was
repeated, no EC2 instance was restarted, and no AWS resource was created.

## Cause and format contract

The earlier solver correction in `83ef28e2` remained intact. The repair added
a finalization path that checked hashes and conservation but omitted exact
solver-versus-audit outcome equality. Those checks cannot detect a W/L swap.
Changing the Ghost/Checker plot test to accept `no_forced_win` in `3d7a3fb8`
was a mistake: it accepted the reporting regression instead of investigating it.

UFIW2 has an eight-byte magic followed by six 32-bit words: version at byte 8,
primary piece at 12, secondary piece at 16, **secondary piece color at 20**,
state count at 24, and substate count at 28. The primary piece is normalized
White. Bytes 32–95 and 96–159 contain source and model SHA-256 strings.

For single-Ghost overlays, force bits 0 and 1 mean Ghost owner and observer.
Ghost in the primary slot is therefore White; Ghost in the secondary slot
has the secondary color. For Jester overlays, force bits 0 and 1 mean
normalized White and Black. Byte 20 is not a general hidden-piece owner field.
Crossed Jester/Ghost and double-Ghost overlays require dedicated readers;
the generic audit now rejects them. Arbitrary-mask runtime sidecars use their
own format contracts and are unaffected.

The shared `information_overlay_format.h` defines these roles and the header
serializer used by the production Ghost writer and regression fixtures. The
native audit checks material and color binding, then derives the force role
from both piece slots. Existing UFIW2 files require no migration. The old
Python Berserker-radius reader now explicitly rejects Ghost overlays instead
of applying its White/Black interpretation to owner/observer bits.

## Independently checked outcomes

These counts are admitted states **before trivial-position filtering**, in
encoded side-to-move order. Each row matches the authenticated original
solver log exactly, including draws and both turns.

| Overlay | Side | Wins | Losses | Draws |
| --- | ---: | ---: | ---: | ---: |
| Ghost+Checker vs King | 0 | 63,354,136 | 0 | 0 |
| Ghost+Checker vs King | 1 | 0 | 63,835,885 | 2,674,251 |
| Ghost vs Checker | 0 | 30,908,984 | 0 | 35,601,152 |
| Ghost vs Checker | 1 | 0 | 17,925,058 | 45,394,581 |
| Ghost+Sniper vs King | 0 | 131,203,740 | 0 | 0 |
| Ghost+Sniper vs King | 1 | 0 | 127,528,894 | 5,491,378 |
| Ghost vs Sniper | 0 | 132,891,644 | 12,982 | 115,646 |
| Ghost vs Sniper | 1 | 48,305 | 124,139,016 | 7,017,861 |

The historical Ghost-to-move Sniper counts recorded at `204c3ee5` were
132,891,644 wins, 6,906 losses, and 121,722 draws. Thus wins are unchanged;
6,076 draws become losses under the corrected rules. The dramatic reversal
in the first repair plot was a reporting error.

After filtering, Ghost/Checker has W/L/D 17,911,313 / 0 / 35,370,171 when
Ghost starts, and 17,897,812 / 0 / 39,193,768 when Checker starts, expressed
from Ghost's perspective. Its classification is `no_forced_loss`. The
flying-Checker correction introduces real draws, so restoring the historical
`win` expectation would also be incorrect. Tests pin both counts and the
mirrored Checker/Ghost cell.

## Gates and regression coverage

`validate_ultimate_information_outcomes.py` requires exactly one certified
solver summary and native admitted audit row per side, with exact W/L/D
equality before trivial filtering. It never tries a W/L permutation to pass.
The gate runs in the information computation runner before a completion
receipt, in plot finalization before creating output, and in the checked-in
metadata publisher before any remote call. Finalization and publication
reopen the hash-bound original solver and audit logs; a self-reported zero
residual in a certificate is insufficient.

The actual defective campaign logs fail this gate at Ghost/Checker side 0:
solver wins 30,908,984 versus audit wins 0. All eight repaired information
overlays pass with the corrected audits.

`test_information_overlay_orientation.py` writes asymmetric known W/L/D
fixtures with the production serializer, reads them through the actual native
audit, applies real filtering and ledger rendering, then reads that ledger
through the plot catalog. It covers both turns, same-side and opposed
material, Ghost in either piece slot, substate transposition, and color/plot
ownership normalization. The negative test swaps W/L while retaining valid
hashes and totals and requires both finalization and publication to fail
before output or remote calls. The legacy Python reader also has an explicit
Ghost rejection regression test.

Publication uses `publish_ultimate_rules_repair_metadata.py`: it authenticates
the 80 concrete and eight information receipts, checks live payload bindings,
uses an expected-parent commit, and verifies every one of the 588 managed
payloads is byte-identical before and after the metadata-only commit. Original
solver source/model bindings are retained; old artifacts are not relabeled as
having been solved with the new reporting source.

## Completed validation and publication

The full tablebase Python suite passes **583 tests (24 skipped)**. The native
information solver/probe tests and production Ghost writer self-test pass.
The corrected SVG and PNG render successfully and were visually checked.

The [corrected Hugging Face metadata revision](https://huggingface.co/datasets/SanderGi/Ultimate-Fish-Tablebases/tree/abadaed7b98be098e8a2ccb3427c9be477df1679)
is `abadaed7b98be098e8a2ccb3427c9be477df1679`. The publisher verified all **588 payload files unchanged** after
the commit. The certificate and generated summary bindings intentionally retain
payload revision `847eb02da6cd3a0879226bac293464c0e72763dd`.
Only the dataset README and repair certificate were published; the corrected
ledger and plot slices are maintained in this repository.

AWS CLI checks confirmed all five retained EC2 instances are still stopped.
No resources were deleted. The existing
[resource inventory](ultimate-fish-repair-resources-20260912.json) and
[deletion commands for owner review](ultimate-fish-repair-cleanup-20260912.sh)
remain applicable. The authenticated receipts, logs, corrected metadata, and
validation record are retained in the existing campaign bucket under
`repaired/validation/orientation-20260914/`.
