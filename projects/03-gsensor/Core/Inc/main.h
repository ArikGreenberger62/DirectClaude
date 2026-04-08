/* main.h — 03-gsensor project */
#ifndef MAIN_H
#define MAIN_H

#include "stm32h5xx_hal.h"

/* ── Pin definitions (from TFL_CONNECT_2_H573.ioc) ──────────────────────── */

/* P3V3_SW_EN → PC11 → GPIO_Output (must be HIGH to power 3.3V rail) */
#define P3V3_SW_EN_PIN    GPIO_PIN_11
#define P3V3_SW_EN_PORT   GPIOC

/* LED_R → PC8, LED_G → PC9 (GPIO direct control for variable blink rates) */
#define LED_R_PIN         GPIO_PIN_8
#define LED_R_PORT        GPIOC
#define LED_G_PIN         GPIO_PIN_9
#define LED_G_PORT        GPIOC

/* UART7: PE7 (RX) / PE8 (TX) → trace at 115200 baud  AF7 */
#define UART7_RX_PIN      GPIO_PIN_7
#define UART7_TX_PIN      GPIO_PIN_8
#define UART7_GPIO_PORT   GPIOE

/* SPI1: PA5 (SCK) / PA6 (MISO) / PA7 (MOSI) — AF5 */
#define SPI1_SCK_PIN      GPIO_PIN_5
#define SPI1_MISO_PIN     GPIO_PIN_6
#define SPI1_MOSI_PIN     GPIO_PIN_7
#define SPI1_GPIO_PORT    GPIOA

/* SPI_NSS_ACC   → PE9  → GPIO_Output, active-low CS for LSM6DSO */
#define ACC_NSS_PIN       GPIO_PIN_9
#define ACC_NSS_PORT      GPIOE
/* SPI_NSS_FLASH → PE10 → GPIO_Output, must be kept HIGH (deasserted) */
#define FLASH_NSS_PIN     GPIO_PIN_10
#define FLASH_NSS_PORT    GPIOE
/* SPI_NSS_EEPROM → PB2 → GPIO_Output, must be kept HIGH (deasserted) */
#define EEPROM_NSS_PIN    GPIO_PIN_2
#define EEPROM_NSS_PORT   GPIOB

/* ── Extern handles ──────────────────────────────────────────────────────── */
extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart7;

/* ── Prototypes ──────────────────────────────────────────────────────────── */
void Error_Handler(void);

#endif /* MAIN_H */
