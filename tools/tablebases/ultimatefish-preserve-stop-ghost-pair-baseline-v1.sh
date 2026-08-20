#!/bin/bash
set -euo pipefail

# Stop the six-day uncached domain-root baseline only after the exact cached
# replacement has completed every domain and demonstrably entered its first
# fixed-point sweep. Preserve the stopped attempt locally and as an exact,
# independently restored S3 evidence archive; delete no baseline file.

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly baseline_unit=ultimatefish-info-specialized-5bbb1395-kghostghostk-v1.service
readonly candidate_unit=ultimatefish-run-ghost-pair-domain-cache-v1.service
readonly root=/mnt/ultimatefish/info-wave-aa82210e-work/kghostghostk
readonly baseline_scratch="${root}/work/solve/kghostghostk"
readonly candidate_scratch="${root}/work/solve-domain-cache-v1/kghostghostk"
readonly work=/mnt/ultimatefish/ghost-pair-domain-baseline-evidence-v1
readonly evidence="${work}/evidence"
readonly manifest="${work}/artifact-manifest.json"
readonly archive_tool=/mnt/ultimatefish/remap-fix-v1-source/tools/tablebases/archive_ultimate_aws_result.py

test ! -e "${work}"
test "$(systemctl is-active "${candidate_unit}")" = active
test "$(systemctl is-active "${baseline_unit}")" = active
test "$(sha256sum /mnt/ultimatefish/ghost-pair-domain-cache-v1-build/ultimate_ghost_pair_information_tablebase | cut -d ' ' -f 1)" = \
  e5adb01295e596b6d56a028ebbd3e1b9e3d9fed0a5ad555f2d08180f3f37349a
test "$(sha256sum "${archive_tool}" | cut -d ' ' -f 1)" = \
  7ce682e83eb234fe5d5391a8f655b4995df7ed13a055f9dda6f040a914a66fec
journalctl -u "${candidate_unit}" --no-pager -o cat | \
  grep -F "ghost_pair_domain_roots geometry 9739120/9739120 roots 9739120/9739120 cache_hits 9739117 cache_misses 3 cache_entries 3 bdd_nodes 3004" >/dev/null
test "${candidate_scratch}.bdd-a.nodes" -nt "${candidate_scratch}.domains"

install -d -m 0755 "${evidence}/units" "${evidence}/logs" "${work}/archives"
systemctl cat "${baseline_unit}" > "${evidence}/units/${baseline_unit}.cat.txt"
systemctl show "${baseline_unit}" > "${evidence}/units/${baseline_unit}.before.txt"
journalctl -u "${baseline_unit}" --no-pager --output=short-iso-precise > \
  "${evidence}/units/${baseline_unit}.journal.txt"
systemctl show "${candidate_unit}" > "${evidence}/units/${candidate_unit}.txt"
journalctl -u "${candidate_unit}" --no-pager -o cat | tail -220 > \
  "${evidence}/units/${candidate_unit}.domain-proof.txt"
cp --reflink=never --preserve=mode,timestamps \
  "${root}/work/logs/solve.log" "${evidence}/logs/baseline-solve.log"

systemctl stop "${baseline_unit}"
test "$(systemctl is-active "${baseline_unit}" || true)" != active
systemctl show "${baseline_unit}" > "${evidence}/units/${baseline_unit}.after.txt"

find "${root}/work/solve" -maxdepth 2 -type f \
  -printf '%p\t%s\t%b\t%i\t%TY-%Tm-%TdT%TH:%TM:%TS%Tz\n' | sort > \
  "${evidence}/baseline-scratch-inventory.tsv"
sha256sum "${baseline_scratch}.domains" > \
  "${evidence}/baseline-domains.sha256"
python3 -c 'import array,json,pathlib,sys; path=pathlib.Path(sys.argv[1]); values=array.array("I"); f=path.open("rb"); values.fromfile(f,path.stat().st_size//values.itemsize); f.close(); nonzero=sum(value!=0 for value in values); last=max((index for index,value in enumerate(values) if value),default=-1); payload={"schema":"ultimate-ghost-pair-domain-baseline-v1","domain_roots":len(values),"initialized_roots":nonzero,"last_initialized_root":last,"fraction":nonzero/len(values)}; pathlib.Path(sys.argv[2]).write_text(json.dumps(payload,indent=2,sort_keys=True)+"\n")' \
  "${baseline_scratch}.domains" "${evidence}/baseline-domain-progress.json"
df -B1 /mnt/ultimatefish > "${evidence}/filesystem.txt"

python3 -c 'import hashlib,json,pathlib,sys; root=pathlib.Path(sys.argv[1]); evidence=root/"evidence"; records=[]; [(records.append({"path":p.relative_to(root).as_posix(),"bytes":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()})) for p in sorted(evidence.rglob("*")) if p.is_file()]; payload={"schema":"ultimate-failure-evidence-manifest-v1","class":"same:ghost+ghost","attempt":"six-day-uncached-domain-root-baseline","superseded_by":"same-ghost-pair-domain-cache-v1","deletion_performed":False,"artifacts":records}; pathlib.Path(sys.argv[2]).write_text(json.dumps(payload,indent=2,sort_keys=True)+"\n")' \
  "${work}" "${manifest}"

python3 "${archive_tool}" \
  --root "${work}" \
  --artifact-manifest "${manifest}" \
  --kind ghost-pair-domain-initialization-baseline-v1 \
  --output-dir "${work}/archives" \
  --s3-prefix "s3://${bucket}" \
  --zstd-level 19 | tee "${work}/preservation-receipt.json"

test -s "${work}/preservation-receipt.json"
cat "${evidence}/baseline-domain-progress.json"
cat "${work}/preservation-receipt.json"
