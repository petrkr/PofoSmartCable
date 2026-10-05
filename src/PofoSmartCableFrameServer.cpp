#include "PofoSmartCableFrameServer.h"

#include "PofoSmartCableLog.h"
#include "PofoSmartCablePhy.h"

PofoResult PofoSmartCableFrameServer::waitZ(PofoSmartCablePhy& phy) {
  for (;;) {
    uint8_t value = 0;
    PofoResult result = phy.receiveByte(&value);
    if (result != PofoResult::OK) {
      return result;
    }
    if (value == 0x5A) {
      return PofoResult::OK;
    }
    result = phy.syncTick();
    if (result != PofoResult::OK) {
      return result;
    }
  }
}

PofoResult PofoSmartCableFrameServer::sendBlock(PofoSmartCablePhy& phy,
                                                 const uint8_t* data,
                                                 size_t length) {
  if (data == 0 || length == 0 || length > 0xffffU) {
    return PofoResult::FRAME_ERROR;
  }

  PofoResult result = waitZ(phy);
  if (result != PofoResult::OK) {
    return result;
  }

  phy.delayMicros(50000);
  result = phy.sendByte(0xA5);
  if (result != PofoResult::OK) {
    return result;
  }

  uint8_t checksum = 0;
  const uint8_t lengthLow = static_cast<uint8_t>(length & 0xffU);
  const uint8_t lengthHigh = static_cast<uint8_t>((length >> 8) & 0xffU);
  checksum = static_cast<uint8_t>(checksum - lengthLow);
  result = phy.sendByte(lengthLow);
  if (result != PofoResult::OK) {
    return result;
  }
  checksum = static_cast<uint8_t>(checksum - lengthHigh);
  result = phy.sendByte(lengthHigh);
  if (result != PofoResult::OK) {
    return result;
  }

  for (size_t index = 0; index < length; ++index) {
    checksum = static_cast<uint8_t>(checksum - data[index]);
    result = phy.sendByte(data[index]);
    if (result != PofoResult::OK) {
      return result;
    }
  }

  result = phy.sendByte(checksum);
  if (result != PofoResult::OK) {
    return result;
  }

  uint8_t acknowledgement = 0;
  result = phy.receiveByte(&acknowledgement);
  if (result != PofoResult::OK) {
    return result;
  }
  if (acknowledgement != checksum) {
    return PofoResult::CHECKSUM_ERROR;
  }

  PofoSmartCableComponentLogger& logger =
      pofoSmartCableLogger("PofoSmartCableFrameServer");
  logger.debugf("TX block: %lu bytes", static_cast<unsigned long>(length));
  return PofoResult::OK;
}

PofoResult PofoSmartCableFrameServer::receiveBlock(PofoSmartCablePhy& phy,
                                                    uint8_t* data,
                                                    size_t capacity,
                                                    size_t* receivedLength) {
  if (receivedLength == 0) {
    return PofoResult::FRAME_ERROR;
  }
  *receivedLength = 0;

  PofoResult result = phy.sendByte(0x5A);
  if (result != PofoResult::OK) {
    return result;
  }

  uint8_t value = 0;
  result = phy.receiveByte(&value);
  if (result != PofoResult::OK) {
    return result;
  }
  if (value != 0xA5) {
    return PofoResult::FRAME_ERROR;
  }

  uint8_t lengthLow = 0;
  uint8_t lengthHigh = 0;
  result = phy.receiveByte(&lengthLow);
  if (result != PofoResult::OK) {
    return result;
  }
  result = phy.receiveByte(&lengthHigh);
  if (result != PofoResult::OK) {
    return result;
  }

  const size_t length = static_cast<size_t>(lengthLow) |
      (static_cast<size_t>(lengthHigh) << 8);
  *receivedLength = length;
  if (length > capacity || (length != 0 && data == 0)) {
    return PofoResult::BUFFER_TOO_SMALL;
  }

  uint8_t checksum = static_cast<uint8_t>(lengthLow + lengthHigh);
  for (size_t index = 0; index < length; ++index) {
    result = phy.receiveByte(&data[index]);
    if (result != PofoResult::OK) {
      return result;
    }
    checksum = static_cast<uint8_t>(checksum + data[index]);
  }

  uint8_t receivedChecksum = 0;
  result = phy.receiveByte(&receivedChecksum);
  if (result != PofoResult::OK) {
    return result;
  }
  if (static_cast<uint8_t>(checksum + receivedChecksum) != 0) {
    return PofoResult::CHECKSUM_ERROR;
  }

  result = phy.sendByte(static_cast<uint8_t>(0 - checksum));
  if (result != PofoResult::OK) {
    return result;
  }

  PofoSmartCableComponentLogger& logger =
      pofoSmartCableLogger("PofoSmartCableFrameServer");
  logger.debugf("RX block: %lu bytes", static_cast<unsigned long>(length));
  return PofoResult::OK;
}
