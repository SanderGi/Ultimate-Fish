#!/usr/bin/env bash
# Resume the authenticated v24 Penguin graph with the CLI-correct v28 binary.
set -euo pipefail

readonly root=/mnt/ultimatefish/penguin-opposed-parallel-v24-stage
readonly work=/mnt/ultimatefish/penguin-opposed-parallel-v24-work
readonly source_root=/mnt/ultimatefish/ghost-parallel-v28-2f48ca98/source
readonly tool_source_root=/mnt/ultimatefish/ghost-parallel-v23-bcfda23c/source
readonly binary=/mnt/ultimatefish/ghost-parallel-v28-2f48ca98/ultimate_ghost_ordinary_information_tablebase-penguin-v28
readonly runner=${root}/run_ultimate_ghost_ordinary_aws.py
readonly binary_sha=c74fabefbe9cdcfe5f6981b56997c263bbcd1a54442193aab9842ead1e76599f
readonly source_bundle_sha=2f48ca98b640546617aed314e7c7d4da34333dd904bafeddefbd6e6477c0c19c
readonly input_sha=06dc64a6ae06df68df65c9819de7a8abec06c124b59ea5cd5010a60769b84b64
readonly normalized_sha=1bafc268ee983ad27b68218d05baece1bdcd709873d6236f75a90b5535c7b02a
readonly model_sha=c073b92a1243ad3027b85bf539528f115b64a1b9fc9a72cc3c9dd8d2ed2cf0b9
readonly observation_sha=6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b
readonly lower_sha=5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd
readonly ghost_sidecar_sha=472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

test "$(systemctl show ultimatefish-info-kghostkpenguin-parallel-v24.service -p Result --value)" = exit-code
test "$(sha256sum "${source_root%/source}/source.tar" | cut -d' ' -f1)" = "${source_bundle_sha}"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test "$(sha256sum "${work}/tablebases/kghostkpenguin.uftb" | cut -d' ' -f1)" = "${input_sha}"
test "$(sha256sum "${work}/tablebases/kpenguink.uftb" | cut -d' ' -f1)" = "${lower_sha}"
test "$(sha256sum "${work}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = "${ghost_sidecar_sha}"
test ! -e "${work}/work/results/kghostkpenguin.ufiw"
test ! -e "${work}/work/results/kghostkpenguin.ufgd"
test "$(find "${work}/work/transitions" -maxdepth 1 -type f -name 'shard-*.verified' | wc -l)" = 64
grep -Fqx "ghost_extra_external_merge_certificate shards 64 geometries 3943680 edges 553681140 strata 584862 block_bytes 10136650560 shard_regeneration_certificates 64 gap_residual 0 offset_residual 0 byte_residual 0 conservation_residual 0" "${work}/work/logs/merge.log"
grep -Fqx "ghost_dragon_compositional_merge_certificate shards 64 payload_bound_shards 64 legacy_exhaustive_shards 0 full_replay_skipped 1 residual 0" "${work}/work/logs/merge.log"
grep -Fqx "ghost_dragon_exact_self_test codec_states 1214653440 remap_residual 0 belief_cap none" "${work}/work/logs/self-test-v28.log"
grep -Fqx "dragon_ghost_normalized_source_sha256 ghost_dragon_source_normalization states 607326720 remap_residual 0 count_residual 0 normalized_sha256 ${normalized_sha}" "${work}/work/logs/self-test-v28.log"

exec python3 "${runner}" \
  --source-root "${tool_source_root}" --work "${work}" \
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
  --parallelism 48 --solve-workers 32 --solve-existing \
  --prebuilt-executable "${binary}" \
  --prebuilt-executable-sha256 "${binary_sha}" \
  --prebuilt-model-sha256 "${model_sha}" --solve-max-nodes 1500000000
