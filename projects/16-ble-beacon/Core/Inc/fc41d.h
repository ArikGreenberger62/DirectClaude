/* fc41d.h — Quectel FC41D BLE Beacon driver interface.
 *
 * UART9 (PD14=RX, PD15=TX, GPIO_AF11_UART9) @ 115200 8N1 — AT command port
 * Power pins:
 *   WIFI_BLE_PWR_EN  (PD5) — HIGH to enable module power
 *   WIFI_BLE_RESETN  (PD6) — open-drain, HIGH to release from reset
 */
#ifndef FC41D_H
#define FC41D_H

#include <stdint.h>
#include "stm32h5xx_hal.h"

/* ── Result codes ────────────────────────────────────────────────────────── */
typedef enum {
    FC41D_RESULT_OK = 0,
    FC41D_RESULT_TIMEOUT,
    FC41D_RESULT_ERROR,
} FC41D_Result_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Power on module, arm UART9 RX interrupt.
 * Call once after MX_GPIO_Init and MX_UART9_Init. */
void FC41D_Init(void);

/* Poll AT commands until module responds (up to 10 s).
 * Sends AT+QVERSION and traces the version string on success.
 * Returns FC41D_RESULT_OK or FC41D_RESULT_TIMEOUT. */
FC41D_Result_t FC41D_Detect(void);

/* Configure BLE as peripheral and start advertising.
 *
 * name    — device name string, e.g. "CCoreAIBLE" (≤ 20 chars recommended)
 * adv_hex — advertising payload as a hex string (max 31 bytes = 62 hex chars)
 *           e.g. "020106 15FFFFFF 48656C6C6F2066726F6D2043436F72654149"
 *           (spaces are stripped automatically)
 *
 * Sequence: QBLEINIT=0 → QBLEINIT=2 → QBLENAME → QBLEADVPARAM → QBLEADVDATA
 *           → QBLEADVSTART
 *
 * Returns FC41D_RESULT_OK on success, FC41D_RESULT_ERROR if any step fails. */
FC41D_Result_t FC41D_BLE_BeaconStart(const char *name, const char *adv_hex);

/* Call from HAL_UART_RxCpltCallback when huart->Instance == UART9. */
void FC41D_RxByte(void);

#endif /* FC41D_H */
