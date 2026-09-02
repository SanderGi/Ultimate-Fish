#!/usr/bin/env bash
set -uo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 PRESERVER BUCKET MANIFEST" >&2
  exit 2
fi

readonly preserver=$1
readonly bucket=$2
readonly manifest=$3

test -x "${preserver}" || {
  echo "missing executable preserver: ${preserver}" >&2
  exit 1
}
test -s "${manifest}" || {
  echo "missing certification sprint manifest: ${manifest}" >&2
  exit 1
}

failures=0
quarantine_preservation() {
  local root=$1
  local preserve=$2
  local reason=$3
  local target="${root}/preservation-failed-$(date -u +%Y%m%dT%H%M%SZ)-$$"
  local suffix=0
  while [[ -e "${target}" ]]; do
    suffix=$((suffix + 1))
    target="${root}/preservation-failed-$(date -u +%Y%m%dT%H%M%SZ)-$$.${suffix}"
  done
  mv "${preserve}" "${target}"
  echo "certification_sprint_preserve_quarantined reason ${reason} path ${target}" >&2
}

while IFS='|' read -r root stem unit prefix threads proof_log result_stem arbitrary_extension proof_profile; do
  [[ -n "${root}" && "${root}" != \#* ]] || continue
  active=$(systemctl show "${unit}" -p ActiveState --value 2>/dev/null || true)
  result=$(systemctl show "${unit}" -p Result --value 2>/dev/null || true)
  # Long-running Type=oneshot solver units remain `activating` until their
  # ExecStart process and every proof/certification phase have returned.  Do
  # not mistake that state (or a reload transition) for completion merely
  # because it is not literally `active`.
  [[ "${active}" != active && "${active}" != activating &&
     "${active}" != reloading ]] || continue
  # systemd-run --collect may remove a successful transient unit before this
  # two-minute timer samples it.  A still-present unit must have succeeded; a
  # collected unit is allowed to reach the preservation verifier only when
  # both result files exist.  The verifier then authenticates their complete
  # headers, proof log, hashes, and conservation certificates, so a failed run
  # with partial output cannot be promoted merely because its unit vanished.
  [[ -z "${result}" || "${result}" == success ]] || continue

  # Parallel sprint jobs normally write ROOT/work/results.  The retained
  # Prince continuation predates that layout and writes ROOT/results; expose
  # it through a stable relative symlink so the authenticated common
  # preservation tool can be reused without copying or renaming result bytes.
  if [[ ! -e "${root}/work/results" && -d "${root}/results" ]]; then
    mkdir -p "${root}/work"
    ln -s ../results "${root}/work/results"
  fi
  result_stem=${result_stem:-${stem}}
  arbitrary_extension=${arbitrary_extension:-ufgd}
  proof_profile=${proof_profile:-ghost_dragon}
  [[ -s "${root}/work/results/${result_stem}.ufiw" &&
     -s "${root}/work/results/${result_stem}.${arbitrary_extension}" ]] || continue

  preserve=${root}/preservation-v1
  receipt=${preserve}/receipt.json
  receipt_put=${preserve}/receipt-upload.json
  # A failed preserver used to leave the canonical directory behind, causing
  # every later timer firing to skip the attempt forever.  Retain such evidence
  # under a timestamped name and retry from the unchanged result files.  A
  # nonempty but truncated/corrupt receipt is treated the same way rather than
  # being uploaded as certification evidence.
  if [[ -e "${preserve}" && ! -s "${receipt}" ]]; then
    quarantine_preservation "${root}" "${preserve}" missing-receipt
  elif [[ -s "${receipt}" ]] && ! python3 - "${receipt}" <<'PY'
import hashlib
import json
import pathlib
import sys

receipt = json.loads(pathlib.Path(sys.argv[1]).read_text())
if receipt.get("schema") != "ultimate-information-preservation-receipt-v1":
    raise SystemExit(1)
if receipt.get("verification") != {
    "head_residual": 0,
    "download_residual": 0,
    "archive_restore_residual": 0,
    "artifact_hash_residual": 0,
}:
    raise SystemExit(1)
for name in ("archive", "certificate"):
    item = receipt.get(name, {})
    path = pathlib.Path(str(item.get("path", "")))
    if not path.is_file() or path.stat().st_size != int(item.get("size", -1)):
        raise SystemExit(1)
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    if digest.hexdigest() != item.get("sha256"):
        raise SystemExit(1)
    if not item.get("key") or not item.get("version_id"):
        raise SystemExit(1)
PY
  then
    quarantine_preservation "${root}" "${preserve}" invalid-receipt
  fi
  if [[ ! -e "${preserve}" ]]; then
    echo "certification_sprint_preserve_start stem ${stem} unit ${unit}"
    args=("${root}" "${stem}" "${unit}" "${bucket}" "${prefix}" "${threads}")
    if [[ -n "${proof_log}" ]]; then
      args+=("${proof_log}")
    fi
    if [[ "${result_stem}" != "${stem}" ]]; then
      # Preserve the optional positional proof-log slot even when a caller
      # does not need an explicit proof file.
      [[ -n "${proof_log}" ]] || args+=("")
      args+=("${result_stem}")
    fi
    if [[ "${arbitrary_extension}" != ufgd ||
          "${proof_profile}" != ghost_dragon ]]; then
      # Preserve every preceding optional slot so the profile cannot be
      # shifted into the result-stem position for a default-named result.
      while [[ ${#args[@]} -lt 8 ]]; do args+=(""); done
      args+=("${arbitrary_extension}" "${proof_profile}")
    fi
    if "${preserver}" "${args[@]}"; then
      echo "certification_sprint_preserve_complete stem ${stem} unit ${unit}"
    else
      status=$?
      echo "certification_sprint_preserve_failed stem ${stem} unit ${unit} status ${status}" >&2
      failures=$((failures + 1))
      continue
    fi
  fi

  # Publish the authenticated receipt itself under a content-addressed key.
  # This makes final ledger import discoverable without trusting a mutable
  # host path.  A crash after archive preservation but before this upload is
  # recoverable on the next timer firing.
  [[ -s "${receipt}" ]] || continue
  [[ ! -e "${receipt_put}" ]] || continue
  receipt_sha=$(sha256sum "${receipt}" | cut -d' ' -f1)
  receipt_size=$(stat -c %s "${receipt}")
  receipt_key=${prefix}/receipts/sha256/${receipt_sha}/${stem}.receipt.json
  temporary=${receipt_put}.tmp
  if aws s3api put-object --region us-west-2 --bucket "${bucket}" \
      --key "${receipt_key}" --body "${receipt}" \
      --metadata "sha256=${receipt_sha}" >"${temporary}"; then
    receipt_version=$(python3 -c \
      'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' \
      "${temporary}")
    head=$(aws s3api head-object --region us-west-2 --bucket "${bucket}" \
      --key "${receipt_key}" --version-id "${receipt_version}" \
      --query '[ContentLength,Metadata.sha256]' --output text)
    if [[ "${head}" == "${receipt_size}"$'\t'"${receipt_sha}" ]]; then
      mv "${temporary}" "${receipt_put}"
      echo "certification_sprint_receipt_complete stem ${stem} sha256 ${receipt_sha} version ${receipt_version}"
    else
      echo "certification_sprint_receipt_head_failed stem ${stem}" >&2
      rm -f "${temporary}"
      failures=$((failures + 1))
    fi
  else
    rm -f "${temporary}"
    failures=$((failures + 1))
  fi
done <"${manifest}"

exit "${failures}"
