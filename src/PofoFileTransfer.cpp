#include "PofoFileTransfer.h"

#include <stdlib.h>
#include <string.h>

#include <Arduino.h>

#include "PofoSmartCable.h"
#include "PofoSmartCableLog.h"

namespace {

const uint32_t kTransmitFinishDelayUs = 50000;

// Adapts PofoSmartCable's per-block byte progress to PofoFileTransfer's
// per-file progress. Set by receiveFile()/transmitFile() before each
// cable_.sendBlock()/receiveBlock() call; read by onBlockProgress(), which
// cable_.setProgressCallback() invokes from inside that call. A free
// function (not a method) because PofoSmartCableProgress is a plain
// function pointer - static state is fine since only one PofoFileTransfer
// transfer runs at a time.
PofoFileTransferProgress gUserProgress = 0;
size_t gOffsetBeforeBlock = 0;
size_t gFileLength = 0;

void onBlockProgress(size_t blockTransferred, size_t) {
  gUserProgress(gOffsetBeforeBlock + blockTransferred, gFileLength);
}

bool isLeapYear(unsigned year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

unsigned buildMonth() {
  const char* date = __DATE__;
  if (date[0] == 'J') {
    return date[1] == 'a' ? 1 : (date[2] == 'n' ? 6 : 7);
  }
  if (date[0] == 'F') return 2;
  if (date[0] == 'M') return date[2] == 'r' ? 3 : 5;
  if (date[0] == 'A') return date[1] == 'p' ? 4 : 8;
  if (date[0] == 'S') return 9;
  if (date[0] == 'O') return 10;
  if (date[0] == 'N') return 11;
  return 12;
}

unsigned buildDay() {
  const char* date = __DATE__;
  return (date[4] == ' ' ? 0 : (date[4] - '0') * 10) + date[5] - '0';
}

unsigned buildYear() {
  const char* date = __DATE__;
  return (date[7] - '0') * 1000 + (date[8] - '0') * 100 +
      (date[9] - '0') * 10 + date[10] - '0';
}

void writeDosTime(uint8_t* init, unsigned year, unsigned month, unsigned day,
                  unsigned hour, unsigned minute, unsigned second) {
  const uint16_t dosTime = static_cast<uint16_t>(
      (hour << 11) | (minute << 5) | (second / 2));
  const uint16_t dosDate = static_cast<uint16_t>(
      ((year - 1980) << 9) | (month << 5) | day);
  init[3] = static_cast<uint8_t>(dosTime);
  init[4] = static_cast<uint8_t>(dosTime >> 8);
  init[5] = static_cast<uint8_t>(dosDate);
  init[6] = static_cast<uint8_t>(dosDate >> 8);
}

void logTimestamp(const char* source, const uint8_t* init) {
  const uint16_t dosTime = static_cast<uint16_t>(init[3]) |
      (static_cast<uint16_t>(init[4]) << 8);
  const uint16_t dosDate = static_cast<uint16_t>(init[5]) |
      (static_cast<uint16_t>(init[6]) << 8);
  pofoSmartCableLogger("PofoFileTransfer").debugf(
      "Timestamp %s: %04u-%02u-%02u %02u:%02u:%02u", source,
      static_cast<unsigned>(1980 + (dosDate >> 9)),
      static_cast<unsigned>((dosDate >> 5) & 0x0f),
      static_cast<unsigned>(dosDate & 0x1f),
      static_cast<unsigned>(dosTime >> 11),
      static_cast<unsigned>((dosTime >> 5) & 0x3f),
      static_cast<unsigned>((dosTime & 0x1f) * 2));
}

void writeFallbackDosTime(uint8_t* init) {
  uint32_t seconds = millis() / 1000UL;
  const unsigned monthDays[] = {31, 28, 31, 30, 31, 30,
                                31, 31, 30, 31, 30, 31};
  unsigned year = buildYear();
  unsigned month = buildMonth();
  unsigned day = buildDay();
  while (seconds >= 86400UL) {
    seconds -= 86400UL;
    unsigned days = monthDays[month - 1];
    if (month == 2 && isLeapYear(year)) {
      ++days;
    }
    ++day;
    if (day > days) {
      day = 1;
      ++month;
      if (month > 12) {
        month = 1;
        ++year;
      }
    }
  }

  writeDosTime(init, year, month, day, seconds / 3600UL,
               (seconds / 60UL) % 60UL, seconds % 60UL);
}

void writeTimestamp(uint8_t* init, time_t timestamp) {
  const bool supplied = timestamp != 0;
  if (timestamp == 0) {
    timestamp = time(0);
  }
  struct tm* calendar = localtime(&timestamp);
  if (calendar != 0 && calendar->tm_year >= 80 && calendar->tm_year <= 207) {
    writeDosTime(init, calendar->tm_year + 1900, calendar->tm_mon + 1,
                 calendar->tm_mday, calendar->tm_hour, calendar->tm_min,
                 calendar->tm_sec);
    logTimestamp(supplied ? "argument" : "system", init);
    return;
  }
  writeFallbackDosTime(init);
  logTimestamp("build date + millis", init);
}

}  // namespace

PofoFileTransferList::PofoFileTransferList()
    : payload_(0), names_(0), count_(0) {
}

PofoFileTransferList::~PofoFileTransferList() {
  clear();
}

size_t PofoFileTransferList::count() const {
  return count_;
}

const char* PofoFileTransferList::name(size_t index) const {
  return index < count_ ? names_[index] : 0;
}

void PofoFileTransferList::clear() {
  free(names_);
  free(payload_);
  names_ = 0;
  payload_ = 0;
  count_ = 0;
}

PofoResult PofoFileTransferList::take(uint8_t* payload, size_t length) {
  clear();
  if (payload == 0 || length < 2) {
    free(payload);
    return PofoResult::FRAME_ERROR;
  }

  const size_t count = static_cast<size_t>(payload[0]) |
      (static_cast<size_t>(payload[1]) << 8);
  const char** names = 0;
  if (count != 0) {
    if (count > static_cast<size_t>(-1) / sizeof(*names)) {
      free(payload);
      return PofoResult::OUT_OF_MEMORY;
    }
    names = static_cast<const char**>(malloc(count * sizeof(*names)));
    if (names == 0) {
      free(payload);
      return PofoResult::OUT_OF_MEMORY;
    }
  }

  size_t offset = 2;
  for (size_t index = 0; index < count; ++index) {
    if (offset >= length) {
      free(names);
      free(payload);
      return PofoResult::FRAME_ERROR;
    }
    names[index] = reinterpret_cast<const char*>(payload + offset);
    while (offset < length && payload[offset] != 0) {
      ++offset;
    }
    if (offset == length) {
      free(names);
      free(payload);
      return PofoResult::FRAME_ERROR;
    }
    ++offset;
  }

  payload_ = payload;
  names_ = names;
  count_ = count;
  return PofoResult::OK;
}

PofoFileTransfer::PofoFileTransfer(PofoSmartCable& cable) : cable_(cable) {
}

void PofoFileTransfer::setProgressCallback(PofoFileTransferProgress progress) {
  progress_ = progress;
}

PofoResult PofoFileTransfer::sendPathRequest(uint8_t function,
                                             const char* path) {
  if (path == 0) {
    return PofoResult::INVALID_ARGUMENT;
  }

  const size_t pathLength = strlen(path);
  if (pathLength > 0xffffU - 4U) {
    return PofoResult::INVALID_ARGUMENT;
  }
  const size_t requestLength = 4 + pathLength;

  uint8_t* request = static_cast<uint8_t*>(malloc(requestLength));
  if (request == 0) {
    return PofoResult::OUT_OF_MEMORY;
  }
  request[0] = function;
  request[1] = 0x00;
  request[2] = 0x70;
  memcpy(request + 3, path, pathLength);
  request[requestLength - 1] = 0;

  PofoResult result = cable_.sendBlock(request, requestLength);
  free(request);
  return result;
}

PofoResult PofoFileTransfer::list(const char* path,
                                  PofoFileTransferList* response) {
  if (response == 0) {
    return PofoResult::INVALID_ARGUMENT;
  }
  response->clear();

  PofoResult result = sendPathRequest(0x06, path);
  if (result != PofoResult::OK) {
    return result;
  }

  uint8_t* payload = 0;
  size_t length = 0;
  result = cable_.receiveBlock(&payload, &length);
  if (result != PofoResult::OK) {
    return result;
  }
  return response->take(payload, length);
}

PofoResult PofoFileTransfer::receiveFile(const char* path, Stream& output) {
  PofoResult result = sendPathRequest(0x02, path);
  if (result != PofoResult::OK) {
    return result;
  }

  uint8_t* control = 0;
  size_t controlLength = 0;
  result = cable_.receiveBlock(&control, &controlLength);
  if (result != PofoResult::OK) {
    return result;
  }
  if (controlLength < 10 || control[0] != 0x20) {
    PofoSmartCable::releaseBlock(control);
    return PofoResult::FRAME_ERROR;
  }

  const size_t fileLength = static_cast<size_t>(control[7]) |
      (static_cast<size_t>(control[8]) << 8) |
      (static_cast<size_t>(control[9]) << 16);
  PofoSmartCable::releaseBlock(control);

  if (progress_ != 0) {
    result = cable_.setProgressCallback(onBlockProgress);
    if (result != PofoResult::OK) {
      return result;
    }
    gUserProgress = progress_;
    gFileLength = fileLength;
  }

  size_t offset = 0;
  while (offset < fileLength) {
    uint8_t* block = 0;
    size_t blockLength = 0;
    gOffsetBeforeBlock = offset;
    result = cable_.receiveBlock(&block, &blockLength);
    if (result != PofoResult::OK) {
      break;
    }
    if (blockLength == 0 || blockLength > fileLength - offset) {
      PofoSmartCable::releaseBlock(block);
      result = PofoResult::FRAME_ERROR;
      break;
    }
    output.write(block, blockLength);
    offset += blockLength;
    PofoSmartCable::releaseBlock(block);
  }

  if (progress_ != 0) {
    cable_.setProgressCallback(0);
  }
  if (result != PofoResult::OK) {
    return result;
  }

  const uint8_t finish[] = {0x20, 0x00, 0x03};
  return cable_.sendBlock(finish, sizeof(finish));
}

PofoResult PofoFileTransfer::transmitFile(const char* path, Stream& input,
                                          size_t length, bool overwrite,
                                          time_t timestamp) {
  if (path == 0 || length > 0xffffffUL) {
    return PofoResult::INVALID_ARGUMENT;
  }

  const size_t pathLength = strlen(path);
  if (pathLength > 78) {
    return PofoResult::INVALID_ARGUMENT;
  }

  const size_t initLength = 12 + pathLength;
  uint8_t* init = static_cast<uint8_t*>(malloc(initLength));
  if (init == 0) {
    return PofoResult::OUT_OF_MEMORY;
  }
  memset(init, 0, initLength);
  init[0] = 0x03;
  init[1] = 0x00;
  init[2] = 0x70;
  writeTimestamp(init, timestamp);
  init[7] = static_cast<uint8_t>(length);
  init[8] = static_cast<uint8_t>(length >> 8);
  init[9] = static_cast<uint8_t>(length >> 16);
  memcpy(init + 11, path, pathLength);

  PofoResult result = cable_.sendBlock(init, initLength);
  free(init);
  if (result != PofoResult::OK) {
    return result;
  }

  uint8_t* control = 0;
  size_t controlLength = 0;
  result = cable_.receiveBlock(&control, &controlLength);
  if (result != PofoResult::OK) {
    return result;
  }
  if (controlLength < 3) {
    PofoSmartCable::releaseBlock(control);
    return PofoResult::FRAME_ERROR;
  }

  const uint8_t status = control[0];
  const size_t blockSize = static_cast<size_t>(control[1]) |
      (static_cast<size_t>(control[2]) << 8);
  PofoSmartCable::releaseBlock(control);

  if (status == 0x10) {
    return PofoResult::REMOTE_ERROR;
  }
  if (status == 0x20) {
    const uint8_t overwriteRequest[] = {0x05, 0x00, 0x70};
    const uint8_t cancelRequest[] = {0x00, 0x00, 0x00};
    const uint8_t* decision = overwrite ? overwriteRequest : cancelRequest;
    result = cable_.sendBlock(decision, sizeof(overwriteRequest));
    if (result != PofoResult::OK) {
      return result;
    }
    if (!overwrite) {
      return PofoResult::FILE_EXISTS;
    }
  }
  if (blockSize == 0) {
    return PofoResult::FRAME_ERROR;
  }

  uint8_t* chunk = static_cast<uint8_t*>(malloc(blockSize));
  if (chunk == 0) {
    return PofoResult::OUT_OF_MEMORY;
  }

  if (progress_ != 0) {
    result = cable_.setProgressCallback(onBlockProgress);
    if (result != PofoResult::OK) {
      free(chunk);
      return result;
    }
    gUserProgress = progress_;
    gFileLength = length;
  }

  size_t offset = 0;
  while (offset < length) {
    size_t chunkLength = length - offset;
    if (chunkLength > blockSize) {
      chunkLength = blockSize;
    }
    if (input.readBytes(chunk, chunkLength) != chunkLength) {
      result = PofoResult::FRAME_ERROR;
      break;
    }
    gOffsetBeforeBlock = offset;
    result = cable_.sendBlock(chunk, chunkLength);
    if (result != PofoResult::OK) {
      pofoSmartCableLogger("PofoFileTransfer").warnf(
          "Transmit payload failed at %lu/%lu: %u",
          static_cast<unsigned long>(offset),
          static_cast<unsigned long>(length), static_cast<unsigned>(result));
      break;
    }
    offset += chunkLength;
  }
  free(chunk);

  if (progress_ != 0) {
    cable_.setProgressCallback(0);
  }
  if (result != PofoResult::OK) {
    return result;
  }

  const uint32_t finalStatusStarted = micros();
  delayMicroseconds(kTransmitFinishDelayUs);

  control = 0;
  controlLength = 0;
  result = cable_.receiveBlock(&control, &controlLength);
  if (result != PofoResult::OK) {
    pofoSmartCableLogger("PofoFileTransfer").warnf(
        "Transmit final status failed: %u", static_cast<unsigned>(result));
    return result;
  }
  const uint8_t finalStatus = controlLength > 0 ? control[0] : 0xff;
  const uint8_t detail = controlLength > 1 ? control[1] : 0xff;
  const uint8_t extra = controlLength > 2 ? control[2] : 0xff;
  const bool complete = finalStatus == 0x20;
  PofoSmartCable::releaseBlock(control);
  pofoSmartCableLogger("PofoFileTransfer").infof(
      "Transmit final status after %lu us: %02X %02X %02X",
      static_cast<unsigned long>(micros() - finalStatusStarted), finalStatus,
      detail, extra);
  return complete ? PofoResult::OK : PofoResult::REMOTE_ERROR;
}
