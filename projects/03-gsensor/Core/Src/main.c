/* main.c — 03-gsensor
 *
 * Reads LSM6DSO accelerometer via SPI1, performs two-position rotation-matrix
 * calibration, then traces raw and calibrated XYZ at 1 Hz on USART1.
 *
 * Calibration sequence:
 *   1. Board stable → sample position 1 automatically.
 *   2. Red LED blinks fast (5 Hz) → cue user to rotate board 90°.
 *   3. Board settles at new position → sample position 2.
 *   4. Compute rotation matrix.
 *   5. OK  → green LED blinks (1 Hz), trace starts.
 *      FAIL → red LED stops, message printed, retry after 3 s.
 *
 * Trace format (UART7, 115200 8N1, once per second):
 *   RAW  Xr=±NNNNN Yr=±NNNNN Zr=±NNNNN  (raw/4, 1024=1G)
 *   CAL  Xc=±NNNNN Yc=±NNNNN Zc=±NNNNN  (after rotation matrix, 1024=1G)
 *
 * MCU : STM32H573VIT3Q @ 240 MHz
 * HAL : STM32Cube FW_H5 V1.6.0
 */

#include "main.h"
#include "lsm6dso.h"
#include "calibration.h"
#include <stdio.h>
#include <string.h>

/* ── Peripheral handles ──────────────────────────────────────────────────── */
SPI_HandleTypeDef  hspi1;
UART_HandleTypeDef huart7;

/* ── Private constants ───────────────────────────────────────────────────── */
/* Blink periods in ms */
#define LED_RED_FAST_PERIOD_MS   100U   /* 5 Hz (50 ms on / 50 ms off) */
#define LED_GREEN_BLINK_PERIOD_MS 500U  /* 1 Hz */
#define ACCEL_POLL_PERIOD_MS     10U    /* 100 Hz sensor polling */
#define TRACE_PERIOD_MS          1000U  /* 1 Hz trace output */
#define RETRY_DELAY_MS           3000U  /* 3 s before retry after failure */

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_UART7_Init(void);
static void Trace_Print(const char *msg, uint16_t len);
static void Led_RedFastBlink(void);
static void Led_GreenBlink(void);
static void Led_AllOff(void);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* P3V3 rail must be on before SPI/sensor power is valid.
     * HAL_Delay requires SysTick, which HAL_Init() already starts above. */
    MX_GPIO_Init();
    HAL_Delay(100U);    /* 100 ms for 3.3V rail + LSM6DSO power-on settle */
    MX_UART7_Init();

    /* SPI1 kernel clock = PCLK2 (APB2 = 120 MHz).
     * CCIPR3 bits [5:3] = SPI1SEL; reset default may point to PLL2P/PLL3P
     * which are not configured — BRG would be clockless, shift reg stalls. */
    MODIFY_REG(RCC->CCIPR3, (7UL << 3U), (0UL << 3U));

    MX_SPI1_Init();

    /* ── Startup banner ─────────────────────────────────────────────────── */
    const char banner[] = "\r\n[GSENSOR] 03-gsensor starting — LSM6DSO ±8G, 104 Hz\r\n";
    Trace_Print(banner, (uint16_t)strlen(banner));

    /* ── Register dump: verify SPI1 and GPIOA configuration ────────────── */
    {
        char dbg[128];
        int  n;

        /* RCC APB2ENR — bit 12 = SPI1EN, must be 1 */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] APB2ENR=0x%08lX (SPI1EN=%lu)\r\n",
                     (unsigned long)RCC->APB2ENR,
                     (unsigned long)((RCC->APB2ENR >> 12U) & 1U));
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        /* CCIPR3 bits [5:3] = SPI1SEL: 000=PCLK2, 001=PLL2P, 010=PLL3P …
         * Must be 000 (PCLK2=APB2=120 MHz).  Any other value means the BRG
         * has no clock source — shift register never shifts, TXC never sets. */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] CCIPR3=0x%08lX (SPI1SEL=%lu, 0=PCLK2 wanted)\r\n",
                     (unsigned long)RCC->CCIPR3,
                     (unsigned long)((RCC->CCIPR3 >> 3U) & 7U));
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        /* GPIOA MODER — PA5/PA6/PA7 bits [15:10] must be 10 10 10 (AF) */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] GPIOA MODER=0x%08lX AFRL=0x%08lX\r\n",
                     (unsigned long)GPIOA->MODER,
                     (unsigned long)GPIOA->AFR[0]);
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }

        /* SPI1 registers — SR bit1=TXP (should be 1 after enable) */
        n = snprintf(dbg, sizeof(dbg),
                     "[REG] SPI1 CR1=0x%08lX CFG1=0x%08lX CFG2=0x%08lX SR=0x%08lX\r\n",
                     (unsigned long)SPI1->CR1,
                     (unsigned long)SPI1->CFG1,
                     (unsigned long)SPI1->CFG2,
                     (unsigned long)SPI1->SR);
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }
    }

    /* ── GPIO bit-bang SPI (Mode 3: CPOL=1, CPHA=1) ─────────────────────────
     * Temporarily reconfigures PA5(SCK)/PA6(MISO)/PA7(MOSI) as plain GPIO,
     * manually clocks out WHO_AM_I read, then restores AF5.
     * If this returns 0x6C the hardware is fine; if 0x00 there is a board fault.
     * PA5/PA6/PA7 MODER bits: [11:10]=PA5, [13:12]=PA6, [15:14]=PA7.
     * MODER values: 00=input, 01=output, 10=AF, 11=analog.                  */
    {
        char    dbg[128];
        int     n;
        uint8_t rx_addr = 0U, rx_data = 0U;

        /* Disable SPI before touching pins */
        SPI1->CR1 &= ~SPI_CR1_SPE;

        /* Pre-load ODR: SCK=HIGH (Mode 3 idle), MOSI=HIGH, before switching */
        GPIOA->BSRR = GPIO_PIN_5 | GPIO_PIN_7;

        /* PA5=output(01), PA6=input(00), PA7=output(01) */
        GPIOA->MODER = (GPIOA->MODER
                        & ~((3U << 10U) | (3U << 12U) | (3U << 14U)))
                       |  ((1U << 10U) | (0U << 12U) | (1U << 14U));

        /* Assert NSS */
        HAL_GPIO_WritePin(ACC_NSS_PORT, ACC_NSS_PIN, GPIO_PIN_RESET);
        __NOP(); __NOP(); __NOP(); __NOP();

        /* ── Byte 0: send 0x8F (WHO_AM_I | READ), capture dummy ─────────── */
        for (int8_t bit = 7; bit >= 0; bit--)
        {
            /* Set MOSI before falling edge */
            if (0x8FU & (1U << (uint8_t)bit)) {
                GPIOA->BSRR = GPIO_PIN_7;
            } else {
                GPIOA->BSRR = (uint32_t)GPIO_PIN_7 << 16U;
            }
            /* Falling edge (leading edge in Mode 3) */
            GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16U;
            __NOP(); __NOP(); __NOP(); __NOP();
            /* Rising edge — sample MISO */
            GPIOA->BSRR = GPIO_PIN_5;
            if (GPIOA->IDR & GPIO_PIN_6) { rx_addr |= (uint8_t)(1U << (uint8_t)bit); }
        }

        /* ── Byte 1: send 0xFF dummy, capture WHO_AM_I response ─────────── */
        for (int8_t bit = 7; bit >= 0; bit--)
        {
            GPIOA->BSRR = GPIO_PIN_7;  /* MOSI = 1 (0xFF) */
            /* Falling edge */
            GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16U;
            __NOP(); __NOP(); __NOP(); __NOP();
            /* Rising edge — sample MISO */
            GPIOA->BSRR = GPIO_PIN_5;
            if (GPIOA->IDR & GPIO_PIN_6) { rx_data |= (uint8_t)(1U << (uint8_t)bit); }
        }

        /* Deassert NSS */
        HAL_GPIO_WritePin(ACC_NSS_PORT, ACC_NSS_PIN, GPIO_PIN_SET);

        /* Restore PA5/PA6/PA7 to AF5 (MODER=10) */
        GPIOA->MODER = (GPIOA->MODER
                        & ~((3U << 10U) | (3U << 12U) | (3U << 14U)))
                       |  ((2U << 10U) | (2U << 12U) | (2U << 14U));

        /* Re-enable SPI */
        SPI1->CR1 |= SPI_CR1_SPE;

        n = snprintf(dbg, sizeof(dbg),
                     "[BB] bit-bang rx={0x%02X 0x%02X} WHO_AM_I=0x%02X (expect 0x6C)\r\n",
                     (unsigned)rx_addr, (unsigned)rx_data, (unsigned)rx_data);
        if (n > 0) { Trace_Print(dbg, (uint16_t)n); }
    }

    /* ── Verify and configure LSM6DSO ───────────────────────────────────── */
    if (LSM6DSO_Init(&hspi1) != LSM6DSO_OK)
    {
        const char err[] = "[GSENSOR] ERROR: LSM6DSO init failed (WHO_AM_I mismatch or SPI fault)\r\n";
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
    uint32_t t_fail  = 0U;

    /* ── Main loop ──────────────────────────────────────────────────────── */
    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* ── LED management ─────────────────────────────────────────────── */
        CalState_t st = Calib_GetState();

        if (st == CAL_STATE_WAIT_MOVE)
        {
            Led_RedFastBlink();
        }
        else if (st == CAL_STATE_DONE)
        {
            Led_GreenBlink();
        }
        else if (st == CAL_STATE_FAILED)
        {
            Led_AllOff();
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
                /* Feed into calibration state machine */
                if (st != CAL_STATE_DONE && st != CAL_STATE_FAILED)
                {
                    Calib_Feed(rx, ry, rz);

                    /* Detect state transitions and print messages */
                    CalState_t new_st = Calib_GetState();

                    if (st == CAL_STATE_SAMPLE_POS1 &&
                        new_st == CAL_STATE_WAIT_MOVE)
                    {
                        const char m[] =
                            "[CAL] Position 1 captured. RED LED fast blink — "
                            "rotate board 90 degrees now.\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                    }
                    else if (st == CAL_STATE_WAIT_MOVE &&
                             new_st == CAL_STATE_WAIT_STABLE2)
                    {
                        const char m[] =
                            "[CAL] Movement detected — hold board still at new position.\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                    }
                    else if (st == CAL_STATE_WAIT_STABLE2 &&
                             new_st == CAL_STATE_SAMPLE_POS2)
                    {
                        const char m[] =
                            "[CAL] Board stable — sampling position 2...\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                    }
                    else if (st == CAL_STATE_SAMPLE_POS2 &&
                             new_st == CAL_STATE_COMPUTE)
                    {
                        const char m[] = "[CAL] Position 2 captured. Computing rotation matrix...\r\n";
                        Trace_Print(m, (uint16_t)strlen(m));
                        /* Transition COMPUTE → DONE/FAILED happens immediately
                         * on next Calib_Feed call; send one more feed to trigger it */
                        Calib_Feed(rx, ry, rz);
                        new_st = Calib_GetState();
                    }

                    if (new_st == CAL_STATE_DONE)
                    {
                        /* Print calibration result: apply R to current reading,
                         * Z_cal should be ~+1024 when board is flat (Z down) */
                        int32_t cx, cy, cz;
                        Calib_Apply(rx, ry, rz, &cx, &cy, &cz);

                        const char pass[] = "[CAL] PASS — rotation matrix computed.\r\n";
                        Trace_Print(pass, (uint16_t)strlen(pass));

                        char buf[96];
                        int n = snprintf(buf, sizeof(buf),
                            "[CAL] Self-test now: Xc=%ld Yc=%ld Zc=%ld\r\n",
                            (long)cx, (long)cy, (long)cz);
                        if (n > 0)
                        {
                            Trace_Print(buf, (uint16_t)n);
                        }

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
                /* Raw (scaled to 1024 = 1G: divide by 4) */
                int32_t xr = (int32_t)rx >> 2;
                int32_t yr = (int32_t)ry >> 2;
                int32_t zr = (int32_t)rz >> 2;

                /* Calibrated (rotation matrix applied, also scaled 1024 = 1G) */
                int32_t xc, yc, zc;
                Calib_Apply(rx, ry, rz, &xc, &yc, &zc);

                char buf[128];
                int n = snprintf(buf, sizeof(buf),
                    "RAW  Xr=%ld Yr=%ld Zr=%ld\r\n"
                    "CAL  Xc=%ld Yc=%ld Zc=%ld\r\n",
                    (long)xr, (long)yr, (long)zr,
                    (long)xc, (long)yc, (long)zc);
                if (n > 0)
                {
                    Trace_Print(buf, (uint16_t)n);
                }
            }
        }
    }
}

/* ── Trace_Print ─────────────────────────────────────────────────────────── */
static void Trace_Print(const char *msg, uint16_t len)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, len, 1000U);
}

/* ── LED helpers ─────────────────────────────────────────────────────────── */
static void Led_RedFastBlink(void)
{
    uint32_t t = HAL_GetTick() % (LED_RED_FAST_PERIOD_MS * 2U);
    GPIO_PinState state = (t < LED_RED_FAST_PERIOD_MS) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, state);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);
}

static void Led_GreenBlink(void)
{
    uint32_t t = HAL_GetTick() % (LED_GREEN_BLINK_PERIOD_MS * 2U);
    GPIO_PinState state = (t < LED_GREEN_BLINK_PERIOD_MS) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, state);
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
}

static void Led_AllOff(void)
{
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);
}

/* ── SystemClock_Config ──────────────────────────────────────────────────── */
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK = 240 MHz                   */
/* APB1=240 MHz, APB2=120 MHz (SPI1 clock), APB3=240 MHz                    */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLL1_SOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 1U;
    RCC_OscInitStruct.PLL.PLLN       = 40U;
    RCC_OscInitStruct.PLL.PLLP       = 2U;
    RCC_OscInitStruct.PLL.PLLQ       = 4U;
    RCC_OscInitStruct.PLL.PLLR       = 2U;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN   = 0U;
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
}

/* ── MX_GPIO_Init ────────────────────────────────────────────────────────── */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* ── SPI1 NSS lines — all deasserted HIGH before anything else ────────
     * SPI1 is shared by LSM6DSO (PE9), SPI Flash (PE10), EEPROM (PB2).
     * If any of these float LOW, the chip will respond to SPI traffic and
     * corrupt every transaction.  De-assert all three immediately.         */
    HAL_GPIO_WritePin(ACC_NSS_PORT,   ACC_NSS_PIN,   GPIO_PIN_SET);
    HAL_GPIO_WritePin(FLASH_NSS_PORT, FLASH_NSS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(EEPROM_NSS_PORT,EEPROM_NSS_PIN,GPIO_PIN_SET);

    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = ACC_NSS_PIN | FLASH_NSS_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = EEPROM_NSS_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ── P3V3_SW_EN = PC11, HIGH to enable 3.3V rail ─────────────────────── */
    HAL_GPIO_WritePin(P3V3_SW_EN_PORT, P3V3_SW_EN_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin   = P3V3_SW_EN_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(P3V3_SW_EN_PORT, &GPIO_InitStruct);

    /* ── LED_R = PC8, LED_G = PC9, both off ──────────────────────────────── */
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin   = LED_R_PIN | LED_G_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ── MX_SPI1_Init ────────────────────────────────────────────────────────── */
/* SPI1 master, Mode 3 (CPOL=1/CPHA=1), 8-bit, software NSS.                */
/* Clock: APB2 = 120 MHz / 16 = 7.5 MHz (≤ 10 MHz max of LSM6DSO).         */
static void MX_SPI1_Init(void)
{
    hspi1.Instance                        = SPI1;
    hspi1.Init.Mode                       = SPI_MODE_MASTER;
    hspi1.Init.Direction                  = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize                   = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity                = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase                   = SPI_PHASE_2EDGE;
    hspi1.Init.NSS                        = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler          = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit                   = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode                     = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation             = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial              = 0x7U;
    hspi1.Init.CRCLength                  = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode                   = SPI_NSS_PULSE_DISABLE;
    hspi1.Init.NSSPolarity                = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold              = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.MasterSSIdleness           = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness    = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp     = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState          = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi1.Init.IOSwap                     = SPI_IO_SWAP_DISABLE;
    hspi1.Init.ReadyMasterManagement      = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
    hspi1.Init.ReadyPolarity              = SPI_RDY_POLARITY_HIGH;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) { Error_Handler(); }
}

/* ── MX_UART7_Init ───────────────────────────────────────────────────────── */
/* 115200 baud, 8N1, blocking TX only. UART7 clock = APB3 = 240 MHz.        */
/* PE7 = RX, PE8 = TX, AF7 — configured in HAL_UART_MspInit.                */
static void MX_UART7_Init(void)
{
    huart7.Instance                    = UART7;
    huart7.Init.BaudRate               = 115200U;
    huart7.Init.WordLength             = UART_WORDLENGTH_8B;
    huart7.Init.StopBits               = UART_STOPBITS_1;
    huart7.Init.Parity                 = UART_PARITY_NONE;
    huart7.Init.Mode                   = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart7.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart7.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart7.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart7.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart7) != HAL_OK) { Error_Handler(); }
}

/* ── Error_Handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
