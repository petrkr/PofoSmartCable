#include <PofoSmartCable.h>
#include <log4mcu.h>

const int kClkIn = 8;
const int kDataIn = 6;
const int kClkOut = 5;
const int kDataOut = 7;

PofoSmartCable cable;
log4mcu::Logger& logger = log4mcu::Logger::get("HelloServer");
log4mcu::SerialLogAppender serialAppender(Serial);

const uint8_t kHelloRequest = 0x01;
const uint8_t kHelloResponse[] = {
    0x20,                    // status: success
    0x00,                    // error
    'P', 'F', 'C', '1',      // magic
    0x00, 0x00, 0xFF, 0xFF, // build_id: 0xFFFF0000, little-endian
    0x00, 0x00, 0x00, 0x00, // version 0.0.0, reserved
};
const uint8_t kDashboardRequest[] = {0x08, 0x00};
const uint8_t kDashboardResponse[] = {
    0x20, 0x00,                         // status, error
    0x00, 0x01, 0x01, 14,               // id, row, column, text length
    'H', 'e', 'l', 'l', 'o', ' ', 'i',  // "Hello it works"
    't', ' ', 'w', 'o', 'r', 'k', 's',
};

void printHex(const uint8_t* data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    Serial.printf("%02X%s", data[index],
                  index + 1 == length ? "\r\n" : " ");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  log4mcu::Logger::setAppender(&serialAppender);
  log4mcu::Logger::setGlobalMinLevel(log4mcu::LogLevel::Debug);

  if (!cable.begin(kClkIn, kDataIn, kClkOut, kDataOut)) {
    logger.error("PofoSmartCable begin failed");
    return;
  }
  logger.info("Waiting for Atari request");
}

void loop() {
  if (!cable.online()) {
    delay(10);
    return;
  }

  // receiveBlock() repeatedly sends Z until it receives A5, then receives
  // LEN/payload/checksum.
  logger.info("Sending Z; waiting for request");
  uint8_t* payload = 0;
  size_t length = 0;
  const PofoResult result = cable.receiveBlock(&payload, &length);
  if (result != PofoResult::OK) {
    logger.warnf("receiveBlock failed: %u", static_cast<unsigned>(result));
    // Re-arm: the next Z waits for fresh CLKIN edges from the Atari.
    cable.reset();
    return;
  }

  logger.infof("Received %u bytes:", static_cast<unsigned>(length));
  printHex(payload, length);

  if (length == 1 && payload[0] == kHelloRequest) {
    logger.info("Sending HELLO response");
    const PofoResult sendResult =
        cable.sendBlock(kHelloResponse, sizeof(kHelloResponse));
    if (sendResult != PofoResult::OK) {
      logger.warnf("sendBlock failed: %u",
                   static_cast<unsigned>(sendResult));
    }
  } else if (length == sizeof(kDashboardRequest) &&
             payload[0] == kDashboardRequest[0] &&
             payload[1] == kDashboardRequest[1]) {
    logger.info("Sending dashboard");
    const PofoResult sendResult =
        cable.sendBlock(kDashboardResponse, sizeof(kDashboardResponse));
    if (sendResult != PofoResult::OK) {
      logger.warnf("sendBlock failed: %u",
                   static_cast<unsigned>(sendResult));
    }
  }

  PofoSmartCable::releaseBlock(payload);
  cable.reset();
}
