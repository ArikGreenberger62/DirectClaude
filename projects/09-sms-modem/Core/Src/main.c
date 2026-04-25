/* main.c — 09-sms-modem
 *
 * Quectel EG915N modem driver on USART2 (PA2/PA3 + PD3/PD4 HW flow control).
 * Trace + command interface on UART7 (PE7=RX, PE8=TX, 115200 8N1).
 *
 * Power-on:
 *   P3V3_SW_EN (PC11) HIGH — 3.3V supply
 *   MC60_GNSS_PWR (PB8) HIGH — modem VCC
 *   MC60_PWRKEY (PD7) pulse — boot modem
 *
 * Trace commands (type into UART7 terminal at 115200):
 *   sendsms +<phone> , <message>
 *
 * Incoming SMS is printed to UART7:
 *   sms received from <phone> , <message>
 *
 * LED behaviour:
 *   RED fast blink (5 Hz)  — modem booting / initialising
 *   GREEN slow blink (1 Hz) — modem ready
 *   RED solid                — modem error
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
#define CMD_BUF_SIZE   256U
#define PHONE_MAX       32U
#define MSG_MAX        161U   /* 160 chars + NUL */

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
    HAL_Init();
    SystemClock_Config();

    /* GPIO, DMA, UARTs — LowLevel CubeMX-generated init */
    MX_GPIO_Init();
    LED_Init();
    MX_GPDMA1_Init();
    MX_GPDMA2_Init();
    MX_UART7_Init();
    MX_USART2_UART_Init();

    /* Initialise UART7 RX ring buffer and arm interrupt */
    RingBuf_Init(&s_cmd_rx);
    s_cmd_pos = 0U;
    cmd_rx_arm();

    /* Startup banner */
    Trace_Print("\r\n[SMS-MODEM] 09-sms-modem starting\r\n");
    Trace_Print("[SMS-MODEM] USART2=modem, UART7=trace/command, 115200 8N1\r\n");

    /* Modem init (blocking — may take up to ~90 s if network is slow) */
    Modem_Init();

    /* ── Main loop ─────────────────────────────────────────────────────────── */
    while (1)
    {
        /* 1. Process modem URCs (+CMT, etc.) */
        Modem_Process();

        /* 2. Drain UART7 RX — assemble command lines */
        uint8_t b;
        while (RingBuf_Get(&s_cmd_rx, &b)) {
            /* Echo character back to terminal */
            HAL_UART_Transmit(&huart7, &b, 1U, 100U);

            if (b == '\r' || b == '\n') {
                if (s_cmd_pos > 0U) {
                    s_cmd_line[s_cmd_pos] = '\0';
                    Trace_Print("\r\n");
                    process_cmd_line(s_cmd_line);
                    s_cmd_pos = 0U;
                }
            } else if (b == 0x08U || b == 0x7FU) {
                /* Backspace */
                if (s_cmd_pos > 0U) { s_cmd_pos--; }
            } else if (s_cmd_pos < (uint16_t)(CMD_BUF_SIZE - 1U)) {
                s_cmd_line[s_cmd_pos++] = (char)b;
            }
        }

        /* 3. Update status LEDs */
        update_leds(Modem_GetState());
    }
}

/* ── Command processor ─────────────────────────────────────────────────────── */
/* Expected format: "sendsms +<phone> , <message>"
 * Separator is " , " (space-comma-space). */
static void process_cmd_line(const char *line)
{
    /* Skip leading whitespace */
    while (*line == ' ') { line++; }

    if (strncmp(line, "sendsms", 7) != 0) {
        Trace_Print("[CMD] unknown command.\r\n");
        Trace_Print("[CMD] usage:   sendsms +<phone> , <message>\r\n");
        Trace_Print("[CMD] example: sendsms +972501234567 , Hello World\r\n");
        return;
    }
    line += 7;   /* skip "sendsms" */
    while (*line == ' ') { line++; }

    /* Find " , " separator */
    const char *sep = strstr(line, " , ");
    if (sep == NULL) {
        /* Try just "," as fallback */
        sep = strchr(line, ',');
        if (sep == NULL) {
            Trace_Print("[CMD] format: sendsms +<phone> , <message>\r\n");
            Trace_Print("[CMD] example: sendsms +972501234567 , Hello World\r\n");
            return;
        }
        /* sep points to ','; phone ends before sep, msg starts after */
        char phone[PHONE_MAX];
        size_t plen = (size_t)(sep - line);
        if (plen == 0U || plen >= PHONE_MAX) {
            Trace_Print("[CMD] phone number invalid\r\n");
            return;
        }
        strncpy(phone, line, plen);
        phone[plen] = '\0';
        /* trim trailing spaces from phone */
        int i = (int)plen - 1;
        while (i >= 0 && phone[i] == ' ') { phone[i--] = '\0'; }

        const char *msg = sep + 1;
        while (*msg == ' ') { msg++; }

        if (*phone == '\0' || *msg == '\0') {
            Trace_Print("[CMD] phone or message empty\r\n");
            return;
        }
        (void)Modem_SendSMS(phone, msg);
        return;
    }

    /* sep points to " , " */
    char phone[PHONE_MAX];
    size_t plen = (size_t)(sep - line);
    if (plen == 0U || plen >= PHONE_MAX) {
        Trace_Print("[CMD] phone number invalid\r\n");
        return;
    }
    strncpy(phone, line, plen);
    phone[plen] = '\0';
    /* trim trailing spaces */
    int i = (int)plen - 1;
    while (i >= 0 && phone[i] == ' ') { phone[i--] = '\0'; }

    const char *msg = sep + 3;   /* skip " , " */
    if (*phone == '\0' || *msg == '\0') {
        Trace_Print("[CMD] phone or message empty\r\n");
        return;
    }

    (void)Modem_SendSMS(phone, msg);
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

/* ── Trace helpers ─────────────────────────────────────────────────────────── */
static void Trace_Print(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, (uint16_t)strlen(msg), 1000U);
}


/* ── UART RX interrupt callbacks ───────────────────────────────────────────── */
static void cmd_rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart7, &s_cmd_rx_byte, 1U);
}

/* Override HAL weak callback — dispatches bytes from USART2 and UART7 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        Modem_RxByte();   /* reads s_rx_byte internally — pRxBuffPtr is stale at callback time */
        /* modem.c re-arms via Modem_RxByte → mdm_rx_arm() */
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
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK=240 MHz. Copied from LowLevel. */
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
