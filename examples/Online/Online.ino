#include <PofoSmartCable.h>
#include <log4mcu.h>

// Change these to the four GPIOs connected to the Smart Cable.
const int kClkIn = 8;
const int kDataIn = 6;
const int kClkOut = 5;
const int kDataOut = 7;

log4mcu::Logger& logger = log4mcu::Logger::get("Online");
log4mcu::SerialLogAppender serialAppender(Serial);
PofoSmartCable cable;

void onLinkStateChanged(bool isOnline) {
  logger.infof("Link %s", isOnline ? "online" : "offline");
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
  cable.setLinkStateCallback(onLinkStateChanged);

  logger.info("Waiting for Smart Cable Z synchronization");
}

void loop() {
  cable.loop();
  delay(10);
}
