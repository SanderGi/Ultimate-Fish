#!/bin/bash
set -euo pipefail

migration=/mnt/ultimatefish/ghost-bomb-solve-v8
archive=${migration}/kbombkghost-strong-v7.tar.zst
restore=${migration}/restore
root=${restore}/ghost-bomb-current-work-v1/kbombkghost
source_root=${restore}/compositional-transitions-bomb-v6-source
binary=${source_root}/ultimate_ghost_bomb_compositional_v6_linux
prefix=work/transitions/kbombkghost-compositional-v7
scratch=work/solve-1.5b-v8/kbombkghost
output=work/results/kbombkghost-1.5b-v8.ufiw
arbitrary=work/results/kbombkghost-1.5b-v8.ufgd
log=${root}/work/logs/solve-1.5b-v8.log
bucket=ultimatefish-info-20260808-a4e679c6-831688117652
key=results/hidden/bomb-ghost/opposed/migration-v7/sha256/9e03643ba0dec2705755ccbc850a075767b622973915222acb3248270fc4a953/kbombkghost-strong-v7.tar.zst
version=pAzUOJxjD4WTvFft7X5r.sjmKIu9WqLs

test ! -e "${migration}"
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge \
  161061273600
mkdir -p "${migration}" "${restore}"

remote=$(aws s3api head-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --version-id "${version}" \
  --query '[ContentLength,Metadata.sha256]' --output text)
test "${remote}" = 817415073$'\t'9e03643ba0dec2705755ccbc850a075767b622973915222acb3248270fc4a953
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --version-id "${version}" "${archive}" >/dev/null
test "$(stat -c %s "${archive}")" = 817415073
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = \
  9e03643ba0dec2705755ccbc850a075767b622973915222acb3248270fc4a953
tar --zstd -xf "${archive}" -C "${restore}"

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  3a142e9091f3543ebd941003ed1527308ba00b0e79756018477f078ca2a9cfa6
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  182faf8689d9ac4ab182f585d9c3264a4d4076cec8c91c93c18d7c45bbb2d5aa
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
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${log}"
mkdir -p "${root}/work/solve-1.5b-v8" "${root}/work/results"

exec >>"${log}" 2>&1
echo "bomb_ghost_opposed_solve_v8 archive_sha256 9e03643ba0dec2705755ccbc850a075767b622973915222acb3248270fc4a953 archive_version ${version} graph_payload_bound 1 replay_skipped 1 max_nodes 1500000000 unique_slots 2147483648 prior_500m_and_1b_failures_preserved 1"
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
"${binary}" \
  --solve \
  "${common[@]}" \
  --input tablebases/kbombkghost.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1500000000 \
  --unique-slots 2147483648 \
  --compact-every 1

sha256sum "${output}" "${arbitrary}"
echo 'bomb_ghost_opposed_solve_v8 complete 1'
