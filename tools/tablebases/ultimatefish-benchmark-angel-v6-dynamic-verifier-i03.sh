#!/usr/bin/env bash
# Recompute same Pawn/Angel to prove dynamic verification is byte-identical.
set -euo pipefail

source_root=/mnt/ultimatefish/angel-graph-v6-source-7fbebde7
dependencies=/mnt/ultimatefish/angel-graph-v5-dependencies-990832f4
manifest=/mnt/ultimatefish/angel-graph-v5-stage-17da6cea/manifest-pawn-angel-same-v1.json
work=/mnt/ultimatefish/angel-graph-v6-benchmark/kpawnangelk-v1
unit=ultimatefish-angel-v6-kpawnangelk-equivalence-v1.service
runner="$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py"

test -f "$runner"
test -f "$dependencies/kpawnk.uftb"
test -f "$dependencies/kqueenangelk.uftb"
test -f "$manifest"
test ! -e "$work"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = 87b0f090ecab3918c3bcf1232051de72ea823e4549b5e9be3b1184c0464aafdd
test "$(sha256sum "$manifest" | cut -d' ' -f1)" = b7953747687e0a10d7069bc50cc469388fdefa4fb265b198adb755ff7aa40db2

systemd-run --unit "$unit" \
  --property=AllowedCPUs=24-29 \
  --property=MemoryMax=32212254720 \
  --property=CPUQuota=600% \
  --property=Nice=5 \
  /usr/bin/python3 "$runner" \
    --work-directory "$work" \
    --dependencies "$dependencies" \
    --dependency-manifest "$manifest" \
    --wave 1 --range-begin 40 --range-end 41 --workers 6 \
    --full --aws-execution-ack EC2 \
    --scratch-limit 51539607552 \
    --resident-limit 21474836480 \
    --reverse-edge-bytes-limit 17179869184 \
    --minimum-free-bytes 137438953472 \
    --monitor-interval 10 \
    --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/benchmarks/angel-graph-v6-dynamic-verifier

sleep 2
test "$(systemctl show "$unit" -p LoadState --value)" = loaded
test "$(systemctl show "$unit" -p ActiveState --value)" = active
echo "ANGEL_V6_DYNAMIC_VERIFIER_BENCHMARK_STARTED unit=$unit work=$work expected_output_sha256=d1296b7db6862c2306c2f693c22357c49b5fe7d1a3d46cb05aad1b248cfb9741"
