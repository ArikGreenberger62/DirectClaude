/* stm32h5xx_it.c — interrupt handlers for 03-gsensor */

#include "main.h"

/* ── SysTick_Handler ─────────────────────────────────────────────────────── */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ── __errno stub ────────────────────────────────────────────────────────── */
/* nano.specs links libm which references __errno.  newlib-nano does not
 * provide it.  Provide a static stub so the linker is satisfied.
 * Bare-metal code never triggers errno-setting math conditions.            */
int *__errno(void)
{
    static int s_errno = 0;
    return &s_errno;
}
