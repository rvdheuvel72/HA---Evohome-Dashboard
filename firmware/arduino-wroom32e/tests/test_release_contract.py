"""Regression contract for published RAMSES Arduino beta identifiers."""
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

expected_tag = f"ramses-arduino-v{version}"
expected_file = f"RAMSES-Arduino-Gateway-v{version}-ESP32-WROOM-32E.zip"
tags = re.findall(r'(?m)^[ \t]*TAG="([^"]+)"[ \t]*$', publisher)
files = re.findall(r'(?m)^[ \t]*FILE="([^"]+)"[ \t]*$', publisher)
assert tags == [expected_tag, expected_tag], f"Incorrect publish/verify TAG values: {tags}"
assert files == [expected_file, expected_file], f"Incorrect publish/verify FILE values: {files}"

# Also detect stale references outside the assignments, e.g. in the dating step.
seen_tags = set(re.findall(r"ramses-arduino-v0\.4\.0-beta\.\d+", publisher))
seen_files = set(re.findall(r"RAMSES-Arduino-Gateway-v0\.4\.0-beta\.\d+-ESP32-WROOM-32E\.zip", publisher))
seen_notes = set(re.findall(r"RELEASE_NOTES_v0\.4\.0-beta\.\d+\.md", publisher))
assert seen_tags == {expected_tag}, seen_tags
assert seen_files == {expected_file}, seen_files
assert seen_notes == {f"RELEASE_NOTES_v{version}.md"}, seen_notes
assert expected_file in readme
assert expected_tag in readme

notes = FW / f"RELEASE_NOTES_v{version}.md"
assert notes.is_file() and version in notes.read_text()
seen_fw_versions = set(re.findall(r"0\.4\.0-beta\.\d+", sketch))
assert seen_fw_versions == {version}, seen_fw_versions
print(f"RAMSES release contract PASS: {version}")
