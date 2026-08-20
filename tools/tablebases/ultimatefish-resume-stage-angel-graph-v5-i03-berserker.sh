#!/usr/bin/env bash
# Complete the authenticated Angel-v5 dependency cache with Berserker-v-K.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
dependency_key=sources/dependencies/legacy-concrete/sha256/f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1/kberserkerk.uftb
dependency_version=mRRAjmezTZfm6VYBs5bRrAMM4Z1yeqmx
dependency_sha=f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
dependency_bytes=12324040
manifest_sha=990832f4acaf9c97d4e7d6940c4b977862cfaa065d8c41bba5438035b36653d6

dependency_root=/mnt/ultimatefish/angel-graph-v5-dependencies-990832f4
stage_root=/mnt/ultimatefish/angel-graph-v5-stage-17da6cea/berserker-dependency-restore-v1
target="$dependency_root/kberserkerk.uftb"

test -d "$dependency_root"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"
test ! -e "$stage_root"
test ! -e "$target"
install -d -m 0755 "$stage_root"

aws s3api get-object --bucket "$bucket" --key "$dependency_key" \
  --version-id "$dependency_version" --region "$region" \
  "$stage_root/kberserkerk.uftb"
test "$(stat -c %s "$stage_root/kberserkerk.uftb")" = "$dependency_bytes"
test "$(sha256sum "$stage_root/kberserkerk.uftb" | cut -d' ' -f1)" = "$dependency_sha"
install -m 0444 "$stage_root/kberserkerk.uftb" "$target"
test "$(stat -c %s "$target")" = "$dependency_bytes"
test "$(sha256sum "$target" | cut -d' ' -f1)" = "$dependency_sha"

echo "ANGEL_V5_BERSERKER_DEPENDENCY_OK sha256=$dependency_sha bytes=$dependency_bytes manifest=$manifest_sha"
