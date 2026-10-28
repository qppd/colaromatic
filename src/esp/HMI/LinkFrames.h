#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * LinkFrames — ESP-NOW frame contract between the machine controller
 * (src/esp/ColAromatic, ESP32 38-pin) and the HMI touch panel
 * (src/esp/HMI, ESP32-S3-Touch-LCD-4.3).
 *
 * SINGLE SOURCE OF TRUTH for the radio protocol documented in
 * docs/05_serial_protocol.md §7. This file lives in src/esp/common/ and is
 * copied verbatim into both sketch folders (Arduino IDE only compiles files
 * inside the sketch folder). A pytest (src/tests/test_firmware_tables.py)
 * fails if the three copies ever diverge.
 *
 * Transport: ESP-NOW unicast to the paired peer MAC, no AP, no internet.
 * ESP-NOW payloads are limited to 250 bytes; every frame here is far below
 * that. Every frame carries a CRC16 so a corrupt radio frame is dropped
 * before it can change machine state, and a sequence number so the repeated
 * delivery of a command is applied exactly once.
 */

#define LINK_MAGIC   0xCAu
#define LINK_VERSION 1u

/* Timing contract (both sides). A sender that misses LINK_TIMEOUT_MS of
   frames from its peer must declare the link lost. The controller reacts to
   HMI silence by removing heater authority (see docs/05 §7.4). */
#define LINK_BEACON_PERIOD_MS  500u   // HMI -> controller alive beacon
#define LINK_STATE_PERIOD_MS   500u   // controller -> HMI telemetry
#define LINK_TIMEOUT_MS        5000u  // 10 missed frames = link lost
#define LINK_CMD_REPEATS       3u     // each command sent 3x, applied once
#define LINK_PAIR_WINDOW_MS    60000u // pairing open after boot / on demand

/* Both boards must rendezvous on one WiFi channel: ESP-NOW only hears
   frames on the sender's channel. Chosen away from WiFi AP channel 6 by
   convention; change here only — it is baked into both firmwares. */
#define LINK_WIFI_CHANNEL      1u

/* ---- Frame header (12 bytes, little endian) --------------------------- */
struct __attribute__((packed)) LinkHeader {
  uint8_t magic;     // LINK_MAGIC
  uint8_t version;   // LINK_VERSION
  uint8_t kind;      // LinkFrameKind
  uint8_t flags;     // LINK_FLAG_*
  uint32_t seq;      // sender's monotonically increasing sequence number
  uint16_t payloadLen;
  uint16_t crc;      // CRC16/CCITT over header-without-crc + payload
};

#define LINK_FLAG_PAIR 0x01u  // sender is pairing / accepts a pairing peer

enum LinkFrameKind : uint8_t {
  LINK_BEACON = 1,  // HMI -> controller: alive + pairing request
  LINK_STATE  = 2,  // controller -> HMI: telemetry snapshot
  LINK_CMD    = 3,  // HMI -> controller: operator command
  LINK_ACK    = 4,  // controller -> HMI: command result
  LINK_EVENT  = 5,  // controller -> HMI: stage/fault/session event for the SD log
};

/* ---- Beacon: HMI -> controller ---------------------------------------- */
struct __attribute__((packed)) LinkBeacon {
  uint32_t uptimeMs;
  uint16_t hmiVersion;
  uint16_t reserved;
};

/* ---- State: controller -> HMI ----------------------------------------- */
struct __attribute__((packed)) LinkState {
  uint32_t uptimeMs;
  uint32_t sessionMs;      // roast elapsed seconds (0 when idle)
  int16_t tempC10;         // degC * 10, INT16_MIN when unavailable
  int16_t rorCpm10;        // degC/min * 10, INT16_MIN when unavailable
  int16_t setpointC10;     // current target degC * 10 (manual setpoint or trajectory)
  uint8_t tempQuality;     // LinkTempQuality
  uint8_t roastState;      // LinkRoastState
  uint8_t mode;            // LinkMode
  uint8_t stage;           // LinkStage
  uint8_t stageMethod;     // LinkStageMethod
  uint8_t stageConfirmed;  // 1 = operator confirmed, 0 = estimated
  uint8_t dutyCmd;         // requested duty %
  uint8_t dutyApp;         // applied (shaped) duty %
  uint8_t fanPct;          // fan output: 0 = off, 100 = on (SSR, no PWM)
  uint8_t relayOn;         // 1 = SSR pin currently high
  uint8_t profileIdx;      // selected profile (0xFF = none)
  uint8_t endpointPending; // 1 = controller waits for operator confirmation
  uint16_t faultsActive;   // LinkFaultBit mask, currently active
  uint16_t faultsLatched;  // LinkFaultBit mask, latched until reset
  uint32_t clearHz;        // TCS3200 clear channel (raw, uncalibrated)
  int32_t gasOhmMilli;     // BME688 gas resistance mOhm, INT32_MIN = n/a
  uint8_t colorStatus;     // LinkColorStatus (calibration state, informational)
  uint8_t reserved0;
  uint16_t reserved1;
};

/* ---- Command: HMI -> controller --------------------------------------- */
struct __attribute__((packed)) LinkCmd {
  uint8_t opcode;  // LinkOpcode
  uint8_t a;       // argument 0 (duty %, profile index, mode, stage)
  int16_t b;       // argument 1 (setpoint degC * 10)
};

enum LinkOpcode : uint8_t {
  OP_START = 1,           // a=profile idx (0xFF none), b=setpoint*10 (0x8000 unused)
  OP_HOLD = 2,            // heater to 0 %, monitoring and fan continue
  OP_RESUME = 3,          // leave hold
  OP_COOL = 4,            // stop the roast and start cooling
  OP_ESTOP = 5,           // software emergency stop (latched)
  OP_RESET = 6,           // clear latched faults (refused while hot / E-stop held)
  OP_SET_DUTY = 7,        // a = manual duty %
  OP_SET_MODE = 8,        // a = LinkMode
  OP_SELECT_PROFILE = 9,  // a = profile index (valid when idle)
  OP_CONFIRM_STAGE = 10,  // a = LinkStage to confirm forward
  OP_CONFIRM_ENDPOINT = 11, // operator accepts a pending endpoint -> cooling
  OP_SET_SETPOINT = 12,   // b = manual/auto setpoint degC * 10
  OP_SET_FAN = 13,        // a = fan % (0 = off, non-zero = on; SSR, no PWM)
  OP_PAIR = 14,           // force pairing mode (a = magic 0x5A)
};

/* ---- Ack: controller -> HMI ------------------------------------------- */
struct __attribute__((packed)) LinkAck {
  uint32_t cmdSeq;   // seq of the command being acknowledged
  uint16_t faults;   // LinkFaultBit mask at the time of the ack
  uint8_t result;    // LinkAckResult
  uint8_t opcode;    // echoed opcode
  char text[24];     // human-readable refusal reason, NUL padded
};

enum LinkAckResult : uint8_t {
  ACK_OK = 0,
  ACK_GATING = 1,    // refused: safety fault, latch, probe or link gate
  ACK_STATE = 2,     // refused in the current roast state (or reset refused)
  ACK_ARG = 3,       // argument out of range
  ACK_PROFILE = 4,   // unknown profile index
};

/* ---- Event: controller -> HMI (volatile panel log; nothing is stored) --- */
struct __attribute__((packed)) LinkEvent {
  uint32_t sessionMs;  // roast elapsed when it happened (0 if idle)
  uint16_t value;      // event-specific numeric evidence
  uint16_t reserved;
  uint8_t type;        // LinkEventType
  uint8_t a;
  uint8_t b;
  uint8_t reserved1;
  char text[28];       // stage name / fault code / reason, NUL padded
};

enum LinkEventType : uint8_t {
  EV_STAGE_CHANGE = 1,
  EV_FAULT_TRIP = 2,
  EV_COOLING_START = 3,
  EV_SESSION_END = 4,
  EV_ENDPOINT_PENDING = 5,
  EV_CRACK = 6,
  EV_SESSION_START = 7,
  EV_ROAST_HEARTBEAT = 8,      // periodic row for the panel's volatile event log
  EV_COOLING_UNVERIFIED = 9,   // cooling ended without a verified reading
  EV_HEATER_COMMAND = 10,      // applied duty changed by >= 5 %
};

/* ---- Shared enums ------------------------------------------------------ */
enum LinkStage : uint8_t {
  STAGE_CHARGE = 0,
  STAGE_TURNING_POINT = 1,
  STAGE_COLOR_CHANGE = 2,
  STAGE_FIRST_CRACK = 3,
  STAGE_MEDIUM_ROAST = 4,
  STAGE_SECOND_CRACK = 5,
  STAGE_DARK_ROAST = 6,
  STAGE_DONE = 7,
};

enum LinkStageMethod : uint8_t {
  METHOD_OPERATOR = 0,
  METHOD_TEMP_MINIMUM = 1,
  METHOD_COLOR_TREND = 2,
  METHOD_PROFILE_ENDPOINT = 3,
  METHOD_ACOUSTIC = 4,
  METHOD_SESSION = 5,
};

enum LinkRoastState : uint8_t {
  ROAST_IDLE = 0,
  ROAST_RUNNING = 1,
  ROAST_HOLD = 2,
  ROAST_COOLING = 3,
  ROAST_DONE = 4,
};

enum LinkMode : uint8_t {
  MODE_MANUAL = 0,
  MODE_AUTO = 1,
};

enum LinkTempQuality : uint8_t {
  TEMP_OK = 0,
  TEMP_DEGRADED = 1,
  TEMP_INVALID = 2,
  TEMP_STALE = 3,
};

enum LinkColorStatus : uint8_t {
  COLOR_UNCALIBRATED = 0,
  COLOR_PROVISIONAL = 1,
  COLOR_VALIDATED = 2,
  COLOR_INVALID = 3,
};

/* Fault mask shared with the panel (docs/01 section K, docs/08 section 4). */
enum LinkFaultBit : uint16_t {
  F_TEMP_INVALID  = 1u << 0,
  F_TEMP_STALE    = 1u << 1,
  F_OVER_TEMP     = 1u << 2,  // process limit (GenConfig GEN_MAX_TEMP_C), latched
  F_LOCAL_OVERTEMP= 1u << 3,  // firmware backstop (CONF.h LOCAL_MAX_TEMP_C)
  F_ROR_RUNAWAY   = 1u << 4,
  F_ESTOP         = 1u << 5,
  F_HMI_LINK_LOST = 1u << 6,
  F_ENOSE_HOT     = 1u << 7,  // warning only
  F_HOST_TIMEOUT  = 1u << 8,  // serial service link watchdog
};

/* Profile index sentinel: no profile selected. */
#define LINK_PROFILE_NONE 0xFFu
#define LINK_SETPOINT_UNCHANGED ((int16_t)0x8000)

/* ---- CRC16/CCITT (poly 0x1021, init 0xFFFF) --------------------------- */
static inline uint16_t linkCrc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

/* CRC covers the header with the crc field treated as zero, then payload. */
static inline uint16_t linkFrameCrc(const LinkHeader *header, const uint8_t *payload, uint16_t payloadLen) {
  uint8_t buf[sizeof(LinkHeader)];
  for (size_t i = 0; i < sizeof(LinkHeader); i++) buf[i] = ((const uint8_t *)header)[i];
  buf[10] = 0;  // crc field occupies bytes 10..11
  buf[11] = 0;
  uint16_t crc = linkCrc16(buf, sizeof(LinkHeader));
  if (payloadLen > 0 && payload != NULL) {
    /* Continue the CRC over the payload without reallocating a big buffer:
       feed it byte by byte through the same routine by chaining. */
    for (size_t i = 0; i < payloadLen; i++) {
      crc ^= (uint16_t)payload[i] << 8;
      for (int bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
      }
    }
  }
  return crc;
}
