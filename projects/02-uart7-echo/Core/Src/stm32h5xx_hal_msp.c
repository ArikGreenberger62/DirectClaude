/* stm32h5xx_hal_msp.c — low-level peripheral init/deinit for 02-uart7-echo */

#include "main.h"

/* ── HAL_MspInit ─────────────────────────────────────────────────────────── */
void HAL_MspInit(void)
{
    /* On STM32H5, SYSCFG and PWR are always clocked — no explicit enable needed. */
}

/* ── UART7 MSP ───────────────────────────────────────────────────────────── */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == UART7)
    {
        __HAL_RCC_UART7_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        /* PE7 → UART7_RX, PE8 → UART7_TX   AF7 */
        GPIO_InitStruct.Pin       = UART7_RX_PIN | UART7_TX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_UART7;
        HAL_GPIO_Init(UART7_GPIO_PORT, &GPIO_InitStruct);

        /* UART7 interrupt: priority 5 (below SysTick at 15) */
        HAL_NVIC_SetPriority(UART7_IRQn, 5U, 0U);
        HAL_NVIC_EnableIRQ(UART7_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART7)
    {
        __HAL_RCC_UART7_CLK_DISABLE();
        HAL_GPIO_DeInit(UART7_GPIO_PORT, UART7_RX_PIN | UART7_TX_PIN);
        HAL_NVIC_DisableIRQ(UART7_IRQn);
    }
}
