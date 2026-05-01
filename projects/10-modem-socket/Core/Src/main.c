/* main.c — 10-modem-socket
 *
 * Quectel EG915N modem: TCP socket to test.traffilog.co.il:30111 (APN=internet).
 * Trace + command interface on UART7 (PE7=RX, PE8=TX, 115200 8N1).
 *
 * Commands (type into UART7 terminal, press ENTER):
 *   send <message>   — send message to TCP server (appends \r\n)
 *
 * Server messages are printed to UART7:
 *   [TCP] recv: <data>
 *
 * LED behaviour:
 *   RED fast (5 Hz)   — booting / initialising
 *   GREEN slow (1 Hz) — TCP socket connected
 *   RED solid         — error
 */

#include "main.h"
#include "gpio.h"
#include "gpdma.h"
#include "usart.h"
#include "modem.h"
#include "ring_buf.h"
#include <string.h>
#include <stdio.h>

/* ── Private constants ─────────────────────────────────────────────────────── */
#define CMD_BUF_SIZE  256U
#define MSG_MAX       200U

/* ── Boot-step diagnostic — readable via SWD without UART ─────────────────── */
volatile uint32_t g_boot_step = 0U;

/* ── UART7 RX ring buffer (trace command input) ────────────────────────────── */
static RingBuf_t s_cmd_rx;
static uint8_t   s_cmd_rx_byte;

/* ── Command assembly buffer ───────────────────────────────────────────────── */
static char     s_cmd_line[CMD_BUF_SIZE];
static uint16_t s_cmd_pos;

/* ── Private function prototypes ───────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_Init(void);
static void Trace_Print(const char *msg);
static void cmd_rx_arm(void);
static void process_cmd_line(const char *line);
static void update_leds(ModemState_t state);

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void)
{
    g_boot_step = 1U;
    HAL_Init();
    g_boot_step = 2U;
    SystemClock_Config();
    g_boot_step = 3U;
    MX_GPIO_Init();
    g_boot_step = 4U;
    LED_Init();
    g_boot_step = 5U;
    MX_GPDMA1_Init();
    g_boot_step = 6U;
    MX_GPDMA2_Init();
    g_boot_step = 7U;
    MX_UART7_Init();
    g_boot_step = 8U;
    /* Detach TX DMA — we use blocking HAL_UART_Transmit, not DMA TX */
    huart7.hdmatx = NULL;

    /* Print banner immediately after UART7 is ready — before any other init
     * that could hang, so we can diagnose boot issues via trace. */
    Trace_Print("\r\n[SOCKET] 10-modem-socket starting\r\n");
    g_boot_step = 9U;
    Trace_Print("[SOCKET] USART2=modem, UART7=trace/command, 115200 8N1\r\n");
    Trace_Print("[SOCKET] target: test.traffilog.co.il:30111 (APN=internet)\r\n");
    g_boot_step = 10U;

    MX_USART2_UART_Init();
    huart2.hdmatx = NULL;   /* detach TX DMA — use blocking AT send */

    RingBuf_Init(&s_cmd_rx);
    s_cmd_pos = 0U;
    cmd_rx_arm();

    /* Blocking init — power on modem, wait for boot, AT init, TCP connect */
    Modem_Init();

    /* ── Main loop ─────────────────────────────────────────────────────────── */
    while (1)
    {
        /* 1. Process modem URCs (+QIURC: recv/closed/pdpdeact) */
        Modem_Process();

        /* 2. Drain UART7 RX — assemble command lines */
        uint8_t b;
        while (RingBuf_Get(&s_cmd_rx, &b)) {
            /* Echo character */
            HAL_UART_Transmit(&huart7, &b, 1U, 100U);

            if (b == '\r' || b == '\n') {
                if (s_cmd_pos > 0U) {
                    s_cmd_line[s_cmd_pos] = '\0';
                    Trace_Print("\r\n");
                    process_cmd_line(s_cmd_line);
                    s_cmd_pos = 0U;
                }
            } else if (b == 0x08U || b == 0x7FU) {
                if (s_cmd_pos > 0U) { s_cmd_pos--; }
            } else if (s_cmd_pos < (uint16_t)(CMD_BUF_SIZE - 1U)) {
                s_cmd_line[s_cmd_pos++] = (char)b;
            }
        }

        /* 3. Update LEDs */
        update_leds(Modem_GetState());
    }
}

/* ── Command processor ─────────────────────────────────────────────────────── */
/* Supported command:
 *   send <message>   — everything after "send " is the message body */
static void process_cmd_line(const char *line)
{
    /* Skip leading whitespace */
    while (*line == ' ') { line++; }

    if (strncmp(line, "send", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        const char *msg = line + 4;
        while (*msg == ' ') { msg++; }
        if (*msg == '\0') {
            Trace_Print("[CMD] usage: send <message>\r\n");
            return;
        }
        (void)Modem_SendTCP(msg);
        return;
    }

    Trace_Print("[CMD] unknown command.\r\n");
    Trace_Print("[CMD] usage:   send <message>\r\n");
    Trace_Print("[CMD] example: send Hello server!\r\n");
}

/* ── LED helpers ───────────────────────────────────────────────────────────── */
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

static void update_leds(ModemState_t state)
{
    uint32_t t = HAL_GetTick();
    switch (state) {
    case MODEM_STATE_BOOTING:
    case MODEM_STATE_INIT:
        /* Red fast 5 Hz */
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN,
            (t % 200U < 100U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
        break;
    case MODEM_STATE_READY:
        /* Green slow 1 Hz */
        HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN,
            (t % 1000U < 500U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_RESET);
        break;
    case MODEM_STATE_ERROR:
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RG_PORT, LED_G_PIN, GPIO_PIN_RESET);
        break;
    default:
        HAL_GPIO_WritePin(LED_RG_PORT, LED_R_PIN | LED_G_PIN, GPIO_PIN_RESET);
        break;
    }
}

/* ── Trace helper ──────────────────────────────────────────────────────────── */
static void Trace_Print(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, (uint16_t)strlen(msg), 1000U);
}

/* ── UART RX interrupt callbacks ───────────────────────────────────────────── */
static void cmd_rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart7, &s_cmd_rx_byte, 1U);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        Modem_RxByte();
    } else if (huart->Instance == UART7) {
        RingBuf_Put(&s_cmd_rx, s_cmd_rx_byte);
        cmd_rx_arm();
    }
}

/* ── HAL timebase callback ─────────────────────────────────────────────────── */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) { HAL_IncTick(); }
}

/* ── Error_Handler ─────────────────────────────────────────────────────────── */
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

/* ── SystemClock_Config ────────────────────────────────────────────────────── */
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
