# 06 — Fuzzy controller design and tuning methodology

> **Both rule bases are INITIAL/UNVALIDATED.** The fuzzy controller regulates a
> heater in a research prototype; it is not a validated roast controller and it
> is not a safety device. No rule, term boundary or threshold here has been
> compared against roasting trials in this repository.

**Scope of this revision.** This document describes the firmware as it ships,
not the removed Python host. The machine runs **two loops in series** (REQ-CTRL-001):

```
colour error (ratio units)  ──fuzzy──►  temperature setpoint °C  ──PID──►  duty %
   ColorSetpoint.cpp                      RoastControl::onControlTick        Pid.cpp
   outer loop                              inner loop
```

Both loops run inside `RoastControl` at the control period (`GEN` control
period, 2 s), only in `MODE_AUTO`, only while the session is `RUNNING`.

## 1. Method (shared by both tables)

| Choice | Value | Why |
|---|---|---|
| Inference | Mamdani, min implication, max aggregation | transparent, easy to inspect, adequate for one output |
| Defuzzification | centroid over a discretised output universe (`GEN_FUZZY_OUT_RES` = 101 points) | deterministic and testable; no library dependency |
| Membership functions | piecewise-linear point lists with shoulder clamping at the universe edge | every shape is reviewable in the table; no hidden code |
| Universe clipping | inputs outside the universe are clamped | avoids undefined membership |
| Missing input (NAN) | `status = missing_input`, output 0 | the controller never guesses |
| No rule fired | the table's `fallback` output (0) with an explicit status | deterministic and safe |
| Rule metadata | `id`, antecedents, consequent, weight | an unvalidated rule cannot masquerade as a validated one |

One implementation, `FuzzyController` in `Fuzzy.*`, evaluates **both** tables, so
the algorithm cannot drift between them (REQ-FUZ-001). Statuses are surfaced as
`ok` / `missing_input` / `no_rules_fired`.

## 2. The outer loop: colour error → setpoint (`ColorSetpoint.cpp`)

**Input** `color_error = COLOR_TARGET_RATIO − browningRatio(R, G, B)`, universe
**−0.30 … +0.30** ratio units. Negative = darker than target, positive = lighter.

**Five input terms, spaced 0.15 apart.** Each side of a peak ramps to zero
exactly where its neighbour's peak is, so at every error **at most two terms are
non-zero and their memberships sum to 1.0**. That overlap is what makes the
surface continuous — the controller never hands over from one rule to the next
at a single point.

| Term | Points | Meaning |
|---|---|---|
| `very_dark` | (−0.30,1) (−0.15,0) | far too dark (shoulder) |
| `dark` | (−0.30,0) (−0.15,1) (0.00,0) | darker than target |
| `on_target` | (−0.15,0) (0.00,1) (0.15,0) | at target |
| `light` | (0.00,0) (0.15,1) (0.30,0) | lighter than target |
| `very_light` | (0.15,0) (0.30,1) | far too light (shoulder) |

**Output** setpoint, universe **150 … 230 °C**, five consequents 20 °C apart:

| Term | Points |
|---|---|
| `coolest` | (150,1) (170,0) |
| `cooler` | (150,0) (170,1) (190,0) |
| `target` | (170,0) (190,1) (210,0) |
| `warmer` | (190,0) (210,1) (230,0) |
| `hottest` | (210,0) (230,1) |

**Rules — one per term, deliberately monotonic** (darker → cooler, lighter →
hotter), so the surface cannot fold back on itself:

| Rule | Antecedent | Consequent |
|---|---|---|
| C1_very_dark_coolest | `very_dark` | `coolest` |
| C2_dark_cooler | `dark` | `cooler` |
| C3_on_target | `on_target` | `target` |
| C4_light_warmer | `light` | `warmer` |
| C5_very_light_hottest | `very_light` | `hottest` |

### 2.1 Why the terms are shaped this way (defect history)

The first version used **three wide terms with flat tops** (`darker` was 1 from
−0.30 to −0.04). Combined with shoulder-shaped outputs it produced a **step
function**: ~157 °C for every negative error, a 30 °C jump at −0.04, a flat
190 °C around zero, then another jump. A setpoint that steps 30 °C inside one
control tick is a step input to the PID and a step input to the plant.

The fix was structural, not cosmetic:

1. five terms instead of three, so no flat plateau spans a third of the universe;
2. **one consequent per term** instead of shared consequences;
3. adjacent memberships that sum to 1.0 everywhere, so exactly two rules blend at
   every point;
4. `static_assert`s that the rule count and the consequent count agree — a rule
   pointing past the end of `OUT_TERMS` would be an out-of-bounds read on the MCU.

**Measured surface** (1201 samples across −0.30 … +0.30, reproduced from the
table during review):

| error | setpoint |
|---|---|
| −0.30 | 156.4 °C |
| −0.15 | 170.0 °C |
| 0.00 | 190.0 °C |
| +0.15 | 210.0 °C |
| +0.30 | 223.6 °C |

* monotone: yes (largest backwards movement 0.003 °C — defuzzification quantisation);
* **largest step: 0.15 °C** per 0.0005 of error (was 30 °C);
* the extremes stop at 156.4 / 223.6 rather than reaching 150 / 230 because the
  end terms are shoulders; the reachable range is therefore slightly inside the
  universe. That is a property of shoulder defuzzification, not a bug.

### 2.2 Fallbacks (REQ-CTRL-003)

`RoastControl::onControlTick` picks the target in this order:

1. the colour setpoint, **only** if `colorRatio` is not NAN *and*
   `ColorSetpoint::setpointC()` returns a number;
2. otherwise `targetAt(elapsed)` — the profile trajectory if one exists, else the
   operator setpoint;
3. otherwise no automatic control (the start is refused with a stated reason).

Missing, out-of-range or saturated light therefore never produces a meaningless
target. `_colorTarget` records which path was used, and the panel can show it.

## 3. The inner loop: setpoint → duty

The shipped inner loop is a **PID** (`Pid.*`), not fuzzy:

* derivative **on measurement** (no derivative kick when the setpoint steps);
* conditional integration and explicit limits, so the duty ceiling cannot wind
  the integral up (`Pid::setLimits(0, dutyCeiling())` is re-applied every tick);
* the output is clamped three times — by the PID limit, by
  `RoastControl::shapeDuty` (ceiling, minimum on/off quantisation), and finally
  by `HeaterSafety`, which may zero it at any moment;
* every mode change, hold, cooling start and completion calls `_pid.reset()` so
  no pre-hold integral carries into the next session.

Manual mode (`MODE_MANUAL`) bypasses both loops entirely: the panel's duty
slider value goes straight through `shapeDuty`.

## 4. The dormant duty-side rule base (`GenFuzzy.h`)

`GenFuzzy.h` still carries the **13-rule temperature→duty table**
(`default_provisional`, `GEN_FUZZY_STATUS "INITIAL_UNVALIDATED"`), with inputs
`temp_error_c` (−25…+25 °C), `ror_c_per_min` (−10…+30 °C/min) and `delta_e`
(disabled, `enabled = 0`) and outputs `off…full` (0…100 %). It was generated from
`src/config/controllers/default_provisional.json`, and it is **not referenced by
the shipped control law** — the PID is used instead. It stays in the tree as:

* a documented record of the original rule base (the rules are listed below);
* an alternative inner loop that can be selected in code later, through the same
  `FuzzyController`.

| Rule | Antecedents | Consequent |
|---|---|---|
| R01 | error `neg_large` | `off` |
| R02 | error `neg_small` ∧ RoR `fast` | `off` |
| R03 | error `neg_small` ∧ RoR `steady` | `low` |
| R04 | error `neg_small` ∧ RoR `falling` | `low` |
| R05 | error `zero` ∧ RoR `fast` | `low` |
| R06 | error `zero` ∧ RoR `steady` | `medium` |
| R07 | error `zero` ∧ RoR `falling` | `high` |
| R08 | error `pos_small` ∧ RoR `fast` | `low` |
| R09 | error `pos_small` ∧ RoR `rising` | `medium` |
| R10 | error `pos_small` ∧ RoR `falling` | `high` |
| R11 | error `pos_large` ∧ RoR `fast` | `medium` |
| R12 | error `pos_large` ∧ RoR `steady` | `high` |
| R13 | error `pos_large` ∧ RoR `falling` | `full` |

Term boundaries for these inputs are the geometric placeholders in `GenFuzzy.h`
(`neg_large` (−25,1)(−15,1)(−8,0) … `fast` (12,0)(20,1)(30,1)). They are an
engineering judgement about *shape*, not roast data.

## 5. Why colour is an input at all — and what it needs

The thesis title promises colour-informed control, so colour selects the
**target**, not the duty:

* a darker-than-target bean says "you are under-heating", which is a statement
  about the *setpoint*, not about how many percent to apply;
* the PID then does what it always does with that target;
* the plant's gain and time constant stay the responsibility of the inner loop,
  so the colour loop never has to model the heater.

This is reachable **only after calibration**: `COLOR_TARGET_RATIO` in `CONF.h` is
a placeholder, so today the colour error is measured against an unmeasured target
(REQ-COL-007, docs/01 §C). Every shipped profile is `PROVISIONAL` and
`autoEligible = 0`, so `RoastControl::start` refuses automatic mode outright.

## 6. Procedure to develop the real tables (Phase 7 → 8)

Do these in order; each produces a record to keep.

1. **Measure the colour target.** Reference patches on this machine, this
   sensor, this geometry → a real `COLOR_TARGET_RATIO` per profile. Without it
   the outer loop's error is meaningless (docs/07).
2. **Characterise the plant, heater only, no beans.** At fixed duties (10, 20,
   30 % of the commissioning ceiling), log chamber temperature. Fit first-order
   gain and time constant. This is the data that justifies the PID gains and, if
   the fuzzy inner loop is ever enabled, the RoR term boundaries.
3. **Choose the control period and the duty ceiling.** The period must be much
   shorter than the plant time constant; the ceiling comes from the electrical
   review, not from the controller.
4. **Closed-loop bench test, no beans.** Step the target (100 → 150 → 200 °C) and
   record overshoot, settling time, steady-state error and duty activity.
5. **Tune boundaries, not rules.** Keep the previous table version alongside the
   new one; a changed table is a changed configuration and must be recorded.
6. **Compare against a baseline.** A fixed-duty roast and a manual-duty roast
   with the same target, same metrics for all three. Only then can a performance
   claim be made — and it must be stated as "for this plant, these conditions".
7. **Roast trials** (docs/10), then revise.

## 7. Anti-windup, limits and interaction with safety

* The inner loop's integrator is conditional and reset on every state change, so
  there is no windup through the ceiling.
* Output is clamped three times: PID limit → `shapeDuty` (ceiling, minimum duty,
  minimum on/off quantisation) → `HeaterSafety::service`, which computes the
  zero-cross phase and can override everything.
* A **hold** forces duty to 0 % while monitoring and the fan continue, and it
  resets the PID so the pre-hold integral is not carried across.
* The safety supervisor is independent: interlocks and latches remove heater
  authority regardless of what either loop asks for, and a trip clears the duty
  request as well as the output (REQ-FUZ-004).

## 8. Known weaknesses of the current controller

1. Both rule bases are unvalidated placeholders; nothing comes from roast data.
2. `COLOR_TARGET_RATIO` is a placeholder, so the outer loop regulates toward an
   unmeasured target even where it runs.
3. Automatic mode cannot start at all: all 12 profiles are `autoEligible = 0`.
   The shipped configuration is manual duty by design.
4. The PID gains (`PID_KP/KI/KD` in `CONF.h`) are unexercised placeholders; there
   is no plant model, so the 2 s control period is a judgement.
5. With the commissioning duty ceiling the controller can saturate: at maximum
   permitted duty the process may still not reach the target. That is a
   hardware/electrical limitation, visible as a persistent positive error next to
   a duty pinned at the ceiling.
6. The colour surface has been reproduced numerically from its tables during
   review; it has never executed on a board, and there is **no simulator** in
   this repository to exercise it against (REQ-SYS-006 `N/A`).
