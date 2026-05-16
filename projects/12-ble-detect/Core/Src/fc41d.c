/* fc41d.c — Quectel FC41D Wi-Fi+BLE module driver (presence detection).
 *
 * Connection: UART9 (PD14=RX, PD15=TX, GPIO_AF11_UART9) @ 115200 8N1
 * Power-on sequence:
 *   1. P3V3_SW_EN (PC11)      — already HIGH from MX_GPIO_Init
 *   2. WIFI_BLE_PWR_EN (PD5)  — drive HIGH (module power)
 *   3. Delay 100 ms
 *   4. WIFI_BLE_RESETN (PD6)  — drive HIGH (release open-drain reset)
 *   5. Wait 3 s for FC41D boot
 *
 * Detection: poll "ATE0" / "AT" every 500 ms for up to 10 s.
 * On first OK: capture AT+QVERSION response and trace it.
 */
#include "fc41d.h"
#include "ring_buf.h"
#include "usart.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ── Private constants ───────────────────────────────────────────────────── */
#define FC41D_LINE_MAX        128U
#define FC41D_POLL_INTERVAL   500U   /* ms between AT attempts */
#define FC41D_DETECT_TIMEOUT  10000U /* ms total detection window */
#define FC41D_CMD_TIMEOUT     1000U  /* ms per individual command */
#define FC41D_BOOT_WAIT_MS    3000U  /* ms after reset release */

/* ── Module-private state ────────────────────────────────────────────────── */
static RingBuf_t s_rx;
static uint8_t   s_rx_byte;

/* ── Private helpers ─────────────────────────────────────────────────────── */

static void trace(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg,
                      (uint16_t)strlen(msg), 1000U);
}

static void tracef(char *buf, uint16_t buflen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, buflen, fmt, ap);
    va_end(ap);
    if (n > 0 && (uint16_t)n < buflen) {
        HAL_UART_Transmit(&huart7, (const uint8_t *)buf, (uint16_t)n, 1000U);
    }
}

static void fc41d_tx(const char *data, uint16_t len)
{
    HAL_UART_Transmit(&huart9, (const uint8_t *)data, len, 2000U);
}

static void fc41d_rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart9, &s_rx_byte, 1U);
}

/* Wait for a complete \r\n-terminated line from the UART9 ring buffer.
 * Strips \r\n, NUL-terminates result.
 * Returns 1 when a non-empty line is ready; 0 on timeout. */
static int fc41d_getline(char *buf, uint16_t maxlen, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint16_t pos   = 0U;
    uint8_t  b;

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (!RingBuf_Get(&s_rx, &b)) { continue; }
        if (b == '\n') {
            if (pos > 0U && buf[pos - 1U] == '\r') { pos--; }
            buf[pos] = '\0';
            if (pos == 0U) { continue; }   /* skip blank lines */
            return 1;
        } else if (b != '\r' && pos < (uint16_t)(maxlen - 1U)) {
            buf[pos++] = (char)b;
        }
    }
    buf[pos] = '\0';
    return 0;
}

/* Send a command and wait for a line containing 'expect'.
 * Returns  1 = found
 *          0 = timeout
 *         -1 = ERROR received */
static int fc41d_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    RingBuf_Flush(&s_rx);
    fc41d_tx(cmd, (uint16_t)strlen(cmd));
    fc41d_tx("\r\n", 2U);

    char     line[FC41D_LINE_MAX];
    uint32_t start = HAL_GetTick();
    uint8_t  b;
    uint16_t pos   = 0U;

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (!RingBuf_Get(&s_rx, &b)) { continue; }
        if (b == '\n') {
            if (pos > 0U && line[pos - 1U] == '\r') { pos--; }
            line[pos] = '\0';
            pos = 0U;
            if (line[0] == '\0') { continue; }    /* blank line */
            if (expect != NULL && strstr(line, expect) != NULL) { return 1; }
            if (strstr(line, "ERROR") != NULL)                  { return -1; }
        } else if (b != '\r' && pos < (uint16_t)(FC41D_LINE_MAX - 1U)) {
            line[pos++] = (char)b;
        }
    }
    return 0;   /* timeout */
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void FC41D_Init(void)
{
    RingBuf_Init(&s_rx);
    s_rx_byte = 0U;

    /* Arm UART9 interrupt-driven RX */
    fc41d_rx_arm();

    /* Step 1: P3V3_SW_EN (PC11) already HIGH from MX_GPIO_Init */
    trace("[FC41D] P3V3_SW_EN (PC11) already HIGH\r\n");

    /* Step 2: Enable module power */
    trace("[FC41D] WIFI_BLE_PWR_EN (PD5) = HIGH\r\n");
    HAL_GPIO_WritePin(WIFI_BLE_PWR_EN_GPIO_Port, WIFI_BLE_PWR_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(100U);

    /* Step 3: Release reset (open-drain — was LOW = in reset) */
    trace("[FC41D] WIFI_BLE_RESETN (PD6) = HIGH (release reset)\r\n");
    HAL_GPIO_WritePin(WIFI_BLE_RESETN_GPIO_Port, WIFI_BLE_RESETN_Pin, GPIO_PIN_SET);

    /* Step 4: Wait for module boot */
    char buf[64];
    tracef(buf, sizeof(buf),
           "[FC41D] waiting %u ms for module boot...\r\n",
           (unsigned)FC41D_BOOT_WAIT_MS);
    HAL_Delay(FC41D_BOOT_WAIT_MS);
}

FC41D_Result_t FC41D_Detect(void)
{
    char     fmt[FC41D_LINE_MAX + 32U];   /* format buffer: prefix + full line + \r\n */
    char     line[FC41D_LINE_MAX];  /* getline output */
    int      rc;
    uint32_t start   = HAL_GetTick();
    int      attempt = 0;

    trace("[FC41D] starting detection (AT poll, 10 s window)...\r\n");

    while ((HAL_GetTick() - start) < FC41D_DETECT_TIMEOUT) {
        attempt++;

        /* Try ATE0 first (disable echo — also confirms UART path) */
        tracef(fmt, sizeof(fmt),
               "[FC41D] attempt %d: ATE0... ", attempt);
        rc = fc41d_cmd("ATE0", "OK", FC41D_CMD_TIMEOUT);
        if (rc == 1) {
            trace("OK\r\n");
            goto detected;
        }
        tracef(fmt, sizeof(fmt), "%s\r\n",
               rc == -1 ? "ERROR" : "timeout");

        /* Fallback: plain AT (echo still on — first response is the echo) */
        tracef(fmt, sizeof(fmt),
               "[FC41D] attempt %d: AT...   ", attempt);
        rc = fc41d_cmd("AT", "OK", FC41D_CMD_TIMEOUT);
        if (rc == 1) {
            trace("OK\r\n");
            /* Suppress echo for cleaner subsequent exchanges */
            (void)fc41d_cmd("ATE0", "OK", FC41D_CMD_TIMEOUT);
            goto detected;
        }
        tracef(fmt, sizeof(fmt), "%s\r\n",
               rc == -1 ? "ERROR" : "timeout");

        HAL_Delay(FC41D_POLL_INTERVAL);
    }

    trace("[FC41D] detect: FAIL - no response within 10 s\r\n");
    return FC41D_RESULT_TIMEOUT;

detected:
    trace("[FC41D] detect: PASS - FC41D responding\r\n");

    /* Capture firmware version */
    trace("[FC41D] AT+QVERSION...\r\n");
    RingBuf_Flush(&s_rx);
    fc41d_tx("AT+QVERSION\r\n", 13U);

    /* Read up to 4 response lines (3 s window total).
     * Version line looks like: +QVERSION: FC41MAAR03A07 */
    for (int i = 0; i < 4; i++) {
        if (!fc41d_getline(line, sizeof(line), 3000U)) { break; }
        tracef(fmt, sizeof(fmt), "[FC41D] ver: %s\r\n", line);
        if (strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0) { break; }
    }

    return FC41D_RESULT_OK;
}

void FC41D_RxByte(void)
{
    RingBuf_Put(&s_rx, s_rx_byte);
    fc41d_rx_arm();
}
