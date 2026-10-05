#include "PofoSmartCablePhy.h"

#include "PofoSmartCableLog.h"

PofoSmartCablePhy::PofoSmartCablePhy()
    : lastClkChangeUs_(0),
      clkChangeCount_(0),
      initialized_(false),
      lastReportedOnline_(false),
      hasReportedLinkState_(false),
      linkStateCallback_(0) {}

PofoSmartCablePhy::~PofoSmartCablePhy() {}

bool PofoSmartCablePhy::begin(int clkIn, int dataIn, int clkOut, int dataOut) {
  if (!configurePins(clkIn, dataIn, clkOut, dataOut)) {
    return false;
  }
  initialized_ = true;
  reset();
  return true;
}

bool PofoSmartCablePhy::online() const {
  const uint32_t lastChange = lastClkChangeUs_;
  const uint32_t now = nowMicros();
  const bool isOnline = initialized_ && lastChange != 0 &&
      static_cast<uint32_t>(now - lastChange) <= kLinkTimeoutMs * 1000U;
  reportLinkState(isOnline);
  return isOnline;
}

PofoResult PofoSmartCablePhy::setLinkStateCallback(
    PofoSmartCableLinkStateCallback callback) {
  if (callback != 0 && linkStateCallback_ != 0) {
    return PofoResult::ALREADY_REGISTERED;
  }
  linkStateCallback_ = callback;
  return PofoResult::OK;
}

PofoResult PofoSmartCablePhy::receiveByte(uint8_t* value) {
  if (value == 0 || !initialized_) {
    return PofoResult::OFFLINE;
  }

  uint8_t received = 0;
  for (uint8_t pair = 0; pair < 4; ++pair) {
    PofoResult result = waitClock(false);
    if (result != PofoResult::OK) {
      return result;
    }
    received = static_cast<uint8_t>((received << 1) | readData());
    writeClock(false);

    result = waitClock(true);
    if (result != PofoResult::OK) {
      return result;
    }
    received = static_cast<uint8_t>((received << 1) | readData());
    writeClock(true);
  }

  *value = received;
  PofoSmartCableComponentLogger& logger =
      pofoSmartCableLogger("PofoSmartCablePhy");
  logger.verbosef("RX 0x%02X", received);
  return PofoResult::OK;
}

PofoResult PofoSmartCablePhy::sendByte(uint8_t value) {
  if (!initialized_) {
    return PofoResult::OFFLINE;
  }

  const uint8_t transmitted = value;
  platformDelayMicros(250);
  for (uint8_t pair = 0; pair < 4; ++pair) {
    writeData((value & 0x80) != 0);
    writeClock(true);
    writeClock(false);
    value <<= 1;

    PofoResult result = waitClock(false);
    if (result != PofoResult::OK) {
      return result;
    }

    writeData((value & 0x80) != 0);
    writeClock(true);
    value <<= 1;

    result = waitClock(true);
    if (result != PofoResult::OK) {
      return result;
    }
  }

  PofoSmartCableComponentLogger& logger =
      pofoSmartCableLogger("PofoSmartCablePhy");
  logger.verbosef("TX 0x%02X", transmitted);
  return PofoResult::OK;
}

PofoResult PofoSmartCablePhy::syncTick() {
  PofoResult result = waitClock(false);
  if (result != PofoResult::OK) {
    return result;
  }
  writeClock(false);

  result = waitClock(true);
  if (result != PofoResult::OK) {
    return result;
  }
  writeClock(true);
  return PofoResult::OK;
}

void PofoSmartCablePhy::delayMicros(uint32_t microseconds) {
  platformDelayMicros(microseconds);
}

void PofoSmartCablePhy::reset() {
  lastClkChangeUs_ = 0;
  clkChangeCount_ = 0;
  hasReportedLinkState_ = false;
  writeClock(false);
  writeData(false);
}

void PofoSmartCablePhy::clockChanged() {
  lastClkChangeUs_ = nowMicros();
  ++clkChangeCount_;
}

PofoResult PofoSmartCablePhy::waitClock(bool high) {
  const uint32_t startedAt = nowMicros();
  while (readClock() != high) {
    const uint32_t now = nowMicros();
    if (static_cast<uint32_t>(now - startedAt) > kHandshakeTimeoutUs) {
      return PofoResult::TIMEOUT;
    }
    idle();
  }
  return PofoResult::OK;
}

void PofoSmartCablePhy::reportLinkState(bool isOnline) const {
  if (hasReportedLinkState_ && lastReportedOnline_ == isOnline) {
    return;
  }
  hasReportedLinkState_ = true;
  lastReportedOnline_ = isOnline;

  const uint32_t now = nowMicros();
  const uint32_t age = lastClkChangeUs_ == 0
      ? UINT32_MAX
      : static_cast<uint32_t>(now - lastClkChangeUs_);
  PofoSmartCableComponentLogger& logger =
      pofoSmartCableLogger("PofoSmartCablePhy");
  if (isOnline) {
    logger.debugf("CLKIN active: age=%lu us, edges=%lu",
                 static_cast<unsigned long>(age),
                 static_cast<unsigned long>(clkChangeCount_));
  } else {
    logger.debugf("CLKIN inactive: age=%lu us, edges=%lu",
                 static_cast<unsigned long>(age),
                 static_cast<unsigned long>(clkChangeCount_));
  }
  if (linkStateCallback_ != 0) {
    linkStateCallback_(isOnline);
  }
}
