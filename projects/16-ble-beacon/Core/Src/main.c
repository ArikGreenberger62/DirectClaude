/* main.c — 16-ble-beacon
 *
 * BLE Beacon using Quectel FC41D module:
 *   - Device name: "CCoreAIBLE"
 *   - Advertising payload: flags + manufacturer specific "Hello from CCoreAI"
 *
 * Advertising data (25 bytes, fits within 31-byte BLE limit):
 *   02 01 06            — Flags: LE General Discoverable, no BR/EDR
 *   15 FF FF FF         — Manufacturer Specific, company 0xFFFF (not assigned),
 *                         length = 21 (1 type + 2 company + 18 message bytes)
 *   48 65 6C 6C 6F 20   — "Hello "
 *   66 72 6F 6D 20      — "from "
 *   43 43 6F 72 65 41 49 — "CCoreAI"
 *
 * The device name "CCoreAIBLE" is set via AT+QBLENAME and appears in:
 *   - GATT Device Name characteristic (for connected clients)
 *   - BLE scan response packets (for active scanners)
 *
 * Trace output: UART7 (PE7=RX, PE8=TX, 115200 8N1) → COM7
 * FC41D AT port: UART9 (PD14=RX, PD15=TX, 115200 8N1)
 *
 * MCU: STM32H573VIT3Q @ 240 MHz (HSE 12 MHz → PLL1 M=1 N=40 P=2)
 */

#include "main.h"
#include "gpio.h"
#include "gpdma.h"
#include "usart.h"
#include "iwdg.h"
#include "fc41d.h"
#include <string.h>
#include <stdio.h>

/* LED pins: PC8=RED, PC9=GREEN */
#define LED_R_PIN    GPIO_PIN_8
#define LED_G_PIN    GPIO_PIN_9
#define LED_RG_PORT  GPIOC

/* BLE beacon configuration */
#define BEACON_NAME     "CCoreAIBLE"

/* Advertising payload (hex string, 25 bytes):
 *   02 01 06  = Flags: LE General Discoverable, BR/EDR Not Supported
 *   15 FF FF FF = Manufacturer Specific (len=21, company=0xFFFF)
 *   48 65 6C 6C 6F 20 66 72 6F 6D 20 43 43 6F 72 65 41 49 = "Hello from CCoreAI"
 */
#define BEACON_ADV_HEX \
    "02010615FFFFFF48656C6C6F2066726F6D2043436F72654149"

/* ── Private prototypes ──────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_Init(void);
static void Trace(const char *msg);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    LED_Init();
    MX_GPDMA1_Init();
    MX_GPDMA2_Init();
    MX_UART7_Init();   /* trace */
    MX_UART9_Init();   /* FC41D AT command port */
    MX_IWDG_Init();

    Trace("\r\n[BEACON] 16-ble-beacon starting\r\n");
    Trace("[BEACON] UART7=trace, UART9=FC41D, 115200 8N1\r\n");
    Trace("[BEACON] name=\"" BEACON_NAME "\"\r\n");
    Trace("[BEACON] message=\"Hello from CCoreAI\"\r\n");

    /* Step 1: power on FC41D and wait for boot */
    FC41D_Init();

    /* Step 2: detect module and read firmware version */
    FC41D_Result_t det = FC41D_Detect();
    if (det != FC41D_RESULT_OK) {
        Trace("[BEACON] FC41D: FAIL - not detected\r\n");
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
        while (1) {
            HAL_IWDG_Refresh(&hiwdg);
            HAL_Delay(500U);
        }
    }
    Trace("[BEACON] FC41D: PASS\r\n");

    /* Step 3: start BLE advertising beacon */
    FC41D_Result_t beacon = FC41D_BLE_BeaconStart(BEACON_NAME, BEACON_ADV_HEX);
    if (beacon != FC41D_RESULT_OK) {
        Trace("[BEACON] BLE beacon: FAIL\r\n");
        /* Blink red fast to indicate BLE init failure */
        while (1) {
            HAL_GPIO_TogglePin(LED_RG_PORT, LED_R_PIN);
            HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
            HAL_IWDG_Refresh(&hiwdg);
            HAL_Delay(200U);
        }
    }
    Trace("[BEACON] BLE beacon: PASS\r\n");
    Trace("[BEACON] advertising — scan with a BLE scanner app to see CCoreAIBLE\r\n");

    /* Step 4: main loop — blink green slowly, refresh watchdog, status trace */
    uint32_t last_trace = HAL_GetTick();

    while (1)
    {
        uint32_t t = HAL_GetTick();

        /* 1 Hz blink: green on for 500 ms, off for 500 ms */
        HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN,
            (t % 1000U < 500U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_RESET);

        /* Status trace every 10 s */
        if ((t - last_trace) >= 10000U) {
            last_trace = t;
            Trace("[STATUS] beacon running — advertising CCoreAIBLE / Hello from CCoreAI\r\n");
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

/* ── Trace ───────────────────────────────────────────────────────────────── */
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
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK=240 MHz. */
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
