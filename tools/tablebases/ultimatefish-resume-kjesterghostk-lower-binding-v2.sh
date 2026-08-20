#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/jester-ghost-current-same-work-v1
binary=${root}/ultimate_jester_ghost_information_tablebase
prefix=work/transitions/kjesterghostk
prior_log=${root}/work/logs/solve.log
log=${root}/work/logs/solve-lower-binding-v2.log
scratch=work/solve-lower-binding-v2/kjesterghostk
output=work/results/kjesterghostk-lower-binding-v2.ufiw
arbitrary=work/results/kjesterghostk-lower-binding-v2.ufjg

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  70b406b369ab54f37c892b9caec32dfcb815a842bada89277bfee145cbb7d68e
test "$(sha256sum "${root}/tablebases/kjesterghostk.uftb" | cut -d ' ' -f 1)" = \
  010c3c46676d9df861367a99433cf293f94e7a199dcf400b367c65d916533a13
test "$(sha256sum "${root}/tablebases/kjesterk.uftb" | cut -d ' ' -f 1)" = \
  3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa
test "$(sha256sum "${root}/tablebases/kjesterk.ufiw" | cut -d ' ' -f 1)" = \
  f1383fc68a7f774e8bf9dd65d907c84bdcdfa62e61c56f08d28fd46e6978b5fc
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f 1)" = \
  18a283f663cae8360a34f9935294e0c80d5643ab5e6298dca0244ee9bd6d0bed
grep -Fq \
  'jester/ghost tablebase error: lower Jester UFIW2 binding mismatch' \
  "${prior_log}"
test ! -e "${log}"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 241591910400

mkdir -p "${root}/work/solve-lower-binding-v2" \
  "${root}/work/results"

exec >>"${log}" 2>&1
echo 'jester_ghost_same_lower_binding_v2 binary_sha256 70b406b369ab54f37c892b9caec32dfcb815a842bada89277bfee145cbb7d68e graph_marker_sha256 18a283f663cae8360a34f9935294e0c80d5643ab5e6298dca0244ee9bd6d0bed lower_jester_model_sha256 ed1b9722003903967eef917251d5439aba9635e31f9be2cc96366044fc7b4c05 upper_nodes 1500000000 lower_nodes 500000000 prior_binding_failure_preserved work/logs/solve.log'

cd "${root}"
common=(
  --transition-prefix "${prefix}"
  --input tablebases/kjesterghostk.uftb
  --lower-jester-table tablebases/kjesterk.uftb
  --lower-jester-overlay tablebases/kjesterk.ufiw
  --lower-jester-model-sha256 ed1b9722003903967eef917251d5439aba9635e31f9be2cc96366044fc7b4c05
  --lower-jester-overlay-sha256 f1383fc68a7f774e8bf9dd65d907c84bdcdfa62e61c56f08d28fd46e6978b5fc
  --lower-ghost-sidecar tablebases/kghostk.ufgm
  --lower-ghost-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
  --source-sha256 010c3c46676d9df861367a99433cf293f94e7a199dcf400b367c65d916533a13
  --model-sha256 93c6fe730d863f802daaadc0752d80f3be2f76b7ee17fa3bcabe011e99dc9ebd
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
  --scratch "${scratch}"
  --bdd-max-upper-nodes 1500000000
  --bdd-upper-unique-slots 2147483648
  --bdd-lower-max-nodes 500000000
  --bdd-lower-unique-slots 1073741824
  --bdd-budget-bytes 236223201280
  --max-disk-bytes 1825361100800
  --max-resident-bytes 182536110080
  --min-free-disk-bytes 53687091200
)

"${binary}" --verify-solve-inputs "${common[@]}"

"${binary}" \
  --solve \
  "${common[@]}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --compact-every 4

sha256sum "${output}" "${arbitrary}"
echo 'jester_ghost_same_lower_binding_v2 complete 1'
