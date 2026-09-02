#!/usr/bin/env bash
# Independently rebuild the opposed Parasite/Ghost transition database from
# the 64 authenticated retained shards. This leaves both the compaction-6
# tablebase checkpoint and the first recovered transition bundle untouched so
# that the solve-time residual can be diagnosed by byte-for-byte comparison.
set -euo pipefail

root=/mnt/ultimatefish/parasite-tracked-current-opposing-v1
binary=/mnt/ultimatefish/parasite-resume-v1-0316dac2-build/ultimate_ghost_parasite_information_tablebase
manifest=${root}/bundle-manifest.json
prefix=work/transitions/kghostkparasite-recovered-v2
log=${root}/work/logs/remerge-recovered-v2.log

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  97467627195e71b65782bf9f26970189af71d29a387a59aa1371c4b1a270a951
test "$(sha256sum "${manifest}" | cut -d ' ' -f1)" = \
  f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7
test "$(find "${root}/work/transitions" -maxdepth 1 -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${root}/${prefix}.header"

exec >"${log}" 2>&1
cd "${root}"
python3 -c '
import json
import subprocess
import sys

manifest, binary, prefix = sys.argv[1:]
command = json.load(open(manifest, encoding="utf-8"))["commands"]["merge"]
if command[0] != "./ultimate_ghost_parasite_information_tablebase":
    raise SystemExit("Parasite manifest merge-command residual")
command[0] = binary
index = command.index("--transition-prefix") + 1
if command[index] != "work/transitions/kghostkparasite":
    raise SystemExit("Parasite manifest transition-prefix residual")
command[index] = prefix
subprocess.run(command, check=True)
' "${manifest}" "${binary}" "${prefix}"
sha256sum "${prefix}.header" "${prefix}.meta" "${prefix}.strata" \
  "${prefix}.index" "${prefix}.blocks" "${prefix}.verified"
cmp "${prefix}.header" work/transitions/kghostkparasite-recovered-v1.header
cmp "${prefix}.meta" work/transitions/kghostkparasite-recovered-v1.meta
cmp "${prefix}.strata" work/transitions/kghostkparasite-recovered-v1.strata
cmp "${prefix}.index" work/transitions/kghostkparasite-recovered-v1.index
cmp "${prefix}.blocks" work/transitions/kghostkparasite-recovered-v1.blocks
cmp "${prefix}.verified" work/transitions/kghostkparasite-recovered-v1.verified
echo 'parasite_transition_remerge_v2 complete 1 byte_identity_residual 0'
