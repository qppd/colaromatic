/* FORMERLY GENERATED — NOW HAND-MAINTAINED.
 * Originally written by src/tools/gen_firmware_tables.py from src/config/*.json,
 * both of which were removed with the Python host, so these values are now
 * edited directly here and must be reviewed like any other code (docs/01 REQ-SYS-002). */

#pragma once

#include <stdint.h>

#include <math.h>

/* source: src/config/controllers/default_provisional.json */

#define GEN_FUZZY_ID      "default_provisional"
#define GEN_FUZZY_STATUS  "INITIAL_UNVALIDATED"
#define GEN_FUZZY_INPUT_COUNT      (3)
#define GEN_FUZZY_OUTPUT_TERM_COUNT (5)
#define GEN_FUZZY_RULE_COUNT       (13)
#define GEN_FUZZY_OUT_MIN          (0.0f)
#define GEN_FUZZY_OUT_MAX          (100.0f)
#define GEN_FUZZY_OUT_RES          (101)
#define GEN_FUZZY_FALLBACK_PCT     (0.0f)

struct GenFuzzyTerm { const char *name; uint8_t n; const float *pts; };
struct GenFuzzyInput {
  const char *id;
  float umin, umax;
  uint8_t enabled;
  const GenFuzzyTerm *terms;
  uint8_t termCount;
};
struct GenFuzzyAnt { uint8_t input; uint8_t term; uint8_t negate; };
struct GenFuzzyRule {
  const char *id;
  const GenFuzzyAnt *ants;
  uint8_t antCount;
  uint8_t consequent;   /* index into GEN_FUZZY_OUTPUT_TERMS */
  float weight;
};

static const float GEN_IN0_PTS[] = {
  /* neg_large */ -25.0f, 1.0f, -15.0f, 1.0f, -8.0f, 0.0f,
  /* neg_small */ -12.0f, 0.0f, -5.0f, 1.0f, -1.0f, 0.0f,
  /* zero */ -3.0f, 0.0f, 0.0f, 1.0f, 3.0f, 0.0f,
  /* pos_small */ 1.0f, 0.0f, 6.0f, 1.0f, 13.0f, 0.0f,
  /* pos_large */ 9.0f, 0.0f, 16.0f, 1.0f, 25.0f, 1.0f,
};
static const GenFuzzyTerm GEN_IN0_TERMS[] = { {"neg_large", 3, GEN_IN0_PTS + 0}, {"neg_small", 3, GEN_IN0_PTS + 2}, {"zero", 3, GEN_IN0_PTS + 4}, {"pos_small", 3, GEN_IN0_PTS + 6}, {"pos_large", 3, GEN_IN0_PTS + 8} };

static const float GEN_IN1_PTS[] = {
  /* falling */ -10.0f, 1.0f, -4.0f, 1.0f, 0.0f, 0.0f,
  /* steady */ -1.0f, 0.0f, 2.0f, 1.0f, 6.0f, 0.0f,
  /* rising */ 4.0f, 0.0f, 9.0f, 1.0f, 14.0f, 0.0f,
  /* fast */ 12.0f, 0.0f, 20.0f, 1.0f, 30.0f, 1.0f,
};
static const GenFuzzyTerm GEN_IN1_TERMS[] = { {"falling", 3, GEN_IN1_PTS + 0}, {"steady", 3, GEN_IN1_PTS + 2}, {"rising", 3, GEN_IN1_PTS + 4}, {"fast", 3, GEN_IN1_PTS + 6} };

static const float GEN_IN2_PTS[] = {
  /* on_target */ 0.0f, 1.0f, 2.0f, 1.0f, 4.0f, 0.0f,
  /* near */ 3.0f, 0.0f, 6.0f, 1.0f, 9.0f, 0.0f,
  /* far */ 8.0f, 0.0f, 15.0f, 1.0f, 30.0f, 1.0f,
};
static const GenFuzzyTerm GEN_IN2_TERMS[] = { {"on_target", 3, GEN_IN2_PTS + 0}, {"near", 3, GEN_IN2_PTS + 2}, {"far", 3, GEN_IN2_PTS + 4} };

static const float GEN_OUT0_PTS[] = {0.0f, 1.0f, 4.0f, 1.0f, 10.0f, 0.0f};
static const float GEN_OUT1_PTS[] = {5.0f, 0.0f, 18.0f, 1.0f, 32.0f, 0.0f};
static const float GEN_OUT2_PTS[] = {26.0f, 0.0f, 45.0f, 1.0f, 64.0f, 0.0f};
static const float GEN_OUT3_PTS[] = {58.0f, 0.0f, 75.0f, 1.0f, 90.0f, 0.0f};
static const float GEN_OUT4_PTS[] = {85.0f, 0.0f, 96.0f, 1.0f, 100.0f, 1.0f};

static const GenFuzzyTerm GEN_FUZZY_OUTPUT_TERMS[] = {
  {"off", 3, GEN_OUT0_PTS},
  {"low", 3, GEN_OUT1_PTS},
  {"medium", 3, GEN_OUT2_PTS},
  {"high", 3, GEN_OUT3_PTS},
  {"full", 3, GEN_OUT4_PTS}
};

static const GenFuzzyInput GEN_FUZZY_INPUTS[GEN_FUZZY_INPUT_COUNT] = {
  {"temp_error_c", -25.0f, 25.0f, 1, GEN_IN0_TERMS, 5},
  {"ror_c_per_min", -10.0f, 30.0f, 1, GEN_IN1_TERMS, 4},
  {"delta_e", 0.0f, 30.0f, 0, GEN_IN2_TERMS, 3},
};

static const GenFuzzyAnt GEN_RULE0_ANTS[] = { {0, 0, 0} };
static const GenFuzzyAnt GEN_RULE1_ANTS[] = { {0, 1, 0}, {1, 3, 0} };
static const GenFuzzyAnt GEN_RULE2_ANTS[] = { {0, 1, 0}, {1, 1, 0} };
static const GenFuzzyAnt GEN_RULE3_ANTS[] = { {0, 1, 0}, {1, 0, 0} };
static const GenFuzzyAnt GEN_RULE4_ANTS[] = { {0, 2, 0}, {1, 3, 0} };
static const GenFuzzyAnt GEN_RULE5_ANTS[] = { {0, 2, 0}, {1, 1, 0} };
static const GenFuzzyAnt GEN_RULE6_ANTS[] = { {0, 2, 0}, {1, 0, 0} };
static const GenFuzzyAnt GEN_RULE7_ANTS[] = { {0, 3, 0}, {1, 3, 0} };
static const GenFuzzyAnt GEN_RULE8_ANTS[] = { {0, 3, 0}, {1, 2, 0} };
static const GenFuzzyAnt GEN_RULE9_ANTS[] = { {0, 3, 0}, {1, 0, 0} };
static const GenFuzzyAnt GEN_RULE10_ANTS[] = { {0, 4, 0}, {1, 3, 0} };
static const GenFuzzyAnt GEN_RULE11_ANTS[] = { {0, 4, 0}, {1, 1, 0} };
static const GenFuzzyAnt GEN_RULE12_ANTS[] = { {0, 4, 0}, {1, 0, 0} };

static const GenFuzzyRule GEN_FUZZY_RULES[GEN_FUZZY_RULE_COUNT] = {
  {"R01", GEN_RULE0_ANTS, 1, 0, 1.0f},
  {"R02", GEN_RULE1_ANTS, 2, 0, 1.0f},
  {"R03", GEN_RULE2_ANTS, 2, 1, 1.0f},
  {"R04", GEN_RULE3_ANTS, 2, 1, 1.0f},
  {"R05", GEN_RULE4_ANTS, 2, 1, 1.0f},
  {"R06", GEN_RULE5_ANTS, 2, 2, 1.0f},
  {"R07", GEN_RULE6_ANTS, 2, 3, 1.0f},
  {"R08", GEN_RULE7_ANTS, 2, 1, 1.0f},
  {"R09", GEN_RULE8_ANTS, 2, 2, 1.0f},
  {"R10", GEN_RULE9_ANTS, 2, 3, 1.0f},
  {"R11", GEN_RULE10_ANTS, 2, 2, 1.0f},
  {"R12", GEN_RULE11_ANTS, 2, 3, 1.0f},
  {"R13", GEN_RULE12_ANTS, 2, 4, 1.0f},
};
