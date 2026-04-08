/* self_test.c — startup self-test for 01-blink
 * Compiled only when -DSELF_TEST is passed to the compiler.
 */
#ifdef SELF_TEST

#include "main.h"
#include "self_test.h"
#include <stdio.h>
#include <string.h>

static void st_report(const char *module, int pass, const char *reason)
{
    char buf[96];
    if (pass) {
        snprintf(buf, sizeof(buf), "[SELF_TEST] %-14s PASS\r\n", module);
    } else {
        snprintf(buf, sizeof(buf), "[SELF_TEST] %-14s FAIL - %s\r\n", module, reason);
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)strlen(buf), 100);
}

/* ── Test: system clock ─────────────────────────────────────────────────── */
static int test_sysclk(void)
{
    /* Expect 240 MHz ± 1 MHz */
    uint32_t clk = HAL_RCC_GetSysClockFreq();
    return (clk >= 239000000UL && clk <= 241000000UL);
}

/* ── Test: RAM write/read ───────────────────────────────────────────────── */
static int test_ram(void)
{
    static volatile uint8_t scratch[64];
    for (int i = 0; i < 64; i++) scratch[i] = (uint8_t)(0xA5 ^ i);
    for (int i = 0; i < 64; i++) {
        if (scratch[i] != (uint8_t)(0xA5 ^ i)) return 0;
    }
    return 1;
}

/* ── Test: P3V3_SW_EN is HIGH ───────────────────────────────────────────── */
static int test_p3v3(void)
{
    return (HAL_GPIO_ReadPin(P3V3_SW_EN_PORT, P3V3_SW_EN_PIN) == GPIO_PIN_SET);
}

/* ── Test: TIM3 running ─────────────────────────────────────────────────── */
static int test_tim3(void)
{
    /* TIM3 PWM must have been started before SelfTest_Run() is called */
    return ((TIM3->CR1 & TIM_CR1_CEN) != 0U);
}

/* ── Public entry ───────────────────────────────────────────────────────── */
void SelfTest_Run(void)
{
    int all_pass = 1;
    int r;

    const char *banner = "\r\n=== 01-blink SELF_TEST ===\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)banner, (uint16_t)strlen(banner), 100);

    r = test_sysclk(); st_report("SYSCLK",    r, "not 240 MHz");   all_pass &= r;
    r = test_ram();    st_report("RAM",        r, "pattern mismatch"); all_pass &= r;
    r = test_p3v3();   st_report("P3V3_SW_EN", r, "pin not HIGH"); all_pass &= r;
    r = test_tim3();   st_report("TIM3",       r, "counter not running"); all_pass &= r;

    const char *result = all_pass
        ? "[SELF_TEST] OVERALL: PASS\r\n\r\n"
        : "[SELF_TEST] OVERALL: FAIL — halting\r\n\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)result, (uint16_t)strlen(result), 100);

    if (!all_pass) {
        /* Halt — do not start application */
        while (1) {
            HAL_GPIO_TogglePin(LED_R_PORT, LED_R_PIN);
            HAL_Delay(100);
        }
    }
}

#endif /* SELF_TEST */
