#include "cems.h"
#include "belt.h"
#include "platform.h"

static cems_state_t state;
static uint32_t     entry_ms;        /* time the current state was entered   */
static uint32_t     prev_ms;         /* time of the previous step            */
static uint8_t      prev_latch;
static int32_t      prev_tacho;
static uint8_t      bsr_done;        /* BSR has already run for this latch    */
static belt_mode_t  comfort_mode;
static belt_mode_t  mode_before_haptic;

static void enter(cems_state_t next)
{
    state = next;
    entry_ms = platform_now_ms();
}

static uint32_t elapsed(void)
{
    return platform_now_ms() - entry_ms;   /* wrap-safe unsigned subtraction */
}

void cems_init(void)
{
    state = CEMS_UNLATCHED;
    entry_ms = platform_now_ms();
    prev_latch = 0U;
    prev_tacho = platform_tacho();
    prev_ms = platform_now_ms();
    bsr_done = 0U;
    comfort_mode = MODE_SOFT;
    mode_before_haptic = MODE_SOFT;
    platform_set_duty(0.0f);
}

cems_state_t cems_state(void)          { return state; }
belt_mode_t  cems_comfort_mode(void)   { return comfort_mode; }

void cems_set_comfort_mode(belt_mode_t mode)
{
    comfort_mode = mode;
}

float cems_assist_duty(belt_mode_t mode, int32_t tacho_counts)
{
    float factor;
    float initial;
    int32_t tacho = tacho_counts;

    /* No movement, or the spring winding the webbing back in: motor off. */
    if (tacho <= 0)
    {
        return 0.0f;
    }

    /* Clamp the reading into the working range. */
    if (tacho < CEMS_TACHO_MIN)
    {
        tacho = CEMS_TACHO_MIN;
    }
    if (tacho > CEMS_TACHO_MAX)
    {
        tacho = CEMS_TACHO_MAX;
    }

    switch (mode)
    {
        case MODE_SOFT:
            initial = CEMS_INIT_SOFT_PCT;
            factor  = CEMS_FACTOR_SOFT_PCT;
            break;
        case MODE_MED:
            initial = CEMS_INIT_MED_PCT;
            factor  = CEMS_FACTOR_MED_PCT;
            break;
        case MODE_HARD:
            /* negative: the motor resists extraction */
            initial = CEMS_INIT_HARD_PCT;
            factor  = CEMS_FACTOR_HARD_PCT;
            break;
        case MODE_OFF:
        default:
            return 0.0f;
    }

    /* (initial % + factor % * tacho) / 100
     * positive = feed webbing out, negative = pull webbing in */
    return (initial + (factor * (float)tacho)) / CEMS_DUTY_PCT_SCALE;
}

/* Slack reduction profile: ramp up, hold, ramp down, all within CEMS_BSR_MS. */
static float bsr_duty(uint32_t e)
{
    float duty;

    if (e < CEMS_BSR_RAMP_MS)
    {
        duty = belt_ramp(e, CEMS_BSR_RAMP_MS);                 /* 0 -> -1.0 */
    }
    else if (e < (CEMS_BSR_MS - CEMS_BSR_RAMP_MS))
    {
        duty = CEMS_BSR_HOLD_DUTY;                             /* hold      */
    }
    else if (e < CEMS_BSR_MS)
    {
        const uint32_t remaining = CEMS_BSR_MS - e;
        duty = belt_ramp(remaining, CEMS_BSR_RAMP_MS);         /* -1.0 -> 0 */
    }
    else
    {
        duty = 0.0f;
    }

    return duty;
}

/* Square-wave vibration for the haptic warning. */
static float haptic_duty(uint32_t e)
{
    const uint32_t phase = (e / CEMS_HAPTIC_PULSE_MS) % 2U;
    return (phase == 0U) ? CEMS_HAPTIC_DUTY : 0.0f;
}

void cems_step(void)
{
    const uint8_t latch = platform_latch_closed();
    const float   current = platform_motor_current();
    const float   abs_current = (current < 0.0f) ? -current : current;
    const int32_t  tacho = platform_tacho();
    const int32_t  tacho_delta = tacho - prev_tacho;
    const uint32_t now = platform_now_ms();
    const uint8_t motor_active = (uint8_t)((state == CEMS_BLA)     ||
                                           (state == CEMS_BSR)     ||
                                           (state == CEMS_COMFORT) ||
                                           (state == CEMS_HAPTIC)  ||
                                           (state == CEMS_PARKING));

    /* ---------- safety checks, before any function logic ---------- */

    /* Overcurrent: stop and latch a fault. */
    if ((motor_active != 0U) && (abs_current > CEMS_MAX_CURRENT_A))
    {
        platform_set_duty(0.0f);
        enter(CEMS_FAULT);
        prev_latch = latch;
        prev_tacho = tacho;
        prev_ms = now;
        return;
    }

    /* Belt released while the motor is working: stop and park it. */
    if ((motor_active != 0U) && (latch == 0U) && (state != CEMS_PARKING))
    {
        platform_set_duty(0.0f);
        bsr_done = 0U;                 /* next latch must run BSR again */
        enter(CEMS_PARKING);
        prev_latch = latch;
        prev_tacho = tacho;
        prev_ms = now;
        return;
    }

    /* ---------- functions ---------- */
    switch (state)
    {
        case CEMS_UNLATCHED:
            platform_set_duty(0.0f);
            if ((latch != 0U) && (prev_latch == 0U))
            {
                if (bsr_done == 0U)
                {
                    enter(CEMS_BSR);           /* 2. BSR, once per latch */
                }
                else
                {
                    enter(CEMS_COMFORT);
                }
            }
            else if (tacho_delta >= CEMS_PULL_THRESHOLD)
            {
                enter(CEMS_BLA);               /* 1. BLA: user is pulling */
            }
            else
            {
                /* stay idle */
            }
            break;

        case CEMS_BLA:
            /* Assist extraction only while the user keeps pulling. As soon as
             * the pull stops the spring takes over and the tacho stops rising,
             * so the assist is removed. */
            if (tacho_delta >= CEMS_PULL_THRESHOLD)
            {
                /* Same control law as Soft: feed webbing out in proportion to
                 * how fast the occupant is pulling. */
                platform_set_duty(cems_assist_duty(MODE_SOFT, tacho_delta));  /* BLA == Soft */
            }
            else
            {
                platform_set_duty(0.0f);
                enter(CEMS_UNLATCHED);
            }
            if ((latch != 0U) && (prev_latch == 0U))
            {
                platform_set_duty(0.0f);
                enter(CEMS_BSR);
            }
            break;

        case CEMS_BSR:
            /* SAFETY, exclusive: comfort changes and haptic requests are
             * deliberately ignored here; nothing may interrupt slack reduction. */
            platform_set_duty(bsr_duty(elapsed()));
            if (elapsed() >= CEMS_BSR_MS)
            {
                platform_set_duty(0.0f);
                bsr_done = 1U;
                enter(CEMS_COMFORT);
            }
            break;

        case CEMS_COMFORT:
            /* 3. Comfort: the same speed-following law, with the gain set by
             * the selected mode. Soft gives the most webbing for a given pull
             * speed, Hard the least. */
            platform_set_duty(cems_assist_duty(comfort_mode, tacho_delta));
            if (platform_haptic_request() != 0U)
            {
                mode_before_haptic = comfort_mode;
                enter(CEMS_HAPTIC);            /* 4. haptic warning */
            }
            break;

        case CEMS_HAPTIC:
            platform_set_duty(haptic_duty(elapsed()));
            if (elapsed() >= CEMS_HAPTIC_MS)
            {
                comfort_mode = mode_before_haptic;   /* back to previous mode */
                enter(CEMS_COMFORT);
            }
            break;

        case CEMS_PARKING:
            /* 5. smooth retraction instead of letting the spring snap back */
            platform_set_duty(CEMS_PARK_DUTY);
            if ((elapsed() >= CEMS_PARK_MS) || (latch != 0U))
            {
                platform_set_duty(0.0f);
                enter(CEMS_UNLATCHED);
            }
            break;

        case CEMS_FAULT:
        default:
            platform_set_duty(0.0f);
            if (latch == 0U)                  /* cleared by releasing the belt */
            {
                bsr_done = 0U;
                enter(CEMS_UNLATCHED);
            }
            break;
    }

    prev_latch = latch;
    prev_tacho = tacho;
    prev_ms = now;
}
