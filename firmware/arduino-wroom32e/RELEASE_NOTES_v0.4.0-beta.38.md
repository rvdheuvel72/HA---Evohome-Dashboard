# RAMSES Arduino Gateway v0.4.0-beta.38

## Shared FIFO-first ID25 diagnostic method

- Adds persistent, gateway-owned ID25 one-shot TX method selection in a
  separate ESP32 NVS namespace: FIFO is the default, UART remains an explicitly
  chosen experimental comparison mode.
- Gateway Diagnostics page has **Read/Save shared method** controls. Home
  Assistant Evohome Dashboard v0.3.8 reads and writes the same setting.
- Authenticated `GET /api/v1/opentherm/tx-method` reads the current choice;
  authenticated `POST` (header `X-Gateway-Config: tx-method`) saves it.
  Saves take effect without reboot. Last successful save wins.
- ID25 one-shot POST ignores per-request method and always uses the saved
  gateway choice. Other RF transmissions are not affected.
- Retains manual confirmation, 60-second cooldown, single-attempt bound,
  read-only ID25 guard, strong authentication and existing RF safety controls.
- A local TX completion alone remains insufficient evidence of RF delivery.

## Required upgrade

Flash this gateway firmware to use the shared setting. Then install HA
integration v0.3.8 to manage it through the HA Gateway test page. The older
HA integration's per-request selector is not a persisted shared setting.

Hardware acceptance remains outstanding. Compare real FIFO/UART Read-Ack
outcomes and confirm NVS persistence across a powered restart.
