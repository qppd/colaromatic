#pragma once

#include <Arduino.h>

#include "CONF.h"
#include "PINS.h"

/*
 * ThermocoupleSensor — MAX6675 K-type probe over SPI.
 *
 * The MAX6675 is read-only: no configuration register, no thermocouple-type
 * setting and no fault register beyond the open-thermocouple bit. A read
 * returns the last 12-bit conversion (0.25 C resolution, 0..1024 C); the chip
 * needs MAX6675_CONVERSION_MS (220 ms) per conversion, which is why the
 * interlock cadence is 250 ms.
 *
 * update() runs on the interlock cadence (TEMP_INTERLOCK_PERIOD_MS), far more
 * often than the telemetry report, so an open or out-of-range probe is seen
 * by the heater interlock quickly. The host then receives flag 2 with an
 * empty value instead of a frozen number.
 *
 * Fault codes are emitted once per outage (takeFault()), not once per read,
 * so a permanently open probe cannot flood the serial link.
 *
 * Limitations: the MAX6675 cannot detect a cold-junction fault or an
 * over/under-voltage condition. A data line shorted low reads as 0 C; an
 * unplugged module is pulled to 0xFFFF and reports TC_OPEN because SO has an
 * internal pull-up. The independent hardware thermal cutoff is still the only
 * protection against a false reading (docs/13_known_limitations.md).
 *
 * Wiring: many MAX6675 breakouts are 5 V boards. Check the module's logic
 * level before connecting SO to the ESP32 (3.3 V); use a 3.3 V module or a
 * level shifter on SO/SCK/CS if it is a 5 V board.
 */
class ThermocoupleSensor {
 public:
  bool begin();
  void update(uint32_t nowMs);

  float temperatureC() const { return _tempC; }  // NAN when invalid
  uint8_t flag() const { return _flag; }  // 0 ok, 2 invalid; 1 = degraded is
                                          // reserved (the MAX6675 cannot
                                          // report it)
  bool valid() const { return _flag != 2; }

  /* One-shot: the fault code for an outage that has not been reported yet. */
  const char *takeFault();

 private:
  uint16_t readRaw();
  void invalidate(const char *code);

  float _tempC = NAN;
  uint8_t _flag = 2;          // invalid until the first good reading
  uint32_t _beginMs = 0;      // first conversion is not ready before 220 ms
  uint32_t _lastReadMs = 0;
  const char *_fault = nullptr;
  bool _faultReported = false;
};
