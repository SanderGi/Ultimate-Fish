#!/usr/bin/env python3
"""Run one concrete shard with an exact post-frontier native checkpoint.

The production shard runner deliberately uses ``--checkpoint-every 0`` for a
parallel frontier scan.  On a bounded AWS recovery lane we prefer a native
checkpoint containing the complete node and degree planes before reverse-graph
construction begins.  This thin wrapper changes only that process argument;
the generator model, inventory, verification, preservation, and S3 code remain
the committed production runner's implementation.
"""

from __future__ import annotations

from collections.abc import Mapping
import sys

import run_ultimate_concrete_tablebase_shard_aws as concrete


BASE_CLASS_COMMAND = concrete.class_command
EXPECTED_MODEL = "ac9b2d323aeca8705b79c8cb720242985f82ec24fc561ef6b3417d5d603948e6"
EXPECTED_INVENTORY = "b82a3d87a42ba342b5b611d068783876e3ecf2701e7684400534b937bbda4fd0"


def checkpointed_class_command(
        record: Mapping[str, object], *, dry_run: int = 0,
        dry_run_begin: int = 0) -> list[str]:
    command = BASE_CLASS_COMMAND(
        record, dry_run=dry_run, dry_run_begin=dry_run_begin)
    if dry_run:
        return command
    option = command.index("--checkpoint-every") + 1
    if command[option] != "0":
        raise RuntimeError("production checkpoint default changed")
    states = int(record["states"])
    if states <= 0 or states > 0xFFFFFFFF:
        raise RuntimeError("checkpoint state extent is outside the native codec")
    command[option] = str(states)
    return command


def main(argv: list[str] | None = None) -> int:
    if concrete.generator_model_sha256() != EXPECTED_MODEL:
        raise RuntimeError("checkpointed shard source is not the certified model")
    if concrete.inventory_sha256() != EXPECTED_INVENTORY:
        raise RuntimeError("checkpointed shard inventory binding residual")
    concrete.class_command = checkpointed_class_command
    return concrete.main(sys.argv[1:] if argv is None else argv)


if __name__ == "__main__":
    raise SystemExit(main())
