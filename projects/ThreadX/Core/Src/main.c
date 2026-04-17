/* main.c — ThreadX blink project
 *
 * LED_R (PC8) blinks at 1 Hz from a ThreadX software timer (tx_timer).
 * LED_G (PC9) blinks at 1 Hz from a dedicated ThreadX thread.
 * UART7 (PE7/PE8, 115200 8N1) emits boot banner + 1 Hz heartbeat trace.
 * P3V3_SW_EN (PC11) driven HIGH to enable the 3.3V rail.
 *
 * MCU : STM32H573VIT3Q @ 240 MHz (HSE 12 MHz → PLL1 M=1 N=40 P=2)
 * RTOS: Azure RTOS ThreadX, Cortex-M33 GNU port, 10 ms tick.
 */

#include "main.h"
#include "app_threadx.h"
#include <string.h>

UART_HandleTypeDef huart7;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART7_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_UART7_Init();

    /* Boot banner — printed BEFORE kernel enter so it's always visible, even
     * if the scheduler fails to start. */
    const char banner[] =
        "\r\n[THX] boot SYSCLK=240000000 Hz, tick=100 Hz, build " __DATE__ " " __TIME__ "\r\n";
    (void)HAL_UART_Transmit(&huart7, (uint8_t *)banner, (uint16_t)strlen(banner), 100U);

    /* Enters the ThreadX scheduler — never returns on success. */
    MX_ThreadX_Init();

    while (1) { }
}

/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK = 240 MHz                    */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLL1_SOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM            = 1U;
    RCC_OscInitStruct.PLL.PLLN            = 40U;
    RCC_OscInitStruct.PLL.PLLP            = 2U;
    RCC_OscInitStruct.PLL.PLLQ            = 4U;
    RCC_OscInitStruct.PLL.PLLR            = 2U;
    RCC_OscInitStruct.PLL.PLLRGE          = RCC_PLL1_VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL       = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN        = 0U;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                     | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

/* LED_R (PC8), LED_G (PC9), P3V3_SW_EN (PC11) as push-pull outputs. */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(P3V3_SW_EN_PORT, P3V3_SW_EN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN | LED_G_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LED_R_PIN | LED_G_PIN | P3V3_SW_EN_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* UART7 @ 115200 8N1 — PCLK1 = 240 MHz kernel clock (default). */
static void MX_UART7_Init(void)
{
    huart7.Instance                      = UART7;
    huart7.Init.BaudRate                 = 115200;
    huart7.Init.WordLength               = UART_WORDLENGTH_8B;
    huart7.Init.StopBits                 = UART_STOPBITS_1;
    huart7.Init.Parity                   = UART_PARITY_NONE;
    huart7.Init.Mode                     = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl                = UART_HWCONTROL_NONE;
    huart7.Init.OverSampling             = UART_OVERSAMPLING_16;
    huart7.Init.OneBitSampling           = UART_ONE_BIT_SAMPLE_DISABLE;
    huart7.Init.ClockPrescaler           = UART_PRESCALER_DIV1;
    huart7.AdvancedInit.AdvFeatureInit   = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart7) != HAL_OK) { Error_Handler(); }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart7, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) { Error_Handler(); }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart7, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) { Error_Handler(); }
    if (HAL_UARTEx_DisableFifoMode(&huart7) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
