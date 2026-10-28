#include "ColorSetpoint.h"

#include <math.h>

namespace {

/* --- colour error universe, in ratio units ----------------------------- */
/* Negative = darker than the target (too little heat), positive = lighter. */
const float IN_DARK_PTS[] = {-0.30f, 1.0f, -0.04f, 0.0f};
const float IN_TARGET_PTS[] = {-0.06f, 0.0f, 0.0f, 1.0f, 0.06f, 0.0f};
const float IN_LIGHT_PTS[] = {0.04f, 0.0f, 0.30f, 1.0f};

const GenFuzzyTerm IN_TERMS[] = {
    {"darker", 2, IN_DARK_PTS},
    {"on_target", 3, IN_TARGET_PTS},
    {"lighter", 2, IN_LIGHT_PTS},
};

/* --- setpoint universe, °C --------------------------------------------- */
/* Same piecewise-linear shoulders as the generated table: an outer point of 1
   stays 1 beyond the universe edge. */
const float OUT_LOW_PTS[] = {150.0f, 1.0f, 170.0f, 0.0f};
const float OUT_MID_PTS[] = {165.0f, 0.0f, 190.0f, 1.0f, 215.0f, 0.0f};
const float OUT_HIGH_PTS[] = {200.0f, 0.0f, 230.0f, 1.0f};

const GenFuzzyTerm OUT_TERMS[] = {
    {"low", 2, OUT_LOW_PTS},
    {"mid", 3, OUT_MID_PTS},
    {"high", 2, OUT_HIGH_PTS},
};

const GenFuzzyInput INPUTS[] = {
    {"color_error", -0.30f, 0.30f, 1, IN_TERMS, 3},
};

const GenFuzzyAnt ANT_DARK[] = {{0, 0, 0}};
const GenFuzzyAnt ANT_TARGET[] = {{0, 1, 0}};
const GenFuzzyAnt ANT_LIGHT[] = {{0, 2, 0}};

/* Monotonic on purpose: lighter than target -> hotter setpoint, darker ->
   cooler. A non-monotonic rule base here would be an untestable guess. */
const GenFuzzyRule RULES[] = {
    {"C1_darker_cooler", ANT_DARK, 1, 0, 1.0f},
    {"C2_on_target", ANT_TARGET, 1, 1, 1.0f},
    {"C3_lighter_hotter", ANT_LIGHT, 1, 2, 1.0f},
};

const FuzzyTable COLOR_TABLE = {
    INPUTS, 1, RULES, 3, OUT_TERMS,
    COLOR_SETPOINT_MIN_C, COLOR_SETPOINT_MAX_C, COLOR_SETPOINT_MIN_C};

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
