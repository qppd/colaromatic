#include "EnoseSensor.h"

#include <Wire.h>

bool EnoseSensor::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  _ready = _bme.begin(ENOSE_I2C_ADDR_LOW, &Wire) ||
           _bme.begin(ENOSE_I2C_ADDR_HIGH, &Wire);
  if (!_ready) {
    _flag = 2;
    _fault = "BME_INIT_FAIL";
    _faultReported = true;
    return false;
  }

  _bme.setTemperatureOversampling(BME680_OS_8X);
  _bme.setHumidityOversampling(BME680_OS_2X);
  _bme.setPressureOversampling(BME680_OS_4X);
  _bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  _bme.setGasHeater(320, 150);  // provisional profile, REQUIRES_CALIBRATION
  _warmupStartMs = millis();
  return true;
}

void EnoseSensor::update(uint32_t nowMs) {
  if (!_ready) {
    _flag = 2;
    return;
  }
  _warmup = (nowMs - _warmupStartMs) > ENOSE_WARMUP_MS;

  if (!_bme.performReading()) {
    invalidate();
    return;
  }

  _gasOhm = _bme.gas_resistance;
  _tempC = _bme.temperature;
  _humidityPct = _bme.humidity;
  _pressureHpa = _bme.pressure / 100.0f;  // Pa -> hPa
  _flag = _warmup ? 0 : 1;
  _faultReported = false;
}

const char *EnoseSensor::takeFault() {
  const char *fault = _fault;
  _fault = nullptr;
  return fault;
}

void EnoseSensor::invalidate() {
  /* Empty fields, not the previous numbers: the host must be able to tell
     "no reading" from "the same reading as last second". */
  _gasOhm = NAN;
  _tempC = NAN;
  _humidityPct = NAN;
  _pressureHpa = NAN;
  _flag = 2;
  if (!_faultReported) {
    _fault = "BME_READ_FAIL";
    _faultReported = true;
  }
}
