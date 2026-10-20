# 10 — Experimental protocol (to be approved before any trial)

> **No roasting trial has been conducted for this project.** This document is
> the protocol to be reviewed and approved by the research adviser **before**
> the first trial. Values marked **[APPROVE]** must be set by the team; sample
> sizes are deliberately not invented here.

## 1. Objective

Establish, with recorded evidence, the relationship between bean colour
(CIELAB), process temperature and Rate of Rise, and e-nose response, for each
bean variety and roast level, and evaluate whether the fuzzy controller tracks
the agreed target trajectory acceptably against a baseline.

Secondary objective: document where the prototype's measurement chain is not
good enough, including for which profiles the intended roast level cannot be
reached reliably.

## 2. Required approvals and pre-conditions

| # | Pre-condition | Evidence |
|---|---|---|
| 1 | Electrical design reviewed; independent thermal cutoff tested (§C of the test plan) | signed checklist |
| 2 | Colour calibration measured, independently verified and marked `VALIDATED` | `src/config/calibration/*.json` + verification record |
| 3 | Thermocouple verified against a reference | test record |
| 4 | Target colour and temperature reference recorded for the profile under test, with provenance | updated profile JSON, status raised to `UNDER_CALIBRATION` |
| 5 | Number of replicates and acceptance thresholds approved | this document, signed |
| 6 | Supervised automatic heating approval issued if auto mode is used | `src/config/commissioning_approval.json` |

Profiles without (2) and (4) may only be run in monitor or supervised manual
mode, and their results are exploratory, not confirmatory.

## 3. Independent variables and design

| Factor | Levels | Notes |
|---|---|---|
| Bean variety | Arabica, Robusta, Liberica, Excelsa | one factor at a time in the exploratory phase |
| Roast level | Light, Medium, Dark | defined by the profile's colour/endpoint criteria |
| Control strategy | fixed duty (baseline), supervised manual (baseline), fuzzy automatic | comparing strategies is the primary question |
| Batch size | **[APPROVE]** g | fixed within an experimental block |
| Charge temperature / initial condition | **[APPROVE]** | recorded, not assumed |

Recommended order of work (smallest useful experiment first):

1. **Plant characterisation** — heater only, no beans, fixed duties; gives the
   time constant and the achievable temperature range.
2. **Baseline roasts** — fixed duty and manual duty, to establish what the
   process does without the controller.
3. **Exploratory automatic roasts** — a few supervised runs to find obvious
   misbehaviour, with the duty ceiling low.
4. **Comparative block** — repeated roasts per condition, randomising run order
   within a day to spread out ambient and operator effects.
5. **Profile-by-profile calibration** — colour targets and endpoint criteria per
   bean/roast level, using the harvested samples.

Replicates: **[APPROVE]** per condition (state the number and why). If only a
fraction of the 12 profiles can be completed before the deadline, use the staged
approach in `docs/16_project_plan.md` and label the remainder explicitly.

## 4. Fixed and recorded conditions for every trial

Record on the trial sheet:

* date, time, operator, ambient temperature and humidity;
* bean variety, lot/roast date of the green beans, origin and processing if known;
* green-bean moisture (% wb) and density if measurable — otherwise record "not measured";
* batch mass (g), charge temperature (measured, not assumed);
* profile id, version and calibration status, controller id and version,
  colour calibration id and status;
* software build, system config version and data-source mode
  (`hardware`, never `simulated` for a real trial);
* sensor mount state (probe position, drum speed, fan setting, illumination
  state, window cleanliness);
* any manual interventions with times (stage confirmations, hold, duty changes, endpoint confirmation).

## 5. Procedure for one trial

1. Perform the pre-run checks in `docs/11_user_manual.md` §4 (sensor validity,
   latched faults, storage writable, probe seated, safety hardware present).
2. Pre-heat to the agreed charge temperature with the fan running; confirm the
   temperature is stable and the probe reads realistically (`+−` [APPROVE] °C).
3. Start the session from the GUI, choosing bean variety, roast level, control
   mode, setpoint/trajectory, operator, and batch size. Confirm the stored
   metadata on the dashboard (profile status, calibration status, data source
   **hardware**).
4. Charge the beans at t≈0. Confirm the turning point appears in the log.
5. Observe: temperature and RoR charts, heater duty, colour trend, gas response,
   stage progression, alarms. Act on alarms; the safe state is automatic.
6. Confirm first crack (and second crack if relevant) from the log. Where the
   acoustic channel is enabled, still confirm by ear/observation. Note the time
   between the acoustic event and your confirmation.
7. End the roast either at an automatic endpoint criterion (validated profiles)
   or by confirming the endpoint yourself. Do not end a trial without a recorded reason.
8. Let cooling finish. Confirm the completion screen shows `cooling_temperature_verified`.
9. Export the session (CSV + JSON) and store it with the trial sheet.
10. Take the post-roast sample: measure ground/powder colour with the reference
    instrument if available, note any obvious defect (tipping, scorching, uneven).
11. Clean the sensor windows and record the cleaning in the maintenance log.

## 6. Data handling

* All raw sessions stay immutable in `src/data/sessions/`; analysis scripts must never
  overwrite them.
* Filter analysis rows by `data_source == "hardware"`; a simulation run must
  never appear in a results table.
* Missing values stay missing: no interpolation, no filling with zero, no
  dropping of a whole trial because one sensor was absent — record the gap.
* Report, for every metric, the number of valid observations it uses.
* Exclude a trial only for a documented cause (procedure violation, sensor
  failure, power loss, operator error) — and report the exclusions.

## 7. Analysis plan (fixed before data collection)

| Question | Metric | Comparison |
|---|---|---|
| Does the controller track the target? | max overshoot (°C), settling time (s), RMS tracking error (°C) | fuzzy vs fixed duty vs manual |
| Is the RoR profile reasonable? | RoR at turning point, at first crack; number of sign changes | between strategies and profiles |
| Is the colour trajectory consistent? | L*, a*, b* at fixed elapsed times and at the endpoint; ΔE spread within condition | replicate repeatability; endpoint accuracy |
| Do the stages occur at consistent states? | temperature and colour at each transition; time between transitions | across replicates |
| Does the e-nose response separate levels? | gas resistance normalised to R₀ at fixed times; area under the curve | between roast levels (report effect size and dispersion; only claim significance if the design supports it) |
| Is the process reproducible? | spread of the above across replicates | within condition |

Statistics: report distribution summaries and dispersion for every metric.
Do **not** claim statistical significance or superior performance unless the
design (replicates, randomisation, controls) supports it and the test was
pre-registered in this document.

## 8. Per-profile calibration record

For each bean/roast-level combination, record in the profile file:

| Field | Source |
|---|---|
| `color_target.l_star/a_star/b_star` | mean of post-roast measurements at the accepted endpoint, with the reference instrument and method |
| `color_target.tolerance_delta_e` | observed spread across replicates, rounded up |
| `temperature_reference.charge_c / turning_point_c / first_crack_c` | measured values during trials |
| `temperature_reference.trajectory` | the accepted target curve, with tolerance |
| `ror_reference.target_ror_c_per_min` | from the accepted automatic runs |
| `endpoint.*` | the criterion that reliably identified the endpoint |
| `provenance.evidence` | session ids used |
| `status` | `UNDER_CALIBRATION` → `VALIDATED` after independent review |

Never copy one profile's numbers into another bean/level profile without its own
measurements. Keep the previous version: `ProfileManager.save` archives a
validated profile and refuses to overwrite it in place.

## 9. Staged fallback if time runs short

1. Complete the plant characterisation and the sensor verification (these
   unblock everything else).
2. Calibrate colour for **one** bean variety across three roast levels.
3. Validate **that** variety's three profiles end to end.
4. Treat the remaining nine combinations as:
   * runnable in monitor/manual mode, and
   * explicitly marked `PROVISIONAL` in the profile file, the GUI and the thesis,
     with the reason and the required work listed.
5. State plainly in the report: "3 of 12 profiles validated; 9 remain
   provisional" — never present an untested combination as validated.
