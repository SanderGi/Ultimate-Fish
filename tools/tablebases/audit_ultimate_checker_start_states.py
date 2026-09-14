#!/usr/bin/env python3
"""Build exact normal/king Checker root-state plot data from public artifacts.

Each certified material group is downloaded and authenticated separately, then
removed before the next group is fetched.  The native reachability auditor
classifies the Checker's four exact substates; this script groups ordinary and
forced-jump roots as normal Checker (0/1) or Checker King (2/3).  Successor
states are never filtered, so the W/L/D result remains the unrestricted exact
tablebase result from each selected root.
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
import time
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import plot_ultimate_tablebases as plot  # noqa: E402


DEFAULT_DATASET = "SanderGi/Ultimate-Fish-Tablebases"
DEFAULT_ORIGIN = "https://huggingface.co"
DEFAULT_REVISION = "847eb02da6cd3a0879226bac293464c0e72763dd"
TRANSFER_BYTES = 8 * 1024 * 1024
RESULT_NAMES = ("unknown", "wins", "losses", "draws")
START_STATES = ("normal", "king")
CONCRETE_LINE = re.compile(
    r"reachability_(primary|secondary)_substate(_total|_trivial)? "
    r"substate (\d+) side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)"
)
INFORMATION_LINE = re.compile(
    r"information_reachability_substate_(admitted|excluded|trivial) "
    r"substate (\d+) side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)"
)
# These certified overlays are intentionally encoded in logical [extra-piece
# substate][Ghost visibility] order while their concrete Ghost-primary sources
# are [visibility][extra-piece substate]. Bind each transpose to its exact
# source/model pair and material orientation so a future overlay cannot
# silently inherit the exception.
TRANSPOSED_INFORMATION_OVERLAYS = {
    (
        "kghostksniper",
        "607aa04e85e59bfff387b4a45ae2078aae96eda7dc8a07ab442cdef15ec855c7",
        "01e5e3e92db72dd0ee561224207d56a8a5ce985b2c33a472b6d29a122f59ddb2",
    ): ("ghost", "sniper", True),
    (
        "kghostsniperk",
        "8425f93c93e9a3c7ad39820efb7fedb690f24fb64b540c6b9e8eced5e8aaa7f5",
        "328509ac52c125a8428c00589bb657c7ff8ad1265d4e2759c9500237e2ca323b",
    ): ("ghost", "sniper", False),
    (
        "kghostkchecker",
        "167d37f16f69d048033e06fabb5b6acc919681380c23ea20dc00c908171c21fe",
        "6cac380d00ad8fc0490718b4498ae7c142a9a9833ac63f5a1098832bffaefa2c",
    ): ("ghost", "checker", True),
    (
        "kghostcheckerk",
        "2cba1e00c090e9cd62f34bc967c40d7bba441478c938b5a338764df2806fa723",
        "e69929a1b356c9fd2245fee77f2dcc33faea5b0acd1825d2d8979b305ce96e30",
    ): ("ghost", "checker", False),
    (
        "kghostcheckerk",
        "fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e",
        "f983e18aae182467f5a0279996c35087d4984b11c80fe6404f16d9214c26d01d",
    ): ("ghost", "checker", False),
    (
        "kghostkchecker",
        "231d2f45d8d1db2a6e47aa413485444d93599fc68ae0d069afabd5e60369ee10",
        "bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316",
    ): ("ghost", "checker", True),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(TRANSFER_BYTES), b""):
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
    with urlopen(Request(info_url, headers=headers(token)), timeout=120) as response:
        info = json.load(response)
    resolved_revision = str(info.get("sha", ""))
    if not re.fullmatch(r"[0-9a-f]{40}", resolved_revision):
        raise RuntimeError("Hugging Face did not return an immutable dataset revision")
    url = (f"{origin.rstrip('/')}/api/datasets/{encoded_dataset}/tree/"
           f"{resolved_revision}/tablebases?recursive=true&expand=false")
    files: dict[str, dict[str, Any]] = {}
    while url:
        with urlopen(Request(url, headers=headers(token)), timeout=120) as response:
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
            if re.fullmatch(r"[0-9a-f]{64}", digest) and size >= 0:
                files[filename] = {
                    "filename": filename, "bytes": size, "sha256": digest}
        url = following
    return resolved_revision, files


def resolve_url(origin: str, dataset: str, revision: str, filename: str) -> str:
    encoded_dataset = "/".join(quote(part, safe="") for part in dataset.split("/"))
    return (f"{origin.rstrip('/')}/datasets/{encoded_dataset}/resolve/"
            f"{quote(revision, safe='')}/tablebases/"
            f"{quote(filename, safe='')}?download=true")


def download(entry: dict[str, Any], destination: Path, origin: str,
             dataset: str, revision: str, token: str | None,
             max_retries: int) -> None:
    """Download one authenticated file with resume and bounded memory."""
    size = int(entry["bytes"])
    if destination.exists():
        if destination.stat().st_size == size and sha256(destination) == entry["sha256"]:
            return
        destination.unlink()
    temporary = destination.with_suffix(destination.suffix + ".download")
    received = temporary.stat().st_size if temporary.exists() else 0
    if received > size:
        temporary.unlink()
        received = 0
    digest = hashlib.sha256()
    if received:
        with temporary.open("rb") as stream:
            for block in iter(lambda: stream.read(TRANSFER_BYTES), b""):
                digest.update(block)
    failures = 0
    url = resolve_url(origin, dataset, revision, str(entry["filename"]))
    while received < size:
        request_headers = headers(token)
        if received:
            request_headers["Range"] = f"bytes={received}-"
        try:
            with urlopen(Request(url, headers=request_headers), timeout=120) as response:
                if received:
                    content_range = response.headers.get("Content-Range", "")
                    if response.status != 206 or not content_range.startswith(
                            f"bytes {received}-"):
                        raise RuntimeError(
                            f"Hugging Face did not honor resume offset for "
                            f"{entry['filename']}")
                with temporary.open("ab") as stream:
                    while received < size:
                        block = response.read(min(TRANSFER_BYTES, size - received))
                        if not block:
                            raise URLError("truncated response")
                        stream.write(block)
                        digest.update(block)
                        received += len(block)
            failures = 0
        except (HTTPError, URLError, TimeoutError, ConnectionError) as error:
            failures += 1
            if failures > max_retries:
                raise RuntimeError(
                    f"download retries exhausted for {entry['filename']}") from error
            delay = min(30, 2 ** failures)
            print(f"retrying {entry['filename']} at byte {received} in {delay}s",
                  flush=True)
            time.sleep(delay)
    if digest.hexdigest() != entry["sha256"]:
        temporary.unlink(missing_ok=True)
        raise RuntimeError(f"Hugging Face SHA-256 mismatch for {entry['filename']}")
    temporary.replace(destination)


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
        if record["primary"] == "checker" or record.get("secondary") == "checker"
    }


def information_material(record: dict[str, Any]) -> bool:
    return bool({record["primary"], record.get("secondary")} & {"jester", "ghost"})


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
    with overlay.open("rb") as stream:
        header = stream.read(160)
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
    transpose_key = (overlay.stem, source_sha, model_sha)
    transpose_material = TRANSPOSED_INFORMATION_OVERLAYS.get(transpose_key)
    if transpose_material is not None:
        if (record["primary"], record.get("secondary"),
                bool(record.get("opposing"))) != transpose_material:
            raise RuntimeError("Ghost/Checker transpose material residual")
        result.append("--information-transpose-substates")
    return result


def empty_counts() -> list[int]:
    return [0, 0, 0, 0]


def add_counts(target: list[int], source: list[int]) -> None:
    for index, value in enumerate(source):
        target[index] += value


def checker_state(substate: int) -> str:
    return "king" if substate & 2 else "normal"


def finish_rows(
    buckets: dict[tuple[str, int], dict[str, list[int]]],
    expected_kinds: set[str],
) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for state in START_STATES:
        sides = []
        for side in range(2):
            raw = buckets.get((state, side), {})
            if set(raw) != expected_kinds:
                raise RuntimeError(
                    f"incomplete Checker {state} audit for side {side}: {sorted(raw)}")
            if "total" in raw:
                total = raw["total"]
                excluded = raw["excluded"]
                admitted = [value - excluded[index]
                            for index, value in enumerate(total)]
            else:
                admitted = raw["admitted"]
                excluded = raw["excluded"]
                total = [value + excluded[index]
                         for index, value in enumerate(admitted)]
            trivial = raw["trivial"]
            if (any(value < 0 for value in admitted) or admitted[0] or trivial[0] or
                    any(value > admitted[index]
                        for index, value in enumerate(trivial))):
                raise RuntimeError(
                    f"invalid Checker {state} conservation for side {side}")
            display = [value - trivial[index]
                       for index, value in enumerate(admitted)]
            def named(values: list[int]) -> dict[str, int]:
                return dict(zip(RESULT_NAMES[1:], values[1:]))
            sides.append({
                "total": named(total),
                "excluded": named(excluded),
                "admitted": named(admitted),
                "trivial": named(trivial),
                "display": named(display),
            })
        result[state] = {
            "first_starts": sides[0], "second_starts": sides[1]}
    return result


def parse_counts(text: str, record: dict[str, Any],
                 information: bool) -> dict[str, Any]:
    checker_slot = "primary" if record["primary"] == "checker" else "secondary"
    buckets: dict[tuple[str, int], dict[str, list[int]]] = {}
    if not information:
        seen: dict[tuple[int, int], dict[str, list[int]]] = {}
        for match in CONCRETE_LINE.finditer(text):
            if match.group(1) != checker_slot:
                continue
            kind = {None: "excluded", "_total": "total",
                    "_trivial": "trivial"}[match.group(2)]
            substate, side = int(match.group(3)), int(match.group(4))
            if not 0 <= substate < 4:
                raise RuntimeError(f"invalid Checker substate {substate}")
            key = (substate, side)
            if kind in seen.setdefault(key, {}):
                raise RuntimeError("duplicate Checker audit row")
            seen[key][kind] = [int(match.group(index)) for index in range(5, 9)]
        for substate in range(4):
            for side in range(2):
                raw = seen.get((substate, side), {})
                if set(raw) != {"total", "excluded", "trivial"}:
                    raise RuntimeError(
                        f"incomplete Checker substate {substate} audit for side {side}")
                target = buckets.setdefault((checker_state(substate), side), {
                    kind: empty_counts() for kind in raw})
                for kind, counts in raw.items():
                    add_counts(target[kind], counts)
        return finish_rows(buckets, {"total", "excluded", "trivial"})

    primary_factor = plot.PIECE_BY_NAME[str(record["primary"])].state_factor
    secondary_factor = plot.PIECE_BY_NAME[str(record["secondary"])].state_factor
    combined_factor = primary_factor * secondary_factor
    seen = {}
    for match in INFORMATION_LINE.finditer(text):
        kind = match.group(1)
        combined, side = int(match.group(2)), int(match.group(3))
        if not 0 <= combined < combined_factor:
            raise RuntimeError(f"invalid combined substate {combined}")
        key = (combined, side)
        if kind in seen.setdefault(key, {}):
            raise RuntimeError("duplicate Checker information audit row")
        seen[key][kind] = [int(match.group(index)) for index in range(4, 8)]
    for combined in range(combined_factor):
        checker_substate = (combined // secondary_factor
                            if checker_slot == "primary"
                            else combined % secondary_factor)
        if not 0 <= checker_substate < 4:
            raise RuntimeError("information substate does not encode a Checker")
        for side in range(2):
            raw = seen.get((combined, side), {})
            if set(raw) != {"admitted", "excluded", "trivial"}:
                raise RuntimeError(
                    f"incomplete combined substate {combined} audit for side {side}")
            target = buckets.setdefault((checker_state(checker_substate), side), {
                kind: empty_counts() for kind in raw})
            for kind, counts in raw.items():
                add_counts(target[kind], counts)
    return finish_rows(buckets, {"admitted", "excluded", "trivial"})


def normalize_and_validate_aggregate(
    filename: str, states: dict[str, Any], aggregate: plot.ReadmeResult,
    allow_role_flip: bool,
) -> list[int]:
    flipped: list[int] = []
    for side, key in enumerate(("first_starts", "second_starts")):
        expected_wdl = aggregate.first_starts if side == 0 else aggregate.second_starts
        expected = {"wins": expected_wdl.wins, "losses": expected_wdl.losses,
                    "draws": expected_wdl.draws}
        def aggregate_side() -> dict[str, int]:
            return {
                name: sum(states[state][key]["display"][name]
                          for state in START_STATES)
                for name in RESULT_NAMES[1:]
            }
        actual = aggregate_side()
        reversed_actual = {"wins": actual["losses"],
                           "losses": actual["wins"],
                           "draws": actual["draws"]}
        if actual != expected and allow_role_flip and reversed_actual == expected:
            for state in START_STATES:
                for bucket in ("total", "excluded", "admitted", "trivial", "display"):
                    row = states[state][key][bucket]
                    row["wins"], row["losses"] = row["losses"], row["wins"]
            actual = aggregate_side()
            flipped.append(side)
        if actual != expected:
            raise RuntimeError(
                f"Checker start-state slices do not reproduce {filename} {key}: "
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
                        default=ROOT / "tablebases/checker-start-state-summary.json")
    parser.add_argument("--work-root", type=Path,
                        default=Path(tempfile.gettempdir()) /
                        "ultimatefish-checker-start-state-audit")
    parser.add_argument("--dataset", default=DEFAULT_DATASET)
    parser.add_argument("--origin", default=DEFAULT_ORIGIN)
    parser.add_argument("--revision", default=DEFAULT_REVISION)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--max-retries", type=int, default=5)
    parser.add_argument(
        "--refresh", action="append", default=[], metavar="FILENAME",
        help="reaudit this certified filename even when its cached hashes match",
    )
    args = parser.parse_args()
    if not 1 <= args.workers <= 4:
        parser.error("--workers must be between 1 and 4")
    if not 0 <= args.max_retries <= 10:
        parser.error("--max-retries must be between 0 and 10")
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
    revision, remote = catalog(args.origin, args.dataset, args.revision, token)
    binary_sha = sha256(args.binary)

    certified_items = [
        (filename, record) for filename, record in material.items()
        if summaries.get(filename) and summaries[filename].status == "certified"
    ]
    certified = {filename for filename, _ in certified_items}
    refresh = set(args.refresh)
    if not refresh <= certified:
        parser.error(
            f"--refresh is not a certified Checker record: "
            f"{sorted(refresh - certified)}"
        )
    for filename, record in certified_items:
        if (record["primary"] == "checker" and
                record.get("secondary") == "checker" and
                not record.get("opposing")):
            continue
        if filename not in remote:
            raise RuntimeError(f"Hugging Face catalog lacks {filename}")
        if information_material(record):
            overlay_name = f"{Path(filename).stem}.ufiw"
            if overlay_name not in remote:
                raise RuntimeError(f"Hugging Face catalog lacks {overlay_name}")

    def transfer_size(item: tuple[str, dict[str, Any]]) -> int:
        filename, record = item
        if filename not in remote:
            return 0
        size = int(remote[filename]["bytes"])
        if information_material(record):
            size += int(remote[f"{Path(filename).stem}.ufiw"]["bytes"])
        return size

    # Audit the smallest groups first. Besides minimizing the pilot cost, this
    # validates every parser path before the largest Checker tables are fetched.
    for filename, record in sorted(
            certified_items, key=lambda item: (transfer_size(item), item[0])):
        aggregate = summaries[filename]
        if (record["primary"] == "checker" and
                record.get("secondary") == "checker" and
                not record.get("opposing")):
            progress[filename] = {
                "excluded": True,
                "reason": ("exchange-folded same-team Checkers have no "
                           "distinguished row Checker"),
            }
            write_json(progress_path, progress)
            continue
        table_entry = remote[filename]
        information = information_material(record)
        overlay_name = f"{Path(filename).stem}.ufiw"
        overlay_entry = remote.get(overlay_name) if information else None
        if information and overlay_entry is None:
            raise RuntimeError(f"Hugging Face catalog lacks {overlay_name}")
        cached = progress.get(filename)
        if (filename not in refresh and cached and
                cached.get("audit_binary_sha256") == binary_sha and
                cached.get("tablebase_sha256") == table_entry["sha256"] and
                cached.get("information_overlay_sha256") ==
                (overlay_entry["sha256"] if overlay_entry else None)):
            print(f"reusing {filename}", flush=True)
            continue
        table = args.work_root / filename
        overlay = args.work_root / overlay_name if overlay_entry else None
        audited = False
        try:
            print(f"downloading {filename} ({table_entry['bytes'] / 1e9:.3f} GB)",
                  flush=True)
            download(table_entry, table, args.origin, args.dataset,
                     revision, token, args.max_retries)
            if overlay_entry and overlay:
                print(f"downloading {overlay_name} "
                      f"({overlay_entry['bytes'] / 1e9:.3f} GB)", flush=True)
                download(overlay_entry, overlay, args.origin, args.dataset,
                         revision, token, args.max_retries)
            completed = subprocess.run(
                command(args.binary, record, table, args.workers, overlay),
                check=True, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT)
            states = parse_counts(completed.stdout, record, information)
            flipped = normalize_and_validate_aggregate(
                filename, states, aggregate, information)
            progress[filename] = {
                "audit_binary_sha256": binary_sha,
                "tablebase_sha256": table_entry["sha256"],
                "information_overlay_sha256": (
                    overlay_entry["sha256"] if overlay_entry else None),
                "checker_slot": ("primary" if record["primary"] == "checker"
                                 else "secondary"),
                "result_kind": "information-v2" if information else "concrete",
                "role_normalized_sides": flipped,
                "start_states": states,
            }
            write_json(progress_path, progress)
            print(f"audited {filename}", flush=True)
            audited = True
        finally:
            # Keep an authenticated current payload after an audit/parser
            # failure so a corrected rerun does not waste the download. A
            # successful material group is removed before the next begins.
            if audited and overlay:
                overlay.unlink(missing_ok=True)
                overlay.with_suffix(overlay.suffix + ".download").unlink(missing_ok=True)
            if audited:
                table.unlink(missing_ok=True)
                table.with_suffix(table.suffix + ".download").unlink(missing_ok=True)

    if set(progress) != certified:
        raise RuntimeError(
            "Checker summary coverage residual: "
            f"missing={sorted(certified - set(progress))} "
            f"extra={sorted(set(progress) - certified)}")
    output = {
        "schema": 1,
        "description": (
            "Exact reachability-admitted Checker root types with authenticated "
            "trivial positions removed; successor play remains unrestricted"),
        "semantics": "reachability-admitted-minus-trivial-v3",
        "start_states": list(START_STATES),
        "substate_groups": {"normal": [0, 1], "king": [2, 3]},
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
