#!/usr/bin/env bash
# Resume same Pawn/Ghost from iteration 42 with exact ROBDD reopen plus the
# unreachable adjacent-King padding correction.
set -euo pipefail

base=/mnt/ultimatefish/ultimatefish-resume-kpawnghostk-v6.sh
delegate=/run/ultimatefish-resume-kpawnghostk-v8-delegate.sh
test "$(sha256sum "${base}" | cut -d' ' -f1)" = \
  f461299e8b62ca1a76f8d296346d1a9c403335c670034b8e06c7399eb5f5d61a
test "$(sha256sum /mnt/ultimatefish/pawn-resume-v8-12475566/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v8 | cut -d' ' -f1)" = \
  f98c9fa41ae87647133e6a94d5deaf7f46bae038a0fa35dc60071856589daa46

python3 - "${base}" "${delegate}" <<'PY'
import pathlib
import sys

source, output = map(pathlib.Path, sys.argv[1:])
text = source.read_text(encoding="utf-8")
text = text.replace(
    "pawn-resume-v6-7136049d/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v6",
    "pawn-resume-v8-12475566/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v8",
)
text = text.replace(
    "2a25f6a0ff36b8882f53e9917a6dab8a45b3c73377a112cf6687bb2292039498",
    "f98c9fa41ae87647133e6a94d5deaf7f46bae038a0fa35dc60071856589daa46",
)
text = text.replace(
    "pawn_ghost_same_resume_v6",
    "pawn_ghost_same_resume_v8 adjacent_padding_patch_sha256 "
    "124755668de5fbb1e67efe648a7014100592aa3ee75dc1cb357baa5766f1be4a",
)
slash_line = " " + chr(92) + "\n"
needle = "  --input tablebases/kpawnghostk.uftb" + slash_line
replacement = needle + (
    "  --normalized-input work/solve/kpawnghostk.normalized.uftb" + slash_line
    + "  --normalized-sha256 "
    "74e4841be19e8eb72c847a49740e7398a6f28479efc09b3a36a3bb97e6407a2a"
    + slash_line
)
if text.count(needle) != 1:
    raise SystemExit("Pawn/Ghost v8 normalized-input insertion residual")
output.write_text(text.replace(needle, replacement), encoding="utf-8")
PY
chmod 0700 "${delegate}"
exec "${delegate}"
