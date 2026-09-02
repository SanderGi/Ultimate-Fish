#!/usr/bin/env bash
# Deterministically archive and restore-verify one immutable Angel/Ghost graph.
set -euo pipefail

readonly orientation=${1:?orientation required}
readonly threads=${2:?thread count required}
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/ghost-angel-v9-159eea06

case "${orientation}" in
  same)
    readonly marker_sha=2aac70e770a0321f78183404f0e9baad3e140f4fcd74662b78023c378b8d6b64
    ;;
  opposing)
    readonly marker_sha=1a0e5e47675cd25339471146339a7d5d46d2da5f9bcd20c91e463c43bdbeafff
    ;;
  *)
    echo "unsupported orientation: ${orientation}" >&2
    exit 2
    ;;
esac

readonly work=${root}/work-${orientation}
readonly archive=${root}/ghost-angel-${orientation}-transitions-v1.tar.zst
readonly restored=${archive}.restored
readonly receipt=${root}/ghost-angel-${orientation}-transitions-v1-preserved.json

test "$(sha256sum "${work}/work/transitions-ready.json" | cut -d' ' -f1)" = "${marker_sha}"
test -d "${work}/work/transitions"
test ! -e "${archive}"
test ! -e "${restored}"
test ! -e "${receipt}"
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 107374182400

tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
  -C "${work}/work" -cf - transitions transitions-ready.json |
  zstd -T"${threads}" -3 -q -o "${archive}"

readonly archive_sha=$(sha256sum "${archive}" | cut -d' ' -f1)
readonly archive_size=$(stat -c %s "${archive}")
readonly key=results/ghost-angel-v9/transition-graphs/${orientation}/sha256/${archive_sha}/ghost-angel-${orientation}-transitions-v1.tar.zst
readonly version=$(aws s3api put-object \
  --bucket "${bucket}" --key "${key}" --body "${archive}" \
  --metadata "sha256=${archive_sha},marker-sha256=${marker_sha},orientation=${orientation}" \
  --query VersionId --output text)

test -n "${version}"
test "${version}" != None
aws s3api get-object --bucket "${bucket}" --key "${key}" \
  --version-id "${version}" "${restored}" >/dev/null
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${archive_sha}"
cmp -s "${archive}" "${restored}"

/usr/bin/python3 - "${receipt}" "${orientation}" "${marker_sha}" \
  "${archive_sha}" "${archive_size}" "${key}" "${version}" <<'PY'
import json
import pathlib
import sys

path, orientation, marker_sha, archive_sha, archive_size, key, version = sys.argv[1:]
payload = {
    "archive_sha256": archive_sha,
    "archive_size": int(archive_size),
    "bucket": "ultimatefish-info-20260808-a4e679c6-831688117652",
    "key": key,
    "marker_sha256": marker_sha,
    "orientation": orientation,
    "restore_byte_identical": True,
    "restore_sha256": archive_sha,
    "schema": 1,
    "version_id": version,
}
pathlib.Path(path).write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
PY

sha256sum "${receipt}"
