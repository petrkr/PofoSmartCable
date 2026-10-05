#pragma once

#include "PofoSmartCablePhyESP32.h"
#include "PofoSmartCableResult.h"
#include "PofoSmartCableFrame.h"

class PofoSmartCable {
 public:
  bool begin(int clkIn, int dataIn, int clkOut, int dataOut);

  // True if CLKIN changed within the configured link timeout.
  bool online() const;

  // Call regularly (e.g. from the sketch's loop()) to have a registered
  // link-state callback fire on transitions, independent of whether the
  // application also calls online() itself.
  void loop();

  // Registers a callback invoked when online() transitions between true
  // and false. Pass 0 to clear it. Returns ALREADY_REGISTERED if a
  // non-zero callback is already set and callback is also non-zero -
  // clear it first.
  PofoResult setLinkStateCallback(PofoSmartCableLinkStateCallback callback);

  // Registers a callback invoked after each byte sent/received inside
  // sendBlock()/receiveBlock(). Pass 0 to clear it. Returns
  // ALREADY_REGISTERED if a non-zero callback is already set and progress
  // is non-zero - clear it first.
  PofoResult setProgressCallback(PofoSmartCableProgress progress);

  PofoResult sendBlock(const uint8_t* data, size_t length);
  PofoResult receiveBlock(uint8_t** payload, size_t* length);
  static void releaseBlock(uint8_t* payload);

  void reset();

 private:
  PofoSmartCablePhyESP32 phy_;
  PofoSmartCableFrame frame_;
};
