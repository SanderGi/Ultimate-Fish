#!/usr/bin/env python3
"""Import restored AWS reachability receipts into the canonical README ledger."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import tempfile

import plan_ultimate_tablebases as plan
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
SUPERVISION = ROOT / "tools/tablebases/ultimate_aws_supervision.json"


def catalog() -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    for record in (*plan.stateful_candidates(),
                   *plan.mirror_copycat_candidates(), *plan.inventory()):
        result[str(record["filename"])] = record
    return result


def hidden(record: dict[str, object]) -> bool:
    return bool({str(record["primary"]), str(record["secondary"])} &
                {"jester", "ghost"})


def cell(total: list[int], unreachable: list[int]) -> str:
    if (len(total) != 4 or len(unreachable) != 4 or total[0] or
            unreachable[0]):
        raise ValueError("WDL side has unknown states or wrong arity")
    parts: list[str] = []
    for outcome in (1, 2, 3):
        if not 0 <= unreachable[outcome] <= total[outcome]:
            raise ValueError("unreachable WDL exceeds total WDL")
        legal = total[outcome] - unreachable[outcome]
        parts.append(f"{legal:,}" +
                     (f" ({unreachable[outcome]:,})"
                      if unreachable[outcome] else ""))
    return " / ".join(parts)


def storage(record: dict[str, object]) -> str:
    table = record["table_archive"]
    wave = record["wave_certificate"]
    reach = record["reachability_s3"]
    return (
        f"S3 table sha256:{table['sha256']} VersionId {table['version_id']}; "
        f"wave certificate sha256:{wave['sha256']} VersionId {wave['version_id']}; "
        f"reachability sha256:{reach['sha256']} VersionId {reach['version_id']}"
    )


def result(record: dict[str, object]) -> dict[str, str]:
    totals = record["totals"]
    unreachable = record["unreachable"]
    first = cell(totals[0], unreachable[0])
    second = cell(totals[1], unreachable[1])
    return {
        "result_kind": "concrete", "first": first, "second": second,
        "reachability": ledger.reachability(first, second),
        "storage": storage(record),
    }


def s3_certificates(record: dict[str, object]) -> list[dict[str, object]]:
    table = record["table_archive"]
    bucket = str(table["bucket"])
    return [
        {"bucket": bucket, "key": table["key"],
         "version_id": table["version_id"], "sha256": table["sha256"],
         "size": table["bytes"]},
        {"bucket": bucket, "key": record["wave_certificate"]["key"],
         "version_id": record["wave_certificate"]["version_id"],
         "sha256": record["wave_certificate"]["sha256"],
         "size": record["wave_certificate"]["bytes"]},
        {"bucket": bucket, "key": record["reachability_s3"]["key"],
         "version_id": record["reachability_s3"]["version_id"],
         "sha256": record["reachability_s3"]["sha256"],
         "size": record["reachability_s3"]["bytes"]},
    ]


def update_supervision(receipts: list[dict[str, object]], path: Path) -> int:
    configuration = json.loads(path.read_text())
    jobs_by_file: dict[str, list[dict[str, object]]] = {}
    for job in configuration.get("jobs", []):
        if job.get("superseded_by"):
            continue
        for filename in job.get("ledger_files", []):
            jobs_by_file.setdefault(str(filename), []).append(job)
    records = catalog()
    for receipt in receipts:
        filename = str(receipt["filename"])
        matches = jobs_by_file.get(filename, [])
        if len(matches) != 1:
            raise ValueError(
                f"{filename}: expected one active supervision job, got "
                f"{len(matches)}")
        material = records[filename]
        job = matches[0]
        job["s3_certificates"] = s3_certificates(receipt)
        if hidden(material):
            # This job certifies a concrete dependency, not the player-view
            # information tablebase that owns the public ledger row.
            job["ledger_certifies"] = False
            job.pop("ledger_results", None)
        else:
            job["ledger_certifies"] = True
            job["ledger_results"] = {filename: result(receipt)}
    with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=path.parent,
            prefix=path.name + ".", delete=False) as stream:
        json.dump(configuration, stream, indent=2)
        stream.write("\n")
        temporary = Path(stream.name)
    temporary.replace(path)
    return len(receipts)


def import_receipts(receipts: list[dict[str, object]], readme: Path) -> tuple[int, int]:
    records = catalog()
    certified: list[str] = []
    stored: list[str] = []
    certified_count = hidden_count = 0
    for receipt in receipts:
        filename = str(receipt["filename"])
        material = records.get(filename)
        if material is None:
            raise ValueError(f"unknown receipt filename: {filename}")
        stored.append(f"{filename}={storage(receipt)}")
        if hidden(material):
            # The concrete artifact and causal audit are exact dependencies,
            # but publishing them as public-information W/L/D would leak the
            # hidden world.  Keep the row PRESERVING/information-required.
            hidden_count += 1
            continue
        value = result(receipt)
        certified.append(filename + "=" + json.dumps(value, separators=(",", ":")))
        certified_count += 1
    ledger.update(readme, [], stored, certified_values=certified)
    return certified_count, hidden_count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipts", type=Path)
    parser.add_argument("--readme", type=Path, default=README)
    parser.add_argument("--supervision-config", type=Path,
                        default=SUPERVISION)
    args = parser.parse_args()
    receipts = json.loads(args.receipts.read_text())
    if not isinstance(receipts, list):
        raise ValueError("receipt document must be a list")
    certified, hidden_dependencies = import_receipts(receipts, args.readme)
    supervised = update_supervision(receipts, args.supervision_config)
    print(json.dumps({"certified": certified,
                      "hidden_dependencies": hidden_dependencies,
                      "supervised": supervised},
                     sort_keys=True))


if __name__ == "__main__":
    main()
