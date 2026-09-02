#!/usr/bin/env bash
# Resume opposed Parasite/Ghost after the i024 migration from authenticated
# compaction 6.  Bdd-a is append-only, so the partial iteration-7 suffix does
# not alter any retained compaction-6 node IDs; partial next roots are ignored.
set -euo pipefail

root=/mnt/ultimatefish/parasite-tracked-current-opposing-v1
binary=/mnt/ultimatefish/parasite-resume-v1-0316dac2-build/ultimate_ghost_parasite_information_tablebase
manifest=${root}/bundle-manifest.json
log=${root}/work/logs/.solve.log.active
scratch=${root}/work/solve/kghostkparasite
transition_prefix=work/transitions/kghostkparasite-recovered-v1

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  97467627195e71b65782bf9f26970189af71d29a387a59aa1371c4b1a270a951
test "$(sha256sum "${manifest}" | cut -d ' ' -f1)" = \
  f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7
test "$(sha256sum "${root}/${transition_prefix}.verified" | cut -d ' ' -f1)" = \
  28994cedabfeb236f11b4ea134554b07877d9b880b96381e32e1e3be0131dea6
grep -Fq 'ghost_extra_external_compaction iteration 6 roots 42648839 marked_nodes 5255806 copied_nodes 5255804 structural_residual 0 root_residual 0' \
  "${log}"
test "$(stat -c %s "${scratch}.bdd-a.nodes")" = 4500000000
test "$(stat -c %s "${scratch}.owner-current")" = 157747200
test ! -e "${root}/work/results/kghostkparasite.ufiw"
test ! -e "${root}/work/results/kghostkparasite.ufgp"

exec >>"${log}" 2>&1
echo 'parasite_opposed_resume_v3 resume_iteration=6 current_slot=current bdd_slot=a append_only_partial_iteration=7 migration_residual=0'
cd "${root}"
exec python3 -c '
import json
import os
import sys

manifest, binary, transition_prefix = sys.argv[1:]
command = json.load(open(manifest, encoding="utf-8"))["commands"]["solve"]
if command[0] != "./ultimate_ghost_parasite_information_tablebase":
    raise SystemExit("Parasite manifest solve-command residual")
command[0] = binary
index = command.index("--transition-prefix") + 1
if command[index] != "work/transitions/kghostkparasite":
    raise SystemExit("Parasite manifest transition-prefix residual")
command[index] = transition_prefix
command.extend([
    "--resume-fixed-point", "--resume-iteration", "6",
    "--resume-current-slot", "current", "--resume-bdd-slot", "a",
])
os.execv(binary, command)
' "${manifest}" "${binary}" "${transition_prefix}"
