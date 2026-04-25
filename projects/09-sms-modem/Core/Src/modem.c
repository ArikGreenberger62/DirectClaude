/* modem.c — Quectel EG915N driver for TFL_CONNECT_2 (USART2 + GPIO).
 *
 * Power-on sequence:
 *   1. P3V3_SW_EN (PC11) HIGH — 3.3V supply (already set by MX_GPIO_Init)
 *   2. MC60_GNSS_PWR (PB8) HIGH — modem power supply
 *   3. Check if modem already on (send AT, expect OK within 500ms)
 *   4. If not on: pulse MC60_PWRKEY (PD7) HIGH 600ms → LOW
 *      (circuit assumption: MCU HIGH → transistor → modem PWRKEY LOW, active-low)
 *   5. Wait up to 30 s for "RDY" or "+CFUN: 1" URC
 *
 * AT init sequence: ATE0, CMEE=1, CPIN?, CREG, wait for registration,
 *                   CMGF=1, CSCS=IRA, CSDH=0, CNMI=2:2, CMGD=1:4
 *
 * Received SMS (+CMT URC) is printed to UART7:
 *   "sms received from <sender> , <body>\r\n"
 */
#include "modem.h"
#include "ring_buf.h"
#include "usart.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ── Private constants ─────────────────────────────────────────────────────── */
#define MDM_LINE_MAX         256U
#define MDM_PHONE_MAX         32U
#define MDM_BOOT_TIMEOUT_MS   45000U    /* modem UART up in ~30 ms, CFUN in ~10 s */
#define MDM_REG_TIMEOUT_MS    60000U
#define MDM_SMS_SEND_TIMEOUT  30000U
#define MDM_PROMPT_TIMEOUT    5000U
#define MDM_VBAT_SETTLE_MS   2000U      /* conservative: let modem PMU rails stabilise */
#define MDM_PWRKEY_PRELOW_MS   50U      /* make sure PWRKEY rests LOW before pulse */
#define MDM_PWRKEY_PULSE_MS   1000U     /* active-high pulse on MCU (≥500 ms)        */

/* ── Module-private state ──────────────────────────────────────────────────── */
static RingBuf_t     s_rx;            /* USART2 receive ring buffer               */
static uint8_t       s_rx_byte;       /* single-byte HAL interrupt receive buffer  */
static uint32_t      s_rx_count;      /* total bytes received from modem (diag)    */

static ModemState_t  s_state = MODEM_STATE_OFF;

/* Line assembly for Modem_Process (non-blocking URC handler) */
static char          s_urc_line[MDM_LINE_MAX];
static uint16_t      s_urc_pos;

/* +CMT two-line protocol state */
static int           s_expect_sms_body;
static char          s_sms_sender[MDM_PHONE_MAX];

/* ── Private helpers ───────────────────────────────────────────────────────── */

static void trace(const char *msg)
{
    HAL_UART_Transmit(&huart7, (const uint8_t *)msg, (uint16_t)strlen(msg), 1000U);
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

/* Send raw bytes to modem USART2. */
static void mdm_tx(const char *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (const uint8_t *)data, len, 2000U);
}

/* Re-arm USART2 interrupt-driven RX (one byte at a time). */
static void mdm_rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1U);
}

/* Wait for a complete line from modem RX ring buffer.
 * Returns 1 when line is complete (in buf, NUL-terminated, no \r\n).
 * Returns 0 on timeout. Blank lines (empty) are skipped when skip_blank=1. */
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
                continue;   /* skip blank line, keep reading */
            }
            return 1;
        } else if (b != '\r' && pos < (uint16_t)(maxlen - 1U)) {
            buf[pos++] = (char)b;
        }
    }
    buf[pos] = '\0';
    return 0;
}

/* Send a command and wait for expected string in any response line.
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
            if (line[0] == '\0') { continue; }    /* blank line */
            if (expect != NULL && strstr(line, expect) != NULL) { return 1; }
            if (strstr(line, "ERROR") != NULL)                  { return -1; }
        } else if (b != '\r' && pos < (uint16_t)(MDM_LINE_MAX - 1U)) {
            line[pos++] = (char)b;
        }
    }
    return 0;   /* timeout */
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
            if (strstr(line, expect) != NULL)    { return 1; }
            if (strstr(line, "ERROR") != NULL)   { return -1; }
        } else if (b != '\r' && pos < (uint16_t)(MDM_LINE_MAX - 1U)) {
            line[pos++] = (char)b;
        }
    }
    return 0;
}

/* Check if modem is already on: send AT and expect OK within 1 s. */
static int mdm_is_alive(void)
{
    RingBuf_Flush(&s_rx);
    mdm_tx("AT\r\n", 4U);
    return mdm_wait_for("OK", 1000U) == 1;
}

/* Power-on sequence.
 *
 * Board electrical behaviour (confirmed by user):
 *   MC60_GNSS_PWR (PB8)  — HIGH turns modem VBAT ON
 *   MC60_PWRKEY   (PD7)  — positive pulse on the MCU pin (0 → 1 → 0) triggers
 *                          the modem boot. Pin idles LOW.
 *
 * Sequence:
 *   1. Ensure PWRKEY is LOW (defensive — starting edge must be clean).
 *   2. Enable P3V3_SW_EN (already SET by MX_GPIO_Init, re-assert for safety).
 *   3. Drive MC60_GNSS_PWR HIGH  → modem VBAT comes up.
 *   4. Wait MDM_VBAT_SETTLE_MS (≥30 ms spec, we use 500 ms).
 *   5. If the modem is already alive (AT→OK within 1 s), skip the pulse so we
 *      do not accidentally toggle a running modem off/on.
 *   6. Otherwise: drive PWRKEY HIGH for MDM_PWRKEY_PULSE_MS, then LOW.
 */
static void mdm_power_on(void)
{
    /* 1. Defensive: force PWRKEY LOW as the resting state, wait to settle. */
    HAL_GPIO_WritePin(MC60_PWRKEY_GPIO_Port, MC60_PWRKEY_Pin, GPIO_PIN_RESET);
    HAL_Delay(MDM_PWRKEY_PRELOW_MS);

    /* 2-3. Bring up supplies. */
    trace("[MODEM] P3V3_SW_EN = HIGH\r\n");
    HAL_GPIO_WritePin(P3V3_SW_EN_GPIO_Port, P3V3_SW_EN_Pin, GPIO_PIN_SET);

    trace("[MODEM] MC60_GNSS_PWR = HIGH (VBAT on)\r\n");
    HAL_GPIO_WritePin(MC60_GNSS_PWR_GPIO_Port, MC60_GNSS_PWR_Pin, GPIO_PIN_SET);

    /* Also enable MC60_PWR (PD12) — extra power rail; harmless if not fitted. */
    trace("[MODEM] MC60_PWR = HIGH\r\n");
    HAL_GPIO_WritePin(MC60_PWR_GPIO_Port, MC60_PWR_Pin, GPIO_PIN_SET);

    /* 4. Wait for VBAT to stabilise (spec ≥30 ms). */
    HAL_Delay(MDM_VBAT_SETTLE_MS);

    /* 5. Skip the pulse if the modem is already running (e.g. MCU-only reset). */
    if (mdm_is_alive()) {
        trace("[MODEM] modem already on (AT -> OK)\r\n");
        return;
    }

    /* 6. Positive PWRKEY pulse: 0 -> 1 (hold) -> 0. */
    trace("[MODEM] PWRKEY pulse: LOW -> HIGH\r\n");
    HAL_GPIO_WritePin(MC60_PWRKEY_GPIO_Port, MC60_PWRKEY_Pin, GPIO_PIN_SET);
    HAL_Delay(MDM_PWRKEY_PULSE_MS);

    trace("[MODEM] PWRKEY pulse: HIGH -> LOW\r\n");
    HAL_GPIO_WritePin(MC60_PWRKEY_GPIO_Port, MC60_PWRKEY_Pin, GPIO_PIN_RESET);
}

/* Wait for boot completion.
 *
 * Strategy:
 *   1. Watch for boot URCs (RDY / +CFUN: 1) for up to 12 s — if one arrives
 *      early, skip the fixed wait.
 *   2. After 12 s (or a URC), send AT and check for OK (modem alive check).
 *   3. If AT→OK: return 1.
 *   4. Otherwise keep retrying until MDM_BOOT_TIMEOUT_MS is exhausted.
 */
static int mdm_wait_boot(void)
{
    char     line[MDM_LINE_MAX];
    uint32_t start      = HAL_GetTick();
    uint32_t last_diag  = start;
    int      got_urc    = 0;

    trace("[MODEM] waiting 12 s for modem to boot...\r\n");

    /* Phase 1: watch for boot URCs for up to 12 s.
     * Print CTS state + RX byte count every 5 s (hardware diagnostic). */
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
        trace("[MODEM] 12 s elapsed — checking modem alive (AT)...\r\n");
    }

    /* Phase 2: AT liveness check (up to 3 attempts, 1 s each). */
    for (int attempt = 0; attempt < 3; attempt++) {
        if (mdm_is_alive()) {
            trace("[MODEM] AT -> OK: modem is alive\r\n");
            return 1;
        }
        char dbg[48];
        tracef(dbg, sizeof(dbg), "[MODEM] AT attempt %d: no OK\r\n", attempt + 1);
        HAL_Delay(1000U);
    }

    /* Phase 3: keep trying until the overall boot timeout expires. */
    trace("[MODEM] initial AT check failed — waiting for URCs or AT...\r\n");
    while ((HAL_GetTick() - start) < MDM_BOOT_TIMEOUT_MS) {
        if (mdm_getline(line, sizeof(line), 500U, 1)) {
            char dbg[80];
            tracef(dbg, sizeof(dbg), "[MODEM] boot: %s\r\n", line);
            if (strcmp(line, "RDY") == 0
                || strstr(line, "+CFUN: 1") != NULL
                || strstr(line, "+QIND: SMS DONE") != NULL) {
                return 1;
            }
        }
        if (mdm_is_alive()) { return 1; }
    }
    return 0;
}

/* Run AT initialisation sequence. Returns 1 on success, 0 on any failure. */
static int mdm_at_init(void)
{
    char buf[80];
    int  rc;

    /* Disable echo */
    rc = mdm_cmd("ATE0", "OK", 3000U);
    tracef(buf, sizeof(buf), "[MODEM] ATE0: %s\r\n", rc == 1 ? "PASS" : "FAIL");
    if (rc != 1) { return 0; }

    /* Extended error codes */
    rc = mdm_cmd("AT+CMEE=1", "OK", 3000U);
    tracef(buf, sizeof(buf), "[MODEM] CMEE: %s\r\n", rc == 1 ? "PASS" : "FAIL");

    /* SIM status — poll up to 30 s; SIM may take 15-20 s after RDY to initialise.
     * Match "+CPIN: READY" (not "READY") to avoid falsely matching "+CPIN: NOT READY". */
    {
        int sim_ready = 0;
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

    /* Enable registration URC */
    (void)mdm_cmd("AT+CREG=1", "OK", 3000U);

    /* Wait for network registration */
    trace("[MODEM] waiting for network registration...\r\n");
    uint32_t reg_start = HAL_GetTick();
    int registered = 0;
    while ((HAL_GetTick() - reg_start) < MDM_REG_TIMEOUT_MS && !registered) {
        rc = mdm_cmd("AT+CREG?", "OK", 3000U);
        /* Re-read ring buffer for the actual +CREG response */
        (void)rc;
        /* Use a targeted check */
        RingBuf_Flush(&s_rx);
        mdm_tx("AT+CREG?\r\n", 10U);
        char line[64];
        if (mdm_getline(line, sizeof(line), 2000U, 1)) {
            /* +CREG: <n>,<stat> or +CREG: <stat> */
            if (strstr(line, ",1") || strstr(line, ",5")
                || strcmp(line, "+CREG: 1") == 0
                || strcmp(line, "+CREG: 5") == 0) {
                registered = 1;
            }
        }
        if (!registered) { HAL_Delay(2000U); }
    }
    tracef(buf, sizeof(buf), "[MODEM] network: %s\r\n", registered ? "PASS" : "FAIL");
    if (!registered) { return 0; }

    /* Signal quality */
    RingBuf_Flush(&s_rx);
    mdm_tx("AT+CSQ\r\n", 8U);
    char csq_line[64];
    if (mdm_getline(csq_line, sizeof(csq_line), 2000U, 1)) {
        tracef(buf, sizeof(buf), "[MODEM] CSQ: %s\r\n", csq_line);
    }

    /* SMS text mode */
    rc = mdm_cmd("AT+CMGF=1", "OK", 3000U);
    tracef(buf, sizeof(buf), "[MODEM] CMGF=1: %s\r\n", rc == 1 ? "PASS" : "FAIL");
    if (rc != 1) { return 0; }

    /* IRA charset (ASCII compatible) */
    rc = mdm_cmd("AT+CSCS=\"IRA\"", "OK", 3000U);
    tracef(buf, sizeof(buf), "[MODEM] CSCS=IRA: %s\r\n", rc == 1 ? "PASS" : "FAIL");

    /* Simplified +CMT headers */
    (void)mdm_cmd("AT+CSDH=0", "OK", 3000U);

    /* Route incoming SMS directly to TE */
    rc = mdm_cmd("AT+CNMI=2,2,0,0,0", "OK", 3000U);
    tracef(buf, sizeof(buf), "[MODEM] CNMI: %s\r\n", rc == 1 ? "PASS" : "FAIL");
    if (rc != 1) { return 0; }

    /* Delete all stored messages (clean slate) */
    (void)mdm_cmd("AT+CMGD=1,4", "OK", 5000U);

    return 1;
}

/* Parse sender phone number from +CMT header line.
 * Line format: +CMT: "<number>","","<timestamp>" */
static void mdm_parse_cmt_sender(const char *line, char *sender, uint16_t maxlen)
{
    const char *p = strchr(line, '"');
    if (p == NULL) { sender[0] = '\0'; return; }
    p++;   /* skip opening quote */
    const char *q = strchr(p, '"');
    if (q == NULL) { sender[0] = '\0'; return; }
    uint16_t len = (uint16_t)(q - p);
    if (len >= maxlen) { len = (uint16_t)(maxlen - 1U); }
    strncpy(sender, p, len);
    sender[len] = '\0';
}

/* Process a complete line received from the modem (URC handler).
 * Called only from Modem_Process (non-blocking context). */
static void mdm_handle_urc_line(const char *line)
{
    char buf[MDM_LINE_MAX + 64U];

    if (s_expect_sms_body) {
        /* This line is the SMS message body */
        s_expect_sms_body = 0;
        tracef(buf, sizeof(buf),
               "sms received from %s , %s\r\n", s_sms_sender, line);
        return;
    }

    if (strncmp(line, "+CMT:", 5) == 0) {
        mdm_parse_cmt_sender(line, s_sms_sender, sizeof(s_sms_sender));
        s_expect_sms_body = 1;
        return;
    }

    /* Other notable URCs — log to trace */
    if (strncmp(line, "+CREG:", 6) == 0
        || strncmp(line, "+QIND:", 6) == 0
        || strcmp(line, "RDY") == 0) {
        tracef(buf, sizeof(buf), "[MODEM] URC: %s\r\n", line);
    }
}

/* ── Public API ────────────────────────────────────────────────────────────── */

void Modem_RxByte(void)
{
    /* s_rx_byte is the buffer passed to HAL_UART_Receive_IT.
     * The HAL writes the received byte there and increments pRxBuffPtr before
     * calling this callback, so reading pRxBuffPtr in the caller would give
     * garbage (one byte past the buffer). Read s_rx_byte directly instead. */
    RingBuf_Put(&s_rx, s_rx_byte);
    s_rx_count++;
    mdm_rx_arm();
}

void Modem_Init(void)
{
    RingBuf_Init(&s_rx);
    s_urc_pos        = 0U;
    s_expect_sms_body = 0;
    s_sms_sender[0]  = '\0';
    s_state          = MODEM_STATE_BOOTING;

    mdm_rx_arm();

    mdm_power_on();
    s_state = MODEM_STATE_BOOTING;

    if (!mdm_wait_boot()) {
        trace("[MODEM] boot timeout — proceeding anyway\r\n");
    }

    s_state = MODEM_STATE_INIT;
    trace("[MODEM] running AT init sequence\r\n");

    if (mdm_at_init()) {
        s_state = MODEM_STATE_READY;
        trace("[MODEM] init: PASS - modem READY\r\n");
        trace("\r\n");
        trace("============================================================\r\n");
        trace("  SMS COMMAND GUIDE\r\n");
        trace("============================================================\r\n");
        trace("  To send an SMS, type a line in this terminal:\r\n");
        trace("\r\n");
        trace("     sendsms +<phone> , <message>\r\n");
        trace("\r\n");
        trace("  Rules:\r\n");
        trace("    * phone MUST start with '+' and country code\r\n");
        trace("    * separator between phone and message is  space comma space\r\n");
        trace("    * message is plain ASCII, up to 160 characters\r\n");
        trace("    * press ENTER to submit the line\r\n");
        trace("\r\n");
        trace("  Example:\r\n");
        trace("     sendsms +972501234567 , Hello World\r\n");
        trace("\r\n");
        trace("  Incoming SMS is printed as:\r\n");
        trace("     sms received from <phone> , <message>\r\n");
        trace("============================================================\r\n");
        trace("\r\n");
    } else {
        s_state = MODEM_STATE_ERROR;
        trace("[MODEM] init: FAIL - check SIM and antenna\r\n");
    }
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

int Modem_SendSMS(const char *phone, const char *msg)
{
    /* Buffer sized for worst case: 160-char msg + prefix + phone. */
    char buf[256];

    /* ── Echo the request to the trace so the user sees what we received ──── */
    trace("\r\n");
    trace("------------------------------------------------------------\r\n");
    trace("[SMS] Send request received\r\n");
    tracef(buf, sizeof(buf), "[SMS]   Destination : %s\r\n", phone);
    tracef(buf, sizeof(buf), "[SMS]   Message     : %s\r\n", msg);
    tracef(buf, sizeof(buf), "[SMS]   Length      : %u chars\r\n",
           (unsigned)strlen(msg));
    trace("------------------------------------------------------------\r\n");

    if (s_state != MODEM_STATE_READY) {
        tracef(buf, sizeof(buf),
               "[SMS] RESULT: FAIL - modem not ready (state=%d)\r\n",
               (int)s_state);
        tracef(buf, sizeof(buf),
               "[SMS]   to \"%s\" : \"%s\"\r\n\r\n", phone, msg);
        return -1;
    }

    trace("[SMS] sending to modem ...\r\n");

    /* Build AT+CMGS="<phone>" */
    char cmd[64];
    int n = snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", phone);
    if (n <= 0 || (size_t)n >= sizeof(cmd)) {
        tracef(buf, sizeof(buf),
               "[SMS] RESULT: FAIL - phone number too long\r\n");
        tracef(buf, sizeof(buf),
               "[SMS]   to \"%s\" : \"%s\"\r\n\r\n", phone, msg);
        return -1;
    }

    /* Flush old data */
    RingBuf_Flush(&s_rx);

    /* Send command — use \r (not \r\n) to trigger > prompt */
    mdm_tx(cmd, (uint16_t)n);
    mdm_tx("\r", 1U);

    /* Wait for '>' prompt (raw character scan, not line-oriented) */
    uint32_t start = HAL_GetTick();
    int got_prompt = 0;
    uint8_t rx;
    while ((HAL_GetTick() - start) < MDM_PROMPT_TIMEOUT) {
        while (RingBuf_Get(&s_rx, &rx)) {
            if (rx == '>') { got_prompt = 1; break; }
        }
        if (got_prompt) { break; }
    }

    if (!got_prompt) {
        tracef(buf, sizeof(buf),
               "[SMS] RESULT: FAIL - no '>' prompt from modem\r\n");
        tracef(buf, sizeof(buf),
               "[SMS]   to \"%s\" : \"%s\"\r\n\r\n", phone, msg);
        return -1;
    }

    HAL_Delay(20U);   /* small pause after prompt (modem recommendation) */

    /* Send message text */
    uint16_t msglen = (uint16_t)strlen(msg);
    mdm_tx(msg, msglen);

    /* Ctrl+Z to commit */
    mdm_tx("\x1A", 1U);

    /* Wait for +CMGS: <mr> response */
    int rc = mdm_wait_for("+CMGS:", MDM_SMS_SEND_TIMEOUT);
    if (rc == 1) {
        tracef(buf, sizeof(buf),
               "[SMS] RESULT: SUCCESS - SMS delivered to modem\r\n");
        tracef(buf, sizeof(buf),
               "[SMS]   to \"%s\" : \"%s\"\r\n", phone, msg);
        trace("------------------------------------------------------------\r\n\r\n");
        return 0;
    } else {
        const char *reason = (rc == -1) ? "ERROR from modem"
                                        : "timeout waiting for +CMGS";
        tracef(buf, sizeof(buf),
               "[SMS] RESULT: FAIL - %s\r\n", reason);
        tracef(buf, sizeof(buf),
               "[SMS]   to \"%s\" : \"%s\"\r\n", phone, msg);
        trace("------------------------------------------------------------\r\n\r\n");
        return -1;
    }
}
