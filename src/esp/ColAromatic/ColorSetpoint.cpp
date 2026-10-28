#include "ColorSetpoint.h"

#include <math.h>

namespace {

/* --- colour error universe, in ratio units ----------------------------- */
/* Negative = darker than the target (too little heat), positive = lighter.

   The five terms are spaced 0.15 apart and each side of a peak ramps to zero
   exactly where its neighbour's peak is, so at every error at most two terms
   are non-zero and their memberships sum to 1. That overlap is what makes the
   surface continuous: the controller never hands over from one rule to the
   next at a single point.

   (The first version of this table used three wide terms with flat tops
   ("darker" was 1 from -0.30 to -0.04). Combined with shoulder-shaped output
   terms it produced a step function: ~157 degC for every negative error, a
   30 degC jump at -0.04, a flat 190 around zero, and another 26 degC jump.
   Measured, not guessed - see the numbers in the commit.) */
const float IN_VERY_DARK_PTS[] = {-0.30f, 1.0f, -0.15f, 0.0f};
const float IN_DARK_PTS[] = {-0.30f, 0.0f, -0.15f, 1.0f, 0.00f, 0.0f};
const float IN_TARGET_PTS[] = {-0.15f, 0.0f, 0.00f, 1.0f, 0.15f, 0.0f};
const float IN_LIGHT_PTS[] = {0.00f, 0.0f, 0.15f, 1.0f, 0.30f, 0.0f};
const float IN_VERY_LIGHT_PTS[] = {0.15f, 0.0f, 0.30f, 1.0f};

const GenFuzzyTerm IN_TERMS[] = {
    {"very_dark", 2, IN_VERY_DARK_PTS},
    {"dark", 3, IN_DARK_PTS},
    {"on_target", 3, IN_TARGET_PTS},
    {"light", 3, IN_LIGHT_PTS},
    {"very_light", 2, IN_VERY_LIGHT_PTS},
};

/* --- setpoint universe, degC ------------------------------------------- */
/* Five consequents, one per input term, moves 20 degC per step up the
   universe. The two end terms are shoulders, so a single rule at full
   strength defuzzifies to the centroid of that shoulder (~156 degC at the
   cold end, ~224 degC at the hot end) rather than to the universe edge; the
   interior terms are symmetric triangles whose centroid is their peak.
   Interior rules therefore interpolate cleanly, and only the two saturated
   extremes sit slightly inside [150, 230]. */
const float OUT_COOLEST_PTS[] = {150.0f, 1.0f, 170.0f, 0.0f};
const float OUT_COOLER_PTS[] = {150.0f, 0.0f, 170.0f, 1.0f, 190.0f, 0.0f};
const float OUT_TARGET_PTS[] = {170.0f, 0.0f, 190.0f, 1.0f, 210.0f, 0.0f};
const float OUT_WARMER_PTS[] = {190.0f, 0.0f, 210.0f, 1.0f, 230.0f, 0.0f};
const float OUT_HOTTEST_PTS[] = {210.0f, 0.0f, 230.0f, 1.0f};

const GenFuzzyTerm OUT_TERMS[] = {
    {"coolest", 2, OUT_COOLEST_PTS},
    {"cooler", 3, OUT_COOLER_PTS},
    {"target", 3, OUT_TARGET_PTS},
    {"warmer", 3, OUT_WARMER_PTS},
    {"hottest", 2, OUT_HOTTEST_PTS},
};

const GenFuzzyInput INPUTS[] = {
    {"color_error", -0.30f, 0.30f, 1, IN_TERMS, 5},
};

const GenFuzzyAnt ANT_VERY_DARK[] = {{0, 0, 0}};
const GenFuzzyAnt ANT_DARK[] = {{0, 1, 0}};
const GenFuzzyAnt ANT_TARGET[] = {{0, 2, 0}};
const GenFuzzyAnt ANT_LIGHT[] = {{0, 3, 0}};
const GenFuzzyAnt ANT_VERY_LIGHT[] = {{0, 4, 0}};

/* Monotonic on purpose: lighter than target -> hotter setpoint, darker ->
   cooler, one rule per term, so the surface rises with the error and cannot
   fold back on itself. A non-monotonic rule base here would be an untestable
   guess. */
const GenFuzzyRule RULES[] = {
    {"C1_very_dark_coolest", ANT_VERY_DARK, 1, 0, 1.0f},
    {"C2_dark_cooler", ANT_DARK, 1, 1, 1.0f},
    {"C3_on_target", ANT_TARGET, 1, 2, 1.0f},
    {"C4_light_warmer", ANT_LIGHT, 1, 3, 1.0f},
    {"C5_very_light_hottest", ANT_VERY_LIGHT, 1, 4, 1.0f},
};

const FuzzyTable COLOR_TABLE = {
    INPUTS, 1, RULES, 5, OUT_TERMS,
    COLOR_SETPOINT_MIN_C, COLOR_SETPOINT_MAX_C, COLOR_SETPOINT_MIN_C};

/* The table and the terms must stay consistent: a rule pointing past the end
   of OUT_TERMS would be an out-of-bounds read on the machine. */
static_assert(sizeof(RULES) / sizeof(RULES[0]) == 5,
              "rule count must match the number of input terms");
static_assert(sizeof(OUT_TERMS) / sizeof(OUT_TERMS[0]) == 5,
              "consequent indices are fixed at 0..4");

}  // namespace

float ColorSetpoint::browningRatio(uint32_t redHz, uint32_t greenHz,
                                   uint32_t blueHz) {
  /* The three channels are integrated one after another, 50 ms apart, so a
     turning drum means they are not the same instant. A ratio is still the
     most robust thing available without calibration; a single channel would
     move with lamp ageing and distance. */
  const float sum = (float)redHz + (float)greenHz + (float)blueHz;
  if (!(sum >= COLOR_MIN_SUM_HZ)) return NAN;      // too dark / no illumination
  if (sum > 3.0f * GEN_COLOR_MAX_HZ) return NAN;   // saturated or a wiring fault
  return (float)redHz / sum;
}

float ColorSetpoint::setpointC(float colorError) {
  if (isnan(colorError) || isinf(colorError)) return NAN;

  /* One controller instance: the rule base is stateless, so reusing it is
     safe and keeps the inference allocation off the stack. */
  static FuzzyController controller(COLOR_TABLE);

  const float values[1] = {colorError};
  const FuzzyController::Result result = controller.evaluate(values);

  /* A rule base with a full-coverage input universe always fires, but never
     turn a "could not compute" into a temperature. */
  if (result.status != FuzzyController::STATUS_OK) return NAN;

  float sp = result.output;
  if (sp < COLOR_SETPOINT_MIN_C) sp = COLOR_SETPOINT_MIN_C;
  if (sp > COLOR_SETPOINT_MAX_C) sp = COLOR_SETPOINT_MAX_C;
  return sp;
}
