# 14 — Handover checklist

Use this at the end of each phase and again at final turnover. Every line is
either **done** with an artifact that exists in this repository, or **open** with
what is missing. Nothing is marked done on the strength of intent.

**Scope note.** The deliverable is a standalone machine: an ESP32 controller
(`src/esp/ColAromatic/`) plus an ESP32-S3 panel (`src/esp/HMI/`), talking over
ESP-NOW by MAC. The earlier Python host, its web dashboard, its simulation mode
and its test suite were removed in commit `31e4632`; entries that referenced
them are now open or removed outright, and `docs/01` §J records the capability
gap that leaves.

## A. Controller firmware deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| A1 | Sketch entry point, module per component, pins in `PINS.h`, settings in `CONF.h` | `src/esp/ColAromatic/` | done |
| A2 | Session state machine: idle, running, hold, cooling, done | `RoastControl.*` | done (never executed) |
| A3 | Temperature processing: EMA filter, least-squares RoR, quality states | `RoastControl.cpp`, `ThermocoupleSensor.*` | done (never executed) |
| A4 | Two-loop control: colour→setpoint (fuzzy) + PID→duty | `ColorSetpoint.*`, `Pid.*` | done; colour surface verified numerically, PID not simulated |
| A5 | Duty shaping: ceiling, minimum duty, minimum on/off quantisation | `RoastControl::shapeDuty`, `HeaterSafety` | done |
| A6 | Interlocks, latches, watchdogs, E-stop, separate fan SSR | `HeaterSafety.*` | done (never executed) |
| A7 | Profile table (12 bean/roast combinations) | `GenProfiles.h` | done (all PROVISIONAL, none auto-eligible) |
| A8 | Bench serial service protocol | `HostLink.*`, docs/05 | done |
| A9 | ESP-NOW link: frames, CRC, sequence de-duplication, pairing | `HmiLink.*`, `LinkFrames.h` | done |
| A10 | Event stream to the panel (stage, fault, cooling, heater) | `RoastControl::pushEvent`, `HmiLink::sendEvent` | done (ephemeral — see G2) |
| A11 | Recorded compile of the current source state | docs/17 §3 | **open** — the recorded build predates this revision; no toolchain was available to re-run it |
| A12 | Automated tests | — | **open** — no test files exist |

## B. Panel firmware deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| B1 | Sketch with board bring-up, LVGL 8, radio, UI timer | `src/esp/HMI/HMI.ino` | done (never compiled) |
| B2 | ESP-NOW client: beacons, MAC addressing, pairing, command wrappers | `esp_now_link.*` | done (never executed) |
| B3 | Telemetry model with one lock, event log, text tables | `hmi_data.*` | done |
| B4 | Screens: boot, dashboard, roast control, curves, diagnostics | `boot_screen.*`, `dashboard_screen.*`, `control_screen.*`, `analytics_screen.*`, `diagnostics_screen.*` | done |
| B5 | Screen lifecycle and navigation | `screen_manager.*` | done |
| B6 | Alerts: safety trips, command refusals, endpoint, completion, link loss | `alert_popup.*` | done |
| B7 | Theme, styles and shared widgets | `ui_theme.h`, `ui_styles.*` | done |
| B8 | Board support for the fitted panel | `lvgl_v8_port.*`, `esp_panel_board_custom_conf.h` | **open** — the file is still `ESP_PANEL_BOARD_NAME "Waveshare:ESP32-S3-Touch-LCD-7"` (800×480, ST7262) from the reference project, while the project README names the 4.3″ model; confirm the fitted panel and set the board name, or the display will not initialise |
| B9 | Pairing instructions for both boards | README, docs/17 §7 | done |
| B10 | Recorded compile of the panel sketch | — | **open** — needs LVGL 8, `esp_panel`, a matching `lv_conf.h` and a board FQBN |
| B11 | Panel flashed and paired with the controller on the bench | — | **open** |

## C. Hardware deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| C1 | Verified BOM with part numbers, quantities and ratings | docs/03 §1 | **open** (quotation not supplied) |
| C2 | Missing-component list | docs/03 §6 | done as a list; procurement open |
| C3 | Measured heater voltage/current/inrush and protection sizing | docs/04 §3 | open |
| C4 | Independent thermal cutoff installed and tested with the controller off | test record | **open** |
| C5 | SSR type confirmed and switching verified with a lamp load (heater and fan SSRs) | test record | **open** |
| C6 | Enclosure, grounding, strain relief, fuse/breaker | photos + measurements | open |
| C7 | Sensor mounting: probe position, colour optics, e-nose sampling | photos + drawings | open |
| C8 | Wiring diagram as built | schematic | open |
| C9 | Assembly records (who, when, what) | log | open |
| C10 | Panel power, backlight and touch verified on the bench | test record | **open** |

## D. Calibration and validation deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| D1 | Colour target ratio measured on the real machine | `CONF.h` `COLOR_TARGET_RATIO` | **open** — currently a placeholder, so the colour loop's target is meaningless |
| D2 | Colour reference patches measured and a transform fitted | calibration record | **open** (no colorimetry in the repository at all) |
| D3 | Thermocouple verified against a reference | test record | open |
| D4 | Plant characterisation (fixed-duty temperature response) | dataset + analysis | open |
| D5 | Fuzzy membership boundaries and output terms tuned from that data | `ColorSetpoint.cpp` tables v2 | **open** — the tables are geometrically sound but unmeasured |
| D6 | PID gains tuned on the plant | `CONF.h` `PID_KP/KI/KD` | **open** — placeholders, never exercised |
| D7 | Baseline comparison (fixed duty vs manual vs automatic) | analysis report | open |
| D8 | Per-profile colour/temperature/RoR/endpoint records | updated profiles | open |
| D9 | Roast trials per docs/10 | session datasets | open |
| D10 | Tolerance/acceptance thresholds approved by the adviser | signed test plan | open |

## E. Documentation deliverables

| # | Document | State |
|---|---|---|
| E1 | `01_requirements_traceability.md` | done (this revision: rewritten for the standalone system; §J open by design) |
| E2 | `02_architecture.md` | done (rewritten for controller + panel; the host layers were retired with it) |
| E3 | `03_hardware_bom_gap_analysis.md` | done (awaiting real quotation; §2 HMI decision now records the LVGL panel choice and the open board-config question) |
| E4 | `04_wiring_and_pinmap.md` | done (provisional; §4–§5 re-pointed at the firmware) |
| E5 | `05_serial_protocol.md` | done (bench service path only; no host parser remains; the ESP-NOW contract is `LinkFrames.h`) |
| E6 | `06_fuzzy_design.md` | done (rewritten for the shipped colour→setpoint + PID loop; the dormant 13-rule duty table is documented as unused) |
| E7 | `07_color_calibration.md` | done (rewritten for the browning-ratio path; still no calibration — §3 is the open work) |
| E8 | `08_data_schema.md` | done (frame, enum and table schemas; explicitly records that no session schema exists) |
| E9 | `09_test_plan.md` | done (A static checks, B tests to write, C hardware commissioning record — all of §B/§C open) |
| E10 | `10_experimental_protocol.md` | done (needs approval; steps re-pointed at the panel and the absence of a recorder) |
| E11 | `11_user_manual.md` | done (rewritten for panel-only operation, pairing, faults and refusals) |
| E12 | `12_troubleshooting.md` | done (rewritten for the panel/controller; serial path kept, host sections removed) |
| E13 | `13_known_limitations.md` | done (thresholds re-mapped to `GenConfig.h`/`CONF.h`; host-era sections removed) |
| E14 | `14_handover_checklist.md` | this document |
| E15 | `15_risk_register.md` | done (P3 re-scored as a panel/board-config risk; D1, D5, P7 reflect the absence of recording and tests) |
| E16 | `16_project_plan.md` | done (phases, gates, critical path and the staged fallback) |
| E17 | `17_firmware_build_flash.md` | done (controller build record + panel section §7) |

## F. Final turnover checks

- [ ] `arduino-cli compile --fqbn esp32:esp32:esp32 src/esp/ColAromatic` succeeds
      and the output is recorded in docs/17. (Not runnable during this revision:
      no toolchain on the build machine.)
- [ ] The panel sketch compiles for the fitted board, with LVGL 8 and the
      Montserrat sizes the theme uses, and the output is recorded.
- [ ] Both boards boot and print their station MAC; each MAC is pasted into the
      other's firmware (or the pairing handshake is used) and STATE frames are
      observed on the panel within 5 s of boot.
- [ ] Unplugging the panel mid-session removes heater authority within
      `LINK_TIMEOUT_MS` and the panel shows LINK LOST on reconnect.
- [ ] The independent thermal cutoff removes heater power **with the controller
      powered off**, and that test is recorded.
- [ ] The E-stop works with the harness connected and disconnected, and release
      is refused while the batch is hot.
- [ ] Duty never exceeds `GEN_DUTY_CEILING_PCT` on a scope or a lamp load, and
      the minimum on/off quantisation is visible.
- [ ] The fan SSR is confirmed on/off only, and it is on during cooling.
- [ ] A full roast is run with no beans, heater load disconnected, and the
      stage/endpoint sequence is observed from the panel.
- [ ] `docs/13_known_limitations.md` is read out to the operator, and no claim in
      the report contradicts it.
- [ ] No claim anywhere in the report is based on a value still marked
      `PENDING-HW`, `PENDING-TRIAL` or `REQUIRES-APPROVAL` in docs/01.
- [ ] The tested configuration is frozen on one page: commit hash, `CONF.h`
      values, generated header contents, panel board config, firmware versions.
- [ ] Known defects (section G) are listed with severity and whether they affect
      results.
- [ ] Hardware is handed over with the as-built wiring diagram, the pin map and a
      safety briefing on the thermal cutoff and the E-stop.
- [ ] Outstanding items are transferred with owners and dates.
- [ ] If roast data must survive the machine being switched off, the recorder
      (docs/01 §J) is implemented first — today nothing is persisted.

## G. Known defects at handover

| # | Defect | Severity | Impact | Owner |
|---|---|---|---|---|
| G1 | Colour target ratio is a placeholder; no colorimetry exists | high | the colour loop's target is not a measurement; no ΔE, no colour endpoint | researcher |
| G2 | No on-machine recording (no SD writer); events and telemetry are lost on power-off | high | no roast evidence outside the panel's volatile log; docs/01 §J is entirely open | both |
| G3 | Profiles have no measured targets and none is auto-eligible | high | automatic mode cannot start; all roasts are manual duty with operator-confirmed endpoints | researcher |
| G4 | Neither sketch has been compiled or flashed | high | the hardware path is unproven end to end | both |
| G5 | No independent thermal cutoff installed | high (safety) | must not be energised without it | builder |
| G6 | Panel board configuration is the reference project's one (`Waveshare:ESP32-S3-Touch-LCD-7`, 800×480) while the README names the 4.3″ model | high | if the fitted panel differs, the display will not initialise | builder |
| G7 | PID gains and fuzzy boundaries are untuned | medium | no performance claim is possible; overshoot unknown | researcher |
| G8 | Colour-change stage detection disabled | low | operator confirms the stage by eye | researcher |
| G9 | Acoustic crack detection uncontrolled | low | disabled by default; needs an audio survey | researcher |
| G10 | Fan operation is not sensed | medium | cooling cannot be proven, only commanded | builder |
| G11 | No automated tests remain in the repository | medium | every future change is unverified until a suite is restored | both |
| G12 | ~~Docs 02, 06 and 11 still describe the removed host~~ | closed | docs 02, 03, 04, 05, 06, 07, 10, 11, 12, 13 and 15 were rewritten or re-pointed at the firmware, and docs 08, 09 and 16 were written, in the same revision | researcher |
