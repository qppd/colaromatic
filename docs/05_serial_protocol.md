# 05 — Service serial protocol (bench port)

* Transport: USB serial (or a UART bridge), 115200 baud, ASCII, one frame per
  line, `\n` terminated. CR is tolerated.
* Direction: the controller streams telemetry; whoever is attached streams
  commands.
* Role: **service and bench work only.** The machine's normal operator path is
  the ESP-NOW link to the touch panel, documented in `LinkFrames.h` and
  summarized in docs/02 §6. This port exists so the controller can be driven,
  observed and calibrated with no panel attached.
* Implementation: `src/esp/ColAromatic/HostLink.cpp`, framed by
  `ColAromatic.ino`.
* There is **no host parser in this repository**: the Python bridge and its
  contract test were removed with the host. Today the consumers are a serial
  terminal, a serial monitor script, or your own tool. If you write one, keep a
  replay test — the frames below are a contract, not a log format.

## 1. Controller → terminal (telemetry)

| Frame | Fields | Notes |
|---|---|---|
| `T,<ms>,<temp_c>,<flag>` | thermocouple temperature | `flag`: `0` ok, `1` degraded (reserved; the MAX6675 cannot report it), `2` invalid. `<temp_c>` is empty when invalid. |
| `C,<ms>,<r_hz>,<g_hz>,<b_hz>,<clear_hz>,<flag>` | TCS3200 light-to-frequency | channel order is red, green, blue, clear; frequencies in Hz |
| `E,<ms>,<gas_ohm>,<temp_c>,<hum_pct>,<press_hpa>,<flag>,<warmup>` | BME688 | `warmup`: `0`/`1`; `<temp_c>` here is the **sensor** temperature |
| `H,<ms>,<relay>,<fan>,<estop>,<link>` | actuator and interlock feedback | `relay` is the actual heater output state, `fan` is `0`/`100` (fan SSR is on/off), `estop` the firmware latch, `link` the firmware's view of this serial link |
| `K,<ms>,<kind>,<confidence>` | acoustic event | `kind` = `first_crack` or `second_crack`; `confidence` 0–1 |
| `ERR,<ms>,<code>[,<detail>]` | controller fault | e.g. `TC_OPEN`, `TC_FAULT`, `TC_INVALID_HEATER_OFF`, `BME_INIT_FAIL`, `BME_READ_FAIL`, `ESTOP_ENGAGED`, `ESTOP_INPUT`, `HOST_TIMEOUT`, `HMI_LINK_LOST`, `LOCAL_OVER_TEMP`, `OVER_TEMP_LATCHED`, `SESSION_ACTIVE`, `ESTOP_STILL_ACTIVE`, `RESET_OK`, `ESP_NOW_INIT_FAILED`, `UNKNOWN_COMMAND` |
| `# ...` | human-readable banner (MAC, firmware notes) | informational |

`<ms>` is the controller's `millis()` and is informational: **an attached tool
should timestamp each frame when it arrives** and use that arrival time for
staleness decisions. A drifting device clock must not make stale data look fresh.

Telemetry is emitted every `REPORT_PERIOD_MS` (1 s), or immediately on
`CMD,POLL`.

## 2. Terminal → controller (commands)

| Command | Effect |
|---|---|
| `SET,HEATER,0\|1` | request heater off/on (still subject to every on-board interlock). **Refused with `ERR,SESSION_ACTIVE` while a roast session is live** — the onboard controller owns the heater then. |
| `SET,FAN,<0..100>` | fan request; `0` = off, any non-zero = on (the fan SSR is on/off, no PWM). Also refused during a live session. |
| `CMD,ESTOP` | latch the firmware E-stop and remove heater power |
| `CMD,RESET` | clear the latches; refused while the E-stop input is active or a latch is still hot — the refusal reason is returned as `ERR` |
| `SET,AUDIO,<threshold>` | enable crack detection at an ADC peak-to-peak threshold, `0` disables |
| `PING` | immediate `H` status frame |
| `CMD,POLL` | immediate full telemetry report (T/C/E/H) |
| `SET,RATE,<ms>` | reserved, not implemented; accepted so it is not reported as an error |

Only a **recognized** command refreshes the serial watchdog, so line noise and
unknown lines cannot keep a dead attach looking alive.

## 3. Timing and failure behaviour

| Condition | Controller | Attached tool |
|---|---|---|
| Normal operation | re-emits telemetry every 1 s | may send commands at any time |
| No recognized command for 3 s after a host has been live (`HOST_TIMEOUT_MS`) | heater off, `ERR,HOST_TIMEOUT`, `link=0`, **duty request cleared** | re-send a command to restore authority |
| A line that is not in the command table | `ERR,UNKNOWN_COMMAND`, watchdog **not** refreshed | fix the sender |
| Thermocouple open / out of range | interlock drops the heater SSR within one loop pass, `ERR,TC_INVALID_HEATER_OFF`, `T` frame flag `2` | the session (if any) aborts to cooling |
| Physical E-stop pressed | latches within the debounce time, `ERR,ESTOP_INPUT`, SSR off | `H` frame `estop=1` |
| Cable unplugged | controller watchdog fires once a session or a host had been live | link loss visible as silence |
| Firmware resets | reprints the boot banner and the MAC; latches cleared | frames resume; a live panel keeps its own view |
| A roast session is running | `SET,HEATER` / `SET,FAN` refused with `SESSION_ACTIVE` | use the panel, or stop the session first |

Two independent rules keep a dead attach from leaving the heater on: the
controller drops authority after 3 s of silence, and a live operator path
speaks at least once a second so silence means *dead*, not *busy*. The
controller's own watchdog runs whether or not anything is attached — a machine
that is never plugged in still cannot be left heating by a crashed tool.

## 4. Why ASCII

* A human can debug it with any serial terminal; the operator manual and the
  troubleshooting guide both rely on this.
* Frame-level corruption is visible rather than silent.
* Bandwidth is ample: four frames per second at ~40 bytes each is under 200 B/s.

The cost is parsing cost, which is negligible at 1 Hz on the ESP32.

## 5. Extending the protocol

If a new measurement is added:

1. add a frame tag that is not already used;
2. emit it from `HostLink` (or `reportTelemetry` in `ColAromatic.ino`);
3. carry it into the panel's `LinkState` if the operator needs it
   (`LinkFrames.h`, with the CRC/length updated on **both** sketches);
4. keep the arity stable — a short frame is a malformed frame and is dropped;
5. update this document in the same change.

Do not reuse a tag with a different arity.

## 6. Relationship to the panel link

The panel does **not** use this UART. It has its own binary ESP-NOW contract
(`LinkFrames.h`): STATE, CMD, ACK, EVENT and BEACON frames with magic, version,
length, CRC16 and sequence de-duplication, addressed MAC to MAC. Both links can
be connected at once: each has its own watchdog, and losing *both* leaves the
heater off. See docs/02 §6 for the authority table.
