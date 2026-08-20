#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/kninjakghost-migrated-v1/restore/kninjakghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v7-ninja-7cb5a18a
old_scratch=${root}/work/solve-source-order-v6/kninjakghost
new_directory=${root}/work/solve-source-order-v7
new_scratch=${new_directory}/kninjakghost
manifest=${root}/work/ninja-v7-resume-expanded.sha256
certificate=${source_root}/ninja-v7-build-certificate.json
binary=${source_root}/ultimate_ghost_ninja_resume_v7_ordinary_linux
restored=${binary}.restored
source_table=${root}/tablebases/kninjakghost.uftb
bucket=ultimatefish-info-20260808-a4e679c6-831688117652
source_key=sources/bundles/queen-ghost-resume-v7/sha256/7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae/ultimatefish-queen-ghost-resume-v7-source.tar.gz
source_version=E1i2XhZzFcsT02PhLMKAFHG1tspK3LAn
source_sha=7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae
source_table_sha=4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
normalized_sha=bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a

test ! -e "${source_root}"
test ! -e "${new_directory}"
test ! -e "${manifest}"
test "$(sha256sum "${source_table}" | awk '{print $1}')" = "${source_table_sha}"
grep -Fq 'reciprocal_ghost_extra_iteration 3 bdd_nodes 387478882' \
  "${root}/work/logs/source-order-v6.log"
grep -Fq \
  'ghost_extra_external_compaction iteration 3 roots 42648839 marked_nodes 6281543 copied_nodes 6281541 structural_residual 0 root_residual 0' \
  "${root}/work/logs/source-order-v6.log"
grep -Fq \
  'reciprocal_ghost_extra_bellman iteration 4 geometry 490000/492960' \
  "${root}/work/logs/source-order-v6.log"
grep -Fq 'external ROBDD exact node budget exhausted' \
  "${root}/work/logs/source-order-v6.log"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 107374182400

install -d -m 0755 "${source_root}"
exec >"${source_root}/build-resume-v7.log" 2>&1

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${source_key}" --version-id "${source_version}" \
  "${source_root}/source.tar.gz" >"${source_root}/source-get.json"
test "$(sha256sum "${source_root}/source.tar.gz" | awk '{print $1}')" = \
  "${source_sha}"
tar -xzf "${source_root}/source.tar.gz" -C "${source_root}"

cd "${source_root}"
taskset -c 12 clang++ \
  -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Ninja \
  -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/position.cpp src/ultimate/nnue.cpp \
  -o "${binary}"

mkdir "${source_root}/self-test"
taskset -c 12 "${binary}" \
  --self-test \
  --orientation opposing \
  --scratch "${source_root}/self-test/kninjakghost" \
  --input "${source_table}" \
  --source-sha256 "${source_table_sha}"
test "$(sha256sum "${source_root}/self-test/kninjakghost.normalized.uftb" | \
  awk '{print $1}')" = "${normalized_sha}"

# Keep the failed v6 tree byte-for-byte.  The compacted iteration-3 roots are
# in the physical *-next planes and refer only to the first 6,281,543 nodes of
# bdd-b.  Nodes appended by the failed iteration-4 prefix remain useful to the
# unique table and deterministically avoid recomputing that prefix.
cp -a --sparse=always "${root}/work/solve-source-order-v6" "${new_directory}"
python3 - "${new_scratch}" <<'PY'
import array
import pathlib
import sys

prefix = pathlib.Path(sys.argv[1])
limit = 6_281_543
for suffix in ("owner-next", "observer-next", "domains"):
    values = array.array("I")
    with (prefix.parent / f"{prefix.name}.{suffix}").open("rb") as stream:
        while block := stream.read(4 << 20):
            values.frombytes(block)
    if values and max(values) >= limit:
        raise SystemExit(f"iteration-3 {suffix} root outside compacted bdd-b")
for suffix in ("visible-owner-next", "visible-observer-next"):
    payload = (prefix.parent / f"{prefix.name}.{suffix}").read_bytes()
    if payload and max(payload) > 1:
        raise SystemExit(f"iteration-3 {suffix} Boolean residual")
PY

# ExternalRobdd binds the node-file extent to maxNodes.  Extend only the cloned
# active bdd-b file from 500M to 750M slots; its original 4.5GB prefix is
# authenticated below and the failed evidence is never modified.
test "$(stat -c %s "${old_scratch}.bdd-b.nodes")" = 4500000000
test "$(stat -c %s "${new_scratch}.bdd-b.nodes")" = 4500000000
old_b_prefix_sha=$(sha256sum "${old_scratch}.bdd-b.nodes" | awk '{print $1}')
test "$(sha256sum "${new_scratch}.bdd-b.nodes" | awk '{print $1}')" = \
  "${old_b_prefix_sha}"
truncate -s 6750000000 "${new_scratch}.bdd-b.nodes"
test "$(stat -c %s "${new_scratch}.bdd-b.nodes")" = 6750000000

(
  cd "${new_directory}"
  sha256sum \
    kninjakghost.bdd-b.nodes \
    kninjakghost.bdd-b.unique \
    kninjakghost.domains \
    kninjakghost.owner-next \
    kninjakghost.observer-next \
    kninjakghost.visible-owner-next \
    kninjakghost.visible-observer-next \
    >"${manifest}"
  sha256sum -c "${manifest}"
)
manifest_sha=$(sha256sum "${manifest}" | awk '{print $1}')

binary_sha=$(sha256sum "${binary}" | awk '{print $1}')
binary_key="sources/binaries/sha256/${binary_sha}/ultimate_ghost_ninja_resume_v7_ordinary_linux"
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},purpose=ninja-ghost-fixed-point-resume-v7" \
  >"${source_root}/binary-put.json"
binary_version=$(python3 -c \
  'import json,sys;print(json.load(open(sys.argv[1]))["VersionId"])' \
  "${source_root}/binary-put.json")
aws s3api head-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  >"${source_root}/binary-head.json"
python3 - "${source_root}/binary-head.json" "${binary_version}" "${binary_sha}" <<'PY'
import json
import sys
d = json.load(open(sys.argv[1]))
assert d["VersionId"] == sys.argv[2]
assert d["Metadata"]["sha256"] == sys.argv[3]
PY
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${restored}" >"${source_root}/binary-get.json"
test "$(sha256sum "${restored}" | awk '{print $1}')" = "${binary_sha}"
cmp "${binary}" "${restored}"

NINJA_BINARY_SHA="${binary_sha}" \
NINJA_BINARY_KEY="${binary_key}" \
NINJA_BINARY_VERSION="${binary_version}" \
NINJA_CHECKPOINT_MANIFEST_SHA="${manifest_sha}" \
NINJA_BDD_B_ORIGINAL_PREFIX_SHA="${old_b_prefix_sha}" \
python3 - "${certificate}" <<'PY'
import json
import os
import pathlib
import sys
record = {
    "schema": "ultimate-ninja-ghost-resume-build-v1",
    "source_sha256": "7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae",
    "source_version_id": "E1i2XhZzFcsT02PhLMKAFHG1tspK3LAn",
    "source_table_sha256": "4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027",
    "normalized_source_sha256": "bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a",
    "binary_sha256": os.environ["NINJA_BINARY_SHA"],
    "binary_key": os.environ["NINJA_BINARY_KEY"],
    "binary_version_id": os.environ["NINJA_BINARY_VERSION"],
    "checkpoint_manifest_sha256": os.environ["NINJA_CHECKPOINT_MANIFEST_SHA"],
    "bdd_b_original_prefix_sha256": os.environ["NINJA_BDD_B_ORIGINAL_PREFIX_SHA"],
    "resume_iteration": 3,
    "resume_current_slot": "next",
    "resume_bdd_slot": "b",
    "compacted_iteration_3_nodes": 6_281_543,
    "partial_iteration_4_unique_nodes_reused": True,
    "max_nodes": 750_000_000,
    "read_only_implication": True,
    "fresh_restore_residual": 0,
}
target = pathlib.Path(sys.argv[1])
temporary = target.with_suffix(".tmp")
temporary.write_text(json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n")
temporary.replace(target)
PY

cat "${certificate}"
sha256sum "${certificate}" "${manifest}" "${binary}"
