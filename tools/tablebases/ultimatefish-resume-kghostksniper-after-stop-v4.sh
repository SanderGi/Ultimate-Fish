#!/usr/bin/env bash
# Resume only the authenticated opposing Ghost/Sniper fixed point.  The full
# transition graph and current/next ROBDD roots are retained from v3; no graph
# shard is regenerated or merged by this continuation.
set -euo pipefail

echo "rejected: legacy solve-existing cannot resume retained fixed-point roots safely" >&2
exit 1

root=/mnt/ultimatefish/opposed-ghost-sniper-rebuild-after-stop-v1
source_root=/mnt/ultimatefish/knight-resume-build-v1
runner=$source_root/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
binary=$root/dependencies/ultimate_ghost_sniper_compositional_v5_linux
source_table=$root/dependencies/kghostksniper.uftb
lower_table=$root/dependencies/ksniperk.uftb
lower_ghost=$root/dependencies/kghostk.ufgm
work=$root/job

test "$(sha256sum "$source_root/source.tar.gz" | cut -d' ' -f1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "$runner" | cut -d' ' -f1)" = \
  bb756bf90ed54db11438b1f4186fca90967d5501ae6aafb2ad5402427c67bd1e
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775
test "$(sha256sum "$work/ultimate_ghost_ordinary_information_tablebase" | cut -d' ' -f1)" = \
  42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775
test "$(sha256sum "$source_table" | cut -d' ' -f1)" = \
  c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78
test "$(sha256sum "$lower_table" | cut -d' ' -f1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "$lower_ghost" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "$work/work/transitions-ready.json" | cut -d' ' -f1)" = \
  181d1fee675c3f93231961f9ac31c36531d8b9329111e0a76020968c21df4600
test "$(df --output=avail -B1 "$root" | tail -1)" -ge 107374182400

exec /usr/bin/python3 "$runner" \
  --source-root "$source_root" \
  --work "$work" \
  --filename kghostksniper.uftb \
  --piece sniper \
  --orientation opposing \
  --source-table "$source_table" \
  --source-sha256 c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78 \
  --lower-table "$lower_table" \
  --lower-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5 \
  --lower-model-sha256 7eaa2c6b443d15a98f935cf7b9fec3e6991a841246a2af8290afadaf1b3feab1 \
  --lower-ghost-sidecar "$lower_ghost" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --model-sha256 69cd2e01bf3286e4de59f8fe2f3b3c8b02aa48268d09e034ee02d9a4b09e0878 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 1 \
  --prebuilt-executable "$binary" \
  --prebuilt-executable-sha256 42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775 \
  --prebuilt-model-sha256 69cd2e01bf3286e4de59f8fe2f3b3c8b02aa48268d09e034ee02d9a4b09e0878 \
  --solve-existing \
  --solve-max-nodes 1000000000
