/* fc41d.c — Quectel FC41D Wi-Fi+BLE module driver.
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
 * WiFi scan: AT+QWSCAN — waits up to 18 s, parses +QWSCAN: lines.
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
#define FC41D_POLL_INTERVAL   500U    /* ms between AT attempts */
#define FC41D_DETECT_TIMEOUT  10000U  /* ms total detection window */
#define FC41D_CMD_TIMEOUT     1000U   /* ms per individual command */
#define FC41D_BOOT_WAIT_MS    3000U   /* ms after reset release */
#define FC41D_SCAN_TIMEOUT    18000U  /* ms to wait for AT+QWSCAN to finish */

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

/* Map security integer to 4-char label. */
static const char *security_str(uint8_t sec)
{
    switch (sec) {
        case 0:  return "OPEN";
        case 1:  return "WEP ";
        case 2:  return "WPA ";
        case 3:  return "WPA2";
        case 4:  return "MIX ";
        default: return "?   ";
    }
}

/* Parse one "+QWSCAN:<sp?><signal>,<security>,<ssid>,<bssid>" line.
 * Accepts both "+QWSCAN: 55,..." and "+QWSCAN:55,..." (space optional).
 * Returns 1 on success, 0 on parse error. */
static int parse_qwscan(const char *line, FC41D_ApInfo_t *ap)
{
    const char *p;
    int         val;
    size_t      rest_len;
    const char *last_comma;
    const char *bssid_p;
    size_t      ssid_len;

    /* Check prefix — space after colon is optional */
    if (strncmp(line, "+QWSCAN:", 8) != 0) { return 0; }
    p = line + 8;
    if (*p == ' ') { p++; }   /* skip optional space */

    /* Parse signal (integer 0–63) */
    val = 0;
    while (*p >= '0' && *p <= '9') { val = val * 10 + (*p++ - '0'); }
    if (*p != ',') { return 0; }
    p++;
    ap->rssi = (int8_t)(val - 100);   /* convert to dBm */

    /* Parse security (single digit) */
    val = 0;
    while (*p >= '0' && *p <= '9') { val = val * 10 + (*p++ - '0'); }
    if (*p != ',') { return 0; }
    p++;
    ap->security = (uint8_t)val;

    /* Remaining: "<ssid>,<bssid>"
     * BSSID is always "AA:BB:CC:DD:EE:FF" (17 chars).
     * Find the last comma to split SSID from BSSID. */
    rest_len = strlen(p);
    if (rest_len < 19U) { return 0; }   /* minimum: 1 SSID + ',' + 17 BSSID */

    last_comma = strrchr(p, ',');
    if (last_comma == NULL || last_comma == p) { return 0; }

    bssid_p = last_comma + 1;
    if (strlen(bssid_p) != 17U) { return 0; }

    /* Copy BSSID */
    memcpy(ap->bssid, bssid_p, 17U);
    ap->bssid[17] = '\0';

    /* Copy SSID */
    ssid_len = (size_t)(last_comma - p);
    if (ssid_len == 0U) { return 0; }
    if (ssid_len >= sizeof(ap->ssid)) { ssid_len = sizeof(ap->ssid) - 1U; }
    memcpy(ap->ssid, p, ssid_len);
    ap->ssid[ssid_len] = '\0';

    return 1;
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
    char     fmt[FC41D_LINE_MAX + 32U];
    char     line[FC41D_LINE_MAX];
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

    for (int i = 0; i < 4; i++) {
        if (!fc41d_getline(line, sizeof(line), 3000U)) { break; }
        tracef(fmt, sizeof(fmt), "[FC41D] ver: %s\r\n", line);
        if (strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0) { break; }
    }

    return FC41D_RESULT_OK;
}

int FC41D_WifiScan(FC41D_ApInfo_t *aps, uint8_t max_aps)
{
    char line[FC41D_LINE_MAX];
    /* Buffer large enough for:
     * "[FC41D] AP 20:  -100 dBm  WPA2  AA:BB:CC:DD:EE:FF  <32-char-ssid>\r\n\0"
     * = 13 + 4 + 12 + 6 + 18 + 2 + 32 + 3 = ~90 chars → 128 is safe */
    char fmt[160];   /* 160 to also cover [UART9] + FC41D_LINE_MAX */
    int  count = 0;

    trace("[FC41D] AT+QWSCAN (WiFi scan, up to 18 s)...\r\n");
    RingBuf_Flush(&s_rx);
    fc41d_tx("AT+QWSCAN\r\n", 11U);

    /* Read lines until OK / ERROR / timeout.
     * Each fc41d_getline call waits up to FC41D_SCAN_TIMEOUT for the NEXT
     * non-blank line — the timer resets on each call so blank lines between
     * scan results do not consume the full budget.
     * Raw lines are echoed with [UART9] prefix for trace-level debugging. */
    for (;;) {
        if (!fc41d_getline(line, sizeof(line), FC41D_SCAN_TIMEOUT)) {
            trace("[FC41D] WifiScan: timeout waiting for response\r\n");
            break;
        }

        /* Echo every raw line from the module for diagnostics */
        tracef(fmt, sizeof(fmt), "[UART9] %s\r\n", line);

        if (strcmp(line, "OK") == 0) {
            break;
        }
        if (strcmp(line, "ERROR") == 0) {
            trace("[FC41D] WifiScan: AT+QWSCAN returned ERROR\r\n");
            count = -1;
            break;
        }
        if (strncmp(line, "+QWSCAN:", 8) != 0) { continue; }

        /* Parse and store the AP entry */
        FC41D_ApInfo_t ap;
        memset(&ap, 0, sizeof(ap));
        if (parse_qwscan(line, &ap)) {
            int idx = count;
            if (aps != NULL && count < (int)max_aps) {
                aps[count] = ap;
            }
            count++;
            /* Trace every found AP */
            tracef(fmt, sizeof(fmt),
                   "[FC41D] AP %2d:  %4d dBm  %s  %s  %s\r\n",
                   idx + 1, (int)ap.rssi,
                   security_str(ap.security), ap.bssid, ap.ssid);
        } else {
            trace("[FC41D] WifiScan: parse_qwscan failed for line above\r\n");
        }
    }

    if (count >= 0) {
        tracef(fmt, sizeof(fmt),
               "[FC41D] WifiScan done: %d network(s) found\r\n", count);
    }

    return count;
}

void FC41D_RxByte(void)
{
    RingBuf_Put(&s_rx, s_rx_byte);
    fc41d_rx_arm();
}
