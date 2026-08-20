#!/usr/bin/env bash
# Add the certified Queen/Angel promotion lower to preserved Pawn v5 stages.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_root=/mnt/ultimatefish/angel-graph-v5-source-17da6cea
dependency_root=/mnt/ultimatefish/angel-graph-v5-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v5-stage-17da6cea
model_sha=53d55edc1143bb30e0c8e4316c58fee47ffba93d69869c595e98cc13a952993b
inventory_sha=da4c92ae406d1fd0a4932f6b1126216242d14afadcc15d6f7b5a0db19f080261
manifest_sha=990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6
pawn_sha=42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
promoted_key=results/angel-graph-v1/concrete/v2/model/68b7198f71f9cc671a3a4c72fb849f44d8d4ccaddbe348c58ed9838238ca9d59/wave-0/sha256/0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c/kqueenkangel-0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c.tar.zst
promoted_version=PhoLsJIi0SeWiAS8NAoFMPE7lqc6lBt6
promoted_archive_sha=0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c
promoted_sha=9f808b1600e53f40e29fb8282af11b8486c0325cc9a445d973711bcc730c7ab1
restore="$stage_root/promoted-queen-restore-v1"

test -d "$source_root"
test -d "$dependency_root"
test -d "$stage_root"
test ! -e "$dependency_root/kqueenkangel.uftb"
test ! -e "$restore"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"
test "$(sha256sum "$dependency_root/kpawnk.uftb" | cut -d' ' -f1)" = "$pawn_sha"
actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"

install -d -m 0755 "$restore"
aws s3api get-object --bucket "$bucket" --key "$promoted_key" \
  --version-id "$promoted_version" --region "$region" \
  "$restore/archive.tar.zst"
test "$(sha256sum "$restore/archive.tar.zst" | cut -d' ' -f1)" = "$promoted_archive_sha"
tar --zstd -xf "$restore/archive.tar.zst" -C "$restore"
test "$(stat -c %s "$restore/tablebases/kqueenkangel.uftb")" = 94894864
test "$(sha256sum "$restore/tablebases/kqueenkangel.uftb" | cut -d' ' -f1)" = "$promoted_sha"
chmod u+w "$dependency_root"
install -m 0444 "$restore/tablebases/kqueenkangel.uftb" \
  "$dependency_root/kqueenkangel.uftb"
chmod a-w "$dependency_root"
echo "ANGEL_V5_PROMOTION_STAGE_RESUMED model=$model_sha inventory=$inventory_sha pawn=$pawn_sha promoted=$promoted_sha"
