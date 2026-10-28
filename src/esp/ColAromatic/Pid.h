#pragma once

#include <Arduino.h>

#include "CONF.h"

/*
 * Pid — parallel-form PID temperature controller.
 *
 *   duty = kp*e + ki*∫e dt - kd*dT/dt        e = setpoint - measured, °C
 *
 * Gains are per second (ki and kd), so the result does not change if the
 * control period drifts; the caller passes the real elapsed time.
 *
 * Three deliberate choices, because this drives a mains heater:
 *
 *   - Derivative acts on the MEASUREMENT, not the error, so a setpoint step
 *     (the colour loop moving the target) cannot produce a derivative kick.
 *   - Conditional integration: the integral stops accumulating while the
 *     output is saturated in the direction the error is pushing. Without it a
 *     long heat-up through the duty ceiling would wind the term up and the
 *     heater would overshoot by tens of degrees on the first approach.
 *   - A missing or non-finite measurement forces output 0 and clears the
 *     state. The heater is already cut by HeaterSafety in that case; this only
 *     keeps the controller from resuming with stale state.
 *
 * The output is a duty REQUEST in percent. It is still subject to the duty
 * ceiling, the minimum on/off quantisation (RoastControl::shapeDuty) and every
 * HeaterSafety interlock — the PID can never bypass those.
 */
class Pid {
 public:
  void begin(float kp, float ki, float kd, float outMin, float outMax);

  /* Change the output range without touching the gains or the integral, so a
     profile that lowers the duty ceiling also narrows the anti-windup window. */
  void setLimits(float outMin, float outMax);

  /* Forget the integral and the previous measurement. Pass the current
     temperature when one is known so the first derivative term is 0. */
  void reset(float measurementC = NAN);

  /* Returns the duty request in [outMin, outMax]. dtS <= 0 falls back to one
     nominal control period. */
  float compute(float setpointC, float measurementC, float dtS);

  float output() const { return _out; }
  float integralTerm() const { return _ki * _integral; }  // percent
  bool valid() const { return _valid; }  // has a previous measurement

 private:
  float _kp = 0.0f;
  float _ki = 0.0f;
  float _kd = 0.0f;
  float _outMin = 0.0f;
  float _outMax = 100.0f;
  float _integral = 0.0f;   // accumulated error*time, °C·s
  float _prevMeas = NAN;
  float _out = 0.0f;
  bool _valid = false;
};
