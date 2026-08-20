#!/usr/bin/env bash
# Stage the worker-aware resume while retaining every earlier work tree.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
base_source=/mnt/ultimatefish/angel-graph-v6-source-7fbebde7
resume_source=/mnt/ultimatefish/angel-graph-v6-resume-source-295785e7
stage_root=/mnt/ultimatefish/angel-graph-v6-resume-stage-295785e7
manifest=/mnt/ultimatefish/angel-graph-v6-resume-manifests/kberserkerangelk-v2-to-v5.json
manifest_sha=c1544365463bab93fb63388d8565f8d38f7052b8a0b654337fbadd9f9c35b243
resume_key=sources/wrappers/sha256/295785e70d9a25a3b339dcb31effea601623b72940e5c6c472f84fdb61d10ec0/resume_ultimate_concrete_frontier_aws.py
resume_version=u5PTjVt5AYAb3ARqIvdmvhKsoyZpw0IK
resume_sha=295785e70d9a25a3b339dcb31effea601623b72940e5c6c472f84fdb61d10ec0
resume_bytes=32165

test -d "$base_source"
test -f "$manifest"
test ! -e "$resume_source"
test ! -e "$stage_root"
install -d -m 0755 "$resume_source" "$stage_root"

aws s3api get-object --bucket "$bucket" --key "$resume_key" \
  --version-id "$resume_version" --region "$region" \
  "$stage_root/resume_ultimate_concrete_frontier_aws.py" >/dev/null
test "$(stat -c %s "$stage_root/resume_ultimate_concrete_frontier_aws.py")" = "$resume_bytes"
test "$(sha256sum "$stage_root/resume_ultimate_concrete_frontier_aws.py" | cut -d' ' -f1)" = "$resume_sha"

cp -a "$base_source/." "$resume_source/"
chmod u+w "$resume_source/tools/tablebases"
install -m 0444 "$stage_root/resume_ultimate_concrete_frontier_aws.py" \
  "$resume_source/tools/tablebases/resume_ultimate_concrete_frontier_aws.py"
chmod -R a-w "$resume_source"

test "$(sha256sum "$resume_source/tools/tablebases/resume_ultimate_concrete_frontier_aws.py" | cut -d' ' -f1)" = "$resume_sha"
test "$(sha256sum "$manifest" | cut -d' ' -f1)" = "$manifest_sha"
test "$(/usr/bin/python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["substates"])' "$manifest")" = 30
test "$(cd "$resume_source" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')" = \
  ac737cb47c399bd8cabbd02f191d07c86fb028eab7bb19f5068c61942c521b68

/usr/bin/python3 \
  "$resume_source/tools/tablebases/resume_ultimate_concrete_frontier_aws.py" \
  --manifest "$manifest" --workers 7

echo "ANGEL_V6_SAME_BERSERKER_RESUME_V6_STAGE_OK resume=$resume_sha manifest=$manifest_sha substates=30 workers=7 original_retained=/mnt/ultimatefish/angel-graph-v6-work/kberserkerangelk-v2"
