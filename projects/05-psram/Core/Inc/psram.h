/* psram.h — IS66WVS16M8FBLL PSRAM driver public API
 *
 * Hardware: SPI2 (CAN_SPI2) at 60 MHz, Mode 0 (CPOL=0, CPHA=0)
 * Chip select: PE13 (PSRAM_SPI2_NSS_Pin) — active LOW, software GPIO
 *
 * tCEM constraint: CS must return HIGH within 4 µs at 85°C.
 * Driver uses 20-byte data chunks per CS assertion.
 */
#ifndef PSRAM_H
#define PSRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ── Status ──────────────────────────────────────────────────────────────── */
typedef enum
{
    PSRAM_OK  = 0,
    PSRAM_ERR = 1
} PSRAM_Status_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief Power-up and identify the PSRAM.
 *        Issues RESET_EN + RESET, waits 1 ms, reads ID register.
 * @return PSRAM_OK if MFR=0x9D and KGD=0x5D (PASS), PSRAM_ERR otherwise.
 */
PSRAM_Status_t PSRAM_Init(void);

/**
 * @brief Write bytes to PSRAM address.
 *        Internally chunks the transfer to satisfy the 4 µs tCEM limit.
 * @param addr  24-bit byte address (0x000000 – 0xFFFFFF)
 * @param buf   pointer to data
 * @param len   number of bytes
 */
PSRAM_Status_t PSRAM_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief Read bytes from PSRAM address (FAST READ, 1 dummy byte).
 *        Internally chunks the transfer to satisfy the 4 µs tCEM limit.
 * @param addr  24-bit byte address
 * @param buf   destination buffer
 * @param len   number of bytes
 */
PSRAM_Status_t PSRAM_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/** @brief Returns the raw MFR and KGD bytes received during the last PSRAM_Init call. */
void PSRAM_GetLastID(uint8_t *mfr, uint8_t *kgd);

#ifdef __cplusplus
}
#endif

#endif /* PSRAM_H */
