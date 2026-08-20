#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/ghost-implication-v8-source
bucket=ultimatefish-info-20260808-a4e679c6-831688117652
source_key=sources/bundles/sha256/0e36f439827f02d88a21c0beee516c0a7b104fe49834a542498c60270437cb1f/ultimatefish-ghost-implication-v8.tar.gz
source_version=1fFkLKovK9u97bC8P1EezyRkh4wjMaRO
source_sha=0e36f439827f02d88a21c0beee516c0a7b104fe49834a542498c60270437cb1f
source_table=/mnt/ultimatefish/info-remap-fix-v2/kpawnkghost-fresh-v4/tablebases/kpawnkghost.uftb
source_table_sha=6d2f23b873a861b0c51cb86a8378013619c5cce0adb29e0060bce07cc2a2ae46
normalized_sha=87cdf78459ad1eaa90cb36c2dc8e8ded1b704d61fcf807c1d739eef687dc9105
binary=${root}/ultimate_ghost_pawn_implication_v8_linux
restored=${binary}.restored
certificate=${root}/pawn-implication-v8-certificate.json

test ! -e "${root}/source.tar.gz"
test ! -e "${root}/src"
test ! -e "${binary}"
test ! -e "${restored}"
test ! -e "${certificate}"
test "$(sha256sum "${source_table}" | awk '{print $1}')" = "${source_table_sha}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 2147483648

exec >"${root}/build-pawn-implication-v8.log" 2>&1

aws s3api get-object \
  --region us-west-2 \
  --bucket "${bucket}" \
  --key "${source_key}" \
  --version-id "${source_version}" \
  "${root}/source.tar.gz" >"${root}/source-get.json"
test "$(sha256sum "${root}/source.tar.gz" | awk '{print $1}')" = "${source_sha}"
tar -xzf "${root}/source.tar.gz" -C "${root}"

cd "${root}"
taskset -c 15 clang++ \
  -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Pawn \
  -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY \
  -DULTIMATE_GHOST_EXTRA_SUBSTATES=2 \
  -DULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN \
  -DULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/position.cpp src/ultimate/nnue.cpp \
  -o "${binary}"

mkdir "${root}/self-test"
taskset -c 15 "${binary}" \
  --self-test \
  --orientation opposing \
  --scratch "${root}/self-test/kpawnkghost" \
  --input "${source_table}" \
  --source-sha256 "${source_table_sha}"
test "$(sha256sum "${root}/self-test/kpawnkghost.normalized.uftb" |
  awk '{print $1}')" = "${normalized_sha}"

binary_sha=$(sha256sum "${binary}" | awk '{print $1}')
binary_key="sources/binaries/sha256/${binary_sha}/ultimate_ghost_pawn_implication_v8_linux"
aws s3api put-object \
  --region us-west-2 \
  --bucket "${bucket}" \
  --key "${binary_key}" \
  --body "${binary}" \
  --metadata "sha256=${binary_sha},purpose=pawn-ghost-read-only-implication-v8" \
  >"${root}/binary-put.json"
binary_version=$(python3 -c \
  'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' \
  "${root}/binary-put.json")
aws s3api head-object \
  --region us-west-2 \
  --bucket "${bucket}" \
  --key "${binary_key}" \
  --version-id "${binary_version}" >"${root}/binary-head.json"
python3 -c \
  'import json,sys; d=json.load(open(sys.argv[1])); assert d["VersionId"] == sys.argv[2]; assert d["Metadata"]["sha256"] == sys.argv[3]' \
  "${root}/binary-head.json" "${binary_version}" "${binary_sha}"
aws s3api get-object \
  --region us-west-2 \
  --bucket "${bucket}" \
  --key "${binary_key}" \
  --version-id "${binary_version}" \
  "${restored}" >"${root}/binary-get.json"
test "$(sha256sum "${restored}" | awk '{print $1}')" = "${binary_sha}"
cmp "${binary}" "${restored}"

IMPLICATION_SOURCE_SHA="${source_sha}" \
IMPLICATION_SOURCE_VERSION="${source_version}" \
IMPLICATION_SOURCE_TABLE_SHA="${source_table_sha}" \
IMPLICATION_NORMALIZED_SHA="${normalized_sha}" \
IMPLICATION_BINARY_SHA="${binary_sha}" \
IMPLICATION_BINARY_KEY="${binary_key}" \
IMPLICATION_BINARY_VERSION="${binary_version}" \
python3 -c '
import json, os, pathlib
record = {
    "schema": "ultimate-ghost-implication-build-v1",
    "source_sha256": os.environ["IMPLICATION_SOURCE_SHA"],
    "source_version_id": os.environ["IMPLICATION_SOURCE_VERSION"],
    "source_table_sha256": os.environ["IMPLICATION_SOURCE_TABLE_SHA"],
    "normalized_source_sha256": os.environ["IMPLICATION_NORMALIZED_SHA"],
    "binary_sha256": os.environ["IMPLICATION_BINARY_SHA"],
    "binary_key": os.environ["IMPLICATION_BINARY_KEY"],
    "binary_version_id": os.environ["IMPLICATION_BINARY_VERSION"],
    "codec_states": 303663360,
    "implication_read_only": True,
    "self_test_residual": 0,
    "fresh_restore_residual": 0,
}
target = pathlib.Path("pawn-implication-v8-certificate.json")
temporary = target.with_suffix(".tmp")
temporary.write_text(json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n")
temporary.replace(target)
'
sha256sum "${certificate}"
