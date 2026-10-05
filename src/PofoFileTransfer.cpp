#include "PofoFileTransfer.h"

#include <stdlib.h>
#include <string.h>

#include <Arduino.h>

#include "PofoSmartCable.h"

namespace {

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
  if (timestamp == 0) {
    timestamp = time(0);
  }
  struct tm* calendar = localtime(&timestamp);
  if (calendar != 0 && calendar->tm_year >= 80 && calendar->tm_year <= 207) {
    writeDosTime(init, calendar->tm_year + 1900, calendar->tm_mon + 1,
                 calendar->tm_mday, calendar->tm_hour, calendar->tm_min,
                 calendar->tm_sec);
    return;
  }
  writeFallbackDosTime(init);
}

}  // namespace

PofoFileTransferFile::PofoFileTransferFile() : data_(0), length_(0) {
}

PofoFileTransferFile::~PofoFileTransferFile() {
  clear();
}

const uint8_t* PofoFileTransferFile::data() const {
  return data_;
}

size_t PofoFileTransferFile::length() const {
  return length_;
}

void PofoFileTransferFile::clear() {
  free(data_);
  data_ = 0;
  length_ = 0;
}

void PofoFileTransferFile::take(uint8_t* data, size_t length) {
  clear();
  data_ = data;
  length_ = length;
}

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

PofoResult PofoFileTransfer::receiveFile(const char* path,
                                         PofoFileTransferFile* response) {
  if (response == 0) {
    return PofoResult::INVALID_ARGUMENT;
  }
  response->clear();

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

  uint8_t* file = 0;
  if (fileLength != 0) {
    file = static_cast<uint8_t*>(malloc(fileLength));
    if (file == 0) {
      return PofoResult::OUT_OF_MEMORY;
    }
  }

  size_t offset = 0;
  while (offset < fileLength) {
    uint8_t* block = 0;
    size_t blockLength = 0;
    result = cable_.receiveBlock(&block, &blockLength);
    if (result != PofoResult::OK) {
      free(file);
      return result;
    }
    if (blockLength == 0 || blockLength > fileLength - offset) {
      PofoSmartCable::releaseBlock(block);
      free(file);
      return PofoResult::FRAME_ERROR;
    }
    memcpy(file + offset, block, blockLength);
    offset += blockLength;
    PofoSmartCable::releaseBlock(block);
  }

  const uint8_t finish[] = {0x20, 0x00, 0x03};
  result = cable_.sendBlock(finish, sizeof(finish));
  if (result != PofoResult::OK) {
    free(file);
    return result;
  }

  response->take(file, fileLength);
  return PofoResult::OK;
}

PofoResult PofoFileTransfer::transmitFile(const char* path,
                                          const uint8_t* data,
                                          size_t length, bool overwrite,
                                          time_t timestamp) {
  if (path == 0 || (data == 0 && length != 0) || length > 0xffffffUL) {
    return PofoResult::INVALID_ARGUMENT;
  }

  const size_t pathLength = strlen(path);
  if (pathLength > 78) {
    return PofoResult::INVALID_ARGUMENT;
  }

  // Exact 90-byte transmit init block from PortfolioESPlink. Unlike LIST,
  // this request has a fixed 79-byte path field at offset 11.
  uint8_t init[90] = {0x03, 0x00, 0x70, 0x0c, 0x7a, 0x21, 0x32};
  writeTimestamp(init, timestamp);
  init[7] = static_cast<uint8_t>(length);
  init[8] = static_cast<uint8_t>(length >> 8);
  init[9] = static_cast<uint8_t>(length >> 16);
  memcpy(init + 11, path, pathLength);

  PofoResult result = cable_.sendBlock(init, sizeof(init));
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

  size_t offset = 0;
  while (offset < length) {
    size_t chunkLength = length - offset;
    if (chunkLength > blockSize) {
      chunkLength = blockSize;
    }
    result = cable_.sendBlock(data + offset, chunkLength);
    if (result != PofoResult::OK) {
      return result;
    }
    offset += chunkLength;
  }

  control = 0;
  controlLength = 0;
  result = cable_.receiveBlock(&control, &controlLength);
  if (result != PofoResult::OK) {
    return result;
  }
  const bool complete = controlLength != 0 && control[0] == 0x20;
  PofoSmartCable::releaseBlock(control);
  return complete ? PofoResult::OK : PofoResult::REMOTE_ERROR;
}
