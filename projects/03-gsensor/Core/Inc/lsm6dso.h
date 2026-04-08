/* lsm6dso.h — LSM6DSO accelerometer driver interface
 *
 * SPI4-wire, software NSS, Mode 3 (CPOL=1, CPHA=1).
 * Configured for ±8G full scale, 104 Hz ODR.
 * Raw output: 16-bit signed, 1G ≈ 4096 counts at ±8G.
 */
#ifndef LSM6DSO_H
#define LSM6DSO_H

#include <stdint.h>
#include "stm32h5xx_hal.h"

/* ── WHO_AM_I ────────────────────────────────────────────────────────────── */
#define LSM6DSO_WHO_AM_I_REG   0x0FU
#define LSM6DSO_WHO_AM_I_VAL   0x6CU

/* ── Status ──────────────────────────────────────────────────────────────── */
typedef enum
{
    LSM6DSO_OK  = 0,
    LSM6DSO_ERR = 1
} LSM6DSO_Status_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief  Verify WHO_AM_I and configure sensor: ±8G, 104 Hz ODR, BDU enabled.
 * @param  hspi  SPI handle (SPI1, already initialised).
 * @retval LSM6DSO_OK on success, LSM6DSO_ERR on any fault.
 */
LSM6DSO_Status_t LSM6DSO_Init(SPI_HandleTypeDef *hspi);

/**
 * @brief  Read raw 16-bit accelerometer values for all three axes.
 * @param  hspi          SPI handle.
 * @param  x / y / z     Output raw signed counts (1G ≈ 4096 at ±8G).
 * @retval LSM6DSO_OK on success, LSM6DSO_ERR on SPI error.
 */
LSM6DSO_Status_t LSM6DSO_ReadAccel(SPI_HandleTypeDef *hspi,
                                    int16_t *x, int16_t *y, int16_t *z);

#endif /* LSM6DSO_H */
