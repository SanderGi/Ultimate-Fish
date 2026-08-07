#!/usr/bin/env python3
"""Deterministic Ultimate Fish candidate-vs-baseline match runner."""

from __future__ import annotations

import argparse
import os
import subprocess
from dataclasses import dataclass


STARTS = (
    (
        "mixed",
        "w;hm=0;fm=1;ep=-;cont=0;forced=-1;"
        "king,w,e1;jester,w,d1;ninja,w,b2;penguin,w,c2;devil,w,f2;"
        "sniper,w,g2;checker,w,h2;sludge,w,a2;"
        "king,b,e10;jester,b,d10;ninja,b,b9;penguin,b,c9;devil,b,f9;"
        "sniper,b,g9;checker,b,h9;sludge,b,a9"
    ),
    (
        "linked",
        "w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,e1;angel,w,b2;"
        "rook,w,c3;fisherman,w,a2;giant,w,f2;king,b,e10;angel,b,b9;"
        "rook,b,c8;fisherman,b,a9;giant,b,f8"
    ),
    (
        "volatile",
        "w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,e1;parasite,w,d3;"
        "bomb,w,b2;berserker,w,f2;ghost,w,c2;pawn,w,g3;king,b,e10;"
        "parasite,b,d8;bomb,b,b9;berserker,b,f9;ghost,b,c9;pawn,b,g8"
    ),
    (
        "classic",
        "w;hm=0;fm=1;ep=-;cont=0;forced=-1;rook,w,a1;knight,w,b1;"
        "bishop,w,c1;queen,w,d1;king,w,e1;bishop,w,f1;knight,w,g1;rook,w,h1;"
        "pawn,w,a2;pawn,w,b2;pawn,w,c2;pawn,w,d2;pawn,w,e2;pawn,w,f2;"
        "pawn,w,g2;pawn,w,h2;rook,b,a10;knight,b,b10;bishop,b,c10;queen,b,d10;"
        "king,b,e10;bishop,b,f10;knight,b,g10;rook,b,h10;pawn,b,a9;pawn,b,b9;"
        "pawn,b,c9;pawn,b,d9;pawn,b,e9;pawn,b,f9;pawn,b,g9;pawn,b,h9"
    ),
    (
        "relocation",
        "w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,e1;mage,w,c2;giant,w,f2;"
        "fisherman,w,a2;rook,w,d3;king,b,e10;mage,b,c9;giant,b,f8;"
        "fisherman,b,a9;rook,b,d8"
    ),
    (
        "evolved_queens",
        "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;queen,w,h3;"
        "queen,w,b3;queen,w,c3;queen,w,b2;pawn,w,a2;pawn,w,h2;giant,w,e1;"
        "giant,w,c1;pawn,w,g2;pawn,w,e3;pawn,w,a3;dragon,w,g1;king,b,b10;"
        "queen,b,h8;queen,b,b8;queen,b,c8;queen,b,b9;pawn,b,a9;pawn,b,h9;"
        "giant,b,e9;giant,b,c9;pawn,b,g9;pawn,b,f8;pawn,b,a8;dragon,b,g10"
    ),
    (
        "evolved_mixed",
        "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;bishop,w,h3;"
        "queen,w,f2;bishop,w,b2;queen,w,c1;queen,w,g3;queen,w,c2;checker,w,b1;"
        "giant,w,d1;pawn,w,d3;checker,w,a3;pawn,w,a2;giant,w,g1;checker,w,f1;"
        "king,b,a10;queen,b,f8;queen,b,b8;queen,b,h10;queen,b,c8;queen,b,b9;"
        "pawn,b,a9;checker,b,g9;checker,b,b10;pawn,b,g10;pawn,b,h9;giant,b,e9;"
        "giant,b,c9"
    ),
)


@dataclass
class Engine:
    path: str
    nnue: str | None = None

    def __post_init__(self) -> None:
        environment = dict(os.environ)
        if self.nnue:
            environment["ULTIMATE_NNUE_FILE"] = self.nnue
        self.process = subprocess.Popen(
            [self.path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1, env=environment,
        )

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def until_any(self, prefixes: tuple[str, ...]) -> str:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            line = line.rstrip("\n")
            if line.startswith(prefixes):
                return line
        raise RuntimeError(f"{self.path} stopped before {prefixes!r}")

    def set_position(self, upn: str) -> None:
        self.send("position upn " + upn)
        line = self.until_any(("positionok", "info string invalid upn"))
        if line != "positionok":
            raise RuntimeError(f"{self.path} rejected position: {line}\n{upn}")

    def new_game(self) -> None:
        self.send("ucinewgame")
        self.send("isready")
        self.until_any(("readyok",))

    def bestmove(self, upn: str, depth: int, nodes: int, movetime: int = 0) -> str | None:
        self.set_position(upn)
        self.send(f"go depth {depth} " +
                  (f"movetime {movetime}" if movetime else f"nodes {nodes}"))
        try:
            line = self.until_any(("bestmove ",))
        except RuntimeError as error:
            assert self.process.stderr is not None
            diagnostics = self.process.stderr.read().strip()
            raise RuntimeError(
                f"{error}\nposition: {upn}\nstderr: {diagnostics or '(empty)'}"
            ) from error
        move = line.split(" ", 1)[1]
        return None if move == "(none)" else move

    def apply(self, upn: str, move: str) -> str:
        self.set_position(upn)
        self.send("move " + move)
        line = self.until_any(("position ", "illegalmove"))
        if line == "illegalmove":
            raise RuntimeError(f"{self.path} rejected generated move {move}\n{upn}")
        return line.split(" ", 1)[1]

    def close(self) -> None:
        if self.process.poll() is None:
            self.send("quit")
            self.process.wait(timeout=5)


def side(upn: str) -> str:
    return upn[0]


def winner(upn: str) -> str | None:
    white = ";king,w," in ";" + upn
    black = ";king,b," in ";" + upn
    if white == black:
        return None
    return "w" if white else "b"


def repetition_key(upn: str) -> str:
    return ";".join(
        field for field in upn.split(";")
        if not field.startswith(("hm=", "fm="))
    )


def play(candidate: Engine, baseline: Engine, candidate_color: str, depth: int,
         nodes: int, movetime: int, plies: int, start: str, verbose: bool) -> float:
    upn = start
    seen = {repetition_key(upn): 1}
    for ply in range(plies):
        current = candidate if side(upn) == candidate_color else baseline
        move = current.bestmove(upn, depth, nodes, movetime)
        if move is None:
            break
        if verbose:
            print(f"  ply {ply + 1}: {side(upn)} {move}")
        upn = candidate.apply(upn, move)
        key = repetition_key(upn)
        seen[key] = seen.get(key, 0) + 1
        if seen[key] >= 3:
            return 0.5
        victor = winner(upn)
        if victor:
            return 1.0 if victor == candidate_color else 0.0
    return 0.5


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("candidate")
    parser.add_argument("baseline")
    parser.add_argument("--games", type=int, default=2 * len(STARTS))
    parser.add_argument("--depth", type=int, default=20,
                        help="maximum iterative-deepening depth")
    parser.add_argument("--nodes", type=int, default=20_000,
                        help="deterministic node budget per move")
    parser.add_argument("--movetime", type=int, default=0,
                        help="wall-clock milliseconds per move; overrides --nodes")
    parser.add_argument("--plies", type=int, default=160)
    parser.add_argument("--fixtures", default=",".join(name for name, _ in STARTS),
                        help="comma-separated fixture names (each is played as a color pair)")
    parser.add_argument("--verbose", action="store_true", help="print every played move")
    parser.add_argument("--candidate-nnue", help="opt-in .ufnn network for candidate only")
    args = parser.parse_args()

    requested = [name.strip() for name in args.fixtures.split(",") if name.strip()]
    starts_by_name = dict(STARTS)
    unknown = [name for name in requested if name not in starts_by_name]
    if unknown or not requested:
        parser.error("unknown or empty fixture selection: " + ", ".join(unknown))
    selected_starts = tuple((name, starts_by_name[name]) for name in requested)

    score = 0.0
    for game in range(args.games):
        color = "w" if game % 2 == 0 else "b"
        fixture_name, start = selected_starts[(game // 2) % len(selected_starts)]
        # Fresh processes make color pairs independent of TT/history state and
        # also allow old binaries whose ucinewgame reset was incomplete to be
        # benchmarked fairly.
        candidate = Engine(args.candidate, args.candidate_nnue)
        baseline = Engine(args.baseline)
        try:
            result = play(candidate, baseline, color, args.depth, args.nodes, args.movetime,
                          args.plies, start, args.verbose)
            score += result
            print(f"game {game + 1}: {fixture_name}, candidate {color}, score {result:.1f}")
        finally:
            candidate.close()
            baseline.close()
    print(f"candidate total: {score:.1f}/{args.games}")


if __name__ == "__main__":
    main()
