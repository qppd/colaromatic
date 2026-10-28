#pragma once

#include <Arduino.h>

#include "CONF.h"
#include "Fuzzy.h"

/*
 * ColorSetpoint — the outer loop: roast colour decides the temperature target.
 *
 * Two controllers in series:
 *
 *   colour  --(fuzzy)-->  setpoint °C  --(PID)-->  heater duty %
 *                              ^                        |
 *                              |                        v
 *                              +------ thermocouple <---+
 *
 * The fuzzy stage is deliberately a slow outer loop: colour lags temperature
 * by a long way, so it should only nudge the target between control ticks,
 * never chase a single reading.
 *
 * REQUIRES_CALIBRATION: the colour input is an UNCALIBRATED proxy. The TCS3200
 * channels are sampled sequentially and the firmware has no L*a*b* conversion
 * (docs/07_color_calibration.md is PENDING-HW), so the ratio below is not a
 * colour measurement — it is a monotonic stand-in that becomes meaningful only
 * after COLOR_TARGET_RATIO has been measured on the real machine against a
 * colour reference target. Until then the operator setpoint / profile
 * trajectory is the only defensible target, and it is the automatic fallback
 * whenever the colour reading is not usable.
 */
class ColorSetpoint {
 public:
  /* Provisional browning ratio: red / (red + green + blue), 0..1, NAN when the
     sensor sees too little light to trust the ratio. As beans brown, red
     reflectance falls more slowly than blue, so the ratio rises with roast
     development. */
  static float browningRatio(uint32_t redHz, uint32_t greenHz, uint32_t blueHz);

  /* Fuzzy map from colour error (COLOR_TARGET_RATIO - measured ratio, positive
     = lighter than target) to a temperature setpoint in
     [COLOR_SETPOINT_MIN_C, COLOR_SETPOINT_MAX_C]. Returns NAN if the error is
     not finite. */
  static float setpointC(float colorError);
};
