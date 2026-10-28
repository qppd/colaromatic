#include "hmi_data.h"

#include <string.h>
#include <stdio.h>

HmiData hmi;

/* ---- one lock for every field above ------------------------------------ */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void hmiInit() {
  portENTER_CRITICAL(&s_mux);
  memset(&hmi, 0, sizeof(hmi));
  hmi.selProfile = LINK_PROFILE_NONE;
  hmi.selMode = MODE_AUTO;
  hmi.setpointC10 = 2000;   // 200.0 degC, inside the colour loop's universe
  hmi.manualDuty = 0;
  portEXIT_CRITICAL(&s_mux);
}

/* ---- inbound (WiFi task) ------------------------------------------------ */

void hmiOnState(const LinkState &st, uint32_t nowMs) {
  portENTER_CRITICAL(&s_mux);
  hmi.st = st;
  hmi.haveState = true;
  hmi.lastStateMs = nowMs;
  hmi.connected = true;
  hmi.stateSerial++;
  /* The controller's selected profile is authoritative once it is running:
     showing anything else would let the operator believe a different batch
     is loaded. While idle the operator's own choice is kept. */
  if (st.roastState != ROAST_IDLE && st.roastState != ROAST_DONE) {
    hmi.selProfile = st.profileIdx;
    hmi.selMode = st.mode;
  }
  portEXIT_CRITICAL(&s_mux);
}

void hmiOnAck(const LinkAck &ack, uint32_t nowMs) {
  portENTER_CRITICAL(&s_mux);
  hmi.ack = ack;
  hmi.haveAck = true;
  hmi.lastAckMs = nowMs;
  hmi.ackSerial++;
  portEXIT_CRITICAL(&s_mux);
}

void hmiOnEvent(const LinkEvent &ev, uint32_t nowMs) {
  portENTER_CRITICAL(&s_mux);
  /* Shift the log down: index 0 is always the newest. */
  const uint8_t keep = (hmi.evCount < HMI_EVENT_LOG) ? hmi.evCount
                                                     : (uint8_t)(HMI_EVENT_LOG - 1);
  for (int i = keep; i > 0; i--) {
    hmi.events[i] = hmi.events[i - 1];
  }
  HmiEvent &e = hmi.events[0];
  e.atMs = nowMs;
  e.sessionMs = ev.sessionMs;
  e.value = ev.value;
  e.type = ev.type;
  e.a = ev.a;
  e.b = ev.b;
  memset(e.text, 0, sizeof(e.text));
  strncpy(e.text, ev.text, sizeof(e.text) - 1);
  if (hmi.evCount < HMI_EVENT_LOG) hmi.evCount++;
  hmi.evSerial++;
  portEXIT_CRITICAL(&s_mux);
}

/* ---- snapshots (LVGL task) --------------------------------------------- */

void hmiSnapshot(LinkState &st, bool &connected, uint8_t &serial) {
  portENTER_CRITICAL(&s_mux);
  st = hmi.st;
  connected = hmi.connected;
  serial = hmi.stateSerial;
  portEXIT_CRITICAL(&s_mux);
}

bool hmiCopyAck(LinkAck &ack, uint8_t &serial) {
  bool have;
  portENTER_CRITICAL(&s_mux);
  have = hmi.haveAck;
  ack = hmi.ack;
  serial = hmi.ackSerial;
  portEXIT_CRITICAL(&s_mux);
  return have;
}

uint8_t hmiCopyEvents(HmiEvent *out, uint8_t maxOut) {
  if (out == nullptr || maxOut == 0) return 0;
  portENTER_CRITICAL(&s_mux);
  const uint8_t n = (hmi.evCount < maxOut) ? hmi.evCount : maxOut;
  for (uint8_t i = 0; i < n; i++) out[i] = hmi.events[i];
  portEXIT_CRITICAL(&s_mux);
  return n;
}

void hmiSetConnected(bool connected) {
  portENTER_CRITICAL(&s_mux);
  hmi.connected = connected;
  portEXIT_CRITICAL(&s_mux);
}

/* ---- operator selection ------------------------------------------------- */

void hmiOperatorSnapshot(uint8_t &profile, uint8_t &mode, int16_t &setpointC10,
                         uint8_t &manualDuty) {
  portENTER_CRITICAL(&s_mux);
  profile = hmi.selProfile;
  mode = hmi.selMode;
  setpointC10 = hmi.setpointC10;
  manualDuty = hmi.manualDuty;
  portEXIT_CRITICAL(&s_mux);
}

void hmiSelectProfile(uint8_t idx) {
  portENTER_CRITICAL(&s_mux);
  hmi.selProfile = idx;
  portEXIT_CRITICAL(&s_mux);
}

void hmiSetMode(uint8_t mode) {
  portENTER_CRITICAL(&s_mux);
  hmi.selMode = mode;
  portEXIT_CRITICAL(&s_mux);
}

void hmiSetSetpointC10(int16_t c10) {
  portENTER_CRITICAL(&s_mux);
  hmi.setpointC10 = c10;
  portEXIT_CRITICAL(&s_mux);
}

void hmiSetManualDuty(uint8_t pct) {
  portENTER_CRITICAL(&s_mux);
  hmi.manualDuty = pct;
  portEXIT_CRITICAL(&s_mux);
}

/* ---- text --------------------------------------------------------------- */

const char *hmiRoastStateText(uint8_t s) {
  switch (s) {
    case ROAST_IDLE: return "IDLE";
    case ROAST_RUNNING: return "ROASTING";
    case ROAST_HOLD: return "HOLD";
    case ROAST_COOLING: return "COOLING";
    case ROAST_DONE: return "DONE";
    default: return "UNKNOWN";
  }
}

const char *hmiStageText(uint8_t s) {
  switch (s) {
    case STAGE_CHARGE: return "CHARGE";
    case STAGE_TURNING_POINT: return "TURNING POINT";
    case STAGE_COLOR_CHANGE: return "COLOUR CHANGE";
    case STAGE_FIRST_CRACK: return "1ST CRACK";
    case STAGE_MEDIUM_ROAST: return "MEDIUM ROAST";
    case STAGE_SECOND_CRACK: return "2ND CRACK";
    case STAGE_DARK_ROAST: return "DARK ROAST";
    case STAGE_DONE: return "DONE";
    default: return "UNKNOWN";
  }
}

const char *hmiStageMethodText(uint8_t m) {
  switch (m) {
    case METHOD_OPERATOR: return "operator";
    case METHOD_TEMP_MINIMUM: return "temperature";
    case METHOD_COLOR_TREND: return "colour";
    case METHOD_PROFILE_ENDPOINT: return "profile";
    case METHOD_ACOUSTIC: return "acoustic";
    case METHOD_SESSION: return "session";
    default: return "unknown";
  }
}

const char *hmiModeText(uint8_t m) {
  return (m == MODE_AUTO) ? "AUTOMATIC" : "MANUAL";
}

const char *hmiTempQualityText(uint8_t q) {
  switch (q) {
    case TEMP_OK: return "ok";
    case TEMP_DEGRADED: return "degraded";
    case TEMP_INVALID: return "invalid";
    case TEMP_STALE: return "stale";
    default: return "unknown";
  }
}

const char *hmiColorStatusText(uint8_t c) {
  switch (c) {
    case COLOR_UNCALIBRATED: return "uncalibrated";
    case COLOR_PROVISIONAL: return "provisional";
    case COLOR_VALIDATED: return "validated";
    case COLOR_INVALID: return "invalid";
    default: return "unknown";
  }
}

const char *hmiEventText(uint8_t type) {
  switch (type) {
    case EV_STAGE_CHANGE: return "STAGE";
    case EV_FAULT_TRIP: return "FAULT";
    case EV_COOLING_START: return "COOLING";
    case EV_SESSION_END: return "END";
    case EV_ENDPOINT_PENDING: return "ENDPOINT";
    case EV_CRACK: return "CRACK";
    case EV_SESSION_START: return "START";
    case EV_ROAST_HEARTBEAT: return "HEARTBEAT";
    case EV_COOLING_UNVERIFIED: return "COOL?";
    case EV_HEATER_COMMAND: return "HEATER";
    default: return "EVENT";
  }
}

struct HmiFaultName {
  uint16_t bit;
  const char *name;
};

static const HmiFaultName FAULT_NAMES[] = {
    {F_TEMP_INVALID, "TEMP_INVALID"},
    {F_TEMP_STALE, "TEMP_STALE"},
    {F_OVER_TEMP, "OVER_TEMP"},
    {F_LOCAL_OVERTEMP, "LOCAL_OVERTEMP"},
    {F_ROR_RUNAWAY, "ROR_RUNAWAY"},
    {F_ESTOP, "ESTOP"},
    {F_HMI_LINK_LOST, "HMI_LINK_LOST"},
    {F_ENOSE_HOT, "ENOSE_HOT"},
    {F_HOST_TIMEOUT, "HOST_TIMEOUT"},
};

void hmiFaultText(uint16_t mask, char *out, size_t n) {
  if (out == nullptr || n == 0) return;
  out[0] = '\0';
  if (mask == 0) return;
  size_t used = 0;
  for (size_t i = 0; i < sizeof(FAULT_NAMES) / sizeof(FAULT_NAMES[0]); i++) {
    if ((mask & FAULT_NAMES[i].bit) == 0) continue;
    const char *sep = (used == 0) ? "" : ",";
    const int written = snprintf(out + used, n - used, "%s%s", sep,
                                 FAULT_NAMES[i].name);
    if (written < 0) break;
    used += (size_t)written;
    if (used >= n) {  // truncated
      out[n - 1] = '\0';
      break;
    }
  }
}

bool hmiFaultIsWarning(uint16_t bit) { return bit == F_ENOSE_HOT; }
