# Evohome touchscreen feature matrix

| Capability | Phase 1 | Source / limitation |
|---|---|---|
| Room temperatures & targets | Implemented (needs hardware validation) | TCC v2 status |
| Change target temperature | Implemented | TCC v2 zone heat setpoint |
| Temporary/permanent override | Implemented | TCC v2 zone override |
| Cancel override | Implemented | Follow Schedule |
| Operating modes | Implemented for modes offered by system | TCC v2 system mode |
| Hot water on/off/resume | Implemented if DHW is installed | TCC v2 DHW |
| Weekly schedule editor | Implemented | TCC v2 schedules, 7-day constraints |
| Schedule conflict prevention | Implemented | Fresh GET + revision hash |
| Error log | Implemented | Private HA storage + HA log |
| Cloud diagnostics | Implemented | Polling health, faults |
| RF installer/binding menu | Catalog only | No verified TCC API |
| Optimum Start/Stop | Catalog only | No verified TCC API control |
| Advanced boiler settings | Catalog only | No verified TCC API control |
| Language/Wi-Fi/factory reset | Catalog only | Physical controller |
| Passive RF heat-demand observation | Implemented; live TRV accuracy still under test | ESP32 MQTT RX, not cloud control |
| Manual OpenTherm ID25 test | Implemented in bundled HA panel v0.3.8; FIFO Read-Ack observed in field test | Admin-only ESP32 LAN diagnostic; experimental RF TX |
| Global gateway RAMSES RF transport | FIFO default, UART selectable in HA and gateway Settings | ESP32 beta.39 uses the same persistent NVS setting for all outgoing compatible RAMSES frames; oversized frames fall back to UART |\n| Manual CV read resilience | Bounded retries and cached last-good values | Maximum three attempts per ID after timeout/local TX failure, 15-second spacing; response latency and actual transport in gateway diagnostics |
| Local/no-cloud heating control | Not implemented | RF monitoring/diagnostics do not replace Evohome controller |
| HomeBoard integration | Deferred | Separate phase |

Do not equate a 'catalog only' entry with control support. UI can never create native controller functions that the transport does not expose.
