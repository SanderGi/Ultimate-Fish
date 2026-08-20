#!/usr/bin/env bash
# Stage a version-pinned frontier resume without modifying the failed v2 work.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2

base_source=/mnt/ultimatefish/angel-graph-v6-source-7fbebde7
resume_source=/mnt/ultimatefish/angel-graph-v6-resume-source-973c7759
stage_root=/mnt/ultimatefish/angel-graph-v6-resume-stage-973c7759
manifest_root=/mnt/ultimatefish/angel-graph-v6-resume-manifests
manifest="$manifest_root/kberserkerangelk-v2-to-v3.json"
failed_work=/mnt/ultimatefish/angel-graph-v6-work/kberserkerangelk-v2

builder_key=sources/wrappers/sha256/35ffda71c50999d429aa3b231ba9239d50cb5f9a7ececcb354a74f5959bb8364/build_ultimate_concrete_frontier_resume_manifest.py
builder_version=xUwlwj48ZTJWhhzQYtp6_UnABTOkWOTJ
builder_sha=35ffda71c50999d429aa3b231ba9239d50cb5f9a7ececcb354a74f5959bb8364
builder_bytes=6762
resume_key=sources/wrappers/sha256/973c7759314ab95fcee9acc320b449617db1b16a36584a2c6ce8cb434e0b0205/resume_ultimate_concrete_frontier_aws.py
resume_version=sMYxIQtduFmhNzXFoR_nqX9OWMY965jz
resume_sha=973c7759314ab95fcee9acc320b449617db1b16a36584a2c6ce8cb434e0b0205
resume_bytes=31967

test -d "$base_source"
test -d "$failed_work"
test ! -e "$resume_source"
test ! -e "$stage_root"
test ! -e "$manifest"
install -d -m 0755 "$resume_source" "$stage_root" "$manifest_root"

aws s3api get-object --bucket "$bucket" --key "$builder_key" \
  --version-id "$builder_version" --region "$region" \
  "$stage_root/build_ultimate_concrete_frontier_resume_manifest.py" >/dev/null
aws s3api get-object --bucket "$bucket" --key "$resume_key" \
  --version-id "$resume_version" --region "$region" \
  "$stage_root/resume_ultimate_concrete_frontier_aws.py" >/dev/null
test "$(stat -c %s "$stage_root/build_ultimate_concrete_frontier_resume_manifest.py")" = "$builder_bytes"
test "$(sha256sum "$stage_root/build_ultimate_concrete_frontier_resume_manifest.py" | cut -d' ' -f1)" = "$builder_sha"
test "$(stat -c %s "$stage_root/resume_ultimate_concrete_frontier_aws.py")" = "$resume_bytes"
test "$(sha256sum "$stage_root/resume_ultimate_concrete_frontier_aws.py" | cut -d' ' -f1)" = "$resume_sha"

cp -a "$base_source/." "$resume_source/"
chmod u+w "$resume_source/tools/tablebases"
install -m 0444 "$stage_root/build_ultimate_concrete_frontier_resume_manifest.py" \
  "$resume_source/tools/tablebases/build_ultimate_concrete_frontier_resume_manifest.py"
install -m 0444 "$stage_root/resume_ultimate_concrete_frontier_aws.py" \
  "$resume_source/tools/tablebases/resume_ultimate_concrete_frontier_aws.py"
chmod -R a-w "$resume_source"

test "$(sha256sum "$resume_source/tools/tablebases/build_ultimate_concrete_frontier_resume_manifest.py" | cut -d' ' -f1)" = "$builder_sha"
test "$(sha256sum "$resume_source/tools/tablebases/resume_ultimate_concrete_frontier_aws.py" | cut -d' ' -f1)" = "$resume_sha"
test "$(cd "$resume_source" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')" = \
  ac737cb47c399bd8cabbd02f191d07c86fb028eab7bb19f5068c61942c521b68
test "$(cd "$resume_source" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')" = \
  da4c92ae406d1fd0a4932f6b1126216242d14afadcc15d6f7b5a0db19f080261

/usr/bin/python3 \
  "$resume_source/tools/tablebases/build_ultimate_concrete_frontier_resume_manifest.py" \
  --source "$failed_work" --wave 0 --index 228 --output "$manifest"
/usr/bin/python3 \
  "$resume_source/tools/tablebases/resume_ultimate_concrete_frontier_aws.py" \
  --manifest "$manifest"
chmod a-w "$manifest"

echo "ANGEL_V6_SAME_BERSERKER_RESUME_V3_STAGE_OK builder=$builder_sha resume=$resume_sha manifest=$(sha256sum "$manifest" | cut -d' ' -f1) original_retained=$failed_work resume_source=$resume_source"
