/* main.c — 12-ignition-status
 *
 * Reads ADC1 Ignition channel (PC0 / ADC1_INP10 / CH10 / Rank 3 / DMA[2])
 * via continuous DMA and controls LED_G (PC9):
 *   ADC raw >= 300  →  LED_G ON   (ignition detected)
 *   ADC raw <  300  →  LED_G OFF  (no ignition)
 *
 * P3V3_SW_EN (PC11) is driven HIGH permanently (already set by MX_GPIO_Init).
 *
 * LED_G (PC9) is a TIM3 PWM pin in CubeMX but is used here as plain GPIO
 * output — MX_TIM3_Init() is NOT called so the AF is never applied.
 *
 * Trace output (UART7, 115200 8N1, every 2 s):
 *   [IGN] raw=XXXX  LED_G=ON
 *   [IGN] raw=XXXX  LED_G=OFF
 *
 * ADC DMA buffer layout (9 elements, 32-bit each, circular):
 *   [0] CH9  PB0  ADC_ASSEMBLY
 *   [1] CH18 PA4  PVIN
 *   [2] CH10 PC0  IGNITION  <-- used
 *   [3] CH11 PC1  ADC_INPUT1
 *   [4] CH12 PC2  ADC_INPUT2
 *   [5] CH13 PC3  ADC_INPUT3
 *   [6] CH5  PB1  ADC_VBAT
 *   [7]      VREFINT
 *   [8]      TEMPSENSOR
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
#define ADC_CH_COUNT        9U
#define ADC_IDX_IGNITION    2U    /* CH10 / PC0 — rank 3 */
#define ADC_IGNITION_THRESH 300U  /* raw counts: above = ignition ON */

static volatile uint32_t g_adc_buf[ADC_CH_COUNT];

/* ── Trace interval ──────────────────────────────────────────────────────── */
#define TRACE_PERIOD_MS     2000U

/* ── Private prototypes ──────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_G_Init(void);
static void Trace_Print(const char *msg, uint16_t len);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* MX_GPIO_Init configures P3V3_SW_EN (PC11) as output HIGH — done. */
    MX_GPIO_Init();

    /* Configure LED_G (PC9) as GPIO output — not set up by MX_GPIO_Init
     * because CubeMX assigns PC9 to TIM3 CH4 PWM.  MX_TIM3_Init is NOT
     * called in this project so the AF is never applied.                 */
    LED_G_Init();
    HAL_Delay(10U);

    /* Ensure P3V3_SW_EN stays HIGH */
    HAL_GPIO_WritePin(P3V3_SW_EN_GPIO_Port, P3V3_SW_EN_Pin, GPIO_PIN_SET);

    /* GPDMA must be initialised before ADC */
    MX_GPDMA1_Init();
    MX_ADC1_Init();

    /* UART7: PE7=RX / PE8=TX, 115200 8N1, AF7 */
    MX_UART7_Init();

    /* ── Startup banner ─────────────────────────────────────────────────── */
    const char banner[] =
        "\r\n[IGN] 12-ignition-status starting\r\n"
        "[IGN] PC0/ADC1_INP10 threshold=300 | LED_G=PC9 | P3V3_SW_EN=PC11 HIGH\r\n";
    Trace_Print(banner, (uint16_t)strlen(banner));

    /* ── ADC self-calibration ───────────────────────────────────────────── */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        const char err[] = "[IGN] FAIL - ADC calibration error\r\n";
        Trace_Print(err, (uint16_t)strlen(err));
        Error_Handler();
    }
    const char cal_ok[] = "[IGN] ADC calibration: PASS\r\n";
    Trace_Print(cal_ok, (uint16_t)strlen(cal_ok));

    /* ── Start ADC DMA (circular, continuous) ───────────────────────────── */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_buf, ADC_CH_COUNT) != HAL_OK)
    {
        const char err[] = "[IGN] FAIL - DMA start error\r\n";
        Trace_Print(err, (uint16_t)strlen(err));
        Error_Handler();
    }
    const char dma_ok[] = "[IGN] ADC DMA started: PASS\r\n";
    Trace_Print(dma_ok, (uint16_t)strlen(dma_ok));

    /* ── Main loop ──────────────────────────────────────────────────────── */
    uint32_t t_trace = HAL_GetTick();

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* Snapshot ignition ADC — 32-bit read is atomic */
        uint32_t ign_raw = g_adc_buf[ADC_IDX_IGNITION];

        /* Update LED_G based on threshold (active-low: RESET=ON, SET=OFF) */
        if (ign_raw >= ADC_IGNITION_THRESH)
        {
            HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
        }

        /* Periodic trace */
        if ((now - t_trace) >= TRACE_PERIOD_MS)
        {
            t_trace = now;

            /* LED is active-low: pin RESET = LED ON */
            GPIO_PinState led_state = HAL_GPIO_ReadPin(LED_G_GPIO_Port, LED_G_Pin);
            char buf[64];
            int n = snprintf(buf, sizeof(buf),
                             "[IGN] raw=%4lu  LED_G=%s\r\n",
                             (unsigned long)ign_raw,
                             (led_state == GPIO_PIN_RESET) ? "ON " : "OFF");
            if (n > 0)
            {
                Trace_Print(buf, (uint16_t)n);
            }
        }

        HAL_Delay(10U);
    }
}

/* ── LED_G_Init ──────────────────────────────────────────────────────────── */
/* Configure PC9 as push-pull GPIO output, initial state LOW (LED off).       */
static void LED_G_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    /* GPIOC clock is already enabled by MX_GPIO_Init */
    gpio.Pin   = LED_G_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_G_GPIO_Port, &gpio);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
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
