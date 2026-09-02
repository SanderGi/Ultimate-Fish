#!/usr/bin/env python3
"""Print the stored edges for one Ghost-extra transition world."""

from __future__ import annotations

import argparse
import struct
from collections import deque
from functools import lru_cache
from pathlib import Path


OFFSETS = struct.Struct("<81I")
EDGE = struct.Struct("<III4B")
INDEX = struct.Struct("<Q")
SQUARES = 80
FILES = 8


def transform_square(square: int, transform: int) -> int:
    if transform & 1:
        square = (square // FILES) * FILES + FILES - 1 - square % FILES
    if transform & 2:
        square = (9 - square // FILES) * FILES + square % FILES
    return square


@lru_cache(maxsize=None)
def geometry_domain(extra_substates: int, transforms: int) -> tuple[tuple[int, ...], ...]:
    geometries: list[tuple[int, ...]] = []
    for side in range(2):
        for white_king in range(SQUARES):
            for black_king in range(SQUARES):
                if black_king == white_king:
                    continue
                for extra in range(SQUARES):
                    if extra in (white_king, black_king):
                        continue
                    for visible in range(2):
                        for substate in range(extra_substates):
                            raw = (side, white_king, black_king, extra,
                                   visible, substate)
                            images = [
                                (side, transform_square(white_king, item),
                                 transform_square(black_king, item),
                                 transform_square(extra, item), visible,
                                 substate)
                                for item in range(transforms)
                            ]
                            if raw == min(images):
                                geometries.append(raw)
    return tuple(geometries)


def rank_excluding(square: int, occupied: tuple[int, ...]) -> int:
    return square - sum(item < square for item in occupied)


def concrete_index(geometry: tuple[int, ...], actual: int,
                   extra_substates: int) -> int:
    side, white_king, black_king, extra, visible, substate = geometry
    if white_king % FILES >= FILES // 2:
        white_king = transform_square(white_king, 1)
        black_king = transform_square(black_king, 1)
        extra = transform_square(extra, 1)
        actual = transform_square(actual, 1)
    white_rank = (white_king // FILES) * (FILES // 2) + white_king % (FILES // 2)
    black_rank = rank_excluding(black_king, (white_king,))
    extra_rank = rank_excluding(extra, (white_king, black_king))
    ghost_rank = rank_excluding(actual, (white_king, black_king, extra))
    placement = ((((side * (SQUARES // 2) + white_rank) * (SQUARES - 1) +
                   black_rank) * (SQUARES - 2) + extra_rank) *
                 (SQUARES - 3) + ghost_rank)
    return (placement * extra_substates + substate) * 2 + visible


def table_wdl(path: Path, index: int) -> int:
    data = path.read_bytes()
    if data[:8] != b"UFTB1\0\0\0":
        raise SystemExit("concrete table has an invalid packed header")
    version = struct.unpack_from("<I", data, 8)[0]
    offset = 48 if version == 5 else 56 if version == 6 else 64 if version == 9 else 0
    if not offset or offset + index // 4 >= len(data):
        raise SystemExit("concrete table is truncated or unsupported")
    return (data[offset + index // 4] >> (2 * (index % 4))) & 3


def read_edges(prefix: Path, index_bytes: bytes, geometry: int,
               actual: int) -> list[tuple[int, ...]]:
    begin = INDEX.unpack_from(index_bytes, geometry * INDEX.size)[0]
    end = INDEX.unpack_from(index_bytes, (geometry + 1) * INDEX.size)[0]
    with prefix.with_suffix(".blocks").open("rb") as blocks:
        blocks.seek(begin)
        payload = blocks.read(end - begin)
    offsets = OFFSETS.unpack_from(payload)
    edge_bytes = payload[OFFSETS.size:]
    return [EDGE.unpack_from(edge_bytes, ordinal * EDGE.size)
            for ordinal in range(offsets[actual], offsets[actual + 1])]


def evaluate_bdd(path: Path, root: int, actual: int) -> int:
    with path.open("rb") as nodes:
        while root > 1:
            nodes.seek(root * 9)
            raw = nodes.read(9)
            if len(raw) != 9:
                raise SystemExit("ROBDD root is outside the node arena")
            variable, low, high = struct.unpack("<BII", raw)
            root = high if variable == actual else low
    return root


def lower_sidecar_state(path: Path, lower_index: int) -> tuple[int, ...]:
    data = path.read_bytes()
    if data[:8] != b"UFGM1\0\0\0" or len(data) < 320:
        raise SystemExit("lower Ghost sidecar has an invalid header")
    (version, header_bytes, _piece, owner_color, files, ranks, squares,
     _concrete_count, substates, geometry_count, _stratum_count, node_count,
     node_bytes, geometry_bytes, _stratum_bytes, _reserved) = struct.unpack_from(
         "<16I", data, 8)
    node_offset, geometry_offset, _stratum_offset = struct.unpack_from(
        "<3Q", data, 72)
    if (version, header_bytes, owner_color, files, ranks, squares, substates,
            node_bytes, geometry_bytes, node_offset) != (
                1, 320, 0, 8, 10, 80, 2, 9, 844, 320):
        raise SystemExit("lower Ghost sidecar metadata is unsupported")

    visible = lower_index % 2
    placement = lower_index // 2
    ghost_rank = placement % 78
    placement //= 78
    observer_rank = placement % 79
    placement //= 79
    owner_king = placement % 80
    side = placement // 80
    observer_king = observer_rank + (observer_rank >= owner_king)
    low, high = sorted((owner_king, observer_king))
    actual = ghost_rank
    if actual >= low:
        actual += 1
    if actual >= high:
        actual += 1

    best = (side, owner_king, observer_king, visible)
    best_transform = 0
    for transform in range(1, 4):
        candidate = (side, transform_square(owner_king, transform),
                     transform_square(observer_king, transform), visible)
        if candidate < best:
            best = candidate
            best_transform = transform
    actual = transform_square(actual, best_transform)

    geometry_id = None
    geometry = b""
    for candidate_id in range(geometry_count):
        offset = geometry_offset + candidate_id * geometry_bytes
        candidate = data[offset:offset + geometry_bytes]
        if len(candidate) != geometry_bytes:
            raise SystemExit("lower Ghost sidecar geometry is truncated")
        if tuple(candidate[:4]) == best:
            geometry_id = candidate_id
            geometry = candidate
            break
    if geometry_id is None:
        raise SystemExit("lower Ghost public geometry is absent")

    root = struct.unpack_from("<I", geometry, 364 + actual * 4)[0]
    while root > 1:
        offset = node_offset + root * node_bytes
        variable, low_root, high_root = struct.unpack_from("<BII", data, offset)
        root = high_root if variable == actual else low_root
    visible_owner = geometry[684 + actual]
    return (*best, actual, geometry_id, root, visible_owner, node_count)


def symbolic_observer(prefix: Path, scratch: Path, root_slot: str,
                      bdd_slot: str, geometry: tuple[int, ...],
                      geometry_id: int, actual: int) -> int:
    if geometry[4]:
        path = Path(f"{scratch}.visible-observer-{root_slot}")
        with path.open("rb") as roots:
            roots.seek(geometry_id * SQUARES + actual)
            raw = roots.read(1)
        return raw[0]
    with prefix.with_suffix(".meta").open("rb") as meta:
        meta.seek(geometry_id * 392 + 64 + actual * 4)
        stratum = struct.unpack("<I", meta.read(4))[0]
    with Path(f"{scratch}.observer-{root_slot}").open("rb") as roots:
        roots.seek(stratum * 4)
        root = struct.unpack("<I", roots.read(4))[0]
    return evaluate_bdd(Path(f"{scratch}.bdd-{bdd_slot}.nodes"), root, actual)


def symbolic_owner(prefix: Path, scratch: Path, root_slot: str,
                   bdd_slot: str, geometry: tuple[int, ...],
                   geometry_id: int, actual: int) -> int:
    if geometry[4]:
        path = Path(f"{scratch}.visible-owner-{root_slot}")
        with path.open("rb") as roots:
            roots.seek(geometry_id * SQUARES + actual)
            raw = roots.read(1)
        return raw[0]
    with Path(f"{scratch}.owner-{root_slot}").open("rb") as roots:
        roots.seek((geometry_id * SQUARES + actual) * 4)
        root = struct.unpack("<I", roots.read(4))[0]
    return evaluate_bdd(Path(f"{scratch}.bdd-{bdd_slot}.nodes"), root, actual)


def trace_observer_win(prefix: Path, index_bytes: bytes,
                       geometries: tuple[tuple[int, ...], ...], table: Path,
                       start_geometry: int, start_actual: int,
                       extra_substates: int, limit: int, scratch: Path | None,
                       root_slot: str, bdd_slot: str) -> None:
    table_bytes = table.read_bytes()
    version = struct.unpack_from("<I", table_bytes, 8)[0]
    plane = 48 if version == 5 else 56 if version == 6 else 64 if version == 9 else 0

    def wdl(geometry: int, actual: int) -> int:
        index = concrete_index(geometries[geometry], actual, extra_substates)
        return (table_bytes[plane + index // 4] >> (2 * (index % 4))) & 3

    def observer_wins(geometry: int, actual: int) -> bool:
        side = geometries[geometry][0]
        value = wdl(geometry, actual)
        return (side == 0 and value == 1) or (side == 1 and value == 2)

    start = (start_geometry, start_actual)
    if not observer_wins(*start):
        raise SystemExit("trace root is not a concrete observer win")
    queue = deque([start])
    parent: dict[tuple[int, int], tuple[tuple[int, int], int] | None] = {
        start: None
    }
    visited = 0
    while queue and visited < limit:
        state = queue.popleft()
        visited += 1
        for ordinal, edge in enumerate(read_edges(prefix, index_bytes,
                                                   *state)):
            _, _, child, child_actual, domain, exact, _ = edge
            if domain == 2 and exact & 2:
                path: list[tuple[tuple[int, int], int]] = [(state, ordinal)]
                cursor = state
                while parent[cursor] is not None:
                    previous, previous_ordinal = parent[cursor]
                    path.append((previous, previous_ordinal))
                    cursor = previous
                path.reverse()
                for step, (node, chosen) in enumerate(path):
                    symbolic = (symbolic_observer(
                        prefix, scratch, root_slot, bdd_slot,
                        geometries[node[0]], *node) if scratch else "-")
                    print(f"trace_step={step} geometry={node[0]} "
                          f"actual={node[1]} side={geometries[node[0]][0]} "
                          f"wdl={wdl(*node)} symbolic_observer={symbolic} "
                          f"edge_ordinal={chosen}")
                    if scratch:
                        for child_ordinal, child_edge in enumerate(
                                read_edges(prefix, index_bytes, *node)):
                            _, _, child, child_actual, domain, exact, _ = child_edge
                            if domain == 0:
                                child_symbolic = symbolic_observer(
                                    prefix, scratch, root_slot, bdd_slot,
                                    geometries[child], child, child_actual)
                                child_concrete = observer_wins(child, child_actual)
                            elif domain == 2:
                                child_symbolic = int(bool(exact & 2))
                                child_concrete = child_symbolic
                            else:
                                child_symbolic = child_concrete = "lower"
                            print(
                                f"trace_child step={step} ordinal={child_ordinal} "
                                f"domain={domain} child={child} "
                                f"child_actual={child_actual} exact={exact} "
                                f"concrete_observer={child_concrete} "
                                f"symbolic_observer={child_symbolic}"
                            )
                print(f"trace_terminal domain={domain} exact={exact} "
                      f"visited={visited} residual=0")
                return
            if domain == 1:
                print(f"trace_lower_boundary geometry={state[0]} "
                      f"actual={state[1]} edge_ordinal={ordinal} "
                      f"visited={visited}")
                continue
            if domain != 0 or not observer_wins(child, child_actual):
                continue
            candidate = (child, child_actual)
            if candidate not in parent:
                parent[candidate] = (state, ordinal)
                queue.append(candidate)
    raise SystemExit(f"observer-win terminal not found after {visited} states")


def trace_owner_dominance(prefix: Path, index_bytes: bytes,
                          geometries: tuple[tuple[int, ...], ...],
                          table: Path, start_geometry: int, start_actual: int,
                          extra_substates: int, limit: int, scratch: Path,
                          root_slot: str, bdd_slot: str,
                          ghost_color: int) -> None:
    def expected(geometry: int, actual: int) -> bool:
        value = table_wdl(
            table, concrete_index(geometries[geometry], actual,
                                  extra_substates))
        side = geometries[geometry][0]
        return ((side == ghost_color and value == 1) or
                (side != ghost_color and value == 2))

    queue = deque([(start_geometry, start_actual)])
    parent: dict[tuple[int, int], tuple[tuple[int, int], int] | None] = {
        (start_geometry, start_actual): None
    }
    visited = 0
    while queue and visited < limit:
        state = queue.popleft()
        visited += 1
        if not expected(*state):
            continue
        current = symbolic_owner(prefix, scratch, root_slot, bdd_slot,
                                 geometries[state[0]], *state)
        if current:
            continue
        candidates: list[tuple[int, int, int]] = []
        boundary: list[tuple[int, tuple[int, ...]]] = []
        for ordinal, edge in enumerate(read_edges(prefix, index_bytes,
                                                   *state)):
            _, _, child, child_actual, domain, exact, _ = edge
            if domain == 0 and expected(child, child_actual):
                child_force = symbolic_owner(
                    prefix, scratch, root_slot, bdd_slot,
                    geometries[child], child, child_actual)
                if not child_force:
                    candidates.append((child, child_actual, ordinal))
            elif domain != 0:
                boundary.append((ordinal, edge))
        if not candidates:
            path: list[tuple[tuple[int, int], int | None]] = [(state, None)]
            cursor = state
            while parent[cursor] is not None:
                previous, edge_ordinal = parent[cursor]
                path.append((previous, edge_ordinal))
                cursor = previous
            path.reverse()
            for step, (node, edge_ordinal) in enumerate(path):
                index = concrete_index(geometries[node[0]], node[1],
                                       extra_substates)
                print(f"owner_trace_step={step} geometry={node[0]} "
                      f"actual={node[1]} side={geometries[node[0]][0]} "
                      f"wdl={table_wdl(table, index)} "
                      f"symbolic_owner={symbolic_owner(prefix, scratch, root_slot, bdd_slot, geometries[node[0]], *node)} "
                      f"edge_ordinal={edge_ordinal}")
            print(f"owner_trace_frontier geometry={state[0]} "
                  f"actual={state[1]} same_class_false_children=0 "
                  f"boundary_edges={boundary} visited={visited}")
            return
        for child, child_actual, ordinal in candidates:
            candidate = (child, child_actual)
            if candidate not in parent:
                parent[candidate] = (state, ordinal)
                queue.append(candidate)
    raise SystemExit(f"owner-dominance frontier not found after {visited} states")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    parser.add_argument("geometry", type=int)
    parser.add_argument("actual", type=int)
    parser.add_argument("--table", type=Path)
    parser.add_argument("--extra-substates", type=int, default=1)
    parser.add_argument("--transforms", type=int, choices=(2, 4), default=4)
    parser.add_argument("--trace-observer-win", type=int, metavar="MAX_STATES")
    parser.add_argument("--trace-owner-dominance", type=int,
                        metavar="MAX_STATES")
    parser.add_argument("--ghost-color", type=int, choices=(0, 1), default=1)
    parser.add_argument("--scratch", type=Path)
    parser.add_argument("--lower-sidecar", type=Path)
    parser.add_argument("--root-slot", choices=("current", "next"),
                        default="current")
    parser.add_argument("--bdd-slot", choices=("a", "b"), default="b")
    args = parser.parse_args()
    if args.geometry < 0 or not -1 <= args.actual < 80:
        parser.error("geometry must be nonnegative and actual must be -1..79")

    index_bytes = args.prefix.with_suffix(".index").read_bytes()
    index_count = len(index_bytes) // INDEX.size
    if len(index_bytes) % INDEX.size or args.geometry + 1 >= index_count:
        raise SystemExit("transition index is malformed or geometry is absent")
    begin = INDEX.unpack_from(index_bytes, args.geometry * INDEX.size)[0]
    end = INDEX.unpack_from(index_bytes, (args.geometry + 1) * INDEX.size)[0]
    with args.prefix.with_suffix(".blocks").open("rb") as blocks:
        blocks.seek(begin)
        payload = blocks.read(end - begin)
    if len(payload) != end - begin or len(payload) < OFFSETS.size:
        raise SystemExit("transition block is truncated")
    offsets = OFFSETS.unpack_from(payload)
    edge_bytes = payload[OFFSETS.size:]
    if len(edge_bytes) % EDGE.size or offsets[-1] != len(edge_bytes) // EDGE.size:
        raise SystemExit("transition edge extent is malformed")
    actuals = range(80) if args.actual == -1 else (args.actual,)
    geometries = (geometry_domain(args.extra_substates, args.transforms)
                  if args.table else ())
    if args.table and args.actual >= 0:
        parent_index = concrete_index(geometries[args.geometry], args.actual,
                                      args.extra_substates)
        print(f"parent_concrete_index={parent_index} "
              f"parent_concrete_wdl={table_wdl(args.table, parent_index)} "
              f"parent_geometry_tuple={geometries[args.geometry]}")
        if args.scratch:
            print(f"parent_symbolic_owner={symbolic_owner(args.prefix, args.scratch, args.root_slot, args.bdd_slot, geometries[args.geometry], args.geometry, args.actual)} "
                  f"parent_symbolic_observer={symbolic_observer(args.prefix, args.scratch, args.root_slot, args.bdd_slot, geometries[args.geometry], args.geometry, args.actual)}")
    if args.trace_observer_win:
        if not args.table or args.actual < 0:
            parser.error("--trace-observer-win requires --table and one actual")
        trace_observer_win(args.prefix, index_bytes, geometries, args.table,
                           args.geometry, args.actual, args.extra_substates,
                           args.trace_observer_win, args.scratch,
                           args.root_slot, args.bdd_slot)
        return
    if args.trace_owner_dominance:
        if not args.table or not args.scratch or args.actual < 0:
            parser.error("--trace-owner-dominance requires --table, --scratch, and one actual")
        trace_owner_dominance(
            args.prefix, index_bytes, geometries, args.table, args.geometry,
            args.actual, args.extra_substates, args.trace_owner_dominance,
            args.scratch, args.root_slot, args.bdd_slot, args.ghost_color)
        return
    for actual in actuals:
        edge_count = offsets[actual + 1] - offsets[actual]
        if args.actual != -1:
            print(f"geometry={args.geometry} actual={actual} edges={edge_count}")
        for ordinal in range(offsets[actual], offsets[actual + 1]):
            relation, action, child, child_actual, domain, exact, reserved = (
                EDGE.unpack_from(edge_bytes, ordinal * EDGE.size)
            )
            if args.actual == -1 and domain != 2:
                continue
            child_detail = ""
            if args.table and domain == 0:
                index = concrete_index(geometries[child], child_actual,
                                       args.extra_substates)
                child_detail = (
                    f" child_geometry_tuple={geometries[child]} "
                    f" child_concrete_index={index} "
                    f"child_concrete_wdl={table_wdl(args.table, index)}"
                )
                if args.scratch:
                    child_detail += (
                        f" child_symbolic_owner={symbolic_owner(args.prefix, args.scratch, args.root_slot, args.bdd_slot, geometries[child], child, child_actual)}"
                        f" child_symbolic_observer={symbolic_observer(args.prefix, args.scratch, args.root_slot, args.bdd_slot, geometries[child], child, child_actual)}"
                    )
            elif args.lower_sidecar and domain == 1:
                lower = lower_sidecar_state(args.lower_sidecar, child)
                child_detail = (
                    " lower_side=" + str(lower[0]) +
                    " lower_owner_king=" + str(lower[1]) +
                    " lower_observer_king=" + str(lower[2]) +
                    " lower_visible=" + str(lower[3]) +
                    " lower_actual=" + str(lower[4]) +
                    " lower_geometry=" + str(lower[5]) +
                    " lower_owner_singleton=" + str(lower[6]) +
                    " lower_visible_owner=" + str(lower[7]) +
                    " lower_nodes=" + str(lower[8])
                )
            print(
                f"actual={actual} ordinal={ordinal - offsets[actual]} "
                f"relation={relation} action={action} child={child} "
                f"child_actual={child_actual} domain={domain} exact={exact} "
                f"reserved={reserved}{child_detail}"
            )


if __name__ == "__main__":
    main()
