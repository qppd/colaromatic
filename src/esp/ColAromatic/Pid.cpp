#include "Pid.h"

#include <math.h>

void Pid::begin(float kp, float ki, float kd, float outMin, float outMax) {
  _kp = kp;
  _ki = ki;
  _kd = kd;
  _outMin = outMin;
  _outMax = outMax;
  reset(NAN);
}

void Pid::setLimits(float outMin, float outMax) {
  _outMin = outMin;
  _outMax = outMax;
}

void Pid::reset(float measurementC) {
  _integral = 0.0f;
  _out = 0.0f;
  if (isnan(measurementC) || isinf(measurementC)) {
    _prevMeas = NAN;
    _valid = false;
  } else {
    _prevMeas = measurementC;
    _valid = true;
  }
}

float Pid::compute(float setpointC, float measurementC, float dtS) {
  /* No usable measurement: hold the heater off and drop the history so the
     controller cannot resume with an integral built from a different regime. */
  if (isnan(setpointC) || isnan(measurementC) || isinf(setpointC) ||
      isinf(measurementC)) {
    reset(NAN);
    return 0.0f;
  }

  if (!(dtS > 0.0f)) dtS = (float)GEN_CONTROL_PERIOD_MS / 1000.0f;

  const float err = setpointC - measurementC;
  const float p = _kp * err;

  /* Derivative on measurement: no kick when the colour loop moves the target. */
  float d = 0.0f;
  if (_valid && !isnan(_prevMeas)) d = -_kd * (measurementC - _prevMeas) / dtS;

  /* Conditional integration: stop accumulating while saturated in the
     direction the error is pushing. */
  const float u0 = p + _ki * _integral + d;
  const bool satHigh = u0 > _outMax;
  const bool satLow = u0 < _outMin;
  const bool pushingUp = err > 0.0f;
  const bool pushingDown = err < 0.0f;
  if (!((satHigh && pushingUp) || (satLow && pushingDown))) {
    _integral += err * dtS;
  }

  /* Hard bound on the stored term, independent of the conditional rule. */
  if (_ki > 0.0f) {
    const float iMin = PID_INTEGRAL_MIN_PCT / _ki;
    const float iMax = PID_INTEGRAL_MAX_PCT / _ki;
    if (_integral < iMin) _integral = iMin;
    if (_integral > iMax) _integral = iMax;
  } else {
    _integral = 0.0f;
  }

  float u = p + _ki * _integral + d;
  if (u < _outMin) u = _outMin;
  if (u > _outMax) u = _outMax;

  _prevMeas = measurementC;
  _valid = true;
  _out = u;
  return u;
}
