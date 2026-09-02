#!/usr/bin/env bash
# Reverify the retained opposed Ghost/Penguin iteration-82 fixed point under
# the sound singleton information-dominance contract.  The old verifier's
# concrete-WDL equality assertion is invalid once future private Ghost actions
# can expand a currently singleton belief.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=8320df74822d873ded883d806b6e10a126646bbd1335f6eb2364d6488b63b0f4
readonly source_version=7e.5ydbHxYLgdIfLshhjQIlZppxvjqqC
readonly source_key="sources/bundles/ghost-singleton-dominance-v3/sha256/${source_sha}/ultimatefish-ghost-singleton-dominance-v3-source.tar"
readonly source_root=/mnt/ultimatefish/ghost-singleton-dominance-v3-8320df74-penguin
readonly root=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5
readonly binary="${source_root}/ultimate_ghost_penguin_singleton_dominance_v3_linux"
readonly restored="${binary}.restored"
readonly old_model_sha=268c2bdf5fb9e1b436406378e97ee213eb460c70b354bbeececd479ef8df33e8
readonly model_sha=c62fb45f7b5e732714b38b883ac30906d0bc0f9ac814e1646c339472c8714959
readonly observation_sha=6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b
readonly old_prefix="${root}/work/transitions/kghostkpenguin"
readonly new_prefix="${root}/work/transitions/kghostkpenguin-singleton-dominance-v3"
readonly log="${root}/work/logs/singleton-dominance-v3.log"
readonly tool_sha=15f5f6228a04a08af3639775b17dd74b30fb7fa856e721d0e0ae8f9d7b9155f0
readonly tool_version=2rsdltkem9gwQxUpcejDVLmwUoW.sVYx
readonly tool="${source_root}/rebind_ultimate_ghost_ordinary_transition_marker.py"

test "$(systemctl show ultimatefish-info-kghostkpenguin-resume-after-stop-v2.service -p ActiveState --value)" != active
grep -Fq 'reciprocal_ghost_extra_iteration 82 bdd_nodes 232995187 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${root}/work/logs/resume-after-stop-v1.log"
grep -Fq 'ghost_extra_singleton_domain checked 88462144 excluded_adjacent_kings 8316320 residual 12793334' \
  "${root}/work/logs/resume-after-stop-v1.log"
test "$(stat -c %s "${root}/work/solve/kghostkpenguin.bdd-b.nodes")" = 4500000000
test "$(sha256sum "${root}/tablebases/kghostkpenguin.uftb" | cut -d' ' -f1)" = \
  d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a
test "$(sha256sum "${root}/tablebases/kpenguink.uftb" | cut -d' ' -f1)" = \
  5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "${source_root}"
test ! -e "${log}"
test ! -e "${root}/work/results/kghostkpenguin.ufiw"
test ! -e "${root}/work/results/kghostkpenguin.ufgd"
for suffix in header meta strata index blocks verified; do
  test ! -e "${new_prefix}.${suffix}"
done

install -d -m 0755 "${source_root}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${source_key}" --version-id "${source_version}" \
  "${source_root}/source.tar" >"${source_root}/source-get.json"
test "$(sha256sum "${source_root}/source.tar" | cut -d' ' -f1)" = "${source_sha}"
tar -xf "${source_root}/source.tar" -C "${source_root}"

cd "${source_root}"
taskset -c 2 clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Penguin \
  -DULTIMATE_GHOST_EXTRA_SUBSTATES=8 \
  -DULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES=4 \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/position.cpp src/ultimate/nnue.cpp -o "${binary}"
install -d -m 0755 "${source_root}/self-test"
taskset -c 2 "${binary}" --self-test --orientation opposing \
  --scratch "${source_root}/self-test/kghostkpenguin" \
  --input "${root}/tablebases/kghostkpenguin.uftb" \
  --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
  >"${source_root}/self-test.log" 2>&1
grep -Fq 'ghost_dragon_exact_self_test codec_states 1214653440 remap_residual 0 belief_cap none' \
  "${source_root}/self-test.log"

readonly binary_sha="$(sha256sum "${binary}" | cut -d' ' -f1)"
readonly binary_key="sources/binaries/ghost-singleton-dominance-v3/sha256/${binary_sha}/ultimate_ghost_penguin_singleton_dominance_v3_linux"
aws s3api put-object --region us-west-2 --bucket "${bucket}" --key "${binary_key}" \
  --body "${binary}" \
  --metadata "sha256=${binary_sha},source-sha256=${source_sha},model-sha256=${model_sha}" \
  >"${source_root}/binary-put.json"
readonly binary_version="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' "${source_root}/binary-put.json")"
aws s3api get-object --region us-west-2 --bucket "${bucket}" --key "${binary_key}" \
  --version-id "${binary_version}" "${restored}" >"${source_root}/binary-get.json"
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${binary_sha}"
cmp "${binary}" "${restored}"

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "sources/tools/ghost-ordinary-transition-marker-rebind-v1/sha256/${tool_sha}/rebind_ultimate_ghost_ordinary_transition_marker.py" \
  --version-id "${tool_version}" "${tool}" >"${source_root}/rebind-tool-get.json"
test "$(sha256sum "${tool}" | cut -d' ' -f1)" = "${tool_sha}"
chmod 0755 "${tool}"
for suffix in header meta strata index blocks; do
  cp --reflink=always "${old_prefix}.${suffix}" "${new_prefix}.${suffix}"
done
python3 "${tool}" "${old_prefix}" "${new_prefix}" \
  --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
  --old-model-sha256 "${old_model_sha}" --new-model-sha256 "${model_sha}" \
  --observation-sha256 "${observation_sha}" \
  --manifest "${root}/work/penguin-singleton-dominance-v3-transition-rebind.json" \
  >"${source_root}/rebind.log"

{
  echo "penguin_ghost_singleton_dominance_v3 source_sha256 ${source_sha} source_version ${source_version} binary_sha256 ${binary_sha} binary_version ${binary_version} model_sha256 ${model_sha} iteration 82 current_slot current bdd_slot b checkpoint_preserved 1 transition_payload_changed 0"
  cd "${root}"
  taskset -c 2 "${binary}" --solve \
    --orientation opposing \
    --transition-prefix work/transitions/kghostkpenguin-singleton-dominance-v3 \
    --lower-dragon-table tablebases/kpenguink.uftb \
    --lower-dragon-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-source-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
    --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
    --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
    --input tablebases/kghostkpenguin.uftb \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch work/solve/kghostkpenguin \
    --output work/results/kghostkpenguin.ufiw \
    --output-arbitrary work/results/kghostkpenguin.ufgd \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
    --resume-fixed-point --resume-converged --resume-iteration 82 \
    --resume-current-slot current --resume-bdd-slot b
  sha256sum work/results/kghostkpenguin.ufiw work/results/kghostkpenguin.ufgd
  echo 'penguin_ghost_singleton_dominance_v3 complete 1'
} >>"${log}" 2>&1

python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename kghostkpenguin.uftb --piece penguin --orientation opposing \
  --source-table "${root}/tablebases/kghostkpenguin.uftb" \
  --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
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
