#!/usr/bin/env bash
# Fresh, source-authenticated parallel opposed Ghost/Penguin certification.
# The retained serial v10 run remains untouched until this independent run
# completes every proof gate.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/penguin-opposed-parallel-v24-stage
readonly work=/mnt/ultimatefish/penguin-opposed-parallel-v24-work
readonly source_root=/mnt/ultimatefish/ghost-parallel-v23-bcfda23c/source
readonly binary=/mnt/ultimatefish/ghost-parallel-v23-bcfda23c/ultimate_ghost_ordinary_information_tablebase-penguin-v23
readonly binary_sha=9b32ba161f6e80c9b7410cd119e7e103c8f70fddd3d5aaa657418953032d3c7f
readonly runner=${root}/run_ultimate_ghost_ordinary_aws.py
readonly runner_sha=4d1356627059d85aa09471218ebb6764f43b71a71f8e55627aa38f7fb5e09194
readonly inventory=${source_root}/tools/tablebases/ultimate_information_tablebases.py
readonly inventory_sha=d6611e23a0f993f085fb72e1456e782bde3e063b3bcc11806b77cea37357ec1a
readonly shard_runner=${source_root}/tools/tablebases/run_ultimate_reciprocal_bishop_ghost_aws.py
readonly shard_runner_sha=0453176f367d5e6d9cc69b1fdd83da7cb69e461ce9254775cb7b3f6359ec34c6
readonly input_sha=06dc64a6ae06df68df65c9819de7a8abec06c124b59ea5cd5010a60769b84b64
readonly model_sha=c073b92a1243ad3027b85bf539528f115b64a1b9fc9a72cc3c9dd8d2ed2cf0b9
readonly observation_sha=6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b
readonly lower_sha=5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd
readonly ghost_sidecar_sha=472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

install -d -m 0755 "${root}/tablebases" "${source_root}/tools/tablebases"
if [[ ! -f "${root}/tablebases/kghostkpenguin.uftb" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "results/concrete/penguin-terminal-fix-v1/sha256/${input_sha}/kghostkpenguin.uftb" \
    --version-id LPA.Ol.SVqn5RvxaS.qA5DmoLQmyzxxn \
    "${root}/tablebases/kghostkpenguin.uftb" >/dev/null
fi
if [[ ! -f "${root}/tablebases/kpenguink.uftb" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "sources/dependencies/concrete-penguin-current-v1/kpenguink.uftb/sha256/${lower_sha}/kpenguink.uftb" \
    --version-id Ml41qMXWL_AkaixSDlGAfJ6Fv23cXgUh \
    "${root}/tablebases/kpenguink.uftb" >/dev/null
fi
if [[ ! -f "${runner}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "sources/tools/run-ultimate-ghost-ordinary-vcurrent/sha256/${runner_sha}/run_ultimate_ghost_ordinary_aws.py" \
    --version-id 0dL9C7w4.YxXy.3RTE.2yLeR.sSJcCK4 "${runner}" >/dev/null
fi
if [[ ! -f "${inventory}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "sources/tools/penguin-opposed-parallel-v24/sha256/${inventory_sha}/ultimate_information_tablebases.py" \
    --version-id ES8rDfdmDjA5LlDb2IKB.OAN.kl8dYpA "${inventory}" >/dev/null
fi
if [[ ! -f "${shard_runner}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "sources/tools/penguin-opposed-parallel-v24/sha256/${shard_runner_sha}/run_ultimate_reciprocal_bishop_ghost_aws.py" \
    --version-id OqRNCu5wZnP5GvbbkAfh0AHdSU3YFjJs "${shard_runner}" >/dev/null
fi
if [[ ! -f "${root}/tablebases/kghostk.ufgm" ]]; then
  cp --reflink=always \
    /mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-solve-2b-v7/tablebases/kghostk.ufgm \
    "${root}/tablebases/kghostk.ufgm"
fi

test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test "$(sha256sum "${runner}" | cut -d' ' -f1)" = "${runner_sha}"
test "$(sha256sum "${inventory}" | cut -d' ' -f1)" = "${inventory_sha}"
test "$(sha256sum "${shard_runner}" | cut -d' ' -f1)" = "${shard_runner_sha}"
test "$(sha256sum "${root}/tablebases/kghostkpenguin.uftb" | cut -d' ' -f1)" = "${input_sha}"
test "$(sha256sum "${root}/tablebases/kpenguink.uftb" | cut -d' ' -f1)" = "${lower_sha}"
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = "${ghost_sidecar_sha}"

exec python3 "${runner}" \
  --source-root "${source_root}" --work "${work}" \
  --filename kghostkpenguin.uftb --piece penguin --orientation opposing \
  --source-table "${root}/tablebases/kghostkpenguin.uftb" \
  --source-sha256 "${input_sha}" \
  --lower-table "${root}/tablebases/kpenguink.uftb" \
  --lower-sha256 "${lower_sha}" \
  --lower-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 "${ghost_sidecar_sha}" \
  --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 48 --solve-workers 32 \
  --prebuilt-executable "${binary}" \
  --prebuilt-executable-sha256 "${binary_sha}" \
  --prebuilt-model-sha256 "${model_sha}" --solve-max-nodes 1500000000
