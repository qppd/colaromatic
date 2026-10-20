#pragma once

#include <Arduino.h>

#include "CONF.h"

class HeaterSafety;

/*
 * HostLink — the ASCII serial protocol (docs/05_serial_protocol.md).
 *
 * Everything that enters or leaves the controller over USB goes through this
 * class, so the wire format has exactly one implementation. The command
 * parser refreshes the heater watchdog ONLY for commands it recognises:
 * line noise must not keep a dead host looking alive (REQ-FW-002).
 */

struct Command {
  enum Type {
    NONE = 0,
    SET_HEATER,
    SET_FAN,
    SET_AUDIO,
    CMD_ESTOP,
    CMD_RESET,
    PING,
    POLL,
    IGNORED,   // known but reserved (SET,RATE): accepted, not acted on
    UNKNOWN,   // not in the command table: reported as UNKNOWN_COMMAND
  };
  Type type = NONE;
  bool heaterOn = false;
  uint8_t fanPct = 0;
  uint16_t audioThreshold = 0;
};

/* One telemetry snapshot, assembled by the sketch and framed here. */
struct SensorFrame {
  float temperatureC = NAN;
  uint8_t temperatureFlag = 2;  // 0 ok, 1 degraded, 2 invalid
  uint32_t redHz = 0;
  uint32_t greenHz = 0;
  uint32_t blueHz = 0;
  uint32_t clearHz = 0;
  float gasOhm = NAN;
  float sensorTempC = NAN;
  float humidityPct = NAN;
  float pressureHpa = NAN;
  uint8_t enoseFlag = 2;
  bool enoseWarmup = false;
};

class HostLink {
 public:
  void begin(Stream &stream, HeaterSafety &safety);

  /* Consume at most one complete line. Returns false when nothing is ready.
     Recognized commands refresh the host watchdog before returning. */
  bool readCommand(Command &cmd);

  void reportSensors(const SensorFrame &frame, uint32_t nowMs, bool hostLinkAlive);
  void reportStatus(uint32_t nowMs, bool hostLinkAlive);  // H frame only (PING)
  void reportFault(const char *code);                     // ERR, throttled
  void emitCrack(const char *kind, float confidence, uint32_t nowMs);
  void emitBanner();

 private:
  void parse(const String &line, Command &cmd);
  void printFloat(float value, uint8_t decimals);  // empty field when NAN
  void printFault(const char *code);

  Stream *_stream = nullptr;
  HeaterSafety *_safety = nullptr;
  const char *_lastFault = nullptr;
  uint32_t _lastFaultMs = 0;
};
