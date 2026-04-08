/* main.h — 01-blink project */
#ifndef MAIN_H
#define MAIN_H

#include "stm32h5xx_hal.h"

/* ── Pin definitions (from TFL_CONNECT_2_H573.ioc) ──────────────────────── */
/* LED_R → PC8  → TIM3_CH3 */
#define LED_R_PIN        GPIO_PIN_8
#define LED_R_PORT       GPIOC
/* LED_G → PC9  → TIM3_CH4 */
#define LED_G_PIN        GPIO_PIN_9
#define LED_G_PORT       GPIOC
/* P3V3_SW_EN → PC11 → GPIO_Output (must be HIGH to power 3.3V rail) */
#define P3V3_SW_EN_PIN   GPIO_PIN_11
#define P3V3_SW_EN_PORT  GPIOC
/* USART1 TX/RX → PA9/PA10 (RS485_USART1, used for debug/self-test) */
#define USART1_TX_PIN    GPIO_PIN_9
#define USART1_TX_PORT   GPIOA
#define USART1_RX_PIN    GPIO_PIN_10
#define USART1_RX_PORT   GPIOA

/* ── TIM3 PWM parameters for 1 Hz, 50% duty cycle ───────────────────────── */
/* TIM3 clock = APB1TimFreq = 240 MHz                                        */
/* PSC=23999 → tick = 240 MHz / 24000 = 10 kHz                               */
/* ARR=9999  → period = 10000 / 10 kHz = 1 s                                 */
/* CCR=5000  → duty = 5000 / 10000 = 50 %                                    */
#define BLINK_TIM_PSC    23999U
#define BLINK_TIM_ARR    9999U
#define BLINK_TIM_CCR    5000U

/* ── Extern handles ──────────────────────────────────────────────────────── */
extern TIM_HandleTypeDef  htim3;
extern UART_HandleTypeDef huart1;

/* ── Prototypes ──────────────────────────────────────────────────────────── */
void Error_Handler(void);

#endif /* MAIN_H */
