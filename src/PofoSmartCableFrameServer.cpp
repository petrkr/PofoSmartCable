#include "PofoSmartCableFrameServer.h"

#include <stdio.h>
#include <stdlib.h>

#include "PofoSmartCableLog.h"
#include "PofoSmartCablePhy.h"

void logHexDump(const char* direction, const uint8_t* data, size_t length) {
  PofoSmartCableComponentLogger& logger =
      pofoSmartCableLogger("PofoSmartCableFrameServer");

  char message[256];
  size_t messageOffset = static_cast<size_t>(
      snprintf(message, sizeof(message), "%s block: %lu bytes", direction,
               static_cast<unsigned long>(length)));

  for (size_t offset = 0; offset < length && messageOffset < sizeof(message);
       offset += 16) {
    // xxd groups bytes in pairs ("XXXX XXXX ..."), no space within a pair.
    char hex[2 * 16 + 8 + 1];
    char ascii[16 + 1];
    const size_t rowLength = length - offset < 16 ? length - offset : 16;
    size_t hexOffset = 0;
    for (size_t index = 0; index < 16; ++index) {
      if (index < rowLength) {
        const uint8_t value = data[offset + index];
        hexOffset += static_cast<size_t>(
            snprintf(hex + hexOffset, 3, "%02x", value));
        ascii[index] = (value >= 0x20 && value < 0x7f)
            ? static_cast<char>(value)
            : '.';
      } else {
        hexOffset += static_cast<size_t>(snprintf(hex + hexOffset, 3, "  "));
      }
      if (index % 2 == 1) {
        hex[hexOffset++] = ' ';
      }
    }
    hex[hexOffset] = 0;
    ascii[rowLength] = 0;
    messageOffset += static_cast<size_t>(
        snprintf(message + messageOffset, sizeof(message) - messageOffset,
                 "\r\n%08lx: %s %s", static_cast<unsigned long>(offset), hex,
                 ascii));
  }

  logger.debugf("%s", message);
}

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
    pofoSmartCableLogger("PofoSmartCableFrameServer").warnf(
        "TX wait Z failed: %u", static_cast<unsigned>(result));
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
    pofoSmartCableLogger("PofoSmartCableFrameServer").warnf(
        "TX checksum ACK failed: %u", static_cast<unsigned>(result));
    return result;
  }
  if (acknowledgement != checksum) {
    pofoSmartCableLogger("PofoSmartCableFrameServer").warnf(
        "TX checksum ACK mismatch: got 0x%02X expected 0x%02X",
        acknowledgement, checksum);
    return PofoResult::CHECKSUM_ERROR;
  }

  logHexDump("TX", data, length);
  return PofoResult::OK;
}

PofoResult PofoSmartCableFrameServer::receiveBlock(PofoSmartCablePhy& phy,
                                                    uint8_t** payload,
                                                    size_t* lengthOut) {
  if (payload == 0 || lengthOut == 0) {
    return PofoResult::FRAME_ERROR;
  }
  *payload = 0;
  *lengthOut = 0;

  PofoResult result = phy.sendByte(0x5A); // 'Z'
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
  uint8_t* data = 0;
  if (length != 0) {
    data = static_cast<uint8_t*>(malloc(length));
    if (data == 0) {
      return PofoResult::OUT_OF_MEMORY;
    }
  }

  uint8_t checksum = static_cast<uint8_t>(lengthLow + lengthHigh);
  for (size_t index = 0; index < length; ++index) {
    result = phy.receiveByte(&data[index]);
    if (result != PofoResult::OK) {
      free(data);
      return result;
    }
    checksum = static_cast<uint8_t>(checksum + data[index]);
  }

  uint8_t receivedChecksum = 0;
  result = phy.receiveByte(&receivedChecksum);
  if (result != PofoResult::OK) {
    free(data);
    return result;
  }
  if (static_cast<uint8_t>(checksum + receivedChecksum) != 0) {
    free(data);
    return PofoResult::CHECKSUM_ERROR;
  }

  result = phy.sendByte(static_cast<uint8_t>(0 - checksum));
  if (result != PofoResult::OK) {
    free(data);
    return result;
  }

  *payload = data;
  *lengthOut = length;

  logHexDump("RX", data, length);
  return PofoResult::OK;
}
