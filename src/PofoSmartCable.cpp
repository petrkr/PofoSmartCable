#include "PofoSmartCable.h"

#include <stdlib.h>

bool PofoSmartCable::begin(int clkIn, int dataIn, int clkOut, int dataOut) {
  return phy_.begin(clkIn, dataIn, clkOut, dataOut);
}

bool PofoSmartCable::online() const {
  return phy_.online();
}

void PofoSmartCable::loop() {
  phy_.online();
}

PofoResult PofoSmartCable::setLinkStateCallback(
    PofoSmartCableLinkStateCallback callback) {
  return phy_.setLinkStateCallback(callback);
}

void PofoSmartCable::reset() {
  phy_.reset();
}

PofoResult PofoSmartCable::setProgressCallback(PofoSmartCableProgress progress) {
  return frame_.setProgressCallback(progress);
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
