#!/usr/bin/env bash
# Solve the exact public-information overlay for same-team Jester/Angel.
set -euo pipefail

binary=/mnt/ultimatefish/jester-angel-v1-stage-34922731/binary/ultimate_tablebase
source_table=/mnt/ultimatefish-penguin/jester-angel-v1-source-work/outputs/kjesterangelk.uftb
lower_table=/mnt/ultimatefish/jester-angel-v1-dependencies-v2-900622da/kjesterk.uftb
lower_overlay=/mnt/ultimatefish/primary-jester-af8d/seed/ultimatefish-information-overlays.primary-af8d/kjesterk.ufiw
work=/mnt/ultimatefish/jester-angel-info-v1/same
scratch=$work/scratch
output=$work/kjesterangelk.ufiw
log=$work/solve.log

test ! -e "$work"
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  07534dbc67816851fa5b0a2ed6b56d2bc5a4182b986f99b6c8147624d6c0d642
test "$(sha256sum "$source_table" | cut -d' ' -f1)" = \
  0c52518058082de282fe2e3dbaec3b507dd091ba4dea7076740ddaaf853d2da9
test "$(sha256sum "$lower_table" | cut -d' ' -f1)" = \
  3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa
test "$(sha256sum "$lower_overlay" | cut -d' ' -f1)" = \
  b8aeceb739780ae9ab74c82cc476478c53d2e4296dd91010b073b9d6c46ee7f9
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -n 1)" -ge 128849018880
test "$(awk '/MemAvailable:/ {print $2 * 1024}' /proc/meminfo)" -ge 128849018880
install -d -m 0755 "$scratch"

export ULTIMATE_TABLEBASE_PATH="$(dirname "$source_table"):$(dirname "$lower_table")"
"$binary" --piece jester --piece2 angel \
  --solve-jester-information "$source_table" \
  --information-overlay "$output" \
  --information-scratch "$scratch" \
  --information-source-sha256 \
  0c52518058082de282fe2e3dbaec3b507dd091ba4dea7076740ddaaf853d2da9 \
  --information-model-sha256 \
  b2dbfbccc7281be74f218fa7f24e953b2d187ebe765be69a36f067fd70213d82 \
  --lower-information-overlay "$lower_overlay" \
  --lower-information-source-sha256 \
  3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa \
  --lower-information-model-sha256 \
  af8d6187577e6c0fedc0b9643020f843e9b61cab03af63ef2cd2138e0ca41c7b \
  >"$log" 2>&1

test -s "$output"
echo "JESTER_ANGEL_SAME_INFORMATION_OK output=$(sha256sum "$output" | cut -d' ' -f1) bytes=$(stat -c %s "$output")"
