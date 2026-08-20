#!/usr/bin/env bash
# Build a separate v10 reachability auditor without modifying the frozen solver.
set -euo pipefail

source_root=/mnt/ultimatefish/angel-copycat-v10-source-393507ea
audit_root=/mnt/ultimatefish/angel-copycat-v10-reachability-auditor-584cf62d
patched_sha=584cf62d4fdfaf8ae5cf7f252df97c963c9234283d7f9e16b3724b56e32ecaa7
patched_key=sources/auditors/angel-copycat-v10-reachability/sha256/$patched_sha/tablebase.cpp
patched_version=km6nlo8Dhy5RB9_vpX6iC0rQP.9A.zbT
bucket=ultimatefish-info-20260808-a4e679c6-831688117652

test -d "$source_root"
test ! -e "$audit_root"
mkdir -p "$audit_root"
cp -a "$source_root" "$audit_root/source"
cp "$audit_root/source/src/ultimate/tablebases/tablebase.cpp" \
  "$audit_root/generator-tablebase.cpp"

aws s3api get-object \
  --bucket "$bucket" --key "$patched_key" --version-id "$patched_version" \
  --region us-west-2 "$audit_root/tablebase.cpp" >"$audit_root/source-get.json"
test "$(sha256sum "$audit_root/tablebase.cpp" | cut -d' ' -f1)" = "$patched_sha"
install -m 0444 "$audit_root/tablebase.cpp" \
  "$audit_root/source/src/ultimate/tablebases/tablebase.cpp"

cd "$audit_root/source"
clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -pthread -Isrc/ultimate \
  src/ultimate/tablebases/tablebase.cpp \
  src/ultimate/position.cpp \
  src/ultimate/tablebases/tablebase_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/tablebases/information_solver.cpp \
  src/ultimate/nnue.cpp \
  -o "$audit_root/ultimate_tablebase-audit-v10"
chmod 0555 "$audit_root/ultimate_tablebase-audit-v10"
sha256sum "$audit_root/ultimate_tablebase-audit-v10" \
  >"$audit_root/ultimate_tablebase-audit-v10.sha256"
"$audit_root/ultimate_tablebase-audit-v10" \
  --piece copycat --piece2 angel --self-test >"$audit_root/self-test.log"
test -s "$audit_root/self-test.log"
