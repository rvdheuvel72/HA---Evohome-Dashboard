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
# Both the publish step and the post-publication verification step declare
# the tag and ZIP name. A single matching occurrence must never mask a stale
# reference elsewhere in this release workflow.
expected_tag = f"ramses-arduino-v{version}"
filename = f"RAMSES-Arduino-Gateway-v{version}-ESP32-WROOM-32E.zip"
all_tags = re.findall(r'(?m)^[ \\t]*TAG="([^"]+)"[ \\t]*
assert filename in readme
assert f"RELEASE_NOTES_v{version}.md" in publisher
notes = FW / f"RELEASE_NOTES_v{version}.md"
assert notes.is_file()
assert version in notes.read_text()
assert version in sketch
print(f"RAMSES release contract PASS: {version}")
, publisher)
all_files = re.findall(r'(?m)^[ \\t]*FILE="([^"]+)"[ \\t]*
assert filename in readme
assert f"RELEASE_NOTES_v{version}.md" in publisher
notes = FW / f"RELEASE_NOTES_v{version}.md"
assert notes.is_file()
assert version in notes.read_text()
assert version in sketch
print(f"RAMSES release contract PASS: {version}")
, publisher)
assert all_tags == [expected_tag, expected_tag], (
    f"Publisher and verification TAG values differ: {all_tags!r} != {expected_tag}"
)
assert all_files == [filename, filename], (
    f"Publisher and verification FILE values differ: {all_files!r} != {filename}"
)
assert filename in readme
assert f"RELEASE_NOTES_v{version}.md" in publisher
notes = FW / f"RELEASE_NOTES_v{version}.md"
assert notes.is_file()
assert version in notes.read_text()
assert version in sketch
print(f"RAMSES release contract PASS: {version}")
