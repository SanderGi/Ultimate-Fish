#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/concrete-penguin-current-v1
readonly archive="${root}/ultimatefish-concrete-penguin-current-source.tar"
readonly archive_sha=8d9fc6dfc8b648c343f442a4cf7a0c0eb8c0d118cd826584c65b22f93bc28f24
readonly archive_version=q2TOOaMxj6tx2LbwO_1xImJW07n0W19e
readonly archive_key="sources/bundles/concrete-penguin-current-v1/sha256/${archive_sha}/ultimatefish-concrete-penguin-current-source.tar"
readonly source="${root}/source"
readonly dependencies="${root}/dependencies"
readonly manifest="${dependencies}/manifest.json"
readonly work=/mnt/ultimatefish/concrete-ghost-angel-opposed-current-v1/work
readonly model=b059287e45d662d3c600117af10f5be0c6858bd01fb0c0eff7470390f22b6bc5
readonly inventory=8818cbbd5661083994b300ce49a55f1cbb6700df2defaa29d1011d4e08f74371

test -d "${root}" && test -d "${dependencies}"
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = "${archive_sha}"
test -f "${source}/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py"
test "$(sha256sum "${manifest}" | cut -d ' ' -f 1)" = \
  765501667e2f3336dc2567293893bff4ea3a055cdbbc2640c20e6c30763aa137
test "$(sha256sum "${dependencies}/kghostk.uftb" | cut -d ' ' -f 1)" = \
  11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5
test "$(cd "${source}" && python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')" = \
  "${model}"
test "$(cd "${source}" && python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.inventory_sha256())')" = \
  "${inventory}"
test ! -e "${work}" || { [[ -d "${work}" ]] && ! find "${work}" -mindepth 1 -print -quit | grep -q .; }

cd "${source}"
exec python3 tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py \
  --work-directory "${work}" \
  --dependencies "${dependencies}" \
  --dependency-manifest "${manifest}" \
  --wave 0 --range-begin 237 --range-end 238 \
  --full --aws-execution-ack EC2 --workers 28 \
  --scratch-limit 483183820800 \
  --resident-limit 137438953472 \
  --reverse-edge-bytes-limit 375809638400 \
  --minimum-free-bytes 322122547200 \
  --monitor-interval 5 \
  --s3-prefix "s3://${bucket}/results/concrete/ghost-angel-opposed-current-v1"
