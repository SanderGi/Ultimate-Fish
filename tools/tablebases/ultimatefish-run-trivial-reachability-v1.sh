#!/usr/bin/env bash
# Build the authenticated trivial-position reachability auditor from a pinned
# concrete source tree plus the content-addressed tablebase.cpp overlay, then
# audit and publish one retained concrete result without changing that result.
set -euo pipefail

test "$#" = 7
base_source=$1
work=$2
filename=$3
piece=$4
piece2=$5
orientation=$6
s3_prefix=${7%/}

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
overlay_sha=039374c2c4cdaaf150dd0705711e29d5fd2b6d8e2f5e019bd53cd1b7e2f2fc46
overlay_version=y6JOZYN0tHTZAYDC0eSHYVy9He.ZefT_
model_sha=ba1f775d14c35ea5ba18e01e5b68efb280cb4ca5116d5a0fd94f620f49d10157
inventory_sha=ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143
source_root=${work}/trivial-audit-source-${overlay_sha:0:8}
binary=${source_root}/ultimate_tablebase-trivial-v1
stem=${filename%.uftb}
sidecar=${work}/${stem}.reachability-trivial-v3.txt
verify=${work}/${stem}.reachability-trivial-v3.s3-verify.txt

test -d "$base_source"
test -f "${work}/outputs/${filename}"
test "$orientation" = same || test "$orientation" = opposing
test ! -e "$sidecar"
test ! -e "$verify"
if test ! -e "$source_root"; then
  cp -a "$base_source" "$source_root"
  chmod -R u+w "$source_root"
  aws s3api get-object --region "$region" --bucket "$bucket" \
    --key "sources/auditors/trivial-v1/sha256/${overlay_sha}/tablebase.cpp" \
    --version-id "$overlay_version" \
    "${source_root}/src/ultimate/tablebases/tablebase.cpp" >/dev/null
fi
test "$(sha256sum "${source_root}/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = \
  "$overlay_sha"
actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"

(
  cd "$source_root"
  g++ -Isrc/ultimate -Isrc/ultimate/tablebases -std=c++17 -O3 -DNDEBUG \
    -Wall -Wextra -Wpedantic \
    src/ultimate/tablebases/tablebase.cpp src/ultimate/position.cpp \
    src/ultimate/tablebases/tablebase_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/tablebases/information_solver.cpp src/ultimate/nnue.cpp \
    -o "$binary"
)
binary_sha=$(sha256sum "$binary" | cut -d' ' -f1)
table_sha=$(sha256sum "${work}/outputs/${filename}" | cut -d' ' -f1)
command=("$binary" --piece "$piece" --workers 30 --checkpoint-every 0)
if test "$piece2" != -; then
  command+=(--piece2 "$piece2")
fi
if test "$orientation" = opposing; then
  command+=(--opposing)
fi
command+=(--audit-reachability "${work}/outputs/${filename}")
{
  echo "reachability_binding filename ${filename} output_sha256 ${table_sha}"
  echo "trivial_audit_binding source_overlay_sha256 ${overlay_sha} model_sha256 ${model_sha} inventory_sha256 ${inventory_sha} binary_sha256 ${binary_sha}"
  "${command[@]}"
} >"${sidecar}.tmp"
test "$(grep -c '^reachability_trivial side ' "${sidecar}.tmp")" = 2
mv "${sidecar}.tmp" "$sidecar"

side_sha=$(sha256sum "$sidecar" | cut -d' ' -f1)
side_bytes=$(stat -c %s "$sidecar")
key="${s3_prefix#s3://${bucket}/}/reachability/sha256/${side_sha}/${stem}.reachability-trivial-v3.txt"
version=$(aws s3api put-object --region "$region" --bucket "$bucket" \
  --key "$key" --body "$sidecar" \
  --metadata "sha256=${side_sha},output-sha256=${table_sha},model-sha256=${model_sha}" \
  --query VersionId --output text)
test "$version" != None
test "$(aws s3api head-object --region "$region" --bucket "$bucket" \
  --key "$key" --version-id "$version" --query ContentLength --output text)" = \
  "$side_bytes"
test "$(aws s3api head-object --region "$region" --bucket "$bucket" \
  --key "$key" --version-id "$version" --query Metadata.sha256 --output text)" = \
  "$side_sha"
aws s3api get-object --region "$region" --bucket "$bucket" --key "$key" \
  --version-id "$version" "$verify" >/dev/null
test "$(sha256sum "$verify" | cut -d' ' -f1)" = "$side_sha"
echo "TRIVIAL_REACHABILITY_V1_OK filename=${filename} table_sha256=${table_sha} sidecar_sha256=${side_sha} sidecar_bytes=${side_bytes} version_id=${version} key=${key}"
