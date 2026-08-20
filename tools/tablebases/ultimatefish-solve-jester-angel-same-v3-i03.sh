#!/usr/bin/env bash
# Solve same-team Jester/Angel with the parallel, legal-dot-corrected solver.
set -euo pipefail

binary=/mnt/ultimatefish/jester-angel-v3b-stage-20db30fc/binary/ultimate_tablebase
source_table=/mnt/ultimatefish-penguin/jester-angel-v1-source-work/outputs/kjesterangelk.uftb
lower_table=/mnt/ultimatefish-penguin/jester-angel-v1-source-work/dependencies/kjesterk.uftb
lower_overlay=/mnt/ultimatefish-penguin/jester-angel-v3-dependencies/overlays/kjesterk-af8d.ufiw
work=/mnt/ultimatefish/jester-angel-info-v3/same
scratch=$work/scratch
output=$work/kjesterangelk.ufiw
log=$work/solve.log

test ! -e "$work"
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  5a9349006bece04d031ffe3ac6924f23793695dcd75cb194dfbfbaaea689197f
test "$(sha256sum "$source_table" | cut -d' ' -f1)" = \
  0c52518058082de282fe2e3dbaec3b507dd091ba4dea7076740ddaaf853d2da9
test "$(sha256sum "$lower_table" | cut -d' ' -f1)" = \
  3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa
test "$(sha256sum "$lower_overlay" | cut -d' ' -f1)" = \
  b8aeceb739780ae9ab74c82cc476478c53d2e4296dd91010b073b9d6c46ee7f9
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -n 1)" -ge 137438953472
test "$(awk '/MemAvailable:/ {print $2 * 1024}' /proc/meminfo)" -ge 107374182400
install -d -m 0755 "$scratch"

export ULTIMATE_TABLEBASE_PATH="$(dirname "$source_table"):$(dirname "$lower_table")"
"$binary" --piece jester --piece2 angel --workers 8 \
  --solve-jester-information "$source_table" \
  --information-overlay "$output" \
  --information-scratch "$scratch" \
  --information-source-sha256 \
  0c52518058082de282fe2e3dbaec3b507dd091ba4dea7076740ddaaf853d2da9 \
  --information-model-sha256 \
  5582d014c827845df5eb658078ad0be8f367b9fde087bf22509e673fdfd23c66 \
  --lower-information-overlay "$lower_overlay" \
  --lower-information-source-sha256 \
  3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa \
  --lower-information-model-sha256 \
  af8d6187577e6c0fedc0b9643020f843e9b61cab03af63ef2cd2138e0ca41c7b \
  >"$log" 2>&1

test -s "$output"
echo "JESTER_ANGEL_SAME_INFORMATION_V3_OK output=$(sha256sum "$output" | cut -d' ' -f1) bytes=$(stat -c %s "$output")"
