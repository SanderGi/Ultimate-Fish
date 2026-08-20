#!/usr/bin/env bash
# Stage and self-test the exact Jester/Angel v1 generator on i03.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_key=sources/bundles/jester-angel-v1/sha256/3492273141788fabb09b8d3bcb241592c97c0958eef1e5dfe3d85ba45f181e6b/ultimatefish-jester-angel-v1-source.tar
source_version=cJs0mFOg2fJYlg4LnZxwgRkYWFpxDHnZ
source_sha=3492273141788fabb09b8d3bcb241592c97c0958eef1e5dfe3d85ba45f181e6b
source_bytes=757760
model_sha=da174e9dd1ab0dbbcccf827eb8c440d84dabab1886fbca7428eeaad0882d6622
inventory_sha=ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143
tablebase_sha=3bcdc19ec3c1da4f0d37f0027b9c5d52362a57ac6d335a46ce1cd09b54ee1c5c
probe_sha=e17573a1b0aa01bbb232a2144903b300c8945c8efb64d862da919dd14dc5449c
runner_sha=5f675fcaa9934a13abbba0adac3c4f3223b762165a2fba37b2bca03e79c16a5a

manifest_key=staging/jester-angel-v1/dependencies/sha256/900622dae473a0f06d77b87d832e6cc735e05a270dbd6597fb0045e3ac44c28f/manifest.json
manifest_version=8mzvgsVtEPm0zuwQMpdoabMl4pYQTEei
manifest_sha=900622dae473a0f06d77b87d832e6cc735e05a270dbd6597fb0045e3ac44c28f
manifest_bytes=227
jester_source=/mnt/ultimatefish/concrete-wave0-batch/dependencies-v3/batch-v3-07-class063-kcopycatjesterk/kjesterk.uftb
jester_sha=3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa
jester_bytes=1232440

source_root=/mnt/ultimatefish/jester-angel-v1-source-34922731
dependency_root=/mnt/ultimatefish/jester-angel-v1-dependencies-v2-900622da
stage_root=/mnt/ultimatefish/jester-angel-v1-stage-34922731

test ! -e "$source_root"
test ! -e "$dependency_root"
test ! -e "$stage_root"
test -f "$jester_source"
install -d -m 0755 "$source_root" "$dependency_root" "$stage_root"

aws s3api get-object --bucket "$bucket" --key "$source_key" \
  --version-id "$source_version" --region "$region" \
  "$stage_root/source.tar" >/dev/null
test "$(stat -c %s "$stage_root/source.tar")" = "$source_bytes"
test "$(sha256sum "$stage_root/source.tar" | cut -d' ' -f1)" = "$source_sha"
tar -xf "$stage_root/source.tar" -C "$source_root"

actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = "$tablebase_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase_probe.cpp" | cut -d' ' -f1)" = "$probe_sha"
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = "$runner_sha"

aws s3api get-object --bucket "$bucket" --key "$manifest_key" \
  --version-id "$manifest_version" --region "$region" \
  "$stage_root/manifest.json" >/dev/null
test "$(stat -c %s "$stage_root/manifest.json")" = "$manifest_bytes"
test "$(sha256sum "$stage_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"
install -m 0444 "$jester_source" "$dependency_root/kjesterk.uftb"
install -m 0444 "$stage_root/manifest.json" "$dependency_root/manifest.json"
test "$(stat -c %s "$dependency_root/kjesterk.uftb")" = "$jester_bytes"
test "$(sha256sum "$dependency_root/kjesterk.uftb" | cut -d' ' -f1)" = "$jester_sha"

install -d -m 0755 "$stage_root/binary"
(
  cd "$source_root"
  g++ -Isrc/ultimate -Isrc/ultimate/tablebases -std=c++17 -O3 -DNDEBUG \
    -Wall -Wextra -Wpedantic \
    src/ultimate/tablebases/tablebase.cpp src/ultimate/position.cpp \
    src/ultimate/tablebases/tablebase_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/tablebases/information_solver.cpp src/ultimate/nnue.cpp \
    -o "$stage_root/binary/ultimate_tablebase"
)
"$stage_root/binary/ultimate_tablebase" --piece jester --piece2 angel \
  --self-test --dry-run 100 >"$stage_root/self-test-same.log"
"$stage_root/binary/ultimate_tablebase" --piece jester --piece2 angel \
  --opposing --self-test --dry-run 100 >"$stage_root/self-test-opposed.log"
grep -q '^jesterroyalswapcodecok samples 20000$' "$stage_root/self-test-same.log"
grep -q '^jesterroyalswapcodecok samples 20000$' "$stage_root/self-test-opposed.log"
grep -q '^dryrun states 0\.\.100 ' "$stage_root/self-test-same.log"
grep -q '^dryrun states 0\.\.100 ' "$stage_root/self-test-opposed.log"
binary_sha=$(sha256sum "$stage_root/binary/ultimate_tablebase" | cut -d' ' -f1)

chmod -R a-w "$source_root" "$dependency_root" "$stage_root"
echo "JESTER_ANGEL_V1_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha manifest=$manifest_sha jester=$jester_sha binary=$binary_sha"
