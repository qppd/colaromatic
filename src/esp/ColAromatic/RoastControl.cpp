#include "RoastControl.h"

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ text */

void RoastControl::copyText(char *dst, size_t n, const char *src) {
  if (n == 0) return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

/* --------------------------------------------------------------- lifecycle */

void RoastControl::begin() {
  _state = ROAST_IDLE;
  _mode = MODE_MANUAL;
  _profileIdx = LINK_PROFILE_NONE;
  _hold = false;
  _endpointPending = false;
  _sessionStartMs = 0;
  _coolingStartMs = 0;
  _coolingTempOk = false;
  _nowMs = 0;
  _endReason[0] = '\0';
  _endStatus = 0;
  _manualDuty = 0.0f;
  _setpointC = NAN;
  _dutyCeiling = GEN_DUTY_CEILING_PCT;
  _profileMaxDuty = NAN;
  _filtC = NAN;
  _quality = TEMP_INVALID;
  _lastValidTempMs = 0;
  _enoseHot = false;
  _ringCount = 0;
  _ringNext = 0;
  _ror = NAN;
  _rorValid = false;
  _rorStrikes = 0;
  _runaway = false;
  _minTempC = NAN;
  _minTempAtMs = 0;
  _stage = STAGE_CHARGE;
  _stageMethod = METHOD_SESSION;
  _stageConfirmed = false;
  _lastConfidence = 0.0f;
  _dutyCmd = 0.0f;
  _dutyApp = 0.0f;
  _lastLoggedDuty = -1000.0f;
  _lastSensorMs = 0;
  _lastControlMs = 0;
  _evHead = 0;
  _evCount = 0;
}

void RoastControl::service(const Sample &sample) {
  _nowMs = sample.nowMs;
  _enoseHot = sample.enoseHot;

  /* Idle and finished: keep a live temperature so the HMI can show the real
     machine state (mirror of the engine's idle acquisition). */
  if (_state == ROAST_IDLE || _state == ROAST_DONE) {
    if (sample.nowMs - _lastSensorMs >= GEN_SENSOR_PERIOD_MS ||
        _lastSensorMs == 0) {
      _lastSensorMs = sample.nowMs;
      updateTemperature(sample);
    }
    return;
  }

  if (_state == ROAST_COOLING) {
    const float elapsedC = (float)(sample.nowMs - _coolingStartMs) / 1000.0f;
    const bool verified = (_quality == TEMP_OK || _quality == TEMP_DEGRADED) && !isnan(_filtC);
    const bool coolEnough = verified && _filtC <= GEN_COOLING_TARGET_C;
    if ((elapsedC >= GEN_COOLING_MIN_S && coolEnough) ||
        elapsedC >= GEN_COOLING_MAX_S) {
      _coolingTempOk = coolEnough;
      if (!coolEnough) {
        pushEvent(EV_COOLING_UNVERIFIED, 0, 0, (uint16_t)elapsedC,
                  "cooling ended without a verified temperature reading");
      }
      _state = ROAST_DONE;
      _stage = STAGE_DONE;
      _stageMethod = METHOD_SESSION;
      _stageConfirmed = true;
      _dutyCmd = 0.0f;
      _dutyApp = 0.0f;
      pushEvent(EV_SESSION_END, 0, _endStatus, (uint16_t)elapsedC, _endReason);
    } else if (sample.nowMs - _lastSensorMs >= GEN_SENSOR_PERIOD_MS ||
               _lastSensorMs == 0) {
      _lastSensorMs = sample.nowMs;
      updateTemperature(sample);
    }
    return;
  }

  /* RUNNING (possibly held) ---------------------------------------------- */
  if (sample.nowMs - _lastSensorMs >= GEN_SENSOR_PERIOD_MS || _lastSensorMs == 0) {
    _lastSensorMs = sample.nowMs;
    onSensorTick(sample);
  }

  if (!_hold && (sample.nowMs - _lastControlMs >= GEN_CONTROL_PERIOD_MS ||
                 _lastControlMs == 0)) {
    _lastControlMs = sample.nowMs;
    onControlTick(sample);
  }
}

/* ------------------------------------------------- temperature processing */

void RoastControl::updateTemperature(const Sample &sample) {
  const bool inRange = !isnan(sample.tempC) &&
                       sample.tempC >= GEN_TEMP_MIN_VALID_C &&
                       sample.tempC <= GEN_TEMP_MAX_VALID_C;
  const bool rawValid = (sample.tempFlag != 2) && inRange;

  if (rawValid) {
    _lastValidTempMs = sample.nowMs;
    _filtC = isnan(_filtC) ? sample.tempC
                           : (GEN_EMA_ALPHA * sample.tempC +
                              (1.0f - GEN_EMA_ALPHA) * _filtC);
    _quality = (sample.tempFlag == 1) ? TEMP_DEGRADED : TEMP_OK;
    /* Keep the RoR window full (one entry per sensor tick). */
    _ring[_ringNext].tMs = sample.nowMs;
    _ring[_ringNext].c = _filtC;
    _ringNext = (_ringNext + 1) % RING_MAX;
    if (_ringCount < RING_MAX) _ringCount++;
  } else if (_lastValidTempMs == 0) {
    _quality = TEMP_INVALID;           // never had a good reading
  } else if (sample.nowMs - _lastValidTempMs >
             (uint32_t)(GEN_TEMP_STALE_S * 1000.0f)) {
    _quality = TEMP_STALE;             // frozen or long-invalid: no longer fresh
  } else {
    _quality = TEMP_INVALID;
  }

  updateRoR(sample.nowMs);
}

/* Least-squares Rate of Rise over the configured window, mirroring the host
   processor: a span shorter than GEN_ROR_MIN_SPAN_S yields no RoR at all
   (the fuzzy input then reads "missing" and the duty falls to 0). */
void RoastControl::updateRoR(uint32_t nowMs) {
  const uint32_t windowMs = (uint32_t)(GEN_ROR_WINDOW_S * 1000.0f);
  float sumT = 0, sumY = 0, sumTT = 0, sumTY = 0;
  float minT = 0, maxT = 0;
  int n = 0;
  uint32_t tRef = nowMs;

  for (int i = 0; i < _ringCount; i++) {
    const RingEntry &e = _ring[i];
    if (nowMs - e.tMs > windowMs) continue;
    if (n == 0) tRef = e.tMs;
    const float t = (float)(e.tMs - tRef) / 1000.0f;
    if (n == 0) { minT = t; maxT = t; }
    if (t < minT) minT = t;
    if (t > maxT) maxT = t;
    sumT += t;
    sumY += e.c;
    sumTT += t * t;
    sumTY += t * e.c;
    n++;
  }

  if (n < 2 || (maxT - minT) < GEN_ROR_MIN_SPAN_S) {
    _ror = NAN;
    _rorValid = false;
    return;
  }
  const float denom = (float)n * sumTT - sumT * sumT;
  if (fabsf(denom) < 1e-6f) {
    _ror = NAN;
    _rorValid = false;
    return;
  }
  const float slopePerS = ((float)n * sumTY - sumT * sumY) / denom;
  _ror = slopePerS * 60.0f;   // degC/min
  _rorValid = true;
}

/* --------------------------------------------------- stage and endpoint */

void RoastControl::advanceStage(uint8_t stage, uint8_t method, bool estimated,
                                float confidence, bool requiresConfirmation,
                                uint32_t nowMs, const char *text, bool confirmed) {
  (void)nowMs;
  (void)estimated;
  (void)requiresConfirmation;
  _stage = stage;
  _stageMethod = method;
  _stageConfirmed = confirmed;
  _lastConfidence = confidence;
  pushEvent(EV_STAGE_CHANGE, stage, method, (uint16_t)(confidence * 100.0f + 0.5f),
            text);
}

void RoastControl::checkStage(const Sample &sample, float elapsedS) {
  (void)sample;
  if (_stage == STAGE_DONE) return;

  /* Turning point: lowest filtered temperature after charge, then a rebound
     of GEN_TP_REBOUND_C with at least GEN_TP_MIN_ELAPSED_S elapsed.
     Estimated, no confirmation required — same as the host detector. */
  if (stageIdx(_stage) < stageIdx(STAGE_TURNING_POINT) && !isnan(_filtC)) {
    if (isnan(_minTempC) || _filtC < _minTempC) {
      _minTempC = _filtC;
      _minTempAtMs = _nowMs;
    }
    const float rebound = _filtC - _minTempC;
    if (_minTempAtMs != 0 && elapsedS >= GEN_TP_MIN_ELAPSED_S &&
        rebound >= GEN_TP_REBOUND_C) {
      advanceStage(STAGE_TURNING_POINT, METHOD_TEMP_MINIMUM, true, 0.6f, false,
                   _nowMs, "temperature_minimum_rebound", false);
    }
  }

  /* Colour-change stage is intentionally NOT evaluated here: it needs a
     validated colour calibration, which exists only in the host tooling
     (docs/13_known_limitations.md). The host threshold is disabled in
     system.json (color_change_delta_l = null) anyway. */
}

void RoastControl::checkEndpoint(const Sample &sample, float elapsedS) {
  (void)sample;
  if (_profileIdx >= GEN_PROFILE_COUNT) return;
  const GenProfile &p = GEN_PROFILES[_profileIdx];

  /* Maximum duration stops immediately, no confirmation (host order). */
  if (!isnan(p.endpointMaxDurationS) && elapsedS >= p.endpointMaxDurationS) {
    beginCooling(_nowMs, "maximum_duration_reached", "complete");
    return;
  }

  /* automatic_ready() on the host: an operator-confirmation endpoint never
     triggers by itself. */
  if (p.operatorConfirmRequired) return;

  if (p.endpointMethod == 2 /* temperature */ && !isnan(p.endpointTargetC) &&
      !isnan(_filtC) && _filtC >= p.endpointTargetC) {
    if (!_endpointPending) {
      _endpointPending = true;
      pushEvent(EV_ENDPOINT_PENDING, 0, 0, (uint16_t)(_filtC * 10.0f),
                "temperature_target_reached");
    }
  }
  /* Colour and time endpoints: colour needs calibration the MCU does not
     have; time is covered by max duration above. */
}

/* ------------------------------------------------------- session control */

float RoastControl::targetAt(float elapsedS) const {
  if (_profileIdx < GEN_PROFILE_COUNT) {
    const GenProfile &p = GEN_PROFILES[_profileIdx];
    if (p.trajCount > 0 && p.traj != nullptr) {
      const GenTrajPoint *traj = p.traj;
      if (elapsedS <= traj[0].elapsedS) return traj[0].targetC;
      for (int i = 0; i + 1 < p.trajCount; i++) {
        if (elapsedS <= traj[i + 1].elapsedS) {
          const float span = traj[i + 1].elapsedS - traj[i].elapsedS;
          if (span <= 0) return traj[i + 1].targetC;
          const float frac = (elapsedS - traj[i].elapsedS) / span;
          return traj[i].targetC + frac * (traj[i + 1].targetC - traj[i].targetC);
        }
      }
      return traj[p.trajCount - 1].targetC;
    }
  }
  return _setpointC;  // engine fallback: trajectory, else operator setpoint
}

float RoastControl::dutyCeiling() const { return _dutyCeiling; }

/* Mirror of control/heater.py::HeaterOutput.command: ceiling, minimum duty,
   and minimum on/off quantisation inside the proportioning window. */
float RoastControl::shapeDuty(float requested) const {
  const float ceiling = dutyCeiling();
  const float windowMs = (float)GEN_HEATER_WINDOW_MS;

  float req = requested;
  if (req > ceiling) req = ceiling;
  if (req < GEN_HEATER_MIN_PCT) req = GEN_HEATER_MIN_PCT;

  float applied = req;
  if (req > 0.0f && req < 100.0f) {
    const float onMs = req / 100.0f * windowMs;
    if (onMs < (float)GEN_HEATER_MIN_ON_MS) {
      applied = (float)GEN_HEATER_MIN_ON_MS / windowMs * 100.0f;
    }
    const float offMs = windowMs - onMs;
    if (offMs < (float)GEN_HEATER_MIN_OFF_MS) {
      applied = 100.0f - ((float)GEN_HEATER_MIN_OFF_MS / windowMs * 100.0f);
    }
  }
  if (applied < 0.0f) applied = 0.0f;
  const float cap = (req > 0.0f) ? ceiling : 0.0f;
  if (applied > cap) applied = cap;
  if (ceiling > 0.0f && applied > ceiling) applied = ceiling;
  return applied;
}

void RoastControl::onSensorTick(const Sample &sample) {
  updateTemperature(sample);

  const float elapsedS = (float)(_nowMs - _sessionStartMs) / 1000.0f;
  if (_state != ROAST_RUNNING && _state != ROAST_HOLD) return;

  checkStage(sample, elapsedS);
  checkEndpoint(sample, elapsedS);

  /* RoR runaway: three consecutive sensor ticks above the limit latch the
     fault (host: control/safety.py counter, same count and threshold). */
  if (_rorValid && !isnan(_ror) && _ror > GEN_MAX_ROR_CPM) {
    if (_rorStrikes < 255) _rorStrikes++;
    if (_rorStrikes >= GEN_ROR_RUNAWAY_STRIKES) _runaway = true;
  } else {
    _rorStrikes = 0;
  }
}

void RoastControl::onControlTick(const Sample &sample) {
  (void)sample;
  float duty = 0.0f;

  if (_mode == MODE_AUTO) {
    const float elapsedS = (float)(_nowMs - _sessionStartMs) / 1000.0f;
    const float target = targetAt(elapsedS);
    float values[GEN_FUZZY_INPUT_COUNT];
    for (int i = 0; i < GEN_FUZZY_INPUT_COUNT; i++) {
      const char *id = GEN_FUZZY_INPUTS[i].id;
      if (strcmp(id, "temp_error_c") == 0) {
        values[i] = (isnan(target) || isnan(_filtC)) ? NAN : (target - _filtC);
      } else if (strcmp(id, "ror_c_per_min") == 0) {
        values[i] = _rorValid ? _ror : NAN;
      } else {
        values[i] = NAN;  // any other enabled input is not sensed onboard
      }
    }
    const FuzzyController::Result result = _fuzzy.evaluate(values);
    duty = result.output;  // 0 for missing input or no rule fired (engine: `or 0.0`)
  } else {
    duty = _manualDuty;
  }

  if (duty < 0.0f) duty = 0.0f;
  if (duty > dutyCeiling()) duty = dutyCeiling();
  _dutyCmd = duty;
  _dutyApp = shapeDuty(duty);

  if (fabsf(_dutyApp - _lastLoggedDuty) >= 5.0f) {
    _lastLoggedDuty = _dutyApp;
    pushEvent(EV_HEATER_COMMAND, (uint8_t)(duty + 0.5f), _mode,
              (uint16_t)(_dutyApp * 10.0f + 0.5f), "heater_command");
  }
}

void RoastControl::beginCooling(uint32_t nowMs, const char *reason,
                                const char *status) {
  _state = ROAST_COOLING;
  _coolingStartMs = nowMs;
  _coolingTempOk = false;
  _hold = false;
  _endpointPending = false;
  _dutyCmd = 0.0f;
  _dutyApp = 0.0f;
  copyText(_endReason, sizeof(_endReason), reason);
  /* status: complete | stopped | aborted_safety | aborted_estop */
  _endStatus = 0;
  if (strcmp(status, "stopped") == 0) _endStatus = 1;
  else if (strcmp(status, "aborted_safety") == 0) _endStatus = 2;
  else if (strcmp(status, "aborted_estop") == 0) _endStatus = 3;
  pushEvent(EV_COOLING_START, _endStatus, 0, 0, reason);
}

void RoastControl::abortToCooling(const char *reason, const char *status) {
  if (_state == ROAST_RUNNING || _state == ROAST_HOLD) {
    beginCooling(_nowMs, reason, status);
  }
}

/* ------------------------------------------------------------- commands */

uint8_t RoastControl::start(uint8_t profileIdx, int16_t setpoint10,
                            const char *&text) {
  if (_state == ROAST_RUNNING || _state == ROAST_HOLD || _state == ROAST_COOLING) {
    text = "SESSION_ALREADY_ACTIVE";
    return ACK_STATE;
  }
  if (profileIdx >= GEN_PROFILE_COUNT) {
    text = "SELECT_A_PROFILE_FIRST";
    return ACK_PROFILE;
  }
  const GenProfile &p = GEN_PROFILES[profileIdx];

  if (setpoint10 != LINK_SETPOINT_UNCHANGED) {
    const float sp = setpoint10 / 10.0f;
    if (isnan(sp) || sp <= 0.0f) {
      text = "SETPOINT_OUT_OF_RANGE";
      return ACK_ARG;
    }
    if (!isnan(p.setpointMaxC) && sp > p.setpointMaxC) {
      text = "SETPOINT_ABOVE_PROFILE_LIMIT";
      return ACK_ARG;
    }
    _setpointC = sp;
  }

  if (_mode == MODE_AUTO) {
    if (!p.autoEligible) {
      text = "AUTO_NEEDS_A_VALIDATED_PROFILE_WITH_TRAJECTORY";
      return ACK_GATING;
    }
    if (p.trajCount < 2 && isnan(_setpointC)) {
      text = "SETPOINT_OR_TRAJECTORY_REQUIRED";
      return ACK_ARG;
    }
  }

  if (_quality != TEMP_OK && _quality != TEMP_DEGRADED) {
    text = "NO_VALID_TEMPERATURE_READING";
    return ACK_GATING;
  }

  /* Begin the session (mirror engine.start_session). */
  _state = ROAST_RUNNING;
  _hold = false;
  _endpointPending = false;
  _profileIdx = profileIdx;
  _sessionStartMs = _nowMs;
  _lastSensorMs = _nowMs;
  _lastControlMs = _nowMs;          // first control tick on the next pass
  _coolingStartMs = 0;
  _coolingTempOk = false;
  _stage = STAGE_CHARGE;
  _stageMethod = METHOD_SESSION;
  _stageConfirmed = false;
  _minTempC = NAN;
  _minTempAtMs = 0;
  _ringCount = 0;
  _ringNext = 0;
  _ror = NAN;
  _rorValid = false;
  _rorStrikes = 0;
  _runaway = false;
  _dutyCmd = 0.0f;
  _dutyApp = 0.0f;
  _lastLoggedDuty = -1000.0f;
  _dutyCeiling = GEN_DUTY_CEILING_PCT;
  if (!isnan(p.maxDutyPct) && p.maxDutyPct < _dutyCeiling) {
    _dutyCeiling = p.maxDutyPct;
  }
  _profileMaxDuty = p.maxDutyPct;

  pushEvent(EV_SESSION_START, _mode, _profileIdx,
            (uint16_t)(_dutyCeiling * 10.0f + 0.5f), p.id);
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::hold(const char *&text) {
  if (_state != ROAST_RUNNING || _hold) {
    text = "NOT_RUNNING";
    return ACK_STATE;
  }
  _hold = true;
  _dutyCmd = 0.0f;
  _dutyApp = 0.0f;
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::resume(const char *&text) {
  if (_state != ROAST_RUNNING || !_hold) {
    text = "HOLD_NOT_ACTIVE";
    return ACK_STATE;
  }
  _hold = false;
  _lastControlMs = _nowMs;  // recompute duty on the next pass
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::cool(const char *&text) {
  if (_state != ROAST_RUNNING && _state != ROAST_HOLD) {
    text = "NO_ACTIVE_SESSION";
    return ACK_STATE;
  }
  beginCooling(_nowMs, "operator_stop", "stopped");
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::setDuty(uint8_t pct, const char *&text) {
  if (pct > 100) {
    text = "DUTY_OUT_OF_RANGE";
    return ACK_ARG;
  }
  if (_state == ROAST_COOLING || _state == ROAST_DONE) {
    text = "SESSION_NOT_RUNNING";
    return ACK_STATE;
  }
  _manualDuty = (float)pct;
  if (pct > _dutyCeiling) {
    text = "CLAMPED_TO_DUTY_CEILING";
  } else {
    text = "OK";
  }
  if (_state == ROAST_RUNNING && !_hold) {
    _lastControlMs = _nowMs;  // apply on the next pass
  }
  return ACK_OK;
}

uint8_t RoastControl::setMode(uint8_t mode, const char *&text) {
  if (mode != MODE_MANUAL && mode != MODE_AUTO) {
    text = "UNKNOWN_MODE";
    return ACK_ARG;
  }
  const bool sessionLive =
      (_state == ROAST_RUNNING || _state == ROAST_HOLD || _state == ROAST_COOLING);
  if (mode == MODE_AUTO && sessionLive) {
    /* Falling back to manual mid-roast is always allowed; entering automatic
       control mid-roast is not — the trajectory was fixed at start. */
    text = "MODE_LOCKED_WHILE_ROASTING";
    return ACK_STATE;
  }
  _mode = mode;
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::selectProfile(uint8_t idx, const char *&text) {
  if (idx >= GEN_PROFILE_COUNT) {
    text = "UNKNOWN_PROFILE";
    return ACK_PROFILE;
  }
  if (_state == ROAST_RUNNING || _state == ROAST_HOLD || _state == ROAST_COOLING) {
    text = "STOP_THE_SESSION_FIRST";
    return ACK_STATE;
  }
  _profileIdx = idx;
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::setSetpoint(int16_t setpoint10, const char *&text) {
  const float sp = setpoint10 / 10.0f;
  if (isnan(sp) || sp <= 0.0f) {
    text = "SETPOINT_OUT_OF_RANGE";
    return ACK_ARG;
  }
  if (_profileIdx < GEN_PROFILE_COUNT) {
    const float limit = GEN_PROFILES[_profileIdx].setpointMaxC;
    if (!isnan(limit) && sp > limit) {
      text = "SETPOINT_ABOVE_PROFILE_LIMIT";
      return ACK_ARG;
    }
  }
  _setpointC = sp;
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::confirmStage(uint8_t stage, const char *&text) {
  if (_state != ROAST_RUNNING && _state != ROAST_HOLD) {
    text = "NO_ACTIVE_SESSION";
    return ACK_STATE;
  }
  if (stage > STAGE_DONE) {
    text = "UNKNOWN_STAGE";
    return ACK_ARG;
  }
  if (stageIdx(stage) <= stageIdx(_stage)) {
    text = "STAGE_ALREADY_PASSED";
    return ACK_STATE;
  }
  advanceStage(stage, METHOD_OPERATOR, false, 1.0f, false, _nowMs,
               "operator_confirmed", true);
  text = "OK";
  return ACK_OK;
}

uint8_t RoastControl::confirmEndpoint(const char *&text) {
  if (!_endpointPending) {
    text = "NO_PENDING_ENDPOINT";
    return ACK_STATE;
  }
  _endpointPending = false;
  beginCooling(_nowMs, "operator_confirmed_endpoint", "complete");
  text = "OK";
  return ACK_OK;
}

void RoastControl::notifyAcoustic(const char *kind, float confidence) {
  if (!GEN_ALLOW_ACOUSTIC) return;
  if (confidence < GEN_ACOUSTIC_MIN_CONF) return;
  if (_state != ROAST_RUNNING && _state != ROAST_HOLD && _state != ROAST_COOLING) {
    return;
  }

  uint8_t target;
  if (kind != nullptr && strcmp(kind, "first_crack") == 0) target = STAGE_FIRST_CRACK;
  else if (kind != nullptr && strcmp(kind, "second_crack") == 0) target = STAGE_SECOND_CRACK;
  else return;

  /* Engine logs the machine event first, then the stage change it caused. */
  pushEvent(EV_CRACK, target, 0, (uint16_t)(confidence * 100.0f + 0.5f), kind);

  if (stageIdx(target) <= stageIdx(_stage)) return;
  advanceStage(target, METHOD_ACOUSTIC, true, confidence, true, _nowMs,
               "acoustic_event", false);
}

/* ------------------------------------------------------------ telemetry */

uint8_t RoastControl::roastState() const {
  if (_state == ROAST_RUNNING && _hold) return ROAST_HOLD;
  return _state;
}

uint16_t RoastControl::extraFaults() const {
  uint16_t bits = 0;
  if (_quality == TEMP_STALE) bits |= F_TEMP_STALE;
  if (_enoseHot) bits |= F_ENOSE_HOT;
  return bits;
}

uint32_t RoastControl::sessionMs(uint32_t nowMs) const {
  if (_state == ROAST_IDLE) return 0;
  return nowMs - _sessionStartMs;
}

bool RoastControl::consumeRunaway() {
  const bool r = _runaway;
  _runaway = false;
  return r;
}

/* --------------------------------------------------------------- events */

void RoastControl::pushEvent(uint8_t type, uint8_t a, uint8_t b, uint16_t value,
                             const char *text) {
  if (_evCount == EVQ_MAX) {  // drop the oldest rather than lose this one
    _evHead = (_evHead + 1) % EVQ_MAX;
    _evCount--;
  }
  const int slot = (_evHead + _evCount) % EVQ_MAX;
  Event &e = _evq[slot];
  e.type = type;
  e.a = a;
  e.b = b;
  e.value = value;
  copyText(e.text, sizeof(e.text), text);
  _evCount++;
}

bool RoastControl::takeEvent(Event &out) {
  if (_evCount == 0) return false;
  out = _evq[_evHead];
  _evHead = (_evHead + 1) % EVQ_MAX;
  _evCount--;
  return true;
}
