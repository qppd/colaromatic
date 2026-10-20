#pragma once

#include <Arduino.h>

#include "CONF.h"
#include "PINS.h"
#include "LinkFrames.h"

/*
 * HeaterSafety — actuator outputs and every on-board interlock.
 *
 * This class owns the SSR pins, the fan output, the E-stop input, the link
 * watchdogs and the fault latches. service() must be called on EVERY loop()
 * pass: it is the single place that decides whether the heater may be
 * energised, and it drives the pins from that decision — including the
 * time-proportioning phase — so an interlock takes effect within one loop
 * pass of the condition becoming visible, without waiting for any command.
 *
 * Heater authority comes from either link:
 *   - the HMI (ESP-NOW): the normal operator path. Silence for
 *     LINK_TIMEOUT_MS removes authority (TRIP_HMI_LINK_LOST).
 *   - the USB serial service link: kept for bench work with no HMI present.
 *     Silence for HOST_TIMEOUT_MS removes authority (TRIP_HOST_TIMEOUT).
 *
 * Trip policy (unchanged from the serial-only design): a trip clears the duty
 * request as well as driving the output low, so recovery always needs a fresh
 * command from a live operator. Latches (E-stop, over-temperature, RoR
 * runaway) additionally require HeaterSafety::resetFaults() while the process
 * is cool, which mirrors the host supervisor's release/acknowledge rules.
 *
 * This is a backstop, not the independent protection required by
 * REQ-SAFE-001: a separately wired thermal cutoff must still remove heater
 * power without this firmware.
 */
class HeaterSafety {
 public:
  enum Trip {
    TRIP_NONE = 0,
    TRIP_HOST_TIMEOUT,     // no recognized serial frame for HOST_TIMEOUT_MS
    TRIP_HMI_LINK_LOST,    // no HMI frame for LINK_TIMEOUT_MS
    TRIP_ESTOP_INPUT,      // E-stop input held low
    TRIP_LOCAL_OVER_TEMP,  // on-board backstop (CONF.h LOCAL_MAX_TEMP_C)
    TRIP_OVER_TEMP,        // process limit (GenConfig GEN_MAX_TEMP_C), latched
    TRIP_ROR_RUNAWAY,      // RoR runaway, latched by RoastControl
    TRIP_PROBE_INVALID,    // thermocouple reading unavailable or stale
  };

  void begin();

  /* Interlocks, proportioning and outputs. service() drives the pins; the
     setters only record intent so that a later interlock evaluation can
     override it. */
  void service(uint32_t nowMs);

  /* Duty request 0..100 %, already shaped (ceiling, minimum on/off) by the
     caller. service() turns it into a zero-cross time-proportioned phase. */
  void setDutyRequest(float pct);
  /* Raw on/off for the serial service path: duty 100/0. The serial host does
     its own time-proportioning by toggling, so no phase is imposed here. */
  void setHeaterRequest(bool on);
  void setFan(uint8_t percent);
  void engageEstop();       // software E-stop (HMI command)
  bool resetFaults();       // refused while latched and hot
  const char *resetRefusal() const;

  /* Feed the latest thermocouple reading (NAN when invalid or stale). Used
     for the "no valid probe, no heat" interlock and both backstops. */
  void setProbeReading(float temperatureC);

  /* Link supervision. onHmiFrame refreshes the ESP-NOW watchdog; onHostFrame
     refreshes the USB serial watchdog (recognized commands only). */
  void onHmiFrame(uint32_t nowMs);
  void onHostFrame(uint32_t nowMs);

  /* RoastControl detects the runaway (it owns the RoR estimate). */
  void latchRunaway();

  /* One-shot: returns the trip that began since the previous call. */
  Trip takeTrip();

  /* State, for telemetry and diagnostics. */
  bool heaterOutput() const { return _outputOn; }
  bool heaterRequested() const { return _duty > 0.0f; }
  float dutyRequest() const { return _duty; }
  uint8_t fanPercent() const { return _fanPct; }
  bool estopLatched() const { return _estopLatched; }
  bool localOverTemp() const { return _localOverTemp; }
  bool overTempLatched() const { return _overTempLatch; }
  bool runawayLatched() const { return _rorLatch; }
  bool hostLinkAlive(uint32_t nowMs) const {
    return (nowMs - _lastHostFrameMs) < HOST_TIMEOUT_MS;
  }
  bool hmiLinkAlive(uint32_t nowMs) const {
    return (nowMs - _lastHmiFrameMs) < LINK_TIMEOUT_MS;
  }
  bool linkAlive(uint32_t nowMs) const {
    return hmiLinkAlive(nowMs) || hostLinkAlive(nowMs);
  }

  /* Telemetry masks in LinkFrames.h numbering. Latched conditions are also
     reported as active, matching the host supervisor's `active` list. */
  uint16_t activeFaults(uint32_t nowMs) const;
  uint16_t latchedFaults() const;

 private:
  void applyOutput(bool on);
  void writeFan(uint8_t percent);
  bool relayPhaseOn(uint32_t nowMs) const;

  float _duty = 0.0f;       // requested duty; cleared by every trip
  bool _outputOn = false;   // actual pin state
  bool _estopLatched = false;   // input or HMI command; cleared by resetFaults
  bool _estopPinRaw = false;    // raw input level (true = low = pressed)
  bool _estopPinActive = false; // debounced pressed
  bool _localOverTemp = false;  // on-board backstop, hysteresis-cleared
  bool _overTempLatch = false;  // process limit, cleared only by resetFaults
  bool _rorLatch = false;       // RoR runaway, cleared only by resetFaults
  bool _hmiEverAlive = false;   // distinguishes HMI loss from a serial-only bench
  bool _hostWasAlive = false;   // a serial host has recently been active
  bool _hostTimedOut = false;   // serial watchdog fired while that host was live
  float _probeC = NAN;          // NAN until the first valid reading
  uint8_t _fanPct = 0;
  uint32_t _lastHostFrameMs = 0;
  uint32_t _lastHmiFrameMs = 0;
  uint32_t _estopDebounceMs = 0;
  Trip _trip = TRIP_NONE;
  const char *_refusal = nullptr;
};
