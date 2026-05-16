/* main.c — 12-ble-detect
 *
 * Detects the Quectel FC41D Wi-Fi+BLE module on TFL_CONNECT_2.
 *
 * Power-on sequence (FC41D):
 *   P3V3_SW_EN (PC11)     already HIGH from MX_GPIO_Init
 *   WIFI_BLE_PWR_EN (PD5) driven HIGH
 *   WIFI_BLE_RESETN (PD6) released HIGH (open-drain, was LOW = in reset)
 *
 * Detection: UART9 (PD14/PD15, AF11, 115200 8N1)
 *   Send "ATE0" / "AT" every 500 ms for up to 10 s.
 *   On OK: send AT+QVERSION, trace version line.
 *
 * Trace output: UART7 (PE7=RX, PE8=TX, 115200 8N1) → COM7
 *
 * LED:
 *   RED fast (5 Hz)  — detecting
 *   GREEN slow (1 Hz) — detected (PASS)
 *   RED solid         — not detected (FAIL)
 *
 * MCU: STM32H573VIT3Q @ 240 MHz (HSE 12 MHz → PLL1 M=1 N=40 P=2)
 */

/* ST_IOT main.h (via include path) provides: stm32h5xx_hal.h, all pin #defines */
#include "main.h"
#include "gpio.h"
#include "gpdma.h"
#include "usart.h"
#include "iwdg.h"
#include "fc41d.h"
#include <string.h>

/* LED pins — PC8=RED, PC9=GREEN (not in ST_IOT IOC, initialised locally) */
#define LED_R_PIN   GPIO_PIN_8
#define LED_G_PIN   GPIO_PIN_9
#define LED_RG_PORT GPIOC

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_Init(void);
static void Trace(const char *msg);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* ST_IOT CubeMX-generated peripheral init */
    MX_GPIO_Init();
    LED_Init();
    MX_GPDMA1_Init();
    MX_GPDMA2_Init();
    MX_UART7_Init();    /* trace */
    MX_UART9_Init();    /* FC41D */
    MX_IWDG_Init();

    Trace("\r\n[BLE-DETECT] 12-ble-detect starting\r\n");
    Trace("[BLE-DETECT] UART9=FC41D, UART7=trace, 115200 8N1\r\n");

    /* Power on FC41D, wait for boot */
    FC41D_Init();

    /* Poll AT commands and report result */
    FC41D_Result_t result = FC41D_Detect();

    if (result == FC41D_RESULT_OK) {
        Trace("[BLE-DETECT] FC41D: PASS - module detected and responding\r\n");
    } else {
        Trace("[BLE-DETECT] FC41D: FAIL - no response within 10 s\r\n");
    }

    /* ── Main loop: blink LED + 1 Hz status trace ─────────────────────────── */
    uint32_t last_trace = HAL_GetTick();

    while (1)
    {
        uint32_t t = HAL_GetTick();

        /* LED: GREEN blink (PASS) or RED solid (FAIL) */
        if (result == FC41D_RESULT_OK) {
            HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN,
                (t % 1000U < 500U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_RESET);
        } else {
            HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
        }

        /* 1 Hz status trace */
        if ((t - last_trace) >= 1000U) {
            last_trace = t;
            Trace(result == FC41D_RESULT_OK
                  ? "[STATUS] FC41D detected - OK\r\n"
                  : "[STATUS] FC41D not detected - FAIL\r\n");
        }

        HAL_IWDG_Refresh(&hiwdg);
    }
}

/* ── LED ─────────────────────────────────────────────────────────────────── */
static void LED_Init(void)
{
    GPIO_InitTypeDef cfg = {0};
    HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN | LED_G_PIN, GPIO_PIN_RESET);
    cfg.Pin   = LED_R_PIN | LED_G_PIN;
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_RG_PORT, &cfg);
}

/* ── Trace helper ────────────────────────────────────────────────────────── */
static void Trace(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg,
                      (uint16_t)strlen(msg), 1000U);
}

/* ── UART RX interrupt callback ──────────────────────────────────────────── */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART9) {
        FC41D_RxByte();
    }
}

/* ── HAL timebase callback (TIM4) ────────────────────────────────────────── */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) { HAL_IncTick(); }
}

/* ── Error_Handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif

/* ── SystemClock_Config ──────────────────────────────────────────────────── */
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK=240 MHz. Copied from ST_IOT. */
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
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}
