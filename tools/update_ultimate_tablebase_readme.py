#!/usr/bin/env python3
"""Regenerate the checked-in tablebase summary table from packed files."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
from typing import Mapping

import plan_ultimate_tablebases as plan
import summarize_ultimate_concrete_certificates as concrete_certificates
import summarize_ultimate_tablebases as summarize
import ultimate_information_tablebases as information
import ultimate_tablebase_shards as shards


ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "tablebases" / "README.md"
START = "<!-- GENERATED_TABLE_START -->"
END = "<!-- GENERATED_TABLE_END -->"


def certified_artifacts(
        report: concrete_certificates.Report | None,
) -> dict[str, concrete_certificates.Artifact]:
    """Index fully restored concrete artifacts by logical filename.

    ``load_report`` has already authenticated the content-addressed certificate,
    its versioned S3 object, fresh download, and archive restore.  Keeping this
    conversion narrow makes it impossible for the README path to accept an
    unverified size-only JSON export.
    """
    if report is None:
        return {}
    result = {artifact.filename: artifact for artifact in report.artifacts}
    if len(result) != len(report.artifacts):
        raise ValueError("concrete certificate report has duplicate filenames")
    return result


def certified_compressed_cell(
        filename: str,
        digest: str,
        raw_bytes: int,
        artifacts: Mapping[str, concrete_certificates.Artifact],
        require: bool = False,
) -> str:
    """Return an exact preserved archive size after binding it to table bytes."""
    artifact = artifacts.get(filename)
    if artifact is None:
        if require:
            raise ValueError(
                f"{filename}: no restored concrete compression certificate")
        return "—"
    if artifact.output_sha256 != digest:
        raise ValueError(
            f"{filename}: certificate output SHA-256 does not match table")
    if artifact.raw_bytes != raw_bytes:
        raise ValueError(
            f"{filename}: certificate raw byte count does not match table")
    return f"{artifact.compressed_bytes:,}"


def display_name(record: dict[str, object]) -> str:
    first = str(record["primary"]).replace("copycat", "Copycat").title()
    second = str(record["secondary"]).title()
    if not second:
        return f"King+{first} vs King"
    if record["opposing"]:
        return f"King+{first} vs King+{second}"
    if first == second:
        return f"King+2 {first}s vs King"
    return f"King+{first}+{second} vs King"


def cached_rows(text: str) -> dict[str, str]:
    """Return previously generated rows keyed by their logical filename.

    Exact table files are immutable once generated. Reusing an older row when
    its file is no newer than the README avoids rereading gigabytes on every
    incremental batch. ``--full`` remains the authoritative end-to-end audit.
    """
    begin = text.index(START)
    end = text.index(END, begin)
    result: dict[str, str] = {}
    for line in text[begin:end].splitlines():
        if not line.startswith("| `"):
            continue
        # Rows generated before compressed preservation was certificate-bound
        # have six fields.  They remain safe summary caches, but their absent
        # size must be represented explicitly rather than shifting SHA-256
        # into the new compressed-size column.
        if line.count("|") == 7:
            body = line[:-2]
            prefix, separator, digest = body.rpartition(" | ")
            if not separator:
                continue
            line = f"{prefix} | — | {digest} |"
        elif line.count("|") != 8:
            continue
        filename, separator, _rest = line[3:].partition("` |")
        if separator:
            result[filename] = line
    return result


def dependency_mtime(information_summary: Path = information.DEFAULT_SUMMARY) -> int:
    """Return the newest input timestamp that invalidates cached README rows."""
    return max(Path(__file__).stat().st_mtime_ns,
               Path(summarize.__file__).stat().st_mtime_ns,
               Path(plan.__file__).stat().st_mtime_ns,
               Path(information.__file__).stat().st_mtime_ns,
               Path(shards.__file__).stat().st_mtime_ns,
               summarize.REACHABILITY.stat().st_mtime_ns,
               summarize.GIANT_CODEC_STATUS.stat().st_mtime_ns,
               information_summary.stat().st_mtime_ns)


def _public_counts(catalog: Mapping[str, object], filename: str,
                   side: str) -> tuple[list[int], list[int]]:
    """Extract exact legal and unreachable W/L/D certificate buckets."""
    try:
        files = catalog["files"]
        if not isinstance(files, Mapping):
            raise TypeError("files is not an object")
        entry = files[filename]
        if not isinstance(entry, Mapping):
            raise TypeError("file entry is not an object")
        sides = entry["sides"]
        if not isinstance(sides, Mapping):
            raise TypeError("sides is not an object")
        side_entry = sides[side]
        if not isinstance(side_entry, Mapping):
            raise TypeError("side entry is not an object")
        outcomes = side_entry["outcomes"]
        if not isinstance(outcomes, Mapping):
            raise TypeError("outcomes is not an object")
        legal = [0]
        unreachable = [0]
        for outcome in information.OUTCOMES:
            result = outcomes[outcome]
            if not isinstance(result, Mapping):
                raise TypeError(f"{outcome} is not an object")
            for field, counts in (("legal", legal),
                                  ("unreachable", unreachable)):
                value = result[field]
                if (isinstance(value, bool) or not isinstance(value, int) or
                        value < 0):
                    raise TypeError(
                        f"{outcome}.{field} is not a non-negative integer")
                counts.append(value)
        return legal, unreachable
    except (KeyError, TypeError) as error:
        raise information.SummaryValidationError(
            f"{filename}: missing validated public-information result for "
            f"{side} side ({error})") from error


def public_information_cell(catalog: Mapping[str, object], filename: str,
                            side: str, concrete_unreachable: list[int]) -> str:
    """Render exact public-information W/L/D and causal-unreachable buckets.

    The exhaustive solver can prove a stronger causal admission domain than an
    older concrete audit (notably for impossible hidden-Ghost histories). Its
    outcome-specific certificate therefore supplies the parentheses. The
    native audit must remain a subset, guarding against accidental admission of
    a position that the concrete generator already proved unreachable.
    """
    legal, unreachable = _public_counts(catalog, filename, side)
    if len(concrete_unreachable) != 4 or any(
            isinstance(value, bool) or not isinstance(value, int) or value < 0
            for value in concrete_unreachable):
        raise ValueError(f"{filename}: malformed concrete unreachable counts")
    try:
        files = catalog["files"]
        if not isinstance(files, Mapping):
            raise TypeError("files is not an object")
        entry = files[filename]
        if not isinstance(entry, Mapping):
            raise TypeError("file entry is not an object")
        states = entry["states_per_side"]
    except (KeyError, TypeError) as error:
        raise information.SummaryValidationError(
            f"{filename}: missing validated states_per_side") from error
    if isinstance(states, bool) or not isinstance(states, int) or states < 0:
        raise information.SummaryValidationError(
            f"{filename}: invalid validated states_per_side")
    for result in (1, 2, 3):
        if concrete_unreachable[result] > unreachable[result]:
            raise information.SummaryValidationError(
                f"{filename}: exact information admission accepts "
                f"{concrete_unreachable[result] - unreachable[result]} "
                f"native-audited unreachable result-{result} states")
    accounted = sum(legal[1:]) + sum(unreachable[1:])
    if accounted != states:
        raise information.SummaryValidationError(
            f"{filename}: public legal counts plus exact unreachable buckets "
            f"account for {accounted} states, expected {states}")
    return " / ".join(
        summarize.result_cell(legal[result] + unreachable[result],
                              unreachable[result])
        for result in (1, 2, 3))


def summary_cells(filename: str, totals: list[list[int]],
                  concrete_unreachable: list[list[int]],
                  catalog: Mapping[str, object]) -> tuple[str, str]:
    """Render both starting-side cells, requiring information where hidden."""
    if filename in information.AFFECTED_FILENAMES:
        # Absence or a domain mismatch is fatal. Falling back to the concrete
        # perfect-information W/L/D would silently publish the wrong claim.
        first = public_information_cell(
            catalog, filename, information.SIDES[0], concrete_unreachable[0])
        second = public_information_cell(
            catalog, filename, information.SIDES[1], concrete_unreachable[1])
        return first, second
    return (summarize.cell(totals[0], concrete_unreachable[0]),
            summarize.cell(totals[1], concrete_unreachable[1]))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--full", action="store_true",
        help="reread, validate, summarize, and hash every packed table")
    parser.add_argument(
        "--concrete-certificates", type=Path,
        help=("restored sha256/*/wave-certificate.json tree used to add exact "
              "versioned-S3 archive sizes"))
    parser.add_argument(
        "--require-certified-compression", action="store_true",
        help="fail if any rendered table lacks a restored compression certificate")
    args = parser.parse_args()
    if args.require_certified_compression and args.concrete_certificates is None:
        parser.error("--require-certified-compression requires "
                     "--concrete-certificates")

    certificate_report = (
        concrete_certificates.load_report(args.concrete_certificates)
        if args.concrete_certificates is not None else None
    )
    compression = certified_artifacts(certificate_report)

    # Validate all 45 rows, their exact-solver certificates, and their logical
    # table SHA-256 bindings before allowing even cached output to be reused.
    # A missing, partial, capped, or stale catalog is an error by design.
    information_catalog = information.load_and_validate(
        information.DEFAULT_SUMMARY, root=ROOT, verify_source_hashes=True)

    records = {str(record["filename"]): record for record in plan.inventory()}
    # Copycat uses v5 because its one deployable character has a linked clone;
    # the linked K+Copycat-v-K+Bishop extension also carries opposing material.
    ordered = [record for record in plan.inventory()
               if (ROOT / "tablebases" / str(record["filename"])).exists()]
    text = README.read_text()
    old_rows = cached_rows(text)
    readme_mtime = README.stat().st_mtime_ns
    logic_mtime = dependency_mtime(information.DEFAULT_SUMMARY)
    reused = 0
    lines = [
        START,
        "| File | Class | In-class edges | First material owner starts W / L / D | "
        "Second material owner / bare King starts W / L / D | "
        "Preserved compressed bytes | SHA-256 |",
        "| --- | --- | ---: | ---: | ---: | ---: | --- |",
    ]
    for record in ordered:
        path = ROOT / "tablebases" / str(record["filename"])
        # Cached rows must not preserve counts produced by the former
        # point-square reflection of a Giant's 2x2 lower-left anchor.
        summarize.require_current_giant_codec(path)
        cached = old_rows.get(path.name)
        if (not args.full and certificate_report is None and
                logic_mtime <= readme_mtime and cached is not None
                and path.stat().st_mtime_ns <= readme_mtime):
            lines.append(cached)
            reused += 1
            continue
        data = shards.read_logical(path)
        _magic, version, _piece, _count, edges = struct.unpack_from("<8sIIII", data)
        if version >= 6:
            edges = struct.unpack_from("<Q", data, 48)[0]
        digest = hashlib.sha256(data).hexdigest()
        totals, illegal = summarize.summary(path, data, digest)
        first_cell, second_cell = summary_cells(
            path.name, totals, illegal, information_catalog)
        compressed_cell = certified_compressed_cell(
            path.name, digest, len(data), compression,
            require=args.require_certified_compression)
        lines.append(
            f"| `{path.name}` | {display_name(record)} | {edges:,} | "
            f"{first_cell} | {second_cell} | {compressed_cell} | `{digest}` |")
    lines.append(END)

    begin = text.index(START)
    end = text.index(END, begin) + len(END)
    README.write_text(text[:begin] + "\n".join(lines) + text[end:])
    print(f"updated {README} with {len(ordered)} tables ({reused} cached)")


if __name__ == "__main__":
    main()
