/* main.c — 11-adc-check
 *
 * Reads ADC1 via DMA (9-channel circular scan) and traces two channels
 * every 5 seconds on UART7 in column format:
 *
 *   Column 1: Ignition  — PC0 / ADC1_INP10 / Rank 3 / DMA[2]
 *   Column 2: PVIN      — PA4 / ADC1_INP18 / Rank 2 / DMA[1]
 *
 * ADC DMA buffer layout (9 elements, 32-bit each, circular):
 *   [0] CH9  PB0  ADC_ASSEMBLY
 *   [1] CH18 PA4  PVIN          <-- traced
 *   [2] CH10 PC0  IGNITION      <-- traced
 *   [3] CH11 PC1  ADC_INPUT1
 *   [4] CH12 PC2  ADC_INPUT2
 *   [5] CH13 PC3  ADC_INPUT3
 *   [6] CH5  PB1  ADC_VBAT
 *   [7]      VREFINT
 *   [8]      TEMPSENSOR
 *
 * Trace output (UART7 115200 8N1, once per 5 s):
 *   [ADC] --- Ignition(PC0) | PVIN(PA4) ---
 *   [ADC]        raw    mV  |   raw    mV
 *   [ADC]       2048  1650  |  3517  2837
 *
 * MCU : STM32H573VIT3Q @ 240 MHz
 * HAL : FW_H5 V1.6.0
 */

#include "main.h"
#include "adc.h"
#include "fdcan.h"
#include "gpdma.h"
#include "i2c.h"
#include "iwdg.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ── ADC DMA buffer ──────────────────────────────────────────────────────── */
#define ADC_CH_COUNT    9U
#define ADC_IDX_PVIN    1U   /* CH18 / PA4  — rank 2 */
#define ADC_IDX_IGN     2U   /* CH10 / PC0  — rank 3 */
#define ADC_VREF_MV     3300U
#define ADC_FULL_SCALE  4095U

static volatile uint32_t g_adc_buf[ADC_CH_COUNT];

/* ── Trace interval ──────────────────────────────────────────────────────── */
#define TRACE_PERIOD_MS 5000U

/* ── Private prototypes ──────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void Trace_Print(const char *msg, uint16_t len);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    HAL_Delay(10U);

    /* GPDMA must be initialised before ADC (MspInit links Channel0 to ADC1) */
    MX_GPDMA1_Init();
    MX_ADC1_Init();

    /* UART7: PE7=RX / PE8=TX, 115200 8N1, AF7 */
    MX_UART7_Init();

    /* ── Startup banner ─────────────────────────────────────────────────── */
    const char banner[] =
        "\r\n[ADC] 11-adc-check starting — ADC1 DMA, 5s trace\r\n"
        "[ADC] Channels: Ignition=PC0/CH10/rank3  PVIN=PA4/CH18/rank2\r\n";
    Trace_Print(banner, (uint16_t)strlen(banner));

    /* ── ADC self-calibration (offset, single-ended) ────────────────────── */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        const char err[] = "[ADC] FAIL - calibration error\r\n";
        Trace_Print(err, (uint16_t)strlen(err));
        Error_Handler();
    }
    const char cal_ok[] = "[ADC] calibration: PASS\r\n";
    Trace_Print(cal_ok, (uint16_t)strlen(cal_ok));

    /* ── Start ADC DMA (circular, continuous) ───────────────────────────── */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_buf, ADC_CH_COUNT) != HAL_OK)
    {
        const char err[] = "[ADC] FAIL - DMA start error\r\n";
        Trace_Print(err, (uint16_t)strlen(err));
        Error_Handler();
    }
    const char dma_ok[] = "[ADC] DMA started: PASS\r\n";
    Trace_Print(dma_ok, (uint16_t)strlen(dma_ok));

    /* ── Column header ──────────────────────────────────────────────────── */
    const char hdr[] =
        "[ADC] --- Ignition(PC0) | PVIN(PA4) ---\r\n"
        "[ADC]      raw    mV    |   raw    mV\r\n";
    Trace_Print(hdr, (uint16_t)strlen(hdr));

    /* ── Main loop ──────────────────────────────────────────────────────── */
    uint32_t t_trace = HAL_GetTick();

    while (1)
    {
        uint32_t now = HAL_GetTick();

        if ((now - t_trace) >= TRACE_PERIOD_MS)
        {
            t_trace = now;

            /* Snapshot — DMA may overwrite concurrently; 32-bit read is atomic */
            uint32_t ign  = g_adc_buf[ADC_IDX_IGN];
            uint32_t pvin = g_adc_buf[ADC_IDX_PVIN];

            /* Convert raw counts to millivolts (3.3 V reference, 12-bit ADC) */
            uint32_t ign_mv  = (ign  * ADC_VREF_MV) / ADC_FULL_SCALE;
            uint32_t pvin_mv = (pvin * ADC_VREF_MV) / ADC_FULL_SCALE;

            char buf[80];
            int n = snprintf(buf, sizeof(buf),
                             "[ADC]  %6lu  %4lu    |  %6lu  %4lu\r\n",
                             (unsigned long)ign,  (unsigned long)ign_mv,
                             (unsigned long)pvin, (unsigned long)pvin_mv);
            if (n > 0)
            {
                Trace_Print(buf, (uint16_t)n);
            }
        }
    }
}

/* ── Trace_Print ─────────────────────────────────────────────────────────── */
static void Trace_Print(const char *msg, uint16_t len)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, len, 1000U);
}

/* ── HAL_TIM_PeriodElapsedCallback ──────────────────────────────────────── */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        HAL_IncTick();
    }
}

/* ── SystemClock_Config ──────────────────────────────────────────────────── */
/* HSE 12 MHz → PLL1 (M=1 N=40 P=2) → SYSCLK = 240 MHz                      */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI
                                     | RCC_OSCILLATORTYPE_HSE
                                     | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.LSEState       = RCC_LSE_ON;
    RCC_OscInitStruct.LSIState       = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLL1_SOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 1;
    RCC_OscInitStruct.PLL.PLLN       = 40;
    RCC_OscInitStruct.PLL.PLLP       = 2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    RCC_OscInitStruct.PLL.PLLR       = 2;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN   = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                     | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* ── Error_Handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
