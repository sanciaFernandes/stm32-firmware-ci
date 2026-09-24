/* Bare-metal STM32F446RE application.
 *
 * Runs the belt tensioning state machine from a simple 10 ms super loop and
 * blinks the Nucleo user LED (PA5) as a heartbeat. */

#include <stdint.h>
#include "cems.h"
#include "platform.h"

/* --- Minimal register definitions (no HAL, no CMSIS) --- */
#define RCC_AHB1ENR     (*(volatile uint32_t *)0x40023830UL)
#define GPIOA_MODER     (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_ODR       (*(volatile uint32_t *)0x40020014UL)

#define GPIOAEN         (1UL << 0)
#define LED_PIN         5U              /* PA5 = user LED LD2 */
#define STEP_MS         10U             /* state machine period */
#define HEARTBEAT_MS    500U

/* Firmware identity, injected by the Makefile at compile time */
#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif
#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

const char fw_version[] = FW_VERSION;
const char git_hash[]   = GIT_HASH;

int main(void)
{
    uint32_t last_step = 0U;
    uint32_t last_blink = 0U;

    platform_init();

    /* Heartbeat LED on PA5 */
    RCC_AHB1ENR |= GPIOAEN;
    GPIOA_MODER &= ~(3UL << (LED_PIN * 2U));
    GPIOA_MODER |=  (1UL << (LED_PIN * 2U));

    cems_init();

    for (;;)
    {
        const uint32_t now = platform_now_ms();

        /* Run the belt logic every 10 ms — never blocking */
        if ((now - last_step) >= STEP_MS)
        {
            last_step = now;
            cems_step();
        }

        /* Heartbeat: if this stops blinking, the loop is stuck */
        if ((now - last_blink) >= HEARTBEAT_MS)
        {
            last_blink = now;
            GPIOA_ODR ^= (1UL << LED_PIN);
        }
    }
}
