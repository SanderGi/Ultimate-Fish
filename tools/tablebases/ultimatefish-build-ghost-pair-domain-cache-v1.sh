#!/bin/bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly base=/mnt/ultimatefish/ghost-specialized-current-5bbb1395-source-v1
readonly work=/mnt/ultimatefish/ghost-pair-domain-cache-v1-build
readonly patch_archive="${work}/ghost-pair-domain-cache-v1-source.tar.zst"
readonly source="${work}/source"
readonly binary="${work}/ultimate_ghost_pair_information_tablebase"
readonly patch_key=sources/bundles/ghost-pair-domain-cache-v1/sha256/79418c52ba8f1d28d179719076b8aa061a6b8c7b04729d2286231923136259de/ghost-pair-domain-cache-v1-source.tar.zst
readonly patch_version=R4hyfQbU3keHm3HAS95g4IayBw23smu2

test ! -e "${work}"
install -d -m 0755 "${work}" "${work}/restore" "${work}/s3-verify"
cp -a --reflink=auto "${base}" "${source}"

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${patch_key}" --version-id "${patch_version}" \
  "${patch_archive}"
test "$(sha256sum "${patch_archive}" | cut -d ' ' -f 1)" = \
  79418c52ba8f1d28d179719076b8aa061a6b8c7b04729d2286231923136259de
zstd -d --no-progress -c "${patch_archive}" | \
  tar -xf - -C "${work}/restore"

for relative in \
  src/ultimate/tablebases/ghost_pair_information_solver.cpp \
  src/ultimate/tablebases/ghost_pair_information_solver.h \
  src/ultimate/tablebases/ghost_pair_information_tablebase.cpp \
  src/ultimate/tablebases/information.cpp; do
  cp --reflink=never "${work}/restore/payload/${relative}" \
    "${source}/${relative}"
done

cd "${source}"
sha256sum -c <(printf '%s  %s\n' \
  0e540d42589a452f08cc624bf833033dd8a6fb69cca1e7e1849e7f94741a1a0d src/ultimate/tablebases/ghost_pair_information_solver.cpp \
  d33bfaf8040f02e74bfb9f80cddc73006764934de4e15757bd2b63d8529542a8 src/ultimate/tablebases/ghost_pair_information_solver.h \
  fc7d690e06ff3bd8574034aa9e3665f66acae56819865a52668316f0a757c03f src/ultimate/tablebases/ghost_pair_information_tablebase.cpp \
  92021874e4cfbe38f0a90bbe1485be519667140187d67e683edd4fefbbfc6b2c src/ultimate/tablebases/ghost_pair_information_model.cpp \
  35e5d0b6d36344fa816e1e08ef8b6778a2e4f92d7796cd2dfbeebe343642f22d src/ultimate/tablebases/ghost_pair_information_model.h \
  1a2356b6d4694bb822c8cf059f01d83859cfeef9c4eebbff0b3cef782775cdbc src/ultimate/tablebases/ghost_information_probe.cpp \
  fcffb9f718cddc02509ee76643a4d1c4ef53db63f08200fef128c54841aacd4a src/ultimate/tablebases/ghost_information_probe.h \
  c1fd50f8bdbf556ce01f25376247d91e6d70afe228fca25a93fc8223e48eb5b0 src/ultimate/tablebases/information.cpp \
  a52cbdcf9e80b2435635f7903372304fa2a7673d1f585e8c3cfb5f57a604e98b src/ultimate/tablebases/information.h \
  a937b9e0ecf30dcdc4a87d5b11a326d840720dba474536030fc12d4d598fabf5 src/ultimate/position.cpp \
  4f5f3817e3b953a2694cf92c46ba081c348827842ff6dc07c824c957a8df3da2 src/ultimate/position.h \
  2f0119ae52e312dc21a21341063dc9dd179dbebdc24136cdf7b5cb5a37820139 src/ultimate/nnue.cpp \
  7b113d78c3537cc4b7b40d12f605e171dcb3f4b34b3a2e4bf26859971b6d46d0 src/ultimate/nnue.h)

clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  src/ultimate/tablebases/ghost_pair_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_pair_information_solver.cpp \
  src/ultimate/tablebases/ghost_pair_information_model.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp src/ultimate/position.cpp \
  src/ultimate/nnue.cpp -o "${binary}" \
  > "${work}/build.log" 2>&1

"${binary}" --self-test --scratch "${work}/self-test" \
  > "${work}/self-test.log" 2>&1
grep -F 'ghost_pair_self_test residual 0' "${work}/self-test.log"

readonly binary_sha="$(sha256sum "${binary}" | cut -d ' ' -f 1)"
readonly binary_bytes="$(stat -c %s "${binary}")"
readonly binary_key="sources/binaries/ghost-pair-domain-cache-v1/sha256/${binary_sha}/ultimate_ghost_pair_information_tablebase"
readonly binary_version="$(aws s3api put-object --region us-west-2 \
  --bucket "${bucket}" --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-bundle-sha256=79418c52ba8f1d28d179719076b8aa061a6b8c7b04729d2286231923136259de" \
  --query VersionId --output text)"
test -n "${binary_version}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${work}/s3-verify/ultimate_ghost_pair_information_tablebase"
cmp -s "${binary}" \
  "${work}/s3-verify/ultimate_ghost_pair_information_tablebase"
test "$(sha256sum "${work}/s3-verify/ultimate_ghost_pair_information_tablebase" | cut -d ' ' -f 1)" = \
  "${binary_sha}"

python3 -c 'import json,pathlib,sys; pathlib.Path(sys.argv[1]).write_text(json.dumps({"schema":"ultimate-ghost-pair-domain-cache-build-v1","source_bundle":{"sha256":"79418c52ba8f1d28d179719076b8aa061a6b8c7b04729d2286231923136259de","version_id":"R4hyfQbU3keHm3HAS95g4IayBw23smu2"},"binary":{"sha256":sys.argv[2],"bytes":int(sys.argv[3]),"key":sys.argv[4],"version_id":sys.argv[5]},"self_test_residual":0,"s3_download_residual":0},indent=2,sort_keys=True)+"\n")' \
  "${work}/build-receipt.json" "${binary_sha}" "${binary_bytes}" \
  "${binary_key}" "${binary_version}"
cat "${work}/build-receipt.json"
