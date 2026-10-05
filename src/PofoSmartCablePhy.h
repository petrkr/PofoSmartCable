#pragma once

#include "PofoSmartCableResult.h"

class PofoSmartCablePhy {
 public:
  PofoSmartCablePhy();
  virtual ~PofoSmartCablePhy();

  bool begin(int clkIn, int dataIn, int clkOut, int dataOut);
  bool online() const;
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

  PofoResult waitClock(bool high);
  void reportLinkState(bool isOnline) const;

  volatile uint32_t lastClkChangeUs_;
  volatile uint32_t clkChangeCount_;
  bool initialized_;
  mutable bool lastReportedOnline_;
  mutable bool hasReportedLinkState_;
};
