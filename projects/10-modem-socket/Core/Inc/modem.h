/* modem.h — Quectel EG915N TCP socket driver for TFL_CONNECT_2.
 *
 * Hardware:
 *   USART2  PA2/PA3/PD3/PD4  — modem UART with HW flow control (CTS/RTS)
 *   PB8  MC60_GNSS_PWR        — modem power supply enable (HIGH = on)
 *   PD7  MC60_PWRKEY           — power key pulse (HIGH 600 ms → modem on)
 *   PC11 P3V3_SW_EN            — 3.3V switched supply enable (HIGH = on)
 *
 * Network:
 *   APN   : internet
 *   Server: test.traffilog.co.il : 30111 (TCP)
 *   Socket: connectID=0, access mode=0 (buffer)
 *
 * Call order:
 *   1. Modem_Init()      — power on, boot, AT init, TCP connect (blocking).
 *   2. Modem_Process()   — call every main-loop iteration to handle URCs.
 *   3. Modem_SendTCP()   — send null-terminated string over TCP (appends \r\n).
 *
 * Trace output: UART7 (115200 8N1, PE7/PE8).
 * Incoming TCP data prints: "[TCP] recv: <data>\r\n"
 */
#ifndef MODEM_H
#define MODEM_H

#include <stdint.h>

typedef enum {
    MODEM_STATE_OFF = 0,  /* not yet initialised                          */
    MODEM_STATE_BOOTING,  /* powered on, waiting for boot URCs            */
    MODEM_STATE_INIT,     /* running AT init + TCP connect sequence        */
    MODEM_STATE_READY,    /* TCP socket connected, ready to send/receive   */
    MODEM_STATE_ERROR,    /* unrecoverable fault                           */
} ModemState_t;

/* Called once from main after all HAL peripherals are initialised.
 * Blocks until TCP socket is connected or timeout (~2 min worst case). */
void Modem_Init(void);

/* Call from main loop every iteration.
 * Drains USART2 RX ring buffer, processes modem URCs (+QIURC:, etc.).
 * Prints received TCP data to UART7. */
void Modem_Process(void);

/* Returns current modem state. */
ModemState_t Modem_GetState(void);

/* Send null-terminated string over the open TCP socket.
 * Automatically appends "\r\n" before transmitting.
 * Blocking — returns when done or failed.
 * Returns  0 on success, -1 on failure. */
int Modem_SendTCP(const char *msg);

/* Called from HAL_UART_RxCpltCallback in main.c when a byte arrives on USART2.
 * Reads the internal receive buffer directly. */
void Modem_RxByte(void);

#endif /* MODEM_H */
