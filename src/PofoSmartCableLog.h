#pragma once

#if defined(__has_include)
#if __has_include(<log4mcu.h>)
#define POFO_SMART_CABLE_HAS_LOG4MCU 1
#endif
#endif

#if defined(POFO_SMART_CABLE_HAS_LOG4MCU)

#include <log4mcu.h>

typedef log4mcu::Logger PofoSmartCableComponentLogger;

inline PofoSmartCableComponentLogger& pofoSmartCableLogger(const char* tag) {
  return log4mcu::Logger::get(tag);
}

#else

class PofoSmartCableNullLogger {
 public:
  void debugf(const char* format, ...) {}
  void infof(const char* format, ...) {}
  void warnf(const char* format, ...) {}
};

typedef PofoSmartCableNullLogger PofoSmartCableComponentLogger;

inline PofoSmartCableComponentLogger& pofoSmartCableLogger(const char* tag) {
  static PofoSmartCableNullLogger logger;
  return logger;
}

#endif
