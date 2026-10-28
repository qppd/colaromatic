/* FORMERLY GENERATED — NOW HAND-MAINTAINED.
 * Originally written by src/tools/gen_firmware_tables.py from src/config/*.json,
 * both of which were removed with the Python host, so these values are now
 * edited directly here and must be reviewed like any other code (docs/01 REQ-SYS-002). */

#pragma once

#include <stdint.h>

#include <math.h>

/* source: src/config/profiles/*.json (index order = alphabetical id) */

struct GenTrajPoint { float elapsedS; float targetC; };
struct GenProfile {
  const char *id;
  const char *label;
  uint8_t roastLevel;      /* 0 light, 1 medium, 2 dark */
  uint8_t status;          /* 0 provisional, 1 draft, 2 under_calibration,
                              3 validated, 4 deprecated */
  const GenTrajPoint *traj; /* NULL when no measured trajectory */
  uint8_t trajCount;
  float chargeC, turnPointC, firstCrackC, secondCrackC;
  uint8_t endpointMethod;  /* 0 operator, 1 color, 2 temperature, 3 time */
  float endpointTargetC;
  float endpointMaxDurationS;
  float endpointDeltaETol;
  uint8_t operatorConfirmRequired;
  float maxDutyPct;        /* NAN: no profile-specific cap */
  float setpointMaxC;      /* NAN: no profile-specific cap */
  uint8_t autoEligible;    /* validated AND has a measured trajectory */
};


#define GEN_PROFILE_COUNT (12)
static const GenProfile GEN_PROFILES[GEN_PROFILE_COUNT] = {
  {"arabica-dark", "Arabica / Dark", 2, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"arabica-light", "Arabica / Light", 0, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"arabica-medium", "Arabica / Medium", 1, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"excelsa-dark", "Excelsa / Dark", 2, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"excelsa-light", "Excelsa / Light", 0, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"excelsa-medium", "Excelsa / Medium", 1, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"liberica-dark", "Liberica / Dark", 2, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"liberica-light", "Liberica / Light", 0, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"liberica-medium", "Liberica / Medium", 1, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"robusta-dark", "Robusta / Dark", 2, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"robusta-light", "Robusta / Light", 0, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
  {"robusta-medium", "Robusta / Medium", 1, 0, NULL, 0, NAN, NAN, NAN, NAN, 0, NAN, NAN, NAN, 1, NAN, NAN, 0},
};
