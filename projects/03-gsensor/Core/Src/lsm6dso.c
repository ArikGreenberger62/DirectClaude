/* lsm6dso.c — LSM6DSO accelerometer SPI driver
 *
 * Hardware: SPI1, software NSS on PE9 (ACC_NSS_PIN / ACC_NSS_PORT).
 * SPI Mode 3: CPOL=1, CPHA=1, 8-bit, MSB first.
 * Max sensor SPI clock: 10 MHz — driver is initialised at 7.5 MHz (prescaler 16).
 *
 * Register map used:
 *   0x0F  WHO_AM_I         (expected 0x6C)
 *   0x10  CTRL1_XL         ODR=104Hz, FS=±8G
 *   0x12  CTRL3_C          BDU=1, IF_INC=1
 *   0x28  OUTX_L_A         start of 6-byte accelerometer burst
 */

#include "lsm6dso.h"
#include "main.h"   /* ACC_NSS_PIN, ACC_NSS_PORT */

/* ── Register addresses ──────────────────────────────────────────────────── */
#define REG_WHO_AM_I    0x0FU
#define REG_CTRL1_XL    0x10U
#define REG_CTRL3_C     0x12U
#define REG_OUTX_L_A    0x28U

/* SPI read flag: bit 7 = 1 */
#define SPI_READ_FLAG   0x80U

/* ── SPI timeout (ms) ────────────────────────────────────────────────────── */
#define SPI_TIMEOUT_MS  10U

/* ── SPI buffer sizes ────────────────────────────────────────────────────── */
/* Largest single transaction: 1 addr byte + 6 data bytes = 7 bytes total   */
#define SPI_BUF_SIZE  8U

/* ── Private helpers ─────────────────────────────────────────────────────── */

static void cs_low(void)
{
    HAL_GPIO_WritePin(ACC_NSS_PORT, ACC_NSS_PIN, GPIO_PIN_RESET);
}

static void cs_high(void)
{
    HAL_GPIO_WritePin(ACC_NSS_PORT, ACC_NSS_PIN, GPIO_PIN_SET);
}

/**
 * @brief  Write one byte to a sensor register.
 *         Uses HAL_SPI_TransmitReceive (required on STM32H5 SPI IP).
 */
static LSM6DSO_Status_t reg_write(SPI_HandleTypeDef *hspi,
                                   uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)(reg & ~SPI_READ_FLAG), val };
    uint8_t rx[2] = { 0U, 0U };

    cs_low();
    HAL_StatusTypeDef rc = HAL_SPI_TransmitReceive(hspi, tx, rx, 2U, SPI_TIMEOUT_MS);
    cs_high();

    return (rc == HAL_OK) ? LSM6DSO_OK : LSM6DSO_ERR;
}

/**
 * @brief  Read one or more consecutive registers.
 *         Full-duplex: send addr + N dummy bytes, capture response bytes [1..N].
 */
static LSM6DSO_Status_t reg_read(SPI_HandleTypeDef *hspi,
                                  uint8_t reg, uint8_t *buf, uint8_t len)
{
    /* +1 for the address byte */
    uint8_t tx[SPI_BUF_SIZE];
    uint8_t rx[SPI_BUF_SIZE];
    uint8_t total = (uint8_t)(len + 1U);

    if (total > SPI_BUF_SIZE) { return LSM6DSO_ERR; }

    tx[0] = (uint8_t)(reg | SPI_READ_FLAG);
    for (uint8_t i = 1U; i < total; i++) { tx[i] = 0xFFU; }

    cs_low();
    HAL_StatusTypeDef rc = HAL_SPI_TransmitReceive(hspi, tx, rx, total, SPI_TIMEOUT_MS);
    cs_high();

    if (rc != HAL_OK) { return LSM6DSO_ERR; }

    /* Data starts at rx[1] (rx[0] is the dummy received during addr phase) */
    for (uint8_t i = 0U; i < len; i++) { buf[i] = rx[i + 1U]; }

    return LSM6DSO_OK;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

LSM6DSO_Status_t LSM6DSO_Init(SPI_HandleTypeDef *hspi)
{
    uint8_t who;

    /* 1. Verify device identity */
    if (reg_read(hspi, REG_WHO_AM_I, &who, 1U) != LSM6DSO_OK) { return LSM6DSO_ERR; }
    if (who != LSM6DSO_WHO_AM_I_VAL) { return LSM6DSO_ERR; }

    /* 2. CTRL3_C: enable block data update (BDU) and address auto-increment (IF_INC)
     *   BDU (bit 6) = 1: output registers not updated until MSB+LSB both read
     *   IF_INC (bit 2) = 1: register address auto-increments on multi-byte read */
    if (reg_write(hspi, REG_CTRL3_C, 0x44U) != LSM6DSO_OK) { return LSM6DSO_ERR; }

    /* 3. CTRL1_XL: ODR=104 Hz (bits [7:4]=0110), FS=±8G (bits [3:2]=11)
     *   0b 0110 11 00 = 0x6C */
    if (reg_write(hspi, REG_CTRL1_XL, 0x6CU) != LSM6DSO_OK) { return LSM6DSO_ERR; }

    /* Allow one ODR period (~10 ms) for first sample to appear */
    HAL_Delay(15U);

    return LSM6DSO_OK;
}

LSM6DSO_Status_t LSM6DSO_ReadAccel(SPI_HandleTypeDef *hspi,
                                    int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t raw[6];

    /* Burst-read 6 bytes starting from OUTX_L_A (auto-increment in sensor) */
    if (reg_read(hspi, REG_OUTX_L_A, raw, 6U) != LSM6DSO_OK) { return LSM6DSO_ERR; }

    /* Reconstruct signed 16-bit values (little-endian: L byte first) */
    *x = (int16_t)((uint16_t)raw[1] << 8U | (uint16_t)raw[0]);
    *y = (int16_t)((uint16_t)raw[3] << 8U | (uint16_t)raw[2]);
    *z = (int16_t)((uint16_t)raw[5] << 8U | (uint16_t)raw[4]);

    return LSM6DSO_OK;
}
