/* Minimal startup code for STM32F446RE (ARM Cortex-M4).
 * Replaces the CubeIDE-generated startup_stm32f446retx.s.
 *
 * What it does, in order:
 *   1. Defines the vector table (initial stack pointer + handler addresses)
 *   2. Reset_Handler: copies .data from flash to RAM, zeroes .bss
 *   3. Calls main()
 */

#include <stdint.h>

/* Symbols provided by the linker script */
extern uint32_t _estack;    /* top of stack            */
extern uint32_t _sidata;    /* .data source, in flash  */
extern uint32_t _sdata;     /* .data start, in RAM     */
extern uint32_t _edata;     /* .data end, in RAM       */
extern uint32_t _sbss;      /* .bss start              */
extern uint32_t _ebss;      /* .bss end                */

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Every unused exception points at Default_Handler (an infinite loop) */
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* The vector table: the FIRST thing in flash at 0x08000000.
 * Entry 0 = initial stack pointer, entry 1 = reset handler. */
__attribute__((section(".isr_vector"), used))
void (* const vector_table[])(void) = {
    (void (*)(void))(&_estack),   /* 0: initial stack pointer */
    Reset_Handler,                /* 1: reset                 */
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,                   /* reserved */
    SVC_Handler,
    DebugMon_Handler,
    0,                            /* reserved */
    PendSV_Handler,
    SysTick_Handler
};

void Reset_Handler(void)
{
    uint32_t *src;
    uint32_t *dst;

    /* 1. Copy initialised globals (.data) from flash into RAM */
    src = &_sidata;
    for (dst = &_sdata; dst < &_edata; dst++)
    {
        *dst = *src;
        src++;
    }

    /* 2. Zero the uninitialised globals (.bss) */
    for (dst = &_sbss; dst < &_ebss; dst++)
    {
        *dst = 0U;
    }

    /* 3. Hand over to the application */
    (void)main();

    /* main() must never return; if it does, trap here */
    for (;;)
    {
    }
}

void Default_Handler(void)
{
    for (;;)
    {
    }
}
