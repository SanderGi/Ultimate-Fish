#!/usr/bin/env bash
# Preserve the fail-closed pre-allocation v1 attempt without modifying it.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
unit=ultimatefish-angel-v6-kberserkerangelk-v1.service
work=/mnt/ultimatefish/angel-graph-v6-work/kberserkerangelk-v1
evidence=/mnt/ultimatefish/angel-graph-v6-failures/kberserkerangelk-v1
expected_plan=1852c2d8c585eca2600246efc5e25d3d1a72403915d10879819e6f920036a0a4

test -d "$work"
test -f "$work/run-plan.json"
test "$(sha256sum "$work/run-plan.json" | cut -d' ' -f1)" = "$expected_plan"
test "$(systemctl show "$unit" -p ActiveState --value)" = failed
test ! -e "$evidence"
install -d -m 0755 "$evidence"

systemctl show "$unit" --no-pager > "$evidence/unit-properties.txt"
journalctl -u "$unit" --no-pager > "$evidence/journal.log"
find "$work" -type f -print0 | sort -z | xargs -0 sha256sum \
  > "$evidence/work-inventory.sha256"

archive="$evidence/failure-evidence.tar.zst"
tar --sort=name --mtime='UTC 1970-01-01' --owner=0 --group=0 \
  --numeric-owner -C /mnt/ultimatefish -cf - \
  angel-graph-v6-work/kberserkerangelk-v1 \
  angel-graph-v6-failures/kberserkerangelk-v1/unit-properties.txt \
  angel-graph-v6-failures/kberserkerangelk-v1/journal.log \
  angel-graph-v6-failures/kberserkerangelk-v1/work-inventory.sha256 \
  | zstd -19 -T2 -o "$archive"

archive_sha=$(sha256sum "$archive" | cut -d' ' -f1)
archive_bytes=$(stat -c %s "$archive")
key="failures/angel-graph-v6/same-berserker/v1/sha256/$archive_sha/failure-evidence.tar.zst"
version=$(aws s3api put-object --bucket "$bucket" --key "$key" \
  --body "$archive" --metadata \
  "sha256=$archive_sha,purpose=angel-v6-same-berserker-failed-v1" \
  --region "$region" --query VersionId --output text)

restore="$evidence/s3-restore.tar.zst"
aws s3api get-object --bucket "$bucket" --key "$key" --version-id "$version" \
  --region "$region" "$restore" >/dev/null
test "$(stat -c %s "$restore")" = "$archive_bytes"
test "$(sha256sum "$restore" | cut -d' ' -f1)" = "$archive_sha"
test "$(aws s3api head-object --bucket "$bucket" --key "$key" \
  --version-id "$version" --region "$region" --query ContentLength \
  --output text)" = "$archive_bytes"
test "$(aws s3api head-object --bucket "$bucket" --key "$key" \
  --version-id "$version" --region "$region" --query Metadata.sha256 \
  --output text)" = "$archive_sha"

echo "ANGEL_V6_SAME_BERSERKER_FAILED_V1_PRESERVED sha256=$archive_sha bytes=$archive_bytes version_id=$version key=$key run_plan_sha256=$expected_plan"
