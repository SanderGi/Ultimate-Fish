#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/ghost-bomb-current-work-v1/kbombkghost
source_root=/mnt/ultimatefish/compositional-transitions-bomb-v6-source
prefix=${root}/work/transitions/kbombkghost-compositional-v7
migration=/mnt/ultimatefish/ghost-bomb-migration-v7
archive=${migration}/kbombkghost-strong-v7.tar.zst
receipt=${migration}/upload-receipt.json
log=${migration}/archive.log
bucket=ultimatefish-info-20260808-a4e679c6-831688117652

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  3a142e9091f3543ebd941003ed1527308ba00b0e79756018477f078ca2a9cfa6
test "$(sha256sum "${source_root}/ultimate_ghost_bomb_compositional_v6_linux" | cut -d ' ' -f 1)" = \
  182faf8689d9ac4ab182f585d9c3264a4d4076cec8c91c93c18d7c45bbb2d5aa
test "$(sha256sum "${root}/work/logs/compositional-merge-v7.log" | cut -d ' ' -f 1)" = \
  95d4b08a144d45ab5ed565281ccf369d9082705e29e4647ad0a68744458adf60
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
  test "$(sha256sum "${prefix}.${suffixes[index]}" | cut -d ' ' -f 1)" = \
    "${expected[index]}"
done
test ! -e "${migration}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 21474836480

mkdir -p "${migration}"
exec >>"${log}" 2>&1
echo 'bomb_ghost_opposed_strong_archive_v7 original_exact_tree_preserved 1 graph_payload_bound 1 replay_skipped 1'

cd /mnt/ultimatefish
files=(
  ghost-bomb-current-work-v1/kbombkghost/tablebases/kbombkghost.uftb
  ghost-bomb-current-work-v1/kbombkghost/tablebases/kbombk.uftb
  ghost-bomb-current-work-v1/kbombkghost/tablebases/kghostk.ufgm
  ghost-bomb-current-work-v1/kbombkghost/work/self-test/kbombkghost.normalized.uftb
  ghost-bomb-current-work-v1/kbombkghost/work/logs
  ghost-bomb-current-work-v1/kbombkghost/work/transitions/kbombkghost-compositional-v7.header
  ghost-bomb-current-work-v1/kbombkghost/work/transitions/kbombkghost-compositional-v7.meta
  ghost-bomb-current-work-v1/kbombkghost/work/transitions/kbombkghost-compositional-v7.strata
  ghost-bomb-current-work-v1/kbombkghost/work/transitions/kbombkghost-compositional-v7.index
  ghost-bomb-current-work-v1/kbombkghost/work/transitions/kbombkghost-compositional-v7.blocks
  ghost-bomb-current-work-v1/kbombkghost/work/transitions/kbombkghost-compositional-v7.verified
  compositional-transitions-bomb-v6-source/source.tar.gz
  compositional-transitions-bomb-v6-source/ultimate_ghost_bomb_compositional_v6_linux
  compositional-transitions-bomb-v6-source/ultimatefish-merge-kbombkghost-compositional-v7.sh
)
tar --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
  -cf - "${files[@]}" | zstd -T1 -3 --no-progress -o "${archive}"

archive_sha=$(sha256sum "${archive}" | cut -d ' ' -f 1)
archive_size=$(stat -c %s "${archive}")
key=results/hidden/bomb-ghost/opposed/migration-v7/sha256/${archive_sha}/kbombkghost-strong-v7.tar.zst
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --body "${archive}" --metadata "sha256=${archive_sha}" \
  >"${receipt}.tmp"
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' \
  "${receipt}.tmp")
remote=$(aws s3api head-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --version-id "${version}" \
  --query '[ContentLength,Metadata.sha256]' --output text)
test "${remote}" = "${archive_size}"$'\t'"${archive_sha}"
mv "${receipt}.tmp" "${receipt}"
echo "bomb_ghost_opposed_strong_archive_v7 archive_sha256 ${archive_sha} archive_bytes ${archive_size} key ${key} version_id ${version} restore_head_verified 1"
cat "${receipt}"
echo 'bomb_ghost_opposed_strong_archive_v7 complete 1'
