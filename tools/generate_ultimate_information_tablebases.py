#!/usr/bin/env python3
"""Run and checkpoint exact public-information tablebase strata.

The C++ solver emits proof counters and a per-concrete-world ``.ufiw`` overlay.
This driver binds a completed run to its logical ``.uftb`` SHA-256 and records
the strict JSON schema consumed by the README updater. Partial checkpoints live
outside the repository by default; only a complete 45-row, fully validated
catalog may be promoted to ``tablebases/information_summary.json``.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import fcntl
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
from typing import Mapping


ROOT = Path(__file__).resolve().parents[1]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import plan_ultimate_tablebases as plan  # noqa: E402
import ultimate_information_tablebases as information  # noqa: E402
import ultimate_tablebase_shards as shards  # noqa: E402


# v1 artifacts are intentionally preserved under ``*pre-legal-dots-v1*``.
# New defaults use a disjoint namespace in addition to strict semantic/model
# hash validation, so a pre-decision-legal-marker solve can never resume or
# reuse an old observation game accidentally.
DEFAULT_CHECKPOINT = Path(
    "/tmp/ultimatefish-information-summary.legal-dots-v2.partial.json")
DEFAULT_OVERLAYS = Path(
    "/tmp/ultimatefish-information-overlays.legal-dots-v2")
SUMMARY_RE = re.compile(
    r"^information_summary side (?P<side>[01]) "
    r"win (?P<win>\d+) loss (?P<loss>\d+) draw (?P<draw>\d+) "
    r"unreachable_win (?P<uwin>\d+) "
    r"unreachable_loss (?P<uloss>\d+) "
    r"unreachable_draw (?P<udraw>\d+) "
    r"sets (?P<sets>\d+) concrete (?P<concrete>\d+) "
    r"bellman_residual (?P<bellman>\d+) rank_residual (?P<rank>\d+) "
    r"belief_cap none exhaustive 1$")
FIXED_RE = re.compile(
    r"^information_fixed_point .* bellman_residual (?P<bellman>\d+) "
    r"rank_residual (?P<rank>\d+)$")
SYMBOLIC_RE = re.compile(
    r"^information_symbolic_certificate .* "
    r"bellman_residual (?P<bellman>\d+) "
    r"monotonicity_residual (?P<monotonicity>\d+) "
    r"singleton_residual (?P<singleton>\d+) "
    r"belief_cap none powerset_exact 1$")
PIECE_TYPE_IDS = {
    "": 30, "jester": 1, "knight": 2, "pawn": 3, "queen": 4,
    "rook": 5, "bishop": 6, "berserker": 7, "bomb": 8,
    "ninja": 9, "turtle": 10, "ghost": 11, "mage": 12,
    "penguin": 14, "parasite": 15, "devil": 16, "sludge": 18,
    "sniper": 19, "prince": 20, "checker": 21, "giant": 23,
    "copycat": 24, "angel": 26, "fisherman": 28, "dragon": 29,
}


def _records() -> dict[str, Mapping[str, object]]:
    return {str(record["filename"]): record for record in plan.inventory()}


def _empty_document() -> dict[str, object]:
    records = information.affected_inventory()
    return {
        "schema_version": information.SCHEMA_VERSION,
        "semantics": dict(information.SEMANTICS),
        "inventory_sha256": information.inventory_fingerprint(records),
        "solver": {
            "name": "ultimatefish-exact-observation-game",
            "version": information.SOLVER_VERSION,
            "observation_model_sha256":
                information.observation_model_fingerprint(),
            "exhaustive": True,
            "belief_cap": None,
        },
        "files": {},
    }


def load_checkpoint(path: Path) -> dict[str, object]:
    if not path.exists():
        return _empty_document()
    document = json.loads(path.read_text())
    expected = _empty_document()
    for key in ("schema_version", "semantics", "inventory_sha256", "solver"):
        if document.get(key) != expected[key]:
            raise RuntimeError(
                f"{path}: stale {key}; start a new checkpoint for this solver")
    if not isinstance(document.get("files"), dict):
        raise RuntimeError(f"{path}: files must be an object")
    for filename, entry in document["files"].items():
        try:
            expected_model = information.solver_model_fingerprint(filename)
        except information.SummaryValidationError as error:
            raise RuntimeError(f"{path}: {error}") from error
        if (not isinstance(entry, dict) or
                entry.get("solver_model_sha256") != expected_model):
            raise RuntimeError(
                f"{path}: stale solver model for completed {filename}")
    return document


def save_checkpoint(path: Path, document: Mapping[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


@contextmanager
def checkpoint_lock(path: Path):
    """Serialize read/modify/write updates from concurrent exact solves."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lock_path = path.with_suffix(path.suffix + ".lock")
    with lock_path.open("a+b") as stream:
        fcntl.flock(stream.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def merge_checkpoint_entry(path: Path, filename: str,
                           entry: Mapping[str, object]) -> dict[str, object]:
    """Atomically merge one completed proof without losing parallel results."""
    with checkpoint_lock(path):
        document = load_checkpoint(path)
        files = document["files"]
        assert isinstance(files, dict)
        files[filename] = dict(entry)
        save_checkpoint(path, document)
        return document


def overlay_is_current(path: Path, record: Mapping[str, object],
                       source_sha256: str, model_sha256: str) -> bool:
    """Validate the small header and exact dense-domain size before reuse."""
    try:
        with path.open("rb") as stream:
            header = stream.read(160)
        if len(header) != 160:
            return False
        (magic, version, primary, secondary, color, count,
         _substates) = struct.unpack_from("<8s6I", header)
        expected_count = information.states_per_side(record) * 2
        return (magic == b"UFIW2\0\0\0" and version == 2 and
                primary == PIECE_TYPE_IDS[str(record["primary"])] and
                secondary == PIECE_TYPE_IDS[str(record["secondary"])] and
                color == int(bool(record["opposing"])) and
                count == expected_count and
                header[32:96].decode() == source_sha256 and
                header[96:160].decode() == model_sha256 and
                path.stat().st_size == 160 + expected_count)
    except (OSError, UnicodeDecodeError, struct.error):
        return False


def arbitrary_is_current(path: Path, source_sha256: str,
                         model_sha256: str, *,
                         lower_jester_overlay_sha256: str | None = None,
                         dragon_orientation: int | None = None,
                         fisherman_orientation: int | None = None,
                         mage_orientation: int | None = None,
                         parasite_orientation: int | None = None) -> bool:
    """Authenticate a permanent exact arbitrary-belief artifact."""
    try:
        with path.open("rb") as stream:
            prefix = stream.read(16)
            if len(prefix) != 16:
                return False
            magic = prefix[:8]
            header_bytes = struct.unpack_from("<I", prefix, 12)[0]
            if magic == b"UFGG1\0\0\0":
                expected_header, payload_offset = 928, 152
                first_section_offset = 104
                source_offset, model_offset, payload_sha_offset = 160, 224, 800
                semantics_offset = 864
                semantics = b"correlated-unordered-pair-public-view-v1"
            elif magic == b"UFJG1\0\0\0":
                expected_header, payload_offset = 816, 168
                first_section_offset = 120
                source_offset, model_offset, payload_sha_offset = 176, 240, 688
                semantics_offset = 752
                semantics = b"king-jester-x-hidden-ghost-correlated-v1"
                expected_version = 1
            elif magic == b"UFGX2\0\0\0":
                expected_header, payload_offset = 988, 148
                first_section_offset = 92
                source_offset, model_offset, payload_sha_offset = 156, 220, 860
                semantics_offset = 924
                semantics = b"fresh-maximal-public-view-v2:reciprocal-bishop-ghost"
                expected_version = 2
            elif magic == b"UFGD1\0\0\0":
                expected_header, payload_offset = 1248, 152
                first_section_offset = 96
                source_offset, model_offset, payload_sha_offset = 160, 288, 1120
                semantics_offset = 1184
                semantics = b"fresh-maximal-public-view-v2:dragon-ghost-generic"
                expected_version = 1
            elif magic == b"UFGB1\0\0\0":
                expected_header, payload_offset = 1248, 152
                first_section_offset = 96
                source_offset, model_offset, payload_sha_offset = 160, 288, 1120
                semantics_offset = 1184
                semantics = b"fresh-maximal-public-view-v2:bomb-ghost-generic"
                expected_version = 1
            elif magic == b"UFGF1\0\0\0":
                expected_header, payload_offset = 1056, 152
                first_section_offset = 96
                source_offset, model_offset, payload_sha_offset = 160, 288, 928
                semantics_offset = 992
                semantics = b"fresh-maximal-public-view-v2:fisherman-ghost-generic"
                expected_version = 1
            elif magic == b"UFMG1\0\0\0":
                expected_header, payload_offset = 1056, 152
                first_section_offset = 96
                source_offset, model_offset, payload_sha_offset = 160, 288, 928
                semantics_offset = 992
                semantics = b"fresh-maximal-public-view-v2:mage-ghost-generic"
                expected_version = 1
            elif magic == b"UFGP1\0\0\0":
                expected_header, payload_offset = 1248, 152
                first_section_offset = 96
                source_offset, model_offset, payload_sha_offset = 160, 288, 1120
                semantics_offset = 1184
                semantics = b"fresh-maximal-public-view-v2:parasite-ghost-generic"
                expected_version = 1
            else:
                return False
            if magic not in {b"UFGX2\0\0\0", b"UFGD1\0\0\0",
                             b"UFGB1\0\0\0", b"UFGF1\0\0\0",
                             b"UFMG1\0\0\0", b"UFGP1\0\0\0"}:
                expected_version = 1
            if header_bytes != expected_header:
                return False
            stream.seek(0)
            header = stream.read(expected_header)
            if len(header) != expected_header:
                return False
            payload_bytes = struct.unpack_from("<Q", header,
                                               payload_offset)[0]
            digest = hashlib.sha256()
            remaining = payload_bytes
            while remaining:
                chunk = stream.read(min(1 << 20, remaining))
                if not chunk:
                    return False
                digest.update(chunk)
                remaining -= len(chunk)
            if stream.read(1):
                return False
        if magic == b"UFGG1\0\0\0":
            dependencies_current = (
                header[288:352].decode() ==
                    information.observation_model_fingerprint() and
                header[544:608].decode() ==
                    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        elif magic == b"UFJG1\0\0\0":
            dependencies_current = (
                header[304:368].decode() ==
                    information.observation_model_fingerprint() and
                lower_jester_overlay_sha256 is not None and
                header[560:624].decode() ==
                    lower_jester_overlay_sha256 and
                header[624:688].decode() ==
                    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        elif magic == b"UFGD1\0\0\0":
            lower_dragon_sha = shards.logical_sha256(
                ROOT / "tablebases" / "kdragonk.uftb")
            dependencies_current = (
                header[352:416].decode() ==
                    information.observation_model_fingerprint() and
                header[416:480].decode() ==
                    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b" and
                header[480:544].decode() == lower_dragon_sha and
                header[544:608].decode() == lower_dragon_sha and
                header[608:672].decode() ==
                    information.concrete_tablebase_model_fingerprint(
                        "kdragonk.uftb"))
        elif magic == b"UFGB1\0\0\0":
            lower_bomb_sha = shards.logical_sha256(
                ROOT / "tablebases" / "kbombk.uftb")
            dependencies_current = (
                header[352:416].decode() ==
                    information.observation_model_fingerprint() and
                header[416:480].decode() ==
                    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b" and
                header[480:544].decode() == lower_bomb_sha and
                header[544:608].decode() == lower_bomb_sha and
                header[608:672].decode() ==
                    information.concrete_tablebase_model_fingerprint(
                        "kbombk.uftb"))
        elif magic in {b"UFGF1\0\0\0", b"UFMG1\0\0\0"}:
            dependencies_current = (
                header[352:416].decode() ==
                    information.observation_model_fingerprint() and
                header[416:480].decode() ==
                    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        elif magic == b"UFGP1\0\0\0":
            lower_parasite_sha = shards.logical_sha256(
                ROOT / "tablebases" / "kparasitek.uftb")
            dependencies_current = (
                header[352:416].decode() ==
                    information.observation_model_fingerprint() and
                header[416:480].decode() ==
                    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b" and
                header[480:544].decode() == lower_parasite_sha and
                header[544:608].decode() == lower_parasite_sha and
                header[608:672].decode() ==
                    information.concrete_tablebase_model_fingerprint(
                        "kparasitek.uftb"))
        else:
            dependencies_current = (
                header[284:348].decode() ==
                    information.observation_model_fingerprint() and
                header[348:412].decode() ==
                    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        material_current = True
        if magic == b"UFGD1\0\0\0":
            material_current = (
                struct.unpack_from("<I", header, 20)[0] ==
                    PIECE_TYPE_IDS["ghost"] and
                struct.unpack_from("<I", header, 24)[0] ==
                    PIECE_TYPE_IDS["dragon"] and
                struct.unpack_from("<I", header, 28)[0] == 0 and
                struct.unpack_from("<I", header, 32)[0] in {0, 1} and
                (dragon_orientation is None or
                 struct.unpack_from("<I", header, 32)[0] ==
                    dragon_orientation))
        elif magic == b"UFGF1\0\0\0":
            material_current = (
                struct.unpack_from("<I", header, 20)[0] ==
                    PIECE_TYPE_IDS["ghost"] and
                struct.unpack_from("<I", header, 24)[0] ==
                    PIECE_TYPE_IDS["fisherman"] and
                struct.unpack_from("<I", header, 28)[0] == 0 and
                struct.unpack_from("<I", header, 32)[0] in {0, 1} and
                (fisherman_orientation is None or
                 struct.unpack_from("<I", header, 32)[0] ==
                    fisherman_orientation))
        elif magic == b"UFMG1\0\0\0":
            material_current = (
                struct.unpack_from("<I", header, 20)[0] ==
                    PIECE_TYPE_IDS["ghost"] and
                struct.unpack_from("<I", header, 24)[0] ==
                    PIECE_TYPE_IDS["mage"] and
                struct.unpack_from("<I", header, 28)[0] == 0 and
                struct.unpack_from("<I", header, 32)[0] in {0, 1} and
                (mage_orientation is None or
                 struct.unpack_from("<I", header, 32)[0] ==
                    mage_orientation))
        elif magic == b"UFGP1\0\0\0":
            material_current = (
                struct.unpack_from("<I", header, 20)[0] ==
                    PIECE_TYPE_IDS["ghost"] and
                struct.unpack_from("<I", header, 24)[0] ==
                    PIECE_TYPE_IDS["parasite"] and
                struct.unpack_from("<I", header, 28)[0] == 0 and
                struct.unpack_from("<I", header, 32)[0] in {0, 1} and
                (parasite_orientation is None or
                 struct.unpack_from("<I", header, 32)[0] ==
                    parasite_orientation))
        return (
            header[:8] == magic and
            struct.unpack_from("<I", header, 8)[0] == expected_version and
            struct.unpack_from("<Q", header,
                               first_section_offset)[0] == expected_header and
            header[source_offset:source_offset+64].decode() == source_sha256 and
            header[model_offset:model_offset+64].decode() == model_sha256 and
            header[payload_sha_offset:payload_sha_offset+64].decode() ==
                digest.hexdigest() and
            header[semantics_offset:semantics_offset+64].split(b"\0", 1)[0] ==
                semantics and
            material_current and
            dependencies_current and
            path.stat().st_size == expected_header + payload_bytes)
    except (OSError, UnicodeDecodeError, struct.error):
        return False


def run_solver(command: list[str], *, label: str = "") -> dict[int, dict[str, int]]:
    process = subprocess.Popen(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, bufsize=1)
    assert process.stdout is not None
    summaries: dict[int, dict[str, int]] = {}
    fixed_seen = False
    for raw in process.stdout:
        print(f"[{label}] {raw}" if label else raw, end="", flush=True)
        line = raw.rstrip("\n")
        if match := FIXED_RE.match(line):
            fixed_seen = True
            if int(match["bellman"]) or int(match["rank"]):
                raise RuntimeError("fixed-point solver reported a residual")
        if match := SYMBOLIC_RE.match(line):
            fixed_seen = True
            if any(int(match[field]) for field in
                   ("bellman", "monotonicity", "singleton")):
                raise RuntimeError("symbolic solver reported a residual")
        if match := SUMMARY_RE.match(line):
            values = {key: int(value) for key, value in match.groupdict().items()
                      if key != "side"}
            summaries[int(match["side"])] = values
    return_code = process.wait()
    if return_code:
        raise RuntimeError(f"information solver exited with status {return_code}")
    if not fixed_seen or set(summaries) != {0, 1}:
        raise RuntimeError("information solver output lacks its exact certificate")
    return summaries


def entry_from_run(record: Mapping[str, object],
                   summaries: Mapping[int, Mapping[str, int]]) -> dict[str, object]:
    path = ROOT / "tablebases" / str(record["filename"])
    states = information.states_per_side(record)
    sides: dict[str, object] = {}
    for side_index, side_name in enumerate(information.SIDES):
        summary = summaries[side_index]
        if summary["concrete"] != states:
            raise RuntimeError(
                f"{path.name}: solver counted {summary['concrete']}, expected {states}")
        outcomes = {}
        for outcome, unreachable in (("win", "uwin"),
                                     ("loss", "uloss"),
                                     ("draw", "udraw")):
            outcomes[outcome] = {
                "legal": summary[outcome],
                "unreachable": summary[unreachable],
            }
        legal = sum(summary[outcome] for outcome in information.OUTCOMES)
        unreachable = sum(summary[f"u{outcome}"]
                          for outcome in information.OUTCOMES)
        if legal + unreachable != states:
            raise RuntimeError(f"{path.name}: run does not conserve its domain")
        sides[side_name] = {
            "outcomes": outcomes,
            "certificate": {
                "information_sets": summary["sets"],
                "concrete_realizations": states,
                "legal_realizations": legal,
                "unreachable_realizations": unreachable,
                "unresolved_information_sets": 0,
                "partition_residual": 0,
                "conservation_residual": 0,
                "bellman_residual": summary["bellman"],
                "rank_residual": summary["rank"],
                "observation_residual": 0,
            },
        }
    return {
        "tablebase_sha256": shards.logical_sha256(path),
        "solver_model_sha256":
            information.solver_model_fingerprint(str(record["filename"])),
        "states_per_side": states,
        "sides": sides,
    }


def solver_command(args: argparse.Namespace, record: Mapping[str, object],
                   overlay: Path, source_sha256: str,
                   model_sha256: str) -> list[str]:
    """Build the exact material-domain command, rejecting open models."""
    filename = str(record["filename"])
    try:
        domain = information.solver_domain(filename)
    except information.SummaryValidationError as error:
        raise RuntimeError(str(error)) from error
    table = ROOT / "tablebases" / filename
    common = [
        "--information-source-sha256", source_sha256,
        "--information-model-sha256", model_sha256,
    ]
    if domain in {"primary-jester", "primary-jester-giant"}:
        command = [
            str(args.binary), "--piece", "jester",
            "--solve-jester-information", str(table),
            "--information-overlay", str(overlay),
            "--information-scratch", str(args.scratch),
            *common,
        ]
        secondary = str(record["secondary"])
        if secondary:
            command[3:3] = ["--piece2", secondary]
            if bool(record["opposing"]):
                command[5:5] = ["--opposing"]
            lower_table = ROOT / "tablebases" / "kjesterk.uftb"
            command.extend([
                "--lower-information-overlay",
                str(args.overlays / "kjesterk.ufiw"),
                "--lower-information-source-sha256",
                shards.logical_sha256(lower_table),
                "--lower-information-model-sha256",
                information.solver_model_fingerprint("kjesterk.uftb"),
            ])
        return command
    if domain == "ghost":
        return [
            str(args.ghost_binary), "--input", str(table),
            "--output", str(overlay), "--scratch", str(args.scratch),
            *common,
        ]
    if domain == "bishop-ghost":
        return [
            str(args.ghost_extra_binary), "--material", "same",
            "--input", str(table),
            "--solve-external", str(args.ghost_extra_transitions),
            "--solve-scratch", str(args.scratch / "kbishopghostk-exact"),
            "--information-output", str(overlay),
            "--information-observation-sha256",
            information.observation_model_fingerprint(),
            *common,
        ]
    if domain == "reciprocal-bishop-ghost":
        lower = ROOT / "tablebases" / "kghostk.ufgm"
        payload = lower.read_bytes()
        if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
                struct.unpack_from("<I", payload, 12)[0] != 320):
            raise RuntimeError(f"{lower}: invalid authenticated lower UFGM")
        lower_sha = hashlib.sha256(payload).hexdigest()
        expected_lower_sha = (
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        if lower_sha != expected_lower_sha:
            raise RuntimeError(f"{lower}: stale lower UFGM SHA-256 {lower_sha}")
        return [
            str(args.reciprocal_ghost_extra_binary), "--solve",
            "--transition-prefix", str(args.reciprocal_ghost_extra_transitions),
            "--input", str(table), "--lower-ghost-sidecar", str(lower),
            "--scratch", str(args.scratch / "kbishopkghost-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / "kbishopkghost.ufgx"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
            "--lower-sidecar-sha256", lower_sha,
            "--lower-source-sha256", payload[96:160].decode(),
            "--lower-model-sha256", payload[160:224].decode(),
            "--lower-observation-sha256", payload[224:288].decode(),
        ]
    if domain in {"dragon-ghost-same", "dragon-ghost-opposing"}:
        lower = ROOT / "tablebases" / "kghostk.ufgm"
        payload = lower.read_bytes()
        if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
                struct.unpack_from("<I", payload, 12)[0] != 320):
            raise RuntimeError(f"{lower}: invalid authenticated lower UFGM")
        lower_sha = hashlib.sha256(payload).hexdigest()
        expected_lower_sha = (
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        if lower_sha != expected_lower_sha:
            raise RuntimeError(f"{lower}: stale lower UFGM SHA-256 {lower_sha}")
        orientation = ("same" if domain == "dragon-ghost-same"
                       else "opposing")
        transitions = (args.dragon_ghost_same_transitions
                       if orientation == "same"
                       else args.dragon_ghost_opposing_transitions)
        lower_dragon = ROOT / "tablebases" / "kdragonk.uftb"
        lower_dragon_sha = shards.logical_sha256(lower_dragon)
        return [
            str(args.dragon_ghost_binary), "--solve",
            "--orientation", orientation,
            "--transition-prefix", str(transitions),
            "--input", str(table), "--lower-ghost-sidecar", str(lower),
            "--lower-dragon-table", str(lower_dragon),
            "--scratch", str(args.scratch / f"{Path(table).stem}-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / f"{Path(table).stem}.ufgd"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
            "--lower-sidecar-sha256", lower_sha,
            "--lower-source-sha256", payload[96:160].decode(),
            "--lower-model-sha256", payload[160:224].decode(),
            "--lower-observation-sha256", payload[224:288].decode(),
            "--lower-dragon-sha256", lower_dragon_sha,
            "--lower-dragon-source-sha256", lower_dragon_sha,
            "--lower-dragon-model-sha256",
            information.concrete_tablebase_model_fingerprint(
                "kdragonk.uftb"),
            "--compact-every", "1",
        ]
    if domain in {"bomb-ghost-same", "bomb-ghost-opposing"}:
        lower = ROOT / "tablebases" / "kghostk.ufgm"
        payload = lower.read_bytes()
        if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
                struct.unpack_from("<I", payload, 12)[0] != 320):
            raise RuntimeError(f"{lower}: invalid authenticated lower UFGM")
        lower_sha = hashlib.sha256(payload).hexdigest()
        expected_lower_sha = (
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        if lower_sha != expected_lower_sha:
            raise RuntimeError(f"{lower}: stale lower UFGM SHA-256 {lower_sha}")
        orientation = ("same" if domain == "bomb-ghost-same"
                       else "opposing")
        transitions = (args.bomb_ghost_same_transitions
                       if orientation == "same"
                       else args.bomb_ghost_opposing_transitions)
        lower_bomb = ROOT / "tablebases" / "kbombk.uftb"
        lower_bomb_sha = shards.logical_sha256(lower_bomb)
        return [
            str(args.bomb_ghost_binary), "--solve",
            "--orientation", orientation,
            "--transition-prefix", str(transitions),
            "--input", str(table), "--lower-ghost-sidecar", str(lower),
            "--lower-bomb-table", str(lower_bomb),
            "--scratch", str(args.scratch / f"{Path(table).stem}-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / f"{Path(table).stem}.ufgb"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
            "--lower-sidecar-sha256", lower_sha,
            "--lower-source-sha256", payload[96:160].decode(),
            "--lower-model-sha256", payload[160:224].decode(),
            "--lower-observation-sha256", payload[224:288].decode(),
            "--lower-bomb-sha256", lower_bomb_sha,
            "--lower-bomb-source-sha256", lower_bomb_sha,
            "--lower-bomb-model-sha256",
            information.concrete_tablebase_model_fingerprint("kbombk.uftb"),
            "--compact-every", "1",
        ]
    if domain in {"fisherman-ghost-same", "fisherman-ghost-opposing"}:
        lower = ROOT / "tablebases" / "kghostk.ufgm"
        payload = lower.read_bytes()
        if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
                struct.unpack_from("<I", payload, 12)[0] != 320):
            raise RuntimeError(f"{lower}: invalid authenticated lower UFGM")
        lower_sha = hashlib.sha256(payload).hexdigest()
        expected_lower_sha = (
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        if lower_sha != expected_lower_sha:
            raise RuntimeError(f"{lower}: stale lower UFGM SHA-256 {lower_sha}")
        orientation = ("same" if domain == "fisherman-ghost-same"
                       else "opposing")
        transitions = (args.fisherman_ghost_same_transitions
                       if orientation == "same"
                       else args.fisherman_ghost_opposing_transitions)
        return [
            str(args.fisherman_ghost_binary), "--solve",
            "--orientation", orientation,
            "--transition-prefix", str(transitions),
            "--input", str(table), "--lower-ghost-sidecar", str(lower),
            "--scratch", str(args.scratch / f"{Path(table).stem}-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / f"{Path(table).stem}.ufgf"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
            "--lower-sidecar-sha256", lower_sha,
            "--lower-source-sha256", payload[96:160].decode(),
            "--lower-model-sha256", payload[160:224].decode(),
            "--lower-observation-sha256", payload[224:288].decode(),
            "--compact-every", "1",
        ]
    if domain in {"mage-ghost-same", "mage-ghost-opposing"}:
        lower = ROOT / "tablebases" / "kghostk.ufgm"
        payload = lower.read_bytes()
        if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
                struct.unpack_from("<I", payload, 12)[0] != 320):
            raise RuntimeError(f"{lower}: invalid authenticated lower UFGM")
        lower_sha = hashlib.sha256(payload).hexdigest()
        expected_lower_sha = (
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        if lower_sha != expected_lower_sha:
            raise RuntimeError(f"{lower}: stale lower UFGM SHA-256 {lower_sha}")
        orientation = ("same" if domain == "mage-ghost-same"
                       else "opposing")
        transitions = (args.mage_ghost_same_transitions
                       if orientation == "same"
                       else args.mage_ghost_opposing_transitions)
        return [
            str(args.mage_ghost_binary), "--solve",
            "--orientation", orientation,
            "--transition-prefix", str(transitions),
            "--input", str(table), "--lower-ghost-sidecar", str(lower),
            "--scratch", str(args.scratch / f"{Path(table).stem}-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / f"{Path(table).stem}.ufmg"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
            "--lower-sidecar-sha256", lower_sha,
            "--lower-source-sha256", payload[96:160].decode(),
            "--lower-model-sha256", payload[160:224].decode(),
            "--lower-observation-sha256", payload[224:288].decode(),
            "--compact-every", "1",
        ]
    if domain in {"parasite-ghost-same", "parasite-ghost-opposing"}:
        lower = ROOT / "tablebases" / "kghostk.ufgm"
        payload = lower.read_bytes()
        if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
                struct.unpack_from("<I", payload, 12)[0] != 320):
            raise RuntimeError(f"{lower}: invalid authenticated lower UFGM")
        lower_sha = hashlib.sha256(payload).hexdigest()
        expected_lower_sha = (
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        if lower_sha != expected_lower_sha:
            raise RuntimeError(f"{lower}: stale lower UFGM SHA-256 {lower_sha}")
        orientation = ("same" if domain == "parasite-ghost-same"
                       else "opposing")
        transitions = (args.parasite_ghost_same_transitions
                       if orientation == "same"
                       else args.parasite_ghost_opposing_transitions)
        lower_parasite = ROOT / "tablebases" / "kparasitek.uftb"
        lower_parasite_sha = shards.logical_sha256(lower_parasite)
        return [
            str(args.parasite_ghost_binary), "--solve",
            "--orientation", orientation,
            "--transition-prefix", str(transitions),
            "--input", str(table), "--lower-ghost-sidecar", str(lower),
            "--lower-parasite-table", str(lower_parasite),
            "--scratch", str(args.scratch / f"{Path(table).stem}-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / f"{Path(table).stem}.ufgp"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
            "--lower-sidecar-sha256", lower_sha,
            "--lower-source-sha256", payload[96:160].decode(),
            "--lower-model-sha256", payload[160:224].decode(),
            "--lower-observation-sha256", payload[224:288].decode(),
            "--lower-parasite-sha256", lower_parasite_sha,
            "--lower-parasite-source-sha256", lower_parasite_sha,
            "--lower-parasite-model-sha256",
            information.concrete_tablebase_model_fingerprint(
                "kparasitek.uftb"),
            "--compact-every", "1",
        ]
    if domain == "ghost-pair":
        lower = ROOT / "tablebases" / "kghostk.ufgm"
        payload = lower.read_bytes()
        if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
                struct.unpack_from("<I", payload, 12)[0] != 320):
            raise RuntimeError(f"{lower}: invalid authenticated lower UFGM")
        lower_sha = hashlib.sha256(payload).hexdigest()
        lower_source = payload[96:160].decode()
        lower_model = payload[160:224].decode()
        lower_observation = payload[224:288].decode()
        expected_lower_sha = (
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        if lower_sha != expected_lower_sha:
            raise RuntimeError(
                f"{lower}: stale lower UFGM SHA-256 {lower_sha}")
        return [
            str(args.ghost_pair_binary), "--solve",
            "--transition-prefix", str(args.ghost_pair_transitions),
            "--input", str(table), "--lower-ghost-sidecar", str(lower),
            "--scratch", str(args.scratch / "kghostghostk-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / "kghostghostk.ufgg"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
            "--lower-sidecar-sha256", lower_sha,
            "--lower-source-sha256", lower_source,
            "--lower-model-sha256", lower_model,
            "--lower-observation-sha256", lower_observation,
        ]
    if domain == "jester-ghost":
        lower_ghost = ROOT / "tablebases" / "kghostk.ufgm"
        lower_jester = ROOT / "tablebases" / "kjesterk.uftb"
        lower_overlay = args.overlays / "kjesterk.ufiw"
        if not lower_overlay.exists():
            raise RuntimeError(f"{lower_overlay}: required exact lower overlay")
        return [
            str(args.jester_ghost_binary), "--solve",
            "--transition-prefix", str(args.jester_ghost_transitions),
            "--input", str(table),
            "--lower-jester-table", str(lower_jester),
            "--lower-jester-overlay", str(lower_overlay),
            "--lower-jester-model-sha256",
            information.solver_model_fingerprint("kjesterk.uftb"),
            "--lower-jester-overlay-sha256",
            hashlib.sha256(lower_overlay.read_bytes()).hexdigest(),
            "--lower-ghost-sidecar", str(lower_ghost),
            "--lower-ghost-sidecar-sha256",
            hashlib.sha256(lower_ghost.read_bytes()).hexdigest(),
            "--scratch", str(args.scratch / "kjesterghostk-exact"),
            "--output", str(overlay), "--output-arbitrary",
            str(args.overlays / "kjesterghostk.ufjg"),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--observation-sha256",
            information.observation_model_fingerprint(),
        ]

    lower_overlay = args.overlays / "kjesterk.ufiw"
    lower_table = ROOT / "tablebases" / "kjesterk.uftb"
    lower_source = shards.logical_sha256(lower_table)
    lower_model = information.solver_model_fingerprint("kjesterk.uftb")
    if domain == "double-jester":
        return [
            str(args.double_jester_binary), "--input", str(table),
            "--lower-information-overlay", str(lower_overlay),
            "--lower-concrete", str(lower_table),
            "--lower-information-source-sha256", lower_source,
            "--lower-information-model-sha256", lower_model,
            "--output", str(overlay), "--scratch", str(args.scratch),
            *common,
        ]
    if domain == "joint-jester":
        return [
            str(args.joint_jester_binary), "--input", str(table),
            "--lower-table", str(lower_table),
            "--lower-overlay", str(lower_overlay),
            "--source-sha256", source_sha256,
            "--model-sha256", model_sha256,
            "--lower-model-sha256", lower_model,
            "--semantics-id", information.SEMANTICS_ID,
            "--output", str(overlay), "--scratch", str(args.scratch),
        ]
    raise AssertionError(f"unhandled information solver domain {domain}")


def solve_one(args: argparse.Namespace) -> None:
    records = _records()
    if args.filename not in information.AFFECTED_FILENAMES:
        raise RuntimeError(f"{args.filename}: not a Jester/Ghost table")
    record = records[args.filename]
    try:
        domain = information.solver_domain(args.filename)
    except information.SummaryValidationError as error:
        raise RuntimeError(str(error)) from error
    missing_dependencies = [
        dependency for dependency in
        information.solver_concrete_dependencies(args.filename)
        if not (ROOT / "tablebases" / dependency).exists()
    ]
    if missing_dependencies:
        raise RuntimeError(
            f"{args.filename}: missing exact transitive concrete "
            f"dependencies: {', '.join(missing_dependencies)}")
    missing_sidecars = [
        dependency for dependency in
        information.solver_sidecar_dependencies(args.filename)
        if not (ROOT / "tablebases" / dependency).exists()
    ]
    if missing_sidecars:
        raise RuntimeError(
            f"{args.filename}: missing authenticated lower information "
            f"dependencies: {', '.join(missing_sidecars)}")

    args.overlays.mkdir(parents=True, exist_ok=True)
    args.scratch.mkdir(parents=True, exist_ok=True)
    overlay = args.overlays / f"{Path(args.filename).stem}.ufiw"
    source_sha256 = shards.logical_sha256(
        ROOT / "tablebases" / args.filename)
    model_sha256 = information.solver_model_fingerprint(args.filename)
    document = load_checkpoint(args.checkpoint)
    files = document["files"]
    assert isinstance(files, dict)
    entry = files.get(args.filename)
    arbitrary = (args.overlays / "kghostghostk.ufgg" if domain == "ghost-pair"
                 else args.overlays / "kbishopkghost.ufgx"
                 if domain == "reciprocal-bishop-ghost"
                 else args.overlays / f"{Path(args.filename).stem}.ufgd"
                 if domain in {"dragon-ghost-same",
                               "dragon-ghost-opposing"}
                 else args.overlays / f"{Path(args.filename).stem}.ufgb"
                 if domain in {"bomb-ghost-same", "bomb-ghost-opposing"}
                 else args.overlays / f"{Path(args.filename).stem}.ufgf"
                 if domain in {"fisherman-ghost-same",
                               "fisherman-ghost-opposing"}
                 else args.overlays / f"{Path(args.filename).stem}.ufmg"
                 if domain in {"mage-ghost-same", "mage-ghost-opposing"}
                 else args.overlays / f"{Path(args.filename).stem}.ufgp"
                 if domain in {"parasite-ghost-same",
                               "parasite-ghost-opposing"}
                 else args.overlays / "kjesterghostk.ufjg")
    lower_jester_arbitrary_sha = None
    fisherman_orientation = (0 if domain == "fisherman-ghost-same" else
                              1 if domain == "fisherman-ghost-opposing" else
                              None)
    mage_orientation = (0 if domain == "mage-ghost-same" else
                        1 if domain == "mage-ghost-opposing" else None)
    parasite_orientation = (0 if domain == "parasite-ghost-same" else
                            1 if domain == "parasite-ghost-opposing" else None)
    dragon_orientation = (0 if domain == "dragon-ghost-same" else
                          1 if domain == "dragon-ghost-opposing" else None)
    if domain == "jester-ghost":
        lower_jester_arbitrary = args.overlays / "kjesterk.ufiw"
        if lower_jester_arbitrary.exists():
            lower_jester_arbitrary_sha = hashlib.sha256(
                lower_jester_arbitrary.read_bytes()).hexdigest()
    if (isinstance(entry, dict) and
            entry.get("tablebase_sha256") == source_sha256 and
            entry.get("solver_model_sha256") == model_sha256 and
            overlay_is_current(overlay, record, source_sha256, model_sha256) and
            (domain not in {"ghost-pair", "jester-ghost",
                            "reciprocal-bishop-ghost", "dragon-ghost-same",
                            "dragon-ghost-opposing", "bomb-ghost-same",
                            "bomb-ghost-opposing", "fisherman-ghost-same",
                            "fisherman-ghost-opposing", "mage-ghost-same",
                            "mage-ghost-opposing", "parasite-ghost-same",
                            "parasite-ghost-opposing"} or arbitrary_is_current(
                arbitrary, source_sha256, model_sha256,
                lower_jester_overlay_sha256=lower_jester_arbitrary_sha,
                dragon_orientation=dragon_orientation,
                fisherman_orientation=fisherman_orientation,
                mage_orientation=mage_orientation,
                parasite_orientation=parasite_orientation))):
        print(f"already complete and verified: {args.filename}")
        return
    command = solver_command(
        args, record, overlay, source_sha256, model_sha256)
    if domain in {"primary-jester", "primary-jester-giant",
                  "double-jester", "joint-jester"} and (
            str(record["secondary"]) or
            domain not in {"primary-jester", "primary-jester-giant"}):
        lower = args.overlays / "kjesterk.ufiw"
        if not lower.exists():
            raise RuntimeError(
                f"{lower} is required; solve kjesterk.uftb first")

    summaries = run_solver(command, label=args.filename)
    if domain in {"ghost-pair", "jester-ghost",
                  "reciprocal-bishop-ghost", "dragon-ghost-same",
                  "dragon-ghost-opposing", "bomb-ghost-same",
                  "bomb-ghost-opposing", "fisherman-ghost-same",
                  "fisherman-ghost-opposing", "mage-ghost-same",
                  "mage-ghost-opposing", "parasite-ghost-same",
                  "parasite-ghost-opposing"} and not arbitrary_is_current(
            arbitrary, source_sha256, model_sha256,
            lower_jester_overlay_sha256=lower_jester_arbitrary_sha,
            dragon_orientation=dragon_orientation,
            fisherman_orientation=fisherman_orientation,
            mage_orientation=mage_orientation,
            parasite_orientation=parasite_orientation):
        raise RuntimeError(
            f"{arbitrary}: missing/stale authenticated all-beliefs artifact")
    document = merge_checkpoint_entry(
        args.checkpoint, args.filename, entry_from_run(record, summaries))
    files = document["files"]
    assert isinstance(files, dict)
    print(f"checkpointed {args.filename} in {args.checkpoint}")

    if len(files) == len(information.AFFECTED_FILENAMES):
        information.validate_summary(document, root=ROOT, verify_source_hashes=True)
        save_checkpoint(information.DEFAULT_SUMMARY, document)
        print(f"promoted complete exact catalog to {information.DEFAULT_SUMMARY}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "filename", choices=(*information.AFFECTED_FILENAMES, "all-one-jester"))
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "src" / "ultimate_tablebase")
    parser.add_argument("--ghost-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_information_tablebase")
    parser.add_argument("--double-jester-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_double_jester_information_tablebase")
    parser.add_argument("--joint-jester-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_joint_jester_information_tablebase")
    parser.add_argument("--ghost-extra-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_extra_information_preflight")
    parser.add_argument("--reciprocal-ghost-extra-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_reciprocal_bishop_ghost_information_tablebase")
    parser.add_argument("--reciprocal-ghost-extra-transitions", type=Path,
                        default=Path(
                          "/tmp/kbishopkghost-exact-transitions"))
    parser.add_argument("--dragon-ghost-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_dragon_information_tablebase")
    parser.add_argument("--dragon-ghost-same-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostdragonk-exact-transitions"))
    parser.add_argument("--dragon-ghost-opposing-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostkdragon-exact-transitions"))
    parser.add_argument("--bomb-ghost-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_bomb_information_tablebase")
    parser.add_argument("--bomb-ghost-same-transitions", type=Path,
                        default=Path(
                          "/tmp/kbombghostk-exact-transitions"))
    parser.add_argument("--bomb-ghost-opposing-transitions", type=Path,
                        default=Path(
                          "/tmp/kbombkghost-exact-transitions"))
    parser.add_argument("--fisherman-ghost-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_fisherman_information_tablebase")
    parser.add_argument("--fisherman-ghost-same-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostfishermank-exact-transitions"))
    parser.add_argument("--fisherman-ghost-opposing-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostkfisherman-exact-transitions"))
    parser.add_argument("--mage-ghost-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_mage_information_tablebase")
    parser.add_argument("--mage-ghost-same-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostmagek-exact-transitions"))
    parser.add_argument("--mage-ghost-opposing-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostkmage-exact-transitions"))
    parser.add_argument("--parasite-ghost-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_parasite_information_tablebase")
    parser.add_argument("--parasite-ghost-same-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostparasitek-exact-transitions"))
    parser.add_argument("--parasite-ghost-opposing-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostkparasite-exact-transitions"))
    parser.add_argument("--ghost-pair-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_ghost_pair_information_tablebase")
    parser.add_argument("--ghost-pair-transitions", type=Path,
                        default=Path(
                          "/tmp/kghostghostk-exact-transitions"))
    parser.add_argument("--jester-ghost-binary", type=Path,
                        default=ROOT / "src" /
                        "ultimate_jester_ghost_information_tablebase")
    parser.add_argument("--jester-ghost-transitions", type=Path,
                        default=Path(
                          "/tmp/kjesterghostk-exact-transitions"))
    parser.add_argument("--ghost-extra-transitions", type=Path,
                        default=Path(
                          "/tmp/kbishopghostk-exact-transitions"))
    parser.add_argument("--checkpoint", type=Path, default=DEFAULT_CHECKPOINT)
    parser.add_argument("--overlays", type=Path, default=DEFAULT_OVERLAYS)
    parser.add_argument("--scratch", type=Path, default=Path("/tmp"))
    parser.add_argument(
        "--jobs", type=int, default=1,
        help="exact class solves to run concurrently in all-one-jester mode")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    if args.filename == "all-one-jester":
        records = _records()
        filenames = []
        for filename in information.AFFECTED_FILENAMES:
            record = records[filename]
            if (str(record["primary"]) == "jester" and
                    str(record["secondary"]) not in {"jester", "ghost"}):
                filenames.append(filename)

        def solve_filename(filename: str) -> None:
            child = argparse.Namespace(**vars(args))
            child.filename = filename
            solve_one(child)

        if args.jobs == 1:
            for filename in filenames:
                solve_filename(filename)
        else:
            with ThreadPoolExecutor(max_workers=args.jobs) as executor:
                list(executor.map(solve_filename, filenames))
    else:
        if args.jobs != 1:
            parser.error("--jobs is only valid with all-one-jester")
        solve_one(args)


if __name__ == "__main__":
    main()
