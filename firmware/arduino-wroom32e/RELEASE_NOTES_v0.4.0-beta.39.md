# RAMSES Arduino Gateway v0.4.0-beta.39 — global RF method and read reliability

## Global transmission method (default FIFO)
The authenticated `GET/POST /api/v1/opentherm/tx-method` persists one
global RAMSES transmit-method preference (FIFO or UART) in ESP32 NVS. It can be
set from **Gateway → Settings → Radio** or **Home Assistant v0.3.9 → Settings**.
The last successful save wins and requires no reboot. The setting is applied
to all compatible outgoing gateway-originated RAMSES packets, including:
- the manual 16-ID OpenTherm batch and individual bridge reads;
- the gateway-source ID25 one-shot and normal read requests;
- the virtual thermostat's pairing/temperature frames; and
- pairing / verification traffic through the gateway's binding sender.

FIFO uses the reference-aligned, primed bitstream with bounded variable length
and **never writes more than the CC1101's 64-byte FIFO capacity**. Frames
whose encoded UART representation cannot fit the single hardware FIFO use
UART as an explicit safety fallback, with the actual method reported in
read diagnostics. No unverified multi-packet FIFO streaming is introduced.

## Manual read-only CV batch resilience
- One initial request plus **at most two** retry attempts per Data-ID, only
  after a timeout or local TX failure, preserving the 15-second spacing.
- No retry for an explicit DATA_INVALID or UNKNOWN_ID response.
- Retries are only within the already authorized manual 16-ID batch, and no
  OpenTherm write, ID0 master-status request, pairing or heating-control
  retry is introduced.
- One-shot ID25 remains exactly one request with its 60-second cooldown.
- New reads defer for 300 ms after an observed valid RAMSES frame to reduce
  traffic overlap; this is **not** CC1101 RSSI-based clear-channel assessment.
- Batch status exposes per-ID retry count, actual method and response latency.

## Validation limits
Compile/host regression results are provided by CI. Live CC1101 RF delivery
success rates, latency distributions and coexistence with the real Evohome
controller are **not** validated by CI. Compare success/timeout counts before
and after flashing this release and do not assume a local TX completion is
proof of a remote Read-Ack.
