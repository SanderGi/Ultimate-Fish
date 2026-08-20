#!/usr/bin/env bash
# Resume the exact same-team Berserker/Angel frontier with parallel reverse scan.
set -euo pipefail

source_root=/mnt/ultimatefish/angel-parallel-resume-v7-source-8f9139d0
stage_root=/mnt/ultimatefish/angel-parallel-resume-v7-stage-8f9139d0
manifest=/mnt/ultimatefish/angel-graph-v6-resume-manifests/kberserkerangelk-v2-to-v5.json
work=/mnt/ultimatefish/angel-graph-v7-parallel-resume-work/kberserkerangelk-v7
runner="$source_root/tools/tablebases/resume_ultimate_concrete_frontier_aws.py"
binary="$source_root/binary/ultimate_tablebase"
equivalence="$stage_root/equivalence-2f9a6672.json"

test ! -e "$work"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = \
  25cdb021fcbed13ab080b4373a20af5b4cb813d61a7fbaa9ac7d8bfd714d7faa
test "$(sha256sum "$runner" | cut -d' ' -f1)" = \
  1bf7afa741c5cfc73543344cc862f70f2a89ba71f9a3d569a6f177f46f697099
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  2662cb223b79c8d815276f27e282bb6846c01c0016354857f09c12a185c27b5c
test "$(sha256sum "$equivalence" | cut -d' ' -f1)" = \
  2f9a6672b28210075be1220eb0cc96967f0e457c59b55e4186880c07852de501
test "$(sha256sum "$manifest" | cut -d' ' -f1)" = \
  c1544365463bab93fb63388d8565f8d38f7052b8a0b654337fbadd9f9c35b243
test "$(cd "$source_root" && /usr/bin/python3 -c \
  'import sys;sys.path.insert(0,"tools/tablebases");import run_ultimate_concrete_tablebase_shard_aws as r;print(r.generator_model_sha256())')" = \
  af60c54fae9c55116ac9ee8a368bf4804c89d60390c5b17ff5b358f89573299d

exec /usr/bin/python3 "$runner" \
  --manifest "$manifest" \
  --work-directory "$work" \
  --full --aws-execution-ack EC2 --workers 7 \
  --execution-bundle-root "$source_root" \
  --execution-binary "$binary" \
  --execution-model-sha256 af60c54fae9c55116ac9ee8a368bf4804c89d60390c5b17ff5b358f89573299d \
  --execution-inventory-sha256 46dbf01ff6b8e80a7bc0b7ecce66340944fdec4aae197e46d81e95d1d5fa289d \
  --execution-binary-sha256 2662cb223b79c8d815276f27e282bb6846c01c0016354857f09c12a185c27b5c \
  --execution-equivalence-certificate "$equivalence" \
  --execution-equivalence-sha256 2f9a6672b28210075be1220eb0cc96967f0e457c59b55e4186880c07852de501 \
  --resident-limit 193273528320 \
  --scratch-limit 214748364800 \
  --reverse-edge-bytes-limit 150323855360 \
  --minimum-free-bytes 137438953472 \
  --minimum-host-memory-available-bytes 236223201280 \
  --monitor-interval 10 \
  --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/results/angel-graph-v7-parallel-resume
