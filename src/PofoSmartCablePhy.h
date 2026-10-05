#pragma once

#include "PofoSmartCableResult.h"

// Reports a link state transition (online() changed). Not called on every
// online() poll - only when the state actually flips.
typedef void (*PofoSmartCableLinkStateCallback)(bool isOnline);

class PofoSmartCablePhy {
 public:
  PofoSmartCablePhy();
  virtual ~PofoSmartCablePhy();

  bool begin(int clkIn, int dataIn, int clkOut, int dataOut);
  bool online() const;

  // Registers a callback invoked when online() transitions between true
  // and false. Pass 0 to clear it. Returns ALREADY_REGISTERED if a
  // non-zero callback is already set and callback is also non-zero -
  // clear it first.
  PofoResult setLinkStateCallback(PofoSmartCableLinkStateCallback callback);

  PofoResult receiveByte(uint8_t* value);
  PofoResult sendByte(uint8_t value);
  PofoResult syncTick();
  void delayMicros(uint32_t microseconds);
  void reset();

 protected:
  void clockChanged();

  virtual bool configurePins(int clkIn, int dataIn, int clkOut,
                             int dataOut) = 0;
  virtual uint32_t nowMicros() const = 0;
  virtual bool readClock() const = 0;
  virtual bool readData() const = 0;
  virtual void writeClock(bool high) = 0;
  virtual void writeData(bool high) = 0;
  virtual void platformDelayMicros(uint32_t microseconds) = 0;
  virtual void idle() = 0;

 private:
  static const uint32_t kLinkTimeoutMs = 300;
  static const uint32_t kHandshakeTimeoutUs = 2000000;

  PofoResult waitClock(bool high);
  void reportLinkState(bool isOnline) const;

  volatile uint32_t lastClkChangeUs_;
  volatile uint32_t clkChangeCount_;
  bool initialized_;
  mutable bool lastReportedOnline_;
  mutable bool hasReportedLinkState_;
  PofoSmartCableLinkStateCallback linkStateCallback_;
};
