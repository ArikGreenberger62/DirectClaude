/* main.c — 05-psram
 *
 * Exercises the IS66WVS16M8FBLL-104K 128Mb PSRAM via SPI2 at 60 MHz.
 *
 * Peripheral init comes from LowLevel CubeMX-generated code.
 * LED_R (PC8) and LED_G (PC9) initialized here (not in LowLevel gpio.c).
 * PE13 (PSRAM_SPI2_NSS_Pin) initialized here as GPIO output (software CS).
 *
 * UART7 command interface (115200 8N1, PE7=RX / PE8=TX):
 *   WriteRAM(addr,len,hh hh ...)  — write hex bytes to PSRAM address
 *   ReadRAM(addr,len)             — read and print RAM_DATA(addr,len,hh hh ...)
 *   addr: hex without 0x prefix or with it (e.g. 001000 or 0x001000)
 *   len:  decimal
 *   bytes: space-separated hex pairs
 *
 * LED behaviour:
 *   GREEN blink (1 Hz): PSRAM identified and self-test passed
 *   RED   blink (1 Hz): PSRAM init or self-test failed
 *
 * MCU : STM32H573VIT3Q @ 240 MHz (HSE 12 MHz → PLL1 M=1 N=40 P=2)
 * SPI2: PLL1Q=120 MHz / 2 = 60 MHz, Mode 0 (CPOL=0, CPHA=0)
 */

#include "main.h"
#include "gpdma.h"
#include "gpio.h"
#include "spi.h"
#include "usart.h"
#include "psram.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Constants ───────────────────────────────────────────────────────────── */
#define LED_BLINK_PERIOD_MS  500U   /* 1 Hz */
#define CMD_BUF_SIZE         256U   /* max UART command line length */
#define MAX_CMD_BYTES         64U   /* max bytes per WriteRAM / ReadRAM */

/* ── UART RX state (filled from ISR) ────────────────────────────────────── */
static volatile uint8_t  g_rx_byte;
static          char     g_rx_work[CMD_BUF_SIZE]; /* ISR fills this */
static volatile uint16_t g_rx_idx;
static          char     g_cmd_buf[CMD_BUF_SIZE]; /* ready for main */
static volatile uint8_t  g_line_ready;

/* ── PSRAM status ────────────────────────────────────────────────────────── */
static uint8_t g_psram_ok;

/* ── Private prototypes ──────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_Init(void);
static void PSRAM_NSS_Init(void);
static void Trace_Print(const char *msg, uint16_t len);
static void process_command(void);
static void Led_GreenBlink(void);
static void Led_RedBlink(void);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* GPIO: P3V3_SW_EN (PC11) HIGH, NSS lines deasserted.
     * LED_R/LED_G and PE13 are not in LowLevel gpio.c. */
    MX_GPIO_Init();
    LED_Init();
    PSRAM_NSS_Init();   /* PE13 HIGH (CS deasserted) before SPI init */

    /* Allow 3.3V rail to settle */
    HAL_Delay(100U);

    /* DMA must be initialised before SPI (MspInit links channels to SPI2) */
    MX_GPDMA1_Init();
    MX_SPI2_Init();

    /* We use direct-register polling only — disable SPI2 IRQ and DMA links
     * so HAL_SPI_IRQHandler cannot interfere with our register-level transfers */
    HAL_NVIC_DisableIRQ(SPI2_IRQn);
    HAL_NVIC_DisableIRQ(GPDMA1_Channel5_IRQn);  /* SPI2 RX DMA */
    HAL_NVIC_DisableIRQ(GPDMA1_Channel6_IRQn);  /* SPI2 TX DMA */
    hspi2.hdmatx = NULL;
    hspi2.hdmarx = NULL;

    /* Workaround: HAL_SPI_Init prescaler write not taking effect on SPI2.
     * Force MBR=001 (div-4) via direct register write while SPE=0. */
    CLEAR_BIT(SPI2->CR1, SPI_CR1_SPE);
    MODIFY_REG(SPI2->CFG1, SPI_CFG1_MBR, SPI_BAUDRATEPRESCALER_4);

    /* UART7: 115200 8N1, PE7=RX / PE8=TX, AF7 */
    MX_UART7_Init();

    /* ── Startup banner ─────────────────────────────────────────────────── */
    const char banner[] =
        "\r\n[PSRAM] 05-psram starting\r\n"
        "[PSRAM] SPI2 60 MHz Mode0, CS=PE13, IS66WVS16M8FBLL 128Mb\r\n";
    Trace_Print(banner, (uint16_t)strlen(banner));

    /* Start UART7 interrupt RX — one byte at a time */
    HAL_UART_Receive_IT(&huart7, (uint8_t *)&g_rx_byte, 1U);

    /* ── Register dump: verify SPI2 clock selection and GPIO state ─────────── */
    {
        char dbg[160];
        int  n;
        /* CCIPR3 bits [11:9] = SPI2SEL; expect 001 = PLL1Q (120 MHz / 2 = 60 MHz) */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] CCIPR3=0x%08lX (SPI2SEL=%lu, 1=PLL1Q)\r\n",
                     (unsigned long)RCC->CCIPR3,
                     (unsigned long)((RCC->CCIPR3 >> 9U) & 7U));
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        n = snprintf(dbg, sizeof(dbg),
                     "[REG] SPI2 CR1=0x%08lX CFG1=0x%08lX CFG2=0x%08lX SR=0x%08lX\r\n",
                     (unsigned long)SPI2->CR1,
                     (unsigned long)SPI2->CFG1,
                     (unsigned long)SPI2->CFG2,
                     (unsigned long)SPI2->SR);
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        /* GPIOE ODR bit13: should be 1 (CS deasserted) */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] GPIOE ODR=0x%08lX (PE13=%lu, want 1=HIGH)\r\n",
                     (unsigned long)GPIOE->ODR,
                     (unsigned long)((GPIOE->ODR >> 13U) & 1U));
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }
    }

    /* ── PSRAM init and self-test ───────────────────────────────────────── */
    PSRAM_Status_t rc = PSRAM_Init();

    if (rc == PSRAM_OK)
    {
        const char ok[] = "[PSRAM] ID OK  MFR=0x9D KGD=0x5D (PASS)\r\n";
        Trace_Print(ok, (uint16_t)strlen(ok));

        /* Self-test: write pattern, read back, compare */
        static const uint8_t wbuf[8] =
            { 0xA1U, 0xB2U, 0xC3U, 0xD4U, 0xE5U, 0xF6U, 0x07U, 0x18U };
        uint8_t rbuf[8] = { 0 };

        PSRAM_Status_t wrc = PSRAM_Write(0x000000U, wbuf, 8U);
        PSRAM_Status_t rrc = PSRAM_Read(0x000000U, rbuf, 8U);

        if (wrc == PSRAM_OK && rrc == PSRAM_OK && memcmp(wbuf, rbuf, 8U) == 0)
        {
            const char st[] = "[PSRAM] SELF_TEST PASS — write/read verified\r\n";
            Trace_Print(st, (uint16_t)strlen(st));
            g_psram_ok = 1U;
        }
        else
        {
            const char st[] = "[PSRAM] SELF_TEST FAIL — write/read mismatch\r\n";
            Trace_Print(st, (uint16_t)strlen(st));
            g_psram_ok = 0U;
        }
    }
    else
    {
        uint8_t mfr = 0U, kgd = 0U;
        PSRAM_GetLastID(&mfr, &kgd);
        char fail[128];
        int fn = snprintf(fail, sizeof(fail),
            "[PSRAM] ERROR — ID check failed: rx MFR=0x%02X (want 0x9D) KGD=0x%02X (want 0x5D)\r\n",
            (unsigned)mfr, (unsigned)kgd);
        if (fn > 0) { Trace_Print(fail, (uint16_t)fn); }
        g_psram_ok = 0U;
    }

    const char help[] =
        "[PSRAM] Commands: WriteRAM(addr,len,hh hh ...) | ReadRAM(addr,len)\r\n"
        "[PSRAM] Example:  WriteRAM(0x001000,4,DE AD BE EF)\r\n"
        "[PSRAM] Example:  ReadRAM(0x001000,4)\r\n";
    Trace_Print(help, (uint16_t)strlen(help));

    /* ── Main loop ──────────────────────────────────────────────────────── */
    while (1)
    {
        if (g_psram_ok) { Led_GreenBlink(); }
        else            { Led_RedBlink();   }

        if (g_line_ready)
        {
            g_line_ready = 0U;
            process_command();
        }
    }
}

/* ── HAL_UART_RxCpltCallback — fills command line buffer from ISR ─────────── */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART7)
    {
        uint8_t b = g_rx_byte;

        if (b == (uint8_t)'\n' || b == (uint8_t)'\r')
        {
            /* End of line: copy work buffer to command buffer and signal main */
            if (g_rx_idx > 0U && !g_line_ready)
            {
                uint16_t idx = g_rx_idx;
                g_rx_work[idx] = '\0';
                (void)memcpy(g_cmd_buf, g_rx_work, (size_t)idx + 1U);
                g_rx_idx    = 0U;
                g_line_ready = 1U;
            }
        }
        else if (g_rx_idx < (uint16_t)(CMD_BUF_SIZE - 1U))
        {
            g_rx_work[g_rx_idx++] = (char)b;
        }

        /* Re-arm for next byte */
        (void)HAL_UART_Receive_IT(&huart7, (uint8_t *)&g_rx_byte, 1U);
    }
}

/* ── process_command ─────────────────────────────────────────────────────── */
static void process_command(void)
{
    char *line = g_cmd_buf;
    char  resp[512];
    int   n;

    /* ── WriteRAM(addr,len,hh hh ...) ───────────────────────────────────── */
    if (strncmp(line, "WriteRAM(", 9U) == 0)
    {
        char   *p    = line + 9U;
        char   *endp;
        uint32_t addr = (uint32_t)strtoul(p, &endp, 16);
        if (*endp != ',') { goto parse_err; }
        p = endp + 1U;
        uint32_t len  = (uint32_t)strtoul(p, &endp, 10);
        if (*endp != ',') { goto parse_err; }
        p = endp + 1U;

        if (len == 0U || len > MAX_CMD_BYTES)
        {
            n = snprintf(resp, sizeof(resp),
                         "[PSRAM] WriteRAM: len %lu out of range (1-%u)\r\n",
                         (unsigned long)len, (unsigned)MAX_CMD_BYTES);
            Trace_Print(resp, (uint16_t)n);
            return;
        }

        uint8_t data[MAX_CMD_BYTES];
        for (uint32_t i = 0U; i < len; i++)
        {
            data[i] = (uint8_t)strtoul(p, &endp, 16);
            p = endp;
            while (*p == ' ' || *p == ',') { p++; }
        }

        if (PSRAM_Write(addr, data, len) == PSRAM_OK)
        {
            n = snprintf(resp, sizeof(resp),
                         "[PSRAM] WriteRAM OK addr=0x%06lX len=%lu\r\n",
                         (unsigned long)addr, (unsigned long)len);
        }
        else
        {
            n = snprintf(resp, sizeof(resp), "[PSRAM] WriteRAM FAIL\r\n");
        }
        Trace_Print(resp, (uint16_t)n);
        return;
    }

    /* ── ReadRAM(addr,len) ───────────────────────────────────────────────── */
    if (strncmp(line, "ReadRAM(", 8U) == 0)
    {
        char   *p    = line + 8U;
        char   *endp;
        uint32_t addr = (uint32_t)strtoul(p, &endp, 16);
        if (*endp != ',') { goto parse_err; }
        p = endp + 1U;
        uint32_t len  = (uint32_t)strtoul(p, &endp, 10);

        if (len == 0U || len > MAX_CMD_BYTES)
        {
            n = snprintf(resp, sizeof(resp),
                         "[PSRAM] ReadRAM: len %lu out of range (1-%u)\r\n",
                         (unsigned long)len, (unsigned)MAX_CMD_BYTES);
            Trace_Print(resp, (uint16_t)n);
            return;
        }

        uint8_t data[MAX_CMD_BYTES];
        if (PSRAM_Read(addr, data, len) == PSRAM_OK)
        {
            /* Build: RAM_DATA(0xAAAAAA,NN,HH HH ... HH)\r\n */
            int off = snprintf(resp, sizeof(resp),
                               "RAM_DATA(0x%06lX,%lu,",
                               (unsigned long)addr, (unsigned long)len);
            for (uint32_t i = 0U; i < len; i++)
            {
                if (off < (int)(sizeof(resp) - 5))
                {
                    off += snprintf(resp + off, sizeof(resp) - (size_t)off,
                                    "%02X", (unsigned)data[i]);
                    if (i + 1U < len && off < (int)(sizeof(resp) - 4))
                    {
                        resp[off++] = ' ';
                    }
                }
            }
            if (off < (int)(sizeof(resp) - 3))
            {
                resp[off++] = ')';
                resp[off++] = '\r';
                resp[off++] = '\n';
                resp[off]   = '\0';
            }
            n = off;
        }
        else
        {
            n = snprintf(resp, sizeof(resp), "[PSRAM] ReadRAM FAIL\r\n");
        }
        Trace_Print(resp, (uint16_t)n);
        return;
    }

    /* ── Unknown command ─────────────────────────────────────────────────── */
    n = snprintf(resp, sizeof(resp),
                 "[PSRAM] Unknown: %.80s\r\n"
                 "[PSRAM] Use: WriteRAM(addr,len,hh hh ...) or ReadRAM(addr,len)\r\n",
                 line);
    Trace_Print(resp, (uint16_t)n);
    return;

parse_err:
    n = snprintf(resp, sizeof(resp), "[PSRAM] Parse error\r\n");
    Trace_Print(resp, (uint16_t)n);
}

/* ── LED_Init ────────────────────────────────────────────────────────────── */
static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIOC clock already enabled by MX_GPIO_Init (P3V3_SW_EN is PC11) */
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LED_R_PIN | LED_G_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_R_PORT, &GPIO_InitStruct);
}

/* ── PSRAM_NSS_Init ──────────────────────────────────────────────────────── */
/* PE13 is not configured in LowLevel gpio.c. Init as GPIO output HIGH (CS
 * deasserted). GPIOE clock is already enabled by MX_GPIO_Init.             */
static void PSRAM_NSS_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(PSRAM_NSS_PORT, PSRAM_NSS_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = PSRAM_NSS_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PSRAM_NSS_PORT, &GPIO_InitStruct);
}

/* ── Trace_Print ─────────────────────────────────────────────────────────── */
static void Trace_Print(const char *msg, uint16_t len)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, len, 1000U);
}

/* ── LED helpers ─────────────────────────────────────────────────────────── */
static void Led_GreenBlink(void)
{
    uint32_t t = HAL_GetTick() % (LED_BLINK_PERIOD_MS * 2U);
    GPIO_PinState s = (t < LED_BLINK_PERIOD_MS) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, s);
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
}

static void Led_RedBlink(void)
{
    uint32_t t = HAL_GetTick() % (LED_BLINK_PERIOD_MS * 2U);
    GPIO_PinState s = (t < LED_BLINK_PERIOD_MS) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, s);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);
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
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK=240 MHz
 * PLL1Q = 12/1*40/4 = 120 MHz (SPI2 kernel clock → / 2 = 60 MHz)          */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE
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
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }

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
    (void)file; (void)line;
}
#endif
