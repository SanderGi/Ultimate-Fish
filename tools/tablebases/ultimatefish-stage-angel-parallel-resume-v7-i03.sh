#!/usr/bin/env bash
# Stage the scheduling-only completed-frontier parallel executor on i03.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_root=/mnt/ultimatefish/angel-parallel-resume-v7-source-8f9139d0
stage_root=/mnt/ultimatefish/angel-parallel-resume-v7-stage-8f9139d0
source_key=sources/bundles/angel-parallel-resume-v7/sha256/8f9139d08e1769e31bf435a1de4eb8c86aa7cf5713e6836bf18f7116892a166c/ultimatefish-parallel-resume-v7-source.tar
source_version=IAasgQeGwjoQAWXgNE1Qas92zl451fQF
source_sha=8f9139d08e1769e31bf435a1de4eb8c86aa7cf5713e6836bf18f7116892a166c
source_bytes=771584
model_sha=af60c54fae9c55116ac9ee8a368bf4804c89d60390c5b17ff5b358f89573299d
inventory_sha=46dbf01ff6b8e80a7bc0b7ecce66340944fdec4aae197e46d81e95d1d5fa289d
resume_key=sources/wrappers/sha256/cf182fb85bcedfa690c7136a7b7cb96cb1f159139e77a0c8f3a1e7fc722310f3/resume_ultimate_concrete_frontier_aws.py
resume_version=wOBdYdcVnNOraTR0grcrBPp2XEUOc1nW
resume_sha=cf182fb85bcedfa690c7136a7b7cb96cb1f159139e77a0c8f3a1e7fc722310f3
resume_bytes=36975

test ! -e "$source_root"
test ! -e "$stage_root"
install -d -m 0755 "$source_root" "$stage_root"

aws s3api get-object --bucket "$bucket" --key "$source_key" \
  --version-id "$source_version" --region "$region" \
  "$stage_root/source.tar" >/dev/null
test "$(stat -c %s "$stage_root/source.tar")" = "$source_bytes"
test "$(sha256sum "$stage_root/source.tar" | cut -d' ' -f1)" = "$source_sha"
tar -xf "$stage_root/source.tar" -C "$source_root"

aws s3api get-object --bucket "$bucket" --key "$resume_key" \
  --version-id "$resume_version" --region "$region" \
  "$stage_root/resume_ultimate_concrete_frontier_aws.py" >/dev/null
test "$(stat -c %s "$stage_root/resume_ultimate_concrete_frontier_aws.py")" = \
  "$resume_bytes"
test "$(sha256sum "$stage_root/resume_ultimate_concrete_frontier_aws.py" | cut -d' ' -f1)" = \
  "$resume_sha"
install -m 0444 "$stage_root/resume_ultimate_concrete_frontier_aws.py" \
  "$source_root/tools/tablebases/resume_ultimate_concrete_frontier_aws.py"

test "$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')" = \
  "$model_sha"
test "$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')" = \
  "$inventory_sha"

install -d -m 0755 "$source_root/binary"
(
  cd "$source_root/src"
  g++ -Iultimate -Iultimate/tablebases -std=c++17 -O3 -DNDEBUG \
    -Wall -Wextra -Wpedantic \
    ultimate/tablebases/tablebase.cpp ultimate/position.cpp \
    ultimate/tablebases/tablebase_probe.cpp \
    ultimate/tablebases/information.cpp \
    ultimate/tablebases/information_solver.cpp ultimate/nnue.cpp \
    -o "$source_root/binary/ultimate_tablebase"
)
chmod 0555 "$source_root/binary/ultimate_tablebase"
binary_sha=$(sha256sum "$source_root/binary/ultimate_tablebase" | cut -d' ' -f1)
binary_bytes=$(stat -c %s "$source_root/binary/ultimate_tablebase")
chmod -R a-w "$source_root"

echo "ANGEL_PARALLEL_RESUME_V7_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha resume=$resume_sha binary=$binary_sha binary_bytes=$binary_bytes"
