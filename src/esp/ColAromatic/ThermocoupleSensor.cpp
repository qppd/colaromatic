#include "ThermocoupleSensor.h"

bool ThermocoupleSensor::begin() {
  SPI.begin(PIN_TC_SCK, PIN_TC_MISO, PIN_TC_MOSI, PIN_TC_CS);
  if (!_chip.begin()) {
    invalidate("TC_INIT_FAIL");
    return false;
  }
  _chip.setThermocoupleType(MAX31856_TCTYPE_K);
  _chip.setConversionMode(MAX31856_CONTINUOUS);
  return true;
}

void ThermocoupleSensor::update(uint32_t nowMs) {
  if ((nowMs - _lastReadMs) < TEMP_INTERLOCK_PERIOD_MS) {
    return;
  }
  _lastReadMs = nowMs;

  uint8_t fault = _chip.readFault();
  if (fault & (MAX31856_FAULT_OPEN | MAX31856_FAULT_OVUV |
               MAX31856_FAULT_TCRANGE | MAX31856_FAULT_CJRANGE)) {
    /* The fault-status register reflects the live condition and is re-read
       every cycle. The Adafruit driver (1.2.8) has no clearFault(): if a
       stale fault persists after the wiring is repaired, re-initialise the
       converter or power-cycle the controller (docs/12_troubleshooting). */
    invalidate((fault & MAX31856_FAULT_OPEN) ? "TC_OPEN" : "TC_FAULT");
    return;
  }

  float value = _chip.readThermocoupleTemperature();
  if (isnan(value) || value < SENSOR_MIN_TEMP_C || value > SENSOR_MAX_TEMP_C) {
    invalidate("TC_FAULT");
    return;
  }

  _tempC = value;
  _flag = 0;
  _faultReported = false;  // next outage reports again
}

const char *ThermocoupleSensor::takeFault() {
  const char *fault = _fault;
  _fault = nullptr;
  return fault;
}

void ThermocoupleSensor::invalidate(const char *code) {
  _tempC = NAN;
  _flag = 2;
  if (!_faultReported) {
    _fault = code;
    _faultReported = true;
  }
}
