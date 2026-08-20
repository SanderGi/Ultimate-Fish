#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-singleton-resume-v1/kghostpenguink-fixed-point-v1
tool=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source/tools/tablebases/archive_ultimate_aws_result.py
output=${root}/archive-source-order-v5
manifest=${output}/artifact-manifest.json

test "$(sha256sum "${tool}" | cut -d ' ' -f 1)" = \
  7ce682e83eb234fe5d5391a8f655b4995df7ed13a055f9dda6f040a914a66fec
test "$(sha256sum "${root}/work/results/kghostpenguink-resume-v4.ufiw" | cut -d ' ' -f 1)" = \
  1a326424c3569058ccba57ef3da8c6ae36188ce481714abd5fd2c8ef17b925ef
test "$(sha256sum "${root}/work/results/kghostpenguink-resume-v4.ufgd" | cut -d ' ' -f 1)" = \
  6b2b85d2a80c2ed4d59de40f2acf5e5a5e057c1c60d59546b562cd6d701f45d6
test "$(sha256sum "${root}/work/logs/resume-fixed-point-v4.log" | cut -d ' ' -f 1)" = \
  9a835c99aa78a4a322c172c83b32faca39a56aabfada6bd7747ebc3c99160b89
grep -Fq \
  'information_symbolic_certificate iterations 62 bdd_nodes 82528399 bellman_residual 0 monotonicity_residual 0 singleton_residual 0 compaction_root_residual 0 belief_cap none powerset_exact 1' \
  "${root}/work/logs/resume-fixed-point-v4.log"
grep -Fq \
  'dragon_ghost_certificate dual_force_residual 0 structural_residual 0 singleton_residual 0 source_remap_residual 0' \
  "${root}/work/logs/resume-fixed-point-v4.log"
test ! -e "${output}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 85899345920

install -d "${output}"
python3 - "${root}" "${manifest}" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
manifest = Path(sys.argv[2])
critical = {
    "work/transitions/kghostpenguink.header":
        "70fdcafad883f225fddfc8f4298d23fc517fd946dbde134a385b30668ea17f59",
    "work/transitions/kghostpenguink.meta":
        "94231d25971081bec979ab1df54c2a26f20999acfddbd7e415dd5adc08f1b3c5",
    "work/transitions/kghostpenguink.strata":
        "57f4b71cf2c37f1915a060de8c64ec4e07e4a0fda1f1714804fd4cbd99dff26a",
    "work/transitions/kghostpenguink.index":
        "b1f9343e25b2c3514beed587ab6895700d9bdd547deae2a49f6c3fd1c175eff0",
    "work/transitions/kghostpenguink.blocks":
        "e6ff06ce9adeabcec418c8698998aa0457aa18d3a0910e33c485ef9a43621de8",
    "work/transitions/kghostpenguink.verified":
        "550b0eae427b8f7bf5edd7fc687136ea613ab4c795fb16bec8dd88d832a62cfa",
    "work/results/kghostpenguink-resume-v4.ufiw":
        "1a326424c3569058ccba57ef3da8c6ae36188ce481714abd5fd2c8ef17b925ef",
    "work/results/kghostpenguink-resume-v4.ufgd":
        "6b2b85d2a80c2ed4d59de40f2acf5e5a5e057c1c60d59546b562cd6d701f45d6",
    "work/logs/resume-fixed-point-v4.log":
        "9a835c99aa78a4a322c172c83b32faca39a56aabfada6bd7747ebc3c99160b89",
}
paths = set(critical)
paths.update(path.relative_to(root).as_posix()
             for path in (root / "work/logs").glob("*.log"))
artifacts = []
for relative in sorted(paths):
    path = root / relative
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(8 * 1024 * 1024):
            digest.update(block)
    actual = digest.hexdigest()
    if relative in critical and actual != critical[relative]:
        raise SystemExit(f"critical artifact mismatch: {relative}")
    artifacts.append({
        "path": relative,
        "bytes": path.stat().st_size,
        "sha256": actual,
    })
payload = {
    "schema": "ultimate-penguin-ghost-source-order-v1",
    "filename": "kghostpenguink.uftb",
    "orientation": "same",
    "source_sha256":
        "e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4",
    "normalized_source_sha256":
        "729f67aaa74d79c44fa375d78b5ef51a0e120f44a48bb67b8b2d0d1e01d5998a",
    "model_sha256":
        "3b9fdf33ca878d18e2a3bea7c3c6a591aaf9c6a453a9e649b0f87784085cbc8b",
    "observation_sha256":
        "6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b",
    "lower_penguin_sha256":
        "5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd",
    "lower_penguin_model_sha256":
        "3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84",
    "lower_ghost_sidecar_sha256":
        "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb",
    "lower_ghost_source_sha256":
        "11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5",
    "lower_ghost_model_sha256":
        "ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5",
    "lower_ghost_observation_sha256":
        "890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23",
    "transition_payload_sha256":
        "d496c46d58d83b2218592510e80fef3b5ba7111b91def728ff6e16e4a6405cdc",
    "arbitrary_sha256":
        "6b2b85d2a80c2ed4d59de40f2acf5e5a5e057c1c60d59546b562cd6d701f45d6",
    "iterations": 62,
    "bdd_nodes": 82528399,
    "bellman_residual": 0,
    "rank_residual": 0,
    "artifacts": artifacts,
}
manifest.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")
PY

/usr/bin/python3 "${tool}" \
  --root "${root}" \
  --artifact-manifest "${manifest}" \
  --kind kghostpenguink-source-order-v4-3b9fdf33 \
  --output-dir "${output}" \
  --s3-prefix s3://ultimatefish-info-20260808-a4e679c6-831688117652/ \
  --zstd-level 9 > "${output}/receipt.json.tmp"
mv "${output}/receipt.json.tmp" "${output}/receipt.json"
