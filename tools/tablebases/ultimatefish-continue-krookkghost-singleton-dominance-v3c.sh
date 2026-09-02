#!/usr/bin/env bash
# Continue the authenticated v3 Rook verification after the first wrapper
# selected the undersized dormant BDD-a file.  The converged iteration-23 roots
# are in the current root slot and retained BDD-b, as proved by the v2 resume
# log and BDD capacity/node-count checks.
set -euo pipefail

readonly root=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1
readonly source_root=/mnt/ultimatefish/ghost-singleton-dominance-v3-8320df74-rook
readonly binary="${source_root}/ultimate_ghost_rook_singleton_dominance_v3_linux"
readonly model_sha=5deb367a58b100f8da7e318910d956b03dd5ef383782884718c0a30341073618
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly log="${root}/work/logs/singleton-dominance-v3c.log"
readonly output=work/results/krookkghost-singleton-dominance-v3.ufiw
readonly arbitrary=work/results/krookkghost-singleton-dominance-v3.ufgd

test "$(sha256sum "${source_root}/source.tar" | cut -d' ' -f1)" = \
  8320df74822d873ded883d806b6e10a126646bbd1335f6eb2364d6488b63b0f4
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = \
  e376dbd06f6e80940fed711332d5d0eeb5c0befcd29ca10fe5ec101677f5fb2d
grep -Fq 'reciprocal_ghost_extra_iteration 23 bdd_nodes 910532134 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${root}/work/logs/action-conditioned-v2.log"
grep -Fq 'cannot verify size of work/solve-action-conditioned-v1/krookkghost.bdd-a.nodes' \
  "${root}/work/logs/singleton-dominance-v3.log"
test "$(stat -c %s "${root}/work/solve-action-conditioned-v1/krookkghost.bdd-b.nodes")" = 11250000000
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"

{
  echo 'rook_ghost_singleton_dominance_v3c iteration 23 current_slot current bdd_slot b checkpoint_preserved 1'
  cd "${root}"
  taskset -c 4 "${binary}" --solve \
    --transition-prefix work/transitions/krookkghost-singleton-dominance-v3 \
    --orientation opposing --input tablebases/krookkghost.uftb \
    --normalized-input work/self-test-source-order-v6/krookkghost.normalized.uftb \
    --normalized-sha256 790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785 \
    --lower-dragon-table tablebases/krookk.uftb \
    --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
    --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
    --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch work/solve-action-conditioned-v1/krookkghost \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 "${observation_sha}" \
    --max-nodes 1250000000 --unique-slots 1073741824 --compact-every 1 \
    --resume-fixed-point --resume-converged --resume-iteration 23 \
    --resume-current-slot current --resume-bdd-slot b
  sha256sum "${output}" "${arbitrary}"
  echo 'rook_ghost_singleton_dominance_v3c complete 1'
} >>"${log}" 2>&1

python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename krookkghost.uftb --piece rook --orientation opposing \
  --source-table "${root}/tablebases/krookkghost.uftb" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --lower-table "${root}/tablebases/krookk.uftb" \
  --lower-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
  --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 "${observation_sha}" --finalize-existing
