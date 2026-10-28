#include "ThermocoupleSensor.h"

#include <SPI.h>

namespace {

/* MAX6675 frame (16 bits, MSB first):
 *   D15      always 0
 *   D14..D3  12-bit thermocouple temperature, 0.25 C per count
 *   D2       1 = thermocouple input open
 *   D1       device ID, always 0
 *   D0       three-state / unused
 */
constexpr uint16_t kOpenThermocouple = 0x0004;
constexpr uint16_t kTemperatureMask = 0x7FF8;  // D14..D3
constexpr float kCelsiusPerLsb = 0.25f;
constexpr uint32_t kSpiHz = 1000000;  // datasheet allows up to 4 MHz

}  // namespace

bool ThermocoupleSensor::begin() {
  pinMode(PIN_TC_CS, OUTPUT);
  digitalWrite(PIN_TC_CS, HIGH);  // idle high: the chip converts while CS is high
  SPI.begin(PIN_TC_SCK, PIN_TC_SO, -1, PIN_TC_CS);
  /* SO is high-impedance between reads. Keep the internal pull-up on so an
     unplugged module reads 0xFFFF (open thermocouple) instead of floating to
     a plausible-looking 0 C. The chip overrides it while CS is low. */
  pinMode(PIN_TC_SO, INPUT_PULLUP);

  _beginMs = millis();
  _lastReadMs = _beginMs;
  /* The MAX6675 has no ID register, so presence cannot be checked here. The
     first conversion (or an open probe) is reported by update(). */
  return true;
}

void ThermocoupleSensor::update(uint32_t nowMs) {
  if ((nowMs - _beginMs) < MAX6675_CONVERSION_MS) {
    return;  // the first conversion after power-up is not ready yet
  }
  if ((nowMs - _lastReadMs) < TEMP_INTERLOCK_PERIOD_MS) {
    return;
  }
  _lastReadMs = nowMs;

  const uint16_t raw = readRaw();
  if (raw & kOpenThermocouple) {
    invalidate("TC_OPEN");
    return;
  }

  const float value = (float)((raw & kTemperatureMask) >> 3) * kCelsiusPerLsb;
  if (value < SENSOR_MIN_TEMP_C || value > SENSOR_MAX_TEMP_C) {
    invalidate("TC_FAULT");
    return;
  }

  _tempC = value;
  _flag = 0;
  _faultReported = false;  // next outage reports again
}

/* A read is 16 clocks with CS low. SPI mode 1 (clock idle low, data sampled
   on the rising edge) matches the MAX6675, which shifts out on the falling
   edge. */
uint16_t ThermocoupleSensor::readRaw() {
  SPI.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE1));
  digitalWrite(PIN_TC_CS, LOW);
  delayMicroseconds(2);  // tCS: the first bit is valid ~100 ns after CS falls
  const uint16_t raw = SPI.transfer16(0x0000);
  digitalWrite(PIN_TC_CS, HIGH);
  SPI.endTransaction();
  return raw;
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
