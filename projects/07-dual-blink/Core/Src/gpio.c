/* gpio.c — 07-dual-blink GPIO initialisation
 *
 * Overrides LowLevel/Core/Src/gpio.c (M1 pattern).
 *
 * Configures the three GPIOC pins used by this project:
 *   PC8  (LED_R)      — push-pull output, starts LOW (LED OFF)
 *   PC9  (LED_G)      — push-pull output, starts LOW (LED OFF)
 *   PC11 (P3V3_SW_EN) — push-pull output, starts HIGH (3.3V rail ON)
 *
 * All other board GPIO pins remain in their post-reset default
 * (input, floating). This is safe for a standalone blink demo.
 * Peripheral-specific pins (UART7 PE7/PE8, etc.) are handled by their
 * respective HAL_xxx_MspInit callbacks in LowLevel/Core/Src/usart.c.
 */

#include "gpio.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIOC clock */
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Drive P3V3_SW_EN HIGH before configuring as output — avoids LOW glitch
     * on the 3.3V power rail during the brief input→output transition. */
    HAL_GPIO_WritePin(P3V3_SW_EN_GPIO_Port, P3V3_SW_EN_Pin, GPIO_PIN_SET);

    /* LEDs start OFF (ODR defaults to 0 on reset, but explicit is safer). */
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin | LED_G_Pin, GPIO_PIN_RESET);

    /* Configure PC8 (LED_R), PC9 (LED_G), PC11 (P3V3_SW_EN) as outputs. */
    GPIO_InitStruct.Pin   = LED_R_Pin | LED_G_Pin | P3V3_SW_EN_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}
