/* Belt tensioning logic — pure functions, no hardware access.
 * Kept hardware-free so it can be unit tested on the host PC. */

#ifndef BELT_H
#define BELT_H

#include <stdint.h>

typedef enum {
    MODE_OFF = 0,
    MODE_SOFT,
    MODE_MED,
    MODE_HARD
} belt_mode_t;

/* Linear pull ramp: 0.0 at t=0 down to -1.0 at t=ramp_ms.
 * Clamped to the range [-1.0, 0.0]. */
float belt_ramp(uint32_t elapsed_ms, uint32_t ramp_ms);

/* Comfort tension duty for a given mode (negative = pull). */
float belt_comfort_duty(belt_mode_t mode);

/* Next mode in the button cycle: OFF -> SOFT -> MED -> HARD -> OFF */
belt_mode_t belt_next_mode(belt_mode_t mode);

/* Clamp any duty command into the safe range [-1.0, 1.0]. */
float belt_clamp_duty(float duty);

#endif /* BELT_H */
