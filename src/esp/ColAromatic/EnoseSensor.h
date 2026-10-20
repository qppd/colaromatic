#pragma once

#include <Arduino.h>

#include <Adafruit_BME680.h>

#include "CONF.h"
#include "PINS.h"

/*
 * EnoseSensor — BME688 gas / environment sensing.
 *
 * update() performs a blocking reading and is called once per report period,
 * never in the interlock path: a BME688 reading takes on the order of 200 ms
 * and must not delay a heater trip (CONF.h REPORT_PERIOD_MS).
 *
 * The gas response is raw sensor response, not an aroma identity, and stays
 * DEGRADED (flag 1) until the warm-up placeholder expires.
 */
class EnoseSensor {
 public:
  bool begin();
  void update(uint32_t nowMs);

  float gasOhm() const { return _gasOhm; }          // NAN when unavailable
  float sensorTempC() const { return _tempC; }
  float humidityPct() const { return _humidityPct; }
  float pressureHpa() const { return _pressureHpa; }
  uint8_t flag() const { return _flag; }            // 0 ok, 1 degraded, 2 invalid
  bool warmupComplete() const { return _warmup; }

  const char *takeFault();

 private:
  void invalidate();

  Adafruit_BME680 _bme = Adafruit_BME680(&Wire);
  bool _ready = false;
  float _gasOhm = NAN;
  float _tempC = NAN;
  float _humidityPct = NAN;
  float _pressureHpa = NAN;
  uint8_t _flag = 2;
  bool _warmup = false;
  uint32_t _warmupStartMs = 0;
  const char *_fault = nullptr;
  bool _faultReported = false;
};
