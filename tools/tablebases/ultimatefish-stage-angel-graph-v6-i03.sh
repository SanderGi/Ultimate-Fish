#!/usr/bin/env bash
# Stage the dynamically load-balanced Angel verifier beside immutable v5.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_key=sources/bundles/angel-graph-v6/sha256/7fbebde72e91fa82396826417582200bba2bb970a2941ca7a10032fe265c1b66/ultimatefish-angel-graph-v6-source.tar
source_version=4Vn2x3lCZzTB4EjhKxI7uNGk6ZV6LwTs
source_sha=7fbebde72e91fa82396826417582200bba2bb970a2941ca7a10032fe265c1b66
model_sha=ac737cb47c399bd8cabbd02f191d07c86fb028eab7bb19f5068c61942c521b68
inventory_sha=da4c92ae406d1fd0a4932f6b1126216242d14afadcc15d6f7b5a0db19f080261
tablebase_sha=87b0f090ecab3918c3bcf1232051de72ea823e4549b5e9be3b1184c0464aafdd
runner_sha=b3a738478f375f268e0b1903889cc80953e6edd68cdc8a43a5b41813d580edd3

source_root=/mnt/ultimatefish/angel-graph-v6-source-7fbebde7
stage_root=/mnt/ultimatefish/angel-graph-v6-stage-7fbebde7

test ! -e "$source_root"
test ! -e "$stage_root"
install -d -m 0755 "$source_root" "$stage_root"

aws s3api get-object --bucket "$bucket" --key "$source_key" \
  --version-id "$source_version" --region "$region" \
  "$stage_root/source.tar"
test "$(stat -c %s "$stage_root/source.tar")" = 767488
test "$(sha256sum "$stage_root/source.tar" | cut -d' ' -f1)" = "$source_sha"
tar -xf "$stage_root/source.tar" -C "$source_root"

actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = "$tablebase_sha"
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = "$runner_sha"
chmod -R a-w "$source_root"

echo "ANGEL_V6_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha tablebase=$tablebase_sha runner=$runner_sha"
