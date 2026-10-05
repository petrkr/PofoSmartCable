#include "PofoFileTransfer.h"

#include <stdlib.h>
#include <string.h>

#include "PofoSmartCable.h"

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
    return PofoResult::FRAME_ERROR;
  }

  const size_t pathLength = strlen(path);
  if (pathLength > 0xffffU - 4U) {
    return PofoResult::FRAME_ERROR;
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
    return PofoResult::FRAME_ERROR;
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
    return PofoResult::FRAME_ERROR;
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
