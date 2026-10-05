#include "PofoSmartCable.h"

bool PofoSmartCable::begin(int clkIn, int dataIn, int clkOut, int dataOut) {
  return phy_.begin(clkIn, dataIn, clkOut, dataOut);
}

bool PofoSmartCable::online() const {
  return phy_.online();
}

PofoResult PofoSmartCable::receiveByte(uint8_t* value) {
  return phy_.receiveByte(value);
}

void PofoSmartCable::reset() {
  phy_.reset();
}

PofoResult PofoSmartCable::syncTick() {
  return phy_.syncTick();
}

PofoResult PofoSmartCable::waitZ() {
  for (;;) {
    uint8_t value = 0;
    PofoResult result = receiveByte(&value);
    if (result != PofoResult::OK) {
      return result;
    }
    if (value == 0x5A) {
      return PofoResult::OK;
    }
    result = syncTick();
    if (result != PofoResult::OK) {
      return result;
    }
  }
}
