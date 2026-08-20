#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostprincek
source_root=/mnt/ultimatefish/penguin-source-order-v6
binary=${source_root}/ultimate_ghost_prince_source_order_v6_linux
runner=${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
old_prefix=work/transitions/kghostprincek
new_prefix=work/transitions/kghostprincek-source-order-v3
prior_journal=${root}/work/logs/prior-memory-v2-journal.log
log=${root}/work/logs/source-order-resume-v3.log
normalized=work/self-test-source-order-v3/kghostprincek.normalized.uftb
scratch=work/solve-source-order-v3/kghostprincek
output=work/results/kghostprincek-source-order-v3.ufiw
arbitrary=work/results/kghostprincek-source-order-v3.ufgd

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  87e359ff191fc2c187a599b1d3431596e83fab7940122af07629c1fb6f305070
test "$(sha256sum "${root}/tablebases/kghostprincek.uftb" | cut -d ' ' -f 1)" = \
  bd77080a64813a7ba4ef9bfeedde322409295944012417e2a7376689cf1cf9b8
test "$(sha256sum "${root}/tablebases/kprincek.uftb" | cut -d ' ' -f 1)" = \
  7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  b59a4155f64c5f950f014d41bdaf43f5390bf0485f75917fb35e26d0519369ac
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  2add2467b863b53c86fc9329b59fae950f176bb4d9f96cfe6e8b3a210b4ce07f
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${root}/${new_prefix}.header"
test ! -e "${prior_journal}"
test ! -e "${log}"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 107374182400

journalctl -u ultimatefish-info-solve-current-kghostprincek-memory-v2.service \
  --no-pager >"${prior_journal}"
grep -Fq \
  'Dragon/Ghost error: exact external Ghost-extra solve exceeds the physical-memory gate' \
  "${prior_journal}"
grep -Fq 'ghost_extra_external_reload 985920/985920' "${prior_journal}"

mkdir -p "${root}/work/self-test-source-order-v3" \
  "${root}/work/solve-source-order-v3" "${root}/work/results"

common=(
  --orientation same
  --lower-dragon-table tablebases/kprincek.uftb
  --lower-dragon-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0
  --lower-dragon-source-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0
  --lower-dragon-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927
  --source-sha256 bd77080a64813a7ba4ef9bfeedde322409295944012417e2a7376689cf1cf9b8
  --model-sha256 3fc5612e277bfe1469999746d5c97a573252dd0785237a3cf3b383093c496086
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

{
  echo "prince_ghost_same_source_order_v3 source_bundle_sha256 a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 source_bundle_version WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3 binary_sha256 87e359ff191fc2c187a599b1d3431596e83fab7940122af07629c1fb6f305070 binary_version 92SwOXklHO3rDr1NfNAYJZFKYgxuiM9R exact_shards 64 old_graph_preserved ${old_prefix} failed_replay_preserved ${prior_journal} corrected_normalized_sha256 a3e5b055d2bf4153caadfcb21b6582fdd59a3f4c55651368a20d43533888c6f8 max_nodes 500000000 unique_slots 1073741824"
  sha256sum "${prior_journal}"
  cd "${root}"
  "${binary}" \
    --self-test \
    --orientation same \
    --scratch work/self-test-source-order-v3/kghostprincek \
    --input tablebases/kghostprincek.uftb \
    --source-sha256 bd77080a64813a7ba4ef9bfeedde322409295944012417e2a7376689cf1cf9b8
  test "$(sha256sum "${normalized}" | cut -d ' ' -f 1)" = \
    a3e5b055d2bf4153caadfcb21b6582fdd59a3f4c55651368a20d43533888c6f8

  "${binary}" \
    --merge-transitions \
    --transition-prefix "${new_prefix}" \
    "${shards[@]}" \
    --expected-geometries 985920 \
    "${common[@]}"

  "${binary}" \
    --solve \
    --transition-prefix "${new_prefix}" \
    "${common[@]}" \
    --input tablebases/kghostprincek.uftb \
    --normalized-input "${normalized}" \
    --normalized-sha256 a3e5b055d2bf4153caadfcb21b6582fdd59a3f4c55651368a20d43533888c6f8 \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch "${scratch}" \
    --output "${output}" \
    --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --max-nodes 500000000 \
    --unique-slots 1073741824 \
    --compact-every 1

  sha256sum "${output}" "${arbitrary}"
  echo 'prince_ghost_same_source_order_v3 complete 1'
} >>"${log}" 2>&1

/usr/bin/python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${root}" \
  --filename kghostprincek.uftb \
  --piece prince \
  --orientation same \
  --source-table "${root}/tablebases/kghostprincek.uftb" \
  --source-sha256 bd77080a64813a7ba4ef9bfeedde322409295944012417e2a7376689cf1cf9b8 \
  --lower-table "${root}/tablebases/kprincek.uftb" \
  --lower-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927 \
  --model-sha256 3fc5612e277bfe1469999746d5c97a573252dd0785237a3cf3b383093c496086 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
