#include <PofoSmartCable.h>
#include <PofoFileTransfer.h>
#include <log4mcu.h>

// Reads back a fixed in-memory buffer as a Stream for transmitFile().
class MemoryStream : public Stream {
 public:
  MemoryStream(const uint8_t* data, size_t length)
      : data_(data), length_(length), offset_(0) {}

  int available() override {
    return static_cast<int>(length_ - offset_);
  }

  int read() override {
    return offset_ < length_ ? data_[offset_++] : -1;
  }

  int peek() override {
    return offset_ < length_ ? data_[offset_] : -1;
  }

  size_t write(uint8_t) override {
    return 0;
  }

 private:
  const uint8_t* data_;
  size_t length_;
  size_t offset_;
};

const int kClkIn = 8;
const int kDataIn = 6;
const int kClkOut = 5;
const int kDataOut = 7;
const char kPath[] = "C:\\EXAMPLE.TXT";
const char kPayload[] = "this is an example file from ESP32\r\n";
const size_t kPayloadLength = sizeof(kPayload) - 1;

PofoSmartCable cable;
PofoFileTransfer fileTransfer(cable);
log4mcu::Logger& logger = log4mcu::Logger::get("Transmit");
log4mcu::SerialLogAppender serialAppender(Serial);
bool transmitted = false;

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
  logger.info("Waiting to write C:\\EXAMPLE.TXT");
}

void loop() {
  if (transmitted || !cable.online()) {
    delay(10);
    return;
  }

  MemoryStream input(reinterpret_cast<const uint8_t*>(kPayload),
                     kPayloadLength);
  const PofoResult result = fileTransfer.transmitFile(
      kPath, input, kPayloadLength, true);
  if (result != PofoResult::OK) {
    logger.warnf("TransmitFile failed: %u", static_cast<unsigned>(result));
    delay(10);
    return;
  }

  logger.infof("C:\\EXAMPLE.TXT written: %u bytes",
               static_cast<unsigned>(kPayloadLength));
  transmitted = true;
}
