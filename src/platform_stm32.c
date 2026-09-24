/* Real hardware implementation of the platform seam (STM32F446RE).
 *
 * SysTick provides the millisecond timebase, PB0 reads the buckle switch,
 * and the motor duty is sent to the motor controller (stubbed here). */

#include "platform.h"

/* --- Cortex-M SysTick (part of the ARM core, not an ST peripheral) --- */
#define SYST_CSR    (*(volatile uint32_t *)0xE000E010UL)  /* control/status */
#define SYST_RVR    (*(volatile uint32_t *)0xE000E014UL)  /* reload value   */
#define SYST_CVR    (*(volatile uint32_t *)0xE000E018UL)  /* current value  */

#define SYST_ENABLE     (1UL << 0)
#define SYST_TICKINT    (1UL << 1)
#define SYST_CLKSOURCE  (1UL << 2)

/* --- ST peripherals --- */
#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define GPIOB_MODER (*(volatile uint32_t *)0x40020400UL)
#define GPIOB_PUPDR (*(volatile uint32_t *)0x4002040CUL)
#define GPIOB_IDR   (*(volatile uint32_t *)0x40020410UL)

#define GPIOBEN     (1UL << 1)
#define LATCH_PIN   0U              /* PB0, pull-up: HIGH = latched */

/* 16 MHz HSI / 16000 = 1 kHz -> one interrupt per millisecond */
#define SYSTICK_RELOAD  (16000UL - 1UL)

/* Written by the interrupt, read by the main loop: must be volatile. */
static volatile uint32_t tick_ms;

void SysTick_Handler(void)          /* overrides the weak default handler */
{
    tick_ms++;
}

void platform_init(void)
{
    /* Buckle switch: PB0 input with pull-up */
    RCC_AHB1ENR |= GPIOBEN;
    GPIOB_MODER &= ~(3UL << (LATCH_PIN * 2U));          /* 00 = input      */
    GPIOB_PUPDR &= ~(3UL << (LATCH_PIN * 2U));
    GPIOB_PUPDR |=  (1UL << (LATCH_PIN * 2U));          /* 01 = pull-up    */

    /* 1 ms timebase */
    SYST_RVR = SYSTICK_RELOAD;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CLKSOURCE | SYST_TICKINT | SYST_ENABLE;
}

uint32_t platform_now_ms(void)
{
    return tick_ms;
}

uint8_t platform_latch_closed(void)
{
    return (uint8_t)((GPIOB_IDR >> LATCH_PIN) & 1UL);
}

float platform_motor_current(void)
{
    /* Real system: request telemetry from the motor controller over UART.
     * Not implemented on this board bring-up, so report no load. */
    return 0.0f;
}

int32_t platform_tacho(void)
{
    /* Real system: tachometer count from the motor controller's telemetry.
     * Stubbed during board bring-up. */
    return 0;
}

uint8_t platform_haptic_request(void)
{
    /* Prototype: toggle button on the bench.
     * Vehicle: drowsiness detection signal from the ECU over LIN/CAN.
     * Stubbed during board bring-up. */
    return 0U;
}

void platform_set_duty(float duty)
{
    /* Real system: send a duty command to the motor controller over UART.
     * Kept as a stub so the firmware links and runs without one attached. */
    (void)duty;
}
