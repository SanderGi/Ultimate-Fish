#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly archive=${restore}/berserker-ghost-opposed-migration-v12.tar.zst
readonly archive_sha=10709917e47875e13c3e8f873df0f9fff9c507a1546f692ff4d552fc193930ea
readonly archive_key="results/checkpoints/berserker-ghost/opposed/migration-v12/sha256/${archive_sha}/berserker-ghost-opposed-migration-v12.tar.zst"
readonly archive_version=B2A.X4sRblLBMfwZl7nrFiHO4QNE19Fg
readonly binary_sha=da683c8395d05f4d9df7d06a291a5be819cb559c76e3cb9e9c2188e871a2479a
readonly binary_key="sources/binaries/berserker-resume-v12/sha256/${binary_sha}/ultimate_ghost_berserker_information_tablebase"
readonly binary_version=cnEL6n8KZvxrE4muzQ.oHCErmKFyWDpF
readonly wrapper_sha=4e049ea3673646fc481b2a5d672e8c118caeb4c73e7b20fb5a6535a9d264b879
readonly wrapper_key="sources/tools/opposed-berserker-fresh-v12/sha256/${wrapper_sha}/solve_ultimate_opposed_berserker_v12.sh"
readonly wrapper_version=QG98zeQMT9WJm6DwYpkq56oiO7B28DOn
readonly service_sha=37e939f015888bf5ed917eab50e3cd104c3e67a470886726000df12eda3204f3
readonly service_key="sources/systemd/opposed-berserker-fresh-v12/sha256/${service_sha}/ultimatefish-info-kberserkerkghost-resume-v12.service"
readonly service_version=4sp1zVCVPPwAuljc6_siSSwlx_c1QXLx
readonly root=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5

test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 751619276800
install -d -m 0755 "${restore}" "${restore}/bin"
if [[ ! -f "${archive}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "${archive_key}" --version-id "${archive_version}" \
    "${archive}" >/dev/null
fi
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = "${archive_sha}"

if [[ ! -f "${restore}/.checkpoint-extracted" ]]; then
  tar --zstd --sparse -xf "${archive}" -C "${restore}"
  touch "${restore}/.checkpoint-extracted"
fi

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${restore}/bin/ultimate_ghost_berserker_information_tablebase" >/dev/null
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${wrapper_key}" --version-id "${wrapper_version}" \
  "${restore}/solve_ultimate_opposed_berserker_v12.sh" >/dev/null
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${service_key}" --version-id "${service_version}" \
  /etc/systemd/system/ultimatefish-info-kberserkerkghost-resume-v12.service \
  >/dev/null

test "$(sha256sum "${restore}/bin/ultimate_ghost_berserker_information_tablebase" | cut -d ' ' -f 1)" = "${binary_sha}"
test "$(sha256sum "${restore}/solve_ultimate_opposed_berserker_v12.sh" | cut -d ' ' -f 1)" = "${wrapper_sha}"
test "$(sha256sum /etc/systemd/system/ultimatefish-info-kberserkerkghost-resume-v12.service | cut -d ' ' -f 1)" = "${service_sha}"
test "$(sha256sum "${root}/tablebases/kberserkerkghost.uftb" | cut -d ' ' -f 1)" = d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55
test "$(sha256sum "${root}/tablebases/kberserkerk.uftb" | cut -d ' ' -f 1)" = f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test "$(sha256sum "${restore}/info-wave-aa82210e-stage/kghostk.ufgm" | cut -d ' ' -f 1)" = 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/transitions/kberserkerkghost.verified" | cut -d ' ' -f 1)" = 823ad1a14af45def9a1e41a3eec0040ec5d8797b51ba219c9505783c35ee90e0
chmod 0755 "${restore}/bin/ultimate_ghost_berserker_information_tablebase" \
  "${restore}/solve_ultimate_opposed_berserker_v12.sh"
systemctl daemon-reload
printf '%s\n' "archive_sha256=${archive_sha}" \
  "archive_version=${archive_version}" "binary_sha256=${binary_sha}" \
  'transition_residual=0' >"${restore}/restore-certified-v12"
echo 'berserker_ghost_opposed_restore_v12 complete 1'
