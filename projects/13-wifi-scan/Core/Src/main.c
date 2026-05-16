/* main.c — 13-wifi-scan
 *
 * Scans for available WiFi networks using the Quectel FC41D module on
 * TFL_CONNECT_2.
 *
 * Flow:
 *   1. Power on FC41D via UART9 AT commands.
 *   2. Send AT+QWSCAN — collect and print all +QWSCAN: result lines.
 *   3. Repeat scan every 30 s; print results table to UART7.
 *
 * Trace output: UART7 (PE7=RX, PE8=TX, 115200 8N1) → COM7
 * LED: GREEN 1 Hz blink = module OK; RED solid = module not found.
 * MCU: STM32H573VIT3Q @ 240 MHz (HSE 12 MHz → PLL1 M=1 N=40 P=2)
 */

/* ST_IOT main.h (via include path) provides: stm32h5xx_hal.h, all pin #defines */
#include "main.h"
#include "gpio.h"
#include "gpdma.h"
#include "usart.h"
#include "iwdg.h"
#include "fc41d.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* LED pins — PC8=RED, PC9=GREEN (initialised locally) */
#define LED_R_PIN   GPIO_PIN_8
#define LED_G_PIN   GPIO_PIN_9
#define LED_RG_PORT GPIOC

/* Repeat scan every 30 s */
#define SCAN_INTERVAL_MS  30000U

/* AP table stored between scans */
static FC41D_ApInfo_t s_aps[FC41D_SCAN_MAX_APS];

/* ── Private prototypes ──────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_Init(void);
static void Trace(const char *msg);
static void Tracef(char *buf, uint16_t buflen, const char *fmt, ...);
static void print_scan_table(int n_aps);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    LED_Init();
    MX_GPDMA1_Init();
    MX_GPDMA2_Init();
    MX_UART7_Init();    /* trace */
    MX_UART9_Init();    /* FC41D */
    MX_IWDG_Init();

    Trace("\r\n[WIFI-SCAN] 13-wifi-scan starting\r\n");
    Trace("[WIFI-SCAN] UART9=FC41D, UART7=trace, 115200 8N1\r\n");
    Trace("[WIFI-SCAN] scan interval = 30 s\r\n");

    /* Power on FC41D and wait for boot */
    FC41D_Init();

    /* Detect module */
    FC41D_Result_t result = FC41D_Detect();

    if (result == FC41D_RESULT_OK) {
        Trace("[WIFI-SCAN] FC41D: PASS - module detected\r\n");
    } else {
        Trace("[WIFI-SCAN] FC41D: FAIL - no response within 10 s\r\n");
    }

    /* Schedule first scan immediately */
    uint32_t last_scan = HAL_GetTick() - SCAN_INTERVAL_MS;
    int      last_n    = 0;

    /* ── Main loop ───────────────────────────────────────────────────────── */
    while (1)
    {
        uint32_t t = HAL_GetTick();

        /* LED: GREEN blink = OK, RED solid = fail */
        if (result == FC41D_RESULT_OK) {
            HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN,
                (t % 1000U < 500U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_RESET);
        } else {
            HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
        }

        /* Periodic WiFi scan */
        if (result == FC41D_RESULT_OK && (t - last_scan) >= SCAN_INTERVAL_MS) {
            last_scan = t;

            Trace("\r\n[WIFI-SCAN] ======== WiFi Scan ========\r\n");
            HAL_IWDG_Refresh(&hiwdg);

            int n = FC41D_WifiScan(s_aps, (uint8_t)FC41D_SCAN_MAX_APS);

            if (n < 0) {
                Trace("[WIFI-SCAN] ERROR: AT+QWSCAN failed\r\n");
            } else if (n == 0) {
                Trace("[WIFI-SCAN] No networks found\r\n");
            } else {
                last_n = n;
                print_scan_table(n);
            }
            Trace("[WIFI-SCAN] ==============================\r\n");
        }

        HAL_IWDG_Refresh(&hiwdg);
        (void)last_n;
    }
}

/* ── Scan results table ──────────────────────────────────────────────────── */
static void print_scan_table(int n_aps)
{
    char buf[80];

    Trace("[WIFI-SCAN] #   RSSI    Sec   BSSID              SSID\r\n");
    Trace("[WIFI-SCAN] --  ------  ----  -----------------  --------------------------------\r\n");

    for (int i = 0; i < n_aps && i < (int)FC41D_SCAN_MAX_APS; i++) {
        const FC41D_ApInfo_t *ap = &s_aps[i];
        const char *sec;
        switch (ap->security) {
            case 0:  sec = "OPEN"; break;
            case 1:  sec = "WEP "; break;
            case 2:  sec = "WPA "; break;
            case 3:  sec = "WPA2"; break;
            case 4:  sec = "MIX "; break;
            default: sec = "?   "; break;
        }
        Tracef(buf, sizeof(buf),
               "[WIFI-SCAN] %2d  %4d dBm  %s  %s  %s\r\n",
               i + 1, (int)ap->rssi, sec, ap->bssid, ap->ssid);
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

/* ── Trace helpers ───────────────────────────────────────────────────────── */
static void Trace(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg,
                      (uint16_t)strlen(msg), 1000U);
}

static void Tracef(char *buf, uint16_t buflen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, buflen, fmt, ap);
    va_end(ap);
    if (n > 0 && (uint16_t)n < buflen) {
        HAL_UART_Transmit(&huart7, (const uint8_t *)buf, (uint16_t)n, 1000U);
    }
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
