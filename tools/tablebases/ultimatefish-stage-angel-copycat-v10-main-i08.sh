#!/usr/bin/env bash
# Stage exact v10 source plus both Same Copycat/Angel lower dependencies on i08.
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

manifest_key=staging/angel-copycat-v10/dependencies/sha256/51385018e8c7ad83c0ad9e357de31a0f9a6cbedeb4b78618ab64905b42bb3fb7/manifest.json
manifest_version=hlr5e8raVpFcIjbmmFJAtSvBxfFomflp
manifest_sha=51385018e8c7ad83c0ad9e357de31a0f9a6cbedeb4b78618ab64905b42bb3fb7
manifest_bytes=391
lower_key=results/angel-copycat-v10-lower/concrete/v2/model/a04cc96d0b907eec3fa87f3daf6d51738b39923ccff32f9e77195aa2352b30ab/wave-bootstrap-linked-copycat/sha256/815cbabc73491f874735ec3c4ffd7bea6eef95e3dfbbeb19f9a20d0f75c4317b/kcopycatlinkedk-815cbabc73491f874735ec3c4ffd7bea6eef95e3dfbbeb19f9a20d0f75c4317b.tar.zst
lower_version=EExdSq2sJnK0GLE1z256zGS39_r4pehV
lower_archive_sha=815cbabc73491f874735ec3c4ffd7bea6eef95e3dfbbeb19f9a20d0f75c4317b
lower_archive_bytes=6127705
lower_sha=e55924a8b97acedcbc9876b95a23837030d5112a35d2b06ec0cb85ff4ab16dd2
lower_bytes=47447464
intact_sha=98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb
intact_bytes=1232448

source_root=/mnt/ultimatefish/angel-copycat-v10-source-393507ea
dependency_root=/mnt/ultimatefish/angel-copycat-v10-main-dependencies-51385018
stage_root=/mnt/ultimatefish/angel-copycat-v10-main-stage-393507ea
intact_source=/mnt/ultimatefish/angel-graph-v7-dependencies-990832f4/kcopycatk.uftb

test ! -e "$source_root"
test ! -e "$dependency_root"
test ! -e "$stage_root"
test -f "$intact_source"
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

aws s3api get-object --bucket "$bucket" --key "$manifest_key" \
  --version-id "$manifest_version" --region "$region" \
  "$stage_root/manifest.json" >/dev/null
test "$(stat -c %s "$stage_root/manifest.json")" = "$manifest_bytes"
test "$(sha256sum "$stage_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"

aws s3api get-object --bucket "$bucket" --key "$lower_key" \
  --version-id "$lower_version" --region "$region" \
  "$stage_root/lower.tar.zst" >/dev/null
test "$(stat -c %s "$stage_root/lower.tar.zst")" = "$lower_archive_bytes"
test "$(sha256sum "$stage_root/lower.tar.zst" | cut -d' ' -f1)" = "$lower_archive_sha"
(
  cd "$source_root"
  /usr/bin/python3 -c \
    'import sys;from pathlib import Path;sys.path.insert(0,"tools/tablebases");import run_double_jester_information_capture as p;p.restore_zstd_archive(Path(sys.argv[1]),Path(sys.argv[2]),"ultimate-concrete-k2-result-v2")' \
    "$stage_root/lower.tar.zst" "$stage_root/lower-restore"
)

install -m 0444 "$intact_source" "$dependency_root/kcopycatk.uftb"
install -m 0444 "$stage_root/lower-restore/tablebases/kcopycatlinkedk.uftb" \
  "$dependency_root/kcopycatlinkedk.uftb"
install -m 0444 "$stage_root/manifest.json" "$dependency_root/manifest.json"
test "$(stat -c %s "$dependency_root/kcopycatk.uftb")" = "$intact_bytes"
test "$(sha256sum "$dependency_root/kcopycatk.uftb" | cut -d' ' -f1)" = "$intact_sha"
test "$(stat -c %s "$dependency_root/kcopycatlinkedk.uftb")" = "$lower_bytes"
test "$(sha256sum "$dependency_root/kcopycatlinkedk.uftb" | cut -d' ' -f1)" = "$lower_sha"
test "$(sha256sum "$dependency_root/manifest.json" | cut -d' ' -f1)" = "$manifest_sha"

chmod -R a-w "$source_root" "$dependency_root" "$stage_root"
echo "ANGEL_COPYCAT_V10_MAIN_STAGE_OK source=$source_sha model=$model_sha inventory=$inventory_sha manifest=$manifest_sha intact=$intact_sha linked=$lower_sha"
