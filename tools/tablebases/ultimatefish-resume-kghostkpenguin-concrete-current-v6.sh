#!/usr/bin/env bash
# Re-solve the retained opposed Ghost/Penguin information graph against the
# current-rule concrete WDL oracle.  The immutable legal-move edge payload is
# rebound only in its source provenance field; no stale information fixed
# point is reused because the corrected exact leaves can change Bellman roots.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5
readonly source_root=/mnt/ultimatefish/ghost-penguin-color-swap-v4-cb96a414
readonly binary="${source_root}/ultimate_ghost_penguin_color_swap_v4_linux"
readonly binary_sha=e94ee29ea1fa1821393e253571357cd785166c20f780695d842c81de77ba1ee1
readonly model_sha=6c78476d6c51a9b5ac990759c6e9c9ee42d25d599e126363340654c00996735d
readonly observation_sha=6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b
readonly old_source_sha=d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a
readonly source_sha=4e968ac9fa26e187a53071d9df99df160c0923de19238341ca803e69662b7719
readonly archive_sha=ba5a4c073b3cc0d6a4be7f329343cf0b2fcc640ccc2ba51655b98b7bc1cdcd9b
readonly archive_version=C8c2kAzzlNynobw8hpCgw5PoorQCAtsu
readonly archive_key="results/concrete/penguin-current-v1/concrete/v2/model/b059287e45d662d3c600117af10f5be0c6858bd01fb0c0eff7470390f22b6bc5/wave-0/sha256/${archive_sha}/kghostkpenguin-${archive_sha}.tar.zst"
readonly stage="${root}/work/concrete-current-v1"
readonly archive="${stage}/kghostkpenguin-${archive_sha}.tar.zst"
readonly restored="${stage}/restore/tablebases/kghostkpenguin.uftb"
readonly old_prefix="${root}/work/transitions/kghostkpenguin-color-swap-v4"
readonly new_prefix="${root}/work/transitions/kghostkpenguin-concrete-current-v1"
readonly relative_prefix=work/transitions/kghostkpenguin-concrete-current-v1
readonly scratch=work/solve-concrete-current-v1/kghostkpenguin
readonly output=work/results/kghostkpenguin.ufiw
readonly arbitrary=work/results/kghostkpenguin.ufgd
readonly log="${root}/work/logs/concrete-current-v6.log"
readonly rebind_manifest="${root}/work/penguin-concrete-current-v1-transition-rebind.json"
readonly tool="${source_root}/rebind_ultimate_ghost_ordinary_transition_marker_v2.py"
readonly tool_sha=598e8a1f8bf21dbdc38e7be8825ad74ffaaa8573c52ab9ad91cb9f228e942101
readonly tool_version=lHmA9IygXkNIZrRJfa6fjoMD7OE2pSPI

grep -Fq 'information_differences 1056410 dominance_residual 956684' \
  "${root}/work/logs/fixed-point-v5.log"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test ! -e "${stage}"
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${rebind_manifest}"
for suffix in header meta strata index blocks verified; do
  test ! -e "${new_prefix}.${suffix}"
done

install -d -m 0755 "${stage}/restore"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${archive_key}" --version-id "${archive_version}" "${archive}" \
  >"${stage}/archive-get.json"
test "$(sha256sum "${archive}" | cut -d' ' -f1)" = "${archive_sha}"
test "$(tar --zstd -tf "${archive}" | grep -Fxc 'tablebases/kghostkpenguin.uftb')" = 1
tar --zstd -xf "${archive}" -C "${stage}/restore" \
  tablebases/kghostkpenguin.uftb
test "$(stat -c %s "${restored}")" = 759158448
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${source_sha}"

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "sources/tools/ghost-ordinary-transition-marker-rebind-v2/sha256/${tool_sha}/rebind_ultimate_ghost_ordinary_transition_marker.py" \
  --version-id "${tool_version}" "${tool}" >"${stage}/rebind-tool-get.json"
test "$(sha256sum "${tool}" | cut -d' ' -f1)" = "${tool_sha}"
chmod 0755 "${tool}"

# The five edge components are immutable and authenticated by the old marker.
# Reflink them, then change only the concrete-source provenance field.
for suffix in header meta strata index blocks; do
  cp --reflink=always "${old_prefix}.${suffix}" "${new_prefix}.${suffix}"
done
python3 "${tool}" "${old_prefix}" "${new_prefix}" \
  --source-sha256 "${old_source_sha}" --new-source-sha256 "${source_sha}" \
  --old-model-sha256 "${model_sha}" --new-model-sha256 "${model_sha}" \
  --observation-sha256 "${observation_sha}" \
  --manifest "${rebind_manifest}" >"${stage}/rebind.log"

taskset -c 2 "${binary}" --self-test --orientation opposing \
  --scratch "${stage}/self-test/kghostkpenguin" \
  --input "${restored}" --source-sha256 "${source_sha}" \
  >"${stage}/self-test.log" 2>&1
grep -Fq 'remap_residual 0' "${stage}/self-test.log"

{
  echo "penguin_ghost_concrete_current_v6 binary_sha256 ${binary_sha} model_sha256 ${model_sha} old_source_sha256 ${old_source_sha} source_sha256 ${source_sha} archive_sha256 ${archive_sha} archive_version ${archive_version} transition_payload_changed 0 fixed_point_reused 0"
  cd "${root}"
  taskset -c 2 "${binary}" --solve \
    --orientation opposing --transition-prefix "${relative_prefix}" \
    --lower-dragon-table tablebases/kpenguink.uftb \
    --lower-dragon-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-source-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
    --source-sha256 "${source_sha}" --model-sha256 "${model_sha}" \
    --observation-sha256 "${observation_sha}" --input "${restored}" \
    --lower-ghost-sidecar tablebases/kghostk.ufgm --scratch "${scratch}" \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1
  sha256sum "${output}" "${arbitrary}"
  echo 'penguin_ghost_concrete_current_v6 complete 1'
} >>"${log}" 2>&1

# Authenticate the new result inventory; preservation/certification remains a
# separate fail-closed stage and will not infer success from this unit alone.
python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename kghostkpenguin.uftb --piece penguin --orientation opposing \
  --source-table "${restored}" --source-sha256 "${source_sha}" \
  --lower-table "${root}/tablebases/kpenguink.uftb" \
  --lower-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
  --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing
