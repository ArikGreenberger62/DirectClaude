/* main.c — 04-gsensor
 *
 * Reads LSM6DSO accelerometer via SPI1, performs two-position rotation-matrix
 * calibration, then traces raw and calibrated XYZ at 1 Hz on UART7.
 *
 * Peripheral init (SystemClock_Config, MX_GPIO_Init, MX_GPDMA1_Init,
 * MX_SPI1_Init, MX_UART7_Init) comes from LowLevel CubeMX-generated code.
 * LED_R (PC8) and LED_G (PC9) are not in LowLevel gpio.c — initialized here.
 *
 * Calibration sequence:
 *   1. Board stable → sample position 1 automatically.
 *   2. Red LED blinks fast (5 Hz) → cue user to rotate board 90°.
 *   3. Board settles at new position → sample position 2.
 *   4. Compute rotation matrix.
 *   5. OK  → green LED blinks (1 Hz), trace starts.
 *      FAIL → LEDs off, retry after 3 s.
 *
 * Trace format (UART7, 115200 8N1, once per second):
 *   RAW  Xr=±NNNNN Yr=±NNNNN Zr=±NNNNN  (raw/4, 1024=1G)
 *   CAL  Xc=±NNNNN Yc=±NNNNN Zc=±NNNNN  (after rotation matrix, 1024=1G)
 *
 * MCU : STM32H573VIT3Q @ 240 MHz (HSE 12 MHz → PLL1 M=1 N=40 P=2)
 * SPI1: PLL1Q=120 MHz / 16 = 7.5 MHz, Mode 3 (CPOL=1, CPHA=1)
 */

#include "main.h"
#include "gpdma.h"
#include "gpio.h"
#include "spi.h"
#include "usart.h"
#include "lsm6dso.h"
#include "calibration.h"
#include <stdio.h>
#include <string.h>

/* ── Private constants ───────────────────────────────────────────────────── */
#define LED_RED_FAST_PERIOD_MS    100U   /* 5 Hz (50 ms on / 50 ms off) */
#define LED_GREEN_BLINK_PERIOD_MS 500U   /* 1 Hz */
#define ACCEL_POLL_PERIOD_MS      10U    /* 100 Hz sensor polling */
#define TRACE_PERIOD_MS           1000U  /* 1 Hz trace output */
#define RETRY_DELAY_MS            3000U  /* 3 s before retry after failure */

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void LED_Init(void);
static void Trace_Print(const char *msg, uint16_t len);
static void Led_RedFastBlink(void);
static void Led_GreenBlink(void);
static void Led_AllOff(void);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* HAL init — uses TIM4 as timebase (stm32h5xx_hal_timebase_tim.c) */
    HAL_Init();
    SystemClock_Config();

    /* GPIO: P3V3_SW_EN (PC11) driven HIGH, all NSS lines deasserted.
     * LED_R/LED_G are not in LowLevel gpio.c — add them via LED_Init. */
    MX_GPIO_Init();
    LED_Init();

    /* Allow 3.3V rail and LSM6DSO power-on to settle */
    HAL_Delay(100U);

    /* DMA must be initialised before SPI (MspInit links channels to SPI1) */
    MX_GPDMA1_Init();

    /* SPI1 in Mode 3 at 7.5 MHz (our modified spi.c).
     * MspInit selects PLL1Q (120 MHz) as SPI1 kernel clock via CCIPR3. */
    MX_SPI1_Init();

    /* LowLevel MspInit links DMA channels and enables SPI1 IRQ.
     * We use polling only — disable IRQ and unlink DMA to prevent
     * HAL_SPI_IRQHandler from interfering with HAL_SPI_TransmitReceive. */
    HAL_NVIC_DisableIRQ(SPI1_IRQn);
    HAL_NVIC_DisableIRQ(GPDMA1_Channel1_IRQn);  /* SPI1 RX DMA */
    HAL_NVIC_DisableIRQ(GPDMA1_Channel2_IRQn);  /* SPI1 TX DMA */
    hspi1.hdmatx = NULL;
    hspi1.hdmarx = NULL;

    /* UART7: 115200 8N1, PE7=RX / PE8=TX, AF7 */
    MX_UART7_Init();

    /* ── Startup banner ─────────────────────────────────────────────────── */
    const char banner[] = "\r\n[GSENSOR] 04-gsensor starting — LSM6DSO ±8G, 104 Hz\r\n";
    Trace_Print(banner, (uint16_t)strlen(banner));

    /* ── Register dump: verify SPI1 clock and GPIO configuration ─────────── */
    {
        char dbg[128];
        int  n;

        /* RCC APB2ENR — bit 12 = SPI1EN, must be 1 */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] APB2ENR=0x%08lX (SPI1EN=%lu)\r\n",
                     (unsigned long)RCC->APB2ENR,
                     (unsigned long)((RCC->APB2ENR >> 12U) & 1U));
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        /* CCIPR3 bits [5:3] = SPI1SEL.
         * MspInit sets PLL1Q → expect 0b001 = 1 (PLL1Q = 120 MHz / 16 = 7.5 MHz) */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] CCIPR3=0x%08lX (SPI1SEL=%lu, 1=PLL1Q wanted)\r\n",
                     (unsigned long)RCC->CCIPR3,
                     (unsigned long)((RCC->CCIPR3 >> 3U) & 7U));
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        /* GPIOA MODER — PA5/PA6/PA7 bits [15:10] must be 10 10 10 (AF) */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] GPIOA MODER=0x%08lX AFRL=0x%08lX\r\n",
                     (unsigned long)GPIOA->MODER,
                     (unsigned long)GPIOA->AFR[0]);
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        /* SPI1 registers */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] SPI1 CR1=0x%08lX CFG1=0x%08lX CFG2=0x%08lX SR=0x%08lX\r\n",
                     (unsigned long)SPI1->CR1,
                     (unsigned long)SPI1->CFG1,
                     (unsigned long)SPI1->CFG2,
                     (unsigned long)SPI1->SR);
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }
    }

    /* ── Verify and configure LSM6DSO ───────────────────────────────────── */
    LSM6DSO_Status_t init_rc = LSM6DSO_Init(&hspi1);

    /* Always print register readback so we can see if writes took effect.
     * CTRL3_C should be 0x44 (BDU+IF_INC), CTRL1_XL should be 0x6C (104Hz ±8G).
     * If either reads 0x00 the SPI write did not reach the sensor. */
    {
        uint8_t ctrl3c = 0U, ctrl1xl = 0U;
        LSM6DSO_ReadReg(&hspi1, 0x12U, &ctrl3c);
        LSM6DSO_ReadReg(&hspi1, 0x10U, &ctrl1xl);
        char dbg[96];
        int  n = snprintf(dbg, sizeof(dbg),
            "[REG] CTRL3_C=0x%02X (want 0x44), CTRL1_XL=0x%02X (want 0x6C)\r\n",
            (unsigned)ctrl3c, (unsigned)ctrl1xl);
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }
    }

    if (init_rc != LSM6DSO_OK)
    {
        const char err[] =
            "[GSENSOR] ERROR: LSM6DSO init failed (WHO_AM_I mismatch, SPI fault, "
            "or CTRL1_XL readback mismatch — see [REG] line above)\r\n";
        Trace_Print(err, (uint16_t)strlen(err));
        Error_Handler();
    }

    const char ok[] = "[GSENSOR] LSM6DSO OK — WHO_AM_I=0x6C, ±8G, 104 Hz\r\n";
    Trace_Print(ok, (uint16_t)strlen(ok));

    /* ── Calibration announcement ───────────────────────────────────────── */
    const char cal_start[] =
        "[CAL] Starting calibration. Place board stable — sampling position 1...\r\n";
    Trace_Print(cal_start, (uint16_t)strlen(cal_start));

    Calib_Init();

    /* ── Tick timestamps ────────────────────────────────────────────────── */
    uint32_t t_poll  = HAL_GetTick();
    uint32_t t_trace = HAL_GetTick();
    uint32_t t_debug = HAL_GetTick();  /* 2 Hz diagnostic in WAIT_MOVE */
    uint32_t t_fail  = 0U;

    /* ── Main loop ──────────────────────────────────────────────────────── */
    while (1)
    {
        uint32_t now = HAL_GetTick();
        CalState_t st = Calib_GetState();

        /* ── LED management ─────────────────────────────────────────────── */
        if (st == CAL_STATE_WAIT_MOVE)
        {
            Led_RedFastBlink();
        }
        else if (st == CAL_STATE_DONE)
        {
            Led_GreenBlink();
        }
        else
        {
            Led_AllOff();
        }

        /* ── Poll accelerometer at 100 Hz ───────────────────────────────── */
        if ((now - t_poll) >= ACCEL_POLL_PERIOD_MS)
        {
            t_poll = now;

            int16_t rx, ry, rz;
            if (LSM6DSO_ReadAccel(&hspi1, &rx, &ry, &rz) == LSM6DSO_OK)
            {
                if (st != CAL_STATE_DONE && st != CAL_STATE_FAILED)
                {
                    Calib_Feed(rx, ry, rz);

                    CalState_t new_st = Calib_GetState();

                    if (st == CAL_STATE_SAMPLE_POS1 && new_st == CAL_STATE_WAIT_MOVE)
                    {
                        const char m[] =
                            "[CAL] Position 1 captured. RED LED fast blink — "
                            "rotate board 90 degrees now.\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                    }
                    else if (st == CAL_STATE_WAIT_MOVE && new_st == CAL_STATE_WAIT_STABLE2)
                    {
                        const char m[] =
                            "[CAL] Movement detected — hold board still at new position.\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                    }
                    else if (st == CAL_STATE_WAIT_STABLE2 && new_st == CAL_STATE_SAMPLE_POS2)
                    {
                        const char m[] =
                            "[CAL] Board stable — sampling position 2...\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                    }
                    else if (st == CAL_STATE_SAMPLE_POS2 && new_st == CAL_STATE_COMPUTE)
                    {
                        const char m[] =
                            "[CAL] Position 2 captured. Computing rotation matrix...\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                        /* Trigger COMPUTE → DONE/FAILED immediately */
                        Calib_Feed(rx, ry, rz);
                        new_st = Calib_GetState();
                    }

                    if (new_st == CAL_STATE_DONE)
                    {
                        int32_t cx, cy, cz;
                        Calib_Apply(rx, ry, rz, &cx, &cy, &cz);

                        const char pass[] = "[CAL] PASS — rotation matrix computed.\r\n";
                        Trace_Print(pass, (uint16_t)strlen(pass));

                        char buf[96];
                        int n = snprintf(buf, sizeof(buf),
                            "[CAL] Self-test now: Xc=%ld Yc=%ld Zc=%ld\r\n",
                            (long)cx, (long)cy, (long)cz);
                        if (n > 0) { Trace_Print(buf, (uint16_t)n); }

                        const char hint[] =
                            "[CAL] Z~+1024 when flat. GREEN blink, trace 1/s.\r\n";
                        Trace_Print(hint, (uint16_t)strlen(hint));
                    }
                    else if (new_st == CAL_STATE_FAILED)
                    {
                        const char m[] =
                            "[CAL] FAIL — vectors not orthogonal enough "
                            "(did not rotate ~90 deg?). Retrying in 3 s...\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                        t_fail = now;
                    }
                }
            }
        }

        /* ── WAIT_MOVE diagnostic: print raw readings every 2 s ─────────── */
        if (st == CAL_STATE_WAIT_MOVE && (now - t_debug) >= 2000U)
        {
            t_debug = now;
            int16_t dx, dy, dz;
            if (LSM6DSO_ReadAccel(&hspi1, &dx, &dy, &dz) == LSM6DSO_OK)
            {
                char dbg[96];
                int n = snprintf(dbg, sizeof(dbg),
                    "[MOVE] rx=%d ry=%d rz=%d  (rotate board now)\r\n",
                    (int)dx, (int)dy, (int)dz);
                if (n > 0) { Trace_Print(dbg, (uint16_t)n); }
            }
            else
            {
                const char err[] = "[MOVE] ReadAccel FAILED\r\n";
                Trace_Print(err, (uint16_t)sizeof(err) - 1U);
            }
        }

        /* ── Retry after failure ────────────────────────────────────────── */
        if (st == CAL_STATE_FAILED && (now - t_fail) >= RETRY_DELAY_MS)
        {
            Calib_Restart();
            const char m[] =
                "[CAL] Restarting calibration — place board stable.\r\n";
            Trace_Print(m, (uint16_t)strlen(m));
            t_trace = now;
        }

        /* ── 1 Hz trace (calibration done only) ─────────────────────────── */
        if (st == CAL_STATE_DONE && (now - t_trace) >= TRACE_PERIOD_MS)
        {
            t_trace = now;

            int16_t rx, ry, rz;
            if (LSM6DSO_ReadAccel(&hspi1, &rx, &ry, &rz) == LSM6DSO_OK)
            {
                int32_t xr = (int32_t)rx >> 2;
                int32_t yr = (int32_t)ry >> 2;
                int32_t zr = (int32_t)rz >> 2;

                int32_t xc, yc, zc;
                Calib_Apply(rx, ry, rz, &xc, &yc, &zc);

                char buf[128];
                int n = snprintf(buf, sizeof(buf),
                    "RAW  Xr=%ld Yr=%ld Zr=%ld\r\n"
                    "CAL  Xc=%ld Yc=%ld Zc=%ld\r\n",
                    (long)xr, (long)yr, (long)zr,
                    (long)xc, (long)yc, (long)zc);
                if (n > 0) { Trace_Print(buf, (uint16_t)n); }
            }
        }
    }
}

/* ── LED_Init ────────────────────────────────────────────────────────────── */
/* LowLevel gpio.c does not configure PC8/PC9 (not in the IOC) — add here.   */
static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIOC clock already enabled by MX_GPIO_Init (P3V3_SW_EN is on PC11) */
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LED_R_PIN | LED_G_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_R_PORT, &GPIO_InitStruct);
}

/* ── Trace_Print ─────────────────────────────────────────────────────────── */
static void Trace_Print(const char *msg, uint16_t len)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, len, 1000U);
}

/* ── LED helpers ─────────────────────────────────────────────────────────── */
static void Led_RedFastBlink(void)
{
    uint32_t t     = HAL_GetTick() % (LED_RED_FAST_PERIOD_MS * 2U);
    GPIO_PinState s = (t < LED_RED_FAST_PERIOD_MS) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, s);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);
}

static void Led_GreenBlink(void)
{
    uint32_t t     = HAL_GetTick() % (LED_GREEN_BLINK_PERIOD_MS * 2U);
    GPIO_PinState s = (t < LED_GREEN_BLINK_PERIOD_MS) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, s);
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
}

static void Led_AllOff(void)
{
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);
}

/* ── HAL_TIM_PeriodElapsedCallback ──────────────────────────────────────── */
/* TIM4 is the HAL timebase source (stm32h5xx_hal_timebase_tim.c).
 * This callback is called from TIM4_IRQHandler and increments uwTick.      */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        HAL_IncTick();
    }
}

/* ── SystemClock_Config ──────────────────────────────────────────────────── */
/* Copied verbatim from LowLevel/Core/Src/main.c (CubeMX-generated).
 * HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK = 240 MHz
 * PLL1Q = 12 MHz / 1 * 40 / 4 = 120 MHz  (SPI1 kernel clock)
 * APB1 = 240 MHz, APB2 = 120 MHz, APB3 = 240 MHz                          */
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
