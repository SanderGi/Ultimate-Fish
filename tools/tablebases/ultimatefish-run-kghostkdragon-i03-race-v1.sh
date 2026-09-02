#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly stage=/mnt/ultimatefish/ghost-dragon-i03-race-v1
readonly archive=${stage}/authenticated-graph-v1.tar.zst
readonly restore=${stage}/restore/ghost-dragon-current-work-v1/kghostkdragon
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-dragon-v26
readonly source=${stage}/ultimatefish-ghost-dragon-parallel-v26-source.tar
readonly normalized=${restore}/work/solve/kghostkdragon.normalized.uftb
readonly log=${restore}/work/logs/solve-i03-race-v1.log

mkdir -p "${stage}"
if [[ ! -s "${archive}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key results/checkpoints/ghost-dragon/opposed/graph-v1/sha256/679efb649622592a9d5034b56ea101623d169ed1a6f6843a6fa0c223ea029030/authenticated-graph-v1.tar.zst \
    --version-id tGQosk2CGRF4If4zw4gDr49iilHlYbax "${archive}"
fi
test "$(sha256sum "${archive}" | cut -d ' ' -f1)" = \
  679efb649622592a9d5034b56ea101623d169ed1a6f6843a6fa0c223ea029030

if [[ ! -d "${restore}" ]]; then
  mkdir -p "${stage}/restore"
  tar --zstd -xf "${archive}" -C "${stage}/restore"
fi
if [[ ! -s "${binary}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key sources/binaries/ghost-parallel-v26/dragon/sha256/109d806d73243b544c106953dfadf1a7a5ac7fcfabd4b4c748bdc57044901afb/ultimate_ghost_ordinary_information_tablebase-dragon-v26 \
    --version-id Q6lITyol9ytTGZe6hjaZeIgUwil8_FTQ "${binary}"
  chmod 0755 "${binary}"
fi
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  109d806d73243b544c106953dfadf1a7a5ac7fcfabd4b4c748bdc57044901afb
if [[ ! -s "${source}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key sources/bundles/ghost-dragon-parallel-v26/sha256/eaee7253ff9313290dd7b3436f7572466932177e503efd1389c347a4a8985d84/ultimatefish-ghost-dragon-parallel-v26-source.tar \
    --version-id jalHavk2h4QI0Cy267M3j_11brHWmWXi "${source}"
fi
test "$(sha256sum "${source}" | cut -d ' ' -f1)" = \
  eaee7253ff9313290dd7b3436f7572466932177e503efd1389c347a4a8985d84

mkdir -p "${restore}/work/solve" "${restore}/work/results" \
  "${restore}/work/logs"
if [[ ! -s "${normalized}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key results/checkpoints/ghost-dragon/opposed/normalized-v1/sha256/96fa7bd503cfee014d52af8fd945b875f49c467972b8ff49e53cca6c0ad625ff/kghostkdragon.normalized.uftb \
    --version-id DZX._9FksVOU8VTkFnvv7xc4A8pY1kyU "${normalized}"
fi
test "$(sha256sum "${normalized}" | cut -d ' ' -f1)" = \
  96fa7bd503cfee014d52af8fd945b875f49c467972b8ff49e53cca6c0ad625ff
test "$(sha256sum "${restore}/tablebases/kghostkdragon.uftb" | cut -d ' ' -f1)" = \
  f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225
test "$(sha256sum "${restore}/tablebases/kdragonk.uftb" | cut -d ' ' -f1)" = \
  28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6
test "$(sha256sum "${restore}/tablebases/kghostk.ufgm" | cut -d ' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test -s "${restore}/work/transitions/kghostkdragon.verified"
test "$(find "${restore}/work/solve" -maxdepth 1 -type f ! -name 'kghostkdragon.normalized.uftb' -print -quit)" = ""
test ! -e "${restore}/work/results/kghostkdragon-memory-v5.ufiw"
test ! -e "${restore}/work/results/kghostkdragon-memory-v5.ufgd"
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 161061273600

exec >>"${log}" 2>&1
echo 'dragon_i03_race_v1 fresh_fixed_point workers=14 archive_restore_residual=0'
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
  --normalized-input work/solve/kghostkdragon.normalized.uftb \
  --normalized-sha256 96fa7bd503cfee014d52af8fd945b875f49c467972b8ff49e53cca6c0ad625ff \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch work/solve/kghostkdragon \
  --output work/results/kghostkdragon-memory-v5.ufiw \
  --output-arbitrary work/results/kghostkdragon-memory-v5.ufgd \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 2000000000 --unique-slots 4294967296 \
  --compact-every 1
