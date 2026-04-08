/* lsm6dso.c — LSM6DSO accelerometer SPI driver
 *
 * Hardware: SPI1, software NSS on PE9 (ACC_NSS_PIN / ACC_NSS_PORT).
 * SPI Mode 3: CPOL=1, CPHA=1, 8-bit, MSB first.
 * Max sensor SPI clock: 10 MHz — driver is initialised at 7.5 MHz (prescaler 16).
 *
 * Uses direct-register SPI transfers instead of HAL_SPI_TransmitReceive,
 * which has FIFO-drain bugs in the STM32H5 HAL V1.6.0 for short (2-byte)
 * 8-bit polling transfers.
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

/* ── SPI buffer sizes ────────────────────────────────────────────────────── */
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
 * @brief  Direct-register SPI1 full-duplex transfer (bypasses HAL polling bug).
 *         Properly flushes RX FIFO before transfer and drains it afterward.
 */
static LSM6DSO_Status_t spi1_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    SPI_TypeDef *spi = SPI1;

    /* 1. Flush any stale data from RX FIFO */
    while (spi->SR & SPI_SR_RXP)
    {
        (void)*(volatile uint8_t *)&spi->RXDR;
    }

    /* 2. Clear all status/error flags */
    spi->IFCR = 0xFFFFFFFFU;

    /* 3. Set transfer size (must be done while SPE=0 or before CSTART) */
    MODIFY_REG(spi->CR2, SPI_CR2_TSIZE, (uint32_t)len);

    /* 4. Enable SPI */
    SET_BIT(spi->CR1, SPI_CR1_SPE);

    /* 5. Start master transfer */
    SET_BIT(spi->CR1, SPI_CR1_CSTART);

    /* 6. Pump TX and RX byte-by-byte */
    uint16_t tx_rem = len;
    uint16_t rx_rem = len;
    uint32_t guard  = 100000U;  /* ~400 µs at 240 MHz, way more than needed */

    while ((tx_rem > 0U || rx_rem > 0U) && --guard)
    {
        uint32_t sr = spi->SR;

        if (tx_rem > 0U && (sr & SPI_SR_TXP))
        {
            *(volatile uint8_t *)&spi->TXDR = *tx++;
            tx_rem--;
        }
        if (rx_rem > 0U && (sr & SPI_SR_RXP))
        {
            *rx++ = *(volatile uint8_t *)&spi->RXDR;
            rx_rem--;
        }
    }

    /* 7. Wait for end-of-transfer */
    guard = 100000U;
    while (!(spi->SR & SPI_SR_EOT) && --guard) {}

    /* 8. Clear EOT + TXTF */
    spi->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;

    /* 9. Disable SPI */
    CLEAR_BIT(spi->CR1, SPI_CR1_SPE);

    return (guard > 0U && tx_rem == 0U && rx_rem == 0U) ? LSM6DSO_OK : LSM6DSO_ERR;
}

/**
 * @brief  Write one byte to a sensor register.
 */
static LSM6DSO_Status_t reg_write(SPI_HandleTypeDef *hspi,
                                   uint8_t reg, uint8_t val)
{
    (void)hspi;
    uint8_t tx[2] = { (uint8_t)(reg & ~SPI_READ_FLAG), val };
    uint8_t rx[2];

    cs_low();
    LSM6DSO_Status_t rc = spi1_xfer(tx, rx, 2U);
    cs_high();

    return rc;
}

/**
 * @brief  Read one or more consecutive registers.
 */
static LSM6DSO_Status_t reg_read(SPI_HandleTypeDef *hspi,
                                  uint8_t reg, uint8_t *buf, uint8_t len)
{
    (void)hspi;
    uint8_t tx[SPI_BUF_SIZE];
    uint8_t rx[SPI_BUF_SIZE];
    uint8_t total = (uint8_t)(len + 1U);

    if (total > SPI_BUF_SIZE) { return LSM6DSO_ERR; }

    tx[0] = (uint8_t)(reg | SPI_READ_FLAG);
    for (uint8_t i = 1U; i < total; i++) { tx[i] = 0xFFU; }

    cs_low();
    LSM6DSO_Status_t rc = spi1_xfer(tx, rx, total);
    cs_high();

    if (rc != LSM6DSO_OK) { return LSM6DSO_ERR; }

    for (uint8_t i = 0U; i < len; i++) { buf[i] = rx[i + 1U]; }

    return LSM6DSO_OK;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

LSM6DSO_Status_t LSM6DSO_Init(SPI_HandleTypeDef *hspi)
{
    uint8_t who;
    uint8_t verify;

    /* 1. Verify device identity */
    if (reg_read(hspi, REG_WHO_AM_I, &who, 1U) != LSM6DSO_OK) { return LSM6DSO_ERR; }
    if (who != LSM6DSO_WHO_AM_I_VAL) { return LSM6DSO_ERR; }

    /* 2. Software reset: clears all config registers to reset values.
     *    SW_RESET bit (bit 0) auto-clears once reset is complete (~50 µs).
     *    Wait 50 ms to be safe before accessing any other registers. */
    if (reg_write(hspi, REG_CTRL3_C, 0x01U) != LSM6DSO_OK) { return LSM6DSO_ERR; }
    HAL_Delay(50U);

    /* 3. CTRL3_C: enable block data update (BDU) and address auto-increment (IF_INC)
     *   BDU (bit 6) = 1: output registers not updated until MSB+LSB both read
     *   IF_INC (bit 2) = 1: register address auto-increments on multi-byte read */
    if (reg_write(hspi, REG_CTRL3_C, 0x44U) != LSM6DSO_OK) { return LSM6DSO_ERR; }

    /* 4. CTRL1_XL: ODR=104 Hz (bits [7:4]=0110), FS=±8G (bits [3:2]=11)
     *   0b 0110 11 00 = 0x6C */
    if (reg_write(hspi, REG_CTRL1_XL, 0x6CU) != LSM6DSO_OK) { return LSM6DSO_ERR; }

    /* 5. Read back CTRL1_XL to verify the write took effect.
     *    If the register still reads 0x00, the SPI write is not reaching the sensor. */
    if (reg_read(hspi, REG_CTRL1_XL, &verify, 1U) != LSM6DSO_OK) { return LSM6DSO_ERR; }
    if (verify != 0x6CU) { return LSM6DSO_ERR; }

    /* 6. Wait for first sample: LSM6DSO datasheet specifies up to 80 ms startup
     *    time from power-down at 104 Hz ODR. */
    HAL_Delay(100U);

    return LSM6DSO_OK;
}

LSM6DSO_Status_t LSM6DSO_ReadReg(SPI_HandleTypeDef *hspi,
                                  uint8_t reg, uint8_t *val)
{
    return reg_read(hspi, reg, val, 1U);
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
