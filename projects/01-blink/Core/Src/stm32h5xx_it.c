/* stm32h5xx_it.c — interrupt handlers for 01-blink */

#include "main.h"

/* ── SysTick_Handler ─────────────────────────────────────────────────────── */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
