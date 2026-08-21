#!/usr/bin/env python3
"""Run one exact public-piece/Ghost information class on EC2."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


BASE_GEOMETRIES = 492_960
SHARDS = 64
PIECES = {"knight": "Knight", "ninja": "Ninja", "queen": "Queen",
          "rook": "Rook", "turtle": "Turtle", "pawn": "Pawn",
          "berserker": "Berserker", "sniper": "Sniper",
          "prince": "Prince", "checker": "Checker", "penguin": "Penguin",
          "copycat": "Copycat", "dragon": "Dragon"}
EXTRA_SUBSTATES = {"pawn": 2, "berserker": 10, "sniper": 4,
                   "prince": 2, "checker": 4, "penguin": 8}
LOWER_SUBSTATES = {"penguin": 4}
EXTRA_PRIMARY = {"knight", "ninja", "queen", "rook", "turtle", "pawn",
                 "berserker", "copycat"}
IMPLICIT_DRAWS = {"knight", "turtle", "checker"}
HORIZONTAL_ONLY = {"pawn", "sniper", "checker"}


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def require_sha(path: Path, expected: str, label: str) -> None:
    if not path.is_file() or sha256_path(path) != expected:
        raise RuntimeError(f"{label} SHA-256 mismatch: {path}")


def validate_prebuilt_binding(executable: Path | None,
                              executable_sha256: str,
                              prebuilt_model_sha256: str,
                              model_sha256: str) -> None:
    if ((executable is None) != (not executable_sha256)):
        raise RuntimeError(
            "prebuilt executable and SHA-256 must be supplied together")
    if (prebuilt_model_sha256 and
            (executable is None or prebuilt_model_sha256 != model_sha256)):
        raise RuntimeError(
            "prebuilt executable model binding must equal --model-sha256")


def run(command: list[str], root: Path, log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    if log.exists():
        for attempt in range(1, 10_000):
            archived = log.with_name(
                f"{log.stem}.prior-{attempt:04d}{log.suffix}")
            if not archived.exists():
                log.replace(archived)
                break
        else:
            raise RuntimeError(f"too many retained phase logs for {log}")
    with log.open("xb") as output:
        completed = subprocess.run(command, cwd=root, stdout=output,
                                   stderr=subprocess.STDOUT, check=False)
    if completed.returncode:
        raise RuntimeError(f"command failed ({completed.returncode}): {log}")


def write_manifest(work: Path, args: argparse.Namespace) -> None:
    result_files = sorted((work / "work/results").glob("*"))
    log_files = sorted((work / "work/logs").glob("*.log"))
    if (not any(path.suffix == ".ufiw" for path in result_files) or
            not any(path.suffix != ".ufiw" for path in result_files) or
            not any(path.name == "solve.log" for path in log_files)):
        raise RuntimeError("ordinary Ghost result/proof inventory is incomplete")
    artifacts = [*result_files, *log_files]
    manifest = {
        "schema": "ultimate-ordinary-ghost-artifacts-v1",
        "filename": args.filename, "piece": args.piece,
        "orientation": args.orientation, "source_sha256": args.source_sha256,
        "model_sha256": args.model_sha256,
        "observation_sha256": args.observation_sha256,
        "lower_sha256": args.lower_sha256,
        "lower_model_sha256": args.lower_model_sha256,
        "lower_ghost_sidecar_sha256": args.lower_ghost_sha256,
        "lower_ghost_source_sha256": args.lower_ghost_source_sha256,
        "lower_ghost_model_sha256": args.lower_ghost_model_sha256,
        "lower_ghost_observation_sha256":
            args.lower_ghost_observation_sha256,
        "files": {str(path.relative_to(work)): {
            "bytes": path.stat().st_size, "sha256": sha256_path(path)}
            for path in artifacts},
    }
    if args.piece == "pawn":
        manifest["promoted_queen_ghost"] = {
            "sidecar_sha256": args.promoted_sidecar_sha256,
            "source_sha256": args.promoted_source_sha256,
            "model_sha256": args.promoted_model_sha256,
            "observation_sha256": args.promoted_observation_sha256,
            "lower_ghost_sidecar_sha256":
                args.promoted_lower_ghost_sidecar_sha256,
            "lower_table_sha256": args.promoted_lower_table_sha256,
            "lower_model_sha256": args.promoted_lower_model_sha256,
        }
    (work / "work/artifact-manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n")


def ranges(geometries: int) -> list[tuple[int, int]]:
    base, extra = divmod(geometries, SHARDS)
    cursor = 0
    result = []
    for index in range(SHARDS):
        count = base + (index < extra)
        result.append((cursor, count))
        cursor += count
    if cursor != geometries:
        raise RuntimeError("ordinary Ghost shard coverage residual")
    return result


def geometry_count(piece: str) -> int:
    """Return the complete public geometry domain for an ordinary piece."""
    if piece not in PIECES:
        raise ValueError(f"unknown ordinary Ghost piece: {piece}")
    return (BASE_GEOMETRIES * EXTRA_SUBSTATES.get(piece, 1) *
            (2 if piece in HORIZONTAL_ONLY else 1))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--piece", choices=tuple(PIECES), required=True)
    parser.add_argument("--orientation", choices=("same", "opposing"),
                        required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--lower-table", type=Path)
    parser.add_argument("--lower-sha256", required=True)
    parser.add_argument("--lower-model-sha256", required=True)
    parser.add_argument("--lower-ghost-sidecar", type=Path)
    parser.add_argument("--promoted-sidecar", type=Path)
    parser.add_argument("--promoted-sidecar-sha256", default="")
    parser.add_argument("--promoted-source-sha256", default="")
    parser.add_argument("--promoted-model-sha256", default="")
    parser.add_argument("--promoted-observation-sha256", default="")
    parser.add_argument("--promoted-lower-ghost-sidecar-sha256", default="")
    parser.add_argument("--promoted-lower-table", type=Path)
    parser.add_argument("--promoted-lower-table-sha256", default="")
    parser.add_argument("--promoted-lower-model-sha256", default="")
    parser.add_argument("--lower-ghost-sha256", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    parser.add_argument("--lower-ghost-source-sha256", required=True)
    parser.add_argument("--lower-ghost-model-sha256", required=True)
    parser.add_argument("--lower-ghost-observation-sha256", required=True)
    parser.add_argument("--parallelism", type=int, default=20)
    parser.add_argument("--prebuilt-executable", type=Path,
                        help=("use one SHA-pinned executable for transition "
                              "generation, merge, and solve"))
    parser.add_argument("--prebuilt-executable-sha256", default="")
    parser.add_argument(
        "--prebuilt-model-sha256", default="",
        help=("bind a SHA-pinned prebuilt executable to its certified model "
              "when the compact source bundle omits inventory-only files"))
    parser.add_argument("--solve-max-nodes", type=int, default=500_000_000,
                        help="exact ROBDD node budget for solve-only retries")
    parser.add_argument("--transitions-only", action="store_true",
                        help="build and merge fresh transition shards, then stop")
    parser.add_argument("--resume-transitions", action="store_true",
                        help=("resume a fresh corrected-rule shard set; "
                              "exhaustively reverify every retained shard "
                              "with the current compiler before reuse"))
    parser.add_argument("--solve-existing", action="store_true",
                        help="solve an authenticated transitions-only work tree")
    parser.add_argument(
        "--existing-transition-prefix", default="",
        help=("solve-existing transition prefix relative to the work tree; "
              "defaults to work/transitions/<tablebase stem>"))
    parser.add_argument(
        "--existing-transition-certificate", default="work/transitions-ready.json",
        help=("solve-existing certificate path relative to the work tree; "
              "defaults to work/transitions-ready.json"))
    parser.add_argument("--finalize-existing", action="store_true",
                        help="authenticate existing outputs and write the manifest")
    args = parser.parse_args()

    if args.transitions_only and args.solve_existing:
        raise RuntimeError("transition and solve modes are mutually exclusive")
    if args.resume_transitions and (args.solve_existing or
                                    not args.transitions_only):
        raise RuntimeError(
            "resume-transitions requires transitions-only mode")
    if not args.transitions_only and args.lower_ghost_sidecar is None:
        raise RuntimeError("solve mode requires --lower-ghost-sidecar")
    if args.solve_max_nodes < 500_000_000:
        raise RuntimeError("solve ROBDD node budget may not shrink below baseline")
    for value, label in ((args.existing_transition_prefix,
                          "existing transition prefix"),
                         (args.existing_transition_certificate,
                          "existing transition certificate")):
        path = Path(value)
        if path.is_absolute() or ".." in path.parts:
            raise RuntimeError(f"{label} must remain inside the work tree")
    validate_prebuilt_binding(
        args.prebuilt_executable, args.prebuilt_executable_sha256,
        args.prebuilt_model_sha256, args.model_sha256)
    if args.prebuilt_executable is not None:
        require_sha(args.prebuilt_executable,
                    args.prebuilt_executable_sha256,
                    "prebuilt executable")

    if args.finalize_existing:
        work = args.work.resolve(strict=True)
        if not (work / "work/results").is_dir():
            raise RuntimeError("ordinary Ghost result directory is missing")
        write_manifest(work, args)
        return

    root = args.source_root.resolve()
    sys.path.insert(0, str(root / "tools/tablebases"))
    import ultimate_information_tablebases as information  # pylint: disable=import-outside-toplevel
    import run_ultimate_reciprocal_bishop_ghost_aws as shared  # pylint: disable=import-outside-toplevel

    expected_names = {}
    for piece in PIECES:
        for orientation in ("same", "opposing"):
            if piece in EXTRA_PRIMARY:
                name = (f"k{piece}ghostk.uftb" if orientation == "same"
                        else f"k{piece}kghost.uftb")
            else:
                name = (f"kghost{piece}k.uftb" if orientation == "same"
                        else f"kghostk{piece}.uftb")
            expected_names[(piece, orientation)] = name
    if args.filename != expected_names[(args.piece, args.orientation)]:
        raise RuntimeError("ordinary Ghost filename/material residual")
    source_model_sha256 = information.solver_model_fingerprint(
        args.filename, root=root)
    if (source_model_sha256 != args.model_sha256 and
            args.prebuilt_model_sha256 != args.model_sha256):
        raise RuntimeError("ordinary Ghost solver model mismatch")
    lower_name = f"k{args.piece}k.uftb"
    implicit_lower = args.piece in IMPLICIT_DRAWS
    theorem_model_sha = hashlib.sha256(
        f"ultimate-insufficient-lower-model-v1:{args.piece}".encode()
    ).hexdigest()
    if args.piece == "checker":
        if args.lower_model_sha256 != theorem_model_sha:
            raise RuntimeError("ordinary Ghost lower model mismatch")
    elif (len(args.lower_model_sha256) != 64 or
          any(character not in "0123456789abcdef"
              for character in args.lower_model_sha256)):
        raise RuntimeError("ordinary Ghost lower model SHA-256 is invalid")
    require_sha(args.source_table, args.source_sha256, "source table")
    theorem_sha = hashlib.sha256(
        f"ultimate-insufficient-lower-v1:{args.piece}".encode()).hexdigest()
    if implicit_lower:
        if args.lower_table is not None or args.lower_sha256 != theorem_sha:
            raise RuntimeError("implicit draw lower theorem binding mismatch")
    else:
        if args.lower_table is None:
            raise RuntimeError("decisive ordinary piece requires a lower table")
        require_sha(args.lower_table, args.lower_sha256, "lower table")
    if args.lower_ghost_sidecar is not None:
        require_sha(args.lower_ghost_sidecar, args.lower_ghost_sha256,
                    "lower Ghost sidecar")
    if args.piece == "pawn" and not args.transitions_only:
        if args.promoted_sidecar is None:
            raise RuntimeError(
                "Pawn solve requires promoted Queen/Ghost sidecar")
        require_sha(args.promoted_sidecar, args.promoted_sidecar_sha256,
                    "promoted Queen/Ghost sidecar")
    if args.piece == "pawn":
        if args.promoted_lower_table is None:
            raise RuntimeError("Pawn transitions require promoted K+Queen-v-K")
        require_sha(args.promoted_lower_table,
                    args.promoted_lower_table_sha256,
                    "promoted lower Queen table")
    if args.work.exists() and not (args.solve_existing or
                                   args.resume_transitions):
        raise RuntimeError("ordinary Ghost work directory already exists")
    if args.resume_transitions and not args.work.is_dir():
        raise RuntimeError("resume-transitions work directory is missing")
    work = args.work.resolve()
    if args.solve_existing:
        certificate_path = work / args.existing_transition_certificate
        if not certificate_path.is_file():
            raise RuntimeError("solve-existing work lacks its transition certificate")
        certificate = json.loads(
            certificate_path.read_text())
        expected = {"filename": args.filename, "piece": args.piece,
                    "orientation": args.orientation,
                    "source_sha256": args.source_sha256,
                    "model_sha256": args.model_sha256}
        if any(certificate.get(key) != value for key, value in expected.items()):
            raise RuntimeError("solve-existing transition binding mismatch")
        shutil.copyfile(args.lower_ghost_sidecar,
                        work / "tablebases/kghostk.ufgm")
        require_sha(work / "tablebases/kghostk.ufgm",
                    args.lower_ghost_sha256, "copied lower Ghost")
    else:
        (work / "tablebases").mkdir(parents=True,
                                     exist_ok=args.resume_transitions)
    copies = [] if args.solve_existing else [(args.source_table, args.filename)]
    if not args.transitions_only and not args.solve_existing:
        copies.append((args.lower_ghost_sidecar, "kghostk.ufgm"))
    if args.lower_table is not None:
        copies.append((args.lower_table, lower_name))
    if args.piece == "pawn":
        copies.append((args.promoted_lower_table, "kqueenk.uftb"))
    for source, name in copies:
        shutil.copyfile(source, work / "tablebases" / name)
    copied = [(work / "tablebases" / args.filename,
               args.source_sha256, "copied source")]
    if not args.transitions_only:
        copied.append((work / "tablebases/kghostk.ufgm",
                       args.lower_ghost_sha256, "copied lower Ghost"))
        if args.piece == "pawn":
            shutil.copyfile(args.promoted_sidecar,
                            work / "tablebases/promoted-queen-ghost.ufgd")
            copied.append((work / "tablebases/promoted-queen-ghost.ufgd",
                           args.promoted_sidecar_sha256,
                           "copied promoted Queen/Ghost sidecar"))
    if not implicit_lower:
        copied.append((work / "tablebases" / lower_name,
                       args.lower_sha256, "copied lower"))
    if args.piece == "pawn":
        copied.append((work / "tablebases/kqueenk.uftb",
                       args.promoted_lower_table_sha256,
                       "copied promoted lower Queen"))
    for path, expected, label in copied:
        require_sha(path, expected, label)
    for directory in ("work/logs", "work/transitions", "work/results",
                      "work/solve", "work/self-test"):
        (work / directory).mkdir(parents=True, exist_ok=True)

    executable = work / "ultimate_ghost_ordinary_information_tablebase"
    sources = [
        "src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp",
        "src/ultimate/tablebases/ghost_ordinary_information_solver.cpp",
        "src/ultimate/tablebases/ghost_public_extra_model.cpp",
        "src/ultimate/tablebases/external_robdd.cpp",
        "src/ultimate/tablebases/ghost_information_probe.cpp",
        "src/ultimate/tablebases/information.cpp", "src/ultimate/position.cpp",
        "src/ultimate/nnue.cpp",
    ]
    build = ["clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
             "-Wextra", "-Wpedantic", "-Werror",
             "-Wno-error=range-loop-construct", "-include", "sstream",
             "-Isrc/ultimate", "-Isrc/ultimate/tablebases",
             f"-DULTIMATE_GHOST_ORDINARY_PIECE={PIECES[args.piece]}",
             *sources, "-o", str(executable)]
    definitions = []
    if args.piece in EXTRA_PRIMARY:
        definitions.append("-DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY")
    if args.piece in EXTRA_SUBSTATES:
        definitions.append(
            f"-DULTIMATE_GHOST_EXTRA_SUBSTATES={EXTRA_SUBSTATES[args.piece]}")
    if args.piece in LOWER_SUBSTATES:
        definitions.append(
            "-DULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES="
            f"{LOWER_SUBSTATES[args.piece]}")
    if args.piece == "checker":
        definitions.append("-DULTIMATE_GHOST_EXTRA_IS_CHECKER")
    if args.piece == "copycat":
        definitions.append("-DULTIMATE_GHOST_EXTRA_IS_COPYCAT")
    if args.piece == "pawn":
        definitions.append(
            "-DULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN")
    if args.piece in HORIZONTAL_ONLY:
        definitions.append("-DULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY")
    if implicit_lower:
        definitions.append("-DULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY")
    build[-len(sources)-2:-len(sources)-2] = definitions
    if not args.solve_existing:
        if args.prebuilt_executable is None:
            run(build, root, work / "work/logs/build.log")
        else:
            shutil.copyfile(args.prebuilt_executable, executable)
            executable.chmod(0o755)
            require_sha(executable, args.prebuilt_executable_sha256,
                        "copied prebuilt executable")
    binding = [
        "--orientation", args.orientation,
        "--lower-dragon-table", ("implicit-draw" if implicit_lower
                                  else f"tablebases/{lower_name}"),
        "--lower-dragon-sha256", args.lower_sha256,
        "--lower-dragon-source-sha256", args.lower_sha256,
        "--lower-dragon-model-sha256", args.lower_model_sha256,
        "--source-sha256", args.source_sha256,
        "--model-sha256", args.model_sha256,
        "--observation-sha256", args.observation_sha256,
    ]
    if args.piece == "pawn":
        binding += [
            "--promoted-lower-dragon-table", "tablebases/kqueenk.uftb",
            "--promoted-lower-dragon-sha256",
            args.promoted_lower_table_sha256,
            "--promoted-lower-dragon-source-sha256",
            args.promoted_lower_table_sha256,
            "--promoted-lower-dragon-model-sha256",
            args.promoted_lower_model_sha256,
        ]
    geometries = geometry_count(args.piece)
    if not args.solve_existing:
        run([str(executable), "--self-test", "--orientation", args.orientation,
             "--scratch", f"work/self-test/{Path(args.filename).stem}",
             "--input", f"tablebases/{args.filename}",
             "--source-sha256", args.source_sha256], work,
            work / "work/logs/self-test.log")
        commands = []
        for index, (begin, count) in enumerate(ranges(geometries)):
            commands.append([str(executable), "--compile-transitions",
              "--transition-prefix", f"work/transitions/shard-{index:02d}",
              "--geometry-begin", str(begin), "--geometry-count", str(count),
              *binding])
        if args.resume_transitions:
            retained = []
            for command in commands:
                if shared.transition_is_complete(work, command):
                    verify = list(command)
                    verify[verify.index("--compile-transitions")] = \
                        "--verify-transitions"
                    name = Path(shared.transition_prefix(command)).name
                    retained.append((verify,
                        work / "work/logs" / f"{name}.reverify.log"))
            with ThreadPoolExecutor(max_workers=args.parallelism) as executor:
                futures = [executor.submit(run, command, work, log)
                           for command, log in retained]
                for future in futures:
                    future.result()
        shared.run_ranges(commands, work, args.parallelism)
    stem = Path(args.filename).stem
    merged = (args.existing_transition_prefix
              if args.solve_existing and args.existing_transition_prefix
              else f"work/transitions/{stem}")
    merge = [str(executable), "--merge-transitions", "--orientation",
             args.orientation, "--transition-prefix", merged]
    for index in range(SHARDS):
        merge += ["--shard", f"work/transitions/shard-{index:02d}"]
    merge += ["--expected-geometries", str(geometries), *binding[2:]]
    if not args.solve_existing:
        run(merge, work, work / "work/logs/merge.log")
        (work / "work/transitions-ready.json").write_text(json.dumps({
            "schema": "ultimate-ordinary-ghost-transitions-v1",
            "filename": args.filename, "piece": args.piece,
            "orientation": args.orientation,
            "source_sha256": args.source_sha256,
            "model_sha256": args.model_sha256,
            "geometries": geometries,
        }, sort_keys=True, indent=2) + "\n")
    if args.transitions_only:
        return
    solve = [str(executable), "--orientation", args.orientation,
      "--transition-prefix", merged, "--input", f"tablebases/{args.filename}",
      "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
      "--scratch", f"work/solve/{stem}",
      "--output", f"work/results/{stem}.ufiw",
      "--output-arbitrary", f"work/results/{stem}.ufgd", *binding[2:],
      "--lower-sidecar-sha256", args.lower_ghost_sha256,
      "--lower-source-sha256", args.lower_ghost_source_sha256,
      "--lower-model-sha256", args.lower_ghost_model_sha256,
      "--lower-observation-sha256", args.lower_ghost_observation_sha256,
      "--max-nodes", str(args.solve_max_nodes),
      "--unique-slots", str(1 << 30),
      "--compact-every", "1"]
    if args.piece == "pawn":
        solve += [
          "--promoted-sidecar", "tablebases/promoted-queen-ghost.ufgd",
          "--promoted-sidecar-sha256", args.promoted_sidecar_sha256,
          "--promoted-source-sha256", args.promoted_source_sha256,
          "--promoted-model-sha256", args.promoted_model_sha256,
          "--promoted-observation-sha256",
          args.promoted_observation_sha256,
          "--promoted-lower-ghost-sidecar-sha256",
          args.promoted_lower_ghost_sidecar_sha256,
          "--promoted-lower-dragon-sha256",
          args.promoted_lower_table_sha256,
          "--promoted-lower-dragon-source-sha256",
          args.promoted_lower_table_sha256,
          "--promoted-lower-dragon-model-sha256",
          args.promoted_lower_model_sha256,
        ]
    run([str(executable), "--solve", *solve[1:]], work,
        work / "work/logs/solve.log")
    write_manifest(work, args)


if __name__ == "__main__":
    main()
