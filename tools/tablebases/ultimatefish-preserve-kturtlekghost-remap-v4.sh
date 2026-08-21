#!/usr/bin/env bash
# Preserve the converged remap-corrected opposed Turtle/Ghost information solve.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kturtlekghost-fresh-v1
tool_root=/mnt/ultimatefish/recovery-tools/turtle-ghost-preservation-v4
runner_sha=bb756bf90ed54db11438b1f4186fca90967d5501ae6aafb2ad5402427c67bd1e
runner_version=D8_ebSazDS2hXV0VfycgLVGLcSMtYNKH
archive_sha=7ce682e83eb234fe5d5391a8f655b4995df7ed13a055f9dda6f040a914a66fec
archive_version=scFAl7pcBwrQzNAVelYEbBYKL7e6e3ZI
runner=$tool_root/run_ultimate_ghost_ordinary_aws.py
archive=$tool_root/archive_ultimate_aws_result.py
output=$root/work/preservation-v4
receipt=$output/preservation-receipt.json
raw_receipt=$root/work/preservation-v4-receipt.tmp

test ! -e "$receipt"
test "$(sha256sum "$root/tablebases/kturtlekghost.uftb" | cut -d' ' -f1)" = \
  40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39
test "$(sha256sum "$root/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "$root/work/results/kturtlekghost-compositional-v4.ufiw" | cut -d' ' -f1)" = \
  fb721ea6584db7e30a401b17ad327708228a46ce74eea74e50a1322466751a72
test "$(sha256sum "$root/work/results/kturtlekghost-compositional-v4.ufgd" | cut -d' ' -f1)" = \
  e936d847d9995cea76569d22ef787e9a973634825fc4f8b3c9359c8e07c5b888
grep -Fq \
  'information_symbolic_certificate iterations 43 bdd_nodes 111531167 bellman_residual 0 monotonicity_residual 0 singleton_residual 0 compaction_root_residual 0 belief_cap none powerset_exact 1' \
  "$root/work/logs/resume-after-stop-v1.log"
grep -Fq \
  'dragon_ghost_certificate dual_force_residual 0 structural_residual 0 singleton_residual 0 source_remap_residual 0 normalized_source_sha256 85b8d05decef63632f5fff6127288ff2b2a306d07afe5f3b144bed236fb5f2a0 transition_payload_sha256 c5a8c858819af6cb5305f83cb78445905745655ec4fe7d1b8c505666a6ad5388 arbitrary_sha256 e936d847d9995cea76569d22ef787e9a973634825fc4f8b3c9359c8e07c5b888' \
  "$root/work/logs/resume-after-stop-v1.log"

mkdir -p "$tool_root"
aws s3api get-object --region "$region" --bucket "$bucket" \
  --key "sources/tools/sha256/$runner_sha/run_ultimate_ghost_ordinary_aws.py" \
  --version-id "$runner_version" "$runner" >/dev/null
aws s3api get-object --region "$region" --bucket "$bucket" \
  --key "sources/tools/sha256/$archive_sha/archive_ultimate_aws_result.py" \
  --version-id "$archive_version" "$archive" >/dev/null
test "$(sha256sum "$runner" | cut -d' ' -f1)" = "$runner_sha"
test "$(sha256sum "$archive" | cut -d' ' -f1)" = "$archive_sha"

if test ! -s "$raw_receipt"; then
  test ! -e "$output"
  python3 "$runner" --finalize-existing \
    --source-root "$root" --work "$root" \
    --filename kturtlekghost.uftb --piece turtle --orientation opposing \
    --source-table "$root/tablebases/kturtlekghost.uftb" \
    --source-sha256 40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39 \
    --lower-sha256 ec27a3cb8babe7c87b0c13229ff51d454da3bed542aa8c27862bfcc630ef26ce \
    --lower-model-sha256 0bfcb5141c1732f3eef3830b1d7552540f9511aa047ec40169f369d24f5f37ee \
    --lower-ghost-sidecar "$root/tablebases/kghostk.ufgm" \
    --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --model-sha256 b4f71efb40b7d7ed9c97f346b9074d9e934f3c542c5d19827dae9ccebdafbe7d \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23

  python3 "$archive" --root "$root" \
    --artifact-manifest "$root/work/artifact-manifest.json" \
    --kind turtle-ghost-opposed-remap-v4 --output-dir "$output" \
    --s3-prefix \
    "s3://$bucket/results/current-information/turtle-ghost-opposed-remap-v4" \
    >"$raw_receipt"
fi

# `aws s3api get-object` writes its own JSON documents to stdout.  Retain that
# raw transcript, but authenticate the archive tool's final single-line JSON.
tail -n 1 "$raw_receipt" >"$output/preservation-receipt.clean.tmp"
python3 - "$output/preservation-receipt.clean.tmp" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
document = json.loads(path.read_text())
certificate = document["certificate"]
assert certificate["schema"] == "ultimate-aws-result-archive-v1"
assert certificate["local_archive_restore_residual"] == 0
remote = certificate["s3"]
assert remote["head_residual"] == 0
assert remote["download_residual"] == 0
assert remote["archive_restore_residual"] == 0
certificate_object = document["certificate_object"]
assert certificate_object["head_residual"] == 0
assert certificate_object["download_residual"] == 0
PY

mv "$output/preservation-receipt.clean.tmp" "$receipt"
mv "$raw_receipt" "$output/archive-command-output.log"
sha256sum "$root/work/artifact-manifest.json" "$receipt"
