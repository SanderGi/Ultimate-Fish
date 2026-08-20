#!/usr/bin/env bash
# Stage the exact one-Angel v9 source and lower-material dependencies on EC2.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_key=sources/bundles/angel-graph-v1/sha256/9244b1177b9a1e3190596d91d8a338b06e655c87cd7f7229964f5720532a53f5/ultimatefish-angel-graph-v1-source.tar
source_version=CY2umuLKMyLZk1m_ihJrcL17dwl7R8Pw
source_sha=9244b1177b9a1e3190596d91d8a338b06e655c87cd7f7229964f5720532a53f5
model_sha=68b7198f71f9cc671a3a4c72fb849f44d8d4ccaddbe348c58ed9838238ca9d59
inventory_sha=da4c92ae406d1fd0a4932f6b1126216242d14afadcc15d6f7b5a0db19f080261

dependency_key=staging/concrete-wave0-batch/dependencies/sha256/58adaf86c71dfc91d60ebf556f38a193b877e962b016c6be53a10b15a713804d/dependencies-full-990832f4.tar.zst
dependency_version=eRzyc1ioApWyZaPVo_DnH1R20TaY80Vk
dependency_sha=58adaf86c71dfc91d60ebf556f38a193b877e962b016c6be53a10b15a713804d
manifest_sha=990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6

source_root=/mnt/ultimatefish/angel-graph-v1-source-9244b117
dependency_root=/mnt/ultimatefish/angel-graph-v1-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v1-stage-9244b117

test ! -e "$source_root"
test ! -e "$dependency_root"
test ! -e "$stage_root"
install -d -m 0755 "$source_root" "$dependency_root" "$stage_root"

aws s3api get-object --bucket "$bucket" --key "$source_key" \
  --version-id "$source_version" --region "$region" \
  "$stage_root/source.tar"
test "$(sha256sum "$stage_root/source.tar" | cut -d' ' -f1)" = "$source_sha"
tar -xf "$stage_root/source.tar" -C "$source_root"

actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"

aws s3api get-object --bucket "$bucket" --key "$dependency_key" \
  --version-id "$dependency_version" --region "$region" \
  "$stage_root/dependencies.tar.zst"
test "$(sha256sum "$stage_root/dependencies.tar.zst" | cut -d' ' -f1)" = "$dependency_sha"
tar --zstd -xf "$stage_root/dependencies.tar.zst" -C "$dependency_root"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"
test "$(stat -c %s "$dependency_root/krk.uftb")" = 1232440
test "$(sha256sum "$dependency_root/krk.uftb" | cut -d' ' -f1)" = \
  abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44

chmod -R a-w "$source_root" "$dependency_root"
echo "ANGEL_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha manifest=$manifest_sha"
