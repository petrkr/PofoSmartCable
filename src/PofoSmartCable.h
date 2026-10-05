#pragma once

#include "PofoSmartCablePhyESP32.h"
#include "PofoSmartCableResult.h"
#include "PofoSmartCableFrame.h"

class PofoSmartCable {
 public:
  bool begin(int clkIn, int dataIn, int clkOut, int dataOut);

  // True if CLKIN changed within the configured link timeout.
  bool online() const;

  // Temporary byte-level diagnostic API. Block framing will be the final API.
  PofoResult receiveByte(uint8_t* value);
  PofoResult sendByte(uint8_t value);

  // Temporary synchronization diagnostic API. sendBlock() will use it later.
  PofoResult waitZ();

  PofoResult sendBlock(const uint8_t* data, size_t length);
  PofoResult receiveBlock(uint8_t** payload, size_t* length);
  static void releaseBlock(uint8_t* payload);

  void reset();

 private:
  PofoResult syncTick();
  PofoSmartCablePhyESP32 phy_;
  PofoSmartCableFrame frame_;
};
