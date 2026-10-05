#include "PofoSmartCablePhyESP32.h"

#include <driver/gpio.h>
#include <esp_timer.h>

PofoSmartCablePhyESP32::PofoSmartCablePhyESP32()
    : clkIn_(-1), dataIn_(-1), clkOut_(-1), dataOut_(-1) {}

bool PofoSmartCablePhyESP32::configurePins(int clkIn, int dataIn, int clkOut,
                                            int dataOut) {
  if (clkIn < 0 || dataIn < 0 || clkOut < 0 || dataOut < 0) {
    return false;
  }
  if (clkIn_ >= 0) {
    detachInterrupt(digitalPinToInterrupt(clkIn_));
  }

  clkIn_ = clkIn;
  dataIn_ = dataIn;
  clkOut_ = clkOut;
  dataOut_ = dataOut;

  pinMode(dataIn_, INPUT_PULLUP);
  pinMode(clkIn_, INPUT_PULLUP);
  pinMode(dataOut_, OUTPUT);
  pinMode(clkOut_, OUTPUT);
  digitalWrite(dataOut_, LOW);
  digitalWrite(clkOut_, LOW);
  attachInterruptArg(digitalPinToInterrupt(clkIn_), onClkInChange, this,
                     CHANGE);
  return true;
}

uint32_t PofoSmartCablePhyESP32::nowMicros() const {
  return static_cast<uint32_t>(esp_timer_get_time());
}

bool PofoSmartCablePhyESP32::readClock() const {
  return gpio_get_level(static_cast<gpio_num_t>(clkIn_)) != 0;
}

bool PofoSmartCablePhyESP32::readData() const {
  return gpio_get_level(static_cast<gpio_num_t>(dataIn_)) != 0;
}

void PofoSmartCablePhyESP32::writeClock(bool high) {
  gpio_set_level(static_cast<gpio_num_t>(clkOut_), high ? 1 : 0);
}

void PofoSmartCablePhyESP32::writeData(bool high) {
  gpio_set_level(static_cast<gpio_num_t>(dataOut_), high ? 1 : 0);
}

void PofoSmartCablePhyESP32::idle() {
  yield();
}

void IRAM_ATTR PofoSmartCablePhyESP32::onClkInChange(void* argument) {
  PofoSmartCablePhyESP32* phy =
      static_cast<PofoSmartCablePhyESP32*>(argument);
  phy->clockChanged();
}
