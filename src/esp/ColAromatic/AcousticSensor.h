#pragma once

#include <Arduino.h>

#include "CONF.h"
#include "PINS.h"

/* One detected acoustic event, handed to the sketch to be framed as K,... */
struct CrackEvent {
  char kind[16];
  float confidence;
};

/*
 * AcousticSensor — envelope microphone for crack detection.
 *
 * Disabled by default (AUDIO_ENABLED_DEFAULT 0) and unvalidated: the
 * threshold is raw ADC peak-to-peak counts and must be calibrated against a
 * real first crack before the host may trust the event (REQ-STAGE-004).
 * The host still requires its own confidence threshold and, for stage
 * changes, operator confirmation.
 *
 * service() busy-samples the ADC for AUDIO_SAMPLE_WINDOW_MS per pass while
 * enabled; leave it disabled until the microphone is fitted and calibrated.
 */
class AcousticSensor {
 public:
  void begin();
  void configure(bool enabled, uint16_t threshold);
  void service(uint32_t nowMs);
  bool takeEvent(CrackEvent &out);
  void resetCrackState();  // CMD,RESET: cracks must be re-detected

  bool enabled() const { return _enabled; }
  uint16_t threshold() const { return _threshold; }

 private:
  bool _enabled = false;
  uint16_t _threshold = AUDIO_THRESHOLD_DEFAULT;
  bool _firstSent = false;
  bool _secondSent = false;
  bool _pending = false;
  uint32_t _lastCrackMs = 0;
  CrackEvent _pendingEvent = {"", 0.0f};
};
