#!/usr/bin/env bash
set -euo pipefail

readonly root=/mnt/ultimatefish/ghost-bomb-solve-v8/restore/ghost-bomb-current-work-v1/kbombkghost
readonly source_root=/mnt/ultimatefish/ghost-bomb-solve-v8/restore/compositional-transitions-bomb-v6-source
readonly binary=${source_root}/ultimate_ghost_bomb_compositional_v6_linux
readonly prefix=work/transitions/kbombkghost-compositional-v7
readonly scratch=work/solve-1.5b-v8/kbombkghost
readonly output=work/results/kbombkghost-1.5b-v8.ufiw
readonly arbitrary=work/results/kbombkghost-1.5b-v8.ufgd
readonly log=${root}/work/logs/solve-1.5b-v8.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  7e66fa2b0a7c42342dbf834c86053a727ff98129965dadb105110d6cd4e74967
test "$(sha256sum "${root}/tablebases/kbombkghost.uftb" | cut -d ' ' -f 1)" = \
  fc10b2a5ee22ca9ec8aecb1caa89a9de03a6431804a594cf95626f63a20eb1db
test "$(sha256sum "${root}/tablebases/kbombk.uftb" | cut -d ' ' -f 1)" = \
  18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

expected=(
  738af19174cc7e9718081cd551e2830a2d75813b29d0b26073b446a498ee808c
  3cbc909cd0a23ba63b41d4e2825ae425f1b8b512d936533689f484fc6a63bf7f
  1390d3c97a3ce71dbd00e953d95f40032b007094e804358dd386465116b83fa9
  76b54f8a6ec6625e849dd15be5e3babaab6d952e262fb40037c859db6241b675
  0d5b5428ddf678971c426af1bf0518b9c5e03c5612548ecca17920d7f7e546c9
  b564b8c0da35c68f109840293387fc79437e8bab79f481fdd416deb5fea4727e
)
suffixes=(header meta strata index blocks verified)
for index in "${!suffixes[@]}"; do
  test "$(sha256sum "${root}/${prefix}.${suffixes[index]}" | cut -d ' ' -f 1)" = \
    "${expected[index]}"
done

test -s "${root}/${scratch}.bdd-b.nodes"
test -s "${root}/${scratch}.bdd-b.unique"
test -s "${root}/${scratch}.owner-next"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
mkdir -p "${root}/work/results"

for slot in a b; do
  nodes="${root}/${scratch}.bdd-${slot}.nodes"
  unique="${root}/${scratch}.bdd-${slot}.unique"
  nodes_size="$(stat -c %s "${nodes}")"
  unique_size="$(stat -c %s "${unique}")"
  test "${nodes_size}" = 13500000000 -o "${nodes_size}" = 27000000000
  test "${unique_size}" = 8589934592 -o "${unique_size}" = 17179869184
  truncate -s 27000000000 "${nodes}"
  truncate -s 8589934592 "${unique}"
done

exec >>"${log}" 2>&1
echo "bomb_ghost_opposed_resume_v16 completed_iteration 13 current_slot next bdd_slot b workers 30 max_nodes 3000000000 unique_slots 4294967296"
cd "${root}"
common=(
  --orientation opposing
  --transition-prefix "${prefix}"
  --lower-bomb-table tablebases/kbombk.uftb
  --lower-bomb-sha256 18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f
  --lower-bomb-source-sha256 18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f
  --lower-bomb-model-sha256 1248a304457b674250861c7f8c0ee14373c8b37a6b1d65e315aa045ae58cdfa3
  --source-sha256 fc10b2a5ee22ca9ec8aecb1caa89a9de03a6431804a594cf95626f63a20eb1db
  --model-sha256 d87ecb37913bbc5c67653a9742ee3e18cc823e97ecacd58aa0472008d7739f68
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)

"${binary}" --verify-transitions "${common[@]}"
"${binary}" --solve "${common[@]}" \
  --input tablebases/kbombkghost.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 3000000000 \
  --unique-slots 4294967296 \
  --workers 30 \
  --compact-every 1 \
  --resume-fixed-point \
  --resume-iteration 13 \
  --resume-current-slot next \
  --resume-bdd-slot b

sha256sum "${output}" "${arbitrary}"
echo 'bomb_ghost_opposed_resume_v16 complete 1'
