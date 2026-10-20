# 06 — Fuzzy logic controller design and tuning methodology

> **The shipped rule base is INITIAL/UNVALIDATED.** The fuzzy controller
> regulates a heater in a research prototype; it is not a validated roast
> controller and it is not a safety device. No rule, term boundary or threshold
> in `src/config/controllers/default_provisional.json` has been compared against
> roasting trials in this repository.

## 1. Method

| Choice | Value | Why |
|---|---|---|
| Inference | Mamdani, min implication, max aggregation | transparent, easy to inspect, adequate for one output |
| Defuzzification | centroid over a discretised output universe (101 points, 0–100 %) | deterministic and testable; no library dependency |
| Membership functions | piecewise-linear point lists | every shape is reviewable in JSON; no hidden code |
| Universe clipping | inputs outside the universe are clamped and a warning is reported | avoids undefined membership while surfacing the condition |
| Missing input | no output at all (`status = missing_input`), caller commands 0 % | the controller never guesses |
| No rule fired | `output.fallback_pct` (0 %) with an explicit status | deterministic and safe |
| Rule metadata | id, antecedents, consequent, weight, rationale, status, evidence | an unvalidated rule cannot masquerade as a validated one |

Unit tests: `src/tests/test_fuzzy.py` (membership geometry, shouldering, inversion,
missing inputs, clamping, output bounds, determinism, configuration validation).

## 2. Inputs — what is used and what is deliberately not

| Input | Enabled | Justification |
|---|---|---|
| `temp_error_c` = target − measured | **yes** | the primary control objective; measured reliably |
| `ror_c_per_min` | **yes** | supplies the damping/anticipation term; measured reliably after ~10 s |
| `delta_e` (colour deviation from the profile target) | **no, by default** | there is no calibrated colour target for any of the 12 profiles yet, so the input would be fed `None`. It can be enabled once a profile has a validated colour target **and** the colour calibration is `VALIDATED`; the engine refuses to start automatic control otherwise. |
| Roast stage | not used as an input | stage is an outcome of the same measured variables; feeding it back adds rules without adding information |
| Time / elapsed | not used as an input | time enters through the target trajectory, not as a separate antecedent |
| Gas-sensor response | not used | no established relationship between gas response and a control target; would be speculation |

Two inputs keep the rule base inspectable (13 rules instead of 50+) and
calibratable within a thesis timeline. Adding a third input multiplies the
calibration burden and is deliberately deferred.

## 3. Terms (all boundaries PROVISIONAL)

`temp_error_c`, universe −25…+25 °C (target − measured)

| Term | Points | Meaning |
|---|---|---|
| `neg_large` | (−25,1) (−15,1) (−8,0) | far above target |
| `neg_small` | (−12,0) (−5,1) (−1,0) | slightly above target |
| `zero` | (−3,0) (0,1) (3,0) | at target |
| `pos_small` | (1,0) (6,1) (13,0) | below target |
| `pos_large` | (9,0) (16,1) (25,1) | far below target |

`ror_c_per_min`, universe −10…+30 °C/min

| Term | Points | Meaning |
|---|---|---|
| `falling` | (−10,1) (−4,1) (0,0) | losing heat rise |
| `steady` | (−1,0) (2,1) (6,0) | nominal rise |
| `rising` | (4,0) (9,1) (14,0) | brisk rise |
| `fast` | (12,0) (20,1) (30,1) | runaway risk |

Output `heater_pct`, universe 0–100 %

| Term | Points |
|---|---|
| `off` | (0,1) (4,1) (10,0) |
| `low` | (5,0) (18,1) (32,0) |
| `medium` | (26,0) (45,1) (64,0) |
| `high` | (58,0) (75,1) (90,0) |
| `full` | (85,0) (96,1) (100,1) |

These boundaries are an engineering judgement about *shape*, not roast data.
They must be replaced by values derived from measured roasts (see §5).

## 4. Rule base (13 rules, all `status: initial`)

| Rule | Antecedents | Consequent | Rationale |
|---|---|---|---|
| R01 | error `neg_large` | `off` | far above target: remove heat immediately |
| R02 | error `neg_small` ∧ RoR `fast` | `off` | above target and still climbing: cut to avoid overshoot |
| R03 | error `neg_small` ∧ RoR `steady` | `low` | above target, steady rise: hold a low input |
| R04 | error `neg_small` ∧ RoR `falling` | `low` | above target but cooling: avoid a stall |
| R05 | error `zero` ∧ RoR `fast` | `low` | at target and rising fast: anticipatory reduction |
| R06 | error `zero` ∧ RoR `steady` | `medium` | maintain the current energy input |
| R07 | error `zero` ∧ RoR `falling` | `high` | at target but the rise is decaying: add heat |
| R08 | error `pos_small` ∧ RoR `fast` | `low` | below target but overshoot is likely |
| R09 | error `pos_small` ∧ RoR `rising` | `medium` | below target, rising adequately |
| R10 | error `pos_small` ∧ RoR `falling` | `high` | below target and cooling: strong correction |
| R11 | error `pos_large` ∧ RoR `fast` | `medium` | far below target but climbing fast: limit input |
| R12 | error `pos_large` ∧ RoR `steady` | `high` | far below target, adequate rise |
| R13 | error `pos_large` ∧ RoR `falling` | `full` | far below target and stalling: maximum permitted heat |

Every rule carries a rationale string in the configuration file, and
`/api/controller` exposes the whole structure for review.

## 5. Procedure to develop the real rule base (Phase 7 → 8)

Do these in order; each produces a record to keep.

1. **Establish the temperature target.** Either a measured trajectory per
   profile (`temperature_reference.trajectory`) or an operator setpoint for
   supervised commissioning. Without a target the controller cannot compute an
   error, and automatic mode is refused.
2. **Characterise the plant, heater only, no beans.** At fixed duties (10, 20,
   30 % of the commissioning ceiling), log the chamber temperature response.
   Fit first-order gain and time constant. This is the data that justifies the
   RoR term boundaries.
3. **Choose controller period and duty ceiling.** The control period must be
   much shorter than the plant time constant; the ceiling comes from the
   electrical review, not from the controller.
4. **Build the initial rule base** from the plant response, keeping every rule's
   rationale in the file.
5. **Closed-loop bench test, no beans.** Step the target (e.g. 100 → 150 → 200 °C)
   and record overshoot, settling time, steady-state error, duty activity.
6. **Tune** boundaries (not rules) against the recorded step responses. Keep old
   versions: `ProfileManager.save` archives a changed validated profile.
7. **Compare against a baseline.** A fixed-duty roast and a manual-duty roast
   with the same target. Report the same metrics for all three. Only then can a
   performance claim be made — and it must be stated as "for this plant, these
   conditions".
8. **Roast trials** (see `docs/10_experimental_protocol.md`), then revise.

## 6. Anti-windup, limits and interaction with safety

* There is no integrator, so there is no windup to manage. The controller is a
  fuzzy proportional-plus-damping (PD-like) structure.
* Output is clamped three times: by the controller universe (0–100 %), by the
  profile `operating_limits.max_duty_pct`, and by the system ceiling
  (`min(hardware_verified ? max_pct : commissioning_max_pct, approval max_duty_pct)`).
* `HeaterOutput` then enforces minimum on/off times so short pulses cannot
  damage the SSR or fail to switch it.
* A **hold** action forces duty to 0 % while keeping monitoring and the fans
  running.
* The safety supervisor is independent: it can zero the duty regardless of what
  the fuzzy controller asks for, and it latches over-temperature and E-stop
  faults (REQ-FUZ-004).

## 7. Known weaknesses of the current controller

1. Rules and boundaries are unvalidated placeholders (§3).
2. Colour is not yet part of the control law; the thesis title promises
   colour-informed control, which is only reachable after calibration
   (Phase 7).
3. The plant model is unknown, so the controller period (2 s) and the RoR term
   placement are judgements, not measurements.
4. With the 30 % commissioning ceiling the controller can saturate: at maximum
   permitted duty the process may still not reach the target. That is a
   hardware/electrical limitation, and it is visible in the log as
   `limited_by: ceiling` together with a persistent positive error.
5. Step-response behaviour in simulation is a property of the **simulator**,
   not of the roaster. It validates software behaviour only.
