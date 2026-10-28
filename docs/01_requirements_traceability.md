# 01 — Requirements register and traceability matrix

**Scope of this revision.** The deliverable is now a **standalone machine**: an
ESP32 controller (`src/esp/ColAromatic/`) that owns the roast session, the
control loop and every safety interlock, plus an ESP32-S3 touch panel
(`src/esp/HMI/`) that is a thin operator client over ESP-NOW. There is no PC in
the machine.

The earlier Python host — engine, web dashboard, simulation mode, JSON
configuration, test suite — was removed from this repository in commit
`31e4632`. Requirements that only described that host are marked
`N/A (host removed)` rather than deleted, so the old identifiers still resolve.

> **Source** column references the section numbers of the project proposal, not
> documents in this repository. Where a proposal section has a counterpart here
> it is cited by number; docs/08, 09 and 16 exist in this repository and describe
> the machine as built.

## Validation status

| Status | Meaning |
|---|---|
| `CODE` | implemented in this repository; reviewed and parsed as C++, **not executed** |
| `PARTIAL` | part of the requirement has an implementation, part does not (stated in the row) |
| `BUILDS` | a compile of this source state is recorded, with the output kept (see docs/17 §3) |
| `PENDING-HW` | needs the physical machine, wiring or load |
| `PENDING-TRIAL` | needs roast trials and/or calibration measurements |
| `REQUIRES-APPROVAL` | the value must be signed off before it is used |
| `NOT-IMPLEMENTED` | nothing in this repository does this |
| `N/A` | out of scope by design, with the reason in the row |

**Nothing here is `VERIFIED-SW`.** The status vocabulary is deliberately weaker
than an earlier revision of this document: the automated test suite was part of
the removed host, and the repository now contains **no tests at all** (0 test
files). No requirement may be marked validated on the strength of a
plausible-looking number, and none is marked validated here.

The only evidence available today is: (a) static review of the source, (b) a
C++ parse of every file, (c) a numerically reproduced defuzzification surface for
the colour loop (REQ-COL-006), and (d) the historical controller build recorded
in docs/17 §3. No firmware in this repository has been flashed or run.

## A. System-level

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-SYS-001 | The machine runs a complete roast without a PC in the loop | §5, §17 | `ColAromatic.ino`, `RoastControl.*` | IDLE → RUNNING/HOLD → COOLING → DONE driven only from the panel | `CODE` |
| REQ-SYS-002 | Tunable values live in configuration, not scattered in code | §17 | `CONF.h`, `GenConfig.h`, `GenProfiles.h`, `GenFuzzy.h` | All thresholds/timings reachable from those headers; no magic numbers elsewhere | `PARTIAL` — the generator (`src/tools/gen_firmware_tables.py`) was removed with the host, so the generated headers are hand-maintained |
| REQ-SYS-003 | Derived or uncalibrated values are never presented as measurements | §3.1 | panel wording, `hmi_data.cpp` | UI labels state "uncalibrated" / "proxy" for colour, and "n/a" for missing values | `CODE` |
| REQ-SYS-004 | Fully offline operation: no AP, no internet, no PC | §17 | `HmiLink.*`, `esp_now_link.cpp` | Both boards are WiFi station mode on one fixed channel, ESP-NOW only | `CODE` |
| REQ-SYS-005 | Clear authority split: controller owns control and safety, panel owns none | §5, §16 | `RoastControl.*`, `HeaterSafety.*`, `src/esp/HMI/` | Panel loss or a hostile panel cannot energise the heater; panel only sends commands | `CODE` |
| REQ-SYS-006 | Runs against a simulated machine | §5, §17 | — | Full roast with simulated inputs | `N/A (host removed)` — the hardware is now the only target |

## B. Profiles

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-PROF-001 | 4 bean varieties × 3 roast levels resolve to one profile each | §6 | `GenProfiles.h`, panel profile picker | 12 profiles present, selectable by index, 0 missing | `CODE` |
| REQ-PROF-002 | A validated profile is never silently overwritten | §14 | — | Write refused; version archived | `NOT-IMPLEMENTED` — profiles are compiled in; there is no editor or persistence on the machine |
| REQ-PROF-003 | Insufficient data is shown as a calibration warning | §6.2 | `GenProfiles.status`, panel wording | Provisional status is visible to the operator | `PARTIAL` — the controller refuses automatic start for non-eligible profiles, but the panel does not yet render a per-profile "PROVISIONAL" badge |
| REQ-PROF-004 | An unvalidated profile cannot energise the heater automatically | §6.2, §9.2 | `RoastControl::start` (`autoEligible`) | Automatic start refused with a stated reason | `CODE` — all 12 shipped profiles have `autoEligible = 0`, so automatic mode cannot start at all today; that is the intended conservative behaviour |
| REQ-PROF-005 | Operator setpoint validated against the profile limit | §6 | `RoastControl::setSetpoint/start` | Out-of-range setpoint refused with a reason | `CODE` |
| REQ-PROF-010 | Session records profile and calibration versions | §15.1 | — | Session metadata contains versions | `NOT-IMPLEMENTED` — needs the recorder in section J |

## C. Colour

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-COL-001 | Raw channel measurements available with quality and timestamp | §7.1 | `ColorSensor.*`, `LinkState` | Raw Hz logged or explicitly empty | `PARTIAL` — the controller samples all channels internally and derives the ratio, but the link carries only the clear channel and a calibration status; per-channel raw values reach the bench serial frame, not the panel |
| REQ-COL-002 | L\*a\*b\* only through a documented colorimetric transform | §7.1 | — | XYZ→Lab matches D65 reference | `NOT-IMPLEMENTED` — no colorimetry on the MCU, by decision (docs/13) |
| REQ-COL-003 | Calibration record keeps identity, references, method, residuals, version | §7.2 | — | Record persists with residual metrics | `NOT-IMPLEMENTED` |
| REQ-COL-004 | No generic RGB→Lab conversion on raw counts | §7.1 | `ColorSetpoint.*` | Without calibration, no L\*a\*b\* is reported | `CODE` — no Lab value exists anywhere in the firmware |
| REQ-COL-005 | Colour repeatability during roasting evaluated | §7.1 | — | Repeatability within an approved tolerance | `PENDING-HW` |
| REQ-COL-006 | Colour error maps to the setpoint continuously and monotonically | §9.1 | `ColorSetpoint.*` | No discontinuity in the surface; lighter → hotter | `CODE` — adjacent input terms sum to 1.0 across −0.30…+0.30 and each term has its own consequent, so the centroid cannot step; sampled numerically during review: monotone, largest step 0.15 °C (the previous three-term table stepped 30 °C) |
| REQ-COL-007 | Colour target ratio is a placeholder until measured | §7.1 | `CONF.h` `COLOR_TARGET_RATIO` | Documented as uncalibrated | `REQUIRES-APPROVAL` |

## D. Temperature and RoR

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-TEMP-001 | Timestamped temperature with filtering and status | §8 | `ThermocoupleSensor.*`, `RoastControl::updateTemperature` | Reading carries timestamp, raw, filtered, quality | `CODE` |
| REQ-TEMP-002 | RoR uses actual elapsed time, not an assumed interval | §8.1 | `RoastControl::updateRoR` | RoR correct for irregular sampling | `CODE` — least-squares slope over the sample timestamps in the window |
| REQ-TEMP-003 | Missing, duplicate, invalid, stale and noisy readings are handled | §8.1 | `RoastControl`, `HeaterSafety`, `ThermocoupleSensor` | Each condition produces a defined status and, where safety-relevant, a trip | `CODE` |
| REQ-TEMP-004 | Probe arrangement (drum vs bean) decided and documented | §8.2 | — | Documented decision with evidence | `PENDING-HW` |
| REQ-TEMP-005 | Thermocouple verified against a reference | §18C | — | Error within approved tolerance | `PENDING-HW` |
| REQ-TEMP-006 | MAX6675 conversion time respected | §8 | `CONF.h`, `ThermocoupleSensor` | No read faster than the conversion time | `CODE` — `TEMP_INTERLOCK_PERIOD_MS >= MAX6675_CONVERSION_MS` is a compile-time `#error` |

## E. Control (colour outer loop + PID inner loop)

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-FUZ-001 | Membership functions and linguistic variables are configurable | §9.3 | `Fuzzy.*`, `ColorSetpoint.cpp` | Terms loaded as data, geometry validated | `PARTIAL` — fully data-driven tables, but they are compile-time C++ arrays, not loadable configuration |
| REQ-FUZ-002 | Every rule carries an id and a rationale/status | §9.4 | `GenFuzzy.h`, `ColorSetpoint.cpp` | No rule without rationale; status not `VALIDATED` | `PARTIAL` — ids are in the tables; rationale and status live in docs, not in the data |
| REQ-FUZ-003 | Bounded heater output respecting limits and constraints | §9.2 | `Pid`, `RoastControl::shapeDuty`, `HeaterSafety` | Output clamped to ceiling, minimum duty and minimum on/off | `CODE` |
| REQ-FUZ-004 | The controller is not the sole over-temperature protection | §9.2 | `HeaterSafety` | Latches remove heater authority independently of any commanded duty | `CODE` (software) / `PENDING-HW` (independent cutoff) |
| REQ-FUZ-005 | Rule base tuned and compared against a baseline | §9.5 | — | Documented metrics vs baseline | `PENDING-TRIAL` |
| REQ-FUZ-006 | Membership boundaries justified by measured data | §9.4 | — | Boundaries derived from measured roasts | `PENDING-TRIAL` / `REQUIRES-APPROVAL` |
| REQ-CTRL-001 | Automatic mode is two loops in series: colour error → setpoint → duty | §9.1 | `ColorSetpoint::setpointC`, `RoastControl::onControlTick` | Setpoint derived from colour, duty from the PID | `CODE` |
| REQ-CTRL-002 | PID uses derivative on measurement and conditional integration | §9.2 | `Pid::compute` | No derivative kick on a setpoint step; no windup through the duty ceiling | `CODE` |
| REQ-CTRL-003 | An unusable colour reading falls back to the trajectory / operator setpoint | §9.1 | `RoastControl::onControlTick`, `targetAt` | Missing or saturated colour never produces a meaningless target | `CODE` |
| REQ-CTRL-004 | Manual duty mode for commissioning and baselines | §13.2 | `RoastControl` (`MODE_MANUAL`), panel duty slider | Fixed duty without the PID | `CODE` |
| REQ-CTRL-005 | A live session fixes the mode and the profile | §13.2 | `RoastControl::setMode/selectProfile` | Entering automatic or switching profile mid-roast is refused | `CODE` |

## F. E-nose

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-ENOSE-001 | Gas and environment collected with status | §10 | `EnoseSensor.*`, `LinkState`, bench frame | All fields parsed and logged or empty | `PARTIAL` — gas resistance reaches the panel; temperature, humidity and pressure only appear in the bench serial frame; nothing is logged |
| REQ-ENOSE-002 | Warm-up and baseline acquisition | §10.1 | `EnoseSensor` | Baseline recorded with sample count | `PARTIAL` — warm-up timing exists; no R₀ baseline is captured |
| REQ-ENOSE-003 | Gas response is not presented as aroma identification | §10 | panel wording, docs/13 | Wording present in UI and docs | `CODE` |
| REQ-ENOSE-004 | Sampling location protects the sensor from heat and condensation | §10.2 | — | Sensor temperature within limit during a roast | `PENDING-HW` |
| REQ-ENOSE-005 | Aroma classification, if attempted, is a separate validated feature | §10.1 | — | Model, labels and evaluation exist | `NOT-IMPLEMENTED` |

## G. Stages and endpoint

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-STAGE-001 | The required stage labels are supported in order | §11 | `LinkFrames.h`, `hmi_data.cpp` | Stages present in order | `CODE` — eight labels: charge, turning point, colour change, first crack, medium, second crack, dark, done |
| REQ-STAGE-002 | Detection method, confidence and confirmation are recorded | §11.6 | `RoastControl::advanceStage`, `LinkState` | Milestone exported with method | `PARTIAL` — method and confirmed/estimated reach the panel and the event frame; there is no record with timestamps |
| REQ-STAGE-003 | Turning point estimated from temperature history only | §11.1 | `RoastControl::checkStage` | Estimated flag set; threshold provisional | `CODE` |
| REQ-STAGE-004 | Cracks are not claimed without evidence | §11.3, §11.5 | `RoastControl::notifyAcoustic`, `AcousticSensor` | Acoustic path disabled by default; confidence threshold applied; operator confirmation available | `CODE` |
| REQ-STAGE-005 | Stage criteria calibrated per profile | §11.2, §11.4 | — | Criteria recorded per profile | `PENDING-TRIAL` |
| REQ-STAGE-006 | Colour-change detection is not claimed by the firmware | §11.2 | `RoastControl::checkStage` | No automatic colour stage on the machine | `CODE` — documented as disabled; an operator can still confirm the stage by eye |
| REQ-STAGE-007 | Endpoint rules: maximum duration is immediate, everything else needs the operator | §11.6 | `RoastControl::checkEndpoint` | Automatic criteria never end a roast by themselves | `CODE` |

## H. Timing

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-TIME-001 | Independent schedules for acquisition, control, telemetry and links | §12 | `GenConfig.h`, `CONF.h`, `RoastControl::service` | Intervals independent and configurable | `CODE` — sensor 1 s, control 2 s, interlock 250 ms, serial report 1 s, beacon/state 500 ms |
| REQ-TIME-002 | Stage transitions, faults and heater events are recorded when they occur | §12 | `RoastControl::pushEvent`, `HmiLink::sendEvent` | Events present in a durable log | `PARTIAL` — events are emitted and shown on the panel's diagnostics screen, but nothing is persisted |
| REQ-TIME-003 | The GUI is never used for safety or control | §12 | `src/esp/HMI/`, `HeaterSafety` | The panel can be switched off without affecting control | `CODE` — panel silence removes heater authority, which is the defined safe direction |
| REQ-TIME-004 | Intervals justified by measurement | §12 | — | Timing report with measured latency | `PENDING-HW` |

## I. Panel (GUI)

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-GUI-001 | Bean and roast-level selectors resolve to a profile | §6, §13.2 | `control_screen.cpp` | Selection shows the profile | `CODE` |
| REQ-GUI-002 | Dashboard shows every required live value | §13.1 | `dashboard_screen.cpp` | Temperature, target, RoR, duty, stage, mode, session time, link state | `PARTIAL` — colour is shown as calibration status and light level, not as a colour value or ΔE |
| REQ-GUI-003 | Live charts for temperature, RoR, colour and gas | §13.3 | `analytics_screen.cpp` | Charts render from live telemetry | `CODE` — temperature with target, RoR, clear channel, gas; history restarts when the screen is re-opened |
| REQ-GUI-004 | Start prevented when sensors are invalid or an interlock is active | §13.2 | `RoastControl::start`, `dispatchHmi` | Start refused, with the reason shown | `CODE` |
| REQ-GUI-005 | Stopping the heater does not stop cooling or ventilation | §13.2 | `RoastControl::beginCooling`, `ColAromatic.ino` | Cooling runs with the heater off and the fan at full | `CODE` |
| REQ-GUI-006 | Completion appears only after the stopping conditions are met | §13.4 | `RoastControl`, `alert_popup.cpp` | Popup only on DONE | `CODE` |
| REQ-GUI-007 | Completion reports the session summary | §13.4 | `alert_popup.cpp` | Colour, gas, temperature, RoR, duration, profile, reason | `PARTIAL` — session time only; the rest of the summary needs the recorder |
| REQ-GUI-008 | Export actions available from the GUI and the API | §13.3, §15.3 | — | Downloads return logged data unchanged | `NOT-IMPLEMENTED` |
| REQ-GUI-009 | The GUI runs on the actual HMI | §13 | `src/esp/HMI/` | HMI protocol confirmed; GUI deployed | `PENDING-HW` — the panel firmware is written and statically checked, but has never been compiled or flashed, and `esp_panel_board_custom_conf.h` is still the 7″ reference board configuration |
| REQ-GUI-010 | Operations that stop the machine are reachable in one tap | §13.2 | `dashboard_screen.cpp`, `control_screen.cpp` | E-stop and STOP & COOL always available while a session is live | `CODE` |

## J. Data and export

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-LOG-001 | Session metadata incl. ids, versions, operator, notes, completion | §15.1 | — | Metadata file complete | `NOT-IMPLEMENTED` |
| REQ-LOG-002 | Full time series incl. raw and processed values | §15.2 | — | Stable column set | `NOT-IMPLEMENTED` — the panel has no SD writer; telemetry exists only on the wire |
| REQ-LOG-003 | CSV + JSON export with units and a missing-value convention | §15.3 | — | Empty field for missing; no substitution | `NOT-IMPLEMENTED` |
| REQ-LOG-004 | Storage failure surfaces as a defect, not silence | §16 | — | `STORAGE_FAILURE` fault raised | `N/A` — there is no storage to fail; revisit with the recorder |
| REQ-LOG-005 | Research summaries (duration, colour, trends, heater, stages, faults) | §15.4 | — | Summary contains all required keys | `NOT-IMPLEMENTED` |
| REQ-LOG-006 | Statistical claims only with a valid design and real results | §15.4 | — | Analysis plan approved | `PENDING-TRIAL` |

## K. Safety

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-SAFE-001 | Independent over-temperature protection not relying on the ESP32 | §16 | — | Cutoff removes heater power with the controller powered off | `PENDING-HW` |
| REQ-SAFE-002 | Correct SSR switching method and heatsinking | §16 | `HeaterSafety::service`, `setFan` | SSR type confirmed; duty within limits; fan on/off only | `PENDING-HW` |
| REQ-SAFE-003 | Over-temperature fault latches and removes heater authority | §16 | `HeaterSafety` | Latched until reset while cool | `CODE` — process limit latches; the on-board backstop clears on hysteresis |
| REQ-SAFE-004 | Probe disconnection or implausible value → defined safe state | §16 | `HeaterSafety`, `ColAromatic.ino` | Heater off, session aborted, cooling started | `CODE` |
| REQ-SAFE-005 | Rate-of-rise runaway detection | §16 | `RoastControl`, `HeaterSafety::latchRunaway` | Latched after 3 consecutive evaluations | `CODE` (threshold `REQUIRES-APPROVAL`) |
| REQ-SAFE-006 | Link failure → defined behaviour | §16 | `HeaterSafety` (both watchdogs) | Heater off after silence; start refused | `CODE` — 5 s without a panel frame, 3 s without a recognised serial frame |
| REQ-SAFE-007 | E-stop handled safely and latched | §16 | `HeaterSafety::engageEstop`, panel E-STOP button | Heater off; release gated on a cool batch | `CODE` |
| REQ-SAFE-008 | Fuse, grounding, isolation, strain relief, high-temperature wiring | §16 | — | Electrical review signed off | `PENDING-HW` |
| REQ-SAFE-009 | Startup and shutdown sequences defined | §16 | `setup()`, `HeaterSafety::begin` | Heater off at boot and at the end of every path | `CODE` |
| REQ-SAFE-010 | Gas-safety architecture if a gas roaster is used | §16 | — | Separate reviewed design | `N/A` — electric baseline |
| REQ-SAFE-011 | A latched fault is cleared only when it is safe to clear it | §16 | `HeaterSafety::resetFaults` | Refused while hot, or while the E-stop input is still active | `CODE` |
| REQ-SAFE-012 | Every trip clears the duty request, not just the output | §16 | `HeaterSafety::service` | No automatic restart when the condition clears; a new command is required | `CODE` |

## L. Firmware and links

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-FW-001 | Bench serial protocol documented and implemented | §17 | `HostLink.*`, docs/05 | Frames parse; commands match the table | `CODE` |
| REQ-FW-002 | Board fail-safe: host silence removes heater authority | §16 | `CONF.h`, `HeaterSafety` | Heater off after silence | `CODE` |
| REQ-FW-003 | Pin map verified against the real board and harness | §17 | `PINS.h`, docs/04 | Signed pin map | `PENDING-HW` |
| REQ-FW-004 | Every radio frame is integrity-checked before it can change state | §17 | `LinkFrames.h` (`linkFrameCrc`), `HmiLink`, `esp_now_link` | Magic, version, length and CRC16 validated; commands applied exactly once by sequence number | `CODE` |
| REQ-FW-005 | The two boards are addressed by MAC, with pairing that recovers | §17 | `CONF.h` `HMI_PEER_MAC`, `esp_now_link`, `HmiLink` | Each board's MAC is printed at boot and can be burned into the other; peer stored in NVS; a replaced board is re-adopted | `CODE` |
| REQ-FW-006 | The panel cannot bypass any interlock | §16 | `dispatchHmi`, `HeaterSafety` | No opcode can force heater on outside the duty path; interlocks evaluate every pass | `CODE` |
| REQ-FW-007 | Commands are retried without being applied twice | §17 | `LinkFrames.h` `LINK_CMD_REPEATS`, `HmiLink::handleFrame` | Duplicate sequence numbers are re-ACKed, not re-applied | `CODE` |

## M. Verification

| ID | Requirement | Source | Module | Acceptance criterion | Status |
|---|---|---|---|---|---|
| REQ-VER-001 | The controller sketch compiles for its board target | §17 | `src/esp/ColAromatic/` | Recorded build output | `PARTIAL` — a build is recorded in docs/17 §3 for an earlier source state; the current tree has not been compiled here (no toolchain on the build machine) |
| REQ-VER-002 | The panel sketch compiles for its board target | §17 | `src/esp/HMI/` | Recorded build output | `NOT-IMPLEMENTED` — never compiled; LVGL 8, `esp_panel` and a matching `lv_conf.h` are required |
| REQ-VER-003 | Automated tests cover the safety and control logic | §17 | — | a test suite runs and passes | `NOT-IMPLEMENTED` — the suite was removed with the host; 0 test files remain. What to write is specified in docs/09 §B |
| REQ-VER-004 | Control logic verified numerically before hardware | §9.5, §17 | `ColorSetpoint`, `Pid` | Reproducible numeric behaviour | `PARTIAL` — the colour surface was reproduced from its source tables during this revision; the PID has only been reasoned through, not simulated |

## Open decisions that block safe implementation

1. Exact ESP32 module and confirmed free GPIOs (the pin map depends on it).
2. Panel model and its `esp_panel` configuration — the repository still carries
   the reference project's **Waveshare ESP32-S3-Touch-LCD-7** configuration
   (800×480, ST7262) while the project README names the fitted panel as the
   4.3″ model; if that is correct, `ESP_PANEL_BOARD_NAME` and the layout
   constants must change before the first flash.
3. SSR type (zero-cross vs random-fire) — determines whether time-proportioning
   is legal.
4. Heater voltage/current per element and the resulting protection sizing.
5. Whether the second heater SSR channel is wired, and confirmation that the fan
   has its own SSR (on/off only).
6. Probe arrangement: drum/environment temperature vs bean temperature.
7. BME688 sampling arrangement (headspace vs exhaust vs diluted sample).
8. Whether an acoustic channel is available for crack detection.
9. Whether on-machine recording is required (section J is entirely
   `NOT-IMPLEMENTED`; without it there is no roast evidence outside the panel's
   volatile log).
