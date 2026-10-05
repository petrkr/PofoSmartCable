#include "PofoSmartCable.h"

#include <stdlib.h>

bool PofoSmartCable::begin(int clkIn, int dataIn, int clkOut, int dataOut) {
  return phy_.begin(clkIn, dataIn, clkOut, dataOut);
}

bool PofoSmartCable::online() const {
  return phy_.online();
}

PofoResult PofoSmartCable::receiveByte(uint8_t* value) {
  return phy_.receiveByte(value);
}

PofoResult PofoSmartCable::sendByte(uint8_t value) {
  return phy_.sendByte(value);
}

void PofoSmartCable::reset() {
  phy_.reset();
}

PofoResult PofoSmartCable::syncTick() {
  return phy_.syncTick();
}

PofoResult PofoSmartCable::waitZ() {
  return frame_.waitZ(phy_);
}

PofoResult PofoSmartCable::sendBlock(const uint8_t* data, size_t length) {
  return frame_.sendBlock(phy_, data, length);
}

PofoResult PofoSmartCable::receiveBlock(uint8_t** payload, size_t* length) {
  return frame_.receiveBlock(phy_, payload, length);
}

void PofoSmartCable::releaseBlock(uint8_t* payload) {
  free(payload);
}
