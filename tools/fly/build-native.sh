#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
target="${1:-all}"
case "$target" in all|brain|rules) ;; *) echo 'Usage: build-native.sh [all|brain|rules]' >&2; exit 2 ;; esac
mkdir -p src
if [[ "$target" == all || "$target" == brain ]]; then
  "${CXX:-c++}" -std=c++17 -O3 -DFLY_BRAIN_CLI tools/fly/brain.cpp -o src/ultimate_fly_brain
fi
if [[ "$target" == all || "$target" == rules ]]; then
  "${CXX:-c++}" -std=c++17 -O3 -DNDEBUG tools/fly/rules-server.cpp src/ultimate/position.cpp -o src/ultimate_fly_rules
fi
