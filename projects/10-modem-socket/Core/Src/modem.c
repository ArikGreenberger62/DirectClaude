/* modem.c — Quectel EG915N TCP socket driver for TFL_CONNECT_2.
 *
 * Power-on sequence (same as 09-sms-modem):
 *   1. P3V3_SW_EN (PC11) HIGH — 3.3V supply
 *   2. MC60_GNSS_PWR (PB8) HIGH — modem VBAT
 *   3. MC60_PWRKEY (PD7) pulse HIGH 1000ms → LOW
 *   4. Wait for boot URCs (RDY / +CFUN: 1)
 *
 * AT init:
 *   ATE0, CMEE=1, CPIN?, CREG (wait for registration)
 *
 * TCP init:
 *   AT+QICLOSE=0           — clean up any existing socket
 *   AT+QIDEACT=1           — deactivate context
 *   AT+QICSGP=1,1,"internet","","",1 — APN
 *   AT+QIACT=1             — activate PDP context
 *   AT+QIACT?              — read and log IP address
 *   AT+QIOPEN=1,0,"TCP","test.traffilog.co.il",30111,0,0 — connect
 *   wait for +QIOPEN: 0,0  — success indication
 *
 * Runtime URCs handled in Modem_Process:
 *   +QIURC: "recv",0       — data available; triggers AT+QIRD read
 *   +QIURC: "closed",0     — server closed the connection
 *   +QIURC: "pdpdeact",1   — PDP context dropped
 */
#include "modem.h"
#include "ring_buf.h"
#include "usart.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ── Private constants ─────────────────────────────────────────────────────── */
#define MDM_LINE_MAX           256U
#define MDM_BOOT_TIMEOUT_MS    45000U
#define MDM_REG_TIMEOUT_MS     60000U
#define MDM_VBAT_SETTLE_MS     2000U
#define MDM_PWRKEY_PRELOW_MS     50U
#define MDM_PWRKEY_PULSE_MS    1000U
#define MDM_TCP_BUF_SIZE       1500U   /* max bytes per AT+QIRD response */

/* ── Module-private state ──────────────────────────────────────────────────── */
static RingBuf_t    s_rx;
static uint8_t      s_rx_byte;
static uint32_t     s_rx_count;

static ModemState_t s_state = MODEM_STATE_OFF;

/* URC line assembly for Modem_Process */
static char         s_urc_line[MDM_LINE_MAX];
static uint16_t     s_urc_pos;

/* TCP receive buffer (static, never on stack) */
static char         s_tcp_buf[MDM_TCP_BUF_SIZE];

/* ── Private helpers ───────────────────────────────────────────────────────── */

static void trace(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, (uint16_t)strlen(msg), 1000U);
}

static void tracef(char *buf, uint16_t buflen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, (size_t)buflen, fmt, ap);
    va_end(ap);
    if (n > 0 && (uint16_t)n < buflen) {
        HAL_UART_Transmit(&huart7, (const uint8_t *)buf, (uint16_t)n, 1000U);
    }
}

static void mdm_tx(const char *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (const uint8_t *)data, len, 2000U);
}

static void mdm_rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1U);
}

/* Read one line from the modem ring buffer.
 * Returns 1 when a complete line is in buf (NUL-terminated, no \r\n).
 * Returns 0 on timeout.
 * When skip_blank=1, empty lines are discarded and reading continues. */
static int mdm_getline(char *buf, uint16_t maxlen, uint32_t timeout_ms, int skip_blank)
{
    uint32_t start = HAL_GetTick();
    uint16_t pos   = 0U;
    uint8_t  b;

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (!RingBuf_Get(&s_rx, &b)) { continue; }
        if (b == '\n') {
            if (pos > 0U && buf[pos - 1U] == '\r') { pos--; }
            buf[pos] = '\0';
            if (skip_blank && pos == 0U) {
                pos = 0U;
                continue;
            }
            return 1;
        } else if (b != '\r' && pos < (uint16_t)(maxlen - 1U)) {
            buf[pos++] = (char)b;
        }
    }
    buf[pos] = '\0';
    return 0;
}

/* Send command, wait for expected string in any response line.
 * Returns  1 = expected found
 *          0 = timeout
 *         -1 = ERROR received */
static int mdm_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    RingBuf_Flush(&s_rx);
    mdm_tx(cmd, (uint16_t)strlen(cmd));
    mdm_tx("\r\n", 2U);

    char     line[MDM_LINE_MAX];
    uint16_t pos   = 0U;
    uint32_t start = HAL_GetTick();
    uint8_t  b;

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (!RingBuf_Get(&s_rx, &b)) { continue; }
        if (b == '\n') {
            if (pos > 0U && line[pos - 1U] == '\r') { pos--; }
            line[pos] = '\0';
            pos = 0U;
            if (line[0] == '\0') { continue; }
            if (expect != NULL && strstr(line, expect) != NULL) { return 1; }
            if (strstr(line, "ERROR") != NULL)                  { return -1; }
        } else if (b != '\r' && pos < (uint16_t)(MDM_LINE_MAX - 1U)) {
            line[pos++] = (char)b;
        }
    }
    return 0;
}

/* Wait for expected string without sending a command first. */
static int mdm_wait_for(const char *expect, uint32_t timeout_ms)
{
    char     line[MDM_LINE_MAX];
    uint16_t pos   = 0U;
    uint32_t start = HAL_GetTick();
    uint8_t  b;

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (!RingBuf_Get(&s_rx, &b)) { continue; }
        if (b == '\n') {
            if (pos > 0U && line[pos - 1U] == '\r') { pos--; }
            line[pos] = '\0';
            pos = 0U;
            if (line[0] == '\0') { continue; }
            if (strstr(line, expect) != NULL)  { return 1; }
            if (strstr(line, "ERROR") != NULL) { return -1; }
        } else if (b != '\r' && pos < (uint16_t)(MDM_LINE_MAX - 1U)) {
            line[pos++] = (char)b;
        }
    }
    return 0;
}

static int mdm_is_alive(void)
{
    RingBuf_Flush(&s_rx);
    mdm_tx("AT\r\n", 4U);
    return mdm_wait_for("OK", 1000U) == 1;
}

/* ── Power-on sequence ─────────────────────────────────────────────────────── */
static void mdm_power_on(void)
{
    HAL_GPIO_WritePin(MC60_PWRKEY_GPIO_Port, MC60_PWRKEY_Pin, GPIO_PIN_RESET);
    HAL_Delay(MDM_PWRKEY_PRELOW_MS);

    trace("[MODEM] P3V3_SW_EN = HIGH\r\n");
    HAL_GPIO_WritePin(P3V3_SW_EN_GPIO_Port, P3V3_SW_EN_Pin, GPIO_PIN_SET);

    trace("[MODEM] MC60_GNSS_PWR = HIGH (VBAT on)\r\n");
    HAL_GPIO_WritePin(MC60_GNSS_PWR_GPIO_Port, MC60_GNSS_PWR_Pin, GPIO_PIN_SET);

    trace("[MODEM] MC60_PWR = HIGH\r\n");
    HAL_GPIO_WritePin(MC60_PWR_GPIO_Port, MC60_PWR_Pin, GPIO_PIN_SET);

    HAL_Delay(MDM_VBAT_SETTLE_MS);

    if (mdm_is_alive()) {
        trace("[MODEM] modem already on (AT -> OK)\r\n");
        return;
    }

    trace("[MODEM] PWRKEY pulse: LOW -> HIGH\r\n");
    HAL_GPIO_WritePin(MC60_PWRKEY_GPIO_Port, MC60_PWRKEY_Pin, GPIO_PIN_SET);
    HAL_Delay(MDM_PWRKEY_PULSE_MS);

    trace("[MODEM] PWRKEY pulse: HIGH -> LOW\r\n");
    HAL_GPIO_WritePin(MC60_PWRKEY_GPIO_Port, MC60_PWRKEY_Pin, GPIO_PIN_RESET);
}

/* ── Wait for boot URCs ────────────────────────────────────────────────────── */
static int mdm_wait_boot(void)
{
    char     line[MDM_LINE_MAX];
    uint32_t start     = HAL_GetTick();
    uint32_t last_diag = start;
    int      got_urc   = 0;

    trace("[MODEM] waiting 12 s for boot...\r\n");

    while ((HAL_GetTick() - start) < 12000U) {
        if ((HAL_GetTick() - last_diag) >= 5000U) {
            last_diag = HAL_GetTick();
            GPIO_PinState cts = HAL_GPIO_ReadPin(MDM_USART2_CTS_GPIO_Port,
                                                  MDM_USART2_CTS_Pin);
            char dbg[96];
            tracef(dbg, sizeof(dbg),
                   "[MODEM] diag: CTS=%s rx_bytes=%lu\r\n",
                   cts == GPIO_PIN_RESET ? "LOW(ok)" : "HIGH(idle)",
                   (unsigned long)s_rx_count);
        }
        if (mdm_getline(line, sizeof(line), 500U, 1)) {
            char dbg[80];
            tracef(dbg, sizeof(dbg), "[MODEM] boot URC: %s\r\n", line);
            if (strcmp(line, "RDY") == 0
                || strstr(line, "+CFUN: 1") != NULL
                || strstr(line, "+QIND: SMS DONE") != NULL) {
                got_urc = 1;
                break;
            }
        }
    }

    if (got_urc) {
        trace("[MODEM] boot URC received — checking AT...\r\n");
    } else {
        trace("[MODEM] 12 s elapsed — checking AT...\r\n");
    }

    for (int attempt = 0; attempt < 3; attempt++) {
        if (mdm_is_alive()) {
            trace("[MODEM] AT -> OK: modem alive\r\n");
            return 1;
        }
        char dbg[48];
        tracef(dbg, sizeof(dbg), "[MODEM] AT attempt %d: no OK\r\n", attempt + 1);
        HAL_Delay(1000U);
    }

    trace("[MODEM] retrying boot wait...\r\n");
    while ((HAL_GetTick() - start) < MDM_BOOT_TIMEOUT_MS) {
        if (mdm_getline(line, sizeof(line), 500U, 1)) {
            char dbg[80];
            tracef(dbg, sizeof(dbg), "[MODEM] boot: %s\r\n", line);
            if (strcmp(line, "RDY") == 0 || strstr(line, "+CFUN: 1") != NULL) {
                return 1;
            }
        }
        if (mdm_is_alive()) { return 1; }
    }
    return 0;
}

/* ── AT base initialisation (echo, SIM, network) ──────────────────────────── */
static int mdm_at_base_init(void)
{
    char buf[80];
    int  rc;

    rc = mdm_cmd("ATE0", "OK", 3000U);
    tracef(buf, sizeof(buf), "[MODEM] ATE0: %s\r\n", rc == 1 ? "PASS" : "FAIL");
    if (rc != 1) { return 0; }

    rc = mdm_cmd("AT+CMEE=1", "OK", 3000U);
    tracef(buf, sizeof(buf), "[MODEM] CMEE: %s\r\n", rc == 1 ? "PASS" : "FAIL");

    /* SIM ready — poll up to 30 s */
    {
        int      sim_ready  = 0;
        uint32_t cpin_start = HAL_GetTick();
        while ((HAL_GetTick() - cpin_start) < 30000U && !sim_ready) {
            rc = mdm_cmd("AT+CPIN?", "+CPIN: READY", 3000U);
            if (rc == 1) {
                sim_ready = 1;
            } else {
                char dbg[64];
                tracef(dbg, sizeof(dbg), "[MODEM] CPIN not ready (%lu s)...\r\n",
                       (unsigned long)((HAL_GetTick() - cpin_start) / 1000U));
                HAL_Delay(2000U);
            }
        }
        tracef(buf, sizeof(buf), "[MODEM] CPIN: %s\r\n", sim_ready ? "PASS" : "FAIL");
        if (!sim_ready) { return 0; }
    }

    (void)mdm_cmd("AT+CREG=1", "OK", 3000U);

    /* Network registration — poll up to 60 s */
    trace("[MODEM] waiting for network registration...\r\n");
    {
        int      registered = 0;
        uint32_t reg_start  = HAL_GetTick();
        while ((HAL_GetTick() - reg_start) < MDM_REG_TIMEOUT_MS && !registered) {
            RingBuf_Flush(&s_rx);
            mdm_tx("AT+CREG?\r\n", 10U);
            char line[64];
            if (mdm_getline(line, sizeof(line), 2000U, 1)) {
                if (strstr(line, ",1") || strstr(line, ",5")
                    || strcmp(line, "+CREG: 1") == 0
                    || strcmp(line, "+CREG: 5") == 0) {
                    registered = 1;
                }
            }
            if (!registered) { HAL_Delay(2000U); }
        }
        tracef(buf, sizeof(buf), "[MODEM] network: %s\r\n",
               registered ? "PASS" : "FAIL");
        if (!registered) { return 0; }
    }

    /* Signal quality (informational) */
    RingBuf_Flush(&s_rx);
    mdm_tx("AT+CSQ\r\n", 8U);
    char csq_line[64];
    if (mdm_getline(csq_line, sizeof(csq_line), 2000U, 1)) {
        tracef(buf, sizeof(buf), "[MODEM] CSQ: %s\r\n", csq_line);
    }

    return 1;
}

/* ── TCP connect (PDP context + socket open) ───────────────────────────────── */
static int mdm_tcp_connect(void)
{
    char buf[128];
    int  rc;

    /* Clean up any leftover socket/context from a previous run */
    trace("[TCP] closing any existing socket...\r\n");
    (void)mdm_cmd("AT+QICLOSE=0", "OK", 5000U);
    HAL_Delay(500U);
    trace("[TCP] deactivating PDP context...\r\n");
    (void)mdm_cmd("AT+QIDEACT=1", "OK", 15000U);
    HAL_Delay(1000U);

    /* Configure APN */
    rc = mdm_cmd("AT+QICSGP=1,1,\"internet\",\"\",\"\",1", "OK", 5000U);
    tracef(buf, sizeof(buf), "[TCP] QICSGP (APN=internet): %s\r\n",
           rc == 1 ? "PASS" : "FAIL");
    if (rc != 1) { return 0; }

    /* Activate PDP context — can take up to 30 s */
    trace("[TCP] activating PDP context (AT+QIACT=1)...\r\n");
    rc = mdm_cmd("AT+QIACT=1", "OK", 30000U);
    tracef(buf, sizeof(buf), "[TCP] QIACT: %s\r\n", rc == 1 ? "PASS" : "FAIL");
    if (rc != 1) { return 0; }

    /* Read and log IP address */
    RingBuf_Flush(&s_rx);
    mdm_tx("AT+QIACT?\r\n", 11U);
    char ip_line[MDM_LINE_MAX];
    if (mdm_getline(ip_line, sizeof(ip_line), 3000U, 1)) {
        tracef(buf, sizeof(buf), "[TCP] IP: %s\r\n", ip_line);
    }

    /* Open TCP socket (access mode 0 = buffer) */
    trace("[TCP] connecting to test.traffilog.co.il:30111...\r\n");
    const char *open_cmd =
        "AT+QIOPEN=1,0,\"TCP\",\"test.traffilog.co.il\",30111,0,0";
    RingBuf_Flush(&s_rx);
    mdm_tx(open_cmd, (uint16_t)strlen(open_cmd));
    mdm_tx("\r\n", 2U);

    /* Wait for +QIOPEN: 0,<err> URC — up to 30 s
     * Format: +QIOPEN: <connectID>,<err>  (err=0 means success) */
    char     qiopen[MDM_LINE_MAX];
    int      got_open  = 0;
    uint32_t ostart    = HAL_GetTick();

    while ((HAL_GetTick() - ostart) < 30000U) {
        if (!mdm_getline(qiopen, sizeof(qiopen), 500U, 0)) { continue; }
        if (qiopen[0] == '\0') { continue; }
        tracef(buf, sizeof(buf), "[TCP] open rsp: %s\r\n", qiopen);

        if (strncmp(qiopen, "+QIOPEN:", 8) == 0) {
            /* Parse error code: second number after comma */
            const char *comma = strchr(qiopen + 8, ',');
            if (comma != NULL) {
                int err = 0;
                const char *ep = comma + 1;
                while (*ep == ' ') { ep++; }
                while (*ep >= '0' && *ep <= '9') {
                    err = err * 10 + (*ep - '0');
                    ep++;
                }
                got_open = (err == 0);
            }
            break;
        }
    }

    tracef(buf, sizeof(buf), "[TCP] connect: %s\r\n", got_open ? "PASS" : "FAIL");
    return got_open;
}

/* ── Read TCP data after +QIURC: "recv",0 ─────────────────────────────────── */
/* Called from mdm_handle_urc_line — single-threaded, ring buffer is live. */
static void mdm_read_tcp_data(void)
{
    /* Request buffered data (up to 1460 bytes) */
    mdm_tx("AT+QIRD=0,1460\r\n", 16U);

    /* Find +QIRD: <len> header */
    char     hdr[MDM_LINE_MAX];
    uint32_t start    = HAL_GetTick();
    int      found    = 0;

    while ((HAL_GetTick() - start) < 3000U && !found) {
        if (!mdm_getline(hdr, sizeof(hdr), 100U, 0)) { continue; }
        if (hdr[0] == '\0') { continue; }
        if (strncmp(hdr, "+QIRD:", 6) == 0) { found = 1; break; }
        if (strcmp(hdr, "OK") == 0)          { return; } /* 0 bytes */
    }

    if (!found) {
        trace("[TCP] QIRD: no +QIRD header\r\n");
        return;
    }

    /* Parse byte count: +QIRD: <len>[,...] */
    int dlen = 0;
    const char *p = hdr + 6;
    while (*p == ' ') { p++; }
    while (*p >= '0' && *p <= '9') {
        dlen = dlen * 10 + (*p - '0');
        p++;
    }

    if (dlen <= 0) { return; }
    if (dlen >= (int)MDM_TCP_BUF_SIZE) {
        dlen = (int)MDM_TCP_BUF_SIZE - 1;
    }

    /* Read exactly dlen raw bytes from ring buffer */
    int got = 0;
    while (got < dlen && (HAL_GetTick() - start) < 5000U) {
        uint8_t b;
        if (RingBuf_Get(&s_rx, &b)) {
            s_tcp_buf[got++] = (char)b;
        }
    }
    s_tcp_buf[got] = '\0';

    /* Trim trailing CR/LF */
    while (got > 0 && (s_tcp_buf[got - 1] == '\r' || s_tcp_buf[got - 1] == '\n')) {
        s_tcp_buf[--got] = '\0';
    }

    if (got > 0) {
        trace("[TCP] recv: ");
        trace(s_tcp_buf);
        trace("\r\n");
    }
}

/* ── URC handler (called from Modem_Process) ──────────────────────────────── */
static void mdm_handle_urc_line(const char *line)
{
    char buf[MDM_LINE_MAX + 32U];

    if (strncmp(line, "+QIURC:", 7) == 0) {
        tracef(buf, sizeof(buf), "[MODEM] URC: %s\r\n", line);

        if (strstr(line, "\"recv\"") != NULL) {
            /* Data available on socket — read it */
            mdm_read_tcp_data();
        } else if (strstr(line, "\"closed\"") != NULL) {
            trace("[TCP] socket closed by remote\r\n");
            s_state = MODEM_STATE_ERROR;
        } else if (strstr(line, "\"pdpdeact\"") != NULL) {
            trace("[TCP] PDP context dropped\r\n");
            s_state = MODEM_STATE_ERROR;
        }
        return;
    }

    if (strncmp(line, "+CREG:", 6) == 0
        || strcmp(line, "RDY") == 0
        || strncmp(line, "+QIND:", 6) == 0) {
        tracef(buf, sizeof(buf), "[MODEM] URC: %s\r\n", line);
    }
}

/* ── Public API ────────────────────────────────────────────────────────────── */

void Modem_RxByte(void)
{
    RingBuf_Put(&s_rx, s_rx_byte);
    s_rx_count++;
    mdm_rx_arm();
}

void Modem_Init(void)
{
    RingBuf_Init(&s_rx);
    s_urc_pos  = 0U;
    s_rx_count = 0U;
    s_state    = MODEM_STATE_BOOTING;

    mdm_rx_arm();

    mdm_power_on();

    if (!mdm_wait_boot()) {
        trace("[MODEM] boot timeout — proceeding anyway\r\n");
    }

    s_state = MODEM_STATE_INIT;
    trace("[MODEM] running AT base init...\r\n");

    if (!mdm_at_base_init()) {
        s_state = MODEM_STATE_ERROR;
        trace("[MODEM] AT init: FAIL\r\n");
        return;
    }
    trace("[MODEM] AT init: PASS\r\n");

    trace("[MODEM] opening TCP socket...\r\n");
    if (!mdm_tcp_connect()) {
        s_state = MODEM_STATE_ERROR;
        trace("[MODEM] TCP connect: FAIL\r\n");
        return;
    }

    s_state = MODEM_STATE_READY;
    trace("[MODEM] TCP connect: PASS — socket READY\r\n");
    trace("\r\n");
    trace("============================================================\r\n");
    trace("  TCP COMMAND GUIDE\r\n");
    trace("============================================================\r\n");
    trace("  To send a message to the server, type:\r\n");
    trace("\r\n");
    trace("     send <message>\r\n");
    trace("\r\n");
    trace("  Example:\r\n");
    trace("     send Hello server!\r\n");
    trace("\r\n");
    trace("  Messages received from the server are printed as:\r\n");
    trace("     [TCP] recv: <data>\r\n");
    trace("============================================================\r\n");
    trace("\r\n");
}

void Modem_Process(void)
{
    uint8_t b;
    while (RingBuf_Get(&s_rx, &b)) {
        if (b == '\n') {
            if (s_urc_pos > 0U && s_urc_line[s_urc_pos - 1U] == '\r') {
                s_urc_pos--;
            }
            s_urc_line[s_urc_pos] = '\0';
            if (s_urc_pos > 0U) {
                mdm_handle_urc_line(s_urc_line);
            }
            s_urc_pos = 0U;
        } else if (b != '\r' && s_urc_pos < (uint16_t)(MDM_LINE_MAX - 1U)) {
            s_urc_line[s_urc_pos++] = (char)b;
        }
    }
}

ModemState_t Modem_GetState(void)
{
    return s_state;
}

int Modem_SendTCP(const char *msg)
{
    char buf[128];

    trace("\r\n");
    trace("------------------------------------------------------------\r\n");
    trace("[TCP] send request\r\n");
    tracef(buf, sizeof(buf), "[TCP]   message: %s\r\n", msg);

    if (s_state != MODEM_STATE_READY) {
        tracef(buf, sizeof(buf),
               "[TCP]   FAIL - not connected (state=%d)\r\n", (int)s_state);
        trace("------------------------------------------------------------\r\n");
        return -1;
    }

    uint16_t mlen = (uint16_t)strlen(msg);
    uint16_t tlen = (uint16_t)(mlen + 2U);  /* +"\r\n" */

    /* AT+QISEND=0,<total_bytes> */
    char cmd[48];
    int  cn = snprintf(cmd, sizeof(cmd), "AT+QISEND=0,%u", (unsigned)tlen);
    if (cn <= 0 || (size_t)cn >= sizeof(cmd)) {
        trace("[TCP]   FAIL - message too long\r\n");
        trace("------------------------------------------------------------\r\n");
        return -1;
    }

    RingBuf_Flush(&s_rx);
    mdm_tx(cmd, (uint16_t)cn);
    mdm_tx("\r\n", 2U);

    /* Wait for '>' prompt */
    uint32_t pstart     = HAL_GetTick();
    int      got_prompt = 0;
    uint8_t  rx;
    while ((HAL_GetTick() - pstart) < 5000U) {
        while (RingBuf_Get(&s_rx, &rx)) {
            if (rx == '>') { got_prompt = 1; break; }
        }
        if (got_prompt) { break; }
    }

    if (!got_prompt) {
        trace("[TCP]   FAIL - no '>' prompt\r\n");
        trace("------------------------------------------------------------\r\n");
        return -1;
    }

    HAL_Delay(20U);              /* small pause after prompt */
    mdm_tx(msg, mlen);           /* message text */
    mdm_tx("\r\n", 2U);          /* line terminator for server */

    /* Wait for SEND OK */
    int rc = mdm_wait_for("SEND OK", 10000U);
    tracef(buf, sizeof(buf), "[TCP]   send: %s\r\n", rc == 1 ? "OK" : "FAIL");
    trace("------------------------------------------------------------\r\n\r\n");
    return rc == 1 ? 0 : -1;
}
