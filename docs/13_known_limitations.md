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
| HMI deployment on the 5-inch panel | not attempted; panel model unknown |
| Firmware on real hardware | compiles; never flashed or run |

## 2. Unvalidated threshold values (all in `src/config/system.json`)

Every number below is an engineering placeholder and must be replaced or
approved before it supports a claim:

| Key | Value | Basis | Needed |
|---|---|---|---|
| `heater.commissioning_max_pct` | 30 % | cautious cap pending the electrical review | measured load, SSR rating |
| `heater.window_s` | 10 s | typical for zero-cross time-proportioning | SSR type confirmation |
| `heater.min_on_ms` / `min_off_ms` | 500 ms | protects the SSR from pulses too short to switch | SSR datasheet |
| `safety.max_temperature_c` | 240 °C | conservative ceiling | valid for the beans and the hardware? |
| `safety.max_ror_c_per_min` | 45 °C/min | runaway guard | measured roasts |
| `safety.temperature_stale_timeout_s` | 5 s | sensor/plan speed | measured update rate |
| `safety.cooling_min_s` / `max_s` / `target_c` | 60 s / 600 s / 60 °C | batch safety | cooling curve measurement |
| `safety.estop_reset_max_c` | 80 °C | conservative release gate | hardware / hand-safety limits |
| `temperature.ror_window_s` / `ror_min_span_s` | 30 s / 10 s | smooths noise, still responsive | noise measurement |
| `temperature.max_rate_c_per_s` | 60 °C/s | catches disconnects | probe dynamics |
| `temperature.noisy_residual_c` | 2 °C | noise flag | measured probe noise |
| `color.max_valid_hz` / `saturation_fraction` | 2 MHz / 0.98 | TCS3200 full scale | module behaviour |
| `stages.turning_point_rebound_c` | 3 °C | detection heuristic, not roast data | probe placement |
| `stages.color_change_delta_l` | **null (disabled)** | no calibrated colour trend exists | calibration + trials |
| `stages.acoustic_min_confidence` | 0.6 | crack detection is unvalidated | audio survey |
| roast / cooling fan command | 70 % / 100 % | fan speed intent | the fan SSR is on/off, so the 70 % setting actually runs the fan at full speed |
| `timing.*` | 1 s / 2 s / 1 s / 30 s | responsiveness vs load | measured loop timing |

## 3. Colour is not yet a measurement

* The colour values the software displays come from
  `src/config/calibration/color_calibration.json`, whose status is
  `SYNTHETIC_PLACEHOLDER`. The matrix is the standard linear-sRGB→XYZ matrix
  scaled to Yn = 100, not a fit to measurements of this sensor.
* Because of that, the shipped colour targets for the 12 profiles are **empty**,
  ΔE-to-target is unavailable, the colour endpoint method is unusable, and the
  fuzzy controller's colour input is disabled by default.
* The colour argument in the project title therefore has a software pipeline and
  a documented calibration procedure — **not** a validated measurement.
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
* Turning-point detection is a heuristic (minimum + rebound threshold). It has
  been tested against the simulator, not against a real probe signal.

## 5. Control

* The fuzzy rule base is `INITIAL_UNVALIDATED`; nothing in it comes from roast
  data.
* No plant model exists, so the control period, the term boundaries and the
  absence of an integral term are judgements.
* Automatic heating with a provisional profile is blocked unless a written
  commissioning approval exists. That is deliberate, and it means the shipped
  configuration **cannot** run automatic control out of the box.
* With the 30 % commissioning ceiling, the controller can saturate and never
  reach the target; that would be a hardware limitation, not a controller bug.
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
* Baseline R₀ is a median over the first samples of a session, so sessions
  compared across days are affected by sensor drift and ambient conditions.
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
* Duty is capped at 30 % until `heater.hardware_verified` is set, and that flag
  must not be set without the electrical review documented in doc 04 §3.
* Electrical switching assumes a zero-cross SSR. A random-fire or phase-angle
  SSR must not be driven by this output stage.
* Fan operation is commanded but not sensed: the software cannot prove that the
  fan is running during cooling.
* The fan has its own SSR and is switched on/off only. Any non-zero fan command
  runs it at full speed; there is no PWM or phase control, so the 70 % roast
  fan setting is not a reduced speed.
* Heater feedback is optional and unverified; a stuck SSR is therefore not
  detected unless that line is wired and `feedback_expected` is enabled.
* The controller-side interlocks (probe fault, E-stop, over-temperature, host
  watchdog) are evaluated on every firmware loop pass and the firmware
  compiles, but none of them has ever run on a board. Only the host side of
  that review is covered by automated tests.
* The host feeds the controller's watchdog by re-asserting its heater command
  once a second. That assumes an honest serial path: an adapter that accepts
  writes and drops them is only detected through the controller's `link`
  field, one report at a time.
* The E-stop input uses the ESP32's internal pull-up on GPIO 16. A long or
  unshielded run needs an external pull-up and filtering that have not been
  specified, and the 20 ms debounce is unverified against the real switch.
* The server has no authentication or transport security: localhost only.
* Gas-roaster safety is out of scope: if a gas burner is used, a separately
  reviewed gas-safety architecture is required instead of this electric design.

## 9. Profiles and process

* All 12 profiles are `PROVISIONAL` with no measured colour, temperature, RoR or
  endpoint values. The software exercises the full data path; it does not
  demonstrate a validated roast for any variety.
* Nested-use bean varieties (Liberica, Excelsa) have the least literature and
  the same placeholder state as the others — nothing has been assumed about them.
* The end-to-end path for an operator-confirmed endpoint has been exercised in
  simulation (including stage confirmation and completion), but the colour and
  temperature evidence that would justify the criteria does not exist yet.

## 10. Software limitations

* Single roast at a time; no queue, no multi-machine support.
* The storage layer writes CSV/JSONL on the host filesystem; there is no
  database, no retention policy and no encryption.
* `research_log_period_s` defaults to the specified 30 s, which is coarse for
  control analysis; the alternative must be recorded as a protocol deviation.
* Charts downsample to ~600 points for the browser, so the on-screen curve is
  not the full-resolution record; the CSV is.
* Idle safety monitoring samples at 1 Hz; between sessions the values shown are
  live but not recorded.
* The simulator is a simple first-order model with synthetic colour kinetics. It
  is a software test fixture, **not** a validated thermal model; simulator
  numbers must never appear in a results table.
* No user accounts, roles, or audit log beyond the recorded operator string.
* Firmware has no OTA update and no on-board logging; a serial disconnect
  mid-roast loses the firmware-side detail (the host keeps its own record).

## 11. Claims the project must not make yet

1. That any profile is validated.
2. That colour measurement is calibrated or accurate.
3. That the controller improves roast quality, stability or consistency.
4. That stages are detected reliably.
5. That the e-nose characterises aroma.
6. That the system is safe to operate unattended.
7. That simulated results predict real roasting behaviour.
