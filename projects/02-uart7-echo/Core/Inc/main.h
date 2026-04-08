/* main.h — 02-uart7-echo project */
#ifndef MAIN_H
#define MAIN_H

#include "stm32h5xx_hal.h"

/* ── Pin definitions ─────────────────────────────────────────────────────── */
/* P3V3_SW_EN → PC11 → must be HIGH to power 3.3V rail */
#define P3V3_SW_EN_PIN   GPIO_PIN_11
#define P3V3_SW_EN_PORT  GPIOC

/* UART7 → PE7 (RX) / PE8 (TX)  AF7 */
#define UART7_RX_PIN     GPIO_PIN_7
#define UART7_TX_PIN     GPIO_PIN_8
#define UART7_GPIO_PORT  GPIOE

/* ── Echo buffer size ────────────────────────────────────────────────────── */
#define ECHO_BUF_SIZE    256U

/* ── Extern handles ──────────────────────────────────────────────────────── */
extern UART_HandleTypeDef huart7;

/* ── Prototypes ──────────────────────────────────────────────────────────── */
void Error_Handler(void);

#endif /* MAIN_H */
