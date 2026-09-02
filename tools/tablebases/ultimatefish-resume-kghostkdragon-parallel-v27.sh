#!/usr/bin/env bash
set -euo pipefail

readonly root=/mnt/ultimatefish-jg/opposed-ghost-dragon-memory-v4
readonly restore=${root}/restore/ghost-dragon-current-work-v1/kghostkdragon
readonly stage=/mnt/ultimatefish/ghost-parallel-v26-eaee7253
readonly source_archive=${stage}/source.tar
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-dragon-v26
readonly scratch=work/solve/kghostkdragon
readonly log=work/logs/verify-solve-parallel-v27.log

test "$(sha256sum "${source_archive}" | cut -d ' ' -f1)" = \
  eaee7253ff9313290dd7b3436f7572466932177e503efd1389c347a4a8985d84
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  109d806d73243b544c106953dfadf1a7a5ac7fcfabd4b4c748bdc57044901afb
test "$(sha256sum "${restore}/tablebases/kghostkdragon.uftb" | cut -d ' ' -f1)" = \
  f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225
test "$(sha256sum "${restore}/tablebases/kdragonk.uftb" | cut -d ' ' -f1)" = \
  28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6
test "$(sha256sum "${restore}/tablebases/kghostk.ufgm" | cut -d ' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(grep -Fc 'ghost_extra_external_compaction iteration 29 roots 42648839 marked_nodes 11189397 copied_nodes 11189395 structural_residual 0 root_residual 0' "${restore}/work/logs/verify-solve-memory-v5.log")" = 1
test ! -e "${restore}/work/results/kghostkdragon-memory-v5.ufiw"
test ! -e "${restore}/work/results/kghostkdragon-memory-v5.ufgd"
for path in "${restore}/work/transitions/kghostkdragon.verified" \
  "${restore}/${scratch}.bdd-b.nodes" \
  "${restore}/${scratch}.bdd-b.unique" \
  "${restore}/${scratch}.owner-next" \
  "${restore}/${scratch}.observer-next" \
  "${restore}/${scratch}.visible-owner-next" \
  "${restore}/${scratch}.visible-observer-next"; do
  test -s "${path}"
done

exec >>"${restore}/${log}" 2>&1
echo 'dragon_opposed_parallel_v27 resume_iteration=29 current_slot=next bdd_slot=b workers=32 checkpoint_residual=0'
cd "${restore}"
exec "${binary}" --solve --orientation opposing \
  --transition-prefix work/transitions/kghostkdragon \
  --lower-dragon-table tablebases/kdragonk.uftb \
  --lower-dragon-sha256 28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6 \
  --lower-dragon-source-sha256 28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6 \
  --lower-dragon-model-sha256 cd3bcf48109e0950bd73e27e157d249dc6c031f2d1cf135bcd4302f307911c74 \
  --source-sha256 f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225 \
  --model-sha256 c3ce5a68d38944b19a36594a6d3e1e3ad862d1c096902b350d4dadda19cf4de7 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --input tablebases/kghostkdragon.uftb \
  --normalized-input "${scratch}.normalized.uftb" \
  --normalized-sha256 96fa7bd503cfee014d52af8fd945b875f49c467972b8ff49e53cca6c0ad625ff \
  --lower-ghost-sidecar tablebases/kghostk.ufgm --scratch "${scratch}" \
  --output work/results/kghostkdragon-memory-v5.ufiw \
  --output-arbitrary work/results/kghostkdragon-memory-v5.ufgd \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 2000000000 --unique-slots 4294967296 --compact-every 1 \
  --resume-fixed-point --resume-iteration 29 \
  --resume-current-slot next --resume-bdd-slot b
