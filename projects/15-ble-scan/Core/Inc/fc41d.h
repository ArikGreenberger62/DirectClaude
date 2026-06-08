/* fc41d.h — Quectel FC41D Wi-Fi+BLE module driver interface.
 *
 * UART9 (PD14=RX, PD15=TX, GPIO_AF11_UART9) @ 115200 8N1 — AT command port
 * Power pins:
 *   WIFI_BLE_PWR_EN  (PD5) — HIGH to enable module power
 *   WIFI_BLE_RESETN  (PD6) — open-drain, HIGH to release from reset
 *   P3V3_SW_EN       (PC11) — already HIGH from MX_GPIO_Init
 */
#ifndef FC41D_H
#define FC41D_H

#include <stdint.h>
#include "stm32h5xx_hal.h"

/* ── Detection result ────────────────────────────────────────────────────── */
typedef enum {
    FC41D_RESULT_OK = 0,
    FC41D_RESULT_TIMEOUT,
} FC41D_Result_t;

/* ── BLE scan result ─────────────────────────────────────────────────────── */
#define BLE_SCAN_MAX_DEVICES  20U

typedef struct {
    char     addr[18];    /* "AA:BB:CC:DD:EE:FF\0" */
    uint8_t  addr_type;   /* 0=public, 1=random */
    uint8_t  rssi_valid;  /* 1 if rssi_dbm was present in URC, 0 if field was empty */
    int16_t  rssi_dbm;    /* signal strength in dBm; only meaningful when rssi_valid=1 */
    char     name[33];    /* Local Name from adv data, "" if absent */
} BLE_Device_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Power on module, arm UART9 RX interrupt.
 * Call once after MX_GPIO_Init and MX_UART9_Init. */
void FC41D_Init(void);

/* Poll AT commands until module responds (up to 10 s).
 * On success: sends AT+QVERSION and traces the version string.
 * Returns FC41D_RESULT_OK or FC41D_RESULT_TIMEOUT. */
FC41D_Result_t FC41D_Detect(void);

/* Scan for BLE devices for duration_ms milliseconds.
 * Stores up to max_devices unique entries (deduplicated by MAC).
 * Extracts device name from advertising data when available.
 * Returns number of unique devices found. */
uint8_t FC41D_BLE_Scan(BLE_Device_t *devices, uint8_t max_devices,
                        uint32_t duration_ms);

/* Connect to a BLE device and read its Device Name characteristic (GATT handle 3).
 * Writes into name_out[0..name_max-1]; disconnects before returning.
 * Call after FC41D_BLE_Scan() with scanning stopped (AT+QBLESCAN=0).
 * Returns 1 if a non-empty name was obtained, 0 on failure. */
int FC41D_BLE_GetName(const BLE_Device_t *dev, char *name_out, uint8_t name_max);

/* Call from HAL_UART_RxCpltCallback when huart->Instance == UART9. */
void FC41D_RxByte(void);

#endif /* FC41D_H */
