#!/usr/bin/env bash
set -euo pipefail

readonly root=/mnt/ultimatefish/info-remap-fix-v2/kcopycatkghost-fresh-v1
readonly audit=/mnt/ultimatefish/information-trivial-v3/kcopycatkghost-fresh-v6
readonly binary=/mnt/ultimatefish/information-trivial-v3/ultimate_tablebase_linux
readonly runner=/mnt/ultimatefish/information-trivial-v3/run_ultimate_information_trivial_batch_aws.py
readonly binary_sha=9e3788b73537ba879bc7b6c9a2f3444cfd521d5c18c74d6b1bd952f04d7b7a0f
readonly runner_sha=a84c09e0dab2bd2f244e0184764a5798f40c322de7c825fa94567a2b244e6265
readonly source_sha=24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50
readonly overlay_sha=306af97eee48e77692d8e3a86f9269e85c3c17fb27ae7c07e45a76870a7523af
readonly source_bundle_sha=c7903d9dfe59aed493b7e49083acdc8cf11535d10f324190f304696dc0154b2a

test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test "$(sha256sum "${runner}" | cut -d' ' -f1)" = "${runner_sha}"
test "$(sha256sum "${root}/tablebases/kcopycatkghost.uftb" | cut -d' ' -f1)" = "${source_sha}"
test "$(sha256sum "${root}/work/results/kcopycatkghost-fixed-point-fresh-v6.ufiw" | cut -d' ' -f1)" = "${overlay_sha}"

mkdir -p "${audit}/sources" "${audit}/overlays" "${audit}/receipts"
ln -sfn "${root}/tablebases/kcopycatkghost.uftb" \
  "${audit}/sources/kcopycatkghost.uftb"
ln -sfn "${root}/work/results/kcopycatkghost-fixed-point-fresh-v6.ufiw" \
  "${audit}/overlays/kcopycatkghost.ufiw"

python3 "${runner}" \
  --binary "${binary}" \
  --binary-sha256 "${binary_sha}" \
  --source-bundle-sha256 "${source_bundle_sha}" \
  --source-dir "${audit}/sources" \
  --overlay-dir "${audit}/overlays" \
  --output-dir "${audit}/receipts" \
  --bucket ultimatefish-info-20260808-a4e679c6-831688117652 \
  --shard 0 --shards 1 --workers 9

test -s "${audit}/receipts/kcopycatkghost.receipt.json"
