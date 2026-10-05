#pragma once

#include <stddef.h>
#include <time.h>

#include <Stream.h>

#include "PofoSmartCableResult.h"

class PofoSmartCable;

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

  // Requests one Portfolio file and writes its content to output as each
  // block arrives, without buffering the complete file in RAM.
  // path is required, for example "C:\\TEST.TXT".
  PofoResult receiveFile(const char* path, Stream& output);

  // Sends one file to Portfolio, reading its content from input in
  // control-block-sized chunks. length is the exact byte count input will
  // provide. path must fit the 78-character reference transmit-init path
  // field. Set overwrite when the destination exists. timestamp is Unix
  // time; zero uses the current system time.
  PofoResult transmitFile(const char* path, Stream& input, size_t length,
                          bool overwrite, time_t timestamp = 0);

 private:
  PofoResult sendPathRequest(uint8_t function, const char* path);

  PofoSmartCable& cable_;
};
