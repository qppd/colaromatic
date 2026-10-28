# 15 — Risk register

Scoring: **L**ikelihood and **I**mpact are 1 (low) … 5 (high); **P** = L × I.
Owners are roles, to be filled with names at handover. Review this register at
every phase gate.

## A. Safety risks

| ID | Risk | L | I | P | Mitigation | Residual | Owner |
|---|---|---|---|---|---|---|---|
| S1 | Mains shock during assembly or operation | 3 | 5 | 15 | isolated low-voltage control; enclosure and earth bonding; work de-energised; review before energising | medium until the enclosure is built and inspected | builder |
| S2 | Thermal runaway / fire in the drum or chaff | 2 | 5 | 10 | hardware thermal cutoff **independent** of the ESP32; software over-temperature latch; supervised roasting; keep a fire blanket/CO₂ extinguisher | medium | builder + operator |
| S3 | SSR fails closed (welded contact) and the heater stays on | 3 | 5 | 15 | hardware cutoff in series; optional feedback line with `feedback_expected`; duty ceiling; never leave unattended | medium | builder |
| S4 | Wrong SSR drive (random-fire SSR driven with slow switching) | 3 | 4 | 12 | confirm SSR type before wiring; time-proportioning only for zero-cross; bench-verify with a lamp load | low after confirmation | electronics |
| S5 | Heater current exceeds wiring/SSR rating | 3 | 4 | 12 | measure elements first; size fuse/breaker and heatsink; derate for enclosure ambient | low after measurement | electronics |
| S6 | BME688 or colour sensor exposed to hot, wet, smoky air | 4 | 2 | 8 | cooled/diluted sampling path; sensor temperature logged with a limit warning; mounting chosen deliberately | low | builder |
| S7 | E-stop only cuts power through software | 3 | 4 | 12 | hardware E-stop path in series with the heater; test with the controller unpowered | low after wiring | electronics |
| S8 | Cooling fails (fan dead) with a hot batch | 3 | 3 | 9 | cooling phase keeps the fan SSR on; `cooling_temperature_verified` warning; operator stays with the machine | medium | operator |
| S9 | Unattended operation of unvalidated automatic control | 2 | 5 | 10 | approval required with a low duty ceiling; documented as supervised; the GUI states it | low if the procedure is followed | operator |
| S10 | 5 V MAX6675 module drives the 3.3 V ESP32 SPI input | 3 | 3 | 9 | verify the module's logic level; use a 3.3 V module or level-shift SO/SCK/CS | low after verification | electronics |

## B. Data and research-integrity risks

| ID | Risk | L | I | P | Mitigation | Residual | Owner |
|---|---|---|---|---|---|---|---|
| D1 | Simulated data reported as measurements | 3 | 5 | 15 | `data_source` on every row, session and screen banner; analysis filter documented; this is a stated project rule | low | researcher |
| D2 | Colour claims without a real calibration | 4 | 4 | 16 | status badges, degraded quality, blocked automatic colour input; calibration workflow documented | medium until measured | researcher |
| D3 | Fabricated or assumed profile targets | 3 | 5 | 15 | profiles ship empty with `REQUIRES_CALIBRATION`; warnings in the GUI and summaries | low | researcher |
| D4 | A sensor fault silently missing from a dataset | 3 | 3 | 9 | quality columns per measurement; empty-for-missing convention; fault events | low | researcher |
| D5 | Session files lost (disk failure) | 2 | 4 | 8 | export after each trial; back up the session directory; `STORAGE_FAILURE` alarm | low | operator |
| D6 | Statistics claimed beyond the design | 3 | 4 | 12 | analysis plan fixed before collection; report dispersion and effect sizes; pre-register replicates | low | researcher |
| D7 | Insufficient replicates to compare strategies | 3 | 3 | 9 | staged plan; treat under-replicated results as exploratory only | medium | researcher |
| D8 | Comparing across days with different ambient conditions | 3 | 3 | 9 | randomise run order within blocks; record ambient conditions | medium | operator |

## C. Schedule and delivery risks

| ID | Risk | L | I | P | Mitigation | Residual | Owner |
|---|---|---|---|---|---|---|---|
| P1 | Hardware delivery/parts delay | 3 | 4 | 12 | software is complete and hardware-independent; procure the safety items first; keep the staged fallback | medium | project lead |
| P2 | Quotation/specification mismatch on delivery | 3 | 3 | 9 | verify every item on arrival against `docs/03...` §1 and update the BOM | medium | project lead |
| P3 | HMI incompatible with the browser dashboard | 3 | 3 | 9 | decision documented with three options; a browser/panel PC needs no new code | medium | software |
| P4 | Colour calibration proves infeasible with the TCS3200 | 3 | 4 | 12 | documented fallback (different sensor or bench measurement of cool samples) to be approved, not purchased unilaterally | high | researcher |
| P5 | Not enough time to validate all 12 profiles | 4 | 3 | 12 | staged approach: one variety × three levels validated, the rest explicitly PROVISIONAL | medium | researcher |
| P6 | Late discovery of a safety problem blocks trials | 2 | 5 | 10 | commissioning sequence in `docs/04...` §5 runs before beans and before mains | low if followed | builder |
| P7 | Firmware defects found only on hardware | 4 | 2 | 8 | protocol contract tests; staged bring-up with LED/lamp loads | medium | embedded |
| P8 | Thesis deadline (19 Dec 2026) compresses analysis time | 3 | 4 | 12 | freeze the configuration before the analysis window; documentation maintained per phase | medium | project lead |

## D. Technical risks the software cannot remove

| ID | Risk | Note |
|---|---|---|
| T1 | One probe cannot represent both chamber and bean temperature | must be resolved by instrumentation, not software |
| T2 | Colour measured through a smoky window may be unusable | needs an optical solution or a different sampling method |
| T3 | E-nose sensor drift across sessions | baseline per session, replace on schedule; do not compare raw values across days without normalisation |
| T4 | Fuzzy control may not beat a simple baseline on this plant | an acceptable, reportable outcome — provided it is measured |
| T5 | Time-proportioning ripple may be visible in the temperature trace | quantify it during plant characterisation; adjust the window rather than guessing |

## E. Review triggers

Re-score this register when any of the following happens:

* the real quotation or delivered parts change the BOM;
* the heater circuit is measured for the first time;
* the HMI model is identified;
* colour calibration is measured (successfully or not);
* the first supported roast completes;
* the commissioning ceiling is raised or `heater.hardware_verified` changes;
* a defect in `docs/14_handover_checklist.md` §G is closed or a new one is added.
