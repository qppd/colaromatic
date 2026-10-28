# 08 — Data schema

**There is no recorded data.** This document therefore describes the schemas
that actually exist in the running system: the frames on the two links, the
enums they carry, and the compiled tables. It replaces the host-era schema
document, which described CSV/JSON session files that no longer have anything to
write them.

| What you might expect | Reality |
|---|---|
| Session metadata file (REQ-LOG-001) | `NOT-IMPLEMENTED` |
| Time-series CSV (REQ-LOG-002) | `NOT-IMPLEMENTED` — no SD writer |
| CSV/JSON export (REQ-LOG-003) | `NOT-IMPLEMENTED` |
| Research summary (REQ-LOG-005) | `NOT-IMPLEMENTED` |
| Event log | exists, but only as a **volatile ring buffer** on the panel (`hmi_data.*`) |

All field names below are the C identifiers in `src/esp/ColAromatic/LinkFrames.h`
(the panel carries an identical copy in `src/esp/HMI/LinkFrames.h`).

## 1. Conventions used by every frame

| Convention | Value | Why |
|---|---|---|
| Byte order | little endian | both boards are Xtensa/ESP32 |
| Struct packing | `__attribute__((packed))` | the layout is the wire format |
| Fixed point | temperatures and RoR ×10 as `int16_t`; gas resistance ×1000 as `int32_t` | integer frames, no float on the wire |
| "Unavailable" sentinel | `INT16_MIN` / `INT32_MIN` | never confusable with a reading |
| Text fields | `char[]`, NUL-padded, not necessarily NUL-terminated if full | fixed-size, no dynamic allocation |
| "No value" for arguments | `LINK_PROFILE_NONE` (0xFF), `LINK_SETPOINT_UNCHANGED` (0x8000) | distinguish "unchanged" from "zero" |
| Integrity | CRC16/CCITT (poly 0x1021, init 0xFFFF) over header-with-crc-zeroed + payload | `linkFrameCrc()` |
| Versioning | `LINK_VERSION` in every header; mismatched frames are dropped | the two sketches are compiled separately |
| Retransmission | `LINK_CMD_REPEATS` (3) sends, sequence number applies once | ESP-NOW has no delivery guarantee |
| Channel | `LINK_WIFI_CHANNEL` (1), baked into both firmwares | ESP-NOW only hears the sender's channel |

## 2. Frame header (12 bytes, every frame)

| Field | Type | Meaning |
|---|---|---|
| `magic` | u8 | start-of-frame marker |
| `version` | u8 | `LINK_VERSION` |
| `kind` | u8 | `LinkFrameKind` (§3) |
| `flags` | u8 | `LINK_FLAG_PAIR` = sender is pairing / accepts a pairing peer |
| `seq` | u32 | sender's monotonically increasing sequence number |
| `payloadLen` | u16 | payload size in bytes |
| `crc` | u16 | CRC16/CCITT as described above |

A frame that fails magic, version, length or CRC is discarded before it can
change any state (REQ-FW-004), and a `seq` that has already been applied is
re-ACKed rather than re-applied (REQ-FW-007).

## 3. Frame kinds and payloads

| `kind` | Name | Direction | Payload |
|---|---|---|---|
| 1 | `LINK_BEACON` | HMI → controller | `LinkBeacon` |
| 2 | `LINK_STATE` | controller → HMI | `LinkState` |
| 3 | `LINK_CMD` | HMI → controller | `LinkCmd` |
| 4 | `LINK_ACK` | controller → HMI | `LinkAck` |
| 5 | `LINK_EVENT` | controller → HMI | `LinkEvent` |

### 3.1 `LinkBeacon` (HMI → controller)

| Field | Type | Meaning |
|---|---|---|
| `uptimeMs` | u32 | panel uptime |
| `hmiVersion` | u16 | panel firmware version |
| `reserved` | u16 | — |

### 3.2 `LinkState` (controller → HMI), sent every `LINK_STATE_PERIOD_MS` (500 ms)

| Field | Type | Unit / sentinel | Meaning |
|---|---|---|---|
| `uptimeMs` | u32 | ms | controller uptime |
| `sessionMs` | u32 | ms | roast elapsed (0 when idle) |
| `tempC10` | i16 | °C ×10, `INT16_MIN` = n/a | filtered temperature |
| `rorCpm10` | i16 | °C/min ×10, `INT16_MIN` = n/a | rate of rise |
| `setpointC10` | i16 | °C ×10 | current target (colour setpoint, trajectory or operator value) |
| `tempQuality` | u8 | `LinkTempQuality` | `TEMP_OK/DEGRADED/INVALID/STALE` |
| `roastState` | u8 | `LinkRoastState` | `IDLE/RUNNING/HOLD/COOLING/DONE` |
| `mode` | u8 | `LinkMode` | `MODE_MANUAL` / `MODE_AUTO` |
| `stage` | u8 | `LinkStage` | charge … done (8 labels) |
| `stageMethod` | u8 | `LinkStageMethod` | operator / temp minimum / colour trend / profile endpoint / acoustic / session |
| `stageConfirmed` | u8 | 0/1 | 1 = operator confirmed, 0 = estimated |
| `dutyCmd` | u8 | % | requested duty |
| `dutyApp` | u8 | % | applied duty after ceiling and min on/off shaping |
| `fanPct` | u8 | 0 or 100 | fan SSR is on/off, never PWM |
| `relayOn` | u8 | 0/1 | actual heater SSR pin state |
| `profileIdx` | u8 | index into `GEN_PROFILES`, `LINK_PROFILE_NONE` = none | selected profile |
| `endpointPending` | u8 | 0/1 | controller is waiting for an operator confirmation |
| `faultsActive` | u16 | `LinkFaultBit` mask | currently active faults |
| `faultsLatched` | u16 | `LinkFaultBit` mask | latched until a cool reset |
| `clearHz` | u32 | Hz | TCS3200 clear channel, raw and uncalibrated |
| `gasOhmMilli` | i32 | mΩ, `INT32_MIN` = n/a | BME688 gas resistance |
| `colorStatus` | u8 | `LinkColorStatus` | calibration state (informational) |
| `reserved0/1` | — | — | zero; reserved for growth without a version bump |

### 3.3 `LinkCmd` (HMI → controller)

| Field | Type | Meaning |
|---|---|---|
| `opcode` | u8 | `LinkOpcode` (§4) |
| `a` | u8 | argument 0 (duty %, profile index, mode, stage) |
| `b` | i16 | argument 1 (setpoint °C ×10) |

### 3.4 `LinkAck` (controller → HMI)

| Field | Type | Meaning |
|---|---|---|
| `cmdSeq` | u32 | `seq` of the command being acknowledged |
| `faults` | u16 | fault mask at ack time |
| `result` | u8 | `ACK_OK` / `ACK_GATING` / `ACK_STATE` / `ACK_ARG` / `ACK_PROFILE` |
| `opcode` | u8 | echoed opcode |
| `text[24]` | char | human-readable refusal reason, NUL padded |

### 3.5 `LinkEvent` (controller → HMI)

| Field | Type | Meaning |
|---|---|---|
| `sessionMs` | u32 | roast elapsed when it happened |
| `value` | u16 | event-specific numeric evidence (e.g. applied duty ×10) |
| `type` | u8 | `LinkEventType` (§4) |
| `a`, `b` | u8 | event-specific (stage index, fault bit, end status) |
| `text[28]` | char | stage name / fault code / reason, NUL padded |

Events reach the panel's diagnostics screen and its volatile ring buffer.
**Nothing is written to storage.**

## 4. Enumerations

| Enum | Values |
|---|---|
| `LinkFrameKind` | `BEACON=1`, `STATE=2`, `CMD=3`, `ACK=4`, `EVENT=5` |
| `LinkOpcode` | `START=1`, `HOLD=2`, `RESUME=3`, `COOL=4`, `ESTOP=5`, `RESET=6`, `SET_DUTY=7`, `SET_MODE=8`, `SELECT_PROFILE=9`, `CONFIRM_STAGE=10`, `CONFIRM_ENDPOINT=11`, `SET_SETPOINT=12`, `SET_FAN=13`, `PAIR=14` (argument must be `HMI_PAIR_MAGIC` 0x5A) |
| `LinkAckResult` | `OK=0`, `GATING=1` (safety/latch/probe/link gate), `STATE=2` (wrong roast state), `ARG=3`, `PROFILE=4` |
| `LinkEventType` | `STAGE_CHANGE=1`, `FAULT_TRIP=2`, `COOLING_START=3`, `SESSION_END=4`, `ENDPOINT_PENDING=5`, `CRACK=6`, `SESSION_START=7`, `ROAST_HEARTBEAT=8`, `COOLING_UNVERIFIED=9`, `HEATER_COMMAND=10` |
| `LinkStage` | `CHARGE=0`, `TURNING_POINT=1`, `COLOR_CHANGE=2`, `FIRST_CRACK=3`, `MEDIUM_ROAST=4`, `SECOND_CRACK=5`, `DARK_ROAST=6`, `DONE=7` |
| `LinkStageMethod` | `OPERATOR=0`, `TEMP_MINIMUM=1`, `COLOR_TREND=2`, `PROFILE_ENDPOINT=3`, `ACOUSTIC=4`, `SESSION=5` |
| `LinkRoastState` | `IDLE=0`, `RUNNING=1`, `HOLD=2`, `COOLING=3`, `DONE=4` |
| `LinkMode` | `MANUAL=0`, `AUTO=1` |
| `LinkTempQuality` | `OK=0`, `DEGRADED=1`, `INVALID=2`, `STALE=3` |
| `LinkColorStatus` | `UNCALIBRATED=0`, `PROVISIONAL=1`, `VALIDATED=2`, `INVALID=3` |
| `LinkFaultBit` (u16 mask) | `F_TEMP_INVALID` 1<<0, `F_TEMP_STALE` 1<<1, `F_OVER_TEMP` 1<<2, `F_LOCAL_OVERTEMP` 1<<3, `F_ROR_RUNAWAY` 1<<4, `F_ESTOP` 1<<5, `F_HMI_LINK_LOST` 1<<6, `F_ENOSE_HOT` 1<<7 (warning only), `F_HOST_TIMEOUT` 1<<8 |

## 5. USB serial frames (ASCII)

The bench port's schema is docs/05 §1: `T`, `C`, `E`, `H`, `K`, `ERR` frames and
the `#` banner, all comma-separated, one per line. The same conventions apply
(no substitution for missing values — the field is empty), but timestamps are
`millis()` and a consumer must use its own receipt time for staleness.

## 6. Compiled tables

### 6.1 `GenProfile` (`GenProfiles.h`, 12 entries)

| Field | Type | Meaning |
|---|---|---|
| `id`, `label` | str | e.g. `arabica-medium`, `Arabica / Medium` |
| `roastLevel` | u8 | 0 light, 1 medium, 2 dark |
| `status` | u8 | 0 provisional, 1 draft, 2 under_calibration, 3 validated, 4 deprecated — **all 12 are 0** |
| `traj`, `trajCount` | `GenTrajPoint*` | measured target trajectory; `NULL` on all 12 today |
| `chargeC`, `turnPointC`, `firstCrackC`, `secondCrackC` | °C | reference points; `NAN` = not measured |
| `endpointMethod` | u8 | 0 operator, 1 colour, 2 temperature, 3 time |
| `endpointTargetC`, `endpointMaxDurationS`, `endpointDeltaETol` | — | endpoint criterion; `NAN` where unused |
| `operatorConfirmRequired` | u8 | 1 on all 12 |
| `maxDutyPct`, `setpointMaxC` | % / °C | profile caps; `NAN` = none |
| `autoEligible` | u8 | **0 on all 12** — automatic mode cannot start |

`NAN` is the schema's "not measured" marker: `RoastControl` treats it as absent,
never as zero.

### 6.2 `FuzzyTable` (`Fuzzy.*` + `ColorSetpoint.cpp` + `GenFuzzy.h`)

Inputs (`id`, `umin`, `umax`, `enabled`, terms), rules (`id`, antecedent list of
`{input, term, negate}`, weight, consequent index) and output terms
(`name`, point list). Two tables exist: the colour→setpoint table in
`ColorSetpoint.cpp` (enabled, used) and the temperature→duty table in
`GenFuzzy.h` (present, not referenced by the shipped control law — docs/06 §4).

## 7. Extending the schema

1. Edit **both** copies of `LinkFrames.h` (`src/esp/ColAromatic/` and
   `src/esp/HMI/`) — they must stay byte-identical.
2. Prefer a `reserved` field over a layout change; a layout change requires a
   `LINK_VERSION` bump and reflashing **both** boards.
3. Add any new enum value at the end of its enum.
4. Update this document and docs/05 in the same change.
5. If a recorder is ever added (docs/01 §J), this document becomes the source of
   the column definitions — that is what it is here for.
