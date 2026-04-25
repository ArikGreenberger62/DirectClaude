/* modem.h — Quectel EG915N modem driver for TFL_CONNECT_2 board.
 *
 * Hardware:
 *   USART2  PA2/PA3/PD3/PD4  — modem UART with HW flow control (CTS/RTS)
 *   PB8  MC60_GNSS_PWR        — modem power supply enable (HIGH = on)
 *   PD7  MC60_PWRKEY           — power key pulse (HIGH 600 ms → modem on)
 *   PC11 P3V3_SW_EN            — 3.3V switched supply enable (HIGH = on)
 *
 * Call order:
 *   1. Modem_Init()     — power on, wait for boot, send AT init sequence.
 *   2. Modem_Process()  — call every main-loop iteration to handle URCs.
 *   3. Modem_SendSMS()  — blocking SMS send (up to ~35 s with network wait).
 *
 * Trace output goes to UART7 (115200 8N1, PE7/PE8).
 * Incoming SMS prints: "sms received from <sender> , <body>\r\n"
 */
#ifndef MODEM_H
#define MODEM_H

#include <stdint.h>

typedef enum {
    MODEM_STATE_OFF     = 0,  /* not yet initialised                          */
    MODEM_STATE_BOOTING,      /* powered on, waiting for RDY URC              */
    MODEM_STATE_INIT,         /* sending AT init sequence                     */
    MODEM_STATE_READY,        /* fully initialised, SMS ready                 */
    MODEM_STATE_ERROR,        /* unrecoverable fault                          */
} ModemState_t;

/* Called once from main after all HAL peripherals are initialised.
 * Blocks until modem is ready or timeout (up to ~90 s worst case). */
void Modem_Init(void);

/* Call from main loop every iteration.
 * Drains USART2 RX ring buffer, processes URCs (+CMT, etc.).
 * Must NOT be called while a blocking Modem_SendSMS is in progress
 * (single-threaded — Modem_SendSMS and Modem_Process share the ring buffer). */
void Modem_Process(void);

/* Returns current modem state. */
ModemState_t Modem_GetState(void);

/* Send SMS to <phone> with <msg> text. Blocking — returns when done or failed.
 * Returns  0 on success, -1 on failure.
 * phone : E.164 string, e.g. "+972501234567"  (max 20 chars)
 * msg   : ASCII text, max 160 chars. */
int Modem_SendSMS(const char *phone, const char *msg);

/* Called from HAL_UART_RxCpltCallback in main.c when a byte arrives on USART2.
 * Reads s_rx_byte internally (the buffer passed to HAL_UART_Receive_IT).
 * Do NOT pass pRxBuffPtr — it is already incremented past the byte at callback time. */
void Modem_RxByte(void);

#endif /* MODEM_H */
