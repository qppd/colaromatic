# 05 — Host ↔ ESP32 serial protocol

* Transport: USB serial (or a UART bridge), 115200 baud, ASCII, one frame per
  line, `\n` terminated. CR is tolerated.
* Direction: the controller streams telemetry; the host streams commands.
* Implementations: `src/esp/ColAromatic/HostLink.cpp` (device, framed by
  `src/esp/ColAromatic/ColAromatic.ino`) and
  `src/colaromatic/sensors/serial_bridge.py` (host).
* Contract test: `src/tests/test_firmware_protocol.py` replays literal firmware
  frames through the host parser. Changing one side without the other fails CI.

## 1. Controller → host (telemetry)

| Frame | Fields | Notes |
|---|---|---|
| `T,<ms>,<temp_c>,<flag>` | thermocouple temperature | `flag`: `0` ok, `1` degraded (reserved; the MAX6675 cannot report it), `2` invalid. `<temp_c>` is empty when invalid. |
| `C,<ms>,<r_hz>,<g_hz>,<b_hz>,<clear_hz>,<flag>` | TCS3200 light-to-frequency | channel order is red, green, blue, clear; frequencies in Hz |
| `E,<ms>,<gas_ohm>,<temp_c>,<hum_pct>,<press_hpa>,<flag>,<warmup>` | BME688 | `warmup`: `0`/`1`; `<temp_c>` here is the **sensor** temperature |
| `H,<ms>,<relay>,<fan>,<estop>,<link>` | actuator and interlock feedback | `relay` is the actual heater output state, `fan` is `0`/`100` (fan SSR is on/off), `estop` the firmware latch, `link` the firmware's view of the host link |
| `K,<ms>,<kind>,<confidence>` | acoustic event | `kind` = `first_crack` or `second_crack`; `confidence` 0–1 |
| `ERR,<ms>,<code>[,<detail>]` | controller fault | e.g. `TC_OPEN`, `TC_FAULT`, `TC_INVALID_HEATER_OFF`, `BME_INIT_FAIL`, `BME_READ_FAIL`, `ESTOP_ENGAGED`, `ESTOP_INPUT`, `HOST_TIMEOUT`, `LOCAL_OVER_TEMP`, `OVER_TEMP_LATCHED`, `ESTOP_STILL_ACTIVE`, `RESET_OK`, `UNKNOWN_COMMAND` |
| `# ...` | human-readable banner | ignored by the host parser |

`<ms>` is the controller's `millis()` and is informational: **the host
timestamps each frame when it arrives** and uses that arrival time for staleness
decisions. A drifting device clock therefore cannot make stale data look fresh.

## 2. Host → controller (commands)

| Command | Effect | Host method |
|---|---|---|
| `SET,HEATER,0|1` | request heater off/on (still subject to every on-board interlock) | `bridge.set_heater(True/False, now)` |
| `SET,FAN,<0..100>` | fan request; `0` = off, any non-zero = on (the fan SSR is on/off, no PWM) | `bridge.set_fan(pct, now)` |
| `CMD,ESTOP` | latch the firmware E-stop and remove heater power | `bridge.emergency_stop()` |
| `CMD,RESET` | clear the latch; refused while the E-stop input is active or over-temperature is latched | `bridge.clear_controller_faults()` |
| `SET,AUDIO,<threshold>` | enable crack detection at an ADC peak-to-peak threshold, `0` disables | manual |
| `PING` | immediate `H` status frame | manual |
| `CMD,POLL` | immediate full telemetry report (T/C/E/H) | manual |
| `SET,RATE,<ms>` | reserved, not implemented; accepted so it is not reported as an error | — |

## 3. Timing and failure behaviour

| Condition | Controller | Host |
|---|---|---|
| Live host, normal operation | — | `engine.step()` re-asserts the last heater command every `timing.keepalive_period_s` (1 s) |
| No **recognized** host frame for 3 s (`HOST_TIMEOUT_MS`) | heater off, `ERR,HOST_TIMEOUT`, `link=0`, request cleared | — |
| A line that is not in the command table | `ERR,UNKNOWN_COMMAND`, watchdog **not** refreshed | — |
| Controller reports `link=0` twice in a row | — | `link_ok()` false → start refused, or a running session aborts to cooling |
| Thermocouple open / out of range (MAX6675 open bit or range check) | interlock drops the heater SSR within one loop pass, `ERR,TC_INVALID_HEATER_OFF`, `T` frame flag `2` | temperature invalid → critical fault → session aborts to cooling |
| No frame received for 4 s (`link_timeout_s`) | — | `link_ok()` false; start refused; heater authority removed mid-session |
| No `T` frame for 3 s (`stale_after_s`) | — | temperature marked `invalid` (not frozen at the last value) |
| Physical E-stop pressed | latches within `ESTOP_DEBOUNCE_MS`, `ERR,ESTOP_INPUT`, SSR off | `H` frame `estop=1` → host latches its own E-stop and aborts the session |
| Cable unplugged | controller watchdog fires | bridge reports link loss; host enters the abort/cooling path |
| Firmware resets | reprints the boot banner; latches cleared | echo frames resume; the host has already removed heater authority |

Two independent rules keep a dead host from leaving the heater on: the
controller drops authority after 3 s of silence, and the host speaks at least
once a second so silence means *dead host*, not *busy host*. The controller's
watchdog only counts commands it recognises, so line noise cannot keep a dead
host looking alive.

## 4. Why ASCII

* A human can debug it with any serial terminal; the operator manual and the
  troubleshooting guide both rely on this.
* Frame-level corruption is visible rather than silent.
* Bandwidth is ample: four frames per second at ~40 bytes each is under 200 B/s.

The cost is parsing cost, which is negligible on the host and, at 1 Hz, on the
ESP32 as well.

## 5. Extending the protocol

If a new measurement is added:

1. add a frame tag that is not already used;
2. parse it in `serial_bridge.feed_line` (with a test);
3. carry it into `MachineSample` → `ProcessSample` → CSV columns and the schema
   document;
4. add the field to the completion summary if it is needed there;
5. update this document and `test_firmware_protocol.py` in the same change.

Do not reuse a tag with a different arity; the host treats a short frame as a
malformed frame and ignores it, which would silently drop data.

## 6. Multi-board or HMI variants (not implemented)

If a separate board drives the HMI, or the HMI also shares the UART, the
protocol should stay as-is with the HMI speaking to the **host** (not the
microcontroller), e.g. the HMI (or a bridge process) subscribing to the same
JSON snapshot the browser dashboard uses over `/ws`. That keeps one source of
truth for the operator display. This has not been implemented or tested.
