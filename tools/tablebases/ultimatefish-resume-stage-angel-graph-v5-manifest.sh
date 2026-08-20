#!/usr/bin/env bash
# Authenticate an augmented manifest for the restored Pawn promotion lower.
set -euo pipefail

source_root=/mnt/ultimatefish/angel-graph-v5-source-17da6cea
dependency_root=/mnt/ultimatefish/angel-graph-v5-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v5-stage-17da6cea
target=$stage_root/manifest-pawn-angel-v1.json
model_sha=53d55edc1143bb30e0c8e4316c58fee47ffba93d69869c595e98cc13a952993b
inventory_sha=da4c92ae406d1fd0a4932f6b1126216242d14afadcc15d6f7b5a0db19f080261
base_manifest_sha=990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6
pawn_sha=42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
promoted_sha=9f808b1600e53f40e29fb8282af11b8486c0325cc9a445d973711bcc730c7ab1

test -d "$source_root"
test -d "$dependency_root"
test -d "$stage_root"
test ! -e "$target"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$base_manifest_sha"
test "$(sha256sum "$dependency_root/kpawnk.uftb" | cut -d' ' -f1)" = "$pawn_sha"
test "$(sha256sum "$dependency_root/kqueenkangel.uftb" | cut -d' ' -f1)" = "$promoted_sha"
actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"

python3 -c '
import hashlib, json, sys
source, target = sys.argv[1:]
payload = open(source, "rb").read()
base_sha = "990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6"
assert hashlib.sha256(payload).hexdigest() == base_sha
document = json.loads(payload)
record = {
    "bytes": 94894864,
    "filename": "kqueenkangel.uftb",
    "sha256": "9f808b1600e53f40e29fb8282af11b8486c0325cc9a445d973711bcc730c7ab1",
}
assert all(item["filename"] != record["filename"] for item in document["files"])
document["files"].append(record)
document["files"].sort(key=lambda item: item["filename"])
document["augmentation"] = {
    "schema": "angel-v5-pawn-promotion-dependency-v1",
    "base_manifest_sha256": base_sha,
    "archive_sha256": "0729822d02a75495ba7e0fdfaf0f7f20bc50072bc4fb74177d903ca6eeb8a21c",
    "archive_version_id": "PhoLsJIi0SeWiAS8NAoFMPE7lqc6lBt6",
}
open(target, "xb").write((json.dumps(document, indent=2, sort_keys=True) + "\n").encode())
' "$dependency_root/manifest.json" "$target"

manifest_sha=$(sha256sum "$target" | cut -d' ' -f1)
echo "ANGEL_V5_MANIFEST_STAGE_RESUMED model=$model_sha inventory=$inventory_sha manifest=$manifest_sha pawn=$pawn_sha promoted=$promoted_sha"
