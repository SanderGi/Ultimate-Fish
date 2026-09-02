#!/usr/bin/env python3
"""Add exact parallel solve and certification machinery to Checker v4."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


ROOT = Path(__file__).resolve().parents[2]
BASE_SHA256 = "52afea0cb343cba4e0b17f15083174086385a6d692ce4c969b92d464e647d9a3"
PREFIX = "ultimatefish-checker-action-conditioned-v21-source"


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f"{label} anchor count is {text.count(old)}, not one")
    return text.replace(old, new)


def tar_info(name: str, payload: bytes, mode: int = 0o644) -> tarfile.TarInfo:
    result = tarfile.TarInfo(name)
    result.size = len(payload)
    result.mode = mode
    result.mtime = result.uid = result.gid = 0
    result.uname = result.gname = ""
    return result


def parallelize_legacy(source: str) -> str:
    source = replace_once(source, "#include <algorithm>\n",
                          "#include <algorithm>\n#include <atomic>\n",
                          "atomic include")
    source = replace_once(source, "#include <cstring>\n",
                          "#include <cstring>\n#include <exception>\n",
                          "exception include")
    source = replace_once(source, "#include <map>\n",
                          "#include <map>\n#include <mutex>\n",
                          "mutex include")
    source = replace_once(source, "#include <string>\n",
                          "#include <string>\n#include <thread>\n",
                          "thread include")
    source = replace_once(
        source, "        blocks_.clear();\n",
        "        std::lock_guard<std::mutex> lock(blocksMutex_);\n"
        "        blocks_.clear();\n", "transition stream lock")
    source = replace_once(
        source, "    std::ifstream blocks_;\n",
        "    std::mutex blocksMutex_;\n    std::ifstream blocks_;\n",
        "transition mutex member")
    source = replace_once(
        source, "    ExternalRobdd::Limits bddLimits;\n",
        "    ExternalRobdd::Limits bddLimits;\n"
        "    std::uint32_t workers = 32;\n"
        "    bool resumeConverged = false;\n", "solve worker options")
    grouping_call = "fresh_root_public_grouping_self_test(material_)"
    if grouping_call not in source:
        grouping_call = "fresh_root_public_grouping_self_test()"
    source = replace_once(
        source, f"        {grouping_call};\n    }}\n\n"
        "    void solve() {\n",
        f"        {grouping_call};\n"
        "        std::cout << \"ghost_extra_external_workers \""
        " << options_.workers << \"\\n\" << std::flush;\n"
        "    }\n\n    void solve() {\n", "worker announcement")

    current = (ROOT / "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp").read_text()
    worker_begin = current.index("    struct BellmanSweepCounts {")
    worker_end = current.index("\n  private:\n", worker_begin)
    worker_block = current[worker_begin:worker_end]
    # Some transition-compatible legacy sources predate the named adjacent-
    # King helper used by the current parallel singleton verifier.  Keep the
    # verifier exact while making the transplanted block self-contained.
    worker_block = worker_block.replace(
        "if (real_kings_adjacent(state)) {",
        "if (std::abs(int(state.whiteKing % Position::BoardFiles) -\n"
        "                             int(state.blackKing % Position::BoardFiles)) <= 1 &&\n"
        "                    std::abs(int(state.whiteKing / Position::BoardFiles) -\n"
        "                             int(state.blackKing / Position::BoardFiles)) <= 1) {",
    )
    legacy_solve_begin = source.index("    void solve() {")
    legacy_solve_end = source.index("\n  private:\n", legacy_solve_begin)
    source = source[:legacy_solve_begin] + worker_block + source[legacy_solve_end:]

    compact_old = ("        auto [fresh, certificate] = bdd_->compact(\n"
                   "          replacement, options_.scratch + \".bdd-remap\", roots);\n")
    compact_new = ("        auto [fresh, certificate] = bdd_->compact(\n"
                   "          replacement, options_.scratch + \".bdd-remap\", roots,\n"
                   "          options_.workers);\n")
    source = replace_once(source, compact_old, compact_new,
                          "parallel compaction")

    verify_begin = source.index("    void verify() {")
    loop_begin = source.index("        // One independent full Bellman pass", verify_begin)
    residual_begin = source.index("        std::uint64_t bellmanResidual = 0;", loop_begin)
    verify_sweep = (
        "        // Exact disjoint-geometry Bellman equality pass.\n"
        "        const auto verifyStarted = std::chrono::steady_clock::now();\n"
        "        (void)bellman_sweep(\n"
        "          \"ghost_extra_external_verify_bellman\",\n"
        "          \"external verify\", verifyStarted);\n")
    source = source[:loop_begin] + verify_sweep + source[residual_begin:]

    # Verification lives below the class's private: boundary, outside
    # worker_block. Replace its exact Bellman residual/monotonicity partition
    # and the following singleton proof explicitly.
    verify_anchor = "    void verify() {"
    singleton_anchor = "    [[nodiscard]] std::uint64_t verify_singletons() {"
    singleton_end_anchor = "\n    void report_fresh_roots() {"
    current_verify_begin = current.index(verify_anchor)
    current_singleton_begin = current.index(singleton_anchor)
    current_singleton_end = current.index(
        singleton_end_anchor, current_singleton_begin)
    source_verify_begin = source.index(verify_anchor)
    source_singleton_begin = source.index(singleton_anchor)
    source_singleton_end = source.index(
        singleton_end_anchor, source_singleton_begin)
    source = (source[:source_verify_begin]
              + current[current_verify_begin:current_singleton_begin]
              + source[source_singleton_begin:])
    source_singleton_begin = source.index(singleton_anchor)
    source_singleton_end = source.index(
        singleton_end_anchor, source_singleton_begin)
    singleton_block = current[
        current_singleton_begin:current_singleton_end].replace(
            "valid_world(state, material_)", "valid_world(state)")
    singleton_block = singleton_block.replace(
        "if (real_kings_adjacent(state)) {",
        "if (std::abs(int(state.whiteKing % Position::BoardFiles) -\n"
        "                             int(state.blackKing % Position::BoardFiles)) <= 1 &&\n"
        "                    std::abs(int(state.whiteKing / Position::BoardFiles) -\n"
        "                             int(state.blackKing / Position::BoardFiles)) <= 1) {",
    )
    source = (source[:source_singleton_begin]
              + singleton_block
              + source[source_singleton_end:])

    # The previous v20 package transplanted the parallel Bellman worker but
    # left the legacy relation model and block compiler in place.  Its manifest
    # therefore claimed action-conditioned observer images while the running
    # recurrence still collapsed observations across distinct actions.  Move
    # all three supporting regions as one compatibility unit: data model,
    # stored-edge compiler, and symbolic successor composition.
    def transplant(begin: str, end: str, label: str) -> None:
        nonlocal source
        current_begin = current.index(begin)
        current_end = current.index(end, current_begin)
        source_begin = source.index(begin)
        source_end = source.index(end, source_begin)
        source = (source[:source_begin]
                  + current[current_begin:current_end]
                  + source[source_end:])

    transplant(
        "struct ExternalSolverRelation {",
        "struct ExternalSolverBlock {",
        "action observation data model",
    )
    transplant(
        "[[nodiscard]] ExternalSolverBlock build_external_solver_block(",
        "struct ExternalGhostExtraSolveOptions {",
        "action observation block compiler",
    )
    transplant(
        "    [[nodiscard]] std::vector<ExternalRobdd::Id> relation_image(",
        "    void swap_force_arrays() {",
        "action observation symbolic successor",
    )
    source = replace_once(
        source,
        "        inherited_lower_mask_self_test();\n",
        "        external_action_observation_conditioning_self_test();\n"
        "        inherited_lower_mask_self_test();\n",
        "action observation startup self-test",
    )
    return source


def parallelize_reciprocal(source: str) -> str:
    """Route the reciprocal Checker fixed point through the parallel sweep.

    Checker v4 enters through run_reciprocal_fixed_point rather than the
    material-generic solve() method.  The latter was parallelized first, but
    leaving this adapter's duplicate serial loop in place meant the running
    Checker binary still used one core.
    """
    function = source.index("[[nodiscard]] bool run_reciprocal_fixed_point(")
    loop = source.index("        std::uint64_t changedOwner = 0;", function)
    swap = source.index("        solver.swap_force_arrays();", loop)
    replacement = (
        "        const auto changed = solver.bellman_sweep(\n"
        "          \"reciprocal_ghost_extra_bellman\",\n"
        "          \"reciprocal\", started);\n"
        "        const std::uint64_t changedOwner = changed.changedOwner;\n"
        "        const std::uint64_t changedObserver = changed.changedObserver;\n"
        "        const std::uint64_t changedVisible = changed.changedVisible;\n"
    )
    return source[:loop] + replacement + source[swap:]


def build(base: Path, output: Path) -> dict[str, object]:
    base_payload = base.read_bytes()
    if digest(base_payload) != BASE_SHA256:
        raise RuntimeError("Checker v4 source base hash mismatch")
    members: dict[str, tuple[bytes, int]] = {}
    with tarfile.open(fileobj=io.BytesIO(base_payload), mode="r:gz") as archive:
        for member in archive.getmembers():
            if not member.isfile():
                continue
            extracted = archive.extractfile(member)
            if extracted is None:
                raise RuntimeError(f"cannot read {member.name}")
            members[member.name.removeprefix("./")] = (
                extracted.read(), member.mode)

    ghost_path = "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp"
    members[ghost_path] = (
        parallelize_legacy(members[ghost_path][0].decode()).encode(), 0o644)
    reciprocal_path = (
        "src/ultimate/tablebases/ghost_public_extra_information_solver.cpp"
    )
    members[reciprocal_path] = (
        parallelize_reciprocal(
            members[reciprocal_path][0].decode()).encode(), 0o644)
    replacements = (
        "src/ultimate/tablebases/external_robdd.cpp",
        "src/ultimate/tablebases/external_robdd.h",
        "tests/tablebases/ultimate_external_robdd.cpp",
    )
    for relative in replacements:
        members[relative] = ((ROOT / relative).read_bytes(), 0o644)
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": "ultimate-checker-action-conditioned-source-v21",
        "semantics": (
            "checker-v4-exact-model-with-thread-safe-parallel-bellman-"
            "verification-compaction-reciprocal-adapter-and-expanded-"
            "unique-index-parallel-symbolic-singleton-proofs-and-"
            "one-geometry-work-stealing-and-complete-action-conditioned-"
            "observer-successor-images-v21"),
        "files": [
            {"path": path, "sha256": digest(members[path][0])}
            for path in (ghost_path, reciprocal_path, *replacements)
        ],
    }
    members["CHECKER-PARALLEL-V13-MANIFEST.json"] = (
        (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(),
        0o644)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for relative, (payload, mode) in sorted(members.items()):
            archive.addfile(tar_info(f"{PREFIX}/{relative}", payload, mode),
                            io.BytesIO(payload))
    return {
        "archive": str(output), "sha256": digest(output.read_bytes()),
        "bytes": output.stat().st_size, "manifest": manifest,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.base, args.output), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
