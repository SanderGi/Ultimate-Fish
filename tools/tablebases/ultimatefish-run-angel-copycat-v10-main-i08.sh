#!/usr/bin/env bash
# Generate and S3-verify Same Copycat/Angel using both exact lower tables.
set -euo pipefail

source_root=/mnt/ultimatefish/angel-copycat-v10-source-393507ea
dependency_root=/mnt/ultimatefish/angel-copycat-v10-main-dependencies-51385018
work_root=/mnt/ultimatefish/angel-copycat-v10-main-work/kcopycatangelk-v1
manifest=$dependency_root/manifest.json

test -d "$source_root"
test -d "$dependency_root"
test -f "$manifest"
test ! -e "$work_root"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = \
  57b3eff579237992c9b3f221fad76cc794c3f9c75e860cb195cb4964cdd43ca2
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = \
  2ec59976d2a9591ea3fb6e485de9a4947840b3402d013a6ac53d715e8752b34a
test "$(sha256sum "$manifest" | cut -d' ' -f1)" = \
  51385018e8c7ad83c0ad9e357de31a0f9a6cbedeb4b78618ab64905b42bb3fb7
test "$(sha256sum "$dependency_root/kcopycatk.uftb" | cut -d' ' -f1)" = \
  98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb
test "$(sha256sum "$dependency_root/kcopycatlinkedk.uftb" | cut -d' ' -f1)" = \
  e55924a8b97acedcbc9876b95a23837030d5112a35d2b06ec0cb85ff4ab16dd2

cd "$source_root"
exec /usr/bin/python3 tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py \
  --work-directory "$work_root" \
  --dependencies "$dependency_root" \
  --dependency-manifest "$manifest" \
  --wave 0 --range-begin 234 --range-end 235 \
  --full --aws-execution-ack EC2 \
  --workers 7 --dry-run-samples 20000 \
  --scratch-limit 103079215104 \
  --resident-limit 51539607552 \
  --reverse-edge-bytes-limit 68719476736 \
  --minimum-free-bytes 274877906944 \
  --monitor-interval 10 \
  --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/results/angel-copycat-v10-main
