/* app_threadx.c — ThreadX application wiring for the blink demo
 *
 * - Red LED (PC8)    : toggled by a ThreadX software timer callback (RedTimer_Cb).
 * - Green LED (PC9)  : toggled by a dedicated thread (Green_Entry).
 * - UART7 heartbeat  : dedicated trace thread (Trace_Entry) emits a line every
 *                      second so the host can verify the kernel is alive and
 *                      both blink paths are running at the expected rate.
 *
 * Static allocation keeps the linker script untouched: the byte pool lives in
 * a fixed BSS buffer; all thread stacks are carved out of that pool.
 */

#include "app_threadx.h"
#include <stdio.h>
#include <string.h>

volatile ULONG g_red_toggles   = 0;
volatile ULONG g_green_toggles = 0;

static TX_BYTE_POOL  app_byte_pool;
static UCHAR         app_byte_pool_buf[TX_APP_MEM_POOL_SIZE];

static TX_THREAD     green_thread;
static TX_THREAD     trace_thread;
static TX_TIMER      red_timer;

static VOID Green_Entry(ULONG arg);
static VOID Trace_Entry(ULONG arg);
static VOID RedTimer_Cb(ULONG arg);

static inline UINT create_thread(TX_THREAD *t, const char *name,
                                 VOID (*entry)(ULONG), ULONG stack_bytes, UINT prio)
{
    CHAR *stack_ptr;
    if (tx_byte_allocate(&app_byte_pool, (VOID **)&stack_ptr,
                         stack_bytes, TX_NO_WAIT) != TX_SUCCESS) { return TX_POOL_ERROR; }
    return tx_thread_create(t, (CHAR *)name, entry, 0U,
                            stack_ptr, stack_bytes, prio, prio,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
}

/* Called by tx_kernel_enter once the scheduler is up enough to create objects.
 * Runs BEFORE any thread, in a restricted context: use create/allocate
 * primitives only, no blocking calls. */
VOID tx_application_define(VOID *first_unused_memory)
{
    (void)first_unused_memory;

    if (tx_byte_pool_create(&app_byte_pool, "app pool",
                            app_byte_pool_buf, TX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
    { Error_Handler(); }

    if (create_thread(&green_thread, "green", Green_Entry,
                      GREEN_THREAD_STACK, GREEN_THREAD_PRIO) != TX_SUCCESS)
    { Error_Handler(); }

    if (create_thread(&trace_thread, "trace", Trace_Entry,
                      TRACE_THREAD_STACK, TRACE_THREAD_PRIO) != TX_SUCCESS)
    { Error_Handler(); }

    /* Auto-reload timer: fires every RED_TIMER_HALF_TICKS ticks (500 ms). */
    if (tx_timer_create(&red_timer, "red", RedTimer_Cb, 0U,
                        RED_TIMER_HALF_TICKS, RED_TIMER_HALF_TICKS,
                        TX_AUTO_ACTIVATE) != TX_SUCCESS)
    { Error_Handler(); }
}

void MX_ThreadX_Init(void)
{
    tx_kernel_enter();
}

static VOID Green_Entry(ULONG arg)
{
    (void)arg;
    while (1)
    {
        HAL_GPIO_TogglePin(LED_G_PORT, LED_G_PIN);
        g_green_toggles++;
        tx_thread_sleep(RED_TIMER_HALF_TICKS);
    }
}

/* Runs in the timer-thread context (NOT an ISR), so HAL_GPIO calls are fine. */
static VOID RedTimer_Cb(ULONG arg)
{
    (void)arg;
    HAL_GPIO_TogglePin(LED_R_PORT, LED_R_PIN);
    g_red_toggles++;
}

/* 1 Hz heartbeat on UART7. Also serves as the on-target self-test:
 * - if tick is not advancing at ~100 Hz, the line frequency will be wrong.
 * - if red/green counters don't track tick/50, the blinkers are not running.
 */
static VOID Trace_Entry(ULONG arg)
{
    (void)arg;
    char   line[96];
    ULONG  next = tx_time_get();

    while (1)
    {
        next += TRACE_PERIOD_TICKS;
        tx_thread_sleep(TRACE_PERIOD_TICKS);

        int n = snprintf(line, sizeof line,
                         "[THX] tick=%lu red=%lu green=%lu\r\n",
                         (unsigned long)tx_time_get(),
                         (unsigned long)g_red_toggles,
                         (unsigned long)g_green_toggles);
        if (n > 0)
        {
            (void)HAL_UART_Transmit(&huart7, (uint8_t *)line, (uint16_t)n, 100U);
        }
    }
}
