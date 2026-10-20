#pragma once

#include <Arduino.h>

#include <Adafruit_MAX31856.h>

#include "CONF.h"
#include "PINS.h"

/*
 * ThermocoupleSensor — MAX31856 K-type probe over SPI.
 *
 * update() runs on the interlock cadence (TEMP_INTERLOCK_PERIOD_MS), far more
 * often than the telemetry report, so an open or out-of-range probe is seen
 * by the heater interlock quickly. The host then receives flag 2 with an
 * empty value instead of a frozen number.
 *
 * Fault codes are emitted once per outage (takeFault()), not once per read,
 * so a permanently open probe cannot flood the serial link.
 */
class ThermocoupleSensor {
 public:
  bool begin();
  void update(uint32_t nowMs);

  float temperatureC() const { return _tempC; }  // NAN when invalid
  uint8_t flag() const { return _flag; }         // 0 ok, 1 degraded, 2 invalid
  bool valid() const { return _flag != 2; }

  /* One-shot: the fault code for an outage that has not been reported yet. */
  const char *takeFault();

 private:
  void invalidate(const char *code);

  Adafruit_MAX31856 _chip = Adafruit_MAX31856(PIN_TC_CS);
  float _tempC = NAN;
  uint8_t _flag = 2;          // invalid until the first good reading
  uint32_t _lastReadMs = 0;
  const char *_fault = nullptr;
  bool _faultReported = false;
};
