#include "ColorSensor.h"

/* Pulse counting runs in the ISR; the counter is read and cleared from loop()
   context, so both sides take the same port mux. */
static volatile uint32_t s_pulses = 0;
static portMUX_TYPE s_pulseMux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR colorIsr() {
  portENTER_CRITICAL_ISR(&s_pulseMux);
  /* Local copy: incrementing a volatile directly is deprecated in C++20. */
  uint32_t pulses = s_pulses;
  pulses++;
  s_pulses = pulses;
  portEXIT_CRITICAL_ISR(&s_pulseMux);
}

void ColorSensor::begin() {
  pinMode(PIN_TCS_S0, OUTPUT);
  pinMode(PIN_TCS_S1, OUTPUT);
  pinMode(PIN_TCS_S2, OUTPUT);
  pinMode(PIN_TCS_S3, OUTPUT);
  pinMode(PIN_TCS_LED, OUTPUT);
  pinMode(PIN_TCS_OUT, INPUT);

  digitalWrite(PIN_TCS_LED, LOW);   // illumination is external today
  digitalWrite(PIN_TCS_S0, LOW);    // 20 % output scaling
  digitalWrite(PIN_TCS_S1, HIGH);

  attachInterrupt(digitalPinToInterrupt(PIN_TCS_OUT), colorIsr, RISING);
  _windowStart = 0;
}

void ColorSensor::service(uint32_t nowMs) {
  if (_windowStart == 0) {
    startWindow(nowMs);
    return;
  }
  if ((nowMs - _windowStart) < COLOR_WINDOW_MS) {
    return;
  }
  finishWindow(nowMs);
  _index = (uint8_t)((_index + 1) % 4);
  startWindow(nowMs);
}

void ColorSensor::applyFilter(uint8_t index) {
  /* Filter selects (TCS3200 datasheet): 00 red, 01 blue, 10 clear, 11 green */
  switch (index) {
    case 0:
      digitalWrite(PIN_TCS_S2, LOW);
      digitalWrite(PIN_TCS_S3, LOW);
      break;
    case 1:
      digitalWrite(PIN_TCS_S2, LOW);
      digitalWrite(PIN_TCS_S3, HIGH);
      break;
    case 2:
      digitalWrite(PIN_TCS_S2, HIGH);
      digitalWrite(PIN_TCS_S3, LOW);
      break;
    default:
      digitalWrite(PIN_TCS_S2, HIGH);
      digitalWrite(PIN_TCS_S3, HIGH);
      break;
  }
}

void ColorSensor::startWindow(uint32_t nowMs) {
  applyFilter(_index);
  delayMicroseconds(200);  // filter settle
  portENTER_CRITICAL(&s_pulseMux);
  s_pulses = 0;
  portEXIT_CRITICAL(&s_pulseMux);
  _windowStart = nowMs;
}

void ColorSensor::finishWindow(uint32_t nowMs) {
  uint32_t counts;
  portENTER_CRITICAL(&s_pulseMux);
  counts = s_pulses;
  portEXIT_CRITICAL(&s_pulseMux);

  uint32_t elapsed = nowMs - _windowStart;
  if (elapsed == 0) {
    elapsed = 1;
  }
  uint32_t hz = (uint32_t)((uint64_t)counts * 1000ULL / elapsed);

  switch (_index) {
    case 0: _redHz = hz; break;
    case 1: _blueHz = hz; break;
    case 2: _clearHz = hz; break;
    default: _greenHz = hz; break;
  }
}
