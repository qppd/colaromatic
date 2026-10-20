#pragma once

#include <Arduino.h>

#include "CONF.h"
#include "Fuzzy.h"
#include "GenProfiles.h"
#include "LinkFrames.h"

/*
 * RoastControl — the onboard roast session (no PC in the loop).
 *
 * A port of the host engine's session semantics (src/colaromatic/app/engine.py)
 * onto the machine controller, so the roaster behaves the same way when it is
 * driven only from the HMI touch panel over ESP-NOW:
 *
 *   - states: IDLE -> RUNNING (manual duty or automatic fuzzy control)
 *             -> COOLING -> DONE, with HOLD as a duty-zero flag inside RUNNING
 *   - temperature processing: EMA filter and regression Rate of Rise over the
 *     configured window, with the same staleness rule as the host
 *   - stage detection: turning point from the temperature minimum rebound,
 *     acoustic first/second crack (estimated, needs operator confirmation),
 *     operator-confirmed stages; DONE on cooling completion
 *   - endpoint: maximum duration stops immediately; automatic criteria (which
 *     need measured colour calibration the MCU does not have) always ask the
 *     operator, exactly like the host does for unvalidated evidence
 *   - duty shaping: ceiling, minimum duty and minimum on/off quantisation
 *     (mirror of control/heater.py::HeaterOutput.command)
 *
 * Safety is NOT here: interlocks, latches and the link watchdogs live in
 * HeaterSafety and remove heater power whatever this class decides.
 *
 * Timing values come from GenConfig.h (baked from src/config/system.json) and
 * profile data from GenProfiles.h (baked from src/config/profiles/*.json).
 */
class RoastControl {
 public:
  /* One pass worth of sensor input, assembled by the sketch. */
  struct Sample {
    uint32_t nowMs = 0;
    float tempC = NAN;       // raw probe reading, NAN when invalid
    uint8_t tempFlag = 2;    // 0 ok, 1 degraded, 2 invalid
    float gasOhm = NAN;
    bool enoseHot = false;   // warning: BME688 above its limit
  };

  /* Session event for the HMI SD log, drained by the sketch. */
  struct Event {
    uint8_t type = 0;   // LinkEventType
    uint8_t a = 0;
    uint8_t b = 0;
    uint16_t value = 0;
    char text[28] = {0};
  };

  void begin();
  void service(const Sample &sample);

  /* Operator commands. Return an LinkAckResult and fill `text` with a
     refusal reason the HMI can show verbatim. Safety gating (latches,
     interlocks) is checked by the sketch before these are called. */
  uint8_t start(uint8_t profileIdx, int16_t setpoint10, const char *&text);
  uint8_t hold(const char *&text);
  uint8_t resume(const char *&text);
  uint8_t cool(const char *&text, const char *status);
  uint8_t setDuty(uint8_t pct, const char *&text);
  uint8_t setMode(uint8_t mode, const char *&text);
  uint8_t selectProfile(uint8_t idx, const char *&text);
  uint8_t setSetpoint(int16_t setpoint10, const char *&text);
  uint8_t confirmStage(uint8_t stage, const char *&text);
  uint8_t confirmEndpoint(const char *&text);
  void notifyAcoustic(const char *kind, float confidence);

  /* Safety path: a HeaterSafety trip while a session is live moves the
     session to cooling with the engine's abort status. No ack: the trip is
     already being reported by the heater link. */
  void abortToCooling(const char *reason, const char *status);

  /* RoR runaway detected here, applied by the sketch to the heater latches. */
  bool consumeRunaway();

  /* Telemetry. */
  uint8_t roastState() const;
  uint8_t mode() const { return _mode; }
  uint8_t stage() const { return _stage; }
  uint8_t stageMethod() const { return _stageMethod; }
  bool stageConfirmed() const { return _stageConfirmed; }
  uint8_t profileIdx() const { return _profileIdx; }
  bool endpointPending() const { return _endpointPending; }
  bool holdActive() const { return _hold; }
  float dutyCmd() const { return _dutyCmd; }
  float dutyApplied() const { return _dutyApp; }
  float setpointC() const { return _setpointC; }
  float filteredTempC() const { return _filtC; }
  float rorCpm() const { return _rorValid ? _ror : NAN; }
  uint8_t tempQuality() const { return _quality; }
  uint16_t extraFaults() const;
  uint32_t sessionMs(uint32_t nowMs) const;
  bool sessionActive() const { return _state != ROAST_IDLE; }

  bool takeEvent(Event &out);

 private:
  static int stageIdx(uint8_t stage) { return (int)stage; }
  static void copyText(char *dst, size_t n, const char *src);

  void onSensorTick(const Sample &sample);
  void onControlTick(const Sample &sample);
  void advanceStage(uint8_t stage, uint8_t method, bool estimated,
                    float confidence, bool requiresConfirmation,
                    uint32_t nowMs, const char *text, bool confirmed);
  void pushEvent(uint8_t type, uint8_t a, uint8_t b, uint16_t value,
                 const char *text);
  float targetAt(float elapsedS) const;
  float dutyCeiling() const;
  float shapeDuty(float requested) const;
  void updateTemperature(const Sample &sample);
  void updateRoR(uint32_t nowMs);
  void checkStage(const Sample &sample, float elapsedS);
  void checkEndpoint(const Sample &sample, float elapsedS);
  void beginCooling(uint32_t nowMs, const char *reason, const char *status);

  /* session */
  uint32_t _nowMs = 0;
  char _endReason[28] = {0};
  uint8_t _endStatus = 0;   // 0 complete, 1 stopped, 2 aborted_safety, 3 aborted_estop
  uint8_t _state = ROAST_IDLE;
  uint8_t _mode = MODE_MANUAL;
  uint8_t _profileIdx = LINK_PROFILE_NONE;
  bool _hold = false;
  bool _endpointPending = false;
  uint32_t _sessionStartMs = 0;
  uint32_t _coolingStartMs = 0;
  bool _coolingTempOk = false;
  uint32_t _lastSensorMs = 0;
  uint32_t _lastControlMs = 0;

  /* operator inputs */
  float _manualDuty = 0.0f;
  float _setpointC = NAN;
  float _dutyCeiling = GEN_DUTY_CEILING_PCT;
  float _profileMaxDuty = NAN;

  /* temperature processing */
  float _filtC = NAN;
  uint8_t _quality = TEMP_INVALID;
  uint32_t _lastValidTempMs = 0;
  bool _enoseHot = false;
  struct RingEntry { uint32_t tMs; float c; };
  static const int RING_MAX = 40;   // > GEN_ROR_WINDOW_S / sensor period
  RingEntry _ring[RING_MAX];
  int _ringCount = 0;               // number of entries, capped at RING_MAX
  int _ringNext = 0;
  float _ror = NAN;
  bool _rorValid = false;
  uint8_t _rorStrikes = 0;
  bool _runaway = false;

  /* stage detection */
  float _minTempC = NAN;
  uint32_t _minTempAtMs = 0;
  uint8_t _stage = STAGE_CHARGE;
  uint8_t _stageMethod = METHOD_SESSION;
  bool _stageConfirmed = false;
  float _lastConfidence = 0.0f;

  /* heater */
  float _dutyCmd = 0.0f;
  float _dutyApp = 0.0f;
  float _lastLoggedDuty = -1000.0f;

  /* events */
  static const int EVQ_MAX = 6;
  Event _evq[EVQ_MAX];
  int _evHead = 0;
  int _evCount = 0;

  FuzzyController _fuzzy;
};
