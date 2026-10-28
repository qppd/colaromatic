# 09 — Test plan

> **Status: nothing here has been executed.** The repository contains **0 test
> files** (docs/01 §M): the previous suite belonged to the removed Python host.
> This plan is (a) what can be checked today without hardware, (b) what must be
> written before the next code change is trusted, and (c) the hardware
> commissioning record — §C — that docs/04 §5 points at.

Evidence rule for every row: a test that has not been run is `OPEN`, no matter
how obvious its result looks. Nothing in this plan may be marked passed on the
strength of a review.

## A. Static checks (executable today, no toolchain needed)

| # | Check | How | Status |
|---|---|---|---|
| A1 | Every firmware file parses as C/C++ | tree-sitter parse of `src/esp/**/*` (`.cpp`, `.h`, `.ino`) | **done during the docs revision** — 56 files, 53 clean; the 3 with error nodes are `IRAM_ATTR` in `ColorSensor.cpp` and macro-built declarations in `lvgl_v8_port.*`, i.e. grammar false positives, not syntax errors |
| A2 | The two copies of `LinkFrames.h` are byte-identical | `diff src/esp/ColAromatic/LinkFrames.h src/esp/HMI/LinkFrames.h` | **pass** |
| A3 | The colour surface is continuous and monotone | reproduce `ColorSetpoint.cpp` tables numerically and sample the surface | **done**: monotone, largest step 0.15 °C per 0.0005 error (the pre-revision table stepped 30 °C) |
| A4 | No dangling file references in the docs | script: every `path` mentioned in `docs/*.md` exists or is on the documented-missing list | **pass** (docs 08/09/16 now exist; the remaining external references are explicitly labelled) |
| A5 | Rule/consequent bounds | `static_assert` in `ColorSetpoint.cpp` | **ready** — the assertions exist but only run at compile time, i.e. once C1 passes |

## B. Software tests to write (blocked on a build toolchain)

Target: native compilation of the pure-logic modules with a small harness
(`arduino-cli`/`g++` + a stub `Arduino.h`), run in CI. Priority order:

| # | Module | What must be asserted | Why it matters |
|---|---|---|---|
| B1 | `Fuzzy.*` | membership geometry and shoulder clamping; min/max inference; centroid determinism; NAN input → `missing_input` and output 0; no rule fired → fallback; input clamped at universe edges | REQ-FUZ-001/002, both loops depend on it |
| B2 | `ColorSetpoint.*` | surface monotone and continuous over −0.30…+0.30; output inside `[150, 230]`; NAN/inf error → NAN; adjacent memberships sum to 1.0 | REQ-COL-006; regression guard for the 30 °C-step defect |
| B3 | `Pid.*` | no derivative kick on a setpoint step; integral does not wind past the ceiling; reset clears state; output clamped to `[0, ceiling]` | REQ-CTRL-002 |
| B4 | `RoastControl` state machine | start refusals (latched fault, bad profile, `autoEligible = 0`, bad setpoint); mode/profile locked while live; hold→resume resets PID; cooling never skipped; endpoint needs confirmation except max duration; RoR runaway latches after exactly 3 strikes | REQ-CTRL-004/005, REQ-STAGE-007, REQ-SAFE-005 |
| B5 | `RoastControl::shapeDuty` | ceiling, minimum on/off quantisation, 0 request → 0 applied | REQ-FUZ-003 |
| B6 | `HeaterSafety` logic (pins stubbed) | each trip clears the duty request, not just the output; latch reset refused while hot or while E-stop is held; both watchdogs; `activeFaults`/`latchedFaults` masks | REQ-SAFE-003…012 |
| B7 | `LinkFrames` | CRC16 known-answer vectors; a corrupted byte fails; duplicate `seq` is re-ACKed not re-applied; length mismatch rejected | REQ-FW-004/007 |
| B8 | Serial command parser | every documented command parses; unknown line → `UNKNOWN_COMMAND` and the watchdog is **not** refreshed; `SET,HEATER` refused while a session is live | docs/05 §2 |
| B9 | `Gen*` tables | every rule's antecedent and consequent indices are in range; term point lists are sorted; universes bracket their terms | guards against an out-of-bounds read on the MCU |

Exit criterion for section B: B1–B7 green locally, and the same run recorded in
docs/17 before any claim of `VERIFIED-SW` appears in docs/01.

## C. Hardware commissioning record (no beans, then mains)

Run in the order given by docs/04 §5. **Every row needs a date, an operator, an
instrument and a result.** Photographs are evidence; memory is not.

| # | Test | Method | Pass criterion | Result |
|---|---|---|---|---|
| C1 | Controller compiles for its target | `arduino-cli compile --fqbn esp32:esp32:esp32 src/esp/ColAromatic` | exit 0, output recorded in docs/17 §3 | **open** |
| C2 | Panel compiles for the fitted board | `arduino-cli compile` with LVGL 8 + `esp_panel` and the correct board config | exit 0, output recorded | **open** — board config still the 7″ reference one (docs/14 B8) |
| C3 | Sensors with no mains | serial monitor: `T` in ice water and boiling water, `C` against a grey card, `E` in still air | plausible values, flags 0 | open |
| C4 | E-stop input | press/release with LEDs on the heater outputs | latch within the debounce time, `ERR,ESTOP_INPUT`, LEDs dark | open |
| C5 | Serial watchdog | pull the USB cable with outputs on LEDs | LEDs dark within 3 s, `ERR,HOST_TIMEOUT` | open |
| C6 | Panel watchdog | power off the panel mid-session | authority removed within 5 s, `F_HMI_LINK_LOST`, panel shows LINK LOST on reconnect | open |
| C7 | Probe interlock | disconnect the thermocouple during a run | heater off within one loop pass, `ERR,TC_INVALID_HEATER_OFF`, session aborted to cooling | open |
| C8 | Over-temperature latch | force the limit in a controlled way (bench, not with beans) | latched, reset refused while hot | open |
| C9 | SSR switching with a **lamp load** | scope or lamp on the heater SSR input; fan SSR too | commanded duty visible; fan on for any non-zero command, off at 0 | open |
| C10 | Minimum on/off visible | request a small duty | pulses ≥ `GEN_HEATER_MIN_ON_MS`, gaps ≥ `GEN_HEATER_MIN_OFF_MS` | open |
| C11 | Duty never exceeds ceiling | lamp load, all modes | ≤ `GEN_DUTY_CEILING_PCT` | open |
| C12 | First element, current measured | clamp meter on one element | matches the nameplate calculation (docs/04 §3) | open |
| C13 | **Independent thermal cutoff trips with the controller powered off** | deliberate trip test | heater power removed without firmware | **open — safety critical** |
| C14 | Dry roast (no beans) at the ceiling | observe stage/endpoint sequence from the panel | sequence completes; completion popup only on DONE | open |
| C15 | Full roast with beans, supervised | docs/10 protocol | trial sheet + photographs complete | open |

## D. Regression checks to re-run after any change

1. A1–A5 (static) — always.
2. B1–B7 — whenever the toolchain exists.
3. C1 and C2 — after **every** firmware edit: a parse is not a compile.
4. C5, C6, C7 — after any change to `HeaterSafety.*`, `LinkFrames.h` or the
   watchdog constants.
5. A2 — after any edit to `LinkFrames.h` or `GenProfiles.h`.

## E. Exit criteria for calling the system verified

A requirement in docs/01 may move to `BUILDS` or beyond only when:

1. its row here has a recorded, dated result with an artifact (build log,
   test output, photograph, instrument reading); and
2. the row's evidence covers the requirement's own acceptance criterion, not a
   neighbouring one; and
3. docs/01, docs/14 and docs/17 are updated in the same change.

Until then everything stays `CODE`, `PENDING-HW` or `NOT-IMPLEMENTED`, which is
where docs/01 currently (and correctly) has it.