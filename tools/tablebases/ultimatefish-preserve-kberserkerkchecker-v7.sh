#!/bin/bash
set -euo pipefail

readonly work=/mnt/ultimatefish-penguin/resume-checker-opposed-local-v5
readonly source=/mnt/ultimatefish/current-wave0-009-checker-opposed-48g-v4
readonly preservation_source=/mnt/ultimatefish/checker-preservation-source-c87858d2-v1/repo
readonly runner="${preservation_source}/tools/tablebases/resume_ultimate_concrete_frontier_aws.py"
readonly manifest="${source}/resume-local-manifest.json"
readonly output="${work}/outputs/kberserkerkchecker.uftb"
readonly result="${work}/results/kberserkerkchecker.json"
readonly plan="${work}/resume-plan.json"
readonly binary="${work}/binary/ultimate_tablebase"
readonly log="${work}/logs/generate/kberserkerkchecker.log"
readonly dependency_manifest="${source}/dependencies/manifest.json"
readonly preservation_result="${work}/preserve-completed-v7.json"
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly prefix="s3://${bucket}/results/current-concrete-84592a0b"

test "$(sha256sum "${manifest}" | cut -d ' ' -f 1)" = 1d3850efa3d267c459cb93bc36a2bc9bdcc89e406d80e02cdf5d9d95d4e7c45b
test "$(sha256sum "${runner}" | cut -d ' ' -f 1)" = 973c7759314ab95fcee9acc320b449617db1b16a36584a2c6ce8cb434e0b0205
test "$(sha256sum "${preservation_source}/tools/tablebases/run_double_jester_information_capture.py" | cut -d ' ' -f 1)" = 544a1bac6f2cfb490297481e45c68dfc67b2fde3e451d3c85c484ad45e3d9253
test "$(sha256sum "${preservation_source}/tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py" | cut -d ' ' -f 1)" = f48afa7357d9f13fe49bfcd7e17db189bd938446b09ee66b81cfaa8687b3e3a6
test "$(sha256sum "${preservation_source}/tools/tablebases/plan_ultimate_tablebases.py" | cut -d ' ' -f 1)" = e390d4e9dfa3da6a299275391907527684c9392f289890a8a01218fde3d838ba
test "$(sha256sum "${preservation_source}/tools/tablebases/ultimate_tablebase_shards.py" | cut -d ' ' -f 1)" = 9977c579ca91c09de7b0614032bce88c72337056da85780747c3982ed2d393b0
test "$(sha256sum "${preservation_source}/tools/tablebases/ultimate_information_tablebases.py" | cut -d ' ' -f 1)" = 5d5130796ccfdac116b8b475f965f74b3d212062f0af44ec30b0ab32770fa71b
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = 52a92ae929da3deaa0fc9dc54f7017f95efbf24ae0b1e907f2fd8b6e89b8e11f
test "$(sha256sum "${dependency_manifest}" | cut -d ' ' -f 1)" = f1daf3e2a0f39e7b275f6acb200d10b4cbf8ff3275528d9284e13db73be661f9
test "$(sha256sum "${output}" | cut -d ' ' -f 1)" = 6919e29cda2593b591baa7d9ae0332990e63c4af74a14aa03cb8eeeae84c1311
test "$(stat -c %s "${output}")" = 1897896056
test "$(sha256sum "${result}" | cut -d ' ' -f 1)" = 244b97fc473b3712fdbe5c0f5e63120b3ce4413a650fe52d57669ec072ccdbc8
test "$(sha256sum "${plan}" | cut -d ' ' -f 1)" = d32eeaa2bfc5c69a361d42b72edce475be2e1d9f70b3b28dd17423080e44e26b
test "$(sha256sum "${log}" | cut -d ' ' -f 1)" = a213294a4720c04d447c1048130d8196f9640c0d8daf51d086e4cc7930ff3028
test ! -e "${work}/certificates/wave-certificate.json"
test ! -e "${preservation_result}"
test "$(df --output=avail -B1 "${work}" | tail -1)" -ge 17179869184

python3 "${runner}" \
  --manifest "${manifest}" \
  --work-directory "${work}" \
  --preserve-completed \
  --aws-execution-ack EC2 \
  --s3-prefix "${prefix}" \
  | tee "${preservation_result}"

test "$(sha256sum "${output}" | cut -d ' ' -f 1)" = 6919e29cda2593b591baa7d9ae0332990e63c4af74a14aa03cb8eeeae84c1311
python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); assert d["status"] == "head-download-full-sha-archive-restore-verified"; assert d["original_scratch_retained"] is True; assert d["local_scratch_retained"] is True; assert d["safe_to_delete_gate"] is False; assert len(d["completed"]) == 1; assert d["completed"][0]["output"]["sha256"] == "6919e29cda2593b591baa7d9ae0332990e63c4af74a14aa03cb8eeeae84c1311"' "${work}/certificates/wave-certificate.json"
python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); assert d["status"] == "completed-local-resume-s3-restored"; assert d["generator_rerun"] is False; assert d["preserved_frontier_reread"] is False; assert d["local_scratch_retained"] is True' "${preservation_result}"

sha256sum "${work}/certificates/wave-certificate.json" "${preservation_result}"
