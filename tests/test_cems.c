/* Scenario tests for the five CEMS functions and the interlocks between them.
 *
 * Each test scripts the simulated hardware (clock, buckle switch, tachometer,
 * motor current, haptic button) and runs the state machine exactly as
 * the firmware main loop does. This is a bench test, in software. */

#include "unity.h"
#include "cems.h"
#include "belt.h"
#include "platform.h"
#include "sim.h"

#define STEP_MS 10U

void setUp(void)
{
    sim_reset();
    cems_init();
}

void tearDown(void) { }

static void run_for(uint32_t ms)
{
    uint32_t t;
    for (t = 0U; t < ms; t += STEP_MS)
    {
        sim_advance_ms(STEP_MS);
        cems_step();
    }
}

/* Simulate the occupant pulling the webbing out at a given speed.
 * counts_per_step is the tachometer reading the firmware sees each sample. */
static void pull_webbing_at(uint32_t ms, int32_t counts_per_step)
{
    uint32_t t;
    for (t = 0U; t < ms; t += STEP_MS)
    {
        sim_advance_ms(STEP_MS);
        sim_pull_webbing(counts_per_step);
        cems_step();
    }
}

static void pull_webbing_for(uint32_t ms)
{
    pull_webbing_at(ms, 20);
}

/* ---------- 1. Belt Latch Assist ---------- */

static void test_bla_assists_while_the_user_pulls(void)
{
    pull_webbing_for(100U);
    TEST_ASSERT_EQUAL(CEMS_BLA, cems_state());
    TEST_ASSERT_TRUE(sim_duty_now() > 0.0f);          /* feeding webbing out */
}

static void test_bla_stops_when_the_user_lets_go(void)
{
    pull_webbing_for(100U);
    TEST_ASSERT_EQUAL(CEMS_BLA, cems_state());

    run_for(3U * STEP_MS);                            /* tacho stops rising */
    TEST_ASSERT_EQUAL(CEMS_UNLATCHED, cems_state());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sim_duty_now());
}

static void test_bla_never_runs_while_latched(void)
{
    sim_set_latch(1U);
    run_for(CEMS_BSR_MS + 200U);                      /* through BSR         */
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());

    pull_webbing_for(200U);                           /* tacho moves anyway  */
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());    /* still not BLA       */
}

/* ---------- 2. Belt Slack Reduction ---------- */

static void test_bsr_runs_for_three_seconds_after_latching(void)
{
    sim_set_latch(1U);
    run_for(100U);
    TEST_ASSERT_EQUAL(CEMS_BSR, cems_state());

    run_for(1000U);                                   /* ~1.1 s: full pull   */
    TEST_ASSERT_EQUAL(CEMS_BSR, cems_state());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, CEMS_BSR_HOLD_DUTY, sim_duty_now());

    run_for(2200U);                                   /* past 3 s            */
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());
}

static void test_bsr_runs_only_once_per_latch(void)
{
    sim_set_latch(1U);
    run_for(CEMS_BSR_MS + 200U);
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());

    /* a glitch on the latch signal must not restart slack reduction */
    sim_set_latch(1U);
    run_for(500U);
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());
}

static void test_bsr_runs_again_after_unlatch_and_relatch(void)
{
    sim_set_latch(1U);
    run_for(CEMS_BSR_MS + 200U);
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());

    sim_set_latch(0U);                                /* unbuckle            */
    run_for(CEMS_PARK_MS + 200U);                     /* parking completes   */
    TEST_ASSERT_EQUAL(CEMS_UNLATCHED, cems_state());

    sim_set_latch(1U);                                /* buckle again        */
    run_for(100U);
    TEST_ASSERT_EQUAL(CEMS_BSR, cems_state());        /* BSR runs again      */
}

/* ---------- interlock: BSR is exclusive ---------- */

static void test_haptic_button_is_ignored_during_bsr(void)
{
    sim_set_latch(1U);
    run_for(100U);
    TEST_ASSERT_EQUAL(CEMS_BSR, cems_state());

    sim_press_haptic_button(1U);                  /* button pressed mid-BSR */
    run_for(500U);
    TEST_ASSERT_EQUAL(CEMS_BSR, cems_state());        /* BSR is not interrupted */

    run_for(CEMS_BSR_MS);                             /* BSR finishes        */
    TEST_ASSERT_EQUAL(CEMS_HAPTIC, cems_state());     /* then the warning runs */
}

/* ---------- 3. Comfort modes: speed-following assist ---------- */

static void reach_comfort(void)
{
    sim_set_latch(1U);
    run_for(CEMS_BSR_MS + 200U);
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());
}

static void test_motor_is_off_when_the_occupant_is_still(void)
{
    reach_comfort();
    run_for(500U);                               /* no tacho movement */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sim_duty_now());
}

static void test_faster_pull_gives_more_assist(void)
{
    float slow_duty;
    float fast_duty;

    reach_comfort();
    cems_set_comfort_mode(MODE_SOFT);

    pull_webbing_at(100U, 10);                   /* moderate pull */
    slow_duty = sim_duty_now();

    pull_webbing_at(100U, 25);                   /* faster pull   */
    fast_duty = sim_duty_now();

    TEST_ASSERT_TRUE(slow_duty > 0.0f);
    TEST_ASSERT_TRUE(fast_duty > slow_duty);     /* pull faster, get more */
}

static void test_soft_assists_medium_less_and_hard_resists(void)
{
    float soft_duty;
    float med_duty;
    float hard_duty;

    reach_comfort();

    cems_set_comfort_mode(MODE_SOFT);
    pull_webbing_at(100U, 20);
    soft_duty = sim_duty_now();

    cems_set_comfort_mode(MODE_MED);
    pull_webbing_at(100U, 20);
    med_duty = sim_duty_now();

    cems_set_comfort_mode(MODE_HARD);
    pull_webbing_at(100U, 20);
    hard_duty = sim_duty_now();

    /* Same pull speed, decreasing assist. Soft and Medium feed webbing out;
     * Hard is NEGATIVE, i.e. the motor pulls back against the occupant so they
     * cannot pitch forward under braking on rough terrain. */
    TEST_ASSERT_TRUE(soft_duty > med_duty);
    TEST_ASSERT_TRUE(med_duty  > hard_duty);
    TEST_ASSERT_TRUE(soft_duty > 0.0f);
    TEST_ASSERT_TRUE(med_duty  > 0.0f);
    TEST_ASSERT_TRUE(hard_duty < 0.0f);          /* resists extraction */
}

static void test_tacho_reading_is_clamped_at_both_ends(void)
{
    float at_max;
    float way_past_max;
    float at_min;

    reach_comfort();
    cems_set_comfort_mode(MODE_SOFT);

    pull_webbing_at(100U, CEMS_TACHO_MAX);       /* fastest expected pull */
    at_max = sim_duty_now();

    pull_webbing_at(100U, 20 * CEMS_TACHO_MAX);  /* absurdly fast pull    */
    way_past_max = sim_duty_now();

    /* beyond the maximum the motor behaves as if pulled at the maximum,
     * so it never feeds out excess webbing */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, at_max, way_past_max);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,
        (CEMS_INIT_SOFT_PCT + (CEMS_FACTOR_SOFT_PCT * (float)CEMS_TACHO_MAX))
            / CEMS_DUTY_PCT_SCALE, at_max);

    /* the slowest pull still gets the initial duty plus the minimum reading */
    pull_webbing_at(100U, CEMS_TACHO_MIN);
    at_min = sim_duty_now();
    TEST_ASSERT_FLOAT_WITHIN(0.001f,
        (CEMS_INIT_SOFT_PCT + (CEMS_FACTOR_SOFT_PCT * (float)CEMS_TACHO_MIN))
            / CEMS_DUTY_PCT_SCALE, at_min);
    TEST_ASSERT_TRUE(at_min > 0.0f);
}

/* ---------- 4. Haptic warning ---------- */

static void test_haptic_runs_five_seconds_then_returns_to_previous_mode(void)
{
    sim_set_latch(1U);
    run_for(CEMS_BSR_MS + 200U);
    cems_set_comfort_mode(MODE_MED);
    run_for(STEP_MS);

    sim_press_haptic_button(1U);
    run_for(STEP_MS);
    TEST_ASSERT_EQUAL(CEMS_HAPTIC, cems_state());

    run_for(2000U);
    TEST_ASSERT_EQUAL(CEMS_HAPTIC, cems_state());     /* still warning       */

    sim_press_haptic_button(0U);
    run_for(3500U);                                   /* past 5 s total      */
    TEST_ASSERT_EQUAL(CEMS_COMFORT, cems_state());
    TEST_ASSERT_EQUAL(MODE_MED, cems_comfort_mode()); /* previous mode back  */
}

static void test_haptic_never_runs_while_unlatched(void)
{
    sim_press_haptic_button(1U);
    run_for(2000U);
    TEST_ASSERT_EQUAL(CEMS_UNLATCHED, cems_state());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sim_duty_now());
}

/* ---------- 5. Belt parking ---------- */

static void test_parking_runs_on_unlatch_then_returns_to_idle(void)
{
    sim_set_latch(1U);
    run_for(CEMS_BSR_MS + 200U);

    sim_set_latch(0U);
    run_for(2U * STEP_MS);
    TEST_ASSERT_EQUAL(CEMS_PARKING, cems_state());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, CEMS_PARK_DUTY, sim_duty_now());

    run_for(CEMS_PARK_MS + 200U);
    TEST_ASSERT_EQUAL(CEMS_UNLATCHED, cems_state());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sim_duty_now());
}

/* ---------- safety ---------- */

static void test_overcurrent_latches_a_fault(void)
{
    sim_set_latch(1U);
    run_for(1000U);                                   /* during BSR          */
    sim_set_current(13.0f);
    run_for(STEP_MS);

    TEST_ASSERT_EQUAL(CEMS_FAULT, cems_state());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sim_duty_now());

    sim_set_current(0.0f);                            /* current recovers    */
    run_for(1000U);
    TEST_ASSERT_EQUAL(CEMS_FAULT, cems_state());      /* still latched       */
}

static void test_motor_command_never_exceeds_limits(void)
{
    sim_set_latch(1U);
    run_for(CEMS_BSR_MS + 500U);
    sim_press_haptic_button(1U);
    run_for(CEMS_HAPTIC_MS + 500U);
    sim_set_latch(0U);
    run_for(CEMS_PARK_MS + 500U);

    TEST_ASSERT_TRUE(sim_min_duty_seen() >= -1.0f);
    TEST_ASSERT_TRUE(sim_max_duty_seen() <=  1.0f);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bla_assists_while_the_user_pulls);
    RUN_TEST(test_bla_stops_when_the_user_lets_go);
    RUN_TEST(test_bla_never_runs_while_latched);
    RUN_TEST(test_bsr_runs_for_three_seconds_after_latching);
    RUN_TEST(test_bsr_runs_only_once_per_latch);
    RUN_TEST(test_bsr_runs_again_after_unlatch_and_relatch);
    RUN_TEST(test_haptic_button_is_ignored_during_bsr);
    RUN_TEST(test_motor_is_off_when_the_occupant_is_still);
    RUN_TEST(test_faster_pull_gives_more_assist);
    RUN_TEST(test_soft_assists_medium_less_and_hard_resists);
    RUN_TEST(test_tacho_reading_is_clamped_at_both_ends);
    RUN_TEST(test_haptic_runs_five_seconds_then_returns_to_previous_mode);
    RUN_TEST(test_haptic_never_runs_while_unlatched);
    RUN_TEST(test_parking_runs_on_unlatch_then_returns_to_idle);
    RUN_TEST(test_overcurrent_latches_a_fault);
    RUN_TEST(test_motor_command_never_exceeds_limits);
    return UNITY_END();
}
