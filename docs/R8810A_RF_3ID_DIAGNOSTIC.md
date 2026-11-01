# R8810A RF-path OpenTherm diagnostic (ESP32 firmware beta.37+)

This diagnostic uses **existing** gateway endpoints. No ESP32 reflash, new release, or Home Assistant update is required.

## Requirements

- Python 3.10+ on a computer on the **same trusted LAN** as the ESP32 gateway.
- The gateway reachable by its IP or `http://evohome-gateway` and protected with a strong web password.
- RAMSES CC1101 receiver active and R8810A previously observed.
- Explicit acceptance of RF identity-collision risk: reads are sent under the physical Evohome 01: identity; the utility cannot guarantee no contention with the actual controller.
- No existing packet capture that you need to preserve. If capture data exists, download it from the ESP32 UI first. The diagnostic refuses to overwrite it without `--replace-capture`.

## Windows PowerShell

Clone this repository, open PowerShell at the repo root, then run:

```powershell
py -3 .\scripts\diag_r8810a_three_id.py --gateway http://evohome-gateway --user YOUR_GATEWAY_WEB_USER --authorize-rf
```

Replace the URL with the gateway's LAN IP if the hostname does not resolve. The script securely prompts for the web password. Never include your password in the command line or share it.

## Linux/macOS

```bash
python3 ./scripts/diag_r8810a_three_id.py --gateway http://evohome-gateway --user YOUR_GATEWAY_WEB_USER --authorize-rf
```

The diagnostic first checks the capture status and then performs **one deliberately authorized read per ID**: 18 (CH pressure), 25 (boiler flow), 117 (CH pump starts). Existing bridge restrictions and 15-second cooldowns are honored. It does not retry a failed read, bind, change a heating setpoint, or write OpenTherm data.

## Results

A new `r8810a-diag-YYYYMMDD-HHMMSS` directory contains:

- `report.md`: comparison and interpretation
- `report.json`: structured results with approximate device-uptime windows
- `capture.tsv`: raw RAMSES **received**/rejected decoder events for independent analysis

Please upload `report.json` and `capture.tsv` together. No passwords are written. The RF traffic does include hardware/controller addresses; treat the files as household telemetry, not public data.

**Interpretation limits:** `timeout` never proves unsupported IDs. `Unknown-DataId` indicates an explicit reply but not whether the bridge or the boiler made that decision. Injected RF requests use the observed Evohome identity; a matching RF response could also belong to the real controller. A direct wired OpenTherm monitor is required to establish what was forwarded over the wire.

## Offline regression tests

```bash
python3 -m unittest discover -s tests -p test_diag_r8810a_three_id.py -v
```

These use fake gateways and never transmit RF.