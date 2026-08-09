#!/usr/bin/env python3
"""Schema and inventory for exact public-information tablebase summaries.

The ordinary ``.uftb`` files solve concrete, perfect-information positions.
Rows containing a Jester or Ghost additionally need a public-information
solution before their README W/L/D counts can describe what a player actually
knows.  This module deliberately contains no solver; it defines the immutable
input inventory and validates the solver's future
``tablebases/information_summary.json`` certificate.

``fresh-maximal-public-view-v2`` means:

* analysis begins with no private draft chronology or prior observations;
* King/Jester identities are maximally ambiguous among the royal silhouettes
  on each team;
* every causally reachable hidden Ghost location compatible with the public
  board is retained, while a visible Ghost is a singleton;
* play uses pure, observation-based strategies and an outcome is a win or loss
  only when that result can be forced for every retained realization;
* before choosing an action, the mover may select each owned piece and
  privately observe the complete UI-visible legal destination/action markers;
  the non-mover does not observe those markers;
* beliefs retain their complete public and owned-private observation history
  and are narrowed only by rule behavior, never by inference from a preferred
  strategy;
* each actual world is classified using both players' information; an owner may
  condition on its own private facts while an uninformed player must use one
  uniform strategy over every retained opponent world;
* each classified actual world is counted once, keeping each README column
  comparable with the dense table;
* unreachable dense-codec records remain outcome-specific annotations rather
  than being silently discarded.

The exact solver is not allowed to truncate a belief set.  In particular, the
live engine's pragmatic 64-world cap is intentionally forbidden here.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Any, Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SUMMARY = ROOT / "tablebases" / "information_summary.json"
SCHEMA_VERSION = 2
SEMANTICS_ID = "fresh-maximal-public-view-v2"
SOLVER_VERSION = "2"
SIDES = ("first", "second")
OUTCOMES = ("win", "loss", "draw")
REACHABILITY = ("legal", "unreachable")
CERTIFICATE_RESIDUALS = (
    "partition_residual",
    "conservation_residual",
    "bellman_residual",
    "rank_residual",
    "observation_residual",
)
OBSERVATION_MODEL_SOURCES = (
    ROOT / "src" / "ultimate" / "information.h",
    ROOT / "src" / "ultimate" / "information.cpp",
)
SHARED_SOLVER_SOURCES = (
    ROOT / "src" / "ultimate" / "position.h",
    ROOT / "src" / "ultimate" / "position.cpp",
    ROOT / "src" / "ultimate" / "information.h",
    ROOT / "src" / "ultimate" / "information.cpp",
    ROOT / "src" / "ultimate" / "information_solver.h",
    ROOT / "src" / "ultimate" / "information_solver.cpp",
)
PRIMARY_JESTER_SOLVER_SOURCES = (
    *SHARED_SOLVER_SOURCES,
    ROOT / "src" / "ultimate" / "tablebase_probe.h",
    ROOT / "src" / "ultimate" / "tablebase_probe.cpp",
    ROOT / "src" / "ultimate" / "tablebase.cpp",
)
GHOST_SOLVER_SOURCES = (
    *SHARED_SOLVER_SOURCES,
    ROOT / "src" / "ultimate" / "ghost_information_tablebase.cpp",
    ROOT / "src" / "ultimate" / "ghost_information_probe.h",
    ROOT / "src" / "ultimate" / "ghost_information_probe.cpp",
)
DOUBLE_JESTER_SOLVER_SOURCES = (
    *SHARED_SOLVER_SOURCES,
    ROOT / "src" / "ultimate" / "double_jester_information_tablebase.cpp",
)
JOINT_JESTER_SOLVER_SOURCES = (
    *SHARED_SOLVER_SOURCES,
    ROOT / "src" / "ultimate" / "joint_jester_information_tablebase.cpp",
)
BISHOP_GHOST_SOLVER_SOURCES = (
    ROOT / "src" / "ultimate" / "position.h",
    ROOT / "src" / "ultimate" / "position.cpp",
    ROOT / "src" / "ultimate" / "information.h",
    ROOT / "src" / "ultimate" / "information.cpp",
    ROOT / "src" / "ultimate" / "ghost_information_probe.h",
    ROOT / "src" / "ultimate" / "ghost_information_probe.cpp",
    ROOT / "src" / "ultimate" / "external_robdd.h",
    ROOT / "src" / "ultimate" / "external_robdd.cpp",
    ROOT / "src" / "ultimate" / "ghost_extra_information_tablebase.cpp",
)

# This tuple is intentionally explicit.  If planner ordering, storage-budget
# selection, or filenames change, a test must consciously update the public-
# information coverage rather than silently dropping a class.
AFFECTED_FILENAMES = (
    "kjesterk.uftb",
    "kghostk.uftb",
    "kjesterjesterk.uftb",
    "kjesterkjester.uftb",
    "kjesterknightk.uftb",
    "kjesterkknight.uftb",
    "kjesterqueenk.uftb",
    "kjesterkqueen.uftb",
    "kjesterrookk.uftb",
    "kjesterkrook.uftb",
    "kjesterbishopk.uftb",
    "kjesterkbishop.uftb",
    "kjesterbombk.uftb",
    "kjesterkbomb.uftb",
    "kjesterninjak.uftb",
    "kjesterkninja.uftb",
    "kjesterturtlek.uftb",
    "kjesterkturtle.uftb",
    "kjestermagek.uftb",
    "kjesterkmage.uftb",
    "kjesterparasitek.uftb",
    "kjesterkparasite.uftb",
    "kjestergiantk.uftb",
    "kjesterkgiant.uftb",
    "kjesterfishermank.uftb",
    "kjesterkfisherman.uftb",
    "kjesterdragonk.uftb",
    "kjesterkdragon.uftb",
    "kbombghostk.uftb",
    "kbishopghostk.uftb",
    "kbishopkghost.uftb",
    "kbombkghost.uftb",
    "kghostdragonk.uftb",
    "kghostfishermank.uftb",
    "kghostghostk.uftb",
    "kghostgiantk.uftb",
    "kghostkdragon.uftb",
    "kghostkfisherman.uftb",
    "kghostkgiant.uftb",
    "kghostkmage.uftb",
    "kghostkparasite.uftb",
    "kghostmagek.uftb",
    "kghostparasitek.uftb",
    "kjesterghostk.uftb",
    "kjesterkghost.uftb",
)

# Exact source domains currently implemented by the repository.  Keep this
# list explicit: similarity of a filename is not proof that a solver's state
# model is closed for that material.  In particular, the symbolic Ghost kernel
# is K+Ghost-v-K only and must never authenticate a Ghost-plus-extra table.
PRIMARY_JESTER_FILENAMES = (
    "kjesterk.uftb",
    "kjesterknightk.uftb", "kjesterkknight.uftb",
    "kjesterqueenk.uftb", "kjesterkqueen.uftb",
    "kjesterrookk.uftb", "kjesterkrook.uftb",
    "kjesterbishopk.uftb", "kjesterkbishop.uftb",
    "kjesterbombk.uftb", "kjesterkbomb.uftb",
    "kjesterninjak.uftb", "kjesterkninja.uftb",
    "kjesterturtlek.uftb", "kjesterkturtle.uftb",
    "kjestermagek.uftb", "kjesterkmage.uftb",
    "kjesterparasitek.uftb", "kjesterkparasite.uftb",
    "kjesterfishermank.uftb", "kjesterkfisherman.uftb",
    "kjesterdragonk.uftb", "kjesterkdragon.uftb",
)
PRIMARY_JESTER_GIANT_FILENAMES = (
    "kjestergiantk.uftb", "kjesterkgiant.uftb",
)

# Capturing the Jester from a primary K+Jester+X-v-K information stratum can
# leave K+X-v-K. The exact solver probes that concrete child through
# TablebaseProbe, so remote/batch deployments must stage these transitive
# inputs in addition to the stratum source and the K+Jester-v-K lower overlay.
# Pieces absent here are native insufficient-material draws and are resolved
# before TablebaseProbe is called.
PRIMARY_JESTER_EXTRA_LOWER_TABLES = {
    "queen": "kqk.uftb",
    "rook": "krk.uftb",
    "bomb": "kbombk.uftb",
    "ninja": "kninjak.uftb",
    "parasite": "kparasitek.uftb",
    "giant": "kgiantk.uftb",
    "dragon": "kdragonk.uftb",
}
PRIMARY_JESTER_INSUFFICIENT_EXTRAS = frozenset({
    "knight", "bishop", "turtle", "mage", "fisherman",
})
SOLVER_DOMAIN_FILENAMES = {
    "primary-jester": PRIMARY_JESTER_FILENAMES,
    # Giant's 2x2 lower-left anchor has a material-specific horizontal
    # reflection. Keep these proofs in a separate hash domain so a future
    # Giant-codec correction cannot silently authenticate point-piece strata.
    "primary-jester-giant": PRIMARY_JESTER_GIANT_FILENAMES,
    "ghost": ("kghostk.uftb",),
    "double-jester": ("kjesterjesterk.uftb",),
    "joint-jester": ("kjesterkjester.uftb",),
    "bishop-ghost": ("kbishopghostk.uftb",),
}
SOLVER_DOMAIN_SOURCES = {
    "primary-jester": PRIMARY_JESTER_SOLVER_SOURCES,
    "primary-jester-giant": PRIMARY_JESTER_SOLVER_SOURCES,
    "ghost": GHOST_SOLVER_SOURCES,
    "double-jester": DOUBLE_JESTER_SOLVER_SOURCES,
    "joint-jester": JOINT_JESTER_SOLVER_SOURCES,
    "bishop-ghost": BISHOP_GHOST_SOLVER_SOURCES,
}
_ROUTED_FILENAMES = tuple(
    filename for filenames in SOLVER_DOMAIN_FILENAMES.values()
    for filename in filenames)
if (len(_ROUTED_FILENAMES) != len(set(_ROUTED_FILENAMES)) or
        not set(_ROUTED_FILENAMES) <= set(AFFECTED_FILENAMES) or
        set(SOLVER_DOMAIN_FILENAMES) != set(SOLVER_DOMAIN_SOURCES)):
    raise RuntimeError("information solver-domain inventory is inconsistent")

SEMANTICS = {
    "id": SEMANTICS_ID,
    "initial_view": "fresh-maximal-public-view",
    "draft_or_observation_history": "none",
    "royal_identity": "all-publicly-indistinguishable-king-jester-assignments",
    "hidden_ghosts": "all-causally-reachable-publicly-compatible-locations",
    "visible_ghosts": "singleton",
    "strategy": "pure-owned-observation-based-sure-outcome",
    "private_strategy": "conditioned-on-owned-private-facts-only",
    "pre_decision_legal_markers":
        "mover-private-all-selectable-owned-piece-destination-action-markers",
    "legal_marker_observer": "side-to-move-only",
    "belief_update":
        "history-preserving-public-and-owned-private-observations",
    "policy_inference": "none-non-signaling",
    "admission_domain": "native-necessary-reachability-audit",
    "outcome_weighting": "concrete-realizations",
    "unreachable_records": "retained-by-outcome",
}

_SHA256 = re.compile(r"[0-9a-f]{64}\Z")


class SummaryValidationError(ValueError):
    """Raised when an information-summary document is not an exact certificate."""


def _planner() -> Any:
    tools = str(Path(__file__).resolve().parent)
    if tools not in sys.path:
        sys.path.insert(0, tools)
    import plan_ultimate_tablebases  # pylint: disable=import-outside-toplevel
    return plan_ultimate_tablebases


def affected_inventory(*, root: Path = ROOT,
                       require_files: bool = True) -> tuple[dict[str, object], ...]:
    """Return the 45 stored classes whose public view hides a Jester or Ghost."""
    planned = _planner().inventory()
    selected = tuple(
        dict(record) for record in planned
        if ({str(record["primary"]), str(record["secondary"])}
            & {"jester", "ghost"})
        and (root / "tablebases" / str(record["filename"])).exists()
    )
    filenames = tuple(str(record["filename"]) for record in selected)
    if filenames != AFFECTED_FILENAMES:
        missing = sorted(set(AFFECTED_FILENAMES) - set(filenames))
        extra = sorted(set(filenames) - set(AFFECTED_FILENAMES))
        if require_files and missing:
            raise SummaryValidationError(
                "public-information inventory is incomplete; missing stored tables: "
                + ", ".join(missing))
        # ``require_files=False`` is useful for constructing test/future roots,
        # but the planner's logical inventory must still remain exactly known.
        logical = tuple(
            str(record["filename"]) for record in planned
            if ({str(record["primary"]), str(record["secondary"])}
                & {"jester", "ghost"})
        )
        if logical != AFFECTED_FILENAMES:
            raise SummaryValidationError(
                f"planner public-information inventory drift (missing={missing}, "
                f"extra={extra})")
        selected = tuple(dict(record) for record in planned
                         if str(record["filename"]) in AFFECTED_FILENAMES)
    return selected


def inventory_fingerprint(records: Sequence[Mapping[str, object]] | None = None) -> str:
    """Hash the logical class layout that a summary claims to cover."""
    if records is None:
        records = affected_inventory()
    normalized = [
        {
            "filename": str(record["filename"]),
            "primary": str(record["primary"]),
            "secondary": str(record["secondary"]),
            "opposing": bool(record["opposing"]),
            "states_per_side": states_per_side(record),
        }
        for record in records
    ]
    payload = json.dumps(normalized, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def semantics_fingerprint() -> str:
    """Hash the complete epistemic contract independently of source layout."""
    payload = json.dumps(
        {"schema_version": SCHEMA_VERSION, "semantics": SEMANTICS},
        sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def _source_fingerprint(paths: Sequence[Path], *, domain: str,
                        root: Path = ROOT) -> str:
    digest = hashlib.sha256()
    # UFIW2 has no separate semantics field, so its model SHA must bind the
    # epistemic contract as well as C++ source bytes.  This makes every v1
    # overlay fail closed even when its concrete table is unchanged.
    contract = json.dumps(
        {"domain": domain, "schema_version": SCHEMA_VERSION,
         "semantics": SEMANTICS}, sort_keys=True,
        separators=(",", ":")).encode()
    digest.update(len(contract).to_bytes(8, "little"))
    digest.update(contract)
    for path in paths:
        relative = path.relative_to(ROOT).as_posix().encode()
        payload = (root / path.relative_to(ROOT)).read_bytes()
        digest.update(len(relative).to_bytes(4, "little"))
        digest.update(relative)
        digest.update(len(payload).to_bytes(8, "little"))
        digest.update(payload)
    return digest.hexdigest()


def observation_model_fingerprint() -> str:
    """Bind certificates to projection code and the v2 observation contract."""
    return _source_fingerprint(
        OBSERVATION_MODEL_SOURCES, domain="observation-model")


def supported_solver_inventory() -> tuple[tuple[str, str], ...]:
    """Return every currently solvable affected filename and exact domain."""
    by_filename = {
        filename: domain
        for domain, filenames in SOLVER_DOMAIN_FILENAMES.items()
        for filename in filenames
    }
    return tuple((filename, by_filename[filename])
                 for filename in AFFECTED_FILENAMES if filename in by_filename)


def unsupported_solver_inventory() -> tuple[str, ...]:
    """Return affected classes lacking a material-correct exact solver."""
    supported = {filename for filename, _ in supported_solver_inventory()}
    return tuple(filename for filename in AFFECTED_FILENAMES
                 if filename not in supported)


def solver_domain(filename: str) -> str:
    """Return the exact implementation domain, rejecting every open class."""
    if filename not in AFFECTED_FILENAMES:
        raise SummaryValidationError(f"unknown information class {filename}")
    for domain, filenames in SOLVER_DOMAIN_FILENAMES.items():
        if filename in filenames:
            return domain
    raise SummaryValidationError(
        f"unsupported information class {filename}; no material-correct "
        "exact solver is implemented")


def solver_model_fingerprint(filename: str, *, root: Path = ROOT) -> str:
    """Bind one class to the move generator and exact proof kernel it used.

    The filename argument deliberately makes this a per-class contract. Ghost
    and two-sided-information solvers can add their own source sets without
    invalidating completed one-primary-Jester proofs.
    """
    domain = solver_domain(filename)
    return _source_fingerprint(
        SOLVER_DOMAIN_SOURCES[domain], domain=f"solver-model:{domain}",
        root=root)


def solver_concrete_dependencies(filename: str) -> tuple[str, ...]:
    """Return every transitive ``.uftb`` required by an exact solver.

    This is a deployment manifest, not a search hint. Missing dependencies
    must fail before a costly graph build rather than surfacing hours later at
    the first lower-material probe.
    """
    domain = solver_domain(filename)
    if domain not in {"primary-jester", "primary-jester-giant"}:
        return ()
    if filename == "kjesterk.uftb":
        return ()
    records = {str(record["filename"]): record
               for record in affected_inventory(root=ROOT,
                                                require_files=False)}
    record = records[filename]
    secondary = str(record["secondary"])
    dependencies = ["kjesterk.uftb"]
    lower = PRIMARY_JESTER_EXTRA_LOWER_TABLES.get(secondary)
    if lower:
        dependencies.append(lower)
    elif secondary not in PRIMARY_JESTER_INSUFFICIENT_EXTRAS:
        raise SummaryValidationError(
            f"{filename}: primary-Jester lower dependency for {secondary} "
            "is not classified")
    return tuple(dependencies)


def states_per_side(record: Mapping[str, object]) -> int:
    """Return the concrete codec representatives in one side-to-move half.

    The legacy K+K+1 codec retains both horizontal reflections, so its packed
    header has twice the planner's symmetry-budget state estimate and one
    side-to-move half equals ``record['states']``. Four-model codecs apply the
    planned reflection fold and split the planner count evenly by turn.
    """
    states = int(record["states"])
    return states if record["phase"] == "kings+1" else states // 2


def _fail(location: str, message: str) -> None:
    raise SummaryValidationError(f"{location}: {message}")


def _object(value: object, location: str, keys: set[str]) -> Mapping[str, object]:
    if not isinstance(value, Mapping):
        _fail(location, "expected an object")
    actual = set(value)
    if actual != keys:
        _fail(location,
              f"expected keys {sorted(keys)}, got {sorted(map(str, actual))}")
    return value


def _natural(value: object, location: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        _fail(location, "expected a non-negative integer")
    return value


def _logical_sha256(path: Path) -> str:
    tools = str(Path(__file__).resolve().parent)
    if tools not in sys.path:
        sys.path.insert(0, tools)
    import ultimate_tablebase_shards as shards  # pylint: disable=import-outside-toplevel
    return shards.logical_sha256(path)


def validate_summary(document: object, *, root: Path = ROOT,
                     verify_source_hashes: bool = True) -> None:
    """Validate completeness, conservation, SHA binding, and exactness.

    Validation succeeds only for all 45 affected classes.  Every starting-side
    result must account for every dense concrete realization, and every exact-
    solver residual must be zero.  ``belief_cap`` must be JSON null.
    """
    top = _object(document, "$", {
        "schema_version", "semantics", "inventory_sha256", "solver", "files",
    })
    if top["schema_version"] != SCHEMA_VERSION:
        _fail("$.schema_version", f"expected {SCHEMA_VERSION}")
    if top["semantics"] != SEMANTICS:
        _fail("$.semantics", f"expected the exact {SEMANTICS_ID} definition")

    records = affected_inventory(root=root, require_files=verify_source_hashes)
    expected_fingerprint = inventory_fingerprint(records)
    if top["inventory_sha256"] != expected_fingerprint:
        _fail("$.inventory_sha256", f"expected {expected_fingerprint}")

    solver = _object(top["solver"], "$.solver", {
        "name", "version", "observation_model_sha256", "exhaustive", "belief_cap",
    })
    if not isinstance(solver["name"], str) or not solver["name"]:
        _fail("$.solver.name", "expected a non-empty string")
    if solver["version"] != SOLVER_VERSION:
        _fail("$.solver.version", f"expected {SOLVER_VERSION}")
    expected_observation = observation_model_fingerprint()
    if solver["observation_model_sha256"] != expected_observation:
        _fail("$.solver.observation_model_sha256",
              f"expected {expected_observation}")
    if solver["exhaustive"] is not True:
        _fail("$.solver.exhaustive", "must be true")
    if solver["belief_cap"] is not None:
        _fail("$.solver.belief_cap", "must be null; capped beliefs are not exact")

    files = top["files"]
    if not isinstance(files, Mapping):
        _fail("$.files", "expected an object")
    expected_files = {str(record["filename"]) for record in records}
    if set(files) != expected_files:
        missing = sorted(expected_files - set(files))
        extra = sorted(set(files) - expected_files)
        _fail("$.files", f"coverage mismatch (missing={missing}, extra={extra})")

    for record in records:
        filename = str(record["filename"])
        location = f"$.files.{filename}"
        entry = _object(files[filename], location, {
            "tablebase_sha256", "solver_model_sha256", "states_per_side", "sides",
        })
        digest = entry["tablebase_sha256"]
        if not isinstance(digest, str) or not _SHA256.fullmatch(digest):
            _fail(f"{location}.tablebase_sha256", "expected lowercase SHA-256")
        path = root / "tablebases" / filename
        if verify_source_hashes:
            actual_digest = _logical_sha256(path)
            if digest != actual_digest:
                _fail(f"{location}.tablebase_sha256",
                      f"source mismatch; expected {actual_digest}")
        expected_solver = solver_model_fingerprint(filename)
        if entry["solver_model_sha256"] != expected_solver:
            _fail(f"{location}.solver_model_sha256",
                  f"expected {expected_solver}")

        expected_states = states_per_side(record)
        states = _natural(entry["states_per_side"], f"{location}.states_per_side")
        if states != expected_states:
            _fail(f"{location}.states_per_side", f"expected {expected_states}")
        sides = _object(entry["sides"], f"{location}.sides", set(SIDES))
        for side_name in SIDES:
            side_location = f"{location}.sides.{side_name}"
            side = _object(sides[side_name], side_location,
                           {"outcomes", "certificate"})
            outcomes = _object(side["outcomes"], f"{side_location}.outcomes",
                               set(OUTCOMES))
            reachable_total = 0
            unreachable_total = 0
            for outcome in OUTCOMES:
                counts = _object(outcomes[outcome],
                                 f"{side_location}.outcomes.{outcome}",
                                 set(REACHABILITY))
                reachable_total += _natural(
                    counts["legal"],
                    f"{side_location}.outcomes.{outcome}.legal")
                unreachable_total += _natural(
                    counts["unreachable"],
                    f"{side_location}.outcomes.{outcome}.unreachable")
            if reachable_total + unreachable_total != states:
                _fail(side_location,
                      "W/L/D legal and unreachable counts do not conserve "
                      f"{states} concrete realizations")

            certificate = _object(side["certificate"],
                                  f"{side_location}.certificate", {
                                      "information_sets", "concrete_realizations",
                                      "legal_realizations", "unreachable_realizations",
                                      "unresolved_information_sets",
                                      *CERTIFICATE_RESIDUALS,
                                  })
            information_sets = _natural(
                certificate["information_sets"],
                f"{side_location}.certificate.information_sets")
            if states and information_sets == 0:
                _fail(f"{side_location}.certificate.information_sets",
                      "must be positive for a non-empty table")
            if information_sets > states:
                _fail(f"{side_location}.certificate.information_sets",
                      "cannot exceed the number of concrete realizations")
            expected_certificate_values = {
                "concrete_realizations": states,
                "legal_realizations": reachable_total,
                "unreachable_realizations": unreachable_total,
                "unresolved_information_sets": 0,
                **{residual: 0 for residual in CERTIFICATE_RESIDUALS},
            }
            for field, expected in expected_certificate_values.items():
                value = _natural(certificate[field],
                                 f"{side_location}.certificate.{field}")
                if value != expected:
                    _fail(f"{side_location}.certificate.{field}",
                          f"expected {expected}, got {value}")


def load_and_validate(path: Path = DEFAULT_SUMMARY, *, root: Path = ROOT,
                      verify_source_hashes: bool = True) -> dict[str, object]:
    """Load a JSON summary and return it only after exact validation."""
    document = json.loads(path.read_text())
    validate_summary(document, root=root, verify_source_hashes=verify_source_hashes)
    if not isinstance(document, dict):  # Established by ``validate_summary``.
        raise AssertionError("validated summary is not a dictionary")
    return document


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", type=Path, default=DEFAULT_SUMMARY)
    parser.add_argument(
        "--no-source-hash", action="store_true",
        help="validate schema/certificates without rereading logical .uftb payloads")
    args = parser.parse_args()
    load_and_validate(args.path, root=ROOT,
                      verify_source_hashes=not args.no_source_hash)
    print(f"validated exact public-information summary: {args.path}")


if __name__ == "__main__":
    main()
