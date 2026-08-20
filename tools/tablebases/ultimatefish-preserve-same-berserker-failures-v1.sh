#!/bin/bash
set -euo pipefail

# Preserve the two superseded memory-gate failures as a compact, exact,
# independently restored S3 archive.  This script deliberately does not
# remove the failed scratch: deletion is a separate, manually audited gate.

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly v9=/mnt/ultimatefish/resume-berserker-same-local-v9
readonly v11=/mnt/ultimatefish/resume-berserker-same-local-v11
readonly source=/mnt/ultimatefish/resume-berserker-same-source-v6
readonly work=/mnt/ultimatefish/same-berserker-failure-evidence-v1
readonly evidence="${work}/evidence"
readonly manifest="${work}/artifact-manifest.json"
readonly archive_tool=/mnt/ultimatefish/remap-fix-v1-source/tools/tablebases/archive_ultimate_aws_result.py

test ! -e "${work}"
test "$(sha256sum "${archive_tool}" | cut -d ' ' -f 1)" = \
  7ce682e83eb234fe5d5391a8f655b4995df7ed13a055f9dda6f040a914a66fec
test "$(sha256sum "${source}/scratch/kberserkerberserkerk.nodes" | cut -d ' ' -f 1)" = \
  2fdf0b96d9d14fdb65bfa63bdc7e9820787c1416c4a7df6e4e2d3c2f9e9d83d3
test "$(sha256sum "${source}/scratch/kberserkerberserkerk.degrees" | cut -d ' ' -f 1)" = \
  992fa27219815c9f1a17a535a100fe201ca6e246e2dbb449616b671ba37f2523

install -d -m 0755 \
  "${evidence}/runs/v9/logs/generate" \
  "${evidence}/runs/v11/logs/generate" \
  "${evidence}/units" \
  "${work}/archives"

for run in v9 v11; do
  if [[ "${run}" == v9 ]]; then
    root="${v9}"
  else
    root="${v11}"
  fi
  cp --reflink=never --preserve=mode,timestamps \
    "${root}/resume-plan.json" "${evidence}/runs/${run}/resume-plan.json"
  cp --reflink=never --preserve=mode,timestamps \
    "${root}/logs/generate/kberserkerberserkerk.log" \
    "${evidence}/runs/${run}/logs/generate/kberserkerberserkerk.log"
  cp --reflink=never --preserve=mode,timestamps \
    "${root}/logs/generate/kberserkerberserkerk.resources.json" \
    "${evidence}/runs/${run}/logs/generate/kberserkerberserkerk.resources.json"
done

for unit in \
  ultimatefish-resume-berserker-same-i03-v9.service \
  ultimatefish-resume-berserker-same-i03-v10.service \
  ultimatefish-resume-berserker-same-i03-v11.service; do
  systemctl show "${unit}" > "${evidence}/units/${unit}.show.txt"
  journalctl -u "${unit}" --no-pager --output=short-iso-precise > \
    "${evidence}/units/${unit}.journal.txt"
done

df -B1 /mnt/ultimatefish > "${evidence}/filesystem-before.txt"
: > "${evidence}/open-handles.txt"
: > "${evidence}/proc-references.txt"
for root in "${v9}" "${v11}"; do
  lsof -- \
    "${root}/scratch/kberserkerberserkerk.offsets" \
    "${root}/scratch/kberserkerberserkerk.predecessors" \
    >> "${evidence}/open-handles.txt" 2>&1 || true
  for target in \
    "${root}/scratch/kberserkerberserkerk.offsets" \
    "${root}/scratch/kberserkerberserkerk.predecessors"; do
    find /proc -maxdepth 3 -type l -lname "${target}" -print \
      >> "${evidence}/proc-references.txt" 2>/dev/null || true
  done
done
test ! -s "${evidence}/open-handles.txt"
test ! -s "${evidence}/proc-references.txt"

{
  for root in "${v9}" "${v11}"; do
    find "${root}" -maxdepth 3 -type f \
      -printf '%p\t%s\t%b\t%i\t%TY-%Tm-%TdT%TH:%TM:%TS%Tz\n' | sort
  done
} > "${evidence}/scratch-inventory.tsv"

sha256sum \
  "${v9}/scratch/kberserkerberserkerk" \
  "${v9}/scratch/kberserkerberserkerk.nodes" \
  "${v9}/scratch/kberserkerberserkerk.degrees" \
  "${v11}/scratch/kberserkerberserkerk" \
  "${v11}/scratch/kberserkerberserkerk.nodes" \
  "${v11}/scratch/kberserkerberserkerk.degrees" \
  > "${evidence}/retained-plane-sha256.txt"

aws s3api head-object --region us-west-2 --bucket "${bucket}" \
  --key checkpoints/current-concrete-7088db91/kberserkerberserkerk/nodes-2fdf0b96d9d14fdb65bfa63bdc7e9820787c1416c4a7df6e4e2d3c2f9e9d83d3.bin \
  --version-id _oWIZINuGQLoTFu6ALG5lAFzJkjY9_Q3 \
  > "${evidence}/frontier-nodes-s3-head.json"
aws s3api head-object --region us-west-2 --bucket "${bucket}" \
  --key checkpoints/current-concrete-7088db91/kberserkerberserkerk/degrees-992fa27219815c9f1a17a535a100fe201ca6e246e2dbb449616b671ba37f2523.bin \
  --version-id pSJZzbK9T16_QkGb.Gzip79BX8dCMMJU \
  > "${evidence}/frontier-degrees-s3-head.json"

python3 -c 'import hashlib,json,pathlib,sys; root=pathlib.Path(sys.argv[1]); evidence=root/"evidence"; records=[]; [(records.append({"path":p.relative_to(root).as_posix(),"bytes":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()})) for p in sorted(evidence.rglob("*")) if p.is_file()]; payload={"schema":"ultimate-failure-evidence-manifest-v1","class":"same:berserker+berserker","failed_runs":["v9","v11"],"superseded_by":"v12-certified","failure_kind":"resident-memory-gate","frontier_manifest_sha256":"afe2e24beb30c7a6703bc4811d2fae3f1c0505a7bfb82246a632f597606f4fc4","artifacts":records}; pathlib.Path(sys.argv[2]).write_text(json.dumps(payload,indent=2,sort_keys=True)+"\n")' \
  "${work}" "${manifest}"

python3 "${archive_tool}" \
  --root "${work}" \
  --artifact-manifest "${manifest}" \
  --kind same-berserker-memory-gate-failures-v9-v11 \
  --output-dir "${work}/archives" \
  --s3-prefix "s3://${bucket}" \
  --zstd-level 19 \
  | tee "${work}/preservation-receipt.json"

test -s "${work}/preservation-receipt.json"
find "${work}" -maxdepth 4 -type f -printf '%p:%s\n' | sort
