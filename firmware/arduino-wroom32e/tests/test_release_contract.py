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

# Both the publication and post-publication verification steps carry their
# own identifiers; do not accept a stale tag or ZIP in either step.
expected_tag = f"ramses-arduino-v{version}"
filename = f"RAMSES-Arduino-Gateway-v{version}-ESP32-WROOM-32E.zip"
tags = re.findall(r'(?m)^[ \t]*TAG="([^"]+)"[ \t]*$', publisher)
files = re.findall(r'(?m)^[ \t]*FILE="([^"]+)"[ \t]*$', publisher)
assert tags == [expected_tag, expected_tag], (
    f"Unexpected publish/verify tags: {tags!r}; expected {expected_tag}"
)
assert files == [filename, filename], (
    f"Unexpected publish/verify asset names: {files!r}; expected {filename}"
)
assert filename in readme
assert f"RELEASE_NOTES_v{version}.md" in publisher
notes = FW / f"RELEASE_NOTES_v{version}.md"
assert notes.is_file()
assert version in notes.read_text()
assert version in sketch
print(f"RAMSES release contract PASS: {version}")
