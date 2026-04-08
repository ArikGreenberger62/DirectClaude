/* self_test.c — startup self-test template
 * Copy to Core/Src/self_test.c in each project.
 * Compiled only when -DSELF_TEST is defined.
 */
#ifdef SELF_TEST

#include "self_test.h"
#include "stm32h5xx_hal.h"
#include <stdio.h>
#include <string.h>

/* ── USART handle — set to the debug UART in main.c before calling SelfTest_Run() */
extern UART_HandleTypeDef huart1;

/* ── Internal helpers ───────────────────────────────────────────────────────── */
static void st_print(const char *module, int pass, const char *reason)
{
    char buf[128];
    if (pass) {
        snprintf(buf, sizeof(buf), "[SELF_TEST] %-12s PASS\r\n", module);
    } else {
        snprintf(buf, sizeof(buf), "[SELF_TEST] %-12s FAIL - %s\r\n", module, reason);
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)strlen(buf), 100);
}

/* ── Individual tests ───────────────────────────────────────────────────────── */
static int test_sysclk(void)
{
    /* Expected: 250 MHz for STM32H573 at max speed — adjust per project */
    uint32_t clk = HAL_RCC_GetSysClockFreq();
    return (clk >= 200000000UL);   /* at least 200 MHz */
}

static int test_ram(void)
{
    static volatile uint8_t scratch[64];
    const uint8_t pattern = 0xA5;
    for (int i = 0; i < 64; i++) scratch[i] = pattern;
    for (int i = 0; i < 64; i++) {
        if (scratch[i] != pattern) return 0;
    }
    return 1;
}

static int test_uart(void)
{
    const char *msg = "[SELF_TEST] UART         testing...\r\n";
    HAL_StatusTypeDef s = HAL_UART_Transmit(&huart1, (uint8_t *)msg,
                                             (uint16_t)strlen(msg), 100);
    return (s == HAL_OK);
}

/* ── Public entry point ─────────────────────────────────────────────────────── */
void SelfTest_Run(void)
{
    int all_pass = 1;
    int r;

    r = test_sysclk(); st_print("SYSCLK:",  r, "unexpected clock freq"); all_pass &= r;
    r = test_ram();    st_print("RAM:",     r, "write/read pattern mismatch"); all_pass &= r;
    r = test_uart();   st_print("UART:",    r, "HAL_UART_Transmit error"); all_pass &= r;

    /* ── Add project-specific tests here ── */

    if (!all_pass) {
        /* Blink error LED and halt — do not enter application */
        while (1) {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  /* adjust pin per board */
            HAL_Delay(200);
        }
    }
}

#endif /* SELF_TEST */
