/* FORMERLY GENERATED — NOW HAND-MAINTAINED.
 * Originally written by src/tools/gen_firmware_tables.py from src/config/*.json,
 * both of which were removed with the Python host, so these values are now
 * edited directly here and must be reviewed like any other code (docs/01 REQ-SYS-002). */

#pragma once

#include <stdint.h>

/* source: src/config/system.json */

/* --- timing (ms) --- */
#define GEN_SENSOR_PERIOD_MS      (1000)
#define GEN_CONTROL_PERIOD_MS     (2000)
#define GEN_RESEARCH_LOG_PERIOD_MS (30000)

/* --- temperature processing --- */
#define GEN_EMA_ALPHA             (0.25f)
#define GEN_ROR_WINDOW_S          (30.0f)
#define GEN_ROR_MIN_SPAN_S        (10.0f)
#define GEN_TEMP_MIN_VALID_C      (-20.0f)
#define GEN_TEMP_MAX_VALID_C      (400.0f)
#define GEN_TEMP_STALE_S          (3.0f)

/* --- safety --- */
#define GEN_MAX_TEMP_C            (240.0f)
#define GEN_RESET_TEMP_C          (120.0f)
#define GEN_ESTOP_RESET_MAX_C     (80.0f)
#define GEN_MAX_ROR_CPM           (45.0f)
#define GEN_ROR_RUNAWAY_STRIKES   (3)  /* consecutive control evaluations (safety.py) */
#define GEN_ENOSE_MAX_C           (80.0f)

/* --- heater shaping (time proportioning, zero-cross SSR) --- */
#define GEN_HEATER_WINDOW_MS      (10000)
#define GEN_HEATER_MIN_ON_MS      (500)
#define GEN_HEATER_MIN_OFF_MS     (500)
#define GEN_HEATER_MIN_PCT        (0.0f)
#define GEN_HEATER_MAX_PCT        (100.0f)
#define GEN_DUTY_CEILING_PCT      (30.0f)  /* min(commissioning_max_pct, max_pct) while hardware_verified=false */

/* --- actuators --- */
#define GEN_ROAST_FAN_PCT         (70)
#define GEN_COOLING_FAN_PCT       (100)

/* --- cooling phase --- */
#define GEN_COOLING_MIN_S         (60.0f)
#define GEN_COOLING_MAX_S         (600.0f)
#define GEN_COOLING_TARGET_C      (60.0f)

/* --- stage detection --- */
#define GEN_TP_REBOUND_C          (3.0f)
#define GEN_TP_MIN_ELAPSED_S      (30.0f)
#define GEN_ALLOW_ACOUSTIC        (1)
#define GEN_ACOUSTIC_MIN_CONF     (0.6f)
#define GEN_COLOR_CHANGE_DELTA_L  (-1.0f)  /* <0: disabled (no validated colour threshold) */

/* --- colour validity window (raw Hz, no CIELAB conversion on the MCU) --- */
#define GEN_COLOR_MIN_HZ          (10.0f)
#define GEN_COLOR_MAX_HZ          (2e+06f)
