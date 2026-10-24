// CC1101 receive-only radio driver for ESP32 Arduino 3.x.
// Register values adapted from IndaloTech/ramses_esp CC1101 parameters.
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "RadioFrequency.h"
#include "OpenThermTxDiagnostics.h"
#include "RamsesUartFifo.h"
#include "RamsesRxDiagnostics.h"
#include "RamsesRxHealth.h"
#include "Cc1101Startup.h"
#include "RamsesSpiAudit.h"

class Cc1101Radio {
 public:
  static constexpr uint8_t SCK = 18, MISO = 19, MOSI = 23, CS = 5;
  static constexpr uint8_t GDO0 = 26, GDO2 = 27;

  bool begin(uint32_t frequencyKhz=868300, uint16_t moduleBand=868,
             bool experimentalRx=false) {
    initStage_="frequency_validation";
    uint32_t synthesizer=0;
    if (!radio_frequency::synth(frequencyKhz,moduleBand,synthesizer,experimentalRx)) return false;
    expectedFrequencyWord_=synthesizer;
    frequencyKhz_=frequencyKhz;
    moduleBand_=moduleBand;
    experimentalRx_=experimentalRx;
    initStage_="spi_chip_identification";
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);
    pinMode(MISO, INPUT);
    SPI.begin(SCK, MISO, MOSI, CS);
    // The ESP32 can soft-reboot without power-cycling the 5V M5Stack
    // module. Perform the full CC1101 CSn/SRES wake sequence and retry
    // boundedly if SO remains busy or the silicon ID cannot be read.
    bool resetOk=false;
    part_=version_=0xFF;
    startupMarcState_=0xFF;
    startupRxConfirmed_=false;
    startupRxRetries_=0;
    for (uint8_t attempt=0;attempt<3;attempt++) {
      if(reset()){
        part_=status(0x30); // PARTNUM
        version_=status(0x31); // VERSION
        if(cc1101_startup::identified(part_,version_)){
          resetOk=true;break;
        }
      }
      delay(40);
    }
    if(!resetOk)return false;
    initStage_="register_programming";
    for (uint8_t i = 0; i < sizeof(settings_); ++i)
      if (!writeRegister(i, settings_[i])) return false;
    initStage_="frequency_programming";
    if (!writeRegister(0x0D, static_cast<uint8_t>(synthesizer >> 16)) ||
        !writeRegister(0x0E, static_cast<uint8_t>(synthesizer >> 8)) ||
        !writeRegister(0x0F, static_cast<uint8_t>(synthesizer))) return false;
    // This was the beta.18 regression: MARCSTATE sampled immediately after
    // SRX can be transient (e.g. IDLE/CAL) even when SPI and the SRX strobe
    // succeeded. That check caused detected=0 with PARTNUM=00 VERSION=14,
    // and retried SRES forever. Observe RX state but do NOT treat it as a
    // second chip-detection gate. The rx-health API reports live state.
    initStage_="rx_strobe";
    const bool rxCommandAccepted=enterReceive();
    if(!rxCommandAccepted)return false;
    const uint32_t started=micros();
    do {
      startupMarcState_=cc1101_startup::decodeMarcState(status(0x35));
      if(cc1101_startup::receiving(startupMarcState_))break;
      delayMicroseconds(150);
    } while(static_cast<uint32_t>(micros()-started)<15000u);
    startupRxConfirmed_=cc1101_startup::receiving(startupMarcState_);
    // Recover an actual stable IDLE/overflow state once, without SRES.
    // Do not disturb active RX or retry on a failed SPI status read (0xFF).
    if(!startupRxConfirmed_ &&
       (startupMarcState_==0x01u || startupMarcState_==0x11u)) {
      ++startupRxRetries_;
      if(enterReceive()) {
        const uint32_t retryStart=micros();
        do {
          startupMarcState_=cc1101_startup::decodeMarcState(status(0x35));
          if(cc1101_startup::receiving(startupMarcState_))break;
          delayMicroseconds(150);
        } while(static_cast<uint32_t>(micros()-retryStart)<15000u);
        startupRxConfirmed_=cc1101_startup::receiving(startupMarcState_);
      }
    }
    initStage_="initialized";
    return cc1101_startup::ready(part_,version_,rxCommandAccepted);
  }

  // SPI and GPIO sampling is restricted to radioTask, never HTTP handlers.
  // One GPIO level is not evidence of a valid asynchronous waveform.
  ramses_rx_health::Sample readRxHealth(uint32_t now,uint32_t uartBytes) {
    ramses_rx_health::Sample s;
    s.uptime_ms=now;
    s.uart_bytes=uartBytes;
    s.marcstate=cc1101_startup::decodeMarcState(status(0x35));
    s.iocfg2=readRegister(0x00);
    s.pktctrl0=readRegister(0x08);
    s.gdo2_level=static_cast<uint8_t>(digitalRead(GDO2));
    return s;
  }
  // Collect a bounded register readback on radioTask only. This never
  // strobes, rewrites registers, resets the CC1101 or transmits RF.
  ramses_spi_audit::Snapshot readSpiAudit(uint32_t uptimeMs) {
    ramses_spi_audit::Snapshot x;
    x.uptime_ms=uptimeMs;
    x.part=status(0x30);
    x.version=status(0x31);
    x.marc_raw=status(0x35); // preserve raw 8-bit data, not masked 5-bit state
    x.iocfg2_a=readRegister(0x00);
    x.iocfg2_b=readRegister(0x00);
    x.pktctrl0_a=readRegister(0x08);
    x.pktctrl0_b=readRegister(0x08);
    x.mdmcfg2=readRegister(0x12);
    x.freq2=readRegister(0x0D);
    x.freq1=readRegister(0x0E);
    x.freq0=readRegister(0x0F);
    x.expected_freq_word=expectedFrequencyWord_;
    x.sampled=true;
    return x;
  }
  uint8_t part() const { return part_; }
  uint8_t version() const { return version_; }
  uint8_t startupMarcState() const { return startupMarcState_; }
  bool startupRxConfirmed() const { return startupRxConfirmed_; }
  uint8_t startupRxRetries() const { return startupRxRetries_; }
  const char* initStage() const { return initStage_; }
  uint32_t frequencyKhz() const { return frequencyKhz_; }
  uint16_t moduleBand() const { return moduleBand_; }

  // Both TX paths operate exclusively on radioTask. Neither one guarantees
  // that a remote bridge received RF energy or decoded the frame.
  bool beginReadRequestTx(bool experimentalOneShot=false,
                          ot_tx::Report* details=nullptr) {
    ot_tx::Report local;
    ot_tx::Report& d=details?*details:local;
    d.method=ot_tx::ASYNC_UART;
    if (!txFrequencyAllowed(experimentalOneShot)) {
      d.phase=ot_tx::FREQUENCY_REJECTED;return false;
    }
    if(!prepareTx(d))return false;
    if (!writeRegister(0x08,0x32)) { // async serial TX, no packet FIFO
      d.phase=ot_tx::TX_CONFIG_FAILED;enterReceive();return false;
    }
    // Read live chip registers. This cannot verify the physical GDO0 wire
    // or that a complete frame was radiated.
    uint32_t expected=0;
    d.verification_measured=true;
    if(!radio_frequency::synth(frequencyKhz_,moduleBand_,expected,experimentalRx_)) {
      d.phase=ot_tx::FREQUENCY_REJECTED;enterReceive();return false;
    }
    d.frequency_expected=expected;
    d.frequency_readback=(static_cast<uint32_t>(readRegister(0x0D))<<16) |
      (static_cast<uint32_t>(readRegister(0x0E))<<8) | readRegister(0x0F);
    d.pktctrl0_readback=readRegister(0x08);
    d.iocfg0_readback=readRegister(0x02);
    d.mdmcfg4_readback=readRegister(0x10);
    d.mdmcfg3_readback=readRegister(0x11);
    d.mdmcfg2_readback=readRegister(0x12);
    if(!strobe(0x35)) {
      d.phase=ot_tx::TX_START_FAILED;enterReceive();return false;
    }
    const uint32_t started=micros();
    while((status(0x35)&0x1fu)!=0x13u) {
      if(micros()-started>10000) {
        d.marc_during=status(0x35)&0x1fu;
        d.phase=ot_tx::TX_START_FAILED;enterReceive();return false;
      }
      delayMicroseconds(100);
    }
    d.saw_tx=true;
    d.marc_during=0x13;
    return true;
  }

  bool endReadRequestTx(ot_tx::Report* details=nullptr) {
    const bool idle=strobe(0x36);
    const bool rx=enterReceive();
    if(details) {
      details->marc_after=status(0x35)&0x1fu;
      details->rx_restored=idle && rx;
    }
    return idle && rx;
  }

  // Read MARCSTATE while the ESP32 UART has finished sending but before SIDLE.
  uint8_t txMarcState() { return status(0x35)&0x1fu; }

  // Observe a stable RX state after the existing TX teardown. The
  // immediate SRX strobe can leave the chip in a transient calibration state.
  // This is observation only: no further TX, reset or receive strobes.
  void verifyPostTxRx(ot_tx::Report& d) {
    const uint32_t started=micros();
    do {
      d.marc_after=status(0x35)&0x1fu;
      if(d.marc_after==0x0Du) {
        d.rx_state_confirmed=true;
        return;
      }
      delayMicroseconds(100);
    } while(static_cast<uint32_t>(micros()-started)<5000u);
  }

  // The reference RAMSES project first primes its FIFO with a 4-byte
  // start/break (00 FF 00 00), then sends 44 UART bytes as 55 8N1 RF
  // bytes and ends with an FF idle MARK: 60 FIFO bytes total.
  // Fixed-length FIFO transmission avoids infinite-streaming underrun risks.
  // HW sync/CRC disabled: RAW RAMSES preamble/sync and checksum are included.
  bool sendOneShotFifo(const uint8_t* wire,size_t wireLength,
                       bool experimentalOneShot,ot_tx::Report& d) {
    d.method=ot_tx::REFERENCE_FIFO;
    const uint32_t begun=micros();
    if(!wire || wireLength!=ramses_uart_fifo::INPUT_LEN ||
       !txFrequencyAllowed(experimentalOneShot)) {
      d.phase=ot_tx::FREQUENCY_REJECTED;return false;
    }
    uint8_t txFifo[ramses_uart_fifo::REFERENCE_RF_LEN]={};
    size_t txLength=0;
    if(!ramses_uart_fifo::encodePrimedReference(
          wire,wireLength,txFifo,sizeof(txFifo),txLength) ||
       txLength!=ramses_uart_fifo::REFERENCE_RF_LEN) {
      d.phase=ot_tx::TX_CONFIG_FAILED;return false;
    }
    d.expected_bytes=static_cast<uint16_t>(txLength);
    if(!prepareTx(d))return false;
    if(!writeRegister(0x02,0x2E) ||  // GDO0 stays high-Z: ESP32 UART TX drives this wire
       !writeRegister(0x06,static_cast<uint8_t>(txLength)) ||
       !writeRegister(0x08,0x00) || // fixed length; FIFO; no HW CRC
       !strobe(0x3B)) {            // flush TX FIFO in IDLE
      d.phase=ot_tx::TX_CONFIG_FAILED;
      endReadRequestTx(&d);return false;
    }
    if(!writeFifoBurst(txFifo,txLength)) {
      d.phase=ot_tx::FIFO_PREFILL_FAILED;
      endReadRequestTx(&d);return false;
    }
    d.written_bytes=static_cast<uint16_t>(txLength);
    d.txbytes_before=status(0x3A);
    if((d.txbytes_before&0x7fu)!=txLength ||
       (d.txbytes_before&0x80u)) {
      d.phase=ot_tx::FIFO_PREFILL_FAILED;
      endReadRequestTx(&d);return false;
    }
    if(!strobe(0x35)) { // STX, fixed length 60 FIFO bytes incl. priming
      d.phase=ot_tx::TX_START_FAILED;
      endReadRequestTx(&d);return false;
    }
    bool idleComplete=false;
    for(;;) {
      const uint8_t state=status(0x35)&0x1fu;
      if(state==0x13u)d.saw_tx=true;
      d.marc_during=state;
      if(state==0x16u) {d.phase=ot_tx::TX_UNDERFLOW;break;}
      if(state==0x01u && d.saw_tx) {idleComplete=true;break;}
      if(micros()-begun>ot_tx::Report::MAX_FIFO_TX_US) {
        d.phase=d.saw_tx?ot_tx::TX_TIMEOUT:ot_tx::TX_START_FAILED;
        break;
      }
      delayMicroseconds(100);
    }
    d.elapsed_us=micros()-begun;
    d.txbytes_after=status(0x3A);
    if(d.txbytes_after&0x80u)d.phase=ot_tx::TX_UNDERFLOW;
    else if(idleComplete && (d.txbytes_after&0x7fu)==0)
      d.phase=ot_tx::LOCAL_COMPLETE;
    else if(idleComplete)d.phase=ot_tx::TX_TIMEOUT;
    const bool rxOk=endReadRequestTx(&d);
    if(!rxOk)d.phase=ot_tx::RX_RESTORE_FAILED;
    return d.phase==ot_tx::LOCAL_COMPLETE;
  }

  bool enterReceive() {
    if (!strobe(0x36)) return false; // SIDLE
    if (!writeRegister(0x02, 0x2E)) return false; // IOCFG0 unused in RX
    if (!writeRegister(0x08, 0x32)) return false; // PKTCTRL0 async RX
    if (!strobe(0x3A)) return false; // SFRX
    return strobe(0x34); // SRX; never STX
  }

  // HGI80-style positive attenuation magnitude; signed CC1101 RSSI.
  // Sample is taken after a frame, so it is only an indicative RSSI.
  uint8_t rssiMagnitude() {
    return ramses_rx::rssiMagnitude(status(0x34));
  }

 private:
  bool txFrequencyAllowed(bool experiment) const {
    return radio_frequency::evohomeTxFrequency(frequencyKhz_,moduleBand_) ||
      (experiment && radio_frequency::experimentalOneShotFrequency(
        frequencyKhz_,moduleBand_,experimentalRx_));
  }
  uint8_t readRegister(uint8_t reg) {
    if(!select())return 0xff;
    SPI.transfer(reg|0x80u);
    const uint8_t result=SPI.transfer(0);
    release();return result;
  }
  bool writeFifoBurst(const uint8_t* bytes,size_t count) {
    if(!bytes || !count || count>64 || !select())return false;
    SPI.transfer(0x7f); // 0x3F TX FIFO + burst bit
    for(size_t i=0;i<count;i++)SPI.transfer(bytes[i]);
    release();return true;
  }
  bool prepareTx(ot_tx::Report& d) {
    d.marc_before=status(0x35)&0x1fu;
    if(!strobe(0x36)) {
      d.phase=ot_tx::RADIO_IDLE_FAILED;return false;
    }
    // 868 MHz nominal -10dBm from TI CC1101 PA table; antenna/board
    // matching is unknown, so this is NOT a radiated-power certification.
    d.pa_configured=ot_tx::Report::PA_868_LOW;
    if(!writeRegister(0x3e,d.pa_configured)) {
      d.phase=ot_tx::PA_PROGRAM_FAILED;enterReceive();return false;
    }
    d.pa_readback=readRegister(0x3e);
    if(d.pa_readback!=d.pa_configured) {
      d.phase=ot_tx::PA_PROGRAM_FAILED;enterReceive();return false;
    }
    return true;
  }

  // 868.3 MHz, ~38.38 kb/s GFSK, asynchronous serial RX on GDO2.
  // Source: IndaloTech/ramses_esp components/cc1101/cc1101_param.c
  static constexpr uint8_t settings_[] = {
      0x0D,0x2E,0x2E,0x07,0xD3,0x91,0xFF,0x04,
      0x31,0x00,0x00,0x0F,0x00,0x21,0x65,0x6A,
      0x6A,0x83,0x10,0x22,0xF8,0x50,0x07,0x30,
      0x18,0x16,0x6C,0x43,0x40,0x91,0x87,0x6B,
      0xF8,0x56,0x10,0xE9,0x21,0x00,0x1F,0x41,
      0x00,0x59,0x7F,0x3F,0x81,0x35,0x09
  };
  uint8_t part_ = 0xFF, version_ = 0xFF;
  uint8_t startupMarcState_ = 0xFF;
  bool startupRxConfirmed_ = false;
  uint8_t startupRxRetries_ = 0;
  const char* initStage_ = "not_started";
  uint32_t expectedFrequencyWord_ = 0;
  uint32_t frequencyKhz_ = 868300;
  uint16_t moduleBand_ = 868;
  bool experimentalRx_=false;

  bool select() {
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(CS, LOW);
    const uint32_t start = micros();
    while (digitalRead(MISO) == HIGH) {
      if (micros() - start > 20000) {
        digitalWrite(CS, HIGH);
        SPI.endTransaction();
        return false;
      }
    }
    return true;
  }
  void release() {
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
  }
  uint8_t status(uint8_t reg) {
    if (!select()) return 0xFF;
    SPI.transfer(reg | 0xC0); // Status register: burst-read bit required
    const uint8_t result = SPI.transfer(0x00);
    release();
    return result;
  }
  bool writeRegister(uint8_t reg, uint8_t value) {
    if (!select()) return false;
    SPI.transfer(reg);
    SPI.transfer(value);
    release();
    return true;
  }
  bool strobe(uint8_t instruction) {
    if (!select()) return false;
    SPI.transfer(instruction);
    release();
    return true;
  }
  bool reset() {
    // TI CC1101 recommended manual reset sequence. The initial CSn-high
    // pulse and CSn low/high toggle allow an already-powered chip to leave
    // an old SPI transaction after an ESP32-only reset. No RF transmission.
    digitalWrite(CS,HIGH);delayMicroseconds(60);
    digitalWrite(CS,LOW); delayMicroseconds(60);
    digitalWrite(CS,HIGH);delayMicroseconds(60);
    if(!select())return false;
    SPI.transfer(0x30); // SRES
    release();
    delay(5); // allow crystal and serial interface to become ready
    return true;
  }
};
