#!/bin/bash
set -euo pipefail

if [[ $# -lt 6 || $# -gt 10 ]]; then
  echo "usage: $0 ROOT STEM UNIT BUCKET PREFIX THREADS [PROOF_LOG] [RESULT_STEM] [ARBITRARY_EXTENSION] [PROOF_PROFILE]" >&2
  exit 2
fi

readonly root=$1
readonly stem=$2
readonly unit=$3
readonly bucket=$4
readonly prefix=${5%/}
readonly threads=$6
readonly proof_log=${7:-}
readonly result_stem=${8:-${stem}}
readonly arbitrary_extension=${9:-ufgd}
readonly proof_profile=${10:-ghost_dragon}
readonly result_root=${root}/work/results
readonly overlay=${result_root}/${result_stem}.ufiw
readonly arbitrary=${result_root}/${result_stem}.${arbitrary_extension}
readonly preserve=${root}/preservation-v1
readonly stage=${preserve}/stage
readonly restore=${preserve}/restore
readonly archive=${preserve}/${stem}.information-v1.tar.zst
readonly certificate=${preserve}/${stem}.preservation-certificate-v1.json
readonly receipt=${preserve}/receipt.json

active=$(systemctl show "${unit}" -p ActiveState --value 2>/dev/null || true)
result=$(systemctl show "${unit}" -p Result --value 2>/dev/null || true)
# Persistent units must have exited successfully.  A systemd-run --collect
# producer can disappear between the watcher's terminal check and this second
# check; allow only the fully collected (both fields empty) case.  The two
# nonempty result files, complete proof certificate, packed headers, hashes,
# deterministic archive restore, and S3 HEAD checks below remain mandatory.
if [[ -n "${active}" || -n "${result}" ]]; then
  test "${active}" = inactive
  test "${result}" = success
fi
test -s "${overlay}"
test -s "${arbitrary}"
test ! -e "${preserve}"
mkdir -p "${stage}/payload/results" "${stage}/payload/logs" "${restore}"
install -m 0644 "${overlay}" "${stage}/payload/results/${stem}.ufiw"
install -m 0644 "${arbitrary}" "${stage}/payload/results/${stem}.${arbitrary_extension}"

readonly proof=${stage}/payload/logs/${stem}.proof.log
case "${proof_profile}" in
ghost_dragon)
  patterns=(
    '^information_symbolic_certificate '
    '^information_summary side 0 '
    '^information_summary side 1 '
    '^ghost_dragon_root_conservation '
    '^ghost_dragon_source_normalization '
    '^dragon_ghost_certificate ')
  ;;
ghost_pair)
  test "${arbitrary_extension}" = ufgg
  patterns=(
    '^information_symbolic_certificate '
    '^information_summary side 0 '
    '^information_summary side 1 '
    '^ghost_pair_domain_cache '
    '^ghost_pair_artifacts ')
  ;;
*)
  echo "unsupported information proof profile: ${proof_profile}" >&2
  exit 2
  ;;
esac
for pattern in "${patterns[@]}"; do
  if [[ -n "${proof_log}" ]]; then
    grep -E "${pattern}" "${proof_log}" | tail -1
  else
    journalctl -u "${unit}" -o cat --no-pager | grep -E "${pattern}" | tail -1
  fi
done >"${proof}"
test "$(grep -c '^information_summary side ' "${proof}")" = 2
grep -Eq '^information_symbolic_certificate .*bellman_residual 0 .*monotonicity_residual 0 .*singleton_residual 0 ' "${proof}"
if [[ "${proof_profile}" = ghost_dragon ]]; then
  grep -Eq '^ghost_dragon_root_conservation .*grouping_residual 0 .*conservation_residual 0$' "${proof}"
  grep -Eq '^dragon_ghost_certificate .*dual_force_residual 0 .*structural_residual 0 .*singleton_residual 0 .*source_remap_residual 0 ' "${proof}"
else
  grep -Eq '^ghost_pair_domain_cache .*residual 0$' "${proof}"
  python3 - "${overlay}" "${arbitrary}" "${proof}" <<'PY'
import hashlib
import pathlib
import re
import struct
import sys

overlay, arbitrary, proof = map(pathlib.Path, sys.argv[1:])
line = next((value for value in proof.read_text().splitlines()
             if value.startswith("ghost_pair_artifacts ")), "")
fields = dict(re.findall(r"([a-z0-9_]+) ([0-9a-f]{64})", line))
def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            value.update(block)
    return value.hexdigest()
overlay_bytes = overlay.read_bytes()
if len(overlay_bytes) < 160 or overlay_bytes[:8] != b"UFIW2\0\0\0":
    raise SystemExit("Ghost-pair overlay header residual")
words = struct.unpack_from("<6I", overlay_bytes, 8)
version, primary, secondary, owner, states, substates = words
if (version != 2 or primary != 11 or secondary != 11 or owner != 0 or
        states != 75_915_840 or substates != 4 or
        len(overlay_bytes) != 160 + states or
        any(value & ~7 for value in overlay_bytes[160:])):
    raise SystemExit("Ghost-pair overlay extent/flag residual")
with arbitrary.open("rb") as stream:
    header = stream.read(928)
if (len(header) != 928 or header[:8] != b"UFGG1\0\0\0" or
        struct.unpack_from("<I", header, 8)[0] != 1 or
        struct.unpack_from("<I", header, 12)[0] != 928 or
        struct.unpack_from("<11I", header, 16) !=
        (11, 11, 0, 8, 10, 80, 3003, 75_915_840, 12, 40, 376) or
        struct.unpack_from("<I", header, 60)[0] != 0 or
        header[160:224] != overlay_bytes[32:96] or
        header[224:288] != overlay_bytes[96:160] or
        header[864:928].rstrip(b"\0") !=
        b"correlated-unordered-pair-public-view-v1"):
    raise SystemExit("Ghost-pair arbitrary header residual")
(nodes, geometries, strata, actuals, owner_roots, node_offset,
 geometry_offset, stratum_offset, actual_offset, owner_offset,
 observer_offset, payload_bytes) = struct.unpack_from("<12Q", header, 64)
cursor = 928
for declared, count, width in (
        (node_offset, nodes, 12), (geometry_offset, geometries, 40),
        (stratum_offset, strata, 376), (actual_offset, actuals, 4),
        (owner_offset, owner_roots, 4), (observer_offset, strata, 4)):
    if declared != cursor or count <= 0:
        raise SystemExit("Ghost-pair arbitrary section residual")
    cursor += count * width
if (payload_bytes != cursor - 928 or arbitrary.stat().st_size != cursor):
    raise SystemExit("Ghost-pair arbitrary extent residual")
payload = hashlib.sha256()
with arbitrary.open("rb") as stream:
    stream.seek(928)
    for block in iter(lambda: stream.read(4 << 20), b""):
        payload.update(block)
if payload.hexdigest().encode() != header[800:864]:
    raise SystemExit("Ghost-pair arbitrary payload residual")
if (fields.get("overlay_sha256") != digest(overlay) or
        fields.get("arbitrary_sha256") != digest(arbitrary)):
    raise SystemExit("Ghost-pair result/proof hash residual")
PY
fi

python3 - "${stage}" <<'PY'
import hashlib
import json
import pathlib
import sys

stage = pathlib.Path(sys.argv[1])
artifacts = []
for path in sorted((stage / "payload").rglob("*")):
    if not path.is_file():
        continue
    relative = path.relative_to(stage / "payload").as_posix()
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    artifacts.append({"path": relative, "bytes": path.stat().st_size,
                      "sha256": digest.hexdigest()})
manifest = {"schema": "ultimate-information-preservation-archive-v1",
            "artifacts": artifacts}
(stage / "archive-manifest.json").write_text(
    json.dumps(manifest, sort_keys=True, indent=2) + "\n")
PY

(
  cd "${stage}"
  tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
    -cf - archive-manifest.json \
    "payload/logs/${stem}.proof.log" \
    "payload/results/${stem}.${arbitrary_extension}" \
    "payload/results/${stem}.ufiw"
) | zstd -T"${threads}" -9 --no-progress -o "${archive}"
zstd -t "${archive}"
readonly archive_sha=$(sha256sum "${archive}" | cut -d' ' -f1)
readonly archive_size=$(stat -c %s "${archive}")
readonly key=${prefix}/sha256/${archive_sha}/${stem}.information-v1.tar.zst
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --body "${archive}" \
  --metadata "sha256=${archive_sha}" >"${preserve}/archive-put.json"
readonly version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' \
  "${preserve}/archive-put.json")
test -n "${version}"
readonly head=$(aws s3api head-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --version-id "${version}" \
  --query '[ContentLength,Metadata.sha256]' --output text)
test "${head}" = "${archive_size}"$'\t'"${archive_sha}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --version-id "${version}" \
  "${restore}/archive.tar.zst" >/dev/null
test "$(sha256sum "${restore}/archive.tar.zst" | cut -d' ' -f1)" = "${archive_sha}"
tar --zstd -xf "${restore}/archive.tar.zst" -C "${restore}"

python3 - "${stage}" "${restore}" <<'PY'
import hashlib
import json
import pathlib
import sys

stage, restore = map(pathlib.Path, sys.argv[1:])
manifest = json.loads((restore / "archive-manifest.json").read_text())
for artifact in manifest["artifacts"]:
    expected = stage / "payload" / artifact["path"]
    actual = restore / "payload" / artifact["path"]
    assert actual.stat().st_size == artifact["bytes"]
    expected_digest = hashlib.sha256()
    actual_digest = hashlib.sha256()
    with expected.open("rb") as left, actual.open("rb") as right:
        while True:
            left_block = left.read(4 << 20)
            right_block = right.read(4 << 20)
            assert left_block == right_block
            if not left_block:
                break
            expected_digest.update(left_block)
            actual_digest.update(right_block)
    assert expected_digest.hexdigest() == artifact["sha256"]
    assert actual_digest.hexdigest() == artifact["sha256"]
PY

python3 - "${certificate}" "${archive_sha}" "${archive_size}" \
  "${key}" "${version}" "${stem}" <<'PY'
import json
import pathlib
import sys

path, digest, size, key, version, stem = sys.argv[1:]
value = {
    "schema": "ultimate-information-preservation-certificate-v1",
    "filename": stem + ".uftb",
    "sha256": digest,
    "size": int(size),
    "s3": {
        "key": key,
        "version_id": version,
        "sha256": digest,
        "head_residual": 0,
        "download_residual": 0,
        "archive_restore_residual": 0,
    },
}
pathlib.Path(path).write_text(json.dumps(value, sort_keys=True, indent=2) + "\n")
PY
readonly certificate_sha=$(sha256sum "${certificate}" | cut -d' ' -f1)
readonly certificate_size=$(stat -c %s "${certificate}")
readonly certificate_key=${prefix}/certificates/sha256/${certificate_sha}/${stem}.preservation-certificate-v1.json
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${certificate_key}" --body "${certificate}" \
  --metadata "sha256=${certificate_sha}" >"${preserve}/certificate-put.json"
readonly certificate_version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' \
  "${preserve}/certificate-put.json")
test -n "${certificate_version}"
readonly certificate_head=$(aws s3api head-object --region us-west-2 \
  --bucket "${bucket}" --key "${certificate_key}" \
  --version-id "${certificate_version}" \
  --query '[ContentLength,Metadata.sha256]' --output text)
test "${certificate_head}" = "${certificate_size}"$'\t'"${certificate_sha}"

python3 - "${receipt}" "${archive}" "${archive_sha}" "${archive_size}" \
  "${key}" "${version}" "${certificate}" "${certificate_sha}" \
  "${certificate_size}" "${certificate_key}" "${certificate_version}" <<'PY'
import json
import pathlib
import sys

(receipt, archive, archive_sha, archive_size, key, version, certificate,
 certificate_sha, certificate_size, certificate_key,
 certificate_version) = sys.argv[1:]
value = {
    "schema": "ultimate-information-preservation-receipt-v1",
    "archive": {"path": archive, "sha256": archive_sha,
                "size": int(archive_size), "key": key,
                "version_id": version},
    "certificate": {"path": certificate, "sha256": certificate_sha,
                    "size": int(certificate_size), "key": certificate_key,
                    "version_id": certificate_version},
    "verification": {"head_residual": 0, "download_residual": 0,
                     "archive_restore_residual": 0,
                     "artifact_hash_residual": 0},
}
pathlib.Path(receipt).write_text(json.dumps(value, sort_keys=True, indent=2) + "\n")
PY
cat "${receipt}"
