#!/usr/bin/env bash
# Complete the two preserved v5 stages whose first wrapper lacked wave-1 Pawn.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_sha=17da6cea7c01926a20ab76046b7e5a8fad2fa80eb778bb4e19d8f38b5df1a5b6
model_sha=53d55edc1143bb30e0c8e4316c58fee47ffba93d69869c595e98cc13a952993b
inventory_sha=da4c92ae406d1fd0a4932f6b1126216242d14afadcc15d6f7b5a0db19f080261
tablebase_sha=7998cb143d183ee901ea4880bbe650d792aee5bc7b7ce571aecff9b9c3fc9161
runner_sha=b3a738478f375f268e0b1903889cc80953e6edd68cdc8a43a5b41813d580edd3
manifest_sha=990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6
pawn_sha=42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
pawn_key=staging/concrete/dependencies/sha256/42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844/kpawnk.uftb
pawn_version=LpBPCebQup_Fveb_lDZhB1JP2CipLz0J

source_root=/mnt/ultimatefish/angel-graph-v5-source-17da6cea
dependency_root=/mnt/ultimatefish/angel-graph-v5-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v5-stage-17da6cea

test -d "$source_root"
test -d "$dependency_root"
test -d "$stage_root"
test ! -e "$dependency_root/kpawnk.uftb"
test "$(sha256sum "$stage_root/source.tar" | cut -d' ' -f1)" = "$source_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = "$tablebase_sha"
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = "$runner_sha"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"

actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"

aws s3api get-object --bucket "$bucket" --key "$pawn_key" \
  --version-id "$pawn_version" --region "$region" \
  "$dependency_root/kpawnk.uftb"
test "$(stat -c %s "$dependency_root/kpawnk.uftb")" = 2464840
test "$(sha256sum "$dependency_root/kpawnk.uftb" | cut -d' ' -f1)" = "$pawn_sha"

chmod -R a-w "$source_root" "$dependency_root"
echo "ANGEL_V5_PAWN_STAGE_RESUMED source=$source_sha model=$model_sha inventory=$inventory_sha manifest=$manifest_sha pawn=$pawn_sha"
