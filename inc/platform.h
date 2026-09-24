/* Platform abstraction: the seam between application logic and hardware.
 *
 *   src/platform_stm32.c  - real registers (SysTick, GPIO), built for the target
 *   tests/platform_sim.c  - scripted fake, built for the host test runner
 *
 * The CEMS state machine only talks to these functions, so the complete
 * five-function behaviour can be exercised on a PC with no board attached. */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

void platform_init(void);

/* Milliseconds since power-on (free running, wraps after ~49 days). */
uint32_t platform_now_ms(void);

/* Motor command. Negative duty = retract (pull webbing in),
 * positive duty = extract (feed webbing out). Range [-1.0, +1.0]. */
void platform_set_duty(float duty);

/* Motor current in amps, from the motor controller's telemetry. */
float platform_motor_current(void);

/* Buckle switch: 1 = latched, 0 = unlatched. */
uint8_t platform_latch_closed(void);

/* Cumulative tachometer count from the motor controller.
 * Increases while the webbing is being pulled out (extraction),
 * decreases while the spring winds it back in. */
int32_t platform_tacho(void);

/* Haptic warning request: 1 = run the warning vibration.
 * On this prototype it is a toggle button on the bench; in the vehicle the
 * same input comes from the ECU's drowsiness detection over LIN/CAN. */
uint8_t platform_haptic_request(void);

#endif /* PLATFORM_H */
