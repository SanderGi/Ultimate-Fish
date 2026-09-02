#!/usr/bin/env bash
set -euo pipefail

# Idempotent final-mile handoff for the lone-Devil fixed-square campaign.
# The expensive solver owns square 2.  The other eleven fragments are exact
# S3-VersionId restores whose hashes are rechecked on every attempted merge.

readonly campaign_root=/mnt/ultimatefish/devil-spawned-c1-v28
readonly fragment_root=${campaign_root}/fragments-restored-v1
readonly c1_root=${campaign_root}/graphs/square-2/devil-2.roots
readonly merge_root=${campaign_root}/merge-v1
readonly merger=/mnt/ultimatefish/devil-finalize-v1/merge_ultimate_devil_spawned_roots.py
readonly output=${merge_root}/kdevilk.uftb
readonly receipt=${merge_root}/devil-root-merge-receipt.json

[[ ! -s "${receipt}" ]] || exit 0
[[ -s "${c1_root}" ]] || exit 0
[[ -x "${merger}" ]] || {
  echo "missing authenticated Devil merger: ${merger}" >&2
  exit 1
}

declare -Ar expected_sha256=(
  [0]=b5fb00cb344575bbc7ebf34bb866ebee6000939eab71558ad1221f3b37d0cf27
  [1]=12eb3e9ba73b134c538399fa251ed708366598b2d7b93637bf506b7dcc2d6232
  [3]=c1299e51b7d5d11be2f6f9eb7201e30c187eb3d314ded94ea9f6c8df0451c9a6
  [8]=bf6c6a16476336bd711cd11dcd49ff82ad2fde5c2fd61d7bea3f03abcf890344
  [9]=7d1cb299af6e3022d4063102f6074917a9fe6e974574094eadf0929e5f130108
  [10]=87fd0b18664b90d6f7354c0ada9f3c1d14a3d3e23c155c58da401b681c7b050d
  [11]=22fb6d5df071ffb0c33d8eff717d71e14fe94cc73017965371dd3e4cfb37c8eb
  [16]=3b3d90ef2f9c932f5b6da3383256997af4cfe9a5e627b77468fbd3b58594b2b8
  [17]=b34ff12132cbbb8879b9bb8737bf3f799b0134246099ad82c46887c1a2380340
  [18]=64386789206e5e82e3c5cf06cf8c8f1419f3e83fbc2e0d8905b9b820c0feec2f
  [19]=f81dd61ca01c641f8757e96b9098102835a27fb003599d2d2d2d959f236aad35
)

fragments=()
for square in 0 1 3 8 9 10 11 16 17 18 19; do
  fragment=${fragment_root}/devil-${square}.roots
  [[ -s "${fragment}" ]] || {
    echo "missing retained Devil fragment: ${fragment}" >&2
    exit 1
  }
  actual_sha256=$(sha256sum "${fragment}")
  actual_sha256=${actual_sha256%% *}
  [[ "${actual_sha256}" == "${expected_sha256[${square}]}" ]] || {
    echo "retained Devil fragment hash residual: ${fragment}" >&2
    exit 1
  }
  fragments+=("${fragment}")
done
fragments+=("${c1_root}")

install -d -m 0755 "${merge_root}"
[[ ! -e "${output}" && ! -e "${receipt}" ]] || {
  echo "partial Devil merge output requires audit: ${merge_root}" >&2
  exit 1
}

"${merger}" --output "${output}" --receipt "${receipt}" "${fragments[@]}"
test -s "${output}"
test -s "${receipt}"

