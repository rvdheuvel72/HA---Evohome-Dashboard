# RAMSES Arduino Gateway 0.4.0-beta.36

## Central-heating diagnostics instead of boiler-only measurements

The ordinary, manually authorized **Read heating data** scan now requests
only three OpenTherm IDs from the R8810A through RAMSES-II 3220:
* **ID18** — central-heating water pressure (bar)
* **ID117** — CH circulating pump starts (unsigned counter)
* **ID121** — CH circulating pump operating hours (unsigned counter)

Burner modulation (ID17), boiler water temperature (ID25), return
temperature (ID28), boiler fault/service flags (ID5), exhaust temperature
(ID33), maximum CH setpoint (ID57), and DHW temperature (ID26) have been
removed from the normal scan and normal single-read choices.
The standalone **advanced legacy ID25** experiment is unchanged and is
clearly separated from the ordinary heating scan.

**ID0 heating status** is shown passively, from checksum-validated real
Read-Ack replies sent by the configured R8810A to the observed original
Evohome Display. CH-active is taken from the OpenTherm slave-status bit.
No ID0 Read-Data request is transmitted: its master-status byte can
affect CH enable demand even if the message type is Read-Data.
The passive status expires after five minutes without an observed reply.

**15-second minimum RF spacing** replaces the previous 60 seconds for
the normal manual R8810A reads. A three-value scan needs at least two
15-second gaps; it may take longer if bridge replies time out or RF
quiet-window checks block sending. Read requests are explicitly
authorized, one at a time, with no binding traffic, boiler writes,
automatic retry, or background active polling. Source-identity conflict
checks remain in effect.

**Limitations:** Support for IDs 117 and 121 has not been confirmed
on this Intergas/Honeywell installation. Unsupported or unanswered IDs
are not silently treated as zero. CH pump counters, if supported, refer
to the pump reported by the OpenTherm boiler interface, **not** the
separate floor-heating circulation pump. Reusing the real Evohome Display
01: source address can collide with the actual controller; 15 seconds
reduces the interval between these deliberately authorized requests,
not that RF collision risk.
