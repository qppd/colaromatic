/*
 * hmi_data.h — the HMI's copy of the machine state.
 *
 * Everything the screens draw comes from here. The radio task (ESP-NOW receive
 * callback) writes it, the LVGL task reads it; every write and every snapshot
 * goes through one mutex so a screen can never see a half-updated struct.
 *
 * Nothing here is authoritative for the machine: the controller owns the roast
 * session, the duty and every safety latch. The only local state is what the
 * operator has typed but not yet had acknowledged (selected profile, mode,
 * setpoint, manual duty) plus the last ack and the event log.
 */

#ifndef HMI_DATA_H
#define HMI_DATA_H

#include <Arduino.h>

#include "GenProfiles.h"
#include "LinkFrames.h"

/* Number of controller events kept for the diagnostics log. The controller
   sends each event once, repeated on the wire, so this is a best-effort
   history, not a recording — there is no SD card writer in this firmware. */
#define HMI_EVENT_LOG 12

struct HmiEvent {
  uint32_t atMs;        // HMI millis() when received
  uint32_t sessionMs;   // roast elapsed time when it happened
  uint16_t value;       // event-specific numeric evidence
  uint8_t type;         // LinkEventType
  uint8_t a;
  uint8_t b;
  char text[28];
};

struct HmiData {
  /* last telemetry frame */
  LinkState st;
  bool haveState;
  uint32_t lastStateMs;
  bool connected;       // a state frame arrived within LINK_TIMEOUT_MS
  uint8_t stateSerial;  // increments on every accepted frame

  /* last command result */
  bool haveAck;
  LinkAck ack;
  uint32_t lastAckMs;
  uint8_t ackSerial;

  /* event log, newest first */
  HmiEvent events[HMI_EVENT_LOG];
  uint8_t evCount;
  uint8_t evSerial;

  /* operator inputs not yet (or just) sent */
  uint8_t selProfile;    // LINK_PROFILE_NONE when nothing is selected
  uint8_t selMode;       // MODE_MANUAL / MODE_AUTO
  int16_t setpointC10;   // degC * 10, what SET_SETPOINT last sent
  uint8_t manualDuty;    // %, what SET_DUTY last sent
};

extern HmiData hmi;

/* Duty ceiling of the controller (GEN_DUTY_CEILING_PCT in GenConfig.h). The
   HMI only uses it to label the slider: the controller clamps regardless. */
#define HMI_DUTY_CEILING_PCT 30

void hmiInit();

/* Called from ESP-NOW receive context. */
void hmiOnState(const LinkState &st, uint32_t nowMs);
void hmiOnAck(const LinkAck &ack, uint32_t nowMs);
void hmiOnEvent(const LinkEvent &ev, uint32_t nowMs);

/* Called from the LVGL task. Each returns a consistent copy. */
void hmiSnapshot(LinkState &st, bool &connected, uint8_t &serial);
bool hmiCopyAck(LinkAck &ack, uint8_t &serial);
uint8_t hmiCopyEvents(HmiEvent *out, uint8_t maxOut);

/* Link watchdog: called by the transport when a state frame is overdue. */
void hmiSetConnected(bool connected);

/* Operator selection (local, sent as commands). */
void hmiOperatorSnapshot(uint8_t &profile, uint8_t &mode, int16_t &setpointC10,
                         uint8_t &manualDuty);
void hmiSelectProfile(uint8_t idx);
void hmiSetMode(uint8_t mode);
void hmiSetSetpointC10(int16_t c10);
void hmiSetManualDuty(uint8_t pct);

/* --- text helpers, shared by the screens ------------------------------- */
const char *hmiRoastStateText(uint8_t s);
const char *hmiStageText(uint8_t s);
const char *hmiStageMethodText(uint8_t m);
const char *hmiModeText(uint8_t m);
const char *hmiTempQualityText(uint8_t q);
const char *hmiColorStatusText(uint8_t c);
const char *hmiEventText(uint8_t type);

/* "TEMP_INVALID,ESTOP" — never longer than n bytes, always NUL terminated. */
void hmiFaultText(uint16_t mask, char *out, size_t n);
bool hmiFaultIsWarning(uint16_t bit);  // E-nose over-temperature is a warning

#endif  // HMI_DATA_H
