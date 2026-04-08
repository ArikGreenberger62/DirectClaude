/* stm32h5xx_it.c — interrupt handlers for 02-uart7-echo */

#include "main.h"

/* ── SysTick_Handler ─────────────────────────────────────────────────────── */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ── UART7_IRQHandler ────────────────────────────────────────────────────── */
void UART7_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart7);
}
