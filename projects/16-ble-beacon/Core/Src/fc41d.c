/* fc41d.c — Quectel FC41D BLE Beacon driver.
 *
 * UART9 (PD14=RX, PD15=TX, AF11) @ 115200 8N1 — FC41D AT command port
 * UART7 (PE7=RX,  PE8=TX,  AF7)  @ 115200 8N1 — trace output
 *
 * BLE beacon sequence (peripheral role):
 *   AT+QBLEINIT=0        — deinit (clear stale state)
 *   AT+QBLEINIT=2        — init as peripheral/server
 *   AT+QBLENAME=<name>   — set device name (visible via GATT + scan response)
 *   AT+QBLEADVPARAM=160,160  — 100 ms advertising interval
 *   AT+QBLEADVDATA=<hex> — set advertising payload
 *   AT+QBLEADVSTART      — begin advertising
 *
 * NOTE: ATE0 with echo enabled returns ERROR on first boot (FC41DAAR03A09).
 *       Fallback: send AT, verify OK, then retry ATE0.
 */

#include "fc41d.h"
#include "ring_buf.h"
#include "usart.h"
#include "iwdg.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ── Private constants ────────────────────────────────────────────────────── */
#define FC41D_LINE_MAX        128U
#define FC41D_POLL_INTERVAL   500U
#define FC41D_DETECT_TIMEOUT  10000U
#define FC41D_CMD_TIMEOUT     3000U
#define FC41D_BOOT_WAIT_MS    3000U

/* ── Module state ─────────────────────────────────────────────────────────── */
static RingBuf_t s_rx;
static uint8_t   s_rx_byte;

/* ── Trace helpers ────────────────────────────────────────────────────────── */

static void trace(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg,
                      (uint16_t)strlen(msg), 1000U);
}

static void tracef(const char *fmt, ...)
{
    char    buf[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        uint16_t slen = (n < (int)sizeof(buf))
                        ? (uint16_t)n
                        : (uint16_t)(sizeof(buf) - 1U);
        HAL_UART_Transmit(&huart7, (const uint8_t *)buf, slen, 1000U);
    }
}

/* ── UART9 helpers ────────────────────────────────────────────────────────── */

static void fc41d_tx(const char *data, uint16_t len)
{
    HAL_UART_Transmit(&huart9, (const uint8_t *)data, len, 2000U);
}

static void fc41d_rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart9, &s_rx_byte, 1U);
}

/* Send cmd, wait for a line containing 'expect'.
 * Returns  1 = found, 0 = timeout, -1 = ERROR received. */
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
            if (line[0] == '\0') { continue; }
            if (expect && strstr(line, expect)) { return 1; }
            if (strstr(line, "ERROR"))          { return -1; }
        } else if (b != '\r' && pos < (uint16_t)(FC41D_LINE_MAX - 1U)) {
            line[pos++] = (char)b;
        }
    }
    return 0;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void FC41D_Init(void)
{
    RingBuf_Init(&s_rx);
    s_rx_byte = 0U;
    fc41d_rx_arm();

    trace("[FC41D] P3V3_SW_EN (PC11) already HIGH\r\n");
    trace("[FC41D] WIFI_BLE_PWR_EN (PD5) = HIGH\r\n");
    HAL_GPIO_WritePin(WIFI_BLE_PWR_EN_GPIO_Port, WIFI_BLE_PWR_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(100U);

    trace("[FC41D] WIFI_BLE_RESETN (PD6) = HIGH (release reset)\r\n");
    HAL_GPIO_WritePin(WIFI_BLE_RESETN_GPIO_Port, WIFI_BLE_RESETN_Pin, GPIO_PIN_SET);

    tracef("[FC41D] waiting %u ms for module boot...\r\n",
           (unsigned)FC41D_BOOT_WAIT_MS);
    HAL_Delay(FC41D_BOOT_WAIT_MS);
}

FC41D_Result_t FC41D_Detect(void)
{
    char     line[FC41D_LINE_MAX];
    int      rc;
    uint32_t start   = HAL_GetTick();
    int      attempt = 0;

    trace("[FC41D] starting detection (AT poll, 10 s window)...\r\n");

    while ((HAL_GetTick() - start) < FC41D_DETECT_TIMEOUT) {
        attempt++;

        tracef("[FC41D] attempt %d: ATE0... ", attempt);
        rc = fc41d_cmd("ATE0", "OK", FC41D_CMD_TIMEOUT);
        if (rc == 1) {
            trace("OK\r\n");
            goto detected;
        }
        tracef("%s\r\n", rc == -1 ? "ERROR" : "timeout");

        /* ATE0 ERROR on first boot (echo on) — verify liveness with AT */
        tracef("[FC41D] attempt %d: AT...   ", attempt);
        rc = fc41d_cmd("AT", "OK", FC41D_CMD_TIMEOUT);
        if (rc == 1) {
            trace("OK\r\n");
            (void)fc41d_cmd("ATE0", "OK", FC41D_CMD_TIMEOUT);
            goto detected;
        }
        tracef("%s\r\n", rc == -1 ? "ERROR" : "timeout");

        HAL_Delay(FC41D_POLL_INTERVAL);
    }

    trace("[FC41D] detect: FAIL - no response within 10 s\r\n");
    return FC41D_RESULT_TIMEOUT;

detected:
    trace("[FC41D] detect: PASS\r\n");

    /* Read firmware version */
    trace("[FC41D] AT+QVERSION...\r\n");
    RingBuf_Flush(&s_rx);
    fc41d_tx("AT+QVERSION\r\n", 13U);
    {
        uint32_t t0 = HAL_GetTick();
        uint8_t  b;
        uint16_t pos = 0U;
        while ((HAL_GetTick() - t0) < 3000U) {
            if (!RingBuf_Get(&s_rx, &b)) { continue; }
            if (b == '\n') {
                if (pos > 0U && line[pos - 1U] == '\r') { pos--; }
                line[pos] = '\0';
                pos = 0U;
                if (line[0] == '\0') { continue; }
                tracef("[FC41D] ver: %s\r\n", line);
                if (strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0) {
                    break;
                }
            } else if (b != '\r' && pos < (uint16_t)(FC41D_LINE_MAX - 1U)) {
                line[pos++] = (char)b;
            }
        }
    }

    return FC41D_RESULT_OK;
}

FC41D_Result_t FC41D_BLE_BeaconStart(const char *name, const char *adv_hex)
{
    int rc;

    /* Step 1: deinit BLE stack — clear any stale state */
    trace("[BEACON] AT+QBLEINIT=0 (deinit)...\r\n");
    rc = fc41d_cmd("AT+QBLEINIT=0", "OK", FC41D_CMD_TIMEOUT);
    tracef("[BEACON] QBLEINIT=0: %s\r\n",
           rc == 1 ? "OK" : rc == -1 ? "ERROR (already deinit, ok)" : "timeout");
    HAL_Delay(300U);

    /* Step 2: init as peripheral/server (role 2) */
    trace("[BEACON] AT+QBLEINIT=2 (peripheral)...\r\n");
    rc = fc41d_cmd("AT+QBLEINIT=2", "OK", FC41D_CMD_TIMEOUT);
    if (rc != 1) {
        tracef("[BEACON] QBLEINIT=2 failed: %s\r\n",
               rc == -1 ? "ERROR" : "timeout");
        return FC41D_RESULT_ERROR;
    }
    trace("[BEACON] QBLEINIT=2: OK\r\n");

    /* Step 3: create minimal GATT service — required before QBLEADVSTART */
    trace("[BEACON] AT+QBLEGATTSSRV=FFF0 (minimal service)...\r\n");
    rc = fc41d_cmd("AT+QBLEGATTSSRV=FFF0", "OK", FC41D_CMD_TIMEOUT);
    tracef("[BEACON] QBLEGATTSSRV: %s\r\n",
           rc == 1 ? "OK" : rc == -1 ? "ERROR" : "timeout");
    if (rc != 1) { return FC41D_RESULT_ERROR; }

    trace("[BEACON] AT+QBLEGATTSCHAR=FFF1 (characteristic)...\r\n");
    rc = fc41d_cmd("AT+QBLEGATTSCHAR=FFF1", "OK", FC41D_CMD_TIMEOUT);
    tracef("[BEACON] QBLEGATTSCHAR: %s\r\n",
           rc == 1 ? "OK" : rc == -1 ? "ERROR" : "timeout");
    if (rc != 1) { return FC41D_RESULT_ERROR; }
    HAL_Delay(100U);

    /* Step 4: set device name */
    {
        char cmd[48];
        (void)snprintf(cmd, sizeof(cmd), "AT+QBLENAME=%s", name);
        tracef("[BEACON] %s...\r\n", cmd);
        rc = fc41d_cmd(cmd, "OK", FC41D_CMD_TIMEOUT);
        if (rc != 1) {
            tracef("[BEACON] QBLENAME failed: %s\r\n",
                   rc == -1 ? "ERROR" : "timeout");
            return FC41D_RESULT_ERROR;
        }
        trace("[BEACON] QBLENAME: OK\r\n");
    }

    /* Step 6: set advertising interval to 100 ms (160 * 0.625 ms) */
    trace("[BEACON] AT+QBLEADVPARAM=160,160 (100 ms interval)...\r\n");
    rc = fc41d_cmd("AT+QBLEADVPARAM=160,160", "OK", FC41D_CMD_TIMEOUT);
    if (rc != 1) {
        tracef("[BEACON] QBLEADVPARAM failed: %s\r\n",
               rc == -1 ? "ERROR" : "timeout");
        return FC41D_RESULT_ERROR;
    }
    trace("[BEACON] QBLEADVPARAM: OK\r\n");

    /* Step 7: set advertising payload */
    {
        /* Build "AT+QBLEADVDATA=<hex>" — strip any spaces from adv_hex */
        char cmd[128];
        uint8_t ci = 0U;
        const char prefix[] = "AT+QBLEADVDATA=";
        for (uint8_t i = 0U; prefix[i] != '\0' && ci < (uint8_t)(sizeof(cmd) - 1U); i++) {
            cmd[ci++] = prefix[i];
        }
        for (uint8_t i = 0U; adv_hex[i] != '\0' && ci < (uint8_t)(sizeof(cmd) - 1U); i++) {
            if (adv_hex[i] != ' ') {
                cmd[ci++] = adv_hex[i];
            }
        }
        cmd[ci] = '\0';
        tracef("[BEACON] %s...\r\n", cmd);
        rc = fc41d_cmd(cmd, "OK", FC41D_CMD_TIMEOUT);
        if (rc != 1) {
            tracef("[BEACON] QBLEADVDATA failed: %s\r\n",
                   rc == -1 ? "ERROR" : "timeout");
            return FC41D_RESULT_ERROR;
        }
        trace("[BEACON] QBLEADVDATA: OK\r\n");
    }

    /* Step 8: start advertising */
    trace("[BEACON] AT+QBLEADVSTART...\r\n");
    rc = fc41d_cmd("AT+QBLEADVSTART", "OK", FC41D_CMD_TIMEOUT);
    if (rc != 1) {
        tracef("[BEACON] QBLEADVSTART failed: %s\r\n",
               rc == -1 ? "ERROR" : "timeout");
        return FC41D_RESULT_ERROR;
    }
    trace("[BEACON] QBLEADVSTART: OK\r\n");
    tracef("[BEACON] advertising as \"%s\" with custom payload\r\n", name);

    return FC41D_RESULT_OK;
}

void FC41D_RxByte(void)
{
    RingBuf_Put(&s_rx, s_rx_byte);
    fc41d_rx_arm();
}
