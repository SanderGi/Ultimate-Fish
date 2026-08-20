#!/usr/bin/env bash
# Stage the exact Same Copycat/Angel v10 source on the idle i098 host.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
region=us-west-2
source_key=sources/bundles/angel-copycat-v10/sha256/393507ea88fdec8ee68336927e0166363143d80fb0b494bd78f9950c3c420ad8/ultimatefish-angel-copycat-v10-source.tar
source_version=GurygO1.0H3Zwbd.4MwYbuRDIIgVrjhZ
source_sha=393507ea88fdec8ee68336927e0166363143d80fb0b494bd78f9950c3c420ad8
source_bytes=787456
model_sha=a04cc96d0b907eec3fa87f3daf6d51738b39923ccff32f9e77195aa2352b30ab
inventory_sha=1bd9a88cccac9f086b48f010b3e6faec134309efc04fe7ad6d8f085a9abce199
tablebase_sha=57b3eff579237992c9b3f221fad76cc794c3f9c75e860cb195cb4964cdd43ca2
probe_sha=e17573a1b0aa01bbb232a2144903b300c8945c8efb64d862da919dd14dc5449c
runner_sha=2ec59976d2a9591ea3fb6e485de9a4947840b3402d013a6ac53d715e8752b34a
manifest_sha=21bda84ff220e46ecd6b258b249ee074974d48c09dda98ff980443f823e230b1

source_root=/mnt/ultimatefish/angel-copycat-v10-source-393507ea
dependency_root=/mnt/ultimatefish/angel-copycat-v10-empty-dependencies
stage_root=/mnt/ultimatefish/angel-copycat-v10-stage-393507ea

test ! -e "$source_root"
test ! -e "$dependency_root"
test ! -e "$stage_root"
install -d -m 0755 "$source_root" "$dependency_root" "$stage_root"

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
test "$actual_model" = "$model_sha"
test "$actual_inventory" = "$inventory_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase.cpp" | cut -d' ' -f1)" = "$tablebase_sha"
test "$(sha256sum "$source_root/src/ultimate/tablebases/tablebase_probe.cpp" | cut -d' ' -f1)" = "$probe_sha"
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d' ' -f1)" = "$runner_sha"
test "$(sha256sum "$source_root/tools/tablebases/empty_concrete_dependencies.json" | cut -d' ' -f1)" = "$manifest_sha"

chmod -R a-w "$source_root" "$dependency_root"
echo "ANGEL_COPYCAT_V10_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha manifest=$manifest_sha"
