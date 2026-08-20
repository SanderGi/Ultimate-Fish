#!/usr/bin/env bash
# Extend i03's authenticated Angel-v5 source stage with Pawn/Angel dependencies.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_root=/mnt/ultimatefish/angel-graph-v5-source-17da6cea
dependency_root=/mnt/ultimatefish/angel-graph-v5-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v5-stage-17da6cea

source_sha=17da6cea7c01926a20ab76046b7e5a8fad2fa80eb778bb4e19d8f38b5df1a5b6
model_sha=53d55edc1143bb30e0c8e4316c58fee47ffba93d69869c595e98cc13a952993b
inventory_sha=da4c92ae406d1fd0a4932f6b1126216242d14afadcc15d6f7b5a0db19f080261
tablebase_sha=7998cb143d183ee901ea4880bbe650d792aee5bc7b7ce571aecff9b9c3fc9161
runner_sha=b3a738478f375f268e0b1903889cc80953e6edd68cdc8a43a5b41813d580edd3

dependency_key=staging/concrete-wave0-batch/dependencies/sha256/58adaf86c71dfc91d60ebf556f38a193b877e962b016c6be53a10b15a713804d/dependencies-full-990832f4.tar.zst
dependency_version=eRzyc1ioApWyZaPVo_DnH1R20TaY80Vk
dependency_sha=58adaf86c71dfc91d60ebf556f38a193b877e962b016c6be53a10b15a713804d
manifest_sha=990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6
pawn_key=staging/concrete/dependencies/sha256/42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844/kpawnk.uftb
pawn_version=LpBPCebQup_Fveb_lDZhB1JP2CipLz0J
pawn_sha=42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
promoted_key=results/angel-graph-v1/concrete/v2/model/68b7198f71f9cc671a3a4c72fb849f44d8d4ccaddbe348c58ed9838238ca9d59/wave-0/sha256/0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c/kqueenkangel-0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c.tar.zst
promoted_version=PhoLsJIi0SeWiAS8NAoFMPE7lqc6lBt6
promoted_archive_sha=0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c
promoted_sha=9f808b1600e53f40e29fb8282af11b8486c0325cc9a445d973711bcc730c7ab1

test -d "$source_root"
test -f "$stage_root/source.tar"
test "$(sha256sum "$stage_root/source.tar" | cut -d' ' -f1)" = "$source_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = "$tablebase_sha"
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = "$runner_sha"
actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"

test ! -e "$dependency_root"
install -d -m 0755 "$dependency_root" "$stage_root/i03-pawn-restore"
aws s3api get-object --bucket "$bucket" --key "$dependency_key" \
  --version-id "$dependency_version" --region "$region" \
  "$stage_root/i03-pawn-restore/dependencies.tar.zst"
test "$(sha256sum "$stage_root/i03-pawn-restore/dependencies.tar.zst" | cut -d' ' -f1)" = "$dependency_sha"
tar --zstd -xf "$stage_root/i03-pawn-restore/dependencies.tar.zst" -C "$dependency_root"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"

aws s3api get-object --bucket "$bucket" --key "$pawn_key" \
  --version-id "$pawn_version" --region "$region" "$dependency_root/kpawnk.uftb"
test "$(stat -c %s "$dependency_root/kpawnk.uftb")" = 2464840
test "$(sha256sum "$dependency_root/kpawnk.uftb" | cut -d' ' -f1)" = "$pawn_sha"

aws s3api get-object --bucket "$bucket" --key "$promoted_key" \
  --version-id "$promoted_version" --region "$region" \
  "$stage_root/i03-pawn-restore/queen-angel.tar.zst"
test "$(sha256sum "$stage_root/i03-pawn-restore/queen-angel.tar.zst" | cut -d' ' -f1)" = "$promoted_archive_sha"
tar --zstd -xf "$stage_root/i03-pawn-restore/queen-angel.tar.zst" \
  -C "$stage_root/i03-pawn-restore"
test "$(stat -c %s "$stage_root/i03-pawn-restore/tablebases/kqueenkangel.uftb")" = 94894864
test "$(sha256sum "$stage_root/i03-pawn-restore/tablebases/kqueenkangel.uftb" | cut -d' ' -f1)" = "$promoted_sha"
install -m 0444 "$stage_root/i03-pawn-restore/tablebases/kqueenkangel.uftb" \
  "$dependency_root/kqueenkangel.uftb"

python3 -c '
import hashlib, json, sys
source, target = sys.argv[1:]
payload = open(source, "rb").read()
base_sha = "990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6"
assert hashlib.sha256(payload).hexdigest() == base_sha
document = json.loads(payload)
record = {"bytes": 94894864, "filename": "kqueenkangel.uftb", "sha256": "9f808b1600e53f40e29fb8282af11b8486c0325cc9a445d973711bcc730c7ab1"}
assert all(item["filename"] != record["filename"] for item in document["files"])
document["files"].append(record)
document["files"].sort(key=lambda item: item["filename"])
document["augmentation"] = {"schema": "angel-v5-pawn-promotion-dependency-v1", "base_manifest_sha256": base_sha, "archive_sha256": "0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c", "archive_version_id": "PhoLsJIi0SeWiAS8NAoFMPE7lqc6lBt6"}
open(target, "xb").write((json.dumps(document, indent=2, sort_keys=True) + "\n").encode())
' "$dependency_root/manifest.json" "$stage_root/manifest-pawn-angel-v1.json"
test "$(sha256sum "$stage_root/manifest-pawn-angel-v1.json" | cut -d' ' -f1)" = da672de18b89da05c76e8c2299e9c54226d7f73a22b10966f3b3291f54b4b9aa

chmod -R a-w "$dependency_root"
echo "ANGEL_V5_I03_PAWN_STAGE_OK model=$model_sha manifest=da672de18b89da05c76e8c2299e9c54226d7f73a22b10966f3b3291f54b4b9aa pawn=$pawn_sha promoted=$promoted_sha"
