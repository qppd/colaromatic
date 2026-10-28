# 02 — System architecture

**Scope of this revision.** The deliverable is a **standalone machine**: two ESP32
boards that talk to each other over ESP-NOW and need no PC, no network and no
host process. The earlier Python host (engine, REST/WebSocket server, browser
dashboard, simulator, JSON configuration, test suite) was removed in commit
`31e4632`; where this document used to describe it, it now describes the firmware
that replaced it.

```
┌──────────────────────────── panel (src/esp/HMI/) ───────────────────────────────┐
│ HMI.ino           bring-up, LVGL tick, radio init                                │
│ screen_manager    boot → dashboard → control → analytics → diagnostics           │
│ *_screen          LVGL widgets; they read hmi_data and send opcodes              │
│ hmi_data          one-lock telemetry mirror + event log + text tables            │
│ esp_now_link      beacons, MAC peer, pairing, retries, command wrappers          │
│                                                                                    │
│  thin client: it observes and it requests. it cannot energise anything.           │
└───────────────────────────────┬───────────────────────────────────────────────────┘
                                │ ESP-NOW, fixed channel, CRC16 + sequence de-dup
                                │ LinkFrames.h (STATE / CMD / ACK / EVENT / BEACON)
┌───────────────────────────────▼──────────────── controller (src/esp/ColAromatic/) ┐
│ ColAromatic.ino   setup()/loop(): the single orchestration point (10 stages/pass)  │
│                                                                                    │
│  ┌── sensing ──────────────┐   ┌── session & control ────────────────────────┐     │
│  │ ThermocoupleSensor 1 s  │   │ RoastControl   state machine, EMA filter,  │     │
│  │ ColorSensor      poll   │   │                 RoR, stages, endpoint,     │     │
│  │ EnoseSensor       ~1 s  │   │                 duty shaping, events       │     │
│  │ AcousticSensor    poll  │   │ ColorSetpoint  colour error → setpoint     │     │
│  └────────────┬────────────┘   │ Fuzzy          Mamdani inference tables    │     │
│               │                │ Pid            setpoint → duty            │     │
│               │                └────────────────────┬───────────────────────┘     │
│               │                                     │ duty 0…100 %                 │
│  ┌────────────▼─────────────────────────────────────▼───────────────────────┐     │
│  │ HeaterSafety   service() on EVERY loop pass: interlocks, latches, both    │     │
│  │                link watchdogs, E-stop, zero-cross time-proportioning,    │     │
│  │                and the only code that writes the heater/fan SSR pins      │     │
│  └───────────────────────────────────────────────────────────────────────────┘     │
│                                                                                    │
│  ┌── links ─────────────────────────────────────────────────────────────────┐     │
│  │ HmiLink   ESP-NOW to the panel (commands in, state/events out)           │     │
│  │ HostLink  USB ASCII service port (docs/05) — bench work, no HMI present  │     │
│  └───────────────────────────────────────────────────────────────────────────┘     │
└────────────────────────────────────────────────────────────────────────────────────┘
                                │
                       heater SSR · fan SSR · E-stop input · MAX6675 · TCS3200 ·
                       BME688 · microphone · status LED
```

## 1. Why the split is this way

* **One machine, no PC.** The controller owns the roast session, the control loop
  and every interlock. Removing the host removed the failure mode where a
  stalled laptop leaves a heater energised (REQ-SYS-001, REQ-SYS-004).
* **Safety sits between the controller and the heater, and nowhere else.**
  `HeaterSafety::service()` is the single writer of the SSR pins, it runs on
  every `loop()` pass, and it can remove authority without the control law's
  cooperation (REQ-FUZ-004, REQ-SAFE-003…012).
* **The panel is not in the control path.** It sends opcodes and renders state.
  Switching it off removes heater authority after `LINK_TIMEOUT_MS`, which is
  the defined *safe* direction — never a way to keep heating (REQ-TIME-003,
  REQ-FW-006).
* **Profiles are data.** 12 combinations of bean variety × roast level live in
  `GenProfiles.h`; the control law is one implementation (REQ-PROF-001).
* **Two loops in series.** Colour error sets the temperature setpoint (fuzzy
  outer loop, `ColorSetpoint`); the setpoint sets the duty (PID inner loop,
  `Pid`). Either loop has a defined fallback when its input is unusable
  (REQ-CTRL-001, REQ-CTRL-003).

## 2. Data flow of one control tick

```
loop() pass (as fast as the work allows; internal ticks are period-gated)
  ├─ 1  serial service commands           → dispatchSerial (bench only)
  ├─ 2  HMI link: proof of life, then queued commands → dispatchHmi
  ├─ 3  thermocouple.update()  then  heaterSafety.setProbeReading(); service()
  │        ← every interlock evaluated here, every pass
  ├─ 4  one-shot trip handling: report the code, abort the session to COOLING
  ├─ 5  colour + acoustic polling; crack events → stage detector
  ├─ 6  roast.service(sample)   (period-gated internally)
  │        sensor tick 1 s   EMA filter, quality, least-squares RoR
  │        stage/endpoint    turning point, operator confirmation, max duration
  │        control tick 2 s  colour error ──► ColorSetpoint.setpointC()
  │                               └─ unavailable? → profile trajectory →
  │                                  operator setpoint → no automatic control
  │                          setpoint − filtered T ──► Pid.compute() → duty
  │                          duty ← shapeDuty(): ceiling, min duty, quantisation
  ├─ 7  outputs: live session → setDutyRequest(dutyApplied), setFan(roast/cool)
  ├─ 8  session events → hmiLink.sendEvent()
  ├─ 9  telemetry report every REPORT_PERIOD_MS → hostLink.reportSensors()
  └─ 10 hmiLink.service(now, state) → STATE frame every LINK_STATE_PERIOD_MS
```

`HeaterSafety::service()` turns the duty request into a zero-cross
time-proportioned phase aligned to `millis()`, or straight to 0 % the moment any
interlock is active.

## 3. Timing schedules

| Schedule | Default | Where | Rationale |
|---|---|---|---|
| Loop pass | unthrottled | `ColAromatic::loop` | an interlock must take effect within one pass |
| Interlock evaluation | every pass | `HeaterSafety::service` | probe/E-stop/over-temp/link silence remove power immediately |
| Sensor acquisition | 1 s | `GenConfig.h` `SENSOR_PERIOD_MS` | fast enough for RoR and staleness |
| Control computation | 2 s | `GenConfig.h` control period | process responds in tens of seconds; avoids hunting |
| Time-proportioning window | 10 s (2 % granularity) | `HeaterSafety` | zero-cross SSR friendly |
| Serial telemetry report | 1 s | `REPORT_PERIOD_MS` | bench observability |
| HMI STATE frame | 500 ms | `LINK_STATE_PERIOD_MS` | panel responsiveness without radio churn |
| HMI link watchdog | 5 s silence → trip | `CONF.h` `LINK_TIMEOUT_MS` | panel loss removes authority |
| Serial watchdog | 3 s silence → trip | `CONF.h` `HOST_TIMEOUT_MS` | bench host loss removes authority |
| BME688 read | inside the report only | `reportTelemetry` | it blocks ~200 ms; it never runs in the interlock path |

## 4. Session state machine

```
IDLE ──start──► RUNNING ──hold──► HOLD ──resume──► RUNNING
   ▲               │                 │
   │               └──── cool / endpoint / fault ────┐
   │                                                 ▼
   └──────────── DONE ◄──cool enough── COOLING ◄─────┘
                     (E-stop or a latched fault) ──► aborted to COOLING
```

* `RUNNING` may be `MODE_MANUAL` (fixed duty) or `MODE_AUTO` (two-loop). A live
  session fixes both the mode and the profile: changing either is refused
  (REQ-CTRL-005).
* **Cooling is never skipped.** Ending a roast removes the heater and runs the
  fan at full; a safety trip does the same thing (REQ-GUI-005).
* Endpoint: maximum duration ends the run immediately; every other criterion
  raises a **pending endpoint** that the operator must confirm (REQ-STAGE-007).
* `DONE` raises the completion popup only after the stopping conditions are met
  (REQ-GUI-006).

## 5. Authority and failure behaviour

| Condition | Behaviour | Code |
|---|---|---|
| Panel silent > 5 s | `TRIP_HMI_LINK_LOST`: duty request cleared, heater off | `HeaterSafety` |
| Serial host silent > 3 s *after having been live* | `TRIP_HOST_TIMEOUT`: same | `HeaterSafety` |
| Probe invalid or stale | `TRIP_PROBE_INVALID`, session aborted to cooling | `loop()` stage 3 |
| Over-temperature (process limit) | latched; reset only while cool | `HeaterSafety` |
| On-board backstop (`CONF.h` limit) | trips, clears on hysteresis | `HeaterSafety` |
| RoR runaway | detected by `RoastControl`, latched by `HeaterSafety` | stage 6 |
| E-stop input or command | latched; release refused while hot or while the input is held | `HeaterSafety` |
| Any trip | **clears the duty request as well as the output** — recovery needs a fresh command from a live operator | `HeaterSafety` |
| Reset refused | returns the reason string, which the panel shows | `resetRefusal()` |
| Panel sends an illegal opcode | `ACK_ARG` / `ACK_STATE` / `ACK_GATING` + reason; nothing changes | `dispatchHmi` |
| Fault during a session | `EV_FAULT_TRIP` event + `abortToCooling(reason, status)` | stage 4 |

There is deliberately **no path** in which the panel or the serial port can set
the heater output directly while a session is live: `dispatchSerial` refuses
`SET_HEATER`/`SET_FAN` with `SESSION_ACTIVE`, and `dispatchHmi` routes every
duty request through `RoastControl::setDuty`.

## 6. The two links

| | ESP-NOW (panel) | USB serial (bench) |
|---|---|---|
| Files | `HmiLink.*`, `LinkFrames.h`, `src/esp/HMI/esp_now_link.*` | `HostLink.*`, docs/05 |
| Purpose | normal operator path | service, calibration, debugging without a panel |
| Framing | binary, magic + version + length + CRC16 + sequence | ASCII lines |
| Addressing | MAC to MAC, peer in NVS, pairing handshake | port |
| Watchdog | 5 s → `TRIP_HMI_LINK_LOST` | 3 s → `TRIP_HOST_TIMEOUT` |
| Authority | commands + state/events | commands only when no session is live |

Both may be connected at once; a live link on *either* keeps authority, and
losing *both* leaves the heater off.

## 7. Module dependency rules

| Module | May use | Must not touch |
|---|---|---|
| `PINS.h`, `CONF.h`, `Gen*.h` | nothing (data only) | — |
| sensor drivers | Arduino/SPI/I²C, `CONF.h`, `PINS.h` | `RoastControl`, `HeaterSafety`, links |
| `Fuzzy`, `ColorSetpoint`, `Pid` | their tables, `CONF.h` | pins, links, `RoastControl` internals |
| `RoastControl` | sensors' samples, `Fuzzy`, `ColorSetpoint`, `Pid`, `Gen*` | pins, links |
| `HeaterSafety` | `CONF.h`, `PINS.h`, `LinkFrames.h` masks | `RoastControl`, screens |
| `HmiLink` / `HostLink` | `LinkFrames.h`, `HeaterSafety` state | any pin write |
| `ColAromatic.ino` | everything above — it is the only wiring point | — |
| `src/esp/HMI/*` | `LinkFrames.h`, `GenProfiles.h` (mirror) | anything on the controller |

`GenConfig.h`, `GenFuzzy.h` and `GenProfiles.h` were generated from
`src/config/*.json` by a tool that no longer exists in this repository; they are
now hand-maintained, which is recorded as REQ-SYS-002 `PARTIAL` in docs/01.

## 8. What this architecture deliberately does not have

* **No recorder.** There is no SD writer and no session file: telemetry lives on
  the wire and events live in the panel's volatile log. Everything in docs/01
  §J is `NOT-IMPLEMENTED` because of this.
* **No simulation mode.** The hardware is the only target (REQ-SYS-006 `N/A`).
* **No network.** No AP, no internet, no browser (REQ-SYS-004).
* **No L\*a\*b\* on the MCU.** The panel shows the clear channel and a calibration
  status, never a colour value that has not been measured (REQ-COL-004).
* **No OTA, no authentication, no user accounts.** Physical access to the panel
  is the trust boundary.

## 9. Build and deployment

| Target | Entry point | Toolchain | Notes |
|---|---|---|---|
| Controller | `src/esp/ColAromatic/ColAromatic.ino` | Arduino IDE / `arduino-cli`, `esp32:esp32:esp32` | see docs/17 §2 |
| Panel | `src/esp/HMI/HMI.ino` | Arduino IDE with LVGL 8 + `esp_panel` | see docs/17 §7; board config still the 7″ reference one |

Both boards print their station MAC at boot; each MAC is pasted into the other
firmware's peer constant (or adopted by the pairing handshake), then the panel
must show state within 5 s of boot. There is nothing else to deploy.
