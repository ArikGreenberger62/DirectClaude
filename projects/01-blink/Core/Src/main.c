/* main.c — 01-blink
 *
 * Blinks LED_R (PC8/TIM3_CH3) and LED_G (PC9/TIM3_CH4)
 * at 1 Hz, 50% duty cycle using TIM3 PWM.
 * P3V3_SW_EN (PC11) is driven HIGH to enable the 3.3V power rail.
 *
 * MCU : STM32H573VIT3Q @ 240 MHz
 * HAL : STM32Cube FW_H5 V1.6.0
 */

#include "main.h"
#include "self_test.h"

/* ── Peripheral handles ──────────────────────────────────────────────────── */
TIM_HandleTypeDef  htim3;
UART_HandleTypeDef huart1;

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_Init(void);

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* HAL init — sets SysTick to 1 ms, configures flash prefetch */
    HAL_Init();

    /* System clock: HSE 12 MHz → PLL → 240 MHz */
    SystemClock_Config();

    /* GPIO: P3V3_SW_EN must be HIGH before any other peripheral init */
    MX_GPIO_Init();

    /* USART1 for self-test reporting (115200 baud) */
    MX_USART1_Init();

    /* TIM3 PWM for LED blinking */
    MX_TIM3_Init();

    /* Start PWM on both channels — LEDs blink from here, no CPU needed */
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) { Error_Handler(); }
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4) != HAL_OK) { Error_Handler(); }

    /* Run startup self-test after all peripherals are running.
     * Self-test will halt on failure — it never returns in that case. */
    SelfTest_Run();

    /* Main loop — nothing to do, PWM runs in hardware */
    while (1)
    {
    }
}

/* ── SystemClock_Config ──────────────────────────────────────────────────── */
/* HSE 12 MHz → PLL1 (M=1, N=40, P=2) → SYSCLK = 240 MHz                   */
/* APB1 = 240 MHz, APB2 = 120 MHz, APB3 = 240 MHz                           */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* VOS0 required for 240 MHz operation */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState            = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLL1_SOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM            = 1U;
    RCC_OscInitStruct.PLL.PLLN            = 40U;   /* VCO = 12 * 40 = 480 MHz */
    RCC_OscInitStruct.PLL.PLLP            = 2U;   /* PLLP = 240 MHz → SYSCLK */
    RCC_OscInitStruct.PLL.PLLQ            = 4U;   /* PLLQ = 120 MHz → FDCAN  */
    RCC_OscInitStruct.PLL.PLLR            = 2U;
    RCC_OscInitStruct.PLL.PLLRGE          = RCC_PLL1_VCIRANGE_3;  /* 8–16 MHz input */
    RCC_OscInitStruct.PLL.PLLVCOSEL       = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN        = 0U;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                     | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    /* 5 wait states required for 240 MHz at VOS0 */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

/* ── MX_GPIO_Init ────────────────────────────────────────────────────────── */
/* Initialises P3V3_SW_EN (PC11) as push-pull output, set HIGH.              */
/* PC8/PC9 are configured as TIM3 AF by MX_TIM3_Init via HAL_TIM_MspInit.   */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* P3V3_SW_EN = PC11, HIGH to enable 3.3V rail */
    HAL_GPIO_WritePin(P3V3_SW_EN_PORT, P3V3_SW_EN_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = P3V3_SW_EN_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(P3V3_SW_EN_PORT, &GPIO_InitStruct);
}

/* ── MX_TIM3_Init ────────────────────────────────────────────────────────── */
/* TIM3: 1 Hz period, 50% duty on CH3 (LED_R) and CH4 (LED_G)               */
static void MX_TIM3_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef      sConfigOC    = {0};

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = BLINK_TIM_PSC;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = BLINK_TIM_ARR;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    /* HAL_TIM_PWM_Init calls HAL_TIM_PWM_MspInit (enables TIM3 clock + configures
       PC8/PC9 as AF2). Do NOT call HAL_TIM_Base_Init first — it would set the
       handle state to READY and cause PWM_Init to skip MspInit entirely. */
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) { Error_Handler(); }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) { Error_Handler(); }

    /* CH3 → LED_R: 50% duty, active high */
    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = BLINK_TIM_CCR;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) { Error_Handler(); }

    /* CH4 → LED_G: 50% duty, active high (same phase as LED_R) */
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) { Error_Handler(); }
}

/* ── MX_USART1_Init ──────────────────────────────────────────────────────── */
/* 115200 baud, 8N1, for self-test output. USART1 clock = APB2 = 120 MHz.   */
static void MX_USART1_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling        = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler        = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK) { Error_Handler(); }
}

/* ── Error_Handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
