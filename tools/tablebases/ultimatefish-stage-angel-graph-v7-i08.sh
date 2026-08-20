#!/usr/bin/env bash
# Stage the exact opposed Copycat/Angel source and its authenticated lower table.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_key=sources/bundles/angel-graph-v7/sha256/7da3ff80da46940d4fc7e2a3a2c0fd49b9968fa12e1a3bc673540a71c00b0e01/ultimatefish-angel-graph-v7-source.tar
source_version=XOJjiA7mcqd_jTn98ZgKaVyEV7_k6ObV
source_sha=7da3ff80da46940d4fc7e2a3a2c0fd49b9968fa12e1a3bc673540a71c00b0e01
source_bytes=753664
model_sha=edb3c62023be556e8b87167419db14dcfee639b63ee3fd7c25b5d36162bf9e7a
inventory_sha=46dbf01ff6b8e80a7bc0b7ecce66340944fdec4aae197e46d81e95d1d5fa289d
tablebase_sha=1230668ce7f70678e72db403a79324d1b9d4ec93837026b30a1d9f69c34eac71
probe_sha=408296df35174f5fd892dcc0671522a0059a97702fa146b3ee5732ef36cab39c
runner_sha=62b58ecf824a176fe7e767ac1a028927ba367c052b527b3501bff77e231cb3ce

dependency_key=staging/concrete-wave0-batch/dependencies/sha256/58adaf86c71dfc91d60ebf556f38a193b877e962b016c6be53a10b15a713804d/dependencies-full-990832f4.tar.zst
dependency_version=eRzyc1ioApWyZaPVo_DnH1R20TaY80Vk
dependency_sha=58adaf86c71dfc91d60ebf556f38a193b877e962b016c6be53a10b15a713804d
manifest_sha=990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6
copycat_sha=98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb
copycat_bytes=1232448

source_root=/mnt/ultimatefish/angel-graph-v7-source-7da3ff80
dependency_root=/mnt/ultimatefish/angel-graph-v7-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v7-stage-7da3ff80

test ! -e "$source_root"
test ! -e "$dependency_root"
test ! -e "$stage_root"
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

aws s3api get-object --bucket "$bucket" --key "$dependency_key" \
  --version-id "$dependency_version" --region "$region" \
  "$stage_root/dependencies.tar.zst" >/dev/null
test "$(sha256sum "$stage_root/dependencies.tar.zst" | cut -d' ' -f1)" = "$dependency_sha"
tar --zstd -xf "$stage_root/dependencies.tar.zst" -C "$dependency_root"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"
test "$(stat -c %s "$dependency_root/kcopycatk.uftb")" = "$copycat_bytes"
test "$(sha256sum "$dependency_root/kcopycatk.uftb" | cut -d' ' -f1)" = "$copycat_sha"

chmod -R a-w "$source_root" "$dependency_root"
echo "ANGEL_V7_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha manifest=$manifest_sha copycat=$copycat_sha"
