#include <PofoSmartCable.h>
#include <PofoFileTransfer.h>
#include <log4mcu.h>

const int kClkIn = 8;
const int kDataIn = 6;
const int kClkOut = 5;
const int kDataOut = 7;

const char* const kListRequestLabels[] = {
    "LIST *.*",
    "LIST C:\\*.*",
};
const char* const kListPaths[] = {
    "*.*",
    "C:\\*.*",
};

PofoSmartCable cable;
PofoFileTransfer fileTransfer(cable);
log4mcu::SerialLogAppender serialAppender(Serial);
uint8_t requestIndex = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  log4mcu::Logger::setAppender(&serialAppender);
  log4mcu::Logger::setGlobalMinLevel(log4mcu::LogLevel::Debug);

  log4mcu::Logger& logger = log4mcu::Logger::get("List");
  if (!cable.begin(kClkIn, kDataIn, kClkOut, kDataOut)) {
    logger.error("PofoSmartCable begin failed");
    return;
  }
  logger.info("Waiting to send LIST requests");
}

void loop() {
  if (requestIndex >= 2 || !cable.online()) {
    delay(10);
    return;
  }

  log4mcu::Logger& logger = log4mcu::Logger::get("List");
  logger.info(kListRequestLabels[requestIndex]);
  PofoFileTransferList response;
  const PofoResult result =
      fileTransfer.list(kListPaths[requestIndex], &response);
  if (result != PofoResult::OK) {
    logger.warnf("LIST failed: %u", static_cast<unsigned>(result));
    delay(10);
    return;
  }

  logger.infof("LIST response: %u items",
               static_cast<unsigned>(response.count()));

  for (size_t index = 0; index < response.count(); ++index) {
    logger.infof("%s", response.name(index));
  }

  ++requestIndex;
}
