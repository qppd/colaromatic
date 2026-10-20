# 02 — System architecture

## 1. Layers

```
┌──────────────────────────── GUI / application layer ─────────────────────────────┐
│ src/colaromatic/web (index.html, app.js, charts.js, styles.css)                      │
│ FastAPI server: REST + WebSocket, session listing, CSV/JSON export                │
│ observes only — closing the browser changes nothing about safety or control       │
└───────────────────────────────────┬──────────────────────────────────────────────┘
                                    │ snapshot dict @ gui_broadcast_period_s (1 s)
┌───────────────────────────────────▼──────────────────────────────────────────────┐
│ RoastEngine (src/colaromatic/app/engine.py)                                          │
│  schedules, session lifecycle, event log, snapshot builder, idle monitoring       │
└───┬─────────────┬──────────────┬──────────────┬───────────────┬──────────────────┘
    │             │              │              │               │
┌───▼─────┐ ┌─────▼──────┐ ┌─────▼──────┐ ┌─────▼──────┐ ┌──────▼──────────┐
│ sensors │ │ processing │ │  control   │ │  profiles  │ │    storage      │
│ base    │ │ color      │ │ fuzzy      │ │ model      │ │ session (CSV,   │
│ simulator│ │ temperature│ │ heater     │ │ manager    │ │  JSONL)         │
│ serial  │ │ stages     │ │ safety     │ │            │ │ export (CSV/JSON)│
└───┬─────┘ └────────────┘ └────────────┘ └────────────┘ └─────────────────┘
    │
┌───▼────────────────────────────── machine boundary ───────────────────────────────┐
│ MachineInterface (read / set_heater / set_fan / link_ok / drain_events)            │
│   • SimulatedMachine  (physics + synthetic sensors)   ← SIMULATED                  │
│   • MachineSerialBridge → ESP32 over USB serial       ← HARDWARE                   │
└───────────────────────────────────────────────────────────────────────────────────┘
```

## 2. Why the layers are split this way

* **One machine boundary.** Control, logging and GUI code is identical for
  simulation and hardware. Swapping `--mode sim` for `--mode hardware` changes
  only which `MachineInterface` implementation the engine receives
  (REQ-SYS-001).
* **Safety is between the controller and the heater.** The supervisor evaluates
  independently of the fuzzy controller and can remove heater authority without
  the controller's cooperation (REQ-FUZ-004).
* **Profiles are data.** There are 12 profiles and one controller
  implementation; a profile supplies the target trajectory, colour target,
  controller reference and endpoint criteria (REQ-PROF-001).
* **The HMI is not in the control path.** The engine runs headless, and can be
  driven entirely from the API or `src/tools/run_simulation.py`
  (REQ-TIME-003).
* **Everything measurable is timestamped with provenance.** Every row carries
  `data_source`, every derived value carries a status, and every colour value
  carries the calibration status that produced it.

## 3. Data flow of one control tick

```
clock.tick (0.2 s)
  ├─ drain machine events (acoustic crack)                     → stage detector
  ├─ every sensor_period_s (1 s)
  │    machine.read() → MachineSample (raw, quality, provenance)
  │      ├─ TemperatureProcessor.add(ts, raw)  → filtered, RoR (regression over real ts)
  │      ├─ ColorProcessor.process(raw, target) → L*a*b*, ΔE, calibration status
  │      ├─ e-nose baseline acquisition (median of first samples)
  │      ├─ StageDetector.update(...)          → stage, method, estimated/confirmed
  │      ├─ SafetySupervisor.evaluate(...)     → level, faults, heater_permitted
  │      └─ evaluate_endpoint(...)             → triggered / needs confirmation
  ├─ every control_period_s (2 s) and only while RUNNING
  │    u = target(elapsed) − filtered_temperature        (auto mode)
  │    FuzzyController.evaluate({temp_error, ror, [delta_e]}) → duty 0…100 %
  │    duty ← min(duty, profile limit, commissioning ceiling, config ceiling)
  │    HeaterOutput.command(duty)            → applied duty, min on/off respected
  ├─ every tick
  │    relay = HeaterOutput.relay_on(mono) and safety.heater_permitted
  │    machine.set_heater(relay)  (sent only on change)
  └─ every research_log_period_s (30 s) + on events
       SessionRecorder.log_row(ProcessSample)
```

## 4. Timing schedules

| Schedule | Default | Where | Rationale |
|---|---|---|---|
| Output/relay tick | 0.2 s | `engine.TICK_S` | resolves the 10 s time-proportioning window to 2 % granularity |
| Sensor acquisition | 1 s | `timing.sensor_period_s` | fast enough for RoR and staleness detection |
| Safety evaluation | every sensor tick | `engine._evaluate_safety` | a fault removes heater authority within ~1 s |
| Control computation | 2 s | `timing.control_period_s` | process responds in tens of seconds; avoids hunting |
| GUI broadcast | 1 s | `timing.gui_broadcast_period_s` | cosmetic; independent of control |
| Research logging | 30 s | `timing.research_log_period_s` | matches the initial specification; see note |
| Event logging | on occurrence | `timing.event_log_enabled` | stage changes, faults, heater command changes ≥5 % |

**Note on the 30 s logging interval.** The proposal specifies 30 s. That is
adequate for slow trends but coarse for control analysis; the default is kept
at the specified value so the stored data matches the approved protocol, and
`src/config/system.json` documents the alternative. Reducing it to 5 s is a
one-line change and must be recorded as a protocol deviation if used
(REQ-TIME-004).

## 5. Session state machine

```
IDLE ──start──► RUNNING ──operator stop / endpoint / fault──► COOLING ──cool enough──► COMPLETE
   ▲                                                                                      │
   └──────────────────────────── start a new session ◄──────────────────────────────────┘
                            (estop, or a critical/latched fault) ──► ABORTED
```

* `monitor` mode: RUNNING with the heater disabled.
* A critical safety fault during RUNNING begins COOLING with completion status
  `aborted_safety`; the record keeps the fault list and the reason.
* Emergency stop engages a latched fault, starts COOLING, and cannot be
  released while the batch is above `estop_reset_max_c`.
* COOLING keeps sampling: cooling is only declared complete on a **fresh valid**
  temperature reading; otherwise it ends at `cooling_max_s` and the summary
  warns that cooling ended unverified.
* Idle monitoring continues at 1 Hz after a session so latched faults can be
  acknowledged safely and the GUI can show a live temperature.

## 6. Threading and determinism

* The engine owns one worker thread that calls `step()` every 0.2 s. `step()`
  takes a re-entrant lock, so `snapshot()` and every API action is safe from
  other threads.
* Time comes from an injected `Clock`. Tests use `ManualClock` and drive
  `engine.step()` directly, so the entire control, logging and API stack runs
  deterministically and fast (155 tests in ~7 s).
* The serial bridge runs a reader thread; the last value of each frame is
  cached **with a host receipt time**, so silence becomes a *stale* reading
  rather than a frozen valid one.

## 7. Module dependency rules

| Module | May import | Must not import |
|---|---|---|
| `core` | stdlib, pydantic | anything else in the project |
| `processing` | `core` | `control`, `app`, `storage` |
| `control` | `core`, `processing` | `app`, `storage`, `web` |
| `profiles` | `core` | `control`, `app` |
| `sensors` | `core`, `processing` | `control`, `app`, `storage` |
| `storage` | `core`, `profiles` | `control`, `app` |
| `app` | everything above | — |

Hardware-specific code lives only in `sensors/serial_bridge.py` and
`src/esp/ColAromatic/`, so the control algorithm can be tested without a
machine (REQ-SYS-001).

## 8. Error handling strategy

| Condition | Behaviour |
|---|---|
| Sensor value invalid | value becomes `None`, status `invalid`, fault raised if safety-relevant |
| Sensor stale / link lost | heater authority removed, `MACHINE_LINK_LOST` / `TEMPERATURE_STALE` |
| Storage write failure | `STORAGE_FAILURE` warning, `storage_ok=false` surfaced in the GUI; roasting continues (data loss is not a safety hazard) |
| Fuzzy input missing | controller returns no output; duty 0; diagnostic in the snapshot (it never guesses) |
| Heater write failure | relay state forced off, error surfaced, retry next tick |
| Engine loop exception | caught per tick, recorded in `error`, heater command dropped, loop continues |
| Controller restart / firmware reset | host treats missing frames as stale; firmware turns the heater off on its own timeout |
| Host stalls or dies | firmware drops heater authority after 3 s of silence; a live host re-asserts its command every 1 s so it never looks dead |

## 9. Deployment options

| Option | Command | Use |
|---|---|---|
| Simulation, browser GUI | `python -m colaromatic.app.server --mode sim --open` | development, demonstration, operator training |
| Hardware, browser GUI | `COLAROMATIC_SERIAL_PORT=COM7 python -m colaromatic.app.server --mode hardware` | bench and trials |
| Headless scripted run | `python src/tools/run_simulation.py …` | dataset generation, regression checks |
| Library use | `RoastEngine` + `MachineInterface` | custom tooling |

For a touchscreen HMI (REQ-GUI-009): if the panel runs a browser or an
embedded WebView, point it at the host's `/` URL. If it is a serial/nextion
panel, a thin adapter that mirrors the WebSocket snapshot must be written — the
protocol is already JSON on the wire, but this has **not** been built or tested.
