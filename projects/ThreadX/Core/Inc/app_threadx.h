/* app_threadx.h — ThreadX application entry points */
#ifndef APP_THREADX_H
#define APP_THREADX_H

#include "tx_api.h"
#include "main.h"

/* Kernel enter (called from main) */
void MX_ThreadX_Init(void);

/* Static byte-pool backing storage (see app_threadx.c) */
#define TX_APP_MEM_POOL_SIZE   (4U * 1024U)

/* Green-LED thread */
#define GREEN_THREAD_STACK     (1U * 1024U)
#define GREEN_THREAD_PRIO      10U

/* Trace thread (UART7 heartbeat) */
#define TRACE_THREAD_STACK     (1U * 1024U)
#define TRACE_THREAD_PRIO      12U

/* Red-LED timer: 50 ticks * 10 ms = 500 ms half-period → 1 Hz blink */
#define RED_TIMER_HALF_TICKS   50U
/* Heartbeat trace period: 100 ticks = 1 s */
#define TRACE_PERIOD_TICKS     100U

/* Counters exposed for the trace thread. */
extern volatile ULONG g_red_toggles;
extern volatile ULONG g_green_toggles;

#endif /* APP_THREADX_H */
