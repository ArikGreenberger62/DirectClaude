/* main.c — 07-dual-blink
 *
 * LED blink pattern (1000 ms cycle, driven by HAL_GetTick() polling):
 *
 *   t =   0 .. 199 ms  RED = ON,  GREEN = OFF   (RED 20% duty, 200 ms ON)
 *   t = 200 .. 999 ms  RED = OFF, GREEN = ON    (GREEN = complement of RED)
 *
 * Both LEDs blink at 1 Hz. GREEN is the exact inverse of RED:
 *   RED ON  → GREEN OFF
 *   RED OFF → GREEN ON
 * No timer interrupt required — the main loop polls HAL_GetTick() continuously.
 *
 * Trace output on UART7 (PE7/PE8), 115200 8N1:
 *   - State-transition lines printed whenever RED or GREEN changes
 *   - 1 Hz heartbeat line
 *
 * MCU : STM32H573VIT3Q @ 240 MHz
 * HAL : STM32Cube FW_H5 V1.6.0
 */

#include "main.h"
#include "gpio.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* ── Timing constants (ms) ───────────────────────────────────────────────── */
#define PERIOD_MS   1000U   /* total cycle duration                          */
#define RED_ON_MS    200U   /* RED on for 200 ms = 20 % duty                 */

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void Trace_Print(const char *msg);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_UART7_Init();

    Trace_Print("[07-dual-blink] init complete\r\n");
    Trace_Print("[RED]   1 Hz, 20% duty  — 200 ms ON / 800 ms OFF\r\n");
    Trace_Print("[GREEN] 1 Hz, complement — ON when RED is OFF, OFF when RED is ON\r\n");

    /* Sentinel values that differ from all valid GPIO_PinState values so the
     * first loop iteration always triggers the transition trace. */
    GPIO_PinState last_red   = (GPIO_PinState)0xFFU;
    GPIO_PinState last_green = (GPIO_PinState)0xFFU;
    uint32_t      last_hb    = 0U;

    while (1)
    {
        uint32_t tick  = HAL_GetTick();
        uint32_t phase = tick % PERIOD_MS;

        /* ── RED: ON during first 200 ms of each 1 s cycle (20% duty) ──── */
        GPIO_PinState red   = (phase < RED_ON_MS) ? GPIO_PIN_SET : GPIO_PIN_RESET;

        /* ── GREEN: exact complement of RED ─────────────────────────────── */
        GPIO_PinState green = (red == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;

        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, red);
        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, green);

        /* ── Trace: log every state transition ─────────────────────────── */
        if (red != last_red || green != last_green)
        {
            char buf[52];
            int  n = snprintf(buf, sizeof(buf),
                              "[t=%3ums] RED=%s GREEN=%s\r\n",
                              (unsigned int)phase,
                              (red   == GPIO_PIN_SET) ? "ON " : "OFF",
                              (green == GPIO_PIN_SET) ? "ON " : "OFF");
            if (n > 0 && n < (int)sizeof(buf))
            {
                Trace_Print(buf);
            }
            last_red   = red;
            last_green = green;
        }

        /* ── 1 Hz heartbeat ─────────────────────────────────────────────── */
        if ((tick - last_hb) >= PERIOD_MS)
        {
            last_hb = tick;
            Trace_Print("[HEARTBEAT] running\r\n");
        }
    }
}

/* ── SystemClock_Config ──────────────────────────────────────────────────── */
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK = 240 MHz                   */
/* APB1 = 240 MHz (UART7 clock source), APB2 = 120 MHz, APB3 = 240 MHz      */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* VOS0 required for 240 MHz operation */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSEState        = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState        = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState    = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource   = RCC_PLL1_SOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM        = 1U;
    RCC_OscInitStruct.PLL.PLLN        = 40U;  /* VCO = 12 * 40 = 480 MHz */
    RCC_OscInitStruct.PLL.PLLP        = 2U;   /* SYSCLK = 480 / 2 = 240 MHz */
    RCC_OscInitStruct.PLL.PLLQ        = 4U;   /* PLLQ  = 120 MHz */
    RCC_OscInitStruct.PLL.PLLR        = 2U;
    RCC_OscInitStruct.PLL.PLLRGE      = RCC_PLL1_VCIRANGE_3;   /* 8–16 MHz */
    RCC_OscInitStruct.PLL.PLLVCOSEL   = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN    = 0U;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                     | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
    /* 5 wait states required at VOS0 / 240 MHz */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ── Trace_Print ─────────────────────────────────────────────────────────── */
static void Trace_Print(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg,
                      (uint16_t)strlen(msg), HAL_MAX_DELAY);
}

/* ── Error_Handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

/* ── assert_failed ───────────────────────────────────────────────────────── */
/* Required by USE_FULL_ASSERT in stm32h5xx_hal_conf.h. */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
