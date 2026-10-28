# 16 — Project plan

**Status of this plan:** written at the point where the software deliverable is
complete-as-code and nothing has been executed on hardware. It orders the
remaining work, marks the gates, and states the staged fallback so that a
deadline can never turn an untested thing into a validated one.

Dates are owners' responsibility; the sequence and the gates are not negotiable
because each gate exists to protect the next one.

## 1. Where the project is

| Deliverable | State | Evidence |
|---|---|---|
| Controller firmware (session, two loops, safety, links) | written, parsed as C++, **never compiled or flashed** | `src/esp/ColAromatic/`, docs/01 §M |
| Panel firmware (5 screens, ESP-NOW client) | written, parsed, **never compiled or flashed**; board config still the 7″ reference | `src/esp/HMI/`, docs/14 B8 |
| Documentation set (01–17) | rewritten for the standalone machine; 08/09/16 added in this revision | docs/14 §E |
| Hardware, wiring, safety items | not verified; quotation not supplied | docs/03 |
| Calibration (colour, thermocouple) | not started; placeholder values in `CONF.h` | docs/07 |
| Roast trials | none | docs/10 |
| Automated tests | none (0 test files) | docs/09 §B |

## 2. Phases

### Phase 1 — Hardware audit and safety items (blocks everything mains-side)

* Attach the quotation and the delivered parts to docs/03 §1; answer docs/04 §3
  questions 1–5.
* Procure the missing safety items (docs/03 §6): independent thermal cutoff,
  fuse/breaker, heatsinks, opto-drivers, earth bonding.
* **Gate G-1:** no mains work before docs/04 §3 is answered and the cutoff is in
  hand. Owner: builder.

### Phase 2 — Build and flash (blocks every software claim)

* Install `arduino-cli` + the ESP32 core; compile the controller (docs/09 C1).
* Identify the fitted panel; align `esp_panel_board_custom_conf.h`; compile the
  panel (docs/09 C2).
* Flash both, print both MACs, pair them, confirm STATE frames within 5 s.
* **Gate G-2:** both sketches compile and boot, output recorded in docs/17.
  Owner: embedded. *Everything in docs/01 marked `CODE` is unproven until this
  gate closes.*

### Phase 3 — Bench commissioning, no beans, lamp loads

* Run docs/09 §C rows C3–C11: sensors, E-stop, both watchdogs, probe interlock,
  SSR switching with a lamp load, minimum on/off, duty ceiling.
* **Gate G-3:** every C3–C11 row dated and signed, with photographs. Owner:
  builder + embedded.

### Phase 4 — First energised tests

* C12 (measured element current), C13 (**thermal cutoff trips with the
  controller powered off**), then a dry roast at the ceiling (C14).
* Raise the duty ceiling only after the electrical review is written into
  docs/04 §3.
* **Gate G-4:** C13 is safety-critical and blocks every bean trial. Owner:
  builder.

### Phase 5 — Measurement campaign (needs Phase 4 done)

* Thermocouple verification against a reference (docs/01 REQ-TEMP-005).
* Colour target measurement: geometry, dark/white references, endpoint ratio →
  `COLOR_TARGET_RATIO` (docs/07 §3–§4).
* Plant characterisation: fixed-duty step responses → PID gains (docs/06 §6).
* **Gate G-5:** measured values replace the placeholders in `CONF.h`, each with
  a dated record. Owner: researcher.

### Phase 6 — Control tuning and validation

* Closed-loop bench test without beans, then supervised automatic roasts once a
  profile is auto-eligible.
* Baseline comparison: fixed duty vs manual vs automatic, same metrics
  (docs/10 §7).
* **Gate G-6:** no performance claim in the thesis without this comparison.
  Owner: researcher.

### Phase 7 — Trials per docs/10

* Staged: one bean variety × three roast levels end to end; the remaining nine
  stay `PROVISIONAL`.
* **Gate G-7:** trial sheets + photographs complete for every reported run;
  exclusions reported. Owner: researcher.

### Phase 8 — Analysis, write-up, handover

* Analysis against the pre-registered plan (docs/10 §7).
* Freeze the tested configuration on one page (docs/14 §F).
* Hand over with defects (docs/14 §G) listed and owned.
* **Gate G-8:** no claim in the report contradicts docs/13, and no claim rests on
  a value still marked `PENDING-HW`, `PENDING-TRIAL` or `REQUIRES-APPROVAL` in
  docs/01. Owner: project lead.

## 3. Critical path

```
Phase 1 ──► Phase 2 ──► Phase 3 ──► Phase 4 ──► Phase 5 ──► Phase 6 ──► Phase 7 ──► Phase 8
 (parts)     (compile)   (bench)    (mains)     (measure)    (tune)      (trials)    (report)
```

Nothing can be compressed to the right of Phase 4: measurement and trials are
sequential with the physical process. If time runs short, the cut happens
**between Phase 6 and Phase 7**, not by skipping a gate.

## 4. Staged fallback if the schedule slips

1. Phases 1–4 are non-negotiable: without them there is no safe machine.
2. Validate **one** variety across three roast levels (docs/10 §9).
3. Leave the other nine combinations `PROVISIONAL`, runnable in supervised
   manual mode only, and say so in the report.
4. If measurement (Phase 5) cannot complete, the honest deliverable is a
   machine that roasts manually with operator-confirmed endpoints, plus a
   documented list of what a validated version needs. State it plainly — the
   register in docs/01 already says the same thing.

## 5. Decision points that need a named approver

| Decision | Blocks | Approver |
|---|---|---|
| Panel model / board config (docs/03 §2) | Phase 2 | project lead |
| Duty ceiling raise (docs/04 §3) | Phase 4→6 | adviser + builder |
| Colour target value written into `CONF.h` | Phase 6 | researcher |
| `autoEligible = 1` on any profile | Phase 6 | adviser, in writing |
| Replicate counts and acceptance thresholds | Phase 7 | adviser, signed before collection |
| Dropping a phase (schedule pressure) | any | project lead, recorded in docs/14 §F |

## 6. Review points

Re-read this plan whenever: the quotation arrives, the panel is identified, a
gate closes, a defect in docs/14 §G is added or removed, or the deadline moves.
The risk register (docs/15) is re-scored at the same moments.