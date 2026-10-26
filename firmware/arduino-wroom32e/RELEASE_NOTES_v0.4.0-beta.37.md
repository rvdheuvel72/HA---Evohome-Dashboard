# RAMSES Arduino Gateway 0.4.0-beta.37

## Restored: full 17-entry OpenTherm diagnostic dashboard

Fixes the beta.36 regression that reduced the visible diagnostics to four
entries. The manually authorized **Read all data** scan requests **16** IDs
at least **15 seconds apart** through the observed R8810A bridge.

All original eight requests are restored:
* 17 — burner modulation (%)
* 18 — CH circuit pressure (bar)
* 25 — boiler flow temperature (°C)
* 28 — heating return temperature (°C; reported value unverified)
* 5 — fault/service flags (raw hex)
* 33 — exhaust temperature (°C)
* 57 — maximum CH setpoint (°C; read only)
* 26 — domestic hot-water temperature (°C)

Eight additional diagnostic IDs are included:
* 117 — CH pump starts (u16)
* 121 — CH pump operating hours (u16)
* 34 — boiler heat exchanger temperature (f8.8)
* 35 — boiler fan speed, setpoint and actual packed u8/u8 (encoded raw bytes;
  avoid assuming actual RPM without boiler-specific scaling)
* 36 — burner flame current (f8.8 µA)
* 116 — burner starts (u16)
* 120 — burner operating hours (u16)
* 115 — manufacturer diagnostic code (u16, displayed raw hex)

The seventeenth visible entry is **ID0 central-heating activity**,
observed passively from the real R8810A bridge. ID0 is never requested
by this manual scan, because an incorrect master-status field could
misrepresent heating enable flags.

The web UI groups entries by heating circuit, boiler operation, and
service diagnostics. A separate individual-read selector exposes all 16
allowed IDs. Genuine Read-Ack replies for raw/packed values are no
longer incorrectly labeled "no verified Read-Ack".

The original gateway-source legacy ID25 experiment remains a separate
advanced test. No automatic polling, retries, heating-control writes,
pairing, or unbounded RF transmission is introduced. The normal manual
cooldown stays **15 seconds** and the observed 01: address conflict and
quiet-window safety checks remain enforced.

**Limitations:** Only IDs previously demonstrated to return Read-Ack
on this installation (17, 18, 25, 28, 57) have confirmed response
evidence; other IDs may time out or report unsupported. Matching RF
responses and local TX completion do not prove the request caused the
response. Scan values are RAM-only and reset on new scan or gateway
reboot; older beta.35/beta.36 values cannot be recovered from firmware.
