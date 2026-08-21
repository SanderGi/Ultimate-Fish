#!/usr/bin/env python3
"""Audit retained UFIW2 overlays against their exact concrete UFTBs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


PIECES = (
    "king", "jester", "knight", "pawn", "queen", "rook", "bishop",
    "berserker", "bomb", "ninja", "turtle", "ghost", "mage", "goop",
    "penguin", "parasite", "devil", "minion", "sludge", "sniper",
    "prince", "checker", "checkerking", "giant", "copycat",
    "copycatclone", "angel", "halo", "fisherman", "dragon", "count",
)
COUNT_LINE = re.compile(
    r"^(information_reachability_(?:admitted|excluded|trivial)) side ([01]) "
    r"unknown ([0-9]+) win ([0-9]+) loss ([0-9]+) draw ([0-9]+)$")
SUBSTATE_COUNT_LINE = re.compile(
    r"^(information_reachability_substate_(?:admitted|excluded|trivial)) "
    r"substate ([0-9]+) side ([01]) unknown ([0-9]+) win ([0-9]+) "
    r"loss ([0-9]+) draw ([0-9]+)$")
TRANSPOSED_SUBSTATE_OVERLAYS = {
    (
        "kghostpenguink",
        "e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4",
        "3b9fdf33ca878d18e2a3bea7c3c6a591aaf9c6a453a9e649b0f87784085cbc8b",
    ),
}


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(4 << 20):
            digest.update(chunk)
    return digest.hexdigest()


def overlay_header(path: Path) -> dict[str, object]:
    with path.open("rb") as stream:
        header = stream.read(160)
        stream.seek(0, 2)
        extent = stream.tell()
    if len(header) != 160:
        raise ValueError(f"truncated UFIW2 header: {path}")
    magic, version, primary, secondary, color, states, substates = \
        struct.unpack_from("<8s6I", header)
    if (magic != b"UFIW2\0\0\0" or version != 2 or
            primary >= len(PIECES) or secondary >= len(PIECES) or
            color > 1 or extent != 160 + states):
        raise ValueError(f"invalid UFIW2 header/extent: {path}")
    source = header[32:96].decode()
    model = header[96:160].decode()
    if not all(re.fullmatch(r"[0-9a-f]{64}", value)
               for value in (source, model)):
        raise ValueError(f"invalid UFIW2 hashes: {path}")
    return {
        "primary": PIECES[primary], "secondary": PIECES[secondary],
        "color": color, "states": states, "substates": substates,
        "source_sha256": source, "model_sha256": model,
    }


def source_header(path: Path) -> dict[str, object]:
    with path.open("rb") as stream:
        header = stream.read(64)
    if len(header) < 48 or header[:8] != b"UFTB1\0\0\0":
        raise ValueError(f"invalid UFTB header: {path}")
    version, primary, states = struct.unpack_from("<3I", header, 8)
    substates = struct.unpack_from("<I", header, 24)[0]
    if version < 1 or version > 7 or primary >= len(PIECES):
        raise ValueError(f"unsupported UFTB codec: {path}")
    secondary, color = ((len(PIECES) - 1, 0) if version < 5 else
                        struct.unpack_from("<2I", header, 40))
    if secondary >= len(PIECES) or color > 1:
        raise ValueError(f"invalid UFTB material: {path}")
    return {"primary": PIECES[primary], "secondary": PIECES[secondary],
            "color": color, "states": states, "substates": substates}


def parse_counts(output: str) -> dict[str, dict[str, dict[str, int]]]:
    result: dict[str, dict[str, dict[str, int]]] = {}
    for line in output.splitlines():
        match = COUNT_LINE.fullmatch(line)
        if not match:
            continue
        label, side, unknown, win, loss, draw = match.groups()
        bucket = label.rsplit("_", 1)[1]
        result.setdefault(side, {})[bucket] = {
            "unknown": int(unknown), "win": int(win),
            "loss": int(loss), "draw": int(draw),
        }
    if set(result) != {"0", "1"} or any(
            set(side) != {"admitted", "excluded", "trivial"}
            for side in result.values()):
        raise ValueError("information trivial auditor output is incomplete")
    for side in result.values():
        if any(side[name]["unknown"] for name in side):
            raise ValueError("information trivial auditor reported unknown WDL")
        for outcome in ("win", "loss", "draw"):
            if side["trivial"][outcome] > side["admitted"][outcome]:
                raise ValueError("information trivial subset residual")
    return result


def parse_substate_counts(
    output: str, substates: int,
    aggregate: dict[str, dict[str, dict[str, int]]],
) -> dict[str, dict[str, dict[str, dict[str, int]]]]:
    result: dict[str, dict[str, dict[str, dict[str, int]]]] = {}
    for line in output.splitlines():
        match = SUBSTATE_COUNT_LINE.fullmatch(line)
        if not match:
            continue
        label, substate, side, unknown, win, loss, draw = match.groups()
        bucket = label.rsplit("_", 1)[1]
        result.setdefault(substate, {}).setdefault(side, {})[bucket] = {
            "unknown": int(unknown), "win": int(win),
            "loss": int(loss), "draw": int(draw),
        }
    if set(result) != {str(value) for value in range(substates)}:
        raise ValueError("information trivial substate coverage residual")
    for substate in result.values():
        if set(substate) != {"0", "1"} or any(
                set(side) != {"admitted", "excluded", "trivial"}
                for side in substate.values()):
            raise ValueError("information trivial substate output is incomplete")
        for side in substate.values():
            if any(bucket["unknown"] for bucket in side.values()):
                raise ValueError("information trivial substate reported unknown WDL")
            for outcome in ("win", "loss", "draw"):
                if side["trivial"][outcome] > side["admitted"][outcome]:
                    raise ValueError("information trivial substate subset residual")
    for side in ("0", "1"):
        for bucket in ("admitted", "excluded", "trivial"):
            for outcome in ("unknown", "win", "loss", "draw"):
                total = sum(
                    result[str(substate)][side][bucket][outcome]
                    for substate in range(substates)
                )
                if total != aggregate[side][bucket][outcome]:
                    raise ValueError(
                        "information trivial substate conservation residual")
    return result


def put_object(bucket: str, key: str, path: Path,
               metadata: dict[str, str]) -> str:
    command = [
        "aws", "s3api", "put-object", "--bucket", bucket, "--key", key,
        "--body", str(path), "--metadata",
        ",".join(f"{name}={value}" for name, value in metadata.items()),
    ]
    response = json.loads(subprocess.check_output(command, text=True))
    version = response.get("VersionId")
    if not version:
        raise RuntimeError("S3 did not return a VersionId")
    return str(version)


def audit(args: argparse.Namespace, overlay: Path) -> dict[str, object]:
    header = overlay_header(overlay)
    source = args.source_dir / f"{overlay.stem}.uftb"
    if not source.is_file() or source.stat().st_size <= 1024:
        raise FileNotFoundError(f"missing concrete source: {source}")
    source_sha = sha256_path(source)
    overlay_sha = sha256_path(overlay)
    if source_sha != header["source_sha256"]:
        raise ValueError(f"source SHA mismatch for {overlay.stem}")
    material = source_header(source)
    if (material["states"] != header["states"] or
            material["substates"] != header["substates"]):
        raise ValueError(f"source/overlay codec mismatch for {overlay.stem}")
    command = [
        str(args.binary), "--piece", str(material["primary"]),
        "--workers", str(args.workers),
        "--audit-information-trivial", str(source),
        "--information-overlay", str(overlay),
        "--information-source-sha256", source_sha,
        "--information-model-sha256", str(header["model_sha256"]),
    ]
    if material["secondary"] != "count":
        command += ["--piece2", str(material["secondary"])]
        if material["color"] == 1:
            command.append("--opposing")
    transpose_key = (overlay.stem, source_sha, str(header["model_sha256"]))
    if transpose_key in TRANSPOSED_SUBSTATE_OVERLAYS:
        if (material["primary"], material["secondary"],
                material["substates"]) != ("ghost", "penguin", 16):
            raise ValueError("Ghost/Penguin transpose material residual")
        command.append("--information-transpose-substates")
    completed = subprocess.run(command, check=True, text=True,
                               capture_output=True)
    counts = parse_counts(completed.stdout)
    substate_counts = parse_substate_counts(
        completed.stdout, int(header["substates"]), counts)
    if ("prince" in (str(material["primary"]), str(material["secondary"])) and
            completed.stdout.count(
                "information_reachability_scope turn_boundary\n") != 1):
        raise ValueError(f"{overlay.stem}: Prince boundary scope residual")
    sidecar = args.output_dir / f"{overlay.stem}.information-trivial-v2.txt"
    sidecar.write_text(
        completed.stdout +
        f"information_trivial_overlay_sha256 {overlay_sha}\n"
        f"information_trivial_binary_sha256 {args.binary_sha256}\n"
        f"information_trivial_source_bundle_sha256 "
        f"{args.source_bundle_sha256}\n")
    sidecar_sha = sha256_path(sidecar)
    key = (f"{args.prefix}/{overlay.stem}.uftb/sha256/{sidecar_sha}/"
           f"{sidecar.name}")
    version = put_object(args.bucket, key, sidecar, {
        "sha256": sidecar_sha, "source-sha256": source_sha,
        "overlay-sha256": overlay_sha,
        "binary-sha256": args.binary_sha256,
    })
    receipt = {
        "schema": "ultimate-information-trivial-receipt-v2",
        "filename": f"{overlay.stem}.uftb", "states": header["states"],
        "substates": header["substates"], "source_sha256": source_sha,
        "model_sha256": header["model_sha256"],
        "overlay_sha256": overlay_sha,
        "binary_sha256": args.binary_sha256,
        "source_bundle_sha256": args.source_bundle_sha256,
        "sidecar_sha256": sidecar_sha, "s3_key": key,
        "s3_version_id": version, "counts": counts,
        "substate_counts": substate_counts,
    }
    receipt_path = args.output_dir / f"{overlay.stem}.receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--source-bundle-sha256", required=True)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--overlay-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--prefix", default="results/information-trivial-v2")
    parser.add_argument("--shard", type=int, required=True)
    parser.add_argument("--shards", type=int, required=True)
    parser.add_argument("--workers", type=int, default=1)
    args = parser.parse_args()
    if (not re.fullmatch(r"[0-9a-f]{64}", args.binary_sha256) or
            not re.fullmatch(r"[0-9a-f]{64}", args.source_bundle_sha256) or
            args.shards <= 0 or not 0 <= args.shard < args.shards or
            not 1 <= args.workers <= 32):
        parser.error("invalid SHA-256 or shard selection")
    if sha256_path(args.binary) != args.binary_sha256:
        raise SystemExit("auditor binary SHA mismatch")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    overlays = sorted(path for path in args.overlay_dir.glob("*.ufiw")
                      if not path.name.startswith("._") and
                      path.stat().st_size > 160)
    selected = overlays[args.shard::args.shards]
    receipts = [audit(args, overlay) for overlay in selected]
    print(json.dumps({"completed": len(receipts),
                      "filenames": [r["filename"] for r in receipts]},
                     sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
