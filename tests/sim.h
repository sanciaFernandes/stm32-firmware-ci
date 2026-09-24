/* Controls exposed by the simulated platform, used only by tests. */

#ifndef SIM_H
#define SIM_H

#include <stdint.h>

void     sim_reset(void);
void     sim_advance_ms(uint32_t ms);
void     sim_set_latch(uint8_t closed);
void     sim_set_current(float amps);
void     sim_pull_webbing(int32_t counts);   /* + = user pulling webbing out */
void     sim_press_haptic_button(uint8_t pressed);  /* bench toggle button */
float    sim_duty_now(void);
float    sim_min_duty_seen(void);
float    sim_max_duty_seen(void);
uint32_t sim_duty_write_count(void);

#endif /* SIM_H */
