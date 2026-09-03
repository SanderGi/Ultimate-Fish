#!/usr/bin/env python3
"""Build exact Giant start-class plot data from the public HF artifacts.

Artifacts are streamed and verified one material class at a time.  Each newly
downloaded UFTB/UFIW is removed before the next class is fetched, keeping peak
disk use to one tablebase group.  The native audit is intentionally serial at
the material-class level and defaults to two worker threads.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any
from urllib.parse import quote
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import plot_ultimate_tablebases as plot  # noqa: E402


DEFAULT_DATASET = "SanderGi/Ultimate-Fish-Tablebases"
DEFAULT_ORIGIN = "https://huggingface.co"
CLASS_SIZES = (20, 16, 15, 12)
RESULT_NAMES = ("unknown", "wins", "losses", "draws")
CONCRETE_LINE = re.compile(
    r"reachability_(primary|secondary)_giant_class_"
    r"(total|excluded|trivial) class (20|16|15|12) side ([01]) "
    r"unknown (\d+) win (\d+) loss (\d+) draw (\d+)"
)
INFORMATION_LINE = re.compile(
    r"information_reachability_(primary|secondary)_giant_class_"
    r"(admitted|trivial) class (20|16|15|12) side ([01]) "
    r"unknown (\d+) win (\d+) loss (\d+) draw (\d+)"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def hf_token() -> str | None:
    for name in ("HF_TOKEN", "HUGGING_FACE_HUB_TOKEN", "HUGGINGFACE_TOKEN"):
        value = os.environ.get(name, "").strip()
        if value:
            return value
    cache = Path(os.environ.get(
        "HF_HOME", Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache")) /
        "huggingface"))
    candidates = (
        Path(os.environ["HF_TOKEN_PATH"]) if os.environ.get("HF_TOKEN_PATH") else None,
        cache / "token",
        Path.home() / ".huggingface/token",
    )
    for candidate in candidates:
        if candidate is None:
            continue
        try:
            value = candidate.read_text(encoding="utf-8").strip()
        except (FileNotFoundError, PermissionError):
            continue
        if value:
            return value
    return None


def headers(token: str | None) -> dict[str, str]:
    return {"Authorization": f"Bearer {token}"} if token else {}


def next_link(value: str | None) -> str | None:
    if not value:
        return None
    for part in value.split(","):
        match = re.search(r'<([^>]+)>;\s*rel="?next"?', part, re.I)
        if match:
            return match.group(1)
    return None


def catalog(origin: str, dataset: str, revision: str,
            token: str | None) -> tuple[str, dict[str, dict[str, Any]]]:
    encoded_dataset = "/".join(quote(part, safe="") for part in dataset.split("/"))
    info_url = (f"{origin.rstrip('/')}/api/datasets/{encoded_dataset}/revision/"
                f"{quote(revision, safe='')}")
    with urlopen(Request(info_url, headers=headers(token))) as response:
        info = json.load(response)
    resolved_revision = str(info.get("sha", ""))
    if not re.fullmatch(r"[0-9a-f]{40}", resolved_revision):
        raise RuntimeError("Hugging Face did not return an immutable dataset revision")
    url = (f"{origin.rstrip('/')}/api/datasets/{encoded_dataset}/tree/"
           f"{resolved_revision}/tablebases?recursive=true&expand=false")
    files: dict[str, dict[str, Any]] = {}
    while url:
        with urlopen(Request(url, headers=headers(token))) as response:
            page = json.load(response)
            following = next_link(response.headers.get("Link"))
        if not isinstance(page, list):
            raise RuntimeError("Hugging Face returned an invalid tablebase catalog")
        for entry in page:
            path = str(entry.get("path", ""))
            if entry.get("type") != "file" or not path.startswith("tablebases/"):
                continue
            filename = Path(path).name
            digest = str(entry.get("lfs", {}).get("oid", ""))
            digest = digest.removeprefix("sha256:")
            size = int(entry.get("lfs", {}).get("size", entry.get("size", -1)))
            if not re.fullmatch(r"[0-9a-f]{64}", digest) or size < 0:
                continue
            files[filename] = {"filename": filename, "bytes": size,
                               "sha256": digest}
        url = following
    return resolved_revision, files


def download(entry: dict[str, Any], destination: Path, origin: str,
             dataset: str, revision: str, token: str | None) -> None:
    encoded_dataset = "/".join(quote(part, safe="") for part in dataset.split("/"))
    encoded_file = quote(str(entry["filename"]), safe="")
    url = (f"{origin.rstrip('/')}/datasets/{encoded_dataset}/resolve/"
           f"{quote(revision, safe='')}/tablebases/{encoded_file}?download=true")
    digest = hashlib.sha256()
    received = 0
    temporary = destination.with_suffix(destination.suffix + ".download")
    try:
        with urlopen(Request(url, headers=headers(token))) as response, \
                temporary.open("xb") as stream:
            while True:
                block = response.read(8 * 1024 * 1024)
                if not block:
                    break
                stream.write(block)
                digest.update(block)
                received += len(block)
        if received != entry["bytes"] or digest.hexdigest() != entry["sha256"]:
            raise RuntimeError(f"Hugging Face verification failed for {entry['filename']}")
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def records() -> dict[str, dict[str, Any]]:
    by_filename: dict[str, dict[str, Any]] = {}
    for record in (
        *plot.stateful_candidates(), *plot.angel_candidates(),
        *plot.mirror_copycat_candidates(), *plot.devil_candidates(),
        *plot.inventory(),
    ):
        by_filename[str(record["filename"])] = record
    return {
        filename: record for filename, record in by_filename.items()
        if record["primary"] == "giant" or record.get("secondary") == "giant"
    }


def command(binary: Path, record: dict[str, Any], table: Path,
            workers: int, overlay: Path | None = None) -> list[str]:
    result = [str(binary), "--piece", str(record["primary"]),
              "--workers", str(workers), "--checkpoint-every", "0"]
    if record.get("secondary"):
        result += ["--piece2", str(record["secondary"])]
    if record.get("opposing"):
        result.append("--opposing")
    if overlay is None:
        result += ["--audit-turn-boundary-reachability"
                   if "prince" in {record["primary"], record.get("secondary")}
                   else "--audit-reachability", str(table)]
        return result
    header = overlay.read_bytes()[:160]
    if len(header) != 160 or header[:8] != b"UFIW2\0\0\0":
        raise RuntimeError(f"invalid information overlay: {overlay.name}")
    source_sha = header[32:96].decode("ascii")
    model_sha = header[96:160].decode("ascii")
    if (not re.fullmatch(r"[0-9a-f]{64}", source_sha) or
            not re.fullmatch(r"[0-9a-f]{64}", model_sha)):
        raise RuntimeError(f"invalid information binding: {overlay.name}")
    result += ["--audit-information-trivial", str(table),
               "--information-overlay", str(overlay),
               "--information-source-sha256", source_sha,
               "--information-model-sha256", model_sha]
    return result


def parse_counts(text: str, slot: str, information: bool) -> dict[int, Any]:
    expression = INFORMATION_LINE if information else CONCRETE_LINE
    rows: dict[tuple[int, int], dict[str, list[int]]] = {}
    for match in expression.finditer(text):
        if match.group(1) != slot:
            continue
        kind = match.group(2)
        giant_class, side = int(match.group(3)), int(match.group(4))
        rows.setdefault((giant_class, side), {})[kind] = [
            int(match.group(index)) for index in range(5, 9)
        ]
    result: dict[int, Any] = {}
    for giant_class in CLASS_SIZES:
        sides = []
        for side in range(2):
            raw = rows.get((giant_class, side), {})
            expected = {"admitted", "trivial"} if information else {
                "total", "excluded", "trivial"}
            if set(raw) != expected:
                raise RuntimeError(
                    f"incomplete Giant-{giant_class} audit for side {side}")
            admitted = raw["admitted"] if information else [
                total - excluded for total, excluded in
                zip(raw["total"], raw["excluded"])
            ]
            trivial = raw["trivial"]
            if admitted[0] or trivial[0] or any(
                    value > admitted[index] for index, value in enumerate(trivial)):
                raise RuntimeError(
                    f"invalid Giant-{giant_class} conservation for side {side}")
            display = [value - trivial[index]
                       for index, value in enumerate(admitted)]
            sides.append({
                "admitted": dict(zip(RESULT_NAMES[1:], admitted[1:])),
                "trivial": dict(zip(RESULT_NAMES[1:], trivial[1:])),
                "display": dict(zip(RESULT_NAMES[1:], display[1:])),
            })
        result[giant_class] = {
            "first_starts": sides[0], "second_starts": sides[1]}
    return result


def normalize_and_validate_aggregate(
        filename: str, classes: dict[int, Any], aggregate: plot.ReadmeResult,
        allow_role_flip: bool) -> list[int]:
    flipped: list[int] = []
    for side, key in enumerate(("first_starts", "second_starts")):
        expected_wdl = aggregate.first_starts if side == 0 else aggregate.second_starts
        expected = {"wins": expected_wdl.wins, "losses": expected_wdl.losses,
                    "draws": expected_wdl.draws}
        def aggregate_side() -> dict[str, int]:
            return {
                name: sum(classes[value][key]["display"][name]
                          for value in CLASS_SIZES)
                for name in RESULT_NAMES[1:]
            }
        actual = aggregate_side()
        reversed_actual = {"wins": actual["losses"],
                           "losses": actual["wins"],
                           "draws": actual["draws"]}
        if actual != expected and allow_role_flip and reversed_actual == expected:
            for value in CLASS_SIZES:
                for bucket in ("admitted", "trivial", "display"):
                    row = classes[value][key][bucket]
                    row["wins"], row["losses"] = row["losses"], row["wins"]
            actual = aggregate_side()
            flipped.append(side)
        if actual != expected:
            raise RuntimeError(
                f"Giant class slices do not reproduce {filename} {key}: "
                f"{actual} != {expected}")
    return flipped


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    temporary.replace(path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "src/ultimate_tablebase")
    parser.add_argument("--readme", type=Path,
                        default=ROOT / "tablebases/README.md")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "tablebases/giant-start-class-summary.json")
    parser.add_argument("--work-root", type=Path,
                        default=Path(tempfile.gettempdir()) /
                        "ultimatefish-giant-start-class-audit")
    parser.add_argument("--dataset", default=DEFAULT_DATASET)
    parser.add_argument("--origin", default=DEFAULT_ORIGIN)
    parser.add_argument("--revision", default="main")
    parser.add_argument("--workers", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.workers <= 4:
        parser.error("--workers must be between 1 and 4")
    args.work_root.mkdir(parents=True, exist_ok=True)
    progress_path = args.work_root / "progress.json"
    if progress_path.exists():
        progress = json.loads(progress_path.read_text(encoding="utf-8"))
    elif args.output.exists():
        previous = json.loads(args.output.read_text(encoding="utf-8"))
        progress = (previous.get("files", {})
                    if previous.get("schema") == 1 and
                    previous.get("semantics") ==
                    "reachability-admitted-minus-trivial-v3" else {})
    else:
        progress = {}
    summaries = plot.read_summary(args.readme)
    material = records()
    token = hf_token()
    revision, remote = catalog(
        args.origin, args.dataset, args.revision, token)
    binary_sha = sha256(args.binary)

    for filename, record in material.items():
        aggregate = summaries.get(filename)
        if aggregate is None or aggregate.status != "certified":
            continue
        if (record["primary"] == "giant" and
                record.get("secondary") == "giant" and
                not record.get("opposing")):
            progress[filename] = {
                "excluded": True,
                "reason": "exchange-folded same-team Giants have no distinguished row Giant",
            }
            write_json(progress_path, progress)
            continue
        cached = progress.get(filename)
        if (cached and cached.get("audit_binary_sha256") == binary_sha and
                cached.get("tablebase_sha256") == remote.get(filename, {}).get("sha256")):
            print(f"reusing {filename}", flush=True)
            continue
        table_entry = remote.get(filename)
        if table_entry is None:
            raise RuntimeError(f"Hugging Face catalog lacks {filename}")
        information = "jester" in {record["primary"], record.get("secondary")} or \
                      "ghost" in {record["primary"], record.get("secondary")}
        overlay_name = f"{Path(filename).stem}.ufiw"
        overlay_entry = remote.get(overlay_name) if information else None
        if information and overlay_entry is None:
            raise RuntimeError(f"Hugging Face catalog lacks {overlay_name}")
        table = args.work_root / filename
        overlay = args.work_root / overlay_name if overlay_entry else None
        try:
            print(f"downloading {filename}", flush=True)
            download(table_entry, table, args.origin, args.dataset,
                     revision, token)
            if overlay_entry and overlay:
                print(f"downloading {overlay_name}", flush=True)
                download(overlay_entry, overlay, args.origin, args.dataset,
                         revision, token)
            completed = subprocess.run(
                command(args.binary, record, table, args.workers, overlay),
                check=True, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT)
            slot = "primary" if record["primary"] == "giant" else "secondary"
            classes = parse_counts(completed.stdout, slot, information)
            flipped = normalize_and_validate_aggregate(
                filename, classes, aggregate, information)
            progress[filename] = {
                "audit_binary_sha256": binary_sha,
                "tablebase_sha256": table_entry["sha256"],
                "information_overlay_sha256": (
                    overlay_entry["sha256"] if overlay_entry else None),
                "giant_slot": slot,
                "result_kind": "information-v2" if information else "concrete",
                "role_normalized_sides": flipped,
                "classes": {str(key): value for key, value in classes.items()},
            }
            write_json(progress_path, progress)
            print(f"audited {filename}", flush=True)
        finally:
            if overlay:
                overlay.unlink(missing_ok=True)
            table.unlink(missing_ok=True)

    certified = {
        filename for filename, record in material.items()
        if summaries.get(filename) and summaries[filename].status == "certified"
    }
    if set(progress) != certified:
        raise RuntimeError(
            "Giant summary coverage residual: "
            f"missing={sorted(certified - set(progress))} "
            f"extra={sorted(set(progress) - certified)}")
    output = {
        "schema": 1,
        "description": (
            "Exact reachability-admitted Giant root-anchor parity classes with "
            "authenticated trivial positions removed"),
        "semantics": "reachability-admitted-minus-trivial-v3",
        "class_sizes": list(CLASS_SIZES),
        "class_definition": (
            "lower-left anchor file/rank parity components: even/even, "
            "even/odd, odd/even, odd/odd"),
        "dataset": args.dataset,
        "dataset_revision": revision,
        "audit_binary_sha256": binary_sha,
        "files": {filename: progress[filename] for filename in sorted(progress)},
    }
    write_json(args.output, output)
    progress_path.unlink(missing_ok=True)
    print(args.output)


if __name__ == "__main__":
    main()
