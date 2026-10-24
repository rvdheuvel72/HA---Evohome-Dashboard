"""Guard the no-SPI interval directly following first CC1101 SRX."""
from pathlib import Path
source = (Path(__file__).resolve().parents[1] / "RamsesGateway" /
          "Cc1101Radio.h").read_text()
rx_enter = source.index("bool enterReceive(bool startupTrace=false)")
srx = source.index("const bool rx=strobe(0x34)", rx_enter)
return_rx = source.index("return rx;", srx)
assert "readSpiAudit(" not in source[srx:return_rx], (
    "Do not read SPI immediately after the SRX command"
)
boot = source.index("const bool rxCommandAccepted=enterReceive(true);")
quiet = source.index("delay(25);", boot)
first_audit = source.index("startupRxSteps_[4]=readSpiAudit", quiet)
between = source[boot:quiet]
assert "status(" not in between and "readSpiAudit(" not in between, (
    "Boot must leave CC1101 SPI undisturbed after SRX until the delay"
)
assert quiet < first_audit
assert "startupAfterRx_=startupRxSteps_[4];" in source[first_audit:first_audit+135]
print("Quiet SRX startup contract PASS")
