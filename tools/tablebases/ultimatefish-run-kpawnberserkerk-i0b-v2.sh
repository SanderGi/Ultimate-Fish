#!/usr/bin/env bash
# Retry same K+Pawn+Berserker-v-K after v1's narrowly exceeded 64-GiB RSS gate.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
root=/mnt/ultimatefish-jg/pawn-berserker-same-v2
source_root=$root/source
dependencies=$root/dependencies
work=$root/work

test ! -e "$root"
test -s /mnt/ultimatefish-jg/pawn-berserker-same-v1/work/logs/generate/kpawnberserkerk.log
mkdir -p "$source_root" "$dependencies" "$root/promotion-restore"

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
  --key staging/concrete/dependencies/sha256/42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844/kpawnk.uftb \
  --version-id LpBPCebQup_Fveb_lDZhB1JP2CipLz0J \
  "$dependencies/kpawnk.uftb" >/dev/null
aws s3api get-object --no-cli-pager --bucket "$bucket" \
  --key staging/concrete/dependencies/sha256/1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3/kqk.uftb \
  --version-id hNATeBXWgoy_Ry1.KuGPWeoBWgsTWPMg \
  "$dependencies/kqk.uftb" >/dev/null
aws s3api get-object --no-cli-pager --bucket "$bucket" \
  --key results/concrete/penguin-causal-1083b6f8/concrete/v2/model/ac9b2d323aeca8705b79c8cb720242985f82ec24fc561ef6b3417d5d603948e6/wave-0/sha256/188b85277444a880806efa7862ec58f1397e1d22249776edd2d6634d9afbaf09/kqueenberserkerk-188b85277444a880806efa7862ec58f1397e1d22249776edd2d6634d9afbaf09.tar.zst \
  --version-id 7ILiSnEFFu5zGlJjntp9Ucb6brvuUTtr \
  "$root/kqueenberserkerk.tar.zst" >/dev/null
test "$(sha256sum "$root/kqueenberserkerk.tar.zst" | cut -d' ' -f1)" = \
  188b85277444a880806efa7862ec58f1397e1d22249776edd2d6634d9afbaf09
zstd -dc "$root/kqueenberserkerk.tar.zst" | \
  tar -xf - -C "$root/promotion-restore"
cp "$root/promotion-restore/tablebases/kqueenberserkerk.uftb" \
  "$dependencies/kqueenberserkerk.uftb"

test "$(sha256sum "$dependencies/kberserkerk.uftb" | cut -d' ' -f1)" = \
  f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test "$(sha256sum "$dependencies/kpawnk.uftb" | cut -d' ' -f1)" = \
  42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
test "$(sha256sum "$dependencies/kqk.uftb" | cut -d' ' -f1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "$dependencies/kqueenberserkerk.uftb" | cut -d' ' -f1)" = \
  0dcdc7fcc0f534cd8bcc8a6d8cb57117590cfa60fe984b7a5efb03e85eedcd1b

/usr/bin/python3 -c 'import json,pathlib; p=pathlib.Path(__import__("sys").argv[1]); files=[("kberserkerk.uftb",12324040,"f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1"),("kpawnk.uftb",2464840,"42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844"),("kqk.uftb",1232440,"1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3"),("kqueenberserkerk.uftb",474474056,"0dcdc7fcc0f534cd8bcc8a6d8cb57117590cfa60fe984b7a5efb03e85eedcd1b")]; p.write_text(json.dumps({"schema":"ultimate-concrete-k2-dependencies-v2","files":[{"filename":n,"bytes":b,"sha256":s} for n,b,s in files]},indent=2,sort_keys=True)+"\n")' \
  "$dependencies/manifest.json"

test "$(df --output=avail -B1 "$root" | tail -1)" -ge 322122547200
exec /usr/bin/python3 \
  "$source_root/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" \
  --work-directory "$work" \
  --dependencies "$dependencies" \
  --dependency-manifest "$dependencies/manifest.json" \
  --wave 1 --range-begin 6 --range-end 7 --full \
  --aws-execution-ack EC2 \
  --scratch-limit 171798691840 \
  --resident-limit 103079215104 \
  --reverse-edge-bytes-limit 137438953472 \
  --minimum-free-bytes 214748364800 \
  --workers 8 \
  --s3-prefix results/current-concrete-d64b5cdf/pawn-berserker-same-v2
