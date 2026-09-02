#!/usr/bin/env bash
# Version-stage the exact same-side Jester/Ghost graph and dependencies for a
# deadline sprint on an otherwise idle host.  The retained source tree is
# read-only; solve arenas and partial outputs are deliberately excluded.
set -euo pipefail

readonly root=/mnt/ultimatefish/jester-ghost-current-same-work-v1
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly key=staging/certification-sprint-20260829/same-jester-ghost-lower-binding-v2.tar
readonly receipt=${root}/jester-ghost-sprint-v1.stage-receipt.json

cd "${root}"
test "$(sha256sum ultimatefish-resume-kjesterghostk-lower-binding-v2.sh | cut -d' ' -f1)" = \
  4f6d315683d940c83870ce47ca9aa8650e938a4bba19b8cd5bd3c09354941ffc
test "$(sha256sum ultimate_jester_ghost_information_tablebase | cut -d' ' -f1)" = \
  70b406b369ab54f37c892b9caec32dfcb815a842bada89277bfee145cbb7d68e
test "$(sha256sum work/transitions/kjesterghostk.verified | cut -d' ' -f1)" = \
  18a283f663cae8360a34f9935294e0c80d5643ab5e6298dca0244ee9bd6d0bed
test "$(sha256sum tablebases/kjesterk.ufiw | cut -d' ' -f1)" = \
  f1383fc68a7f774e8bf9dd65d907c84bdcdfa62e61c56f08d28fd46e6978b5fc

files=(
  ultimatefish-resume-kjesterghostk-lower-binding-v2.sh
  ultimate_jester_ghost_information_tablebase
  tablebases/kjesterghostk.uftb
  tablebases/kjesterk.uftb
  tablebases/kjesterk.ufiw
  tablebases/kghostk.ufgm
  work/logs/solve.log
  work/transitions/kjesterghostk.header
  work/transitions/kjesterghostk.index
  work/transitions/kjesterghostk.meta
  work/transitions/kjesterghostk.strata
  work/transitions/kjesterghostk.blocks
  work/transitions/kjesterghostk.verified
)
for file in "${files[@]}"; do test -s "${file}"; done

tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
  -cf - "${files[@]}" |
  aws s3 cp - "s3://${bucket}/${key}" --region us-west-2 \
    --expected-size 27868299999 --only-show-errors

aws s3api head-object --bucket "${bucket}" --key "${key}" \
  --region us-west-2 --output json >"${receipt}"
python3 - "${receipt}" "${key}" <<'PY'
import json, sys
path, key = sys.argv[1:]
value = json.load(open(path))
assert value.get("VersionId")
assert int(value.get("ContentLength", 0)) > 27_000_000_000
value["key"] = key
value["coverage_residual"] = 0
open(path, "w").write(json.dumps(value, indent=2, sort_keys=True) + "\n")
PY
cat "${receipt}"
