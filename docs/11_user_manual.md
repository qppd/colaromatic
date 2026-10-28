# 11 — Operator user manual

Audience: the researcher/operator running the prototype machine.
Read [13 — Known limitations](13_known_limitations.md) before interpreting any
value. There is **no browser, no PC and no app**: everything below happens on
the touch panel (`src/esp/HMI/`), which talks to the controller over ESP-NOW.

## 1. Powering up and pairing

1. Power the controller first, then the panel. The controller prints
   `# controller MAC AA:BB:CC:DD:EE:FF` on the USB serial port (115200), and the
   panel prints its own MAC the same way.
2. Each board must know the other's MAC. The controller reads `HMI_PEER_MAC` in
   `CONF.h`; the panel reads `CONTROLLER_PEER_MAC` in `esp_now_link.h`. Paste
   each MAC into the other firmware and reflash, **or** leave both zero and use
   the handshake: press **RE-PAIR** on the panel's diagnostics screen. That sends
   `OP_PAIR`, both ends forget their stored peer, and the panel adopts the first
   controller that answers its beacon with a valid STATE frame. The diagnostics
   screen shows both MACs so you can verify which board you are talking to.
3. Within 5 s of boot the panel must show live values. If the header reads
   `OFFLINE`, the radio link is not up — see §9 and docs/12.
4. The boot screen appears for 3 s, then the dashboard loads.

Nothing here needs an access point, an internet connection or a laptop. USB
serial is a **service port only**: it is for bench work (docs/05), and while a
roast is live the serial path cannot drive the heater or the fan.

## 2. Screen map

| Screen | What it is for |
|---|---|
| **Boot** | splash, firmware version, 3 s timer |
| **Dashboard** | live values: temperature + target, RoR, duty `commanded/applied`, stage + method, session time, profile, `temp/colour/gas` quality, endpoint banner, link state |
| **Control** | bean/roast profile picker, stage confirmation, control mode (MANUAL / AUTOMATIC), automatic target ±1/±10 °C, manual duty slider, START / STOP & COOL |
| **Curves** (analytics) | live charts: temperature + target, RoR, colour (clear channel), gas |
| **Diagnostics** (diag) | link state, pairing, self/peer MAC, frame age, sensor quality, **ACTIVE FAULTS**, **LATCHED FAULTS**, controller event log, RESET FAULTS |

The **bottom bar** on the dashboard is always present during a session:
`START`, `HOLD`/`RESUME`, `STOP & COOL`, and `E-STOP` — the machine-stopping
actions are one tap away from anywhere (REQ-GUI-010).

## 3. Before every roast (pre-flight)

1. **Hardware checks.** Probe seated and reading a plausible ambient value;
   colour window clean; sampling tube clear; fan free; E-stop reachable; the
   independent thermal cutoff present and untripped.
2. **Link.** Dashboard header shows a connected icon, not `OFFLINE`, and
   Diagnostics shows `connected`, `paired: yes`, and a frame age of "just now".
3. **Faults.** Diagnostics shows `none` under both ACTIVE and LATCHED faults.
   A latched fault blocks START until it is acknowledged **while the batch is
   cool** (RESET FAULTS refuses otherwise and tells you why).
4. **Quality.** Dashboard quality line reads `temp ok  colour …  gas …`. Colour
   is reported as `uncalibrated` by design — there is no colour calibration in
   this repository (docs/07), so colour is a trend, never a measurement.
5. **Profile.** Every shipped profile is `PROVISIONAL` and none is
   auto-eligible, so **AUTOMATIC mode will be refused**. That is expected, not a
   bug: use MANUAL duty until a profile is validated (docs/01 §B).
6. **Recording.** There is none. No SD card, no session file — whatever you see
   on Curves is lost when the panel powers off (docs/01 §J). Photograph the
   screens if you need evidence.

## 4. Starting a roast

1. Open **Control** and pick a bean/roast combination from the profile grid; the
   `current:` line confirms what is loaded.
2. Choose the **control mode**:

   | Mode | Behaviour | When to use |
   |---|---|---|
   | MANUAL | you set a fixed duty with the slider; no setpoint, no PID | commissioning, baselines, every roast today |
   | AUTOMATIC | colour error → setpoint (fuzzy) → PID → duty | only once a profile is auto-eligible and the colour target is measured |

3. In AUTOMATIC, set the target with `−10 / −1 / +1 / +10`. In MANUAL, set the
   duty. The applied duty is always ≤ the ceiling shown by the controller; the
   panel cannot raise a ceiling.
4. Press **START**. If the controller refuses, the reason appears immediately as
   a popup — see §7.
5. The mode and the profile are now **fixed for the life of the session**;
   changing either is refused until the roast ends (REQ-CTRL-005).

## 5. During the roast

* Watch **temperature against target** and the **RoR** curve on Curves. Charts
  only advance while telemetry is arriving, and they restart when you re-open
  the screen.
* **Stage** advances automatically only where the sensors support it (turning
  point from the temperature history). Everything else is yours: pick the stage
  on the Control screen and press **CONFIRM**. First and second crack should
  always be confirmed by an operator — the acoustic path is disabled by default.
* The stage line shows `method · confirmed/estimated`. Never quote an estimate
  as an observation.
* **HOLD** sets duty to 0 % while monitoring and the fan continue; **RESUME**
  returns to the previous mode with a clean controller state.
* **STOP & COOL** ends the roast: heater off, fan at full. Cooling never stops
  because you stopped roasting.
* **E-STOP** removes heater authority immediately and latches the fault. It
  cannot be released while the batch is hot. The physical E-stop switch behaves
  the same way. Do not use either as a routine stop.
* If the temperature probe fails, the heater drops **on the controller**, the
  session is aborted to cooling, and an event is pushed to the panel. Heating
  does not resume by itself when the probe comes back — a fresh START is needed.
* If the link drops, the header shows `OFFLINE`/`LINK LOST`; the controller
  removes heater authority after 5 s of silence by itself, without the panel
  asking it to.

## 6. Ending the roast

* Maximum duration ends the run by itself; every other endpoint criterion only
  raises a **pending endpoint**, which you confirm with the endpoint button.
  Until you confirm, nothing ends the roast (REQ-STAGE-007).
* The completion popup appears only after the stopping conditions are met
  (REQ-GUI-006) and shows the session time. It is the only summary you get:
  there is no export and no file (REQ-GUI-007 `PARTIAL`, REQ-GUI-008
  `NOT-IMPLEMENTED`).
* Record the trial-sheet entries from docs/10 by hand or from photographs.

## 7. Refusals you will see

The controller answers every command with an ACK and a reason string; the panel
turns that into a popup. Typical texts:

| Message | Meaning | Action |
|---|---|---|
| `SAFETY_LATCHED` | a latched fault is present | clear it on Diagnostics while cool |
| `PROFILE_NOT_ELIGIBLE` / auto refused | the profile is `PROVISIONAL`, `autoEligible = 0` | use MANUAL |
| `BAD_PROFILE` / `BAD_ARG` | index or value out of range | reselect on the Control screen |
| `SESSION_ACTIVE` | a session is already running, or serial tried to drive outputs during one | stop it first |
| `STATE`-class refusal (e.g. resume while not on hold) | the command does not apply to the current state | check the state on the dashboard |
| `ESTOP_STILL_ACTIVE` | the E-stop input is still held | release the switch, then retry |
| `RESET` refused while hot | latched fault and the batch is above the release temperature | wait for it to cool |

## 8. Faults

Bits shown under ACTIVE and LATCHED on the Diagnostics screen:

| Bit | Meaning | What the machine does |
|---|---|---|
| `F_TEMP_INVALID` | no usable thermocouple reading | heater off, session aborted to cooling |
| `F_TEMP_STALE` | readings stopped arriving | same as above |
| `F_OVER_TEMP` | process limit reached | **latched**, heater off until reset while cool |
| `F_LOCAL_OVERTEMP` | firmware backstop (`CONF.h`) | heater off; clears on hysteresis |
| `F_ROR_RUNAWAY` | RoR above limit for 3 consecutive sensor ticks | **latched** |
| `F_ESTOP` | E-stop input or operator command | **latched**; release gated on temperature |
| `F_HMI_LINK_LOST` | no panel frame for 5 s | heater authority removed |
| `F_HOST_TIMEOUT` | no recognised serial frame for 3 s after a host was live | heater authority removed |
| `F_ENOSE_HOT` | e-nose sensor above its limit | **warning only** — no heater interlock |

Latched faults need **RESET FAULTS** while the machine is cool; active faults
clear when their condition clears. A trip always clears the duty request too, so
the heater never restarts by itself.

## 9. Common panel symptoms

Full table in docs/12. The three you will hit most:

| Symptom | Likely cause | Action |
|---|---|---|
| `OFFLINE` and `--` everywhere | peer MAC wrong, boards on different channels, or the panel booted before the controller | check both MACs, reflash, power the controller first |
| Charts frozen | no telemetry since the screen was opened | check link age on Diagnostics |
| Values stuck at `--` | controller has no valid probe | see §8, check the thermocouple wiring |

## 10. After the roast

1. Confirm cooling finished and the batch is cool before touching anything.
2. Record the trial sheet (docs/10) from what you saw; nothing is stored.
3. Clean the colour window and note it in the maintenance log.
4. If you used the USB service port, note that a session already active makes
   `SET_HEATER`/`SET_FAN` fail with `SESSION_ACTIVE` — that is intentional.

## 11. Maintenance schedule

| Interval | Task |
|---|---|
| Every roast | clean the colour window, check the sampling tube, check the probe connector |
| Every 10 roasts | verify the thermocouple in ice/boiling water; inspect wiring and terminals |
| Monthly | inspect the SSR heatsinks and the enclosure earth |
| Before each trial campaign | trip the hardware thermal cutoff deliberately; verify earth continuity |
| Whenever firmware is re-flashed | re-check the pairing (both MACs), re-run the pre-flight checks, and run a supervised no-bean roast before trusting automatic mode |
