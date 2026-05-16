/* main.c — 15-ble-scan
 *
 * Scans for BLE devices using the Quectel FC41D module and prints results
 * on the trace UART.
 *
 * Startup sequence:
 *   1. Power on FC41D (WIFI_BLE_PWR_EN PD5, WIFI_BLE_RESETN PD6), wait 3 s boot
 *   2. Poll AT until module responds (up to 10 s), read firmware version
 *   3. AT+QBLEINIT=1 (central), AT+QBLESCAN=1, collect URCs for 5 s
 *   4. Print scan summary on UART7 (trace)
 *   5. Main loop: blink LED + refresh watchdog
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

/* BLE scan duration */
#define BLE_SCAN_DURATION_MS  15000U

/* ── Private prototypes ──────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_Init(void);
static void Trace(const char *msg);
static void print_scan_results(const BLE_Device_t *devices, uint8_t count);

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

    Trace("\r\n[BLE-SCAN] 15-ble-scan starting\r\n");
    Trace("[BLE-SCAN] UART7=trace, UART9=FC41D, 115200 8N1\r\n");

    /* Step 1: power on FC41D and wait for boot */
    FC41D_Init();

    /* Step 2: detect module */
    FC41D_Result_t det = FC41D_Detect();
    if (det != FC41D_RESULT_OK) {
        Trace("[BLE-SCAN] FC41D: FAIL - not detected\r\n");
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
        while (1) {
            HAL_IWDG_Refresh(&hiwdg);
            HAL_Delay(500U);
        }
    }
    Trace("[BLE-SCAN] FC41D: PASS\r\n");

    /* Step 3: run BLE scan */
    static BLE_Device_t devices[BLE_SCAN_MAX_DEVICES];
    uint8_t n = FC41D_BLE_Scan(devices, BLE_SCAN_MAX_DEVICES,
                                BLE_SCAN_DURATION_MS);

    /* Step 4: print scan summary */
    print_scan_results(devices, n);

    /* Step 5: main loop — blink green, refresh watchdog */
    uint32_t last_trace = HAL_GetTick();

    while (1)
    {
        uint32_t t = HAL_GetTick();

        HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN,
            (t % 1000U < 500U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_RESET);

        if ((t - last_trace) >= 5000U) {
            last_trace = t;
            char buf[64];
            int  n_written = snprintf(buf, sizeof(buf),
                                      "[STATUS] scan done, %u device%s found\r\n",
                                      (unsigned)n, n == 1U ? "" : "s");
            if (n_written > 0) { Trace(buf); }
        }

        HAL_IWDG_Refresh(&hiwdg);
    }
}

/* ── print_scan_results ──────────────────────────────────────────────────── */
static void print_scan_results(const BLE_Device_t *devices, uint8_t count)
{
    char buf[128];
    int  nw;

    nw = snprintf(buf, sizeof(buf),
                  "[BLE-SCAN] ---- scan complete: %u device%s ----\r\n",
                  (unsigned)count, count == 1U ? "" : "s");
    if (nw > 0) { Trace(buf); }

    for (uint8_t i = 0U; i < count; i++) {
        if (devices[i].name[0] != '\0') {
            nw = snprintf(buf, sizeof(buf),
                          "[BLE] #%02u %s type=%u rssi=%d dBm name=%s\r\n",
                          (unsigned)(i + 1U),
                          devices[i].addr,
                          (unsigned)devices[i].addr_type,
                          (int)devices[i].rssi_dbm,
                          devices[i].name);
        } else {
            nw = snprintf(buf, sizeof(buf),
                          "[BLE] #%02u %s type=%u rssi=%d dBm\r\n",
                          (unsigned)(i + 1U),
                          devices[i].addr,
                          (unsigned)devices[i].addr_type,
                          (int)devices[i].rssi_dbm);
        }
        if (nw > 0) { Trace(buf); }
    }

    if (count == 0U) {
        Trace("[BLE-SCAN] no devices found\r\n");
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
