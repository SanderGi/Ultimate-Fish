#!/usr/bin/env bash
# Stage and self-test the legal-dot-corrected Jester/Angel solver on i03.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_key=sources/bundles/jester-angel-v1/sha256/a76ecfa97eddfc788fa3008124d17b7f017c8e85eae0533aae728f3c9ab54814/ultimatefish-jester-angel-v1-source.tar
source_version=8Q2gEs4aobJ7PeY7NB05YrJNJHLQzJuN
source_sha=a76ecfa97eddfc788fa3008124d17b7f017c8e85eae0533aae728f3c9ab54814
source_bytes=768000
model_sha=c686edc2be6663e9f16631910605ae20b954d9065b92b3737e0974e812c6a4de
inventory_sha=ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143
tablebase_sha=7b2ae39ece77eafbd927345a03305c4a2e5aedce19d647b1e259f5cdc899ca14
probe_sha=e17573a1b0aa01bbb232a2144903b300c8945c8efb64d862da919dd14dc5449c
runner_sha=5f675fcaa9934a13abbba0adac3c4f3223b762165a2fba37b2bca03e79c16a5a

source_root=/mnt/ultimatefish/jester-angel-v2-source-a76ecfa9
stage_root=/mnt/ultimatefish/jester-angel-v2-stage-a76ecfa9

test ! -e "$source_root"
test ! -e "$stage_root"
install -d -m 0755 "$source_root" "$stage_root"

aws s3api get-object --bucket "$bucket" --key "$source_key" \
  --version-id "$source_version" --region "$region" \
  "$stage_root/source.tar" >/dev/null
test "$(stat -c %s "$stage_root/source.tar")" = "$source_bytes"
test "$(sha256sum "$stage_root/source.tar" | cut -d' ' -f1)" = "$source_sha"
tar -xf "$stage_root/source.tar" -C "$source_root"

actual_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')
actual_inventory=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')
actual_information_model=$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import ultimate_information_tablebases as i;print(i.solver_model_fingerprint("kjesterkangel.uftb"))')
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"
test "$actual_information_model" = \
  bd3a0c5fbab17f460cc90903a3374e528b86103329df0563c3c3d15767bfbb1f
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = "$tablebase_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase_probe.cpp" | cut -d' ' -f1)" = "$probe_sha"
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = "$runner_sha"

install -d -m 0755 "$stage_root/binary"
(
  cd "$source_root"
  g++ -Isrc/ultimate -Isrc/ultimate/tablebases -std=c++17 -O3 -DNDEBUG \
    -Wall -Wextra -Wpedantic \
    src/ultimate/tablebases/tablebase.cpp src/ultimate/position.cpp \
    src/ultimate/tablebases/tablebase_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/tablebases/information_solver.cpp src/ultimate/nnue.cpp \
    -o "$stage_root/binary/ultimate_tablebase"
)
"$stage_root/binary/ultimate_tablebase" --piece jester --piece2 angel \
  --self-test --dry-run 100 >"$stage_root/self-test-same.log"
"$stage_root/binary/ultimate_tablebase" --piece jester --piece2 angel \
  --opposing --self-test --dry-run 100 >"$stage_root/self-test-opposed.log"
for log in "$stage_root/self-test-same.log" "$stage_root/self-test-opposed.log"; do
  grep -q '^angeljestertransitiondecisionok index 492966 residual 0$' "$log"
  grep -q '^jesterroyalswapcodecok samples 20000$' "$log"
  grep -q '^dryrun states 0\.\.100 ' "$log"
done
binary_sha=$(sha256sum "$stage_root/binary/ultimate_tablebase" | cut -d' ' -f1)

chmod -R a-w "$source_root" "$stage_root"
echo "JESTER_ANGEL_V2_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha information_model=$actual_information_model binary=$binary_sha"
