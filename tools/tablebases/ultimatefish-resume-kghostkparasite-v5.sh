#!/usr/bin/env bash
# Resume opposed Parasite/Ghost from authenticated compaction 6 with the
# clean byte-identical transition remerge and exact existing-ROBDD reopening.
set -euo pipefail

root=/mnt/ultimatefish/parasite-tracked-current-opposing-v1
binary=/mnt/ultimatefish/parasite-resume-v2-7136049d-build/ultimate_ghost_parasite_information_tablebase
manifest=${root}/bundle-manifest.json
log=${root}/work/logs/.solve.log.active
scratch=${root}/work/solve/kghostkparasite
transition_prefix=work/transitions/kghostkparasite-recovered-v2

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  79f3e9ec4f08e5cfc7df4012e87cb8ad8bb55d8b8a7a08ecf53c61193e948548
test "$(sha256sum "${manifest}" | cut -d ' ' -f1)" = \
  f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7
test "$(sha256sum "${root}/${transition_prefix}.verified" | cut -d ' ' -f1)" = \
  9860b273de8f6f0f864b12a4a27adbc59334e649d3f0d7c9508f04843211fef4
grep -Fq 'ghost_extra_external_compaction iteration 6 roots 42648839 marked_nodes 5255806 copied_nodes 5255804 structural_residual 0 root_residual 0' \
  "${log}"
test "$(stat -c %s "${scratch}.bdd-a.nodes")" = 4500000000
test "$(stat -c %s "${scratch}.owner-current")" = 157747200
test ! -e "${root}/work/results/kghostkparasite.ufiw"
test ! -e "${root}/work/results/kghostkparasite.ufgp"

exec >>"${log}" 2>&1
echo 'parasite_opposed_resume_v5 resume_iteration=6 current_slot=current bdd_slot=a transition_remerge_v2_residual=0 existing_robdd_open_v1=1'
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
