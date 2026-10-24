# RAMSES Arduino Gateway v0.4.0-beta.24 — identify exact RX-strobe SPI failure

## Evidence from beta.23
Live M5Stack M146 868 MHz output:
- `SPI-pre t=456 part=00 ver=14 marc=01 ... uni=0` (SPI working)
- `SPI-cfg t=458 part=00 ver=14 marc=01 io2=0D/0D pkt=31/31 ... uni=0` (SPI working; `pkt=31` is hexadecimal `0x31`)
- `SPI-srx t=474 part=1F ver=1F marc=1F io2=1F/1F pkt=1F/1F ... uni=1` (SPI register contents no longer readable)
- `SPI-runtime` remains all `0x1F`, `sync=0`, `valid=0`.

Thus the error begins inside CC1101 receive-entry, not during initial SPI setup or register programming. TI CC1101 documentation identifies `0x1F` as a possible **chip status byte** when the radio is receiving and the FIFO count is >=15; that raises the possibility that chip status is being returned in place of register data. That hypothesis is **not proven**.

## New read-only trace
- Captures each command of the *existing* receive-entry sequence, with no new writes, transmit requests or state transitions:
  - `SPI-idle` after SIDLE
  - `SPI-iocfg` after IOCFG0 write
  - `SPI-packet` after setting PKTCTRL0 to `0x32` for asynchronous RX
  - `SPI-flush` after SFRX
  - `SPI-srx-now` immediately after SRX
- Retains the previous `SPI-pre`, `SPI-cfg`, `SPI-srx` (after startup settling), and `SPI-runtime` checkpoints.
- Every snapshot includes `hdr=%02X/%02X/%02X`: the **first SPI byte**, or CC1101 chip status returned while sending the header for PARTNUM, MARCSTATE and IOCFG2. Their register **data bytes** are still logged separately as `part=`, `marc=`, and `io2=`.
- Adds raw first SPI response bytes to authenticated `GET /api/v1/radio/spi-audit` for comparison with data bytes.
- Increases the existing bounded diagnostics event queue to hold the extra startup entries. All operations remain on radioTask; the HTTP endpoint reads cached snapshots.
- Expands SPI snapshot regression tests to ensure header-byte changes trigger logging, and startup log lines remain untruncated.

**This is a diagnostic build, not proof of restored reception.** Do not enable RF TX/pairing. A physical gateway test is still required to determine why SPI readback changes after starting RX.

Arduino IDE/source and firmware ZIP: `RAMSES-Arduino-Gateway-v0.4.0-beta.24-ESP32-WROOM-32E.zip`.
