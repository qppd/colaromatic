# 14 — Handover checklist

Use this at the end of each phase and again at final turnover. Every line is
either **done** with an artifact, or **open** with an owner and a date. Nothing
is marked done on the strength of intent.

## A. Software deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| A1 | Source code, complete and organised by layer | `src/colaromatic/` | done |
| A2 | Dependency list and installation steps | `pyproject.toml`, `README.md` §Quick start | done |
| A3 | Operator dashboard (selectors, live values, charts, controls, completion, export) | `src/colaromatic/web/` | done |
| A4 | Fuzzy controller with configurable terms and rules | `src/colaromatic/control/fuzzy.py`, `src/config/controllers/default_provisional.json` | done (rule base UNVALIDATED) |
| A5 | Profile engine with 12 bean/roast profiles and versioning | `src/colaromatic/profiles/`, `src/config/profiles/` | done (all PROVISIONAL) |
| A6 | Colour processing module + calibration tool | `processing/color.py`, `src/tools/color_calibrate.py` | done (no measured calibration) |
| A7 | RoR calculation module | `processing/temperature.py` | done |
| A8 | E-nose acquisition and baseline handling | `sensors/`, `app/engine.py` | done (sim + protocol) |
| A9 | Stage monitoring with estimated/confirmed status | `processing/stages.py` | done |
| A10 | Safety logic with latched faults and gating | `control/safety.py`, `app/engine.py` | done (SW only) |
| A11 | Data logging and CSV/JSON export | `storage/` | done |
| A12 | Automated tests | `src/tests/` (155 passing) | done |
| A13 | Headless simulation runner | `src/tools/run_simulation.py` | done |
| A14 | Commissioning approval CLI | `src/tools/grant_commissioning_approval.py` | done |

## B. Firmware deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| B1 | ESP32 firmware implementing the protocol, interlocks and watchdog | `src/esp/ColAromatic/ColAromatic.ino` + `HeaterSafety.*`, `HostLink.*` | **compiles**, not flashed |
| B2 | Arduino IDE sketch layout: pins in `PINS.h`, settings in `CONF.h`, one `.h`/`.cpp` pair per component | `src/esp/ColAromatic/` | done |
| B3 | Firmware build, flashing and verification guide | `docs/17_firmware_build_flash.md` | done |
| B4 | Controller flashed and functional on the real board | — | open |
| B5 | Pin map verified against the delivered board and harness | `docs/04_wiring_and_pinmap.md` §1 | open |

## C. Hardware deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| C1 | Verified BOM with part numbers, quantities and ratings | `docs/03...` §1 | open (quotation not supplied) |
| C2 | Missing-component list | `docs/03...` §6 | done as a list; procurement open |
| C3 | Measured heater voltage/current/inrush and protection sizing | `docs/04...` §3 | open |
| C4 | Independent thermal cutoff installed and tested with the controller off | test record | open |
| C5 | SSR type confirmed and switching verified with a lamp load (heater and fan SSRs) | test record | open |
| C6 | Enclosure, grounding, strain relief, fuse/breaker | photos + measurements | open |
| C7 | Sensor mounting: probe position, colour optics, e-nose sampling | photos + drawings | open |
| C8 | Wiring diagram as built | schematic | open |
| C9 | Assembly records (who, when, what) | log | open |

## D. Calibration and validation deliverables

| # | Item | Artifact | State |
|---|---|---|---|
| D1 | Colour references measured and calibration fitted | `src/config/calibration/*.json` (`VALIDATED`) | open |
| D2 | Independent colour verification (held-out patches) | verification record | open |
| D3 | Thermocouple verified against a reference | test record | open |
| D4 | Plant characterisation (fixed-duty temperature response) | dataset + analysis | open |
| D5 | Fuzzy rule base tuned from measured behaviour | controller config v2 | open |
| D6 | Baseline comparison (fixed duty vs manual vs fuzzy) | analysis report | open |
| D7 | Per-profile colour/temperature/RoR/endpoint records | updated profiles | open |
| D8 | Roast trials per `docs/10_experimental_protocol.md` | session datasets | open |
| D9 | Tolerance/acceptance thresholds approved by the adviser | signed test plan | open |

## E. Documentation deliverables

| # | Document | State |
|---|---|---|
| E1 | `01_requirements_traceability.md` | done |
| E2 | `02_architecture.md` | done |
| E3 | `03_hardware_bom_gap_analysis.md` | done (awaiting real quotation) |
| E4 | `04_wiring_and_pinmap.md` | done (provisional) |
| E5 | `05_serial_protocol.md` | done |
| E6 | `06_fuzzy_design.md` | done |
| E7 | `07_color_calibration.md` | done |
| E8 | `08_data_schema.md` | done |
| E9 | `09_test_plan.md` (with actual results) | done (hardware sections pending) |
| E10 | `10_experimental_protocol.md` | done (needs approval) |
| E11 | `11_user_manual.md` | done |
| E12 | `12_troubleshooting.md` | done |
| E13 | `13_known_limitations.md` | done |
| E14 | `14_handover_checklist.md` | this document |
| E15 | `15_risk_register.md` | done |
| E16 | `16_project_plan.md` | done |
| E17 | Thesis/report chapter mapping to these documents | open |

## F. Final turnover checks

- [ ] `python -m pytest` passes on the handover machine (record the output).
- [ ] `python -m colaromatic.app.server --mode sim --open` runs a complete roast
      and produces a session directory with CSV, JSONL, profile and summary.
- [ ] `arduino-cli compile --fqbn esp32:esp32:esp32 src/esp/ColAromatic` (or the
      Arduino IDE **Verify**) succeeds; the output is recorded.
- [ ] Exported CSV and JSON of one session were opened in a spreadsheet and in a
      script, and the values matched the GUI completion screen.
- [ ] A simulated session is clearly labelled `data_source=simulated` in every
      artifact handed over.
- [ ] No claim in the report contradicts `docs/13_known_limitations.md`.
- [ ] Every session referenced in the report exists in the handed-over dataset
      with `data_source=hardware`.
- [ ] The tested configuration is frozen: exact commit/build, config versions,
      profile versions, calibration ids and firmware version recorded in one page.
- [ ] Known defects listed with severity and whether they affect results.
- [ ] Hardware is handed over with the as-built wiring diagram, pin map, and a
      safety briefing on the thermal cutoff and E-stop.
- [ ] Storage of the datasets agreed (media, location, backup, retention).
- [ ] Outstanding items from this checklist transferred with owners and dates.

## G. Known defects at handover

| # | Defect | Severity | Impact | Owner |
|---|---|---|---|---|
| G1 | Colour calibration is synthetic | high | L*a*b*, ΔE and the colour endpoint cannot be reported | researcher |
| G2 | Profiles have no measured targets | high | automatic control is supervised-only; endpoints are operator-confirmed | researcher |
| G3 | Fuzzy rule base unvalidated | medium | no performance claim possible | researcher |
| G4 | Firmware never flashed/tested | high | hardware path unproven end to end | both |
| G5 | No independent thermal cutoff installed | high (safety) | must not be energised without it | builder |
| G6 | Colour-change stage detection disabled | low | operator confirms the stage manually | researcher |
| G7 | Acoustic crack detection uncontrolled | low | disabled by default; needs an audio survey | researcher |
| G8 | 30 s research log interval is coarse for control analysis | low | control traces are sparse; reduce and record the deviation | researcher |
| G9 | No authentication on the API | low | localhost-only operation | operator |
| G10 | Fan operation not sensed | medium | cooling cannot be proven | builder |
