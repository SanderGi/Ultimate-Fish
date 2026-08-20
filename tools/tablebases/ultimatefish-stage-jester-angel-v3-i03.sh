#!/usr/bin/env bash
# Stage and self-test the parallel Jester/Angel information solver on i03.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_key=sources/bundles/jester-angel-v1/sha256/20db30fcf7fd2dbc19bbbd1a1a860c3b06765551150ef8b0736b78d15b2dfe97/ultimatefish-jester-angel-v1-source.tar
source_version=XJuPcn2ij3cr2OWIApeI0B9ORQP.Z5WJ
source_sha=20db30fcf7fd2dbc19bbbd1a1a860c3b06765551150ef8b0736b78d15b2dfe97
source_bytes=768000
model_sha=d64b5cdfd9caf9849ceaf8e58f617930e462351920647c332d4ebff39f8dd8ab
inventory_sha=ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143
information_model_sha=5582d014c827845df5eb658078ad0be8f367b9fde087bf22509e673fdfd23c66
tablebase_sha=2fcaa42fb4616fc5e17c9c1bcbaa81d1b003bd86d08ce80444b1cbeaa2bdaf81
probe_sha=e17573a1b0aa01bbb232a2144903b300c8945c8efb64d862da919dd14dc5449c
runner_sha=5f675fcaa9934a13abbba0adac3c4f3223b762165a2fba37b2bca03e79c16a5a

source_root=/mnt/ultimatefish/jester-angel-v3b-source-20db30fc
stage_root=/mnt/ultimatefish/jester-angel-v3b-stage-20db30fc

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
test "$actual_information_model" = "$information_model_sha"
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
  --workers 8 --self-test --dry-run 20000 >"$stage_root/self-test-same.log"
"$stage_root/binary/ultimate_tablebase" --piece jester --piece2 angel \
  --opposing --workers 8 --self-test --dry-run 20000 \
  >"$stage_root/self-test-opposed.log"
for log in "$stage_root/self-test-same.log" "$stage_root/self-test-opposed.log"; do
  grep -q '^angeljestertransitiondecisionok index 492966 residual 0$' "$log"
  grep -q '^parallelresumereversescanok workers 8$' "$log"
  grep -q '^jesterroyalswapcodecok samples 20000$' "$log"
  grep -q '^dryrun states 0\.\.20000 ' "$log"
done
binary_sha=$(sha256sum "$stage_root/binary/ultimate_tablebase" | cut -d' ' -f1)

chmod -R a-w "$source_root" "$stage_root"
echo "JESTER_ANGEL_V3_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha information_model=$actual_information_model binary=$binary_sha"
