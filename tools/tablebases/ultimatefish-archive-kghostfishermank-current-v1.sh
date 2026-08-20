#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/fisherman-ghost-current-same-v3
tool=/mnt/ultimatefish/ghost-extra-remap-fix-v3-source/tools/tablebases/archive_ultimate_aws_result.py
output=${root}/archive-current-v1

test "$(sha256sum "${tool}" | cut -d ' ' -f 1)" = 7ce682e83eb234fe5d5391a8f655b4995df7ed13a055f9dda6f040a914a66fec
test "$(sha256sum "${root}/bundle-manifest.json" | cut -d ' ' -f 1)" = 7b57c21b115a5e729d61a072eec06c3554caab6033f00b04855c71295b94adc9
test "$(sha256sum "${root}/work/artifact-manifest.json" | cut -d ' ' -f 1)" = b36b78883064b70399cc4464b5948a168e52594240ac8d4967fd55d5c19f6373
test "$(sha256sum "${root}/work/results/kghostfishermank.ufiw" | cut -d ' ' -f 1)" = fca677c6f0446c1495cf8f69bee64d4302733881a0e2e3345b9870710ed6d5c3
test "$(sha256sum "${root}/work/results/kghostfishermank.ufgf" | cut -d ' ' -f 1)" = 8e179ae9856f360bec5754c201223852d0e256fc6c7b847ac0e5e73dd2e6fdb5
test ! -e "${output}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 107374182400

install -d "${output}"
/usr/bin/python3 "${tool}" \
  --root "${root}" \
  --artifact-manifest "${root}/work/artifact-manifest.json" \
  --kind kghostfishermank-current-341b0efa \
  --output-dir "${output}" \
  --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/ \
  --zstd-level 19 > "${output}/receipt.json.tmp"
mv "${output}/receipt.json.tmp" "${output}/receipt.json"
