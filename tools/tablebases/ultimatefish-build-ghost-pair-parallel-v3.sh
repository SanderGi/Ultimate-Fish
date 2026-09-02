#!/bin/bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/same-ghost-pair-rebuild-after-stop-v1
readonly work=/mnt/ultimatefish/ghost-pair-parallel-v3-f4b99f10
readonly bundle="${work}/ultimatefish-ghost-pair-parallel-v3-source.tar.gz"
readonly bundle_key=sources/bundles/ghost-pair-parallel-v3/sha256/f4b99f1041b389c0663907872bbfce11b1f53d2365a98de1a15cd809a14f9440/ultimatefish-ghost-pair-parallel-v3-source.tar.gz
readonly bundle_version=MysZN8_WXRj.y1JFH3Ay4sVUhP8e.1pP
readonly source="${work}/source"
readonly binary="${work}/ultimate_ghost_pair_information_tablebase-parallel-v3"
readonly test_binary="${work}/ultimate_ghost_pair_information_solver_test-parallel-v3"

test ! -e "${work}"
install -d -m 0755 "${work}"
cp -a --reflink=auto "${root}/source" "${source}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${bundle_key}" --version-id "${bundle_version}" "${bundle}"
test "$(sha256sum "${bundle}" | cut -d ' ' -f 1)" = \
  f4b99f1041b389c0663907872bbfce11b1f53d2365a98de1a15cd809a14f9440
tar -xzf "${bundle}" -C "${source}"

cd "${source}"
sha256sum -c <(printf '%s  %s\n' \
  ce5ac14ae65b44a45e807319310a2cf2a82bf0ca9b8c70c1b9f02285a8527d92 src/ultimate/tablebases/ghost_pair_information_solver.cpp \
  db0e664da8619775e3e107448403ec3acb7cb3fdbd2b8a838ec267ff339171d5 src/ultimate/tablebases/ghost_pair_information_solver.h \
  55345b46951d7721675fd45e1f122067799e10c0d5f628a195a9243e9016107c src/ultimate/tablebases/ghost_pair_information_tablebase.cpp \
  bd2afad8c009158d27dd6b66225e59d8a4c4b199401e173ad28faee598703b12 tools/tablebases/run_ultimate_ghost_pair_current_aws.py \
  92021874e4cfbe38f0a90bbe1485be519667140187d67e683edd4fefbbfc6b2c src/ultimate/tablebases/ghost_pair_information_model.cpp \
  1a2356b6d4694bb822c8cf059f01d83859cfeef9c4eebbff0b3cef782775cdbc src/ultimate/tablebases/ghost_information_probe.cpp \
  ba2be45ae2ff2cb66e16d9fe5954ff3f85068960002be59e9a0b8d62c7c0f18f src/ultimate/tablebases/information.cpp \
  a937b9e0ecf30dcdc4a87d5b11a326d840720dba474536030fc12d4d598fabf5 src/ultimate/position.cpp \
  2f0119ae52e312dc21a21341063dc9dd179dbebdc24136cdf7b5cb5a37820139 src/ultimate/nnue.cpp)

readonly sources=(
  src/ultimate/tablebases/ghost_pair_information_tablebase.cpp
  src/ultimate/tablebases/ghost_pair_information_solver.cpp
  src/ultimate/tablebases/ghost_pair_information_model.cpp
  src/ultimate/tablebases/ghost_information_probe.cpp
  src/ultimate/tablebases/information.cpp
  src/ultimate/position.cpp
  src/ultimate/nnue.cpp
)
clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Isrc/ultimate -Isrc/ultimate/tablebases "${sources[@]}" -o "${binary}" \
  > "${work}/build.log" 2>&1
clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  tests/tablebases/ultimate_ghost_pair_information_solver.cpp \
  "${sources[@]:1}" -o "${test_binary}" \
  > "${work}/test-build.log" 2>&1
"${test_binary}" > "${work}/test.log" 2>&1
grep -F 'ultimate Ghost-pair information solver tests passed' "${work}/test.log"
"${binary}" --self-test --scratch "${work}/self-test" --workers 8 \
  > "${work}/self-test.log" 2>&1
grep -F 'ghost_pair_self_test residual 0' "${work}/self-test.log"

readonly binary_sha="$(sha256sum "${binary}" | cut -d ' ' -f 1)"
readonly binary_bytes="$(stat -c %s "${binary}")"
readonly binary_key="sources/binaries/ghost-pair-parallel-v3/sha256/${binary_sha}/ultimate_ghost_pair_information_tablebase-parallel-v3"
readonly binary_version="$(aws s3api put-object --region us-west-2 \
  --bucket "${bucket}" --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-bundle-sha256=f4b99f1041b389c0663907872bbfce11b1f53d2365a98de1a15cd809a14f9440" \
  --query VersionId --output text)"
test -n "${binary_version}"
install -d -m 0755 "${work}/restore"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${work}/restore/ultimate_ghost_pair_information_tablebase-parallel-v3"
cmp -s "${binary}" \
  "${work}/restore/ultimate_ghost_pair_information_tablebase-parallel-v3"

python3 -c 'import json,pathlib,sys; pathlib.Path(sys.argv[1]).write_text(json.dumps({"schema":"ultimate-ghost-pair-parallel-build-v3","source_bundle":{"sha256":"f4b99f1041b389c0663907872bbfce11b1f53d2365a98de1a15cd809a14f9440","version_id":"MysZN8_WXRj.y1JFH3Ay4sVUhP8e.1pP"},"binary":{"sha256":sys.argv[2],"bytes":int(sys.argv[3]),"key":sys.argv[4],"version_id":sys.argv[5]},"concurrency_test_residual":0,"self_test_residual":0,"s3_restore_residual":0},indent=2,sort_keys=True)+"\n")' \
  "${work}/build-receipt.json" "${binary_sha}" "${binary_bytes}" \
  "${binary_key}" "${binary_version}"
cat "${work}/build-receipt.json"
