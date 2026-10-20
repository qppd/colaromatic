#include "HeaterSafety.h"

/* The fan is driven by LEDC. Arduino-ESP32 2.x and 3.x differ in that API. */
static void fanWrite(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_FAN, duty);
#else
  ledcWrite(0, duty);
#endif
}

void HeaterSafety::begin() {
  pinMode(PIN_SSR_1, OUTPUT);
  pinMode(PIN_SSR_2, OUTPUT);
  applyOutput(false);

  pinMode(PIN_FAN, OUTPUT);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_FAN, 25000, 8);
#else
  ledcSetup(0, 25000, 8);
  ledcAttachPin(PIN_FAN, 0);
#endif
  writeFan(0);

  /* GPIO 35 was rejected: input-only pins have no internal pull-up, so
     INPUT_PULLUP there would silently do nothing and leave this floating. */
  pinMode(PIN_ESTOP_IN, INPUT_PULLUP);
  _estopPinRaw = (digitalRead(PIN_ESTOP_IN) == LOW);
  _estopDebounceMs = millis();

  /* Give both links their timeout window from boot; no heater can be
     requested before a live command arrives anyway. */
  _lastHostFrameMs = millis();
  _lastHmiFrameMs = millis();
  _duty = 0.0f;
}

// ---- interlock -----------------------------------------------------------

void HeaterSafety::service(uint32_t nowMs) {
  Trip trip = TRIP_NONE;

  const bool hmiOk = hmiLinkAlive(nowMs);
  const bool serialOk = hostLinkAlive(nowMs);
  const bool linkOk = hmiOk || serialOk;
  const bool live = (_duty > 0.0f) || _outputOn;

  /* 1. Link watchdogs: whoever has stopped talking loses heater authority.
        Reported only when the loss actually removed a live request. An HMI
        that has been seen before reports as a link loss; without an HMI
        (serial bench) the same condition is the original host timeout. */
  if (!linkOk && live) {
    _duty = 0.0f;
    trip = _hmiEverAlive ? TRIP_HMI_LINK_LOST : TRIP_HOST_TIMEOUT;
  }
  if (hmiOk) _hmiEverAlive = true;
  if (serialOk) {
    _hostWasAlive = true;
    _hostTimedOut = false;
  } else if (_hostWasAlive) {
    _hostTimedOut = true;
  }

  /* 2. Hardware E-stop input (active low), debounced. Highest reporting
        priority: it overwrites any other trip for this pass. */
  bool pressed = (digitalRead(PIN_ESTOP_IN) == LOW);
  if (pressed != _estopPinRaw) {
    _estopPinRaw = pressed;
    _estopDebounceMs = nowMs;
  }
  if (!pressed) {
    _estopPinActive = false;   // released: a later press reports again
  } else if (!_estopPinActive && (nowMs - _estopDebounceMs) >= ESTOP_DEBOUNCE_MS) {
    _estopPinActive = true;
    if (!_estopLatched) {
      _estopLatched = true;
      _duty = 0.0f;
      trip = TRIP_ESTOP_INPUT;
    }
  }

  /* 3. No valid (or no fresh) thermocouple reading, no heat. The reading is
        NAN also when RoastControl judged it stale. */
  if (isnan(_probeC) && live) {
    _duty = 0.0f;
    if (trip == TRIP_NONE) trip = TRIP_PROBE_INVALID;
  }

  if (!isnan(_probeC)) {
    /* 4. Process over-temperature (safety.max_temperature_c). Latched: only
          resetFaults() while cool re-arms it — mirrors the host supervisor. */
    if (_probeC > GEN_MAX_TEMP_C) {
      _duty = 0.0f;
      if (!_overTempLatch) {
        _overTempLatch = true;
        if (trip == TRIP_NONE) trip = TRIP_OVER_TEMP;
      }
    }

    /* 5. On-board backstop (LOCAL_MAX_TEMP_C), deliberately above the process
          limit so step 4 trips first. Auto-clears with hysteresis. */
    if (_probeC > LOCAL_MAX_TEMP_C) {
      _duty = 0.0f;
      if (!_localOverTemp) {
        _localOverTemp = true;
        if (trip == TRIP_NONE) trip = TRIP_LOCAL_OVER_TEMP;
      }
    } else if (_probeC < (LOCAL_MAX_TEMP_C - LOCAL_OVERTEMP_HYST_C)) {
      _localOverTemp = false;
    }
  }

  /* 6. Drive the pins from the decision, including the proportioning phase.
        This runs every loop pass: a probe that goes open-circuit, a pressed
        E-stop or a silent link removes heater power within one pass, whether
        or not any command ever arrives. */
  applyOutput(relayPhaseOn(nowMs) && !isnan(_probeC) && !_estopLatched &&
              !_localOverTemp && !_overTempLatch && !_rorLatch && linkOk);

  if (trip != TRIP_NONE) {
    _trip = trip;
  }
}

HeaterSafety::Trip HeaterSafety::takeTrip() {
  Trip trip = _trip;
  _trip = TRIP_NONE;
  return trip;
}

// ---- commands ------------------------------------------------------------

void HeaterSafety::setDutyRequest(float pct) {
  _duty = constrain(pct, 0.0f, 100.0f);
  if (_duty <= 0.0f) {
    applyOutput(false);
  }
}

void HeaterSafety::setHeaterRequest(bool on) {
  _duty = on ? 100.0f : 0.0f;
  if (!on) {
    applyOutput(false);
  }
}

void HeaterSafety::setFan(uint8_t percent) {
  _fanPct = percent;
  writeFan(percent);
}

void HeaterSafety::engageEstop() {
  _estopLatched = true;
  _duty = 0.0f;
  applyOutput(false);
}

void HeaterSafety::latchRunaway() {
  if (_rorLatch) return;
  _rorLatch = true;
  _duty = 0.0f;
  if (_trip == TRIP_NONE) _trip = TRIP_ROR_RUNAWAY;
  applyOutput(false);
}

bool HeaterSafety::resetFaults() {
  /* Refusal rules mirror the host supervisor (control/safety.py):
     - release the E-stop only with a valid reading at or below
       safety.estop_reset_max_c;
     - acknowledge process latches only at or below safety.reset_temperature_c;
     - E-stop first: acknowledge is refused while the E-stop is active. */
  if (digitalRead(PIN_ESTOP_IN) == LOW) {
    _refusal = "ESTOP_STILL_ACTIVE";
    return false;
  }
  const bool anyLatch = _estopLatched || _overTempLatch || _rorLatch || _localOverTemp;
  if (anyLatch && isnan(_probeC)) {
    _refusal = "NO_VALID_TEMP";
    return false;
  }
  if (_estopLatched) {
    if (_probeC > GEN_ESTOP_RESET_MAX_C) {
      _refusal = "ESTOP_TOO_HOT";
      return false;
    }
    _estopLatched = false;
    _estopPinActive = false;
  }
  if (_overTempLatch || _rorLatch || _localOverTemp) {
    if (_probeC > GEN_RESET_TEMP_C) {
      _refusal = "OVER_TEMP_LATCHED";
      return false;
    }
    _overTempLatch = false;
    _rorLatch = false;
    _localOverTemp = false;
  }
  _hostTimedOut = false;
  _refusal = nullptr;
  return true;
}

const char *HeaterSafety::resetRefusal() const { return _refusal; }

void HeaterSafety::setProbeReading(float temperatureC) { _probeC = temperatureC; }

void HeaterSafety::onHostFrame(uint32_t nowMs) { _lastHostFrameMs = nowMs; }

void HeaterSafety::onHmiFrame(uint32_t nowMs) { _lastHmiFrameMs = nowMs; }

// ---- telemetry masks -----------------------------------------------------

uint16_t HeaterSafety::latchedFaults() const {
  uint16_t bits = 0;
  if (_estopLatched) bits |= F_ESTOP;
  if (_overTempLatch) bits |= F_OVER_TEMP;
  if (_localOverTemp) bits |= F_LOCAL_OVERTEMP;
  if (_rorLatch) bits |= F_ROR_RUNAWAY;
  return bits;
}

uint16_t HeaterSafety::activeFaults(uint32_t nowMs) const {
  uint16_t bits = latchedFaults();
  if (isnan(_probeC)) bits |= F_TEMP_INVALID;
  if (!hmiLinkAlive(nowMs)) bits |= F_HMI_LINK_LOST;
  if (_hostTimedOut) bits |= F_HOST_TIMEOUT;
  return bits;
}

// ---- outputs -------------------------------------------------------------

bool HeaterSafety::relayPhaseOn(uint32_t nowMs) const {
  if (_duty <= 0.0f) return false;
  if (_duty >= 100.0f) return true;
  const uint32_t phase = nowMs % GEN_HEATER_WINDOW_MS;
  return phase < (uint32_t)(_duty / 100.0f * (float)GEN_HEATER_WINDOW_MS);
}

void HeaterSafety::applyOutput(bool on) {
  _outputOn = on;
  digitalWrite(PIN_SSR_1, on ? HIGH : LOW);
  digitalWrite(PIN_SSR_2, on ? HIGH : LOW);
}

void HeaterSafety::writeFan(uint8_t percent) {
  fanWrite((uint8_t)map(constrain((int)percent, 0, 100), 0, 100, 0, 255));
}
