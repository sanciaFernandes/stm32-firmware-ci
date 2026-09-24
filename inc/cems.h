/* CEMS - motorised seatbelt controller.
 *
 * Five functions, with the interlocks between them enforced by the state
 * machine rather than by convention:
 *
 *   1. BLA  (Belt Latch Assist)     comfort  - unlatched only. When the user
 *                                              pulls the webbing, the motor
 *                                              feeds it out to make pulling
 *                                              effortless. Stops as soon as the
 *                                              user lets go.
 *   2. BSR  (Belt Slack Reduction)  SAFETY   - runs once immediately after
 *                                              latching, for 3 s, exclusively:
 *                                              no other function may run or
 *                                              interrupt it.
 *   3. Comfort (Soft/Medium/Hard)   comfort  - latched only, after BSR.
 *   4. Haptic warning               SAFETY   - latched only. Triggered by a
 *                                              toggle button on this prototype
 *                                              (drowsiness detection from the
 *                                              vehicle ECU in the car): the
 *                                              motor vibrates for 5 s, then
 *                                              returns to the previous comfort
 *                                              mode.
 *   5. Belt Parking                 comfort  - on unlatching, retracts the
 *                                              webbing smoothly instead of
 *                                              letting the spring snap it back.
 */

#ifndef CEMS_H
#define CEMS_H

#include <stdint.h>
#include "belt.h"        /* belt_mode_t: MODE_OFF / SOFT / MED / HARD */

typedef enum {
    CEMS_UNLATCHED = 0,  /* belt released, motor idle                     */
    CEMS_BLA,            /* assisting webbing extraction                  */
    CEMS_BSR,            /* slack reduction: exclusive, one-shot, 3 s      */
    CEMS_COMFORT,        /* holding the selected comfort tension          */
    CEMS_HAPTIC,         /* 5 s warning vibration                         */
    CEMS_PARKING,        /* smooth retraction after unlatching            */
    CEMS_FAULT           /* overcurrent: motor off until the belt is released */
} cems_state_t;

/* --- timings (ms) --- */
#define CEMS_BSR_MS            3000U   /* total slack reduction time      */
#define CEMS_BSR_RAMP_MS        500U   /* ramp up, and ramp down, within it */
#define CEMS_HAPTIC_MS         5000U   /* warning vibration duration      */
#define CEMS_HAPTIC_PULSE_MS    250U   /* half period of the vibration    */
#define CEMS_PARK_MS           1200U   /* smooth retraction time          */

/* --- duties --- */
#define CEMS_BSR_HOLD_DUTY     (-0.99f)
#define CEMS_PARK_DUTY         (-0.10f)
#define CEMS_HAPTIC_DUTY       (-0.30f)

/* --- speed-following assist ---------------------------------------------
 * BLA and the comfort modes share one control law. Each sample the tachometer
 * reports how far the webbing moved, and the motor duty is:
 *
 *     tacho    = tachometer reading this sample, clamped to [1, 30]
 *     duty_pct = initial(mode) % + (factor(mode) % per count * tacho)
 *     duty     = duty_pct / 100          (the motor controller takes 0..1)
 *
 * In Hard mode both terms are negative, so the motor pulls back against the
 * occupant instead of assisting: extraction is deliberately hard.
 *
 * The initial duty is a base amount of help at the slowest pull; the factor
 * adds more as the pull gets faster. Clamping the reading means a very slow
 * pull still gets help (minimum 1) and an unusually fast pull is treated as
 * the fastest expected pull (maximum 30), so the motor never feeds out excess
 * webbing. With no movement at all the motor is off.
 *
 *   BLA / Soft - most assist: smoothest, easiest movement
 *   Medium     - less assist
 *   Hard       - negative: resists extraction, so the occupant does not pitch
 *                forward under braking on rough terrain
 *
 * All values are bench-tuned. */
#define CEMS_TACHO_MIN            1     /* readings below this count as this   */
#define CEMS_TACHO_MAX           30     /* readings above this count as this   */
/* Per-mode initial duty and factor, in percent.
 * Positive duty feeds webbing out (assists extraction);
 * negative duty pulls webbing in (resists extraction).
 *
 *   BLA / Soft  +5% .. +20%   easiest movement
 *   Medium      +5% .. +14%
 *   Hard        -5% ..  -8%   RESISTS: the motor pulls against the occupant,
 *                             so they cannot pitch forward under braking */
#define CEMS_INIT_BLA_PCT      ( 5.0f)
#define CEMS_FACTOR_BLA_PCT    ( 0.5f)
#define CEMS_INIT_SOFT_PCT     ( 5.0f)
#define CEMS_FACTOR_SOFT_PCT   ( 0.5f)
#define CEMS_INIT_MED_PCT      ( 5.0f)
#define CEMS_FACTOR_MED_PCT    ( 0.3f)
#define CEMS_INIT_HARD_PCT     (-5.0f)
#define CEMS_FACTOR_HARD_PCT   (-0.1f)
#define CEMS_DUTY_PCT_SCALE  (100.0f)   /* percent -> 0..1 duty */

/* --- thresholds --- */
#define CEMS_PULL_THRESHOLD       5     /* counts per step counted as a pull */
#define CEMS_MAX_CURRENT_A     12.0f

/* Assist duty (0..1) for a mode and a tachometer reading this sample:
 * (initial % + factor % * clamp(reading, TACHO_MIN, TACHO_MAX)) / 100.
 * Returns 0 when there is no movement. */
float cems_assist_duty(belt_mode_t mode, int32_t tacho_counts);

void         cems_init(void);
void         cems_step(void);                    /* call every ~10 ms */
cems_state_t cems_state(void);

/* Comfort mode selected by the occupant's button. */
void         cems_set_comfort_mode(belt_mode_t mode);
belt_mode_t  cems_comfort_mode(void);

#endif /* CEMS_H */
