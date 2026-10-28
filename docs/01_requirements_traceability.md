# 01 — Requirements register and traceability matrix

Every requirement below is traceable to a module, a hardware dependency, a test
and an acceptance criterion. **Validation status** records what has actually
been demonstrated in this repository:

| Status | Meaning |
|---|---|
| `VERIFIED-SW` | demonstrated by an automated test with simulated inputs |
| `BUILDS` | compiles/builds; behaviour not exercised |
| `PENDING-HW` | requires physical hardware; procedure written, not executed |
| `PENDING-TRIAL` | requires roasting trials and/or calibration measurements |
| `REQUIRES-APPROVAL` | value/threshold must be approved before it is used |

No requirement is marked validated on the basis of a plausible-looking number.

## A. System-level

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-SYS-001 | Modular design that runs against a simulated machine without hardware | §5, §17 | `sensors/base.py`, `sensors/simulator.py`, `app/engine.py` | none | `test_engine.py`, `test_api.py` | Full roast completes with `data_source=simulated` | VERIFIED-SW |
| REQ-SYS-002 | Tunable values live in configuration, not code | §17 | `core/config.py`, `src/config/*.json` | none | `test_engine.py::test_manual_roast_runs_to_completion` | Changing `system.json` changes behaviour without code edits | VERIFIED-SW |
| REQ-SYS-003 | Simulated values are never presented as measurements | §3.1, §3.2 | `core/types.py`, `storage/session.py`, `web/app.js` | none | `test_engine.py`, `test_storage_and_bridge.py` | `data_source` present on every row, session and screenshot banner | VERIFIED-SW |

## B. Profiles

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-PROF-001 | 4 bean varieties × 3 roast levels resolve through one engine | §6 | `profiles/manager.py` | none | `test_profiles.py::test_all_twelve_combinations_resolve` | 12 profiles load, 0 missing | VERIFIED-SW |
| REQ-PROF-002 | A validated profile is never silently overwritten | §14 | `profiles/manager.py::save` | none | `test_profiles.py::test_save_refuses_to_overwrite_validated_profile` | Write refused; version bump archives previous | VERIFIED-SW |
| REQ-PROF-003 | Insufficient data is displayed as a calibration warning | §6.2 | `profiles/model.py::warnings`, `web/app.js` | none | `test_profiles.py::test_shipped_profiles_are_provisional_and_unpopulated` | Warning list non-empty for provisional profiles | VERIFIED-SW |
| REQ-PROF-004 | Unvalidated profiles cannot energise the heater automatically without approval | §6.2, §9.2 | `profiles/manager.py::permission`, `app/engine.py` | none | `test_profiles.py`, `test_engine.py::test_auto_blocked_without_approval` | Start refused; approval path documented and capped | VERIFIED-SW |
| REQ-PROF-010 | Every session records profile and calibration versions | §15.1 | `storage/session.py::session_metadata` | none | `test_engine.py` | `session.json` contains profile/controller/calibration versions | VERIFIED-SW |

## C. Colour

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-COL-001 | Raw channel measurements logged with quality and timestamp | §7.1 | `processing/color.py`, `storage/session.py` | TCS3200 + illumination | `test_storage_and_bridge.py` | Raw Hz columns populated or explicitly empty | VERIFIED-SW |
| REQ-COL-002 | L*a*b* produced only through a documented colorimetric transform | §7.1 | `processing/color.py` | TCS3200 + reference instrument | `test_color.py::test_white_point_maps_to_l100` | XYZ→Lab matches D65 reference values | VERIFIED-SW |
| REQ-COL-003 | Calibration record keeps identity, references, method, residuals, version | §7.2 | `processing/color.py::CalibrationRecord` | reference patches | `test_color.py::test_calibration_round_trip_on_disk` | Record persists with residual metrics | VERIFIED-SW |
| REQ-COL-004 | No generic RGB→Lab conversion on raw counts | §7.1 | `processing/color.py::ColorProcessor` | none | `test_color.py::test_no_calibration_reports_no_lab` | Without calibration no L*a*b* is reported | VERIFIED-SW |
| REQ-COL-005 | Colour repeatability during roasting evaluated | §7.1 | — | optical rig | procedure in `docs/09_test_plan.md` §C | Repeatability within an approved tolerance | PENDING-HW |

## D. Temperature and RoR

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-TEMP-001 | Timestamped temperature with trend, filter and status | §8 | `processing/temperature.py` | K-type + MAX6675 | `test_temperature.py` | Result carries timestamp, raw, filtered, status | VERIFIED-SW |
| REQ-TEMP-002 | RoR uses actual elapsed time | §8.1 | `processing/temperature.py` | none | `test_temperature.py::test_ror_uses_actual_timestamps_not_assumed_interval` | RoR correct for irregular sampling | VERIFIED-SW |
| REQ-TEMP-003 | Missing, duplicate, invalid, stale and noisy readings handled | §8.1 | `processing/temperature.py`, `control/safety.py` | none | `test_temperature.py`, `test_heater_safety.py` | Each condition produces a defined status/fault | VERIFIED-SW |
| REQ-TEMP-004 | Probe arrangement (drum vs bean) decided and documented | §8.2 | — | probe location | procedure in `docs/03...` §4 | Documented decision with evidence | PENDING-HW |
| REQ-TEMP-005 | Thermocouple verified against a reference | §18C | — | reference thermometer | procedure in `docs/09_test_plan.md` §C | Error within approved tolerance | PENDING-HW |

## E. Fuzzy control

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-FUZ-001 | Configurable membership functions and linguistic variables | §9.3 | `control/fuzzy.py` | none | `test_fuzzy.py` | Terms loaded from JSON; geometry validated | VERIFIED-SW |
| REQ-FUZ-002 | Rules carry id, antecedents, consequent, rationale, status, evidence | §9.4 | `control/fuzzy.py::Rule` | none | `test_fuzzy.py::test_every_rule_carries_rationale_and_status` | No rule without rationale; config not VALIDATED | VERIFIED-SW |
| REQ-FUZ-003 | Bounded heater output respecting limits and constraints | §9.2 | `control/heater.py` | SSR type | `test_heater_safety.py` | Output clamped to ceiling and min on/off | VERIFIED-SW |
| REQ-FUZ-004 | Fuzzy controller is not the sole over-temperature protection | §9.2 | `control/safety.py` | independent thermal cutoff | `test_heater_safety.py::test_over_temperature_latches_until_acknowledged` | Supervisor removes heater authority independently | VERIFIED-SW (SW part); PENDING-HW (hardware cutoff) |
| REQ-FUZ-005 | Rule base tuned and compared against a baseline | §9.5 | — | hardware + trials | `docs/09_test_plan.md` §D | Documented metrics vs baseline | PENDING-TRIAL |
| REQ-FUZ-006 | Membership boundaries justified by data | §9.4 | — | trials | `docs/06...` §5 | Boundaries derived from measured roasts | REQUIRES-APPROVAL / PENDING-TRIAL |

## F. E-nose

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-ENOSE-001 | Gas, temperature, humidity, pressure, status, timestamp collected | §10 | `sensors/*`, `core/types.py` | BME688 | `test_firmware_protocol.py`, `test_storage_and_bridge.py` | All fields parsed and logged or empty | VERIFIED-SW |
| REQ-ENOSE-002 | Warm-up and baseline acquisition | §10.1 | `app/engine.py::_enose_with_baseline` | BME688 | `test_engine.py` (baseline event) | Baseline recorded with sample count | VERIFIED-SW |
| REQ-ENOSE-003 | Gas response not presented as aroma identification | §10 | `web/index.html`, `docs/13...` | none | code review + GUI text | Wording present in UI and docs | VERIFIED-SW |
| REQ-ENOSE-004 | Sampling location protects the sensor from heat/condensation/particulates | §10.2 | — | sampling arrangement | procedure in `docs/03...` §7 | Temperature at sensor within limit during a roast | PENDING-HW |
| REQ-ENOSE-005 | Aroma classification, if attempted, is a separate validated feature | §10.1 | — | labelled data | not started | Model, labels and evaluation exist | NOT-STARTED |

## G. Stages

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-STAGE-001 | Six required stage labels supported | §11 | `processing/stages.py` | none | `test_stages_endpoint.py` | Stages present in order | VERIFIED-SW |
| REQ-STAGE-002 | Detection method, timestamp, confidence, confirmation recorded | §11.6 | `processing/stages.py`, `storage/session.py` | none | `test_stages_endpoint.py::test_milestones_are_recorded` | Milestones exported with method | VERIFIED-SW |
| REQ-STAGE-003 | Turning point estimated from temperature history only if signal supports it | §11.1 | `processing/stages.py` | probe | `test_stages_endpoint.py` | Estimated flag set; threshold provisional | VERIFIED-SW (logic) / PENDING-HW (signal quality) |
| REQ-STAGE-004 | First/second crack not claimed without evidence | §11.3, §11.5 | `processing/stages.py`, firmware | microphone | `test_stages_endpoint.py::test_acoustic_event_requires_confidence_threshold` | Acoustic path disabled by default; operator confirmation available | VERIFIED-SW |
| REQ-STAGE-005 | Stage criteria calibrated per profile | §11.2, §11.4 | — | trials + colour calibration | `docs/10...` | Criteria recorded per profile | PENDING-TRIAL |

## H. Timing

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-TIME-001 | Independent schedules for acquisition, control, GUI, logging, events | §12 | `app/engine.py`, `src/config/system.json` | none | `test_engine.py`, `test_api.py` | Intervals configurable and independent | VERIFIED-SW |
| REQ-TIME-002 | Stage transitions, faults and heater events recorded when they occur | §12 | `app/engine.py::_event`, `storage/session.py` | none | `test_engine.py` | Events present in `events.jsonl` | VERIFIED-SW |
| REQ-TIME-003 | GUI refresh never used for safety or control | §12 | `app/server.py`, `app/engine.py` | none | architecture review; engine runs headless | GUI can be closed without affecting control | VERIFIED-SW |
| REQ-TIME-004 | Intervals justified by measurement | §12 | — | hardware | `docs/09_test_plan.md` §C | Timing report with measured latency | PENDING-HW |

## I. GUI

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-GUI-001 | Bean and roast-level selectors resolve to a profile | §6, §13.2 | `web/index.html`, `web/app.js` | none | `test_api.py::test_profile_endpoints` | Selection shows profile, status and warnings | VERIFIED-SW |
| REQ-GUI-002 | Dashboard shows every required live value | §13.1 | `web/app.js` | none | `test_engine.py::test_snapshot_exposes_all_dashboard_fields` | All tiles populated from the snapshot | VERIFIED-SW |
| REQ-GUI-003 | Live charts for temperature, RoR, colour, gas, heater | §13.3 | `web/charts.js` | none | manual + history payload test | Charts render from session history | VERIFIED-SW |
| REQ-GUI-004 | Start prevented when sensors invalid, profile unavailable or interlock active | §13.2 | `app/engine.py::start_session` | none | `test_engine.py` (fault/link tests) | Start raises a documented error | VERIFIED-SW |
| REQ-GUI-005 | Stopping the heater does not stop cooling/ventilation | §13.2 | `app/engine.py::_begin_cooling` | fan wiring | `test_engine.py` | Cooling phase runs with heater off and fan on | VERIFIED-SW (SW), PENDING-HW (fan) |
| REQ-GUI-006 | Completion screen only after stopping conditions are met | §13.4 | `app/engine.py::_finalize`, `web/app.js` | none | `test_engine.py`, `test_api.py` | Overlay appears only with a summary | VERIFIED-SW |
| REQ-GUI-007 | Completion screen reports colour, gas, temperature, RoR, duration, profile, reason, warnings | §13.4 | `web/app.js::showCompletion` | none | API test on `summary_lines`/completion | All fields present or explicitly unavailable | VERIFIED-SW |
| REQ-GUI-008 | Export actions available from the GUI and the API | §13.3, §15.3 | `app/server.py` | none | `test_api.py` (CSV/JSON export) | Downloads return logged data unchanged | VERIFIED-SW |
| REQ-GUI-009 | GUI runs on the actual HMI or a documented alternative is chosen | §13 | — | 5-inch HMI | `docs/03...` §2 | HMI protocol confirmed; GUI deployed | PENDING-HW |

## J. Data and export

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-LOG-001 | Session metadata incl. ids, versions, operator, notes, completion | §15.1 | `storage/session.py` | none | `test_engine.py` | `session.json` complete | VERIFIED-SW |
| REQ-LOG-002 | Full time series incl. raw and processed values | §15.2 | `storage/session.py` | none | `test_storage_and_bridge.py` | 35 stable columns | VERIFIED-SW |
| REQ-LOG-003 | CSV + JSON export with units and missing-value convention | §15.3 | `storage/export.py` | none | `test_storage_and_bridge.py` | Empty field for missing; no substitution | VERIFIED-SW |
| REQ-LOG-004 | Storage failure surfaces as a defect, not silence | §16 | `app/engine.py`, `control/safety.py` | storage medium | `test_heater_safety.py` | `STORAGE_FAILURE` fault raised | VERIFIED-SW |
| REQ-LOG-005 | Research summaries (duration, colour, trends, heater, stages, faults) | §15.4 | `app/engine.py::_finalize`, `storage/export.py` | none | `test_engine.py` | Summary contains all required keys | VERIFIED-SW |
| REQ-LOG-006 | Statistical claims only with a valid design and real results | §15.4 | — | trials | `docs/10...` | Analysis plan approved | PENDING-TRIAL |

## K. Safety

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-SAFE-001 | Independent over-temperature protection not relying on the ESP32 | §16 | — | thermal cutoff | `docs/04...` §3 | Cutoff removes heater power with the controller powered off | PENDING-HW |
| REQ-SAFE-002 | Correct SSR/heater switching method and heatsinking | §16 | `control/heater.py` (host), firmware | SSR + heatsink | `test_heater_safety.py` | SSR type confirmed; duty within limits | PENDING-HW |
| REQ-SAFE-003 | Over-temperature fault latches and removes heater authority | §16 | `control/safety.py` | none | `test_heater_safety.py` | Latched until acknowledged when cool | VERIFIED-SW |
| REQ-SAFE-004 | Temperature disconnection / implausible value → defined safe state | §16 | `control/safety.py`, `app/engine.py` | none | `test_engine.py::test_temperature_fault_aborts_and_removes_heater_authority` | Heater off, session aborted, cooling started | VERIFIED-SW |
| REQ-SAFE-005 | RoR runaway detection | §16 | `control/safety.py` | none | `test_heater_safety.py::test_ror_runaway_requires_three_consecutive_evaluations` | Latched fault after 3 evaluations | VERIFIED-SW (threshold REQUIRES-APPROVAL) |
| REQ-SAFE-006 | HMI/machine link failure → defined behaviour | §16 | `app/engine.py`, `control/safety.py`, firmware | link | `test_engine.py::test_start_refused_when_link_lost` | Start refused / heater off on link loss | VERIFIED-SW |
| REQ-SAFE-007 | Emergency stop handled safely and latched | §16 | `app/engine.py`, `control/safety.py`, firmware `HeaterSafety.*` | E-stop input | `test_engine.py::test_emergency_stop_latches_and_blocks_new_sessions`, `test_safety_protocol.py::test_controller_reported_estop_aborts_the_roast` | Heater off, release gated on cool batch | VERIFIED-SW |
| REQ-SAFE-008 | Fuse, grounding, isolation, strain relief, high-temperature wiring | §16 | — | enclosure | `docs/04...` §3 | Electrical review signed off | PENDING-HW |
| REQ-SAFE-009 | Startup and shutdown sequences defined | §16 | `app/engine.py` | none | `test_engine.py` | Heater starts off and ends off in every path | VERIFIED-SW |
| REQ-SAFE-010 | Gas-safety architecture if a gas roaster is used | §16 | — | — | not applicable yet | Separate reviewed design | N/A (electric baseline) |

## L. Firmware

| ID | Requirement | Source | Module | Hardware dependency | Test | Acceptance criterion | Status |
|---|---|---|---|---|---|---|---|
| REQ-FW-001 | Host↔controller protocol documented and implemented on both sides | §17 | `sensors/serial_bridge.py`, `src/esp/ColAromatic/HostLink.cpp` | ESP32 | `test_firmware_protocol.py`, `arduino-cli compile` | Frames parse; commands match the table | VERIFIED-SW + BUILDS |
| REQ-FW-002 | Board fail-safe: host silence removes heater authority | §16 | firmware `CONF.h` `HOST_TIMEOUT_MS`, host keepalive in `engine.step()` | ESP32 | `test_safety_protocol.py` (host side), code review (firmware) | Heater off 3 s after silence; a live host never goes silent for 1 s | VERIFIED-SW (host) + BUILDS (firmware) |
| REQ-FW-003 | Pin map verified against the real board and harness | §17 | `src/esp/ColAromatic/PINS.h` | board | checklist | Signed pin map | PENDING-HW |

## Open decisions that block safe implementation

1. Exact ESP32 module and confirmed free GPIOs (firmware pin map depends on it).
2. HMI model and protocol — browser HMI assumed; see `docs/03...` §2.
3. SSR type (zero-cross vs random-fire) — determines whether time-proportioning is legal.
4. Heater voltage/current per element and the resulting protection sizing.
5. How many heater SSR circuits are actually used, whether they are independent, and confirmation that the fan has its own SSR (on/off only).
6. Probe arrangement: drum/environment temperature vs bean temperature.
7. BME688 sampling arrangement (headspace vs exhaust vs diluted sample).
8. Whether an acoustic channel is available for crack detection.
