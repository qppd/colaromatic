# 12 — Troubleshooting guide

Every entry states the symptom, the likely cause, the immediate action, and how
to confirm the diagnosis. Safety-relevant entries are marked **SAFETY**.

## 1. Start-up

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| `python -m colaromatic.app.server` exits with a missing-module error | runtime dependencies not installed | `pip install fastapi uvicorn pydantic numpy` | server starts and prints the URL |
| `pyserial is required for hardware mode` | hardware mode without pyserial | `pip install pyserial` | bridge imports without raising |
| `COLAROMATIC_SERIAL_PORT is required in hardware mode` | environment variable unset | set it to the actual port | `bridge.link_ok()` becomes true once frames arrive |
| Browser shows nothing at `http://127.0.0.1:8080/` | wrong host/port, or bound to another interface | use the printed host/port; check the firewall | `GET /api/health` returns `{"status":"ok"}` |
| Dashboard shows `reconnecting…` and never updates | WebSocket blocked (proxy, browser extension) | the dashboard falls back to polling every 2 s; check the console | the tiles update even while the badge shows reconnecting |
| Simulation banner is visible during a hardware session | `--mode sim` still in use | restart with `--mode hardware` | badge reads `HARDWARE` and `/api/health` shows `data_source: hardware` |

## 2. Sensors

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| Temperature `missing`, or `start refused: no valid temperature reading` | probe disconnected, broken thermocouple, wrong CS pin, MAX6675 unpowered or not detected | check the connector and polarity; check SCK/SO/CS wiring and the module supply | live tile shows a plausible ambient value; firmware prints `T` frames with flag `0` |
| Temperature `invalid` repeatedly with a plausible value shown | reading outside `min_valid_c…max_valid_c`, or an implausible rate (a jump > 60 °C/s) | inspect the probe for intermittent contact and the connector for corrosion | notes name the exact rejection reason |
| Temperature goes `stale` during a roast | no frames for `stale_after_s` (3 s) | check the serial cable, USB hub, port power; controller may have reset | firmware banner reappears in the serial monitor |
| RoR shows `insufficient history` at the start | fewer than 3 samples, or a span under 10 s | wait ~10 s; nothing else needed | RoR appears after the window fills |
| RoR jumps wildly | noisy probe or a loose connection | check the probe seating; consider a longer `ror_window_s` | the `notes` column flags a noisy fit |
| L*a*b* displays `—` with `SYNTHETIC_PLACEHOLDER`/`UNVALIDATED` | that is the honest state, not a failure | run the colour calibration workflow (doc 07) | badge changes to `validated` after an approved calibration |
| Colour flagged `degraded · sensor saturation risk` | channel near full scale — illumination too strong or the sensor too close | reduce illumination or move the sensor back | warning disappears and normalised channels stay below the white reference |
| Colour values drift during a roast | window fouling, lamp drift, smoke scatter | clean the window; check with the compensation on/off | re-measure a grey card before/after; compare ΔE |
| Gas resistance `invalid` / warm-up incomplete | BME688 not responding, or the first 60 s | check the I²C address (0x76/0x77) and pull-ups; wait for warm-up | `H`/`E` frames show flag `0` and `warmup=1` |
| Gas values creep upward over months | sensor ageing/drift | re-baseline each session; schedule replacement | baseline R₀ comparison across sessions |

## 3. Control and heater

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| Heater output stays at 0 % in automatic mode | missing fuzzy input (RoR not available yet), or no rule fired, or the safety supervisor removed authority | check the controller panel: `status` is `missing_input` / `no_rules_fired`; check the safety badge | controller `status: ok` and `output > 0` |
| Duty is capped below what you asked for | the effective ceiling (commissioning cap, profile limit, approval limit) | check the heater tile's `ceiling_pct` and `limited_by` | the event log records `heater_command` with `ceiling_pct` |
| Temperature never reaches the target | the ceiling is too low for the plant | verify with a fixed-duty baseline; raise the ceiling only after the electrical review | plateau temperature vs duty curve |
| Temperature overshoots the target badly | controller terms unvalidated, or the plant is much faster than assumed | switch to manual for the rest of the run; re-tune per doc 06 §5 | step-response record |
| Duty oscillates rapidly | control period too short for the plant, or min on/off too small | lengthen `control_period_s`, or increase `min_on_ms` | measured relay on-time trace |
| Relay does not follow the command | SSR wiring, drive level, wrong SSR type, or a stuck contact | **SAFETY** remove mains and inspect; verify with a lamp load | commanded vs feedback (if wired) or a scope on the SSR input |
| `HEATER_FEEDBACK_MISMATCH` | the feedback line and command disagree | **SAFETY** stop the roast, remove mains, inspect the SSR | fault clears only when they agree |
| Fan does not run during cooling | fan SSR wiring or drive, fan supply, or a stuck SSR | **SAFETY** stop and inspect; do not leave a hot batch | air movement at the exhaust; falling temperature |
| Fan runs but speed cannot be varied | expected: the fan SSR is on/off, so any non-zero command is full speed | none — use the on/off command, or replace the hardware with a variable-speed drive | `H` frame fan field is `0` or `100` |
| `cooling ended without a verified temperature` | temperature invalid during cooling | inspect the probe; treat the batch as hot | the summary warning disappears once the probe is fixed |

## 4. Safety faults

| Fault code | Meaning | Action |
|---|---|---|
| `TEMPERATURE_INVALID` | no usable temperature | fix the probe/link; the heater is already off |
| `TEMPERATURE_STALE` | readings stopped arriving | fix the link |
| `OVER_TEMPERATURE` | above `max_temperature_c` (240 °C default) | latch: cool, inspect the batch, investigate the cause, then acknowledge |
| `ROR_RUNAWAY` | RoR above the limit for 3 consecutive evaluations | latch: verify the threshold against a real roast before relying on it |
| `ENOSE_OVER_TEMPERATURE` | sensor above its limit (warning only) | improve the sampling arrangement; the heater is not interlocked for this |
| `MACHINE_LINK_LOST` | controller link down | check the cable/port; the heater is off |
| `EMERGENCY_STOP` | E-stop latched | confirm the hazard is cleared and the batch is cool before releasing |
| `STORAGE_FAILURE` | session data could not be written | free space / fix permissions; the current session may be incomplete — do not present it as complete data |
| `CONTROL_OVERRUN` | the control loop ran late | close other CPU-heavy work; check for a slow disk or an overloaded machine |
| `SAFETY_INTERLOCK`, `PROFILE_NOT_VALIDATED` | a start-gating condition | read the reason string; use manual/monitor mode or obtain an approval |

The heater is hard-off for `critical` and `emergency_stop` levels, and latched
faults keep it off until they are acknowledged **and** the batch is cool.

## 5. Data and export

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| Session directory missing | storage path not writable, or a different `data_dir` was used | check the server console and the storage panel | `GET /api/sessions` lists the session |
| CSV has empty cells | genuinely missing measurements | none — that is the convention | cross-check the matching `*_status` column |
| CSV much shorter than expected | `research_log_period_s` is 30 s by default | reduce it (and record the protocol deviation) | row count ≈ duration / period |
| Export returns 404 | wrong session id | use the id from `GET /api/sessions` | the link in the session list works |
| JSON export is huge | full-precision rows | expected: it embeds the CSV rows verbatim | file size vs row count |
| Analysis shows impossible values | the session was a **simulated** run | filter `data_source == "hardware"` | metadata shows `data_source` |

## 6. Firmware

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| `HOST_TIMEOUT` repeatedly | host not running, or serial writes failing | check the host log and the port | no `HOST_TIMEOUT` while the host is running |
| `TC_OPEN` immediately at boot | MAX6675 unpowered, SCK/SO/CS miswired, or a 5 V module without level shifting | recheck wiring against the verified pin map and the module's logic level | no fault on boot, `T` frames flag `0` |
| `TC_OPEN` / `TC_FAULT` | broken thermocouple wire, loose connector, non-K thermocouple, or a reading outside the valid range | stop the roast; the firmware drops the heater as soon as the probe fails | `T` frames carry flag `2` and an empty value |
| `TC_INVALID_HEATER_OFF` | the heater trip caused by the fault above | fix the probe, then let the host issue a new command; the controller does not resume on its own | heater stays off while the probe is invalid |
| `BME_INIT_FAIL` | I²C address, pull-ups, sensor damaged | try the other address; scan the bus | `E` frames with flag `0` |
| `BME_READ_FAIL` | transient I²C failure | check the bus length and pull-ups | `E` frames return to flag `0` or `1` |
| `UNKNOWN_COMMAND` | host and firmware protocol versions differ | compare `docs/05_serial_protocol.md` with the firmware command table | no such errors after matching versions |
| Heater switches off by itself during a run | `HOST_TIMEOUT` (host silent > 3 s), `ESTOP_INPUT`, `TC_INVALID_HEATER_OFF` or `LOCAL_OVER_TEMP` | read the `ERR` frames; the code names the cause | the corresponding fault code appears, and the host session aborts to cooling |
| Crack events appear constantly | audio threshold too low, fan/motor noise | disable with `SET,AUDIO,0` until calibrated | events stop |

## 7. When in doubt

1. Stop the roast (**Stop & cool**), or **Emergency stop** if there is any doubt
   about the heater.
2. Read the fault codes in the GUI and locate them in §4.
3. Preserve the session directory and the serial log — they are the evidence.
4. Do not restart into automatic mode until the cause is understood and the
   checks in doc 11 §3 pass again.
