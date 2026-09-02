#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=44ed7ae2896d4149ffae401b7ebc41ac955b0c20a0bfbe25f6a7d0f05c4fd9b0
readonly source_version=i1reqLi9pZmc2bWdB1dMOtuSV4BtrcM0
readonly source_key="sources/bundles/pawn-parallel-v25/sha256/${source_sha}/ultimatefish-pawn-parallel-v25-source.tar"
readonly binary_sha=254451322d7894657c0ea56f620ba7f10de8bdccf01edddb5c0899b142de6e1f
readonly binary_version=7Qz8YaZvgZQLmz7Z7UhvCXtQJfrl0M9p
readonly binary_key="sources/binaries/ghost-parallel-v25/pawn/sha256/${binary_sha}/ultimate_ghost_ordinary_information_tablebase-pawn-v25"
readonly stage=/mnt/ultimatefish/ghost-parallel-v25-${source_sha:0:8}
readonly source_archive=${stage}/source.tar
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-pawn-v25
readonly root=/mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-fresh-v12
readonly scratch=work/solve/kpawnghostk
readonly log=work/logs/solve-v17.log

test "$(sha256sum "${source_archive}" | cut -d' ' -f1)" = "${source_sha}"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test "$(sha256sum "${root}/tablebases/kpawnghostk.uftb" | cut -d' ' -f1)" = \
  4f6a0f7300fd856518c7701fb49eea53298382fcd35c0fffca7a575113a669e7
test "$(sha256sum "${root}/${scratch}.normalized.uftb" | cut -d' ' -f1)" = \
  74e4841be19e8eb72c847a49740e7398a6f28479efc09b3a36a3bb97e6407a2a
test "$(sha256sum "${root}/tablebases/kpawnk.uftb" | cut -d' ' -f1)" = \
  42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
test "$(sha256sum "${root}/tablebases/kqueenk.uftb" | cut -d' ' -f1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/tablebases/promoted-queen-ghost.ufgd" | cut -d' ' -f1)" = \
  09dbca339054c1c9e425d992b3a7de21c42a70824e2f04ae3ccef88f5718067b
test "$(grep -Fc 'ghost_extra_external_compaction iteration 21 roots 172560447 marked_nodes 14716186 copied_nodes 14716184 structural_residual 0 root_residual 0' "${root}/work/logs/solve-v15.log")" = 1
test ! -e "${root}/work/results/kpawnghostk.ufiw"
test ! -e "${root}/work/results/kpawnghostk.ufgd"
for path in "${root}/work/transitions/kpawnghostk.verified" \
  "${root}/${scratch}.bdd-b.nodes" "${root}/${scratch}.bdd-b.unique" \
  "${root}/${scratch}.owner-next" "${root}/${scratch}.observer-next" \
  "${root}/${scratch}.visible-owner-next" \
  "${root}/${scratch}.visible-observer-next"; do
  test -s "${path}"
done

exec >>"${root}/${log}" 2>&1
echo 'pawn_ghost_parallel_v17 resume_iteration=21 current_slot=next bdd_slot=b workers=32 exact_checkpoint_residual=0'
cd "${root}"
exec "${binary}" --solve --orientation same \
  --transition-prefix work/transitions/kpawnghostk \
  --input tablebases/kpawnghostk.uftb \
  --normalized-input "${scratch}.normalized.uftb" \
  --normalized-sha256 74e4841be19e8eb72c847a49740e7398a6f28479efc09b3a36a3bb97e6407a2a \
  --lower-ghost-sidecar tablebases/kghostk.ufgm --scratch "${scratch}" \
  --output work/results/kpawnghostk.ufiw \
  --output-arbitrary work/results/kpawnghostk.ufgd \
  --lower-dragon-table tablebases/kpawnk.uftb \
  --lower-dragon-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844 \
  --lower-dragon-source-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844 \
  --lower-dragon-model-sha256 6f84793f4343b5855fc0ddadd7a4452b9788c67070fa03b1f81bffb59ca4e011 \
  --promoted-lower-dragon-table tablebases/kqueenk.uftb \
  --promoted-lower-dragon-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --promoted-lower-dragon-source-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --promoted-lower-dragon-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --source-sha256 4f6a0f7300fd856518c7701fb49eea53298382fcd35c0fffca7a575113a669e7 \
  --model-sha256 c550edb3f797695790872535c007a013374b6191394532ae3d6f6f3206935ded \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --promoted-sidecar tablebases/promoted-queen-ghost.ufgd \
  --promoted-sidecar-sha256 09dbca339054c1c9e425d992b3a7de21c42a70824e2f04ae3ccef88f5718067b \
  --promoted-source-sha256 7b0ec34fc2f00524b0bf731f46deb7182c2c50fb17d1188dfa76f2238a697bb0 \
  --promoted-model-sha256 c7be59127e706959d44b8441ec308de3b1389b0405a595730c4daa3c584aef65 \
  --promoted-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --promoted-lower-ghost-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 21 \
  --resume-current-slot next --resume-bdd-slot b
