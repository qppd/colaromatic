# 07 — Colour measurement and calibration

> **Current state: there is no measured colour calibration in this repository,
> and there is no colorimetry on the machine.** The firmware reports the
> calibration status `COLOR_UNCALIBRATED` next to every colour value, and the
> panel never shows an L\*a\*b\* number (REQ-COL-004). The target the colour loop
> regulates toward, `COLOR_TARGET_RATIO` in `CONF.h`, is a **placeholder (0.45)**
> that has never been measured on this machine (REQ-COL-007).

## 1. What the firmware actually measures

`ColorSensor` reads the TCS3200's four light-to-frequency channels
sequentially, 50 ms each. `ColorSetpoint::browningRatio` reduces them to one
number:

```
browning_ratio = red / (red + green + blue)          // 0..1
   NAN when red+green+blue < COLOR_MIN_SUM_HZ        // no light / sensor dead
   NAN when red+green+blue > 3 × GEN_COLOR_MAX_HZ    // saturated or wiring fault
```

`ColAromatic.ino` additionally reports the clear channel as `COLOR_INVALID`
when it falls outside `[GEN_COLOR_MIN_HZ, GEN_COLOR_MAX_HZ]`, and otherwise
`COLOR_UNCALIBRATED`. The status reaches the panel in `LinkState.colorStatus`
and is rendered by `hmiColorStatusText()`.

Why a ratio and not L\*a\*b\*:

* a single channel moves with lamp ageing, distance and window fouling; a ratio
  cancels the common factor;
* as beans brown, red reflectance falls more slowly than blue, so the ratio
  **rises monotonically with development** — a defensible ordinal signal;
* the three channels are sampled 50 ms apart, so on a turning drum they are not
  the same instant. Only a *slow* quantity survives that, which is another
  reason this is an outer-loop target selector and never a fast correction.

Applying a generic RGB→CIELAB conversion to raw counts would be a fabrication.
The code refuses to do it (REQ-COL-004), and docs/13 §3 records the gap.

## 2. Why a TCS3200 needs care before any of this means anything

The sensor says nothing about colour appearance. Making its output meaningful
requires:

1. controlled illumination (values are only valid for the light source and
   geometry they were calibrated under),
2. dark and white references through the **same** optics,
3. a target derived from **measured** reference colour, not assumed,
4. documented conditions that stay stable during roasting.

## 3. What has to be measured (all PENDING-HW)

| # | Quantity | Where it lands | Why it blocks |
|---|---|---|---|
| 1 | Optical geometry: distance, angle, illumination, shielding, window | this document + docs/04 | without it no reading is repeatable |
| 2 | Dark and white references (repeat readings, mean and spread) | this document | saturation/gain limits, sanity checks |
| 3 | `browning_ratio` at the **target roast level**, measured against a reference instrument on the same batch | `CONF.h` `COLOR_TARGET_RATIO` | the outer loop's error is meaningless without it |
| 4 | Ratio spread across replicates at a fixed roast level | this document | tells you whether the ratio can even resolve a target |
| 5 | Optical stability during a roast (§4) | this document | decides whether in-situ readings are usable at all |

Only item 3 changes firmware; everything else is recorded evidence that item 3
is worth something. When a measured value replaces the placeholder, the change
is a `CONF.h` edit plus a note here and a status change in docs/01 (REQ-COL-007
moves from `REQUIRES-APPROVAL` to measured).

## 4. Measurement procedure (to be executed)

1. Mount the sensor at the intended distance and angle. Record: distance (mm),
   angle (deg), illumination (source, colour temperature, setting), shielding
   (yes/no), window material.
2. Fix the illumination and let it stabilise. Record warm-up time and whether
   the values drift.
3. **Dark reference:** cover the optics completely; take ~20 readings; keep the
   mean and the spread.
4. **White reference:** a diffuse white standard of known reflectance at the
   bean plane; take repeated readings.
5. **Development series:** roast a batch (or use the roast series you already
   run), and at fixed elapsed times record `C,<r>,<g>,<b>,<clear>` from the
   service port while a reference instrument measures the same sample's colour.
6. **Target point:** at the intended endpoint, record the mean ratio and the
   reference colour. That ratio is the value for `COLOR_TARGET_RATIO`.
7. Re-measure after re-mounting the sensor to expose alignment sensitivity.
8. Never overwrite a previous record: each attempt is a new entry with date,
   operator and geometry. Keep `COLOR_TARGET_RATIO` at its placeholder until an
   independent check agrees.

Template for the record (keep it alongside this document):

```text
reference_id:      colour-2026-xx-xx
geometry:          distance __ mm, angle __ deg, illumination __, shielded __
dark:     r=  g=  b=  clear=   (mean of n readings, spread __)
white:    r=  g=  b=  clear=   (mean of n readings, spread __)
endpoint: r=  g=  b=  clear=   ratio = r/(r+g+b) = __
reference colour at endpoint: __ (instrument, method)
replicate spread of ratio at endpoint: __
operator / date: __
```

## 5. Optical-stability assessment during roasting (REQ-COL-005)

For each item, record what was done and the measured effect:

| Effect | How to test | Pass criterion (to be approved) |
|---|---|---|
| Bean movement / fill level | static card vs rotating drum readings | ratio spread within the approved tolerance |
| Lamp drift / heating | 30 min run, readings every minute | no monotonic drift beyond tolerance |
| Window fouling | readings before/after a roast | documented cleaning interval |
| Smoke/steam | readings during a roast | flag when scatter degrades the reading |
| Chamber reflections | compare with/without shielding | documented |
| Sensor temperature | log sensor-adjacent temperature | no shift attributable to temperature |

If the TCS3200 cannot meet the agreed repeatability, that is a finding to
document and escalate — **not** a reason to invent numbers. Options to propose
for approval include a different sensor (a calibrated colour sensor or a
camera-based setup) and a defined sampling procedure (measuring a cooled sample
with a bench instrument and recording that separately).

## 6. Independent verification (do not skip)

1. Split the measurement series: derive the target from one subset, check it on
   the held-out subset.
2. Re-measure after re-mounting the sensor.
3. Measure three roasted-bean samples both with the in-situ sensor and with a
   reference instrument; record the differences.
4. Only then change `COLOR_TARGET_RATIO`, and record who approved it.

## 7. What the firmware enforces today

| Guard | Behaviour |
|---|---|
| No usable light (`sum < COLOR_MIN_SUM_HZ`) | ratio is NAN → automatic mode falls back to the profile/operator target (REQ-CTRL-003) |
| Saturation (`sum > 3 × GEN_COLOR_MAX_HZ`) | ratio is NAN → same fallback |
| Clear channel outside `[GEN_COLOR_MIN_HZ, GEN_COLOR_MAX_HZ]` | `colorStatus = COLOR_INVALID` on the panel |
| Everything else | `colorStatus = COLOR_UNCALIBRATED` — the honest default until §3 is done |
| Non-finite colour error | `ColorSetpoint::setpointC()` returns NAN; no setpoint is produced |
| Profiles | all 12 are `PROVISIONAL` and none is auto-eligible, so automatic mode cannot start at all yet |

## 8. Calibration status vocabulary

`LinkFrames.h` already carries the four states the panel can render:
`COLOR_UNCALIBRATED`, `COLOR_PROVISIONAL`, `COLOR_VALIDATED`, `COLOR_INVALID`.
The firmware currently emits only the first and the last. Reaching
`COLOR_PROVISIONAL` or `COLOR_VALIDATED` means items 1–3 of §3 are done and the
value has been reviewed; that status change is a firmware edit, and it must be
mirrored in docs/01 (REQ-COL-007) in the same commit.
