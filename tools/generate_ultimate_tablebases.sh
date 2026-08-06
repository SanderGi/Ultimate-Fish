#!/bin/sh
set -eu

workspace=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${ULTIMATE_TABLEBASE_DIR:-"$workspace/tablebases"}

mkdir -p "$output"
make -C "$workspace/src" ultimate-tablebase

generate() {
    piece=$1
    name=$2
    "$workspace/src/ultimate_tablebase" \
        --piece "$piece" \
        --output "$output/$name.uftb" \
        --checkpoint "$output/$name.checkpoint" \
        --checkpoint-every 50000
}

generate rook krk
generate queen kqk
generate ninja kninjak
generate dragon kdragonk
