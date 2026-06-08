/* fc41d.c — Quectel FC41D Wi-Fi+BLE driver: module detect + BLE scan.
 *
 * UART9 (PD14=RX, PD15=TX, AF11) @ 115200 8N1 — FC41D AT command port
 * UART7 (PE7=RX,  PE8=TX,  AF7)  @ 115200 8N1 — trace output
 *
 * BLE scan sequence:
 *   AT+QBLEINIT=1        — init as central (scanner)
 *   AT+QBLESCAN=1        — start continuous scan; URCs: <name>,<addr_type>,<addr>
 *   AT+QBLESCAN=0        — stop scan after duration_ms
 *
 * URC format (AT Commands Manual V2.0, Chapter 2.2.16):
 *   Mode 1 (no adv data): +QBLESCAN:<name>,<address_type>,<address>
 *   Mode 2 (with adv):    +QBLESCAN:<name>,<address_type>,<address>,<adv_data>
 *
 *   <name>         — BLE device name (empty string if device has no name)
 *   <address_type> — 0=public, 1=random
 *   <address>      — 12 lower-case hex chars, no colons
 *
 * NOTE: firmware FC41DAAR03A09 sends NO space after colon; address has no colons.
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
#define FC41D_LINE_MAX            128U
#define FC41D_POLL_INTERVAL       500U
#define FC41D_DETECT_TIMEOUT      10000U
#define FC41D_CMD_TIMEOUT         2000U
#define FC41D_BOOT_WAIT_MS        3000U
#define FC41D_SCAN_POLL_MS        200U   /* getline timeout during BLE scan loop */
#define FC41D_GATT_CONNECT_MS     8000U  /* max wait for +QBLECONN:0,0 URC */
#define FC41D_GATT_READ_MS        5000U  /* max wait for +QBLEGATTRD URC */

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

/* Read one complete \r\n-terminated line from the ring buffer.
 * Strips \r\n, NUL-terminates.
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
        } else if (b != '\r' && pos < (maxlen - 1U)) {
            buf[pos++] = (char)b;
        }
    }
    buf[pos] = '\0';
    return 0;
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

/* ── BLE scan parsing helpers ─────────────────────────────────────────────── */

/* Consume chars until delim or NUL, write to out[0..max-1], advance pointer. */
static const char *parse_field(const char *p, char *out,
                                uint8_t max, char delim)
{
    uint8_t i = 0U;
    while (*p != '\0' && *p != delim && i < (uint8_t)(max - 1U)) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    if (*p == delim) { p++; }
    return p;
}

static uint8_t hex_nibble(char c)
{
    if (c >= '0' && c <= '9') { return (uint8_t)(c - '0'); }
    if (c >= 'A' && c <= 'F') { return (uint8_t)(c - 'A' + 10); }
    if (c >= 'a' && c <= 'f') { return (uint8_t)(c - 'a' + 10); }
    return 0U;
}

static uint8_t hex_to_bytes(const char *hex, uint8_t *out, uint8_t max_len)
{
    uint8_t len = 0U;
    while (*hex != '\0' && *(hex + 1U) != '\0' && len < max_len) {
        out[len++] = (uint8_t)((uint8_t)(hex_nibble(*hex) << 4U)
                               | hex_nibble(*(hex + 1U)));
        hex += 2;
    }
    return len;
}

/* Walk BLE AD structures to extract device name (AD type 0x08 or 0x09). */
static void extract_ble_name(const char *adv_hex, char *name_out,
                              uint8_t name_max)
{
    uint8_t       adv[64];
    uint8_t       adv_len = hex_to_bytes(adv_hex, adv, (uint8_t)sizeof(adv));
    const uint8_t *p      = adv;
    const uint8_t *end    = adv + adv_len;

    name_out[0] = '\0';

    while (p < end) {
        uint8_t field_len = *p;
        if (field_len == 0U || (p + 1U + field_len) > end) { break; }
        uint8_t ad_type = *(p + 1U);
        if (ad_type == 0x08U || ad_type == 0x09U) {
            uint8_t nlen = (uint8_t)(field_len - 1U);
            if (nlen >= name_max) { nlen = (uint8_t)(name_max - 1U); }
            for (uint8_t j = 0U; j < nlen; j++) {
                name_out[j] = (char)*(p + 2U + j);
            }
            name_out[nlen] = '\0';
            return;
        }
        p += field_len + 1U;
    }
}

/* Parse one +QBLESCAN URC into a BLE_Device_t.
 *
 * Two observed formats (auto-detected by field-1 length):
 *
 *   Actual firmware FC41DAAR03A09 (RSSI first, addr NO colons):
 *     +QBLESCAN:<rssi_or_empty>,<addr_type>,<addr_12hex>[,<adv_hex>]
 *     e.g. +QBLESCAN:,1,1f953971eae5
 *          +QBLESCAN:-55,0,00a050f0354b,0201...
 *
 *   AT manual / some devices (addr first, may have colons):
 *     +QBLESCAN:<addr>,<addr_type>,<rssi>[,<adv_hex>]
 *     e.g. +QBLESCAN:00:A0:50:F0:35:4B,0,-55
 *          +QBLESCAN:00a050f0354b,0,-55
 *
 * Detection rule: if field1 length is 12 or 17 → addr is first (manual format);
 * otherwise (empty or a numeric RSSI) → addr is field3 (actual format).
 *
 * Both 12-char no-colon and 17-char colon MAC formats are accepted.
 *
 * Returns 1 on success, 0 if the line is not a valid +QBLESCAN device line. */
static int parse_qblescan(const char *line, BLE_Device_t *dev)
{
    static const char prefix[] = "+QBLESCAN:";
    if (strncmp(line, prefix, sizeof(prefix) - 1U) != 0) { return 0; }
    const char *p = line + sizeof(prefix) - 1U;

    /* Use wide buffers — field1 may be a 17-char colon-format MAC */
    char field1[24];
    char field2[4];
    char field3[24];
    char adv_hex[128];

    p = parse_field(p, field1,  (uint8_t)sizeof(field1),  ',');
    p = parse_field(p, field2,  (uint8_t)sizeof(field2),  ',');
    p = parse_field(p, field3,  (uint8_t)sizeof(field3),  ',');
    (void)parse_field(p, adv_hex, (uint8_t)sizeof(adv_hex), '\0');

    uint8_t f1_len = (uint8_t)strlen(field1);
    uint8_t f3_len = (uint8_t)strlen(field3);

    /* Select which field holds the MAC address */
    const char *addr_raw;
    const char *rssi_raw;
    if (f1_len == 12U || f1_len == 17U) {
        addr_raw = field1;   /* manual format: addr,addr_type,rssi */
        rssi_raw = field3;
    } else if (f3_len == 12U || f3_len == 17U) {
        rssi_raw = field1;   /* actual firmware format: rssi,addr_type,addr */
        addr_raw = field3;
    } else {
        return 0;
    }

    uint8_t addr_len = (uint8_t)strlen(addr_raw);

    if (addr_len == 12U) {
        /* 12 hex chars, no colons → format as XX:XX:XX:XX:XX:XX */
        (void)snprintf(dev->addr, sizeof(dev->addr),
                       "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
                       addr_raw[0],  addr_raw[1],  addr_raw[2],  addr_raw[3],
                       addr_raw[4],  addr_raw[5],  addr_raw[6],  addr_raw[7],
                       addr_raw[8],  addr_raw[9],  addr_raw[10], addr_raw[11]);
    } else {
        /* 17-char colon format: copy verbatim, normalize hex digits to upper */
        for (uint8_t j = 0U; j < 17U; j++) {
            char c = addr_raw[j];
            dev->addr[j] = (c >= 'a' && c <= 'f') ? (char)(c - ('a' - 'A')) : c;
        }
        dev->addr[17U] = '\0';
    }

    /* addr_type from field2: single ASCII digit */
    dev->addr_type = (field2[0] >= '0' && field2[0] <= '9')
                     ? (uint8_t)(field2[0] - '0') : 0U;

    /* rssi: optional leading '-', then decimal digits; empty → rssi_valid=0 */
    {
        int        val = 0;
        int        neg = 0;
        const char *r  = rssi_raw;
        if (*r == '-') { neg = 1; r++; }
        if (*r >= '0' && *r <= '9') {
            while (*r >= '0' && *r <= '9') { val = val * 10 + (*r++ - '0'); }
            dev->rssi_dbm   = (int16_t)(neg ? -val : val);
            dev->rssi_valid = 1U;
        } else {
            dev->rssi_dbm   = 0;
            dev->rssi_valid = 0U;
        }
    }

    /* device name from advertising payload (may be absent) */
    if (adv_hex[0] != '\0') {
        extract_ble_name(adv_hex, dev->name, (uint8_t)sizeof(dev->name));
    } else {
        dev->name[0] = '\0';
    }

    return 1;
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

    trace("[FC41D] AT+QVERSION...\r\n");
    RingBuf_Flush(&s_rx);
    fc41d_tx("AT+QVERSION\r\n", 13U);
    for (int i = 0; i < 4; i++) {
        if (!fc41d_getline(line, sizeof(line), 3000U)) { break; }
        tracef("[FC41D] ver: %s\r\n", line);
        if (strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0) { break; }
    }

    return FC41D_RESULT_OK;
}

uint8_t FC41D_BLE_Scan(BLE_Device_t *devices, uint8_t max_devices,
                        uint32_t duration_ms)
{
    char    line[FC41D_LINE_MAX];
    uint8_t count = 0U;
    int     rc;

    /* Deinit BLE stack first — clear any stale state from a prior run */
    trace("[BLE-SCAN] AT+QBLEINIT=0 (deinit, ignore result)...\r\n");
    rc = fc41d_cmd("AT+QBLEINIT=0", "OK", FC41D_CMD_TIMEOUT);
    tracef("[BLE-SCAN] QBLEINIT=0: %s\r\n",
           rc == 1 ? "OK" : rc == -1 ? "ERROR (already deinit, ok)" : "timeout");
    HAL_Delay(500U);   /* brief pause before reinit */

    /* Init BLE stack as central (scanner) */
    trace("[BLE-SCAN] AT+QBLEINIT=1 (central)...\r\n");
    rc = fc41d_cmd("AT+QBLEINIT=1", "OK", FC41D_CMD_TIMEOUT);
    if (rc != 1) {
        tracef("[BLE-SCAN] QBLEINIT=1 failed: %s\r\n",
               rc == -1 ? "ERROR" : "timeout");
        return 0U;
    }
    trace("[BLE-SCAN] QBLEINIT=1: OK\r\n");

    /* Enable active scanning — module sends scan requests so devices reply with
     * scan response data, which usually contains the device name (AD type 0x09).
     * Format: AT+QBLESCANPARAM=<type>,<interval>,<window>  (1=active, units 0.625ms)
     * Ignore error — command is optional and may not be supported on all firmware. */
    trace("[BLE-SCAN] AT+QBLESCANPARAM=1,100,50 (active scan)...\r\n");
    rc = fc41d_cmd("AT+QBLESCANPARAM=1,100,50", "OK", FC41D_CMD_TIMEOUT);
    tracef("[BLE-SCAN] QBLESCANPARAM: %s\r\n",
           rc == 1 ? "OK" : rc == -1 ? "ERROR (may be unsupported)" : "timeout");

    /* Start scanning */
    trace("[BLE-SCAN] AT+QBLESCAN=1...\r\n");
    rc = fc41d_cmd("AT+QBLESCAN=1", "OK", FC41D_CMD_TIMEOUT);
    if (rc != 1) {
        tracef("[BLE-SCAN] QBLESCAN=1 failed: %s\r\n",
               rc == -1 ? "ERROR" : "timeout");
        return 0U;
    }

    /* Non-flushing status query — response appears as [RAW] lines in scan loop */
    fc41d_tx("AT+QBLESCAN?\r\n", 14U);
    tracef("[BLE-SCAN] scanning for %u s (ensure BLE devices nearby are advertising)...\r\n",
           (unsigned)(duration_ms / 1000U));

    /* Collect +QBLESCAN URCs for duration_ms */
    uint32_t scan_start  = HAL_GetTick();
    uint32_t next_probe  = scan_start + 5000U;   /* AT liveness probe every 5 s */

    while ((HAL_GetTick() - scan_start) < duration_ms) {
        HAL_IWDG_Refresh(&hiwdg);   /* kick watchdog every ~200 ms */

        /* Periodic liveness probe — no flush, response logged as [RAW] */
        if (HAL_GetTick() >= next_probe) {
            fc41d_tx("AT\r\n", 4U);
            next_probe += 5000U;
        }

        if (!fc41d_getline(line, sizeof(line), FC41D_SCAN_POLL_MS)) { continue; }

        /* Log every raw line so we can see what the module is actually sending */
        tracef("[RAW] %s\r\n", line);

        if (strncmp(line, "+QBLESCAN:", 10U) != 0)                  { continue; }

        BLE_Device_t dev = {0};
        if (!parse_qblescan(line, &dev)) {
            tracef("[PARSE-FAIL] %s\r\n", line);
            continue;
        }

        /* Deduplicate by MAC address */
        uint8_t dup = 0U;
        for (uint8_t i = 0U; i < count; i++) {
            if (strncmp(devices[i].addr, dev.addr, 17U) == 0) {
                dup = 1U;
                break;
            }
        }
        if (dup) { continue; }

        if (count < max_devices) {
            devices[count] = dev;
            count++;
            tracef("[BLE-SCAN] found #%u: %s rssi=%d dBm%s%s\r\n",
                   (unsigned)count, dev.addr, (int)dev.rssi_dbm,
                   dev.name[0] ? " name=" : "",
                   dev.name[0] ? dev.name  : "");
        }
    }

    /* Stop scan */
    (void)fc41d_cmd("AT+QBLESCAN=0", "OK", FC41D_CMD_TIMEOUT);
    trace("[BLE-SCAN] scan stopped\r\n");

    return count;
}

void FC41D_RxByte(void)
{
    RingBuf_Put(&s_rx, s_rx_byte);
    fc41d_rx_arm();
}

int FC41D_BLE_GetName(const BLE_Device_t *dev, char *name_out, uint8_t name_max)
{
    char     cmd[64];
    char     line[FC41D_LINE_MAX];
    int      rc;
    uint32_t start;

    name_out[0] = '\0';

    /* ── Connect ─────────────────────────────────────────────────────────── */
    (void)snprintf(cmd, sizeof(cmd), "AT+QBLECONN=0,%u,%s",
                   (unsigned)dev->addr_type, dev->addr);
    tracef("[BLE-GATT] connecting %s type=%u...\r\n",
           dev->addr, (unsigned)dev->addr_type);

    RingBuf_Flush(&s_rx);
    fc41d_tx(cmd, (uint16_t)strlen(cmd));
    fc41d_tx("\r\n", 2U);

    /* Wait for OK (command accepted by module) */
    rc    = 0;
    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 3000U) {
        if (!fc41d_getline(line, sizeof(line), 300U)) { continue; }
        tracef("[BLE-GATT-RAW] %s\r\n", line);
        if (strstr(line, "ERROR") != NULL) {
            tracef("[BLE-GATT] QBLECONN ERROR — reinit BLE stack\r\n");
            (void)fc41d_cmd("AT+QBLEINIT=0", "OK", FC41D_CMD_TIMEOUT);
            HAL_Delay(300U);
            (void)fc41d_cmd("AT+QBLEINIT=1", "OK", FC41D_CMD_TIMEOUT);
            HAL_Delay(300U);
            return 0;
        }
        if (strcmp(line, "OK") == 0) { rc = 1; break; }
    }
    if (rc == 0) {
        tracef("[BLE-GATT] QBLECONN: no OK (timeout)\r\n");
        return 0;
    }

    /* Wait for +QBLECONN:0,<status> URC */
    rc    = 0;
    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < FC41D_GATT_CONNECT_MS) {
        HAL_IWDG_Refresh(&hiwdg);
        if (!fc41d_getline(line, sizeof(line), 500U)) { continue; }
        tracef("[BLE-GATT-RAW] %s\r\n", line);
        if (strncmp(line, "+QBLECONN:0,", 12U) == 0) {
            if (line[12] == '0') {
                rc = 1;
            } else {
                tracef("[BLE-GATT] connect failed: %s\r\n", line);
            }
            break;
        }
        if (strstr(line, "ERROR") != NULL) { break; }
    }
    if (rc == 0) {
        tracef("[BLE-GATT] no connect URC — cancelling pending connection\r\n");
        (void)fc41d_cmd("AT+QBLEDISCONN=0", "OK", 2000U);
        HAL_Delay(500U);
        return 0;
    }
    tracef("[BLE-GATT] connected to %s\r\n", dev->addr);

    /* ── Read Device Name characteristic (Generic Access, handle 0x0003) ── */
    trace("[BLE-GATT] AT+QBLEGATTCRD=0,3,0 (Device Name)...\r\n");
    RingBuf_Flush(&s_rx);
    fc41d_tx("AT+QBLEGATTCRD=0,3,0\r\n", 22U);

    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < FC41D_GATT_READ_MS) {
        HAL_IWDG_Refresh(&hiwdg);
        if (!fc41d_getline(line, sizeof(line), 500U)) { continue; }
        tracef("[BLE-GATT-RAW] %s\r\n", line);

        /* Accept both +QBLEGATTRD and +QBLEGATTCRD as URC prefixes */
        int matched = (strncmp(line, "+QBLEGATTRD:", 12U) == 0)
                   || (strncmp(line, "+QBLEGATTCRD:", 13U) == 0);
        if (matched != 0) {
            /* Find the last comma-separated field — that is the data payload */
            const char *last = line + strlen(line);
            while (last > line && *(last - 1U) != ',') { last--; }

            uint8_t dlen = (uint8_t)strlen(last);
            if (dlen > 64U) { dlen = 64U; }   /* sanity cap */

            /* Determine if data is hex-encoded (even length, all hex chars) */
            uint8_t all_hex = 1U;
            for (uint8_t j = 0U; j < dlen; j++) {
                char c = last[j];
                if (!((c >= '0' && c <= '9') ||
                      (c >= 'a' && c <= 'f') ||
                      (c >= 'A' && c <= 'F'))) {
                    all_hex = 0U;
                    break;
                }
            }

            if (all_hex != 0U && (dlen & 1U) == 0U && dlen > 0U) {
                /* Hex-encoded: decode to ASCII */
                uint8_t bytes[32];
                uint8_t blen = hex_to_bytes(last, bytes, (uint8_t)sizeof(bytes));
                if (blen >= name_max) { blen = (uint8_t)(name_max - 1U); }
                for (uint8_t j = 0U; j < blen; j++) {
                    name_out[j] = (char)bytes[j];
                }
                name_out[blen] = '\0';
            } else {
                /* Raw ASCII */
                uint8_t cplen = (dlen < (uint8_t)(name_max - 1U))
                                ? dlen : (uint8_t)(name_max - 1U);
                for (uint8_t j = 0U; j < cplen; j++) { name_out[j] = last[j]; }
                name_out[cplen] = '\0';
            }
            tracef("[BLE-GATT] name: \"%s\"\r\n", name_out);
            break;
        }

        /* Stop waiting on error, spontaneous disconnect, or final OK */
        if (strstr(line, "ERROR") != NULL
            || strncmp(line, "+QBLEDISCONN:0", 14U) == 0
            || strcmp(line, "OK") == 0) {
            break;
        }
    }

    /* ── Disconnect (always, even on read failure) ───────────────────────── */
    (void)fc41d_cmd("AT+QBLEDISCONN=0", "OK", 3000U);
    HAL_Delay(300U);   /* brief gap before next connection attempt */
    tracef("[BLE-GATT] disconnected from %s\r\n", dev->addr);

    return (name_out[0] != '\0') ? 1 : 0;
}
