"""Ensure the standalone RAMSES firmware version, publisher and docs agree."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
FW = ROOT / "firmware/arduino-wroom32e"
sketch = (FW / "RamsesGateway/RamsesGateway.ino").read_text()
publisher = (ROOT / ".github/workflows/publish-ramses-arduino-v0.4-beta.yml").read_text()
readme = (FW / "README.md").read_text()
match = re.search(r'constexpr char FW_VERSION\[\] = "([^"]+)"', sketch)
assert match, "Firmware version constant missing"
version = match.group(1)
assert re.fullmatch(r"0\.4\.0-beta\.\d+", version), version
human = version.replace("-beta", " beta")
assert publisher.splitlines()[0] == f"name: Release RAMSES Arduino v{human}"
assert f'TAG="ramses-arduino-v{version}"' in publisher
filename = f"RAMSES-Arduino-Gateway-v{version}-ESP32-WROOM-32E.zip"
assert f'FILE="{filename}"' in publisher
assert filename in readme
assert f"RELEASE_NOTES_v{version}.md" in publisher
notes = FW / f"RELEASE_NOTES_v{version}.md"
assert notes.is_file()
assert version in notes.read_text()
assert version in sketch
print(f"RAMSES release contract PASS: {version}")
