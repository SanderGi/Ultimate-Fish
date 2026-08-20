#!/usr/bin/env bash
# Add the exact same-side Queen/Angel promotion table to i03's Pawn stage.
set -euo pipefail

dependency_root=/mnt/ultimatefish/angel-graph-v5-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v5-stage-17da6cea
restore=$stage_root/i03-same-queen-restore
source_manifest=$stage_root/manifest-pawn-angel-v1.json
target_manifest=$stage_root/manifest-pawn-angel-same-v1.json

archive_sha=ed526cdeb043de2a86226d6629362758f1d0b3f06bcc8acdf368376fb5ebb020
archive_version=4QmWt4TC5PTxUfsr27j1s4mjxMGQ13VO
table_sha=b750e8f77635e11c7e27784ce8dd41e9b6eb0aa61b7a5fdebcf508afbf7a7d0f
table_bytes=142342264

test -d "$dependency_root"
test "$(sha256sum "$source_manifest" | cut -d' ' -f1)" = da672de18b89da05c76e8c2299e9c54226d7f73a22b10966f3b3291f54b4b9aa
test "$(sha256sum "$restore/archive.tar.zst" | cut -d' ' -f1)" = "$archive_sha"
test "$(stat -c %s "$restore/tablebases/kqueenangelk.uftb")" = "$table_bytes"
test "$(sha256sum "$restore/tablebases/kqueenangelk.uftb" | cut -d' ' -f1)" = "$table_sha"
test ! -e "$dependency_root/kqueenangelk.uftb"
test ! -e "$target_manifest"

chmod u+w "$dependency_root"
install -m 0444 "$restore/tablebases/kqueenangelk.uftb" \
  "$dependency_root/kqueenangelk.uftb"

python3 -c '
import hashlib, json, sys
source, target = sys.argv[1:]
payload = open(source, "rb").read()
assert hashlib.sha256(payload).hexdigest() == "da672de18b89da05c76e8c2299e9c54226d7f73a22b10966f3b3291f54b4b9aa"
document = json.loads(payload)
record = {"bytes": 142342264, "filename": "kqueenangelk.uftb", "sha256": "b750e8f77635e11c7e27784ce8dd41e9b6eb0aa61b7a5fdebcf508afbf7a7d0f"}
assert all(item["filename"] != record["filename"] for item in document["files"])
document["files"].append(record)
document["files"].sort(key=lambda item: item["filename"])
document["same_queen_augmentation"] = {"schema": "angel-v5-pawn-same-promotion-dependency-v1", "archive_sha256": "ed526cdeb043de2a86226d6629362758f1d0b3f06bcc8acdf368376fb5ebb020", "archive_version_id": "4QmWt4TC5PTxUfsr27j1s4mjxMGQ13VO"}
open(target, "xb").write((json.dumps(document, indent=2, sort_keys=True) + "\n").encode())
' "$source_manifest" "$target_manifest"

chmod a-w "$dependency_root"
target_sha=$(sha256sum "$target_manifest" | cut -d' ' -f1)
echo "ANGEL_V5_I03_SAME_QUEEN_STAGE_OK archive=$archive_sha version=$archive_version table=$table_sha manifest=$target_sha"
