#!/usr/bin/env bash
# Deterministically preserve the completed same Ghost/Ghost transition graph.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
unit=ultimatefish-info-kghostghostk-transitions-after-stop-v2.service
root=/mnt/ultimatefish/same-ghost-pair-rebuild-after-stop-v1
migration=$root/migration
marker=$root/job/work/transitions-ready.json
archive=$migration/kghostghostk-transitions-v2.tar.zst

while systemctl is-active --quiet "$unit"; do
  sleep 30
done
test "$(systemctl show "$unit" -p ActiveState --value)" = inactive
test "$(systemctl show "$unit" -p Result --value)" = success
test "$(systemctl show "$unit" -p ExecMainStatus --value)" = 0
test -s "$marker"
test ! -e "$migration"
test "$(sha256sum "$root/source.tar.gz" | cut -d' ' -f1)" = \
  5bbb139577ee0d4ee577f85813c98f6f2c8e499ebc8dc39b72dbd1daffd950d0
test "$(sha256sum "$root/job/ultimate_ghost_pair_information_tablebase" | cut -d' ' -f1)" = \
  e5adb01295e596b6d56a028ebbd3e1b9e3d9fed0a5ad555f2d08180f3f37349a
/usr/bin/python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); expected={"schema":"ultimate-ghost-pair-current-transitions-v1","filename":"kghostghostk.uftb","source_sha256":"12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6","model_sha256":"779e91a7df7dc806a472963b1073470d484e7a1b638d71dfc9c8dbe5c839e110","observation_sha256":"890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23","lower_ghost_sha256":"472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb"}; assert p == expected' "$marker"

mkdir -p "$migration"
tar --sparse --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
  --pax-option=delete=atime,delete=ctime -C "$root" \
  -cf - \
  job/ultimate_ghost_pair_information_tablebase \
  job/tablebases/kghostghostk.uftb \
  job/tablebases/kghostk.ufgm \
  job/work/transitions/kghostghostk.header \
  job/work/transitions/kghostghostk.meta \
  job/work/transitions/kghostghostk.strata \
  job/work/transitions/kghostghostk.actual \
  job/work/transitions/kghostghostk.index \
  job/work/transitions/kghostghostk.blocks \
  job/work/transitions/kghostghostk.verified \
  job/work/transitions-ready.json \
  job/work/logs/self-test.log \
  job/work/logs/merge.log \
  source source.tar.gz | zstd -T0 -6 -o "$archive"

archive_sha=$(sha256sum "$archive" | cut -d' ' -f1)
archive_bytes=$(stat -c %s "$archive")
key="results/migrations/same-ghost-pair-transitions-v2/sha256/$archive_sha/kghostghostk-transitions-v2.tar.zst"
aws s3 cp --no-progress "$archive" "s3://$bucket/$key" \
  --metadata "sha256=$archive_sha"
aws s3api head-object --no-cli-pager --bucket "$bucket" --key "$key" \
  >"$migration/archive-head.json"
/usr/bin/python3 -c 'import hashlib,json,pathlib,sys; migration=pathlib.Path(sys.argv[1]); marker=pathlib.Path(sys.argv[2]); key=sys.argv[3]; sha=sys.argv[4]; size=int(sys.argv[5]); head=json.loads((migration/"archive-head.json").read_text()); assert head["ContentLength"] == size and head["Metadata"]["sha256"] == sha; receipt={"schema":"ultimate-ghost-pair-transition-migration-v2","filename":"kghostghostk.uftb","archive":{"bucket":"ultimatefish-info-20260808-a4e679c6-831688117652","key":key,"version_id":head["VersionId"],"bytes":size,"sha256":sha},"transition_marker":{"bytes":marker.stat().st_size,"sha256":hashlib.sha256(marker.read_bytes()).hexdigest()},"source_sha256":"12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6","model_sha256":"779e91a7df7dc806a472963b1073470d484e7a1b638d71dfc9c8dbe5c839e110","observation_sha256":"890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23"}; (migration/"migration-ready.json").write_text(json.dumps(receipt,indent=2,sort_keys=True)+"\n")' "$migration" "$marker" "$key" "$archive_sha" "$archive_bytes"
cat "$migration/migration-ready.json"
