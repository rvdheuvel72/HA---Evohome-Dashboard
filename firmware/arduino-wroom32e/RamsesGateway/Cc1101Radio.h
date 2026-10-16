// CC1101 receive-only radio driver for ESP32 Arduino 3.x.
// Register values adapted from IndaloTech/ramses_esp CC1101 parameters.
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "RadioFrequency.h"

class Cc1101Radio {
 public:
  static constexpr uint8_t SCK = 18, MISO = 19, MOSI = 23, CS = 5;
  static constexpr uint8_t GDO0 = 26, GDO2 = 27;

  bool begin(uint32_t frequencyKhz=868300, uint16_t moduleBand=868,
             bool experimentalRx=false) {
    uint32_t synthesizer=0;
    if (!radio_frequency::synth(frequencyKhz,moduleBand,synthesizer,experimentalRx)) return false;
    frequencyKhz_=frequencyKhz;
    moduleBand_=moduleBand;
    experimentalRx_=experimentalRx;
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);
    pinMode(MISO, INPUT);
    SPI.begin(SCK, MISO, MOSI, CS);
    if (!reset()) return false;
    part_ = status(0x30);  // PARTNUM
    version_ = status(0x31); // VERSION
    if (part_ != 0x00 || version_ == 0x00 || version_ == 0xFF) return false;
    for (uint8_t i = 0; i < sizeof(settings_); ++i)
      if (!writeRegister(i, settings_[i])) return false;
    if (!writeRegister(0x0D, static_cast<uint8_t>(synthesizer >> 16)) ||
        !writeRegister(0x0E, static_cast<uint8_t>(synthesizer >> 8)) ||
        !writeRegister(0x0F, static_cast<uint8_t>(synthesizer))) return false;
    return enterReceive();
  }

  uint8_t part() const { return part_; }
  uint8_t version() const { return version_; }
  uint32_t frequencyKhz() const { return frequencyKhz_; }
  uint16_t moduleBand() const { return moduleBand_; }

  // Caller MUST be the exclusive owner of radio and UART (radioTask).
  // The GDO0 pin becomes a serial DATA INPUT to CC1101 during async TX.
  // The transmit output is NOT independently hardware validated.
  bool beginReadRequestTx(bool experimentalOneShot=false) {
    if (!radio_frequency::evohomeTxFrequency(frequencyKhz_,moduleBand_) &&
        !(experimentalOneShot &&
          radio_frequency::experimentalOneShotFrequency(frequencyKhz_,moduleBand_,experimentalRx_)))
      return false;
    if (!strobe(0x36)) return false; // SIDLE
    if (!writeRegister(0x08, 0x32)) return false; // async serial (no packet FIFO)
    if (!strobe(0x35)) return false; // STX
    // Wait for TX state before placing the 38.4k UART preamble on GDO0;
    // otherwise the first symbols can be lost during PLL calibration.
    const uint32_t started=millis();
    while((status(0x35)&0x1fu)!=0x13u){
      if(millis()-started>50){ strobe(0x36); enterReceive(); return false; }
      delayMicroseconds(100);
    }
    return true;
  }
  bool endReadRequestTx() {
    if (!strobe(0x36)) return false; // SIDLE
    return enterReceive();
  }

  bool enterReceive() {
    if (!strobe(0x36)) return false; // SIDLE
    if (!writeRegister(0x02, 0x2E)) return false; // IOCFG0 unused in RX
    if (!writeRegister(0x08, 0x32)) return false; // PKTCTRL0 async RX
    if (!strobe(0x3A)) return false; // SFRX
    return strobe(0x34); // SRX; never STX
  }

  // Positive signal attenuation value resembling HGI80 RSSI (approximately -dBm).
  // Precision and calibration remain to be established with hardware.
  uint8_t rssiMagnitude() {
    const int raw = static_cast<int8_t>(status(0x34));
    const int dbm = raw >= 0 ? raw / 2 - 74 : (raw + 256) / 2 - 74;
    const int magnitude = -dbm;
    return static_cast<uint8_t>(constrain(magnitude, 0, 255));
  }

 private:
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
    digitalWrite(CS, HIGH); delayMicroseconds(5);
    digitalWrite(CS, LOW); delayMicroseconds(10);
    digitalWrite(CS, HIGH); delayMicroseconds(45);
    if (!select()) return false;
    SPI.transfer(0x30); // SRES. No RF TX instruction anywhere.
    release();
    delay(2);
    return true;
  }
};
