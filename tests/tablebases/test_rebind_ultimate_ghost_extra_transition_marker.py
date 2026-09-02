import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/tablebases/rebind_ultimate_ghost_extra_transition_marker.py"


def test_rebinds_only_source_and_hardlinks_payload(tmp_path: Path) -> None:
    source = tmp_path / "source"
    destination = tmp_path / "destination"
    header = b"extra-transition-header"
    old_source = "1" * 64
    new_source = "2" * 64
    model = "3" * 64
    observation = "4" * 64
    lower = [str(index) * 64 for index in range(5, 8)]
    components = []
    for index, suffix in enumerate(
            (".header", ".meta", ".strata", ".index", ".blocks")):
        payload = header if suffix == ".header" else bytes([index]) * (index + 3)
        Path(str(source) + suffix).write_bytes(payload)
        components.append(hashlib.sha256(payload).hexdigest())
    marker = (header + "".join(lower).encode() + old_source.encode() +
              model.encode() + observation.encode() +
              "".join(components).encode())
    Path(str(source) + ".verified").write_bytes(marker)
    manifest = tmp_path / "manifest.json"

    subprocess.run([
        sys.executable, str(TOOL), str(source), str(destination),
        "--old-source-sha256", old_source,
        "--new-source-sha256", new_source,
        "--model-sha256", model,
        "--observation-sha256", observation,
        "--manifest", str(manifest),
    ], check=True)

    rebound = Path(str(destination) + ".verified").read_bytes()
    source_offset = len(header) + 3 * 64
    assert rebound[:source_offset] == marker[:source_offset]
    assert rebound[source_offset:source_offset + 64] == new_source.encode()
    assert rebound[source_offset + 64:] == marker[source_offset + 64:]
    for suffix in (".header", ".meta", ".strata", ".index", ".blocks"):
        assert Path(str(source) + suffix).stat().st_ino == \
            Path(str(destination) + suffix).stat().st_ino
    receipt = json.loads(manifest.read_text())
    assert receipt["payload_changed"] is False
    assert receipt["residual"] == 0


def test_rejects_component_tamper(tmp_path: Path) -> None:
    source = tmp_path / "source"
    destination = tmp_path / "destination"
    header = b"header"
    bindings = [str(index) * 64 for index in range(1, 7)]
    components = []
    for index, suffix in enumerate(
            (".header", ".meta", ".strata", ".index", ".blocks")):
        payload = header if suffix == ".header" else bytes([index])
        Path(str(source) + suffix).write_bytes(payload)
        components.append(hashlib.sha256(payload).hexdigest())
    marker = header + "".join(bindings + ["7" * 64, "8" * 64,
                                           "9" * 64] + components).encode()
    Path(str(source) + ".verified").write_bytes(marker)
    Path(str(source) + ".blocks").write_bytes(b"tampered")

    result = subprocess.run([
        sys.executable, str(TOOL), str(source), str(destination),
        "--old-source-sha256", "7" * 64,
        "--new-source-sha256", "a" * 64,
        "--model-sha256", "8" * 64,
        "--observation-sha256", "9" * 64,
        "--manifest", str(tmp_path / "manifest.json"),
    ], text=True, capture_output=True)
    assert result.returncode != 0
    assert "transition .blocks mismatch" in result.stderr
