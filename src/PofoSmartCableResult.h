#pragma once

#include <stdint.h>

enum class PofoResult : uint8_t {
  OK,
  OFFLINE,
  TIMEOUT,
  SYNC_ERROR,
  FRAME_ERROR,
  BUFFER_TOO_SMALL,
  OUT_OF_MEMORY,
  CHECKSUM_ERROR,
};
