# 13 — Known limitations and unvalidated assumptions

This is the single list of everything that is not yet true about ColAromatic.
Any report, defence or handover should be able to point at this document and be
consistent with it.

## 1. Nothing here is experimental data

| Item | State |
|---|---|
| Roasting trials | **none performed** |
| Colour calibration | **synthetic placeholder**; no measured reference patches |
| Temperature verification against a reference | not performed |
| E-nose measurements on real coffee | none |
| Fuzzy controller performance | never measured on the physical plant |
| Sensor repeatability during roasting | not measured |
| Hardware thermal cutoff, fusing, grounding | designed on paper only |
| HMI panel firmware | written (`src/esp/HMI/`), never compiled or flashed; board configuration inherited from the reference 7" sketch and must match the fitted panel |
| HMI event logging | events are shown on the panel and lost on power-off; no SD writer |
| Firmware on real hardware | compiles; never flashed or run |

## 2. Unvalidated threshold values (all in `GenConfig.h` / `CONF.h`)

Every number below is an engineering placeholder and must be replaced or
approved before it supports a claim. The `GEN_*` macros were generated from the
removed host's `src/config/system.json`; the generator is gone (docs/01
REQ-SYS-002), so these headers are now hand-maintained. Firmware-only constants
live in `CONF.h`.

| Key | Value | Basis | Needed |
|---|---|---|---|
| `GEN_DUTY_CEILING_PCT` | 30 % | cautious cap pending the electrical review | measured load, SSR rating |
| `GEN_HEATER_WINDOW_MS` | 10 000 ms | typical for zero-cross time-proportioning | SSR type confirmation |
| `GEN_HEATER_MIN_ON_MS` / `MIN_OFF_MS` | 500 ms | protects the SSR from pulses too short to switch | SSR datasheet |
| `GEN_MAX_TEMP_C` (process) / `LOCAL_MAX_TEMP_C` (backstop) | 240 °C / 250 °C | conservative ceilings | valid for the beans and the hardware? |
| `GEN_MAX_ROR_CPM` + `GEN_ROR_RUNAWAY_STRIKES` | 45 °C/min, 3 strikes | runaway guard | measured roasts |
| `GEN_TEMP_STALE_S` | 3 s | sensor/plan speed | measured update rate |
| `GEN_COOLING_MIN_S` / `MAX_S` / `TARGET_C` | 60 s / 600 s / 60 °C | batch safety | cooling curve measurement |
| `GEN_ESTOP_RESET_MAX_C` | 80 °C | conservative release gate | hardware / hand-safety limits |
| `GEN_ROR_WINDOW_S` / `GEN_ROR_MIN_SPAN_S` | 30 s / 10 s | smooths noise, still responsive | noise measurement |
| `GEN_EMA_ALPHA` | 0.25 | filter lag vs smoothness | measured probe noise |
| `GEN_COLOR_MIN_HZ` / `MAX_HZ`, `COLOR_MIN_SUM_HZ` | 10 Hz / 2 MHz / 30 Hz sum | TCS3200 validity window | module behaviour |
| `GEN_TP_REBOUND_C` / `GEN_TP_MIN_ELAPSED_S` | 3 °C / 30 s | detection heuristic, not roast data | probe placement |
| `GEN_COLOR_CHANGE_DELTA_L` | −1 (**disabled**) | no calibrated colour trend exists | calibration + trials |
| `GEN_ACOUSTIC_MIN_CONF` | 0.6 | crack detection is unvalidated | audio survey |
| roast / cooling fan command | `GEN_ROAST_FAN_PCT` 70 / `GEN_COOLING_FAN_PCT` 100 | fan speed intent | the fan SSR is on/off, so the 70 % setting actually runs the fan at full speed |
| `GEN_SENSOR_PERIOD_MS` / `GEN_CONTROL_PERIOD_MS` / `REPORT_PERIOD_MS` | 1 s / 2 s / 1 s | responsiveness vs load | measured loop timing |
| `LINK_STATE_PERIOD_MS` / `LINK_TIMEOUT_MS` / `HOST_TIMEOUT_MS` | 500 ms / 5 s / 3 s | telemetry cadence and both watchdogs | measured radio and serial behaviour |
| `COLOR_TARGET_RATIO` | 0.45 placeholder | **not a measurement** | docs/07 §3 |
| `PID_KP` / `PID_KI` / `PID_KD` | placeholders | never exercised | plant characterisation (docs/06 §6) |

## 3. Colour is not yet a measurement

* There is **no colorimetry in the repository at all**: no L\*a\*b\*, no ΔE, no
  calibration record. The firmware computes a browning ratio
  (`red / (red+green+blue)`) and reports `COLOR_UNCALIBRATED` beside it
  (docs/07).
* `COLOR_TARGET_RATIO` (0.45) is a placeholder, so the colour error the outer
  loop regulates on is measured against an unmeasured target. ΔE-to-target does
  not exist and the colour endpoint method is unusable.
* Because no profile carries a measured colour target, none is auto-eligible:
  automatic mode cannot start (docs/01 §B).
* The colour argument in the project title therefore has a ratio and a
  documented calibration procedure — **not** a validated measurement.
* The optical fixture (illumination, distance, angle, shielding, window,
  sampling location) is unspecified engineering work.

## 4. Temperature and RoR

* A single temperature channel is supported. Whether it measures the bean mass,
  the drum or the chamber is undecided; RoR and stage inferences are only as
  good as that choice.
* The MAX6675 has a 0.25 °C resolution and a 0..1024 °C range; it cannot report
  temperatures below 0 °C. Its cold-junction compensation is internal and less
  accurate than the MAX31856 it replaces, and it has no fault register beyond
  the open-thermocouple bit. A cold-junction error or an over/under-voltage
  condition is not detectable. A data line shorted low reads as 0 °C; an
  unplugged module is pulled to 0xFFFF by the internal pull-up on SO and
  reports `TC_OPEN`. The independent hardware thermal cutoff remains essential.
* Many MAX6675 breakouts are 5 V boards. The ESP32's SPI pins are not 5 V
  tolerant: the module must be a 3.3 V type or SO/SCK/CS must be level-shifted.
* The EMA filter introduces lag: the controller regulates the filtered value,
  and the reported RoR is computed from the filtered series. Both effects need
  to be quantified before the RoR numbers are quoted as process values.
* RoR needs ~10 s of history; the first seconds of a roast have no RoR and
  automatic control therefore commands 0 % until an RoR exists rather than
  guessing.
* Turning-point detection is a heuristic (minimum + rebound threshold,
  `GEN_TP_REBOUND_C` / `GEN_TP_MIN_ELAPSED_S`). It has been reviewed, never
  executed: there is no simulator in this repository and no board run.

## 5. Control

* Both rule bases are `INITIAL_UNVALIDATED`; nothing in them comes from roast
  data (docs/06).
* No plant model exists, so the control period (2 s), the term boundaries and
  the PID gains are judgements, not measurements.
* Automatic mode is blocked for every shipped profile: all 12 have
  `autoEligible = 0`, so `RoastControl::start` refuses it with a reason. That is
  deliberate, and it means the shipped configuration **cannot** run automatic
  control out of the box.
* The colour surface is continuous and monotone (reproduced numerically from its
  tables), but it has never executed on a board; the PID has only been reasoned
  through, never simulated.
* With the 30 % duty ceiling the controller can saturate and never reach the
  target; that would be a hardware limitation, not a controller bug.
* The project cannot claim "optimal adaptive control" without a comparative
  experiment against a baseline; that experiment has not been run.

## 6. Stages

* Only the turning point is detected automatically from the installed sensors.
* Colour-change detection is disabled because no calibrated colour trend or
  threshold exists.
* First and second crack rely on operator confirmation, or on an acoustic
  channel that is disabled in firmware by default and unvalidated.
* Medium/dark roast stages only advance automatically from a **validated**
  profile endpoint criterion; otherwise they are operator decisions.
* Estimated and confirmed stages are distinguished in the UI and in the data;
  do not treat an estimate as an observation.

## 7. E-nose

* Gas resistance is a sensor response. It does not identify volatile compounds,
  and no aroma/quality classification model exists or is claimed.
* No baseline R₀ is captured at all (REQ-ENOSE-002 `PARTIAL`): raw gas
  resistance is all the machine reports, so values cannot be normalised across
  sessions and are affected by sensor drift and ambient conditions.
* Placement is unresolved: the sensor must stay below `enose_max_c` and free of
  condensation and particulates, which probably requires a cooled or diluted
  sampling path that has not been engineered here.
* Warm-up (60 s in firmware) is a placeholder; the BME688's own stabilisation
  behaviour under this duty cycle has not been characterised.

## 8. Safety

* The software supervisor is not the independent protection. Until a hardware
  thermal cutoff is wired, installed and tested, the prototype has **no**
  independent over-temperature protection.
* The heater circuit has never been energised in this project; no current,
  inrush, heatsink or leakage measurement exists.
* Duty is capped at `GEN_DUTY_CEILING_PCT` (30 %), and that ceiling must not be
  raised without the electrical review documented in docs/04 §3.
* Electrical switching assumes a zero-cross SSR. A random-fire or phase-angle
  SSR must not be driven by this output stage.
* Fan operation is commanded but not sensed: the software cannot prove that the
  fan is running during cooling.
* The fan has its own SSR and is switched on/off only. Any non-zero fan command
  runs it at full speed; there is no PWM or phase control, so the 70 % roast
  fan setting is not a reduced speed.
* Heater feedback is optional and unverified; a stuck SSR is therefore not
  detected unless that line is wired and `feedback_expected` is enabled.
* The controller-side interlocks (probe fault, E-stop, over-temperature, both
  link watchdogs) are evaluated on every firmware loop pass and the firmware
  compiles — but none of them has ever run on a board, and the repository has no
  automated tests to cover them (docs/01 §M).
* The panel feeds the ESP-NOW watchdog by sending STATE/heartbeat traffic; the
  bench serial path feeds its watchdog with a recognized command at least once a
  second. Silence for 5 s (panel) or 3 s (serial) removes heater authority —
  again, untested on hardware.
* The E-stop input uses the ESP32's internal pull-up on `PIN_ESTOP_IN` (GPIO 16).
  A long or unshielded run needs an external pull-up and filtering that have not
  been specified, and the debounce is unverified against the real switch.
* There is no server, so there is no authentication question — but there is also
  no audit trail: physical access to the panel is the trust boundary.
* Gas-roaster safety is out of scope: if a gas burner is used, a separately
  reviewed gas-safety architecture is required instead of this electric design.

## 9. Profiles and process

* All 12 profiles are `PROVISIONAL` with no measured colour, temperature, RoR or
  endpoint values. The software exercises the full data path; it does not
  demonstrate a validated roast for any variety.
* Nested-use bean varieties (Liberica, Excelsa) have the least literature and
  the same placeholder state as the others — nothing has been assumed about them.
* The end-to-end path for an operator-confirmed endpoint exists in code
  (`RoastControl::checkEndpoint`, the panel's endpoint button and completion
  popup) but has never been executed — no session of any kind has been run.

## 10. Software limitations

* Single roast at a time; no queue, no multi-machine support.
* **Nothing is recorded.** There is no SD writer and no session file: telemetry
  lives on the wire, events live in a small volatile ring buffer on the panel,
  and Curves history exists only while that screen is open. Power-off loses
  everything (docs/01 §J).
* There is no CSV/JSON export of any kind (REQ-LOG-003 `NOT-IMPLEMENTED`).
* The two sketches each carry their own copy of `LinkFrames.h` (and
  `GenProfiles.h` in the panel). They must be kept identical by hand; a mismatch
  compiles fine and fails silently on CRC or length.
* No user accounts, roles or audit log beyond the operator string on the
  trial sheet.
* Firmware has no OTA update: reflashing needs the USB cable.
* The panel's charts downsample to a fixed point count, so the on-screen curve
  is not full-resolution — and it is the only copy there is.
* Idle monitoring shows live values between sessions, but records nothing.

## 11. Claims the project must not make yet

1. That any profile is validated.
2. That colour measurement is calibrated or accurate.
3. That the controller improves roast quality, stability or consistency.
4. That stages are detected reliably.
5. That the e-nose characterises aroma.
6. That the system is safe to operate unattended.
7. That anything in this repository has been demonstrated on hardware.
