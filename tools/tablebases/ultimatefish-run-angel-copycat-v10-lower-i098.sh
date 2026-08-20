#!/usr/bin/env bash
# Generate and S3-verify the arbitrary linked-Copycat lower table on i098.
set -euo pipefail

source_root=/mnt/ultimatefish/angel-copycat-v10-source-393507ea
dependency_root=/mnt/ultimatefish/angel-copycat-v10-empty-dependencies
work_root=/mnt/ultimatefish/angel-copycat-v10-lower-work/kcopycatlinkedk-v1
manifest=$source_root/tools/tablebases/empty_concrete_dependencies.json

test -d "$source_root"
test -d "$dependency_root"
test -f "$manifest"
test ! -e "$work_root"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = \
  57b3eff579237992c9b3f221fad76cc794c3f9c75e860cb195cb4964cdd43ca2
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = \
  2ec59976d2a9591ea3fb6e485de9a4947840b3402d013a6ac53d715e8752b34a
test "$(sha256sum "$manifest" | cut -d' ' -f1)" = \
  21bda84ff220e46ecd6b258b249ee074974d48c09dda98ff980443f823e230b1

cd "$source_root"
exec /usr/bin/python3 tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py \
  --work-directory "$work_root" \
  --dependencies "$dependency_root" \
  --dependency-manifest "$manifest" \
  --bootstrap-linked-copycat \
  --full --aws-execution-ack EC2 \
  --workers 7 --dry-run-samples 20000 \
  --scratch-limit 25769803776 \
  --resident-limit 21474836480 \
  --reverse-edge-bytes-limit 12884901888 \
  --minimum-free-bytes 137438953472 \
  --monitor-interval 10 \
  --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/results/angel-copycat-v10-lower
