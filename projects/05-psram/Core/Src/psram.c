/* psram.c — IS66WVS16M8FBLL-104K PSRAM SPI driver
 *
 * Hardware: SPI2 (CAN_SPI2), 60 MHz (PLL1Q 120 MHz / prescaler 2)
 *           MISO=PB14, MOSI=PB15, SCK=PA12 (AF5)
 *           CS=PE13 (PSRAM_SPI2_NSS_Pin) — active LOW, software GPIO
 *
 * SPI Mode 0 (CPOL=0, CPHA=0): data sampled on rising CLK edge.
 * Mode 0 + AFCNTR=0 is safe — no spurious edge (only CPOL=1 has that issue).
 *
 * tCEM constraint: CE# must return HIGH within 4 µs at 85°C.
 * At 60 MHz SPI, 1 byte = 133 ns → max ~30 bytes per CS assertion.
 * Driver uses 20-byte data chunks (write=24B total=3.2 µs, read=25B=3.33 µs).
 *
 * Uses direct-register SPI transfers (not HAL_SPI_TransmitReceive) to avoid
 * the STM32H5 HAL V1.6.0 FIFO bug in polling mode.
 */

#include "psram.h"
#include "main.h"   /* PSRAM_NSS_PIN, PSRAM_NSS_PORT */
#include "spi.h"    /* hspi2 handle (needed for DMA unlink done in main) */

/* ── Last received ID bytes (for diagnostics) ────────────────────────────── */
static uint8_t s_last_mfr;
static uint8_t s_last_kgd;

void PSRAM_GetLastID(uint8_t *mfr, uint8_t *kgd)
{
    *mfr = s_last_mfr;
    *kgd = s_last_kgd;
}

/* ── PSRAM commands ──────────────────────────────────────────────────────── */
#define CMD_WRITE       0x02U
#define CMD_FAST_READ   0x0BU
#define CMD_RDID        0x9FU
#define CMD_RESET_EN    0x66U
#define CMD_RESET       0x99U

/* ── Expected ID values ──────────────────────────────────────────────────── */
#define MFR_ID_ISSI     0x9DU
#define KGD_PASS        0x5DU

/* ── tCEM chunk size: max data bytes per CS assertion ────────────────────── */
#define CHUNK_DATA      20U

/* ── CS helpers ──────────────────────────────────────────────────────────── */
static void cs_low(void)
{
    HAL_GPIO_WritePin(PSRAM_NSS_PORT, PSRAM_NSS_PIN, GPIO_PIN_RESET);
}

static void cs_high(void)
{
    HAL_GPIO_WritePin(PSRAM_NSS_PORT, PSRAM_NSS_PIN, GPIO_PIN_SET);
}

/* ── Direct-register SPI2 full-duplex transfer ───────────────────────────── */
/**
 * @brief  Transfers len bytes over SPI2 using direct register access.
 *         Flushes RX FIFO before transfer; drains RX byte-by-byte during.
 *         Avoids HAL_SPI_TransmitReceive (STM32H5 HAL V1.6.0 FIFO bug).
 */
static PSRAM_Status_t spi2_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    SPI_TypeDef *spi = SPI2;

    /* 1. Flush any stale bytes from RX FIFO */
    while (spi->SR & SPI_SR_RXP)
    {
        (void)*(volatile uint8_t *)&spi->RXDR;
    }

    /* 2. Clear all flags */
    spi->IFCR = 0xFFFFFFFFU;

    /* 3. Set transfer size */
    MODIFY_REG(spi->CR2, SPI_CR2_TSIZE, (uint32_t)len);

    /* 4. Enable SPI */
    SET_BIT(spi->CR1, SPI_CR1_SPE);

    /* 5. Start master transfer */
    SET_BIT(spi->CR1, SPI_CR1_CSTART);

    /* 6. Pump TX and RX byte-by-byte */
    uint16_t tx_rem = len;
    uint16_t rx_rem = len;
    uint32_t guard  = 200000U;  /* generous timeout — > 25 bytes at 60 MHz */

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
    guard = 200000U;
    while (!(spi->SR & SPI_SR_EOT) && --guard) {}

    /* 8. Clear EOT + TXTF */
    spi->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;

    /* 9. Disable SPI */
    CLEAR_BIT(spi->CR1, SPI_CR1_SPE);

    return (guard > 0U && tx_rem == 0U && rx_rem == 0U) ? PSRAM_OK : PSRAM_ERR;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

PSRAM_Status_t PSRAM_Init(void)
{
    uint8_t dummy;

    /* Deassert CS, wait > 150 µs power-up time */
    cs_high();
    HAL_Delay(1U);

    /* RESET ENABLE (0x66) */
    uint8_t cmd = CMD_RESET_EN;
    cs_low();
    (void)spi2_xfer(&cmd, &dummy, 1U);
    cs_high();

    /* RESET (0x99) — must immediately follow RESET ENABLE */
    cmd = CMD_RESET;
    cs_low();
    (void)spi2_xfer(&cmd, &dummy, 1U);
    cs_high();

    /* Wait for reset completion */
    HAL_Delay(1U);

    /* READ ID: [0x9F, 0x00, 0x00, 0x00, 0xFF, 0xFF]
     * Response: [_, _, _, _, MFR, KGD]
     * rx[4] = Manufacturer ID (expect 0x9D)
     * rx[5] = KGD             (expect 0x5D = PASS) */
    uint8_t tx_id[6] = { CMD_RDID, 0x00U, 0x00U, 0x00U, 0xFFU, 0xFFU };
    uint8_t rx_id[6] = { 0 };

    cs_low();
    PSRAM_Status_t rc = spi2_xfer(tx_id, rx_id, 6U);
    cs_high();

    s_last_mfr = rx_id[4];
    s_last_kgd = rx_id[5];

    if (rc != PSRAM_OK)                        { return PSRAM_ERR; }
    if (rx_id[4] != MFR_ID_ISSI)              { return PSRAM_ERR; }
    if (rx_id[5] != KGD_PASS)                 { return PSRAM_ERR; }

    return PSRAM_OK;
}

PSRAM_Status_t PSRAM_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    while (len > 0U)
    {
        uint32_t chunk = (len > CHUNK_DATA) ? CHUNK_DATA : len;

        /* Build TX frame: CMD(1) + ADDR(3) + DATA(chunk) */
        uint8_t tx[4U + CHUNK_DATA];
        uint8_t rx[4U + CHUNK_DATA];

        tx[0] = CMD_WRITE;
        tx[1] = (uint8_t)((addr >> 16U) & 0xFFU);
        tx[2] = (uint8_t)((addr >>  8U) & 0xFFU);
        tx[3] = (uint8_t)( addr         & 0xFFU);

        for (uint32_t i = 0U; i < chunk; i++)
        {
            tx[4U + i] = buf[i];
        }

        cs_low();
        PSRAM_Status_t rc = spi2_xfer(tx, rx, (uint16_t)(4U + chunk));
        cs_high();

        if (rc != PSRAM_OK) { return PSRAM_ERR; }

        buf  += chunk;
        addr += chunk;
        len  -= chunk;
    }
    return PSRAM_OK;
}

PSRAM_Status_t PSRAM_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    while (len > 0U)
    {
        uint32_t chunk = (len > CHUNK_DATA) ? CHUNK_DATA : len;

        /* FAST READ frame: CMD(1) + ADDR(3) + DUMMY(1) + DATA(chunk)
         * Dummy byte = 8 wait cycles required at frequencies above 33 MHz */
        uint16_t total = (uint16_t)(5U + chunk);
        uint8_t tx[5U + CHUNK_DATA];
        uint8_t rx[5U + CHUNK_DATA];

        tx[0] = CMD_FAST_READ;
        tx[1] = (uint8_t)((addr >> 16U) & 0xFFU);
        tx[2] = (uint8_t)((addr >>  8U) & 0xFFU);
        tx[3] = (uint8_t)( addr         & 0xFFU);
        tx[4] = 0xFFU;  /* dummy byte */
        for (uint32_t i = 0U; i < chunk; i++) { tx[5U + i] = 0xFFU; }

        cs_low();
        PSRAM_Status_t rc = spi2_xfer(tx, rx, total);
        cs_high();

        if (rc != PSRAM_OK) { return PSRAM_ERR; }

        /* Data starts at rx[5] (after CMD+ADDR+DUMMY) */
        for (uint32_t i = 0U; i < chunk; i++)
        {
            buf[i] = rx[5U + i];
        }

        buf  += chunk;
        addr += chunk;
        len  -= chunk;
    }
    return PSRAM_OK;
}
