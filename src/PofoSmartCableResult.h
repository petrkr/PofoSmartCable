#pragma once

#include <stdint.h>

enum class PofoResult : uint8_t {
  OK,
  OFFLINE,
  TIMEOUT,
  SYNC_ERROR,
  INVALID_ARGUMENT,
  FRAME_ERROR,
  BUFFER_TOO_SMALL,
  OUT_OF_MEMORY,
  CHECKSUM_ERROR,
  FILE_EXISTS,
  REMOTE_ERROR,
};
