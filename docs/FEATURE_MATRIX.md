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
| Local/no-cloud control | Not implemented | Future RF/LAN research |
| HomeBoard integration | Deferred | Separate phase |

Do not equate a 'catalog only' entry with control support. UI can never create native controller functions that the transport does not expose.
