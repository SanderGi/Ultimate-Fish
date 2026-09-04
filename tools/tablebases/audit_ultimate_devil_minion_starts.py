#!/usr/bin/env python3
"""Stream the certified lone-Devil class and summarize alive root Minions.

The Hugging Face payloads are read in publication order and piped directly to
the native UFDS auditor.  No tablebase payload is written to disk.  Completed
logical partitions are checkpointed as small JSON records, so subsequent runs
do not download them again.  Both downloading/SHA verification and auditing
are deliberately serial and use a fixed 8 MiB transfer buffer.
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
from typing import Any, BinaryIO
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp"
CERTIFICATE = ROOT / "tablebases/ultimate-devil-stateful-class-certificate.json"
OUTPUT = ROOT / "tablebases/devil-minion-start-summary.json"
DEFAULT_DATASET = "SanderGi/Ultimate-Fish-Tablebases"
DEFAULT_ORIGIN = "https://huggingface.co"
DEFAULT_REVISION = "ed8375c43f26e42c5b14ac79ff41e77d4317b97f"
LFS_MAX_FILE_BYTES = 50_000_000_000
TRANSFER_BYTES = 8 * 1024 * 1024
PROGRESS_BYTES = 1024 * 1024 * 1024
RESULTS = ("win", "loss", "draw")
MAX_MINIONS = 5


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(TRANSFER_BYTES), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


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
        raise RuntimeError("Hugging Face did not return an immutable revision")
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


def read_small(entry: dict[str, Any], origin: str, dataset: str,
               revision: str, token: str | None) -> bytes:
    url = resolve_url(origin, dataset, revision, str(entry["filename"]))
    with urlopen(Request(url, headers=headers(token)), timeout=120) as response:
        data = response.read(int(entry["bytes"]) + 1)
    if (len(data) != entry["bytes"] or
            hashlib.sha256(data).hexdigest() != entry["sha256"]):
        raise RuntimeError(f"Hugging Face verification failed for {entry['filename']}")
    return data


def stream_payload(entry: dict[str, Any], sink: BinaryIO,
                   logical_digest: "hashlib._Hash", origin: str,
                   dataset: str, revision: str, token: str | None,
                   max_retries: int) -> None:
    """Download, authenticate, and pipe one LFS object with bounded memory."""
    size = int(entry["bytes"])
    received = 0
    reported = 0
    failures = 0
    digest = hashlib.sha256()
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
                while received < size:
                    block = response.read(min(TRANSFER_BYTES, size - received))
                    if not block:
                        raise URLError("truncated response")
                    sink.write(block)
                    digest.update(block)
                    logical_digest.update(block)
                    received += len(block)
                    if received - reported >= PROGRESS_BYTES or received == size:
                        print(
                            f"streamed {entry['filename']} "
                            f"{received / 1_000_000_000:.3f} / "
                            f"{size / 1_000_000_000:.3f} GB",
                            flush=True)
                        reported = received
            failures = 0
        except (HTTPError, URLError, TimeoutError, ConnectionError) as error:
            failures += 1
            if failures > max_retries:
                raise RuntimeError(
                    f"download retries exhausted for {entry['filename']}") from error
            delay = min(30, 2 ** failures)
            print(
                f"retrying {entry['filename']} at byte {received} in {delay}s",
                flush=True)
            time.sleep(delay)
    if digest.hexdigest() != entry["sha256"]:
        raise RuntimeError(f"Hugging Face SHA-256 mismatch for {entry['filename']}")


def validate_certificate(path: Path) -> dict[str, Any]:
    certificate = json.loads(path.read_text(encoding="utf-8"))
    if (certificate.get("schema") !=
            "ultimate-devil-stateful-class-certificate-v1"):
        raise RuntimeError("invalid Devil class certificate schema")
    partitions = certificate.get("partitions", ())
    if len(partitions) != 12:
        raise RuntimeError("Devil certificate does not contain twelve partitions")
    return certificate


def logical_name(partition: dict[str, Any]) -> str:
    return f"kdevilk-{str(partition['label']).lower()}.ufds"


def payloads(partition: dict[str, Any], remote: dict[str, dict[str, Any]],
             origin: str, dataset: str, revision: str,
             token: str | None) -> list[dict[str, Any]]:
    name = logical_name(partition)
    expected_size = 32 + int(partition["states"]) * 10
    if expected_size <= LFS_MAX_FILE_BYTES:
        entry = remote.get(name)
        if (entry is None or entry["bytes"] != expected_size or
                entry["sha256"] != partition["sidecar_sha256"]):
            raise RuntimeError(f"Hugging Face binding mismatch for {name}")
        return [entry]

    manifest_name = name.removesuffix(".ufds") + ".ufdsm"
    manifest_entry = remote.get(manifest_name)
    if manifest_entry is None:
        raise RuntimeError(f"Hugging Face lacks {manifest_name}")
    manifest = json.loads(read_small(
        manifest_entry, origin, dataset, revision, token))
    parts = manifest.get("parts", ())
    if (manifest.get("schema") != "ultimate-fish-ufds-shard-manifest-v1" or
            manifest.get("filename") != name or
            manifest.get("bytes") != expected_size or
            manifest.get("sha256") != partition["sidecar_sha256"] or
            manifest.get("square") != partition["square"] or
            len(parts) < 2):
        raise RuntimeError(f"invalid Hugging Face shard manifest for {name}")
    result = []
    for index, part in enumerate(parts):
        expected_name = name.removesuffix(".ufds") + f"-part{index:03d}.ufdsp"
        entry = remote.get(expected_name)
        if (entry is None or part.get("filename") != expected_name or
                entry["bytes"] != part.get("bytes") or
                entry["sha256"] != part.get("sha256")):
            raise RuntimeError(f"Hugging Face shard binding mismatch: {expected_name}")
        result.append(entry)
    if sum(int(entry["bytes"]) for entry in result) != expected_size:
        raise RuntimeError(f"Hugging Face shard extent mismatch for {name}")
    return result


def build_auditor(work_root: Path) -> tuple[Path, str, str]:
    source_sha = sha256(SOURCE)
    binary = work_root / "audit_ultimate_devil_stateful_sidecar"
    receipt = work_root / "auditor-source.sha256"
    if not (binary.is_file() and receipt.is_file() and
            receipt.read_text(encoding="utf-8").strip() == source_sha):
        temporary = binary.with_suffix(".tmp")
        subprocess.run([
            "c++", "-std=c++17", "-O3", "-DNDEBUG", "-pthread",
            "-Wall", "-Wextra", "-Wpedantic", "-Werror", str(SOURCE),
            "-o", str(temporary),
        ], check=True)
        temporary.replace(binary)
        receipt.write_text(source_sha + "\n", encoding="utf-8")
    return binary, source_sha, sha256(binary)


def audit_partition(partition: dict[str, Any], entries: list[dict[str, Any]],
                    binary: Path, origin: str, dataset: str, revision: str,
                    token: str | None, max_retries: int) -> dict[str, Any]:
    process = subprocess.Popen([
        str(binary), "--input", "-", "--square", str(partition["square"]),
        "--workers", "1",
    ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if process.stdin is None or process.stdout is None or process.stderr is None:
        raise RuntimeError("failed to create auditor pipes")
    logical_digest = hashlib.sha256()
    started = time.monotonic()
    try:
        for entry in entries:
            stream_payload(entry, process.stdin, logical_digest, origin,
                           dataset, revision, token, max_retries)
        process.stdin.close()
        output = process.stdout.read()
        error = process.stderr.read().decode("utf-8", errors="replace")
        return_code = process.wait()
    except Exception:
        process.kill()
        process.wait()
        raise
    if return_code:
        raise RuntimeError(f"Devil auditor failed: {error.strip()}")
    if logical_digest.hexdigest() != partition["sidecar_sha256"]:
        raise RuntimeError(
            f"logical sidecar SHA-256 mismatch for {logical_name(partition)}")
    census = json.loads(output)
    expected_outcomes = {
        name: int(partition["outcomes"][name]) for name in RESULTS}
    if (census.get("schema") != "ultimate-devil-stateful-census-v1" or
            census.get("square") != partition["square"] or
            census.get("states") != partition["states"] or
            census.get("outcomes") != expected_outcomes or
            census.get("conservation_residual") != 0 or
            census.get("sorted_key_residual") != 0):
        raise RuntimeError(
            f"certified census mismatch for {logical_name(partition)}")
    census["elapsed_seconds"] = round(time.monotonic() - started, 3)
    census["payloads"] = [
        {key: entry[key] for key in ("filename", "bytes", "sha256")}
        for entry in entries]
    return census


def empty_outcomes() -> dict[str, int]:
    return {name: 0 for name in RESULTS}


def add_outcomes(destination: dict[str, int], source: dict[str, Any]) -> None:
    for name in RESULTS:
        destination[name] += int(source[name])


def side_record(outcomes: dict[str, int]) -> dict[str, dict[str, int]]:
    displayed = {
        "wins": outcomes["win"],
        "losses": outcomes["loss"],
        "draws": outcomes["draw"],
    }
    zero = {name: 0 for name in displayed}
    return {
        "admitted": dict(displayed), "trivial": zero,
        "display": dict(displayed)}


def build_summary(certificate: dict[str, Any], certificate_sha: str,
                  progress: dict[str, Any], dataset: str, revision: str,
                  source_sha: str, binary_sha: str) -> dict[str, Any]:
    partitions = certificate["partitions"]
    expected = {str(partition["square"]) for partition in partitions}
    if set(progress) != expected:
        raise RuntimeError("cannot finalize an incomplete Devil Minion audit")
    classes = {
        minions: [empty_outcomes(), empty_outcomes()]
        for minions in range(MAX_MINIONS + 1)}
    full = empty_outcomes()
    alive = empty_outcomes()
    total_states = 0
    bindings = []
    for partition in partitions:
        census = progress[str(partition["square"])]
        total_states += int(census["states"])
        add_outcomes(full, census["outcomes"])
        rows = census.get("by_minion_count", ())
        if len(rows) != MAX_MINIONS + 1:
            raise RuntimeError("incomplete Minion-count census")
        for minions, row in enumerate(rows):
            if row.get("minions") != minions:
                raise RuntimeError("misordered Minion-count census")
            sides = row.get("alive_outcomes_by_side_to_move", ())
            if len(sides) != 2:
                raise RuntimeError("census lacks alive Minion/side cross-tab")
            for side in range(2):
                add_outcomes(classes[minions][side], sides[side])
                add_outcomes(alive, sides[side])
        bindings.append({
            "label": partition["label"], "square": partition["square"],
            "states": census["states"], "sidecar_sha256": partition["sidecar_sha256"],
            "elapsed_seconds": census["elapsed_seconds"],
            "payloads": census["payloads"],
        })
    aggregate = certificate["aggregate"]
    aggregate_fields = {"win": "wins", "loss": "losses", "draw": "draws"}
    expected_full = {
        name: int(aggregate[aggregate_fields[name]]) for name in RESULTS}
    if total_states != aggregate["states"] or full != expected_full:
        raise RuntimeError("Devil partition audit does not reproduce certificate")
    alive_states = sum(alive.values())
    return {
        "schema": 1,
        "semantics": "reachable-devil-alive-root-current-minions-v1",
        "description": (
            "Certified lone-Devil UFDS states grouped only by the current "
            "root Minion count while all solved successors remain unrestricted"),
        "dataset": dataset,
        "dataset_revision": revision,
        "certificate_sha256": certificate_sha,
        "audit_source_sha256": source_sha,
        "audit_binary_sha256": binary_sha,
        "minion_counts": list(range(MAX_MINIONS + 1)),
        "full_class_states": total_states,
        "alive_root_states": alive_states,
        "dead_devil_continuation_states": total_states - alive_states,
        "full_class_outcomes": full,
        "alive_root_outcomes": alive,
        "partitions": bindings,
        "files": {
            "kdevilk.uftb": {
                "minion_counts": {
                    str(minions): {
                        "first_starts": side_record(classes[minions][0]),
                        "second_starts": side_record(classes[minions][1]),
                    }
                    for minions in range(MAX_MINIONS + 1)
                }
            }
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--certificate", type=Path, default=CERTIFICATE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument(
        "--work-root", type=Path,
        default=Path(tempfile.gettempdir()) / "ultimatefish-devil-minion-audit")
    parser.add_argument("--dataset", default=DEFAULT_DATASET)
    parser.add_argument("--origin", default=DEFAULT_ORIGIN)
    parser.add_argument("--revision", default=DEFAULT_REVISION)
    parser.add_argument("--square", type=int, action="append")
    parser.add_argument("--max-retries", type=int, default=8)
    args = parser.parse_args()
    if args.max_retries < 0 or args.max_retries > 20:
        parser.error("--max-retries must be between 0 and 20")
    args.work_root.mkdir(parents=True, exist_ok=True)
    progress_path = args.work_root / "progress.json"
    progress = (json.loads(progress_path.read_text(encoding="utf-8"))
                if progress_path.exists() else {})
    certificate = validate_certificate(args.certificate)
    certificate_sha = sha256(args.certificate)
    binary, source_sha, binary_sha = build_auditor(args.work_root)
    token = hf_token()
    revision, remote = catalog(
        args.origin, args.dataset, args.revision, token)
    selected = set(args.square or (
        int(partition["square"]) for partition in certificate["partitions"]))
    valid = {int(partition["square"]) for partition in certificate["partitions"]}
    if not selected.issubset(valid):
        parser.error("--square must name a certified fixed Devil square")

    for partition in certificate["partitions"]:
        square = int(partition["square"])
        if square not in selected:
            continue
        cached = progress.get(str(square))
        if (cached and cached.get("sidecar_sha256") == partition["sidecar_sha256"] and
                cached.get("dataset_revision") == revision and
                cached.get("audit_source_sha256") == source_sha):
            print(f"reusing {partition['label']} checkpoint", flush=True)
            continue
        entries = payloads(
            partition, remote, args.origin, args.dataset, revision, token)
        print(
            f"auditing {partition['label']} from {len(entries)} payload(s), "
            f"{sum(int(entry['bytes']) for entry in entries) / 1_000_000_000:.3f} GB",
            flush=True)
        census = audit_partition(
            partition, entries, binary, args.origin, args.dataset, revision,
            token, args.max_retries)
        census.update({
            "label": partition["label"],
            "sidecar_sha256": partition["sidecar_sha256"],
            "dataset_revision": revision,
            "audit_source_sha256": source_sha,
            "audit_binary_sha256": binary_sha,
        })
        progress[str(square)] = census
        write_json(progress_path, progress)
        print(
            f"checkpointed {partition['label']} in "
            f"{census['elapsed_seconds']:.1f}s",
            flush=True)

    expected = {str(partition["square"]) for partition in certificate["partitions"]}
    if set(progress) == expected:
        summary = build_summary(
            certificate, certificate_sha, progress, args.dataset, revision,
            source_sha, binary_sha)
        write_json(args.output, summary)
        print(f"wrote {args.output}", flush=True)
    else:
        print(
            f"partial audit checkpoint: {len(set(progress) & expected)}/12 partitions",
            flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"Devil Minion start audit error: {error}", file=sys.stderr)
        raise
