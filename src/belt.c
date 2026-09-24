#include "belt.h"

float belt_ramp(uint32_t elapsed_ms, uint32_t ramp_ms)
{
    float duty;

    if (ramp_ms == 0U)
    {
        return -1.0f;            /* degenerate case: full pull */
    }

    duty = -((float)elapsed_ms / (float)ramp_ms);

    return belt_clamp_duty(duty);
}

float belt_comfort_duty(belt_mode_t mode)
{
    float duty;

    switch (mode)
    {
        case MODE_SOFT: duty = -0.03f; break;
        case MODE_MED:  duty = -0.08f; break;
        case MODE_HARD: duty = -0.15f; break;
        case MODE_OFF:
        default:        duty =  0.00f; break;
    }

    return duty;
}

belt_mode_t belt_next_mode(belt_mode_t mode)
{
    belt_mode_t next;

    switch (mode)
    {
        case MODE_OFF:  next = MODE_SOFT; break;
        case MODE_SOFT: next = MODE_MED;  break;
        case MODE_MED:  next = MODE_HARD; break;
        case MODE_HARD:
        default:        next = MODE_OFF;  break;
    }

    return next;
}

float belt_clamp_duty(float duty)
{
    if (duty < -1.0f)
    {
        duty = -1.0f;
    }
    else if (duty > 1.0f)
    {
        duty = 1.0f;
    }
    else
    {
        /* already in range */
    }

    return duty;
}
