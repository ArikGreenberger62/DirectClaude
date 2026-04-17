/* main.h — ThreadX blink project */
#ifndef MAIN_H
#define MAIN_H

#include "stm32h5xx_hal.h"

/* LED_R  → PC8  (driven by ThreadX software timer) */
#define LED_R_PIN        GPIO_PIN_8
#define LED_R_PORT       GPIOC
/* LED_G  → PC9  (driven by dedicated ThreadX thread) */
#define LED_G_PIN        GPIO_PIN_9
#define LED_G_PORT       GPIOC
/* P3V3_SW_EN → PC11 (must be HIGH to power the 3.3V rail) */
#define P3V3_SW_EN_PIN   GPIO_PIN_11
#define P3V3_SW_EN_PORT  GPIOC

/* UART7 trace port (PE7=RX, PE8=TX, AF7, 115200 8N1) — host COM7 */
extern UART_HandleTypeDef huart7;

void Error_Handler(void);

#endif /* MAIN_H */
