#!/usr/bin/env bash
# Switch only after the converged iteration-23 receipt exists.  The current
# proof phase does not mutate the retained current roots or their BDD IDs.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly old_unit=ultimatefish-info-kghostkchecker-parallel-v22.service
readonly new_unit=ultimatefish-info-kghostkchecker-parallel-v23.service
readonly wrapper=/usr/local/bin/ultimatefish-resume-kghostkchecker-parallel-v23.sh
readonly manifest=/opt/ultimatefish-certification-sprint/manifest
readonly log=/mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker/work/logs/compositional-merge-solve-v4.log

test "$(grep -Fc 'reciprocal_ghost_extra_iteration 23 bdd_nodes 164178043 changed_owner 0 changed_observer 0 changed_visible 0 peak_rss_bytes 17896607744' "${log}")" = 1
test "$(systemctl is-active "${old_unit}")" = active
test ! -e /mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker/work/results/kghostkchecker-compositional-v4.ufiw
test ! -e /mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker/work/results/kghostkchecker-compositional-v4.ufgd

readonly staged=$(mktemp /run/ultimatefish-checker-v23.XXXXXX)
trap 'rm -f "${staged}"' EXIT
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/tools/checker-parallel-v23/sha256/a3c2afac7947ef773cfa957e6e059959b5722d71d88ba7346fa318ea25d74aad/ultimatefish-resume-kghostkchecker-parallel-v23.sh \
  --version-id t2RkdAThB5qqYWMrDe4ee18u2XJlI.Qg "${staged}" >/dev/null
test "$(sha256sum "${staged}" | cut -d ' ' -f1)" = \
  a3c2afac7947ef773cfa957e6e059959b5722d71d88ba7346fa318ea25d74aad

systemctl stop "${old_unit}"
test "$(systemctl is-active "${old_unit}" || true)" = inactive
install -m 0755 "${staged}" "${wrapper}"
sed -i 's/ultimatefish-info-kghostkchecker-parallel-v22\.service/ultimatefish-info-kghostkchecker-parallel-v23.service/' "${manifest}"
grep -Fq "${new_unit}" "${manifest}"
systemd-run --unit="${new_unit%.service}" --collect \
  --property=AllowedCPUs=0-31 --property=MemoryMax=96G \
  --property=MemorySwapMax=0 "${wrapper}"
test "$(systemctl is-active "${new_unit}")" = active
echo 'checker_opposed_parallel_v23_handoff converged_iteration=23 parallel_singleton=1 residual=0'
