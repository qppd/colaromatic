/*
 * ColAromatic — ESP32 machine controller (PROVISIONAL FIRMWARE)
 *
 * Arduino IDE sketch. The machine runs standalone: the roast control loop,
 * safety supervisor and session recording run on THIS board, and the operator
 * talks to it from the HMI touch panel over ESP-NOW. There is no PC in the
 * machine (docs/02_architecture.md). The USB serial link is kept as a
 * service/bench port only.
 *
 *   ColAromatic.ino   this file — setup()/loop() orchestration
 *   PINS.h            every GPIO assignment (single source of truth)
 *   CONF.h            firmware-only timing/threshold constants
 *   GenConfig.h       GENERATED from src/config/system.json         (do not edit)
 *   GenFuzzy.h        GENERATED from src/config/controllers/*.json  (do not edit)
 *   GenProfiles.h     GENERATED from src/config/profiles/*.json     (do not edit)
 *   HeaterSafety.*    heater/fan SSR outputs, E-stop, both link watchdogs, latches
 *   RoastControl.*    session states, RoR, stages, endpoint, duty shaping
 *   Fuzzy.*           Mamdani inference (same algorithm as the host tooling)
 *   ColorSetpoint.*   fuzzy colour error -> temperature setpoint (outer loop)
 *   Pid.*             PID temperature -> heater duty (inner loop)
 *   HmiLink.*         ESP-NOW link to the touch panel (LinkFrames.h)
 *   HostLink.*        the ASCII serial protocol (docs/05_serial_protocol.md)
 *   ThermocoupleSensor.*  MAX6675 K-type probe
 *   ColorSensor.*     TCS3200 light-to-frequency
 *   EnoseSensor.*     BME688 gas sensor
 *   AcousticSensor.*  envelope microphone / crack detection
 *
 * SAFETY NOTES — read before energising anything
 *  - This firmware is NOT the independent protection required for the heater
 *    circuit (REQ-SAFE-001). A separately wired thermal cutoff or mechanical
 *    thermostat must remove heater power without the ESP32.
 *  - Every interlock is evaluated on every loop pass: an invalid thermocouple,
 *    a stale reading, an E-stop, over-temperature, an RoR runaway or silence
 *    from BOTH links removes heater power within one pass.
 *  - A trip clears the duty request as well as the output, so recovery needs
 *    a fresh command from a live operator (and latches need a cool reset).
 *  - Heater and fan each have their own SSR. The heater uses on/off
 *    time-proportioning computed HERE, phase-aligned to millis(). The fan SSR
 *    is on/off only: no PWM, so any non-zero fan request runs it at full
 *    speed. Phase-angle or random-fire SSRs must NOT be driven this way.
 *  - Every threshold is provisional and REQUIRES_CALIBRATION; treat a serial
 *    monitor session with no mains connected as the only safe first test.
 */

#include <Arduino.h>

#include "PINS.h"
#include "CONF.h"
#include "ThermocoupleSensor.h"
#include "ColorSensor.h"
#include "EnoseSensor.h"
#include "AcousticSensor.h"
#include "HeaterSafety.h"
#include "HostLink.h"
#include "HmiLink.h"
#include "RoastControl.h"
#include "ColorSetpoint.h"

HeaterSafety heaterSafety;
HostLink hostLink;
HmiLink hmiLink;
RoastControl roast;
ThermocoupleSensor thermocouple;
ColorSensor colorSensor;
EnoseSensor enoseSensor;
AcousticSensor acousticSensor;

uint32_t lastReportMs = 0;
bool reportNow = false;
bool sessionWasLive = false;

/* ---- helpers ------------------------------------------------------------ */

const char *tripCodeText(HeaterSafety::Trip trip) {
  switch (trip) {
    case HeaterSafety::TRIP_HOST_TIMEOUT:
      return "HOST_TIMEOUT";
    case HeaterSafety::TRIP_HMI_LINK_LOST:
      return "HMI_LINK_LOST";
    case HeaterSafety::TRIP_ESTOP_INPUT:
      return "ESTOP_INPUT";
    case HeaterSafety::TRIP_LOCAL_OVER_TEMP:
      return "LOCAL_OVER_TEMP";
    case HeaterSafety::TRIP_OVER_TEMP:
      return "OVER_TEMPERATURE";
    case HeaterSafety::TRIP_ROR_RUNAWAY:
      return "ROR_RUNAWAY";
    case HeaterSafety::TRIP_PROBE_INVALID:
      return "TC_INVALID_HEATER_OFF";
    default:
      return "UNKNOWN_TRIP";
  }
}

static bool sessionLive() {
  const uint8_t s = roast.roastState();
  return s == ROAST_RUNNING || s == ROAST_HOLD || s == ROAST_COOLING;
}

static void abortSessionForTrip(uint32_t now, HeaterSafety::Trip trip) {
  const char *code = tripCodeText(trip);
  char reason[28];
  snprintf(reason, sizeof(reason), "safety:%s", code);
  const char *status =
      (trip == HeaterSafety::TRIP_ESTOP_INPUT) ? "aborted_estop" : "aborted_safety";
  hmiLink.sendEvent(EV_FAULT_TRIP, (uint8_t)trip, 0, 0, roast.sessionMs(now), reason);
  roast.abortToCooling(reason, status);
}

static void reportFaultEverywhere(const char *code) {
  hostLink.reportFault(code);
  hmiLink.sendEvent(EV_FAULT_TRIP, 0, 0, 0, roast.sessionMs(millis()), code);
}

/* ---- serial service path (bench, no HMI) -------------------------------- */

void dispatchSerial(const Command &cmd) {
  switch (cmd.type) {
    case Command::SET_HEATER:
      if (sessionLive()) {           // the onboard session owns the heater
        hostLink.reportFault("SESSION_ACTIVE");
        break;
      }
      heaterSafety.setHeaterRequest(cmd.heaterOn);
      break;

    case Command::SET_FAN:
      if (sessionLive()) {
        hostLink.reportFault("SESSION_ACTIVE");
        break;
      }
      heaterSafety.setFan(cmd.fanPct);
      break;

    case Command::SET_AUDIO:
      acousticSensor.configure(cmd.audioThreshold > 0, cmd.audioThreshold);
      break;

    case Command::CMD_ESTOP:
      heaterSafety.engageEstop();
      roast.abortToCooling("safety:serial_estop", "aborted_estop");
      reportFaultEverywhere("ESTOP_ENGAGED");
      break;

    case Command::CMD_RESET:
      if (heaterSafety.resetFaults()) {
        acousticSensor.resetCrackState();
        hostLink.reportFault("RESET_OK");
      } else {
        hostLink.reportFault(heaterSafety.resetRefusal());
      }
      break;

    case Command::PING:
      hostLink.reportStatus(millis(), heaterSafety.hostLinkAlive(millis()));
      break;

    case Command::POLL:
      reportNow = true;
      break;

    case Command::IGNORED:
      break;  // reserved: accepted so it is not reported as an error

    default:
      hostLink.reportFault("UNKNOWN_COMMAND");
      break;
  }
}

/* ---- HMI command path --------------------------------------------------- */

void dispatchHmi(const HmiLink::Command &hc, uint32_t now) {
  const LinkCmd &cmd = hc.cmd;
  const char *text = "OK";
  uint8_t result = ACK_OK;
  bool ackAfterSend = true;  // OP_PAIR reconfigures the link: ack first

  switch (cmd.opcode) {
    case OP_START: {
      const uint16_t latched = heaterSafety.latchedFaults();
      if (latched != 0) {
        result = ACK_GATING;
        text = "SAFETY_LATCHED";
      } else {
        result = roast.start(cmd.a, cmd.b, text);
      }
      break;
    }
    case OP_HOLD:
      result = roast.hold(text);
      break;
    case OP_RESUME:
      result = roast.resume(text);
      break;
    case OP_COOL:
      result = roast.cool(text, "stopped");
      break;

    case OP_ESTOP:
      heaterSafety.engageEstop();
      roast.abortToCooling("safety:operator_estop", "aborted_estop");
      hmiLink.sendEvent(EV_FAULT_TRIP, F_ESTOP, 0, 0, roast.sessionMs(now),
                        "OPERATOR_ESTOP");
      text = "OK";
      break;

    case OP_RESET:
      if (heaterSafety.resetFaults()) {
        acousticSensor.resetCrackState();
        text = "OK";
      } else {
        text = heaterSafety.resetRefusal();
        result = (strcmp(text, "ESTOP_STILL_ACTIVE") == 0) ? ACK_STATE : ACK_GATING;
      }
      break;

    case OP_SET_DUTY:
      result = roast.setDuty(cmd.a, text);
      break;
    case OP_SET_MODE:
      result = roast.setMode(cmd.a, text);
      break;
    case OP_SELECT_PROFILE:
      result = roast.selectProfile(cmd.a, text);
      break;
    case OP_SET_SETPOINT:
      result = roast.setSetpoint(cmd.b, text);
      break;
    case OP_CONFIRM_STAGE:
      result = roast.confirmStage(cmd.a, text);
      break;
    case OP_CONFIRM_ENDPOINT:
      result = roast.confirmEndpoint(text);
      break;

    case OP_SET_FAN:
      if (sessionLive()) {
        result = ACK_STATE;
        text = "SESSION_ACTIVE";
      } else {
        heaterSafety.setFan(cmd.a);
        text = "OK";
      }
      break;

    case OP_PAIR:
      if (cmd.a == HMI_PAIR_MAGIC) {
        /* Acknowledge first: opening pairing drops the peer we would answer
           on. The HMI clears its own stored peer on the same command. */
        hmiLink.sendAck(hc.seq, ACK_OK, cmd.opcode,
                        heaterSafety.activeFaults(now) | roast.extraFaults(),
                        "PAIRING_OPEN");
        hmiLink.openPairing(now);
        ackAfterSend = false;
      } else {
        result = ACK_ARG;
        text = "BAD_PAIR_MAGIC";
      }
      break;

    default:
      result = ACK_ARG;
      text = "UNKNOWN_OPCODE";
      break;
  }

  if (ackAfterSend) {
    hmiLink.sendAck(hc.seq, result, cmd.opcode,
                    heaterSafety.activeFaults(now) | roast.extraFaults(), text);
  }
}

/* ---- telemetry ---------------------------------------------------------- */

void reportTelemetry(uint32_t now) {
  /* The BME688 reading blocks for roughly 200 ms: it belongs here, once per
     report, and never in the interlock path. */
  enoseSensor.update(now);

  SensorFrame frame;
  frame.temperatureC = thermocouple.temperatureC();
  frame.temperatureFlag = thermocouple.flag();
  frame.redHz = colorSensor.redHz();
  frame.greenHz = colorSensor.greenHz();
  frame.blueHz = colorSensor.blueHz();
  frame.clearHz = colorSensor.clearHz();
  frame.gasOhm = enoseSensor.gasOhm();
  frame.sensorTempC = enoseSensor.sensorTempC();
  frame.humidityPct = enoseSensor.humidityPct();
  frame.pressureHpa = enoseSensor.pressureHpa();
  frame.enoseFlag = enoseSensor.flag();
  frame.enoseWarmup = enoseSensor.warmupComplete();

  hostLink.reportSensors(frame, now, heaterSafety.hostLinkAlive(now));
}

void buildState(LinkState &st, uint32_t now) {
  memset(&st, 0, sizeof(st));
  st.uptimeMs = now;
  st.sessionMs = roast.sessionMs(now);

  const float t = roast.filteredTempC();
  const float ror = roast.rorCpm();
  const float sp = roast.setpointC();
  st.tempC10 = isnan(t) ? INT16_MIN : (int16_t)(t * 10.0f);
  st.rorCpm10 = isnan(ror) ? INT16_MIN : (int16_t)(ror * 10.0f);
  st.setpointC10 = isnan(sp) ? INT16_MIN : (int16_t)(sp * 10.0f);
  st.tempQuality = roast.tempQuality();
  st.roastState = roast.roastState();
  st.mode = roast.mode();
  st.stage = roast.stage();
  st.stageMethod = roast.stageMethod();
  st.stageConfirmed = roast.stageConfirmed() ? 1 : 0;
  st.dutyCmd = (uint8_t)(roast.dutyCmd() + 0.5f);
  st.dutyApp = (uint8_t)(roast.dutyApplied() + 0.5f);
  st.fanPct = heaterSafety.fanPercent();
  st.relayOn = heaterSafety.heaterOutput() ? 1 : 0;
  st.profileIdx = roast.profileIdx();
  st.endpointPending = roast.endpointPending() ? 1 : 0;
  st.faultsActive = heaterSafety.activeFaults(now) | roast.extraFaults();
  st.faultsLatched = heaterSafety.latchedFaults();
  st.clearHz = colorSensor.clearHz();

  const float gas = enoseSensor.gasOhm();
  st.gasOhmMilli = isnan(gas) ? INT32_MIN : (int32_t)(gas * 1000.0f);

  const uint32_t clear = colorSensor.clearHz();
  st.colorStatus = (clear < (uint32_t)GEN_COLOR_MIN_HZ ||
                    clear > (uint32_t)GEN_COLOR_MAX_HZ)
                       ? COLOR_INVALID
                       : COLOR_UNCALIBRATED;  // no CIELAB on the MCU (docs/13)
}

/* ---- sketch ------------------------------------------------------------- */

void setup() {
  Serial.begin(SERIAL_BAUD);
  hostLink.begin(Serial, heaterSafety);
  heaterSafety.begin();
  roast.begin();

  colorSensor.begin();
  acousticSensor.begin();
  thermocouple.begin();
  enoseSensor.begin();

  if (!hmiLink.begin()) {
    hostLink.reportFault("ESP_NOW_INIT_FAILED");
  }

  hostLink.emitBanner();

  if (const char *code = thermocouple.takeFault()) {
    hostLink.reportFault(code);
  }
  if (const char *code = enoseSensor.takeFault()) {
    hostLink.reportFault(code);
  }
}

void loop() {
  uint32_t now = millis();

  /* 1. Serial service commands. A recognized command refreshes the serial
        watchdog; anything else leaves it running so noise cannot keep a dead
        host alive. */
  Command cmd;
  while (hostLink.readCommand(cmd)) {
    dispatchSerial(cmd);
  }

  /* 2. HMI link: proof of life first, then queued commands. */
  if (hmiLink.linkOk(now)) {
    heaterSafety.onHmiFrame(now);
  }
  HmiLink::Command hc;
  while (hmiLink.takeCommand(hc)) {
    dispatchHmi(hc, now);
  }

  /* 3. Probe first, then every interlock in one place. This is what removes
        heater power on a fault, on every single pass of the loop. A reading
        the roast controller judges stale is fed as NAN so the probe interlock
        covers staleness too. */
  thermocouple.update(now);
  const bool stale = (roast.tempQuality() == TEMP_STALE);
  heaterSafety.setProbeReading(stale ? NAN : thermocouple.temperatureC());
  heaterSafety.service(now);

  /* 4. One-shot fault codes: once per outage or command, never a flood. */
  HeaterSafety::Trip trip = heaterSafety.takeTrip();
  if (trip != HeaterSafety::TRIP_NONE) {
    const char *code = tripCodeText(trip);
    hostLink.reportFault(code);
    abortSessionForTrip(now, trip);
  }
  if (const char *code = thermocouple.takeFault()) {
    reportFaultEverywhere(code);
  }
  if (const char *code = enoseSensor.takeFault()) {
    reportFaultEverywhere(code);
  }

  /* 5. Acoustic events: serial debug frame first (host parser contract),
        then the onboard stage detector. */
  colorSensor.service(now);
  acousticSensor.service(now);

  CrackEvent crack;
  if (acousticSensor.takeEvent(crack)) {
    hostLink.emitCrack(crack.kind, crack.confidence, now);
    roast.notifyAcoustic(crack.kind, crack.confidence);
  }

  /* 6. The onboard session: temperature processing, stages, endpoint, control
        tick, cooling. Runs every pass; internal ticks are period-gated. */
  RoastControl::Sample sample;
  sample.nowMs = now;
  sample.tempC = thermocouple.temperatureC();
  sample.tempFlag = thermocouple.flag();
  sample.gasOhm = enoseSensor.gasOhm();
  sample.colorRatio = ColorSetpoint::browningRatio(
      colorSensor.redHz(), colorSensor.greenHz(), colorSensor.blueHz());
  const float enoseTemp = enoseSensor.sensorTempC();
  sample.enoseHot = !isnan(enoseTemp) && enoseTemp > GEN_ENOSE_MAX_C;
  roast.service(sample);

  if (roast.consumeRunaway()) {
    heaterSafety.latchRunaway();
    abortSessionForTrip(now, HeaterSafety::TRIP_ROR_RUNAWAY);
  }

  /* 7. Outputs. While a session is live the onboard controller is the single
        writer of duty and fan; the serial path may only drive them when no
        session is running (dispatch enforces that). */
  const uint8_t rs = roast.roastState();
  const bool live = sessionLive();
  if (live) {
    heaterSafety.setDutyRequest(rs == ROAST_RUNNING ? roast.dutyApplied() : 0.0f);
    heaterSafety.setFan(rs == ROAST_COOLING ? (uint8_t)GEN_COOLING_FAN_PCT
                                            : (uint8_t)GEN_ROAST_FAN_PCT);
  } else if (sessionWasLive) {
    /* Session just finished (cooling complete): fan off, heater already 0. */
    heaterSafety.setFan(0);
  }
  sessionWasLive = live;

  /* 8. Session events (stage, faults, cooling, heater commands) -> HMI SD log. */
  RoastControl::Event ev;
  while (roast.takeEvent(ev)) {
    hmiLink.sendEvent(ev.type, ev.a, ev.b, ev.value, roast.sessionMs(now), ev.text);
  }

  /* 9. Telemetry at the report cadence (or immediately after CMD,POLL). */
  if (reportNow || (now - lastReportMs) >= REPORT_PERIOD_MS) {
    lastReportMs = now;
    reportNow = false;
    reportTelemetry(now);
    digitalWrite(PIN_STATUS_LED,
                 heaterSafety.estopLatched() ? HIGH : !digitalRead(PIN_STATUS_LED));
  }

  /* 10. HMI state frames (service() rate-limits to LINK_STATE_PERIOD_MS). */
  LinkState st;
  buildState(st, now);
  hmiLink.service(now, st);
}
