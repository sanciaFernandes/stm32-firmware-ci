/* Host-side unit tests for the belt logic (Unity framework).
 * These run on the PC — no board required. */

#include "unity.h"
#include "belt.h"

void setUp(void)    { }
void tearDown(void) { }

/* --- belt_ramp --------------------------------------------------------- */

static void test_ramp_starts_at_zero(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, belt_ramp(0U, 1000U));
}

static void test_ramp_midpoint_is_half(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, belt_ramp(500U, 1000U));
}

static void test_ramp_ends_at_full_pull(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, belt_ramp(1000U, 1000U));
}

static void test_ramp_clamps_beyond_full(void)
{
    /* elapsed > ramp time must not command more than -1.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, belt_ramp(5000U, 1000U));
}

static void test_ramp_zero_ramp_time(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, belt_ramp(10U, 0U));
}

/* --- belt_comfort_duty ------------------------------------------------- */

static void test_comfort_levels(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  0.00f, belt_comfort_duty(MODE_OFF));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.03f, belt_comfort_duty(MODE_SOFT));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.08f, belt_comfort_duty(MODE_MED));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.15f, belt_comfort_duty(MODE_HARD));
}

static void test_comfort_invalid_mode_is_safe(void)
{
    /* an out-of-range mode must fall back to "no pull" */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.00f, belt_comfort_duty((belt_mode_t)99));
}

/* --- belt_next_mode ---------------------------------------------------- */

static void test_mode_cycle_wraps(void)
{
    TEST_ASSERT_EQUAL(MODE_SOFT, belt_next_mode(MODE_OFF));
    TEST_ASSERT_EQUAL(MODE_MED,  belt_next_mode(MODE_SOFT));
    TEST_ASSERT_EQUAL(MODE_HARD, belt_next_mode(MODE_MED));
    TEST_ASSERT_EQUAL(MODE_OFF,  belt_next_mode(MODE_HARD));
}

/* --- belt_clamp_duty --------------------------------------------------- */

static void test_clamp_limits(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, belt_clamp_duty(-2.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  1.0f, belt_clamp_duty( 3.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.4f, belt_clamp_duty(-0.4f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ramp_starts_at_zero);
    RUN_TEST(test_ramp_midpoint_is_half);
    RUN_TEST(test_ramp_ends_at_full_pull);
    RUN_TEST(test_ramp_clamps_beyond_full);
    RUN_TEST(test_ramp_zero_ramp_time);
    RUN_TEST(test_comfort_levels);
    RUN_TEST(test_comfort_invalid_mode_is_safe);
    RUN_TEST(test_mode_cycle_wraps);
    RUN_TEST(test_clamp_limits);
    return UNITY_END();
}
