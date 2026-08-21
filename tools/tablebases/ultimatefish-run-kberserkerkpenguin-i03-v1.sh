#!/usr/bin/env bash
# Generate and preserve opposing K+Berserker-v-K+Penguin once i03 CPUs free.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
root=/mnt/ultimatefish/berserker-penguin-opposed-v1
source_root=$root/source
dependencies=$root/dependencies
work=$root/work

test ! -e "$root"
mkdir -p "$source_root" "$dependencies"

aws s3api get-object --no-cli-pager --bucket "$bucket" \
  --key sources/bundles/jester-angel-v1/sha256/20db30fcf7fd2dbc19bbbd1a1a860c3b06765551150ef8b0736b78d15b2dfe97/ultimatefish-jester-angel-v1-source.tar \
  --version-id XJuPcn2ij3cr2OWIApeI0B9ORQP.Z5WJ \
  "$root/source.tar" >/dev/null
test "$(sha256sum "$root/source.tar" | cut -d' ' -f1)" = \
  20db30fcf7fd2dbc19bbbd1a1a860c3b06765551150ef8b0736b78d15b2dfe97
test "$(stat -c %s "$root/source.tar")" = 768000
tar -xf "$root/source.tar" -C "$source_root"

aws s3api get-object --no-cli-pager --bucket "$bucket" \
  --key sources/dependencies/legacy-concrete/sha256/f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1/kberserkerk.uftb \
  --version-id mRRAjmezTZfm6VYBs5bRrAMM4Z1yeqmx \
  "$dependencies/kberserkerk.uftb" >/dev/null
aws s3api get-object --no-cli-pager --bucket "$bucket" \
  --key sources/dependencies/legacy-concrete/sha256/5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd/kpenguink.uftb \
  --version-id iKra4qhe7X2JlXtgJ1VORJz6C1M45ZRr \
  "$dependencies/kpenguink.uftb" >/dev/null
test "$(sha256sum "$dependencies/kberserkerk.uftb" | cut -d' ' -f1)" = \
  f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test "$(sha256sum "$dependencies/kpenguink.uftb" | cut -d' ' -f1)" = \
  5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd

/usr/bin/python3 -c 'import json,pathlib; p=pathlib.Path(__import__("sys").argv[1]); files=[("kberserkerk.uftb",12324040,"f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1"),("kpenguink.uftb",4929640,"5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd")]; p.write_text(json.dumps({"schema":"ultimate-concrete-k2-dependencies-v2","files":[{"filename":n,"bytes":b,"sha256":s} for n,b,s in files]},indent=2,sort_keys=True)+"\n")' \
  "$dependencies/manifest.json"

test "$(df --output=avail -B1 "$root" | tail -1)" -ge 1202590842880
exec /usr/bin/python3 \
  "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" \
  --work-directory "$work" \
  --dependencies "$dependencies" \
  --dependency-manifest "$dependencies/manifest.json" \
  --wave 0 --range-begin 17 --range-end 18 --full \
  --aws-execution-ack EC2 \
  --scratch-limit 966367641600 \
  --resident-limit 68719476736 \
  --reverse-edge-bytes-limit 858993459200 \
  --minimum-free-bytes 214748364800 \
  --workers 8 \
  --s3-prefix results/current-concrete-d64b5cdf/berserker-penguin-opposed-v1
