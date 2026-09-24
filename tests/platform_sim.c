/* Simulated hardware for host-side scenario tests.
 *
 * The test drives time, the buckle switch and the motor current, and records
 * every duty command the state machine issues. No board required. */

#include "platform.h"
#include "sim.h"

static uint32_t sim_ms;
static uint8_t  sim_latch;
static float    sim_current;
static int32_t  sim_tacho;
static uint8_t  sim_haptic_btn;
static float    sim_duty;
static float    sim_min_duty;          /* strongest pull seen (most negative) */
static float    sim_max_duty;          /* strongest feed-out seen (most positive) */
static uint32_t sim_duty_writes;

/* ---- platform interface, as seen by the firmware logic ---- */

void platform_init(void)
{
    /* nothing to initialise in the simulator */
}

uint32_t platform_now_ms(void)
{
    return sim_ms;
}

uint8_t platform_latch_closed(void)
{
    return sim_latch;
}

float platform_motor_current(void)
{
    return sim_current;
}

int32_t platform_tacho(void)
{
    return sim_tacho;
}

uint8_t platform_haptic_request(void)
{
    return sim_haptic_btn;
}

void platform_set_duty(float duty)
{
    sim_duty = duty;
    sim_duty_writes++;
    if (duty < sim_min_duty)
    {
        sim_min_duty = duty;
    }
    if (duty > sim_max_duty)
    {
        sim_max_duty = duty;
    }
}

/* ---- controls used by the test ---- */

void sim_reset(void)
{
    sim_ms = 0U;
    sim_latch = 0U;
    sim_current = 0.0f;
    sim_tacho = 0;
    sim_haptic_btn = 0U;
    sim_duty = 0.0f;
    sim_min_duty = 0.0f;
    sim_max_duty = 0.0f;
    sim_duty_writes = 0U;
}

void     sim_advance_ms(uint32_t ms)   { sim_ms += ms; }
void     sim_set_latch(uint8_t closed) { sim_latch = closed; }
void     sim_set_current(float amps)   { sim_current = amps; }
void     sim_pull_webbing(int32_t counts) { sim_tacho += counts; }
void     sim_press_haptic_button(uint8_t pressed) { sim_haptic_btn = pressed; }
float    sim_duty_now(void)            { return sim_duty; }
float    sim_min_duty_seen(void)       { return sim_min_duty; }
float    sim_max_duty_seen(void)       { return sim_max_duty; }
uint32_t sim_duty_write_count(void)    { return sim_duty_writes; }
