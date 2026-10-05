#pragma once

#include <stddef.h>
#include <time.h>

#include "PofoSmartCableResult.h"

class PofoSmartCable;

// Received Portfolio file. The object owns the complete file content.
class PofoFileTransferFile {
 public:
  PofoFileTransferFile();
  ~PofoFileTransferFile();

  const uint8_t* data() const;
  size_t length() const;
  void clear();

 private:
  friend class PofoFileTransfer;
  void take(uint8_t* data, size_t length);

  PofoFileTransferFile(const PofoFileTransferFile&);
  PofoFileTransferFile& operator=(const PofoFileTransferFile&);

  uint8_t* data_;
  size_t length_;
};

// Parsed response to the Portfolio server-side LIST request.
class PofoFileTransferList {
 public:
  PofoFileTransferList();
  ~PofoFileTransferList();

  size_t count() const;
  const char* name(size_t index) const;
  void clear();

 private:
  friend class PofoFileTransfer;
  PofoResult take(uint8_t* payload, size_t length);

  PofoFileTransferList(const PofoFileTransferList&);
  PofoFileTransferList& operator=(const PofoFileTransferList&);

  uint8_t* payload_;
  const char** names_;
  size_t count_;
};

// Server-side Atari Portfolio File Transfer requests over Smart Cable framing.
class PofoFileTransfer {
 public:
  explicit PofoFileTransfer(PofoSmartCable& cable);

  // path is required, for example "*.*" or "C:\\*.*".
  PofoResult list(const char* path, PofoFileTransferList* response);

  // Requests one Portfolio file and receives its complete content.
  // path is required, for example "C:\\TEST.TXT".
  PofoResult receiveFile(const char* path, PofoFileTransferFile* response);

  // Sends one file to Portfolio. path must fit the 78-character reference
  // transmit-init path field. Set overwrite when the destination exists.
  // timestamp is Unix time; zero uses the current system time.
  PofoResult transmitFile(const char* path, const uint8_t* data,
                          size_t length, bool overwrite,
                          time_t timestamp = 0);

 private:
  PofoResult sendPathRequest(uint8_t function, const char* path);

  PofoSmartCable& cable_;
};
