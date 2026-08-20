#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish-jg/opposed-ghost-dragon-memory-v4
restore=${root}/restore/ghost-dragon-current-work-v1/kghostkdragon
source_root=/mnt/ultimatefish/compositional-transitions-v3-source
binary=${source_root}/ultimate_ghost_dragon_compositional_v3_linux
archive=${root}/authenticated-graph-v1.tar.zst
prefix=${restore}/work/transitions/kghostkdragon
log=${restore}/work/logs/verify-solve-memory-v5.log

test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = \
  679efb649622592a9d5034b56ea101623d169ed1a6f6843a6fa0c223ea029030
test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  86f1ca0fbf1555f4d6026f97a8424b1892e4f8d0c18aec96575b6c4ba6788f5a
test "$(sha256sum "${restore}/tablebases/kghostkdragon.uftb" | cut -d ' ' -f 1)" = \
  f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225
test "$(sha256sum "${restore}/tablebases/kdragonk.uftb" | cut -d ' ' -f 1)" = \
  28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6
test "$(sha256sum "${restore}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${restore}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  a38e67a57bf0c19b6fc264713356c09e7026e2812216a4eb0dd8c44906f617f6
grep -Fq '"orientation":"opposing"' "${restore}/work/transitions-ready.json"
grep -Fq '"source_sha256":"f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225"' \
  "${restore}/work/transitions-ready.json"
grep -Fq '"model_sha256":"c3ce5a68d38944b19a36594a6d3e1e3ad862d1c096902b350d4dadda19cf4de7"' \
  "${restore}/work/transitions-ready.json"
test "$(sha256sum "${prefix}.header" | cut -d ' ' -f 1)" = \
  bbec189f8ceeb25d1a1bcf06c7e7b2dea7a9f1008fdbd6c565ef16039fd3a03c
test "$(sha256sum "${prefix}.meta" | cut -d ' ' -f 1)" = \
  bf2b0cd70a372f8025b45b7e97e94362af04cdc04df2d979dd52209877166220
test "$(sha256sum "${prefix}.strata" | cut -d ' ' -f 1)" = \
  6df37a80f9cbaa89cba3176a84337e2233ecfb2d5ac3c70106afc99ff1262325
test "$(sha256sum "${prefix}.index" | cut -d ' ' -f 1)" = \
  54bf248c70758a112538091abee15c3daa61016bce685a186ece37a65ec3247d
test "$(sha256sum "${prefix}.blocks" | cut -d ' ' -f 1)" = \
  dae9183ce016ad9cca8c867292cdd778b4f41e5020f90a325b663ed90e7eb191
test "$(sha256sum "${prefix}.verified" | cut -d ' ' -f 1)" = \
  273e56f7d8f3f7110ffc101ae423ff18c8b2fb51815c1485d4c4b37aa0652eb0
test "$(sha256sum "${root}/self-test-v3.normalized.uftb" | cut -d ' ' -f 1)" = \
  96fa7bd503cfee014d52af8fd945b875f49c467972b8ff49e53cca6c0ad625ff

mkdir -p "${restore}/work/evidence" "${restore}/work/results" \
  "${restore}/work/solve"
install -m 0644 "${prefix}.verified" \
  "${restore}/work/evidence/kghostkdragon-legacy.verified"
install -m 0644 "${root}/self-test-v3.normalized.uftb" \
  "${restore}/work/solve/kghostkdragon.normalized.uftb"

exec >>"${log}" 2>&1
echo "dragon_opposed_memory_v5 archive_sha256 679efb649622592a9d5034b56ea101623d169ed1a6f6843a6fa0c223ea029030 archive_version u4cDO9VSrWR_gXYqqzU3B0FdfFnsBWfR source_bundle_sha256 75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83 source_bundle_version 5Ig1aSjwOsVHwI4opCNihZRgvJg_2YXd binary_sha256 86f1ca0fbf1555f4d6026f97a8424b1892e4f8d0c18aec96575b6c4ba6788f5a binary_version S.iTuzESz5k8_1vwX.18qrs6FNIXpu9n legacy_marker_preserved ${restore}/work/evidence/kghostkdragon-legacy.verified"

common=(
  --orientation opposing
  --transition-prefix work/transitions/kghostkdragon
  --lower-dragon-table tablebases/kdragonk.uftb
  --lower-dragon-sha256 28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6
  --lower-dragon-source-sha256 28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6
  --lower-dragon-model-sha256 cd3bcf48109e0950bd73e27e157d249dc6c031f2d1cf135bcd4302f307911c74
  --source-sha256 f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225
  --model-sha256 c3ce5a68d38944b19a36594a6d3e1e3ad862d1c096902b350d4dadda19cf4de7
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)

cd "${restore}"
"${binary}" --verify-transitions "${common[@]}"

"${binary}" \
  --solve \
  "${common[@]}" \
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
  --max-nodes 2000000000 \
  --unique-slots 4294967296 \
  --compact-every 1

sha256sum work/results/kghostkdragon-memory-v5.ufiw \
  work/results/kghostkdragon-memory-v5.ufgd
echo 'dragon_opposed_memory_v5 complete 1'
