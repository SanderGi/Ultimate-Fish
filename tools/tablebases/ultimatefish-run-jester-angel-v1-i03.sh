#!/usr/bin/env bash
# Generate and S3-verify both concrete Jester/Angel source tables on i03.
set -euo pipefail

source_root=/mnt/ultimatefish/jester-angel-v1-source-34922731
dependency_root=/mnt/ultimatefish/jester-angel-v1-dependencies-v2-900622da
work_root=/mnt/ultimatefish-penguin/jester-angel-v1-source-work
manifest=$dependency_root/manifest.json

test -d "$source_root"
test -d "$dependency_root"
test -f "$manifest"
test ! -e "$work_root"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = \
  3bcdc19ec3c1da4f0d37f0027b9c5d52362a57ac6d335a46ce1cd09b54ee1c5c
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = \
  5f675fcaa9934a13abbba0adac3c4f3223b762165a2fba37b2bca03e79c16a5a
test "$(sha256sum "$manifest" | cut -d' ' -f1)" = \
  900622dae473a0f06d77b87d832e6cc735e05a270dbd6597fb0045e3ac44c28f
test "$(sha256sum "$dependency_root/kjesterk.uftb" | cut -d' ' -f1)" = \
  3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa

cd "$source_root"
exec /usr/bin/python3 tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py \
  --work-directory "$work_root" \
  --dependencies "$dependency_root" \
  --dependency-manifest "$manifest" \
  --wave 0 --range-begin 238 --range-end 240 \
  --full --aws-execution-ack EC2 \
  --workers 16 --dry-run-samples 20000 \
  --scratch-limit 68719476736 \
  --resident-limit 51539607552 \
  --reverse-edge-bytes-limit 51539607552 \
  --minimum-free-bytes 64424509440 \
  --monitor-interval 10 \
  --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/results/jester-angel-v1-source
