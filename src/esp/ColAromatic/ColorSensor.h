#pragma once

#include <Arduino.h>

#include "CONF.h"
#include "PINS.h"

/*
 * ColorSensor — TCS3200 light-to-frequency colour sensing.
 *
 * One channel is integrated for COLOR_WINDOW_MS, the pulse count is converted
 * to Hz, and the next channel is selected. service() is non-blocking except
 * for the 200 us filter-settle delay, so it never delays the interlocks.
 *
 * Frequencies only; converting to CIELAB needs the calibration record and is
 * done on the host (docs/07_color_calibration.md).
 */
class ColorSensor {
 public:
  void begin();
  void service(uint32_t nowMs);

  uint32_t redHz() const { return _redHz; }
  uint32_t greenHz() const { return _greenHz; }
  uint32_t blueHz() const { return _blueHz; }
  uint32_t clearHz() const { return _clearHz; }

 private:
  void applyFilter(uint8_t index);
  void startWindow(uint32_t nowMs);
  void finishWindow(uint32_t nowMs);

  uint32_t _redHz = 0;
  uint32_t _greenHz = 0;
  uint32_t _blueHz = 0;
  uint32_t _clearHz = 0;
  uint8_t _index = 0;        // 0 red, 1 blue, 2 clear, 3 green
  uint32_t _windowStart = 0;
};
