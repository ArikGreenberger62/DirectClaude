/* stm32h5xx_hal_msp.c — low-level peripheral clock + pin setup */
#include "main.h"

void HAL_MspInit(void) { /* SYSCFG/PWR always clocked on STM32H5 */ }

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    if (huart->Instance == UART7)
    {
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_UART7;
        PeriphClkInit.Uart7ClockSelection  = RCC_UART7CLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) { Error_Handler(); }

        __HAL_RCC_UART7_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        /* PE7 → UART7_RX, PE8 → UART7_TX, AF7 */
        GPIO_InitStruct.Pin       = GPIO_PIN_7 | GPIO_PIN_8;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_UART7;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART7)
    {
        __HAL_RCC_UART7_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOE, GPIO_PIN_7 | GPIO_PIN_8);
    }
}
