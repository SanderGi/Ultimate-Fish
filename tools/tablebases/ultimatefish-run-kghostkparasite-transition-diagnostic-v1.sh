#!/usr/bin/env bash
# Reproduce the opposed Parasite/Ghost solve-time transition failure against a
# reflinked transition bundle and an empty diagnostic scratch path. The retained
# fixed-point checkpoint and both recovered transition prefixes are read-only.
set -euo pipefail

root=/mnt/ultimatefish/parasite-tracked-current-opposing-v1
build=/mnt/ultimatefish/parasite-transition-diagnostic-v1-65a0dde0
binary=${build}/ultimate_ghost_parasite_transition_diagnostic_v1
source_prefix=${root}/work/transitions/kghostkparasite-recovered-v1
diagnostic_prefix=${root}/work/transitions/kghostkparasite-diagnostic-v1
scratch=work/diagnostic/kghostkparasite
log=${root}/work/logs/transition-residual-diagnostic-v1.log

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  a1d2b6aedf9911d109b74ab9a99b067a4957d83a5d7c07d7ba320864da3aae7c
test "$(sha256sum "${root}/bundle-manifest.json" | cut -d ' ' -f1)" = \
  f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7
test ! -e "${diagnostic_prefix}.header"
test ! -e "${root}/work/diagnostic"
install -d -m 0755 "${root}/work/diagnostic"
for suffix in header meta strata index blocks verified; do
  cp --reflink=always "${source_prefix}.${suffix}" \
    "${diagnostic_prefix}.${suffix}"
done

cd "${root}"
python3 - "${root}/bundle-manifest.json" "${binary}" \
  "work/transitions/kghostkparasite-diagnostic-v1" "${scratch}" \
  "${log}" <<'PY'
import json
import pathlib
import subprocess
import sys

manifest, binary, transition_prefix, scratch, log_path = sys.argv[1:]
command = json.load(open(manifest, encoding="utf-8"))["commands"]["solve"]
if command[0] != "./ultimate_ghost_parasite_information_tablebase":
    raise SystemExit("Parasite diagnostic solve-command residual")
command[0] = binary
command[command.index("--transition-prefix") + 1] = transition_prefix
command[command.index("--scratch") + 1] = scratch
command[command.index("--output") + 1] = "work/diagnostic/result.ufiw"
command[command.index("--output-arbitrary") + 1] = \
    "work/diagnostic/result.ufgp"
command.extend([
    "--resume-fixed-point", "--resume-iteration", "6",
    "--resume-current-slot", "current", "--resume-bdd-slot", "a",
])
with open(log_path, "w", encoding="utf-8") as log:
    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                            check=False)
text = pathlib.Path(log_path).read_text(encoding="utf-8")
needle = "Parasite lower-table transition residual geometry="
if result.returncode != 1 or needle not in text:
    raise SystemExit("Parasite diagnostic did not reproduce the detailed residual")
print(text.rstrip().splitlines()[-1])
PY
