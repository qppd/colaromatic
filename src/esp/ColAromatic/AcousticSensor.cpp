#include "AcousticSensor.h"

#include <string.h>

void AcousticSensor::begin() {
  pinMode(PIN_AUDIO_IN, INPUT);  // ADC1_CH0; input-only pin, no pull-up
  _enabled = (AUDIO_ENABLED_DEFAULT != 0);
  _threshold = AUDIO_THRESHOLD_DEFAULT;
  _firstSent = false;
  _secondSent = false;
  _pending = false;
}

void AcousticSensor::configure(bool enabled, uint16_t threshold) {
  _enabled = enabled;
  _threshold = threshold;
}

void AcousticSensor::service(uint32_t nowMs) {
  (void)nowMs;
  if (!_enabled || _pending) {
    return;
  }

  /* Coarse peak-to-peak envelope over a short window. The threshold must be
     calibrated against first-crack audio for the actual drum and microphone. */
  uint16_t minValue = 4095;
  uint16_t maxValue = 0;
  uint32_t start = millis();
  while ((millis() - start) < AUDIO_SAMPLE_WINDOW_MS) {
    uint16_t value = analogRead(PIN_AUDIO_IN);
    if (value < minValue) minValue = value;
    if (value > maxValue) maxValue = value;
  }

  uint16_t peakToPeak = maxValue - minValue;
  if (peakToPeak <= _threshold) {
    return;
  }
  if ((millis() - _lastCrackMs) < AUDIO_COOLDOWN_MS) {
    return;
  }

  const char *kind = nullptr;
  if (!_firstSent) {
    _firstSent = true;
    kind = "first_crack";
  } else if (!_secondSent) {
    _secondSent = true;
    kind = "second_crack";
  } else {
    return;  // no further cracks are claimed for this roast
  }

  float confidence =
      0.5f + ((float)(peakToPeak - _threshold) / (2.0f * ((float)_threshold + 1.0f)));
  if (confidence > 1.0f) {
    confidence = 1.0f;
  }

  memset(&_pendingEvent, 0, sizeof(_pendingEvent));
  strncpy(_pendingEvent.kind, kind, sizeof(_pendingEvent.kind) - 1);
  _pendingEvent.confidence = confidence;
  _pending = true;
  _lastCrackMs = millis();
}

bool AcousticSensor::takeEvent(CrackEvent &out) {
  if (!_pending) {
    return false;
  }
  out = _pendingEvent;
  _pending = false;
  return true;
}

void AcousticSensor::resetCrackState() {
  _firstSent = false;
  _secondSent = false;
  _pending = false;
  _lastCrackMs = 0;
}
