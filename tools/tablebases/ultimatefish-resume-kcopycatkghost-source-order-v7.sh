#!/bin/bash
# Resume the authenticated opposed Copycat/Ghost fixed point from the last
# completed and compacted iteration after the EC2 checkpoint migration.
set -euo pipefail

root=/mnt/ultimatefish/info-remap-fix-v2/kcopycatkghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v6-copycat-a26d3665
binary=${source_root}/ultimate_ghost_copycat_source_order_v6_linux
prefix=work/transitions/kcopycatkghost-source-order-v6
scratch=work/solve-source-order-v6/kcopycatkghost
log=${root}/work/logs/solve.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  e2889ac280f680f5bb3d33b190320fc13b55a7f9f90dc062e709181472827a3f
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  3694b19e2c04e02385c96ce4ff7d2f7c50d0d9bac048dc2cf512b2c04e9e3e0d
grep -Fqx \
  'reciprocal_ghost_extra_iteration 46 bdd_nodes 189421893 changed_owner 434 changed_observer 0 changed_visible 438 peak_rss_bytes 7385681920' \
  "${log}"
grep -Fqx \
  'ghost_extra_external_compaction iteration 46 roots 42636359 marked_nodes 5429135 copied_nodes 5429133 structural_residual 0 root_residual 0' \
  "${log}"
! grep -q '^reciprocal_ghost_extra_iteration 47 ' "${log}"
test ! -e "${root}/work/results/kcopycatkghost-source-order-v6.ufiw"
test ! -e "${root}/work/results/kcopycatkghost-source-order-v6.ufgd"

exec >>"${log}" 2>&1
echo 'copycat_ghost_opposed_source_order_resume_v7 resume_iteration 46 current_slot current bdd_slot a checkpoint_migration 1'
cd "${root}"
"${binary}" --solve \
  --transition-prefix "${prefix}" \
  --orientation opposing \
  --input tablebases/kcopycatkghost.uftb \
  --normalized-input work/self-test-source-order-v6/kcopycatkghost.normalized.uftb \
  --normalized-sha256 736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output work/results/kcopycatkghost-source-order-v6.ufiw \
  --output-arbitrary work/results/kcopycatkghost-source-order-v6.ufgd \
  --lower-dragon-table tablebases/kcopycatk.uftb \
  --lower-dragon-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
  --lower-dragon-source-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
  --lower-dragon-model-sha256 d94883c4fd100918a23947c3c71d5e461588778aefbbe037f260d48d5083095f \
  --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
  --model-sha256 0f9a93e320f77de4d390b726ef4a34ba9c456e419c33cdb26e098d20280b3f8f \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 46 \
  --resume-current-slot current --resume-bdd-slot a
