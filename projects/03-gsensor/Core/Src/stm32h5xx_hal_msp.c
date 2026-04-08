/* stm32h5xx_hal_msp.c — low-level peripheral init/deinit for 03-gsensor */

#include "main.h"

/* ── HAL_MspInit ─────────────────────────────────────────────────────────── */
/* On STM32H5, SYSCFG and PWR are always clocked — no explicit enable needed. */
void HAL_MspInit(void)
{
}

/* ── SPI1 MSP ────────────────────────────────────────────────────────────── */
/* PA5 = SCK, PA6 = MISO, PA7 = MOSI — all AF5                               */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin       = SPI1_SCK_PIN | SPI1_MISO_PIN | SPI1_MOSI_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(SPI1_GPIO_PORT, &GPIO_InitStruct);
    }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_DISABLE();
        HAL_GPIO_DeInit(SPI1_GPIO_PORT, SPI1_SCK_PIN | SPI1_MISO_PIN | SPI1_MOSI_PIN);
    }
}

/* ── UART7 MSP ───────────────────────────────────────────────────────────── */
/* PE7 = RX, PE8 = TX — AF7.  TX-only blocking transmit; no NVIC needed.    */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == UART7)
    {
        __HAL_RCC_UART7_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        GPIO_InitStruct.Pin       = UART7_RX_PIN | UART7_TX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_UART7;
        HAL_GPIO_Init(UART7_GPIO_PORT, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART7)
    {
        __HAL_RCC_UART7_CLK_DISABLE();
        HAL_GPIO_DeInit(UART7_GPIO_PORT, UART7_RX_PIN | UART7_TX_PIN);
    }
}
