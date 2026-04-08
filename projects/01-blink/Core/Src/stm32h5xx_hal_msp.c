/* stm32h5xx_hal_msp.c — low-level peripheral init/deinit for 01-blink */

#include "main.h"

/* ── HAL_MspInit ─────────────────────────────────────────────────────────── */
/* On STM32H5, SYSCFG and PWR are always clocked — no explicit enable needed. */
void HAL_MspInit(void)
{
}

/* ── TIM3 MSP ────────────────────────────────────────────────────────────── */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (htim->Instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /* PC8 → TIM3_CH3 (LED_R), PC9 → TIM3_CH4 (LED_G)
         * Alternate Function AF2 = TIM3 on GPIOC */
        GPIO_InitStruct.Pin       = LED_R_PIN | LED_G_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    }
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, LED_R_PIN | LED_G_PIN);
    }
}

/* ── USART1 MSP ──────────────────────────────────────────────────────────── */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9 → USART1_TX, PA10 → USART1_RX  AF7 */
        GPIO_InitStruct.Pin       = USART1_TX_PIN | USART1_RX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, USART1_TX_PIN | USART1_RX_PIN);
    }
}
