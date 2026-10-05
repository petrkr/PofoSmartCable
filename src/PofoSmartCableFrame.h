#pragma once

#include <stddef.h>
#include <stdint.h>

#include "PofoSmartCableResult.h"

class PofoSmartCablePhy;

// Reports byte-level progress within a single sendBlock()/receiveBlock()
// call. transferred and total are both in bytes; total is the block's
// length.
typedef void (*PofoSmartCableProgress)(size_t transferred, size_t total);

class PofoSmartCableFrame {
 public:
  // Registers a callback invoked after each byte sent/received inside
  // sendBlock()/receiveBlock(). Pass 0 to clear it. Returns
  // ALREADY_REGISTERED if a non-zero callback is already set and progress
  // is non-zero - clear it first.
  PofoResult setProgressCallback(PofoSmartCableProgress progress);

  PofoResult waitZ(PofoSmartCablePhy& phy);
  PofoResult sendBlock(PofoSmartCablePhy& phy, const uint8_t* data,
                       size_t length);
  PofoResult receiveBlock(PofoSmartCablePhy& phy, uint8_t** payload,
                          size_t* length);

 private:
  PofoSmartCableProgress progress_ = 0;
};
