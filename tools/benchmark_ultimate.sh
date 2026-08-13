#!/bin/sh
set -eu

workspace=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
engine=${ULTIMATE_FISH_BINARY:-"$workspace/src/ultimatefish"}
belief_benchmark=${ULTIMATE_BELIEF_BENCHMARK_BINARY:-"$workspace/src/ultimate_belief_benchmark"}
hidden_search_benchmark=${ULTIMATE_HIDDEN_SEARCH_BENCHMARK_BINARY:-"$workspace/src/ultimate_hidden_search_benchmark"}

make -C "$workspace/src" ultimatefish >/dev/null

classic='w;hm=0;fm=1;ep=-;cont=0;forced=-1;rook,w,a1;knight,w,b1;bishop,w,c1;queen,w,d1;king,w,e1;bishop,w,f1;knight,w,g1;rook,w,h1;pawn,w,a2;pawn,w,b2;pawn,w,c2;pawn,w,d2;pawn,w,e2;pawn,w,f2;pawn,w,g2;pawn,w,h2;rook,b,a10;knight,b,b10;bishop,b,c10;queen,b,d10;king,b,e10;bishop,b,f10;knight,b,g10;rook,b,h10;pawn,b,a9;pawn,b,b9;pawn,b,c9;pawn,b,d9;pawn,b,e9;pawn,b,f9;pawn,b,g9;pawn,b,h9'
ultimate='w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,e1;jester,w,d1;ninja,w,b2;penguin,w,c2;devil,w,f2;sniper,w,g2;checker,w,h2;sludge,w,a2;king,b,e10;jester,b,d10;ninja,b,b9;penguin,b,c9;devil,b,f9;sniper,b,g9;checker,b,h9;sludge,b,a9'
unranked_loss='w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;giant,w,c1;ghost,w,e1;ninja,w,f1;bomb,w,g1;penguin,w,h1;parasite,w,e2;fisherman,w,f2;turtle,w,a2;pawn,w,b2;checker,b,a8;turtle,b,b8;turtle,b,c8;turtle,b,d8;devil,b,e8;devil,b,f8;turtle,b,g8;turtle,b,h8;giant,b,c9;giant,b,e9;ninja,b,g9;turtle,b,h9;sniper,b,a10;king,b,g10;turtle,b,h10'

printf 'Classic control\n'
printf '%s\n' "position upn $classic" 'perft 3' 'go depth 6' quit | "$engine"
printf '\nUltimate interactions\n'
printf '%s\n' "position upn $ultimate" 'perft 1' 'perft 2' 'perft 3' 'go depth 5' quit | "$engine"

printf '\nRecovered Unranked loss opening\n'
printf '%s\n' "position upn $unranked_loss" 'go depth 4' 'go depth 7' quit | "$engine"

printf '\nExact public-belief match\n'
if [ -z "${ULTIMATE_BELIEF_BENCHMARK_BINARY:-}" ]; then
    make -C "$workspace/src" ultimate-belief-benchmark >/dev/null
fi
"$belief_benchmark"

printf '\nHidden-information search scaling\n'
if [ -z "${ULTIMATE_HIDDEN_SEARCH_BENCHMARK_BINARY:-}" ]; then
    make -C "$workspace/src" ultimate-hidden-search-benchmark >/dev/null
fi
"$hidden_search_benchmark"

if [ "${ULTIMATE_LONG_BENCHMARK:-0}" = 1 ]; then
    printf '\nRecovered Unranked loss opening (10-second horizon)\n'
    printf '%s\n' "position upn $unranked_loss" \
        'go movetime 10000 depth 20' quit | "$engine"
fi
