#include "HostLink.h"

#include "HeaterSafety.h"

void HostLink::begin(Stream &stream, HeaterSafety &safety) {
  _stream = &stream;
  _safety = &safety;
  _stream->setTimeout(SERIAL_TIMEOUT_MS);  // never stall the loop on a partial line
}

// ---- command side --------------------------------------------------------

bool HostLink::readCommand(Command &cmd) {
  cmd.type = Command::NONE;
  if (_stream == nullptr) {
    return false;
  }
  while (_stream->available() > 0) {
    String line = _stream->readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    parse(line, cmd);
    if (cmd.type != Command::UNKNOWN) {
      _safety->onHostFrame(millis());  // noise must not refresh the watchdog
    }
    return true;
  }
  return false;
}

void HostLink::parse(const String &line, Command &cmd) {
  cmd.type = Command::UNKNOWN;
  if (line.startsWith("SET,HEATER,")) {
    cmd.type = Command::SET_HEATER;
    cmd.heaterOn = line.substring(11).toInt() != 0;
  } else if (line.startsWith("SET,FAN,")) {
    cmd.type = Command::SET_FAN;
    cmd.fanPct = (uint8_t)constrain(line.substring(8).toInt(), 0, 100);
  } else if (line.startsWith("SET,AUDIO,")) {
    cmd.type = Command::SET_AUDIO;
    int value = line.substring(10).toInt();
    cmd.audioThreshold = (uint16_t)(value <= 0 ? 0 : value);
  } else if (line.startsWith("SET,RATE,")) {
    cmd.type = Command::IGNORED;  // reserved, validated before it is used
  } else if (line.equals("CMD,ESTOP")) {
    cmd.type = Command::CMD_ESTOP;
  } else if (line.equals("CMD,RESET")) {
    cmd.type = Command::CMD_RESET;
  } else if (line.equals("CMD,POLL")) {
    cmd.type = Command::POLL;
  } else if (line.equals("PING")) {
    cmd.type = Command::PING;
  }
}

// ---- telemetry side ------------------------------------------------------

void HostLink::reportSensors(const SensorFrame &f, uint32_t nowMs, bool hostLinkAlive) {
  if (_stream == nullptr) {
    return;
  }
  _stream->print(F("T,"));
  _stream->print(nowMs);
  _stream->print(',');
  if (f.temperatureFlag == 2 || isnan(f.temperatureC)) {
    _stream->print(',');  // empty value, flag says why
  } else {
    _stream->print(f.temperatureC, 3);
    _stream->print(',');
  }
  _stream->println(f.temperatureFlag);

  _stream->print(F("C,"));
  _stream->print(nowMs);
  _stream->print(',');
  _stream->print(f.redHz);
  _stream->print(',');
  _stream->print(f.greenHz);
  _stream->print(',');
  _stream->print(f.blueHz);
  _stream->print(',');
  _stream->print(f.clearHz);
  _stream->println(F(",0"));

  _stream->print(F("E,"));
  _stream->print(nowMs);
  _stream->print(',');
  printFloat(f.gasOhm, 1);
  _stream->print(',');
  printFloat(f.sensorTempC, 2);
  _stream->print(',');
  printFloat(f.humidityPct, 2);
  _stream->print(',');
  printFloat(f.pressureHpa, 2);
  _stream->print(',');
  _stream->print(f.enoseFlag);
  _stream->print(',');
  _stream->println(f.enoseWarmup ? 1 : 0);

  reportStatus(nowMs, hostLinkAlive);
}

void HostLink::reportStatus(uint32_t nowMs, bool hostLinkAlive) {
  if (_stream == nullptr || _safety == nullptr) {
    return;
  }
  _stream->print(F("H,"));
  _stream->print(nowMs);
  _stream->print(',');
  _stream->print(_safety->heaterOutput() ? 1 : 0);
  _stream->print(',');
  _stream->print(_safety->fanPercent());
  _stream->print(',');
  _stream->print(_safety->estopLatched() ? 1 : 0);
  _stream->print(',');
  _stream->println(hostLinkAlive ? 1 : 0);
}

void HostLink::emitCrack(const char *kind, float confidence, uint32_t nowMs) {
  if (_stream == nullptr) {
    return;
  }
  _stream->print(F("K,"));
  _stream->print(nowMs);
  _stream->print(',');
  _stream->print(kind);
  _stream->print(',');
  _stream->println(confidence, 2);
}

void HostLink::emitBanner() {
  if (_stream != nullptr) {
    _stream->println(F(FIRMWARE_BANNER));
  }
}

// ---- faults --------------------------------------------------------------

void HostLink::reportFault(const char *code) {
  uint32_t now = millis();
  /* A persistent condition must not flood the link: repeats of the same code
     are dropped inside the throttle window. */
  if (code == _lastFault && (now - _lastFaultMs) < FAULT_REPORT_THROTTLE_MS) {
    return;
  }
  printFault(code);
  _lastFault = code;
  _lastFaultMs = now;
}

void HostLink::printFault(const char *code) {
  if (_stream == nullptr) {
    return;
  }
  _stream->print(F("ERR,"));
  _stream->print(millis());
  _stream->print(',');
  _stream->println(code);
}

void HostLink::printFloat(float value, uint8_t decimals) {
  if (_stream == nullptr) {
    return;
  }
  /* Nothing at all for NAN: the caller prints the field separator, so an
     empty field is what the host must see for "no value". */
  if (!isnan(value)) {
    _stream->print(value, decimals);
  }
}
