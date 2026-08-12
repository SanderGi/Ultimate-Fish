#!/usr/bin/env python3
"""Verify, publish, and import the retained exact single-Ghost artifacts.

The single-Ghost result is represented by a dense UFIW2 force overlay and an
arbitrary-belief UFGM1 ROBDD.  This command checks every node, geometry,
stratum, reverse map, force flag, extent, and digest binding before publishing
one content-addressed certificate.  It never solves or rewrites the artifacts.
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
PLOT = ROOT / "tools/tablebases/plot_ultimate_tablebases.py"
NO_STRATUM = 0xFFFFFFFF
STATES_PER_SIDE = 985_920


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def verify_model(path: Path) -> dict[str, object]:
    payload = path.read_bytes()
    if len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0":
        raise ValueError("invalid UFGM1 header")
    values = struct.unpack_from("<16I", payload, 8)
    (version, header, piece, owner, files, ranks, squares, concrete,
     substates, geometry_count, stratum_count, node_count, node_bytes,
     geometry_bytes, stratum_bytes, reserved) = values
    expected = (1, 320, 11, 0, 8, 10, 80, 1_971_840, 2)
    if values[:9] != expected or values[12:] != (9, 844, 18, 0):
        raise ValueError("UFGM1 metadata residual")
    node_offset, geometry_offset, stratum_offset = \
        struct.unpack_from("<3Q", payload, 72)
    if (node_offset != 320 or
            geometry_offset != node_offset + node_count * node_bytes or
            stratum_offset != geometry_offset +
            geometry_count * geometry_bytes or
            len(payload) != stratum_offset + stratum_count * stratum_bytes):
        raise ValueError("UFGM1 section/extent residual")
    concrete_sha = payload[96:160].decode()
    model_sha = payload[160:224].decode()
    observation_sha = payload[224:288].decode()
    if (any(len(value) != 64 for value in
            (concrete_sha, model_sha, observation_sha)) or
            payload[288:320].rstrip(b"\0") !=
            b"history-mask-public-view-v2"):
        raise ValueError("UFGM1 digest/semantics residual")

    variables = bytearray(node_count)
    unique: set[int] = set()
    for index in range(node_count):
        offset = node_offset + index * node_bytes
        variable = payload[offset]
        low, high = struct.unpack_from("<II", payload, offset + 1)
        variables[index] = variable
        if index < 2:
            if (variable, low, high) != (80, index, index):
                raise ValueError("UFGM1 terminal residual")
            continue
        if (variable >= 80 or low >= index or high >= index or low == high or
                (variables[low] if low > 1 else 80) <= variable or
                (variables[high] if high > 1 else 80) <= variable):
            raise ValueError("UFGM1 ROBDD ordering/reduction residual")
        key = variable | (low << 7) | (high << 39)
        if key in unique:
            raise ValueError("UFGM1 duplicate ROBDD node")
        unique.add(key)

    geometry_codes: set[int] = set()
    actual_maps: list[tuple[int, ...]] = []
    live_masks: list[int] = []
    terminal_masks: list[int] = []
    visible_flags: list[int] = []
    kings: list[tuple[int, int]] = []
    for index in range(geometry_count):
        offset = geometry_offset + index * geometry_bytes
        side, owner_king, observer_king, visible = payload[offset:offset + 4]
        mask = lambda at: int.from_bytes(payload[at:at + 10], "little")
        live = mask(offset + 4)
        terminal = mask(offset + 14)
        terminal_owner = mask(offset + 24)
        terminal_observer = mask(offset + 34)
        actual_map = struct.unpack_from("<80I", payload, offset + 44)
        roots = struct.unpack_from("<80I", payload, offset + 364)
        visibility = payload[offset + 684:offset + 844]
        code = side | (owner_king << 1) | (observer_king << 8) | \
            (visible << 15)
        if (side > 1 or owner_king >= 80 or observer_king >= 80 or
                owner_king == observer_king or visible > 1 or
                terminal_owner & ~terminal or terminal_observer & ~terminal or
                live & terminal or terminal_owner & terminal_observer or
                any(root >= node_count for root in roots) or
                any(value > 1 for value in visibility) or
                code in geometry_codes):
            raise ValueError("UFGM1 geometry residual")
        geometry_codes.add(code)
        actual_maps.append(actual_map)
        live_masks.append(live)
        terminal_masks.append(terminal)
        visible_flags.append(visible)
        kings.append((owner_king, observer_king))

    strata: list[tuple[int, int, int]] = []
    for index in range(stratum_count):
        offset = stratum_offset + index * stratum_bytes
        geometry = struct.unpack_from("<I", payload, offset)[0]
        live = int.from_bytes(payload[offset + 4:offset + 14], "little")
        root = struct.unpack_from("<I", payload, offset + 14)[0]
        if (geometry >= geometry_count or root >= node_count or not live or
                live & ~live_masks[geometry]):
            raise ValueError("UFGM1 stratum residual")
        strata.append((geometry, live, root))

    for geometry in range(geometry_count):
        for actual, stratum in enumerate(actual_maps[geometry]):
            live = bool(live_masks[geometry] & (1 << actual))
            terminal = bool(terminal_masks[geometry] & (1 << actual))
            if actual in kings[geometry]:
                valid = not live and not terminal and stratum == NO_STRATUM
            elif live and not visible_flags[geometry]:
                valid = stratum != NO_STRATUM
            else:
                valid = stratum == NO_STRATUM
            if not valid:
                raise ValueError("UFGM1 actual-to-stratum residual")
            if stratum != NO_STRATUM and (
                    stratum >= stratum_count or
                    strata[stratum][0] != geometry or
                    not strata[stratum][1] & (1 << actual)):
                raise ValueError("UFGM1 forward stratum map residual")
    for index, (geometry, live, _root) in enumerate(strata):
        for actual in range(80):
            if (live & (1 << actual) and
                    actual_maps[geometry][actual] != index):
                raise ValueError("UFGM1 reverse stratum map residual")
    return {
        "bytes": len(payload), "sha256": sha256(path),
        "concrete_sha256": concrete_sha, "model_sha256": model_sha,
        "observation_sha256": observation_sha, "nodes": node_count,
        "geometries": geometry_count, "strata": stratum_count,
    }


def verify_table(path: Path, model: dict[str, object]) -> dict[str, object]:
    payload = path.read_bytes()
    if len(payload) < 40 or payload[:8] != b"UFTB1\0\0\0":
        raise ValueError("invalid single-Ghost UFTB1 header")
    version, piece, count, _model, substates, wdl_bytes, dtw_bytes, flags = \
        struct.unpack_from("<8I", payload, 8)
    if ((version, piece, count, substates, wdl_bytes, dtw_bytes, flags) !=
            (4, 11, 1_971_840, 2, 492_960, 1_971_840, 0) or
            len(payload) != 40 + wdl_bytes + dtw_bytes or
            sha256(path) != model["concrete_sha256"]):
        raise ValueError("single-Ghost UFTB1 binding/extent residual")
    return {"bytes": len(payload), "sha256": sha256(path),
            "wdl": payload[40:40 + wdl_bytes]}


def side_counts(flags: bytes, wdl: bytes, *, first: bool,
                index_begin: int) -> tuple[str, str]:
    counts = Counter(flags)
    if any(value & ~7 for value in counts):
        raise ValueError("UFIW2 force-flag residual")
    legal = Counter({"win": 0, "loss": 0, "draw": 0})
    unreachable = Counter({"win": 0, "loss": 0, "draw": 0})
    for local_index, value in enumerate(flags):
        owner = bool(value & 1)
        observer = bool(value & 2)
        if owner and observer:
            raise ValueError("UFIW2 contradictory forces")
        if value & 4:
            if first:
                outcome = "win" if owner else "loss" if observer else "draw"
            else:
                outcome = "loss" if owner else "win" if observer else "draw"
            legal[outcome] += 1
        else:
            index = index_begin + local_index
            concrete = (wdl[index // 4] >> (2 * (index % 4))) & 3
            if concrete not in (1, 2, 3):
                raise ValueError("unreachable UFIW2 state has unknown concrete WDL")
            unreachable[("win", "loss", "draw")[concrete - 1]] += 1
    cell = " / ".join(
        f"{legal[name]:,}" +
        (f" ({unreachable[name]:,})" if unreachable[name] else "")
        for name in ("win", "loss", "draw"))
    reach = f"{sum(legal.values()):,} / {sum(unreachable.values()):,}"
    return cell, reach


def verify_overlay(path: Path, model: dict[str, object],
                   table: dict[str, object]) -> dict[str, object]:
    payload = path.read_bytes()
    if len(payload) < 160:
        raise ValueError("truncated UFIW2 overlay")
    header = struct.unpack_from("<8s6I", payload)
    if (header != (b"UFIW2\0\0\0", 2, 11, 30, 0, 1_971_840, 2) or
            len(payload) != 160 + 1_971_840 or
            payload[32:96].decode() != model["concrete_sha256"] or
            payload[96:160].decode() != model["model_sha256"]):
        raise ValueError("UFIW2 header/binding/extent residual")
    flags = payload[160:]
    first, first_reach = side_counts(
        flags[:STATES_PER_SIDE], table["wdl"], first=True, index_begin=0)
    second, second_reach = side_counts(
        flags[STATES_PER_SIDE:], table["wdl"], first=False,
        index_begin=STATES_PER_SIDE)
    return {"bytes": len(payload), "sha256": sha256(path),
            "first": first, "second": second,
            "reachability": f"{first_reach}; {second_reach}",
            "flag_counts": dict(sorted(Counter(flags).items()))}


def aws(*arguments: str) -> dict[str, object]:
    output = subprocess.run(["aws", *arguments], check=True, text=True,
                            capture_output=True).stdout
    return json.loads(output)


def checked_head(args: argparse.Namespace, key: str, version: str,
                 artifact: dict[str, object]) -> None:
    head = aws("s3api", "head-object", "--bucket", args.bucket,
               "--key", key, "--version-id", version,
               "--region", args.region, "--output", "json")
    if (head.get("VersionId") != version or
            head.get("ContentLength") != artifact["bytes"] or
            head.get("Metadata", {}).get("sha256") != artifact["sha256"]):
        raise ValueError("version-pinned artifact HEAD residual")


def publish(args: argparse.Namespace) -> dict[str, object]:
    model = verify_model(args.model)
    table = verify_table(args.table, model)
    overlay = verify_overlay(args.overlay, model, table)
    del table["wdl"]
    checked_head(args, args.table_key, args.table_version, table)
    checked_head(args, args.model_key, args.model_version, model)
    checked_head(args, args.overlay_key, args.overlay_version, overlay)
    table["s3"] = {"bucket": args.bucket, "key": args.table_key,
                   "version_id": args.table_version}
    model["s3"] = {"bucket": args.bucket, "key": args.model_key,
                   "version_id": args.model_version}
    overlay["s3"] = {"bucket": args.bucket, "key": args.overlay_key,
                     "version_id": args.overlay_version}
    certificate = {
        "schema": "ultimate-single-ghost-information-certificate-v1",
        "predicate": "fresh-maximal-public-view-v2-exact-single-ghost",
        "concrete_table": table, "model": model, "overlay": overlay,
        "verification": {
            "unknown_states": 0, "hash_residuals": 0,
            "header_residuals": 0, "extent_residuals": 0,
            "concrete_wdl_residuals": 0, "robdd_residuals": 0,
            "geometry_residuals": 0,
            "stratum_residuals": 0, "force_flag_residuals": 0,
        },
    }
    payload = (json.dumps(certificate, indent=2, sort_keys=True) + "\n").encode()
    digest = hashlib.sha256(payload).hexdigest()
    key = f"{args.certificate_prefix.strip('/')}/sha256/{digest}/certificate.json"
    with tempfile.NamedTemporaryFile() as stream:
        stream.write(payload)
        stream.flush()
        try:
            result = aws("s3api", "head-object", "--bucket", args.bucket,
                         "--key", key, "--region", args.region,
                         "--output", "json")
            if result.get("Metadata", {}).get("sha256") != digest:
                raise ValueError("existing certificate metadata residual")
        except subprocess.CalledProcessError:
            result = aws("s3api", "put-object", "--bucket", args.bucket,
                         "--key", key, "--body", stream.name,
                         "--metadata", f"sha256={digest}",
                         "--region", args.region, "--output", "json")
    version = result.get("VersionId")
    if not version:
        raise ValueError("certificate upload lacks VersionId")
    storage = (
        f"S3 table sha256:{table['sha256']} VersionId {args.table_version}; "
        f"Ghost model sha256:{model['sha256']} VersionId "
        f"{args.model_version}; overlay sha256:{overlay['sha256']} VersionId "
        f"{args.overlay_version}; certificate sha256:{digest} VersionId {version}")
    value = {"result_kind": "information v2", "first": overlay["first"],
             "second": overlay["second"],
             "reachability": overlay["reachability"], "storage": storage}
    ledger.update(args.readme, [], [], certified_values=[
        "kghostk.uftb=" + json.dumps(value, separators=(",", ":"))])
    subprocess.run(["python3", str(PLOT)], cwd=ROOT, check=True)
    return {"certificate_sha256": digest, "certificate_version_id": version,
            **value}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--table", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--overlay", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--table-key", required=True)
    parser.add_argument("--table-version", required=True)
    parser.add_argument("--model-key", required=True)
    parser.add_argument("--model-version", required=True)
    parser.add_argument("--overlay-key", required=True)
    parser.add_argument("--overlay-version", required=True)
    parser.add_argument("--certificate-prefix", required=True)
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--readme", type=Path, default=README)
    return parser.parse_args()


def main() -> None:
    print(json.dumps(publish(parse_args()), sort_keys=True))


if __name__ == "__main__":
    main()
