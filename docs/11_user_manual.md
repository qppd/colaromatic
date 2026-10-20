# 11 — Operator user manual

Audience: the researcher/operator running the prototype.
Read [13 — Known limitations](13_known_limitations.md) before interpreting any value.

## 1. Starting the system

```bash
# simulation (no roaster attached)
python -m colaromatic.app.server --mode sim --open

# hardware (ESP32 attached)
export COLAROMATIC_SERIAL_PORT=COM7          # Windows example; /dev/ttyUSB0 on Linux
python -m colaromatic.app.server --mode hardware --host 127.0.0.1 --port 8080
```

Open `http://127.0.0.1:8080`. The dashboard shows a red-striped banner whenever
the data source is simulated — if that banner is visible, **nothing you record
is a measurement**.

## 2. Screen map

| Area | What it shows |
|---|---|
| Top bar | data-source badge, colour-calibration badge, safety badge, elapsed timer |
| Setup card | bean variety, roast level, resolved profile with status and warnings, control mode, operator, batch size, notes, Start |
| Live controls | Hold heat, Stop & cool, Emergency stop, Release E-stop, Acknowledge faults, live setpoint (auto) |
| Stages | the six stage labels with current/estimated/confirmed state, and a manual Confirm selector |
| Recent sessions | the last sessions with CSV/JSON download links |
| Tiles | temperature, RoR, stage, heater command, L*a*b*, BME688 gas, elapsed |
| Charts | temperature (+target), RoR, L*a*b*, gas resistance, heater output |
| Safety & alarms | active and latched faults with codes and requirement references, plus the event log |
| E-nose & environment | gas resistance, R/R₀, baseline R₀, sensor temperature, humidity, pressure, warm-up |
| Completion screen | appears only after the stopping conditions are met |

## 3. Before every roast (pre-flight)

1. **Hardware checks.** Probe seated and reading a plausible ambient value;
   colour window clean; e-nose sampling tube clear; fan free; E-stop reachable;
   the independent thermal cutoff present and untripped.
2. **Safety status.** The safety badge reads `ok` and there are no latched
   faults. A latched fault blocks starting until it is acknowledged (and the
   batch is cool enough).
3. **Colour calibration.** The badge must read `validated` for any roast whose
   colour is to be reported. Anything else (`synthetic placeholder`,
   `unvalidated`, `missing`) means colour values are not measurements.
4. **Profile status.** Every shipped profile is `provisional` and shows
   `PROVISIONAL PROFILE — CALIBRATION REQUIRED`. That is expected, not a bug.
5. **Storage.** The connection panel shows `storage: writing`. Check free disk
   space — a 30 s logging interval is small, but set the interval deliberately.

## 4. Starting a roast

1. Choose **Bean variety** and **Roast level**. The card shows the profile id,
   version, status, warnings, colour target availability and endpoint method.
2. Choose **Control mode**:

   | Mode | Behaviour | When to use |
   |---|---|---|
   | Monitor only | records sensors, heater never energised | sensor checkout, dry runs, troubleshooting |
   | Manual duty | you set a fixed duty with the slider; the fuzzy controller is not driving | commissioning, baseline roasts |
   | Automatic fuzzy | the controller computes duty from temperature error and RoR | only with a validated profile, or with a written commissioning approval |

3. In automatic mode, enter a **target temperature** (if the profile has no
   trajectory, the target is the operator setpoint and is labelled as such).
4. Enter the operator initials, batch size and any note.
5. Press **Start roast**. If the request is refused, the reason is stated —
   see §8.

The duty ceiling shown on the tile is the *effective* ceiling: the minimum of
the hardware-verification cap (30 % by default), the profile limit, and any
commissioning-approval limit. It cannot be raised from the GUI.

## 5. During the roast

* Watch the **temperature** chart against the dashed target and the **RoR**
  chart for the shape of the rise.
* The **stage** tile advances automatically where the sensors support it
  (turning point from the temperature minimum; colour change only with a
  calibrated colour trend). **Estimated** stages are marked as such.
* Confirm stages you observe yourself: choose the stage in the Selector and press
  **Confirm**. First and second crack should always be confirmed by the operator
  unless a validated acoustic method exists. A confirmed stage is recorded with
  your operator name.
* **Hold heat** sets duty to 0 % while keeping monitoring and ventilation — use it
  for an unplanned pause.
* **Stop & cool** ends the roast and starts the cooling phase. The heater is
  removed; ventilation continues. Stopping never stops cooling.
* **Emergency stop** removes heater authority immediately, latches the fault, and
  starts cooling. It cannot be released while the batch is above the configured
  temperature. Do not use it as a routine stop.
* The **physical E-stop** on the machine behaves the same way: the controller
  latches it, the dashboard shows it, and the running roast is aborted to
  cooling. Release the switch first, then press **Release E-stop**.
* If the **temperature probe fails**, the heater is removed on both sides: the
  controller drops the SSR and the host aborts the roast to cooling. Heating does
  not resume on its own when the probe comes back.
* If a fault appears, read it in **Safety & alarms**. The code and the
  requirement reference are shown so the cause can be traced in the docs.

## 6. Ending a roast

* Endpoint criteria for validated profiles trigger automatically. When a
  criterion is met but the profile or calibration is not validated, the GUI asks
  for **confirmation** — the endpoint is *estimated* until you confirm.
* Operator-confirmed endpoints are the norm for provisional profiles: press
  **Stop & cool** when you judge the roast complete (or **Confirm endpoint** if
  the prompt is shown).
* The **completion screen** appears only after the stopping conditions are met.
  It lists final colour, gas, temperature, RoR, duration, the profile and
  calibration versions, the completion reason, warnings and any faults.
* Export with **Export CSV** (time series) or **Export JSON** (everything).
  Keep both with the trial sheet.

## 7. Reading the numbers honestly

| Value | Status when… |
|---|---|
| L*a*b* | only meaningful if the colour badge is `validated`; otherwise synthetic/unvalidated |
| RoR | needs ~10 s of history; `insufficient history` appears at the start; a noisy signal is flagged `degraded` |
| Gas resistance | a **sensor response**, not aroma identification; compare within a session using R/R₀ |
| Stage | `estimated` vs `confirmed` is shown; temperature-derived stages depend on probe placement |
| Heater duty | *applied* duty already includes the ceiling and the minimum on/off quantisation |
| Duration | `duration_s` is roast time; `session_duration_s` adds cooling |

## 8. Common refusals and what they mean

| Message | Meaning | Action |
|---|---|---|
| `start refused: safety interlock active` | a latched or active fault | fix the cause; acknowledge when cool |
| `start refused: no valid temperature reading` | probe disconnected, or out of range | check the probe and connector |
| `start refused: machine link unavailable` | serial link down | check the port, cable, firmware |
| `automatic heating blocked: profile not validated …` | provisional profile without approval | use manual mode, or issue an approval if commissioning is intended |
| `automatic control requires a temperature target` | no trajectory and no setpoint | enter a setpoint or use manual mode |
| `automatic control has the colour-deviation input enabled …` | controller wants colour but there is no validated target | disable the input in the controller config, or calibrate colour |
| `a session is already active` | a roast is running | stop it first |

## 9. After the roast

1. Confirm the completion screen shows `cooling_temperature_verified: true`. If
   it warns that cooling ended unverified, treat the batch as hotter than reported.
2. Record the trial sheet entries from `docs/10_experimental_protocol.md`.
3. Clean the colour window and note the cleaning in the maintenance log.
4. Do not edit session files. Analysis is a read-only consumer.

## 10. Maintenance schedule

| Interval | Task |
|---|---|
| Every roast | clean the colour window, check the e-nose tube for dust, check the probe connector |
| Every 10 roasts | verify the thermocouple in ice/boiling water; inspect wiring and terminals |
| Monthly | re-verify the colour calibration; record a new calibration version if it drifts |
| Before each trial campaign | trip the hardware thermal cutoff deliberately; verify earth continuity |
| Whenever firmware is re-flashed | re-run the pre-flight checks and a supervised manual roast before trusting automatic mode |
