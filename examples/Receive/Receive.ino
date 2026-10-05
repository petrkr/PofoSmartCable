#include <PofoSmartCable.h>
#include <PofoFileTransfer.h>
#include <log4mcu.h>

const int kClkIn = 8;
const int kDataIn = 6;
const int kClkOut = 5;
const int kDataOut = 7;
const char kPath[] = "C:\\TEST.TXT";

PofoSmartCable cable;
PofoFileTransfer fileTransfer(cable);
log4mcu::Logger& logger = log4mcu::Logger::get("Receive");
log4mcu::SerialLogAppender serialAppender(Serial);
bool received = false;

// Logs progress on each 1% change - the callback itself fires once per
// byte, but logging every call would flood the serial output.
void onProgress(size_t transferred, size_t total) {
  static uint8_t lastPercent = 0xff;
  const uint8_t percent = static_cast<uint8_t>(transferred * 100UL / total);
  if (percent != lastPercent) {
    logger.infof("Progress: %u%% (%u/%u bytes)", percent,
                 static_cast<unsigned>(transferred),
                 static_cast<unsigned>(total));
    lastPercent = percent;
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
  fileTransfer.setProgressCallback(onProgress);
  logger.info("Waiting to receive C:\\TEST.TXT");
}

void loop() {
  if (received || !cable.online()) {
    delay(10);
    return;
  }

  const PofoResult result = fileTransfer.receiveFile(kPath, Serial);
  if (result != PofoResult::OK) {
    logger.warnf("ReceiveFile failed: %u", static_cast<unsigned>(result));
    delay(10);
    return;
  }

  Serial.println();
  logger.info("Receive complete");
  received = true;
}
