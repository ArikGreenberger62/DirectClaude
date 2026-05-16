/* fc41d.h — Quectel FC41D Wi-Fi+BLE module driver interface.
 *
 * Peripheral: UART9 (PD14=RX, PD15=TX, GPIO_AF11_UART9) @ 115200 8N1
 * Power pins:
 *   WIFI_BLE_PWR_EN  (PD5) — drive HIGH to enable module power
 *   WIFI_BLE_RESETN  (PD6) — open-drain, drive HIGH to release from reset
 *   P3V3_SW_EN       (PC11) — already HIGH from MX_GPIO_Init
 */
#ifndef FC41D_H
#define FC41D_H

#include <stdint.h>
#include "stm32h5xx_hal.h"

typedef enum {
    FC41D_RESULT_OK = 0,      /* module detected and responded to AT */
    FC41D_RESULT_TIMEOUT,     /* no response within timeout */
} FC41D_Result_t;

/* Power on the module and arm UART9 RX interrupt.
 * Call once after MX_GPIO_Init and MX_UART9_Init. */
void FC41D_Init(void);

/* Send AT commands to detect the module.
 * Polls every 500 ms for up to 10 s.
 * On success: sends AT+QVERSION and traces the version string.
 * Returns FC41D_RESULT_OK or FC41D_RESULT_TIMEOUT. */
FC41D_Result_t FC41D_Detect(void);

/* Call from HAL_UART_RxCpltCallback when huart->Instance == UART9.
 * Stores received byte into ring buffer and re-arms interrupt. */
void FC41D_RxByte(void);

#endif /* FC41D_H */
