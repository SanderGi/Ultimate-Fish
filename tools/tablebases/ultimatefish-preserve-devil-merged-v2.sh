#!/usr/bin/env bash
set -euo pipefail

readonly campaign=/mnt/ultimatefish/devil-spawned-c1-v28
readonly merge=${campaign}/merge-v1
readonly tools_root=/mnt/ultimatefish/devil-preservation-v2/tools/tablebases
readonly work=${campaign}/preservation-v2
readonly fragments=${campaign}/fragments-for-preservation-v2
readonly summary=${work}/preservation-retry/preservation-retry.json

if test -s "${summary}"; then
  exit 0
fi
test "$(sha256sum "${tools_root}/prepare_ultimate_devil_spawned_merge_preservation.py" | cut -d ' ' -f 1)" = \
  13854bfb7146682299a1152c56a18c5a2153db2f47c97e3845094f8d41057210
test "$(sha256sum "${tools_root}/preserve_ultimate_concrete_existing_aws.py" | cut -d ' ' -f 1)" = \
  b335c6ad0636f8043b4f1a13863248cfd02cdaa33615ae33c55af2d302cddab2
test "$(sha256sum "${tools_root}/merge_ultimate_devil_spawned_roots.py" | cut -d ' ' -f 1)" = \
  60b029808ffad9daa3df8555f40f430ff2a2e112fc633451b01b208d0f69a001
test -s "${merge}/kdevilk.uftb"
test -s "${merge}/devil-root-merge-receipt.json"
test ! -e "${work}"

mkdir -p "${fragments}"
cp -f "${campaign}"/fragments-restored-v1/devil-*.roots "${fragments}/"
cp -f "${campaign}/graphs/square-2/devil-2.roots" "${fragments}/"
test "$(find "${fragments}" -maxdepth 1 -name 'devil-*.roots' -type f | wc -l)" = 12

python3 "${tools_root}/prepare_ultimate_devil_spawned_merge_preservation.py" \
  --output "${merge}/kdevilk.uftb" \
  --receipt "${merge}/devil-root-merge-receipt.json" \
  --fragments-directory "${fragments}" \
  --work-directory "${work}"
python3 "${tools_root}/preserve_ultimate_concrete_existing_aws.py" \
  --work-directory "${work}" \
  --filename kdevilk.uftb \
  --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/results/devil-spawned-certified-v2
test -s "${summary}"

