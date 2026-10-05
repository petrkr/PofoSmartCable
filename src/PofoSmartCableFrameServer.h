#pragma once

#include <stddef.h>
#include <stdint.h>

#include "PofoSmartCableResult.h"

class PofoSmartCablePhy;

class PofoSmartCableFrameServer {
 public:
  PofoResult waitZ(PofoSmartCablePhy& phy);
  PofoResult sendBlock(PofoSmartCablePhy& phy, const uint8_t* data,
                       size_t length);
  PofoResult receiveBlock(PofoSmartCablePhy& phy, uint8_t** payload,
                          size_t* length);
};
