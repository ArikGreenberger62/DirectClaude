/* main.c — 02-uart7-echo
 *
 * UART7 line echo: reads characters until CR or LF, then echoes the
 * accumulated line back wrapped in double-quotes:  "received text"\r\n
 *
 * UART7: PE7 (RX) / PE8 (TX)  115200 8N1  AF7
 *
 * MCU : STM32H573VIT3Q @ 240 MHz
 * HAL : STM32Cube FW_H5 V1.6.0
 */

#include "main.h"
#include <string.h>

/* ── Peripheral handle ───────────────────────────────────────────────────── */
UART_HandleTypeDef huart7;

/* ── Echo state (shared between callback and main loop) ──────────────────── */
static volatile uint8_t  g_rx_byte;            /* single-byte DMA target     */
static          uint8_t  g_rx_buf[ECHO_BUF_SIZE]; /* accumulated line        */
static volatile uint16_t g_rx_len;             /* chars in g_rx_buf          */
static          uint8_t  g_tx_buf[ECHO_BUF_SIZE + 4U]; /* "line"\r\n        */
static volatile uint16_t g_tx_len;             /* bytes ready to send        */
static volatile uint8_t  g_echo_pending;       /* flag: main loop must send  */

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART7_Init(void);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* P3V3_SW_EN must be HIGH before peripheral power can be assumed stable */
    MX_GPIO_Init();

    MX_UART7_Init();

    /* Arm the first single-byte receive interrupt */
    if (HAL_UART_Receive_IT(&huart7, (uint8_t *)&g_rx_byte, 1U) != HAL_OK)
    {
        Error_Handler();
    }

    while (1)
    {
        if (g_echo_pending)
        {
            /* Build the quoted reply assembled in the callback */
            HAL_UART_Transmit(&huart7, g_tx_buf, g_tx_len, 1000U);
            g_echo_pending = 0U;
        }
    }
}

/* ── HAL_UART_RxCpltCallback ─────────────────────────────────────────────── */
/* Called from UART7_IRQHandler → HAL_UART_IRQHandler each time one byte     */
/* is received.  Accumulates the line; on CR/LF builds the quoted reply and   */
/* sets g_echo_pending so the main loop sends it.                             */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != UART7)
    {
        return;
    }

    uint8_t ch = g_rx_byte;

    if (ch == '\r' || ch == '\n')
    {
        if (g_rx_len > 0U)
        {
            /* Build: " + line + " + \r\n */
            g_tx_buf[0] = '"';
            memcpy(&g_tx_buf[1], g_rx_buf, g_rx_len);
            g_tx_buf[1U + g_rx_len]      = '"';
            g_tx_buf[2U + g_rx_len]      = '\r';
            g_tx_buf[3U + g_rx_len]      = '\n';
            g_tx_len                     = (uint16_t)(4U + g_rx_len);
            g_rx_len                     = 0U;
            g_echo_pending               = 1U;
        }
        /* If g_rx_len == 0 the line is empty (e.g. second byte of CRLF) —
         * skip silently to avoid echoing an empty quoted pair. */
    }
    else
    {
        if (g_rx_len < (ECHO_BUF_SIZE - 1U))
        {
            g_rx_buf[g_rx_len] = ch;
            g_rx_len++;
        }
        /* If buffer full: discard the character; line will still be echoed
         * when CR/LF arrives with whatever fit in the buffer. */
    }

    /* Re-arm for the next byte */
    HAL_UART_Receive_IT(&huart7, (uint8_t *)&g_rx_byte, 1U);
}

/* ── SystemClock_Config ──────────────────────────────────────────────────── */
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK = 240 MHz                   */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLL1_SOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 1U;
    RCC_OscInitStruct.PLL.PLLN       = 40U;
    RCC_OscInitStruct.PLL.PLLP       = 2U;
    RCC_OscInitStruct.PLL.PLLQ       = 4U;
    RCC_OscInitStruct.PLL.PLLR       = 2U;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN   = 0U;

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

/* ── MX_GPIO_Init ────────────────────────────────────────────────────────── */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(P3V3_SW_EN_PORT, P3V3_SW_EN_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = P3V3_SW_EN_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(P3V3_SW_EN_PORT, &GPIO_InitStruct);
}

/* ── MX_UART7_Init ───────────────────────────────────────────────────────── */
/* 115200 baud, 8N1, interrupt-driven RX.  GPIO MspInit is in hal_msp.c.    */
static void MX_UART7_Init(void)
{
    huart7.Instance          = UART7;
    huart7.Init.BaudRate     = 115200U;
    huart7.Init.WordLength   = UART_WORDLENGTH_8B;
    huart7.Init.StopBits     = UART_STOPBITS_1;
    huart7.Init.Parity       = UART_PARITY_NONE;
    huart7.Init.Mode         = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart7.Init.OverSampling = UART_OVERSAMPLING_16;
    huart7.Init.OneBitSampling        = UART_ONE_BIT_SAMPLE_DISABLE;
    huart7.Init.ClockPrescaler        = UART_PRESCALER_DIV1;
    huart7.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart7) != HAL_OK) { Error_Handler(); }
}

/* ── Error_Handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
