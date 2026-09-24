/* Bare-metal STM32F446RE application: blinks the Nucleo user LED (PA5)
 * and exercises the belt tensioning logic. */

#include <stdint.h>
#include "belt.h"

/* --- Minimal register definitions (no HAL, no CMSIS) --- */
#define RCC_BASE        0x40023800UL
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30UL))

#define GPIOA_BASE      0x40020000UL
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14UL))

#define GPIOAEN         (1UL << 0)      /* RCC_AHB1ENR bit 0 */
#define LED_PIN         5U              /* PA5 = user LED LD2 */

/* Firmware identity, injected by the Makefile at compile time */
#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif
#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

const char fw_version[] = FW_VERSION;
const char git_hash[]   = GIT_HASH;

static void delay(volatile uint32_t count)
{
    while (count-- > 0U)
    {
    }
}

int main(void)
{
    /* Enable the GPIOA clock — without this, writes to GPIOA do nothing */
    RCC_AHB1ENR |= GPIOAEN;

    /* PA5 as output: MODER bits [11:10] = 01 */
    GPIOA_MODER &= ~(3UL << (LED_PIN * 2U));
    GPIOA_MODER |=  (1UL << (LED_PIN * 2U));

    for (;;)
    {
        GPIOA_ODR ^= (1UL << LED_PIN);     /* toggle the LED */
        delay(1000000U);
    }
}
