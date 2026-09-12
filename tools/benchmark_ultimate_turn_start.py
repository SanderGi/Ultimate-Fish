#!/usr/bin/env python3
"""Paired, tablebase-free baseline/candidate search timing (no engine builds)."""

import argparse
import json
import re
import statistics
import subprocess
import time


FIXTURES = {
    "classic": "w;rook,w,a1;knight,w,b1;bishop,w,c1;queen,w,d1;king,w,e1;bishop,w,f1;knight,w,g1;rook,w,h1;pawn,w,a2;pawn,w,b2;pawn,w,c2;pawn,w,d2;pawn,w,e2;pawn,w,f2;pawn,w,g2;pawn,w,h2;rook,b,a10;knight,b,b10;bishop,b,c10;queen,b,d10;king,b,e10;bishop,b,f10;knight,b,g10;rook,b,h10;pawn,b,a9;pawn,b,b9;pawn,b,c9;pawn,b,d9;pawn,b,e9;pawn,b,f9;pawn,b,g9;pawn,b,h9",
    "ultimate": "w;king,w,e1;jester,w,d1;ninja,w,b2;penguin,w,c2;devil,w,f2;sniper,w,g2;checker,w,h2;sludge,w,a2;king,b,e10;jester,b,d10;ninja,b,b9;penguin,b,c9;devil,b,f9;sniper,b,g9;checker,b,h9;sludge,b,a9",
    "minions": "w;king,w,d1;king,b,f10;devil,w,b1;minion,w,a4;minion,w,b4;devil,b,g10;minion,b,f7;minion,b,g7",
    "cooldowns": "w;king,w,a1;king,b,h10;sniper,w,b2,0,1;sniper,b,g9,0,1;rook,w,c3;rook,b,f8",
    "native_interactions": "w;king,w,a1;giant,w,c1;ghost,w,e1;ninja,w,f1;bomb,w,g1;penguin,w,h1;parasite,w,e2;fisherman,w,f2;turtle,w,a2;pawn,w,b2;checker,b,a8;turtle,b,b8;turtle,b,c8;turtle,b,d8;devil,b,e8;devil,b,f8;turtle,b,g8;turtle,b,h8;giant,b,c9;giant,b,e9;ninja,b,g9;turtle,b,h9;sniper,b,a10;king,b,g10;turtle,b,h10",
}


def search(binary, upn, mode, nodes, depth):
    command = f"go depth {depth}" if mode == "depth" else f"go depth 40 nodes {nodes}"
    started = time.perf_counter()
    result = subprocess.run(
        [binary], input=f"position upn {upn}\n{command} tablebases 0\nquit\n",
        text=True, capture_output=True, check=True, timeout=180,
    )
    wall = time.perf_counter() - started
    matches = re.findall(
        r"info depth (\d+) score (cp|mate) (-?\d+) nodes (\d+) time (\d+) pv([^\n]*)",
        result.stdout,
    )
    if not matches:
        raise RuntimeError(result.stdout + result.stderr)
    completed, score_kind, score, visited, milliseconds, pv = matches[-1]
    milliseconds, visited = int(milliseconds), int(visited)
    return dict(depth=int(completed), score_kind=score_kind, score=int(score), nodes=visited,
                milliseconds=milliseconds, nps=visited * 1000 / max(1, milliseconds),
                wall_seconds=wall, pv=pv.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline")
    parser.add_argument("candidate")
    parser.add_argument("--repeats", type=int, default=9)
    parser.add_argument("--nodes", type=int, default=1000000)
    parser.add_argument("--depth", type=int, default=8)
    args = parser.parse_args()
    records = []
    # Alternate AB/BA, one process at a time, with fresh search/TT state.
    for mode in ("nodes", "depth"):
        for name, upn in FIXTURES.items():
            for run in range(args.repeats + 1):
                pair = {}
                order = ("baseline", "candidate") if run % 2 else ("candidate", "baseline")
                for variant in order:
                    pair[variant] = search(getattr(args, variant), upn, mode,
                                           args.nodes, args.depth)
                record = dict(fixture=name, mode=mode, run=run,
                              warmup=run == 0, **pair)
                print(json.dumps(record), flush=True)
                if run:
                    records.append(record)
            selected = [r for r in records if r["fixture"] == name and r["mode"] == mode]
            print(json.dumps(dict(summary=True, fixture=name, mode=mode,
                baseline_ms=statistics.median(r["baseline"]["milliseconds"] for r in selected),
                candidate_ms=statistics.median(r["candidate"]["milliseconds"] for r in selected),
                median_paired_nps_ratio=statistics.median(
                    r["candidate"]["nps"] / r["baseline"]["nps"] for r in selected),
                same_nodes_score_pv=all(all(r["baseline"][k] == r["candidate"][k]
                    for k in ("nodes", "score_kind", "score", "pv")) for r in selected))), flush=True)


if __name__ == "__main__":
    main()
