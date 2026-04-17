/* tx_initialize_low_level.S — Cortex-M33 GNU port, STM32H573 @ 240 MHz
 *
 * Adapted from
 *   STM32Cube_FW_H5_V1.6.0/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/
 *   tx_initialize_low_level.S
 * Keeping only the GCC section (other toolchain blocks stripped) and retuning
 * SYSTEM_CLOCK to match our SYSCLK so the SysTick reload gives a 100 Hz tick.
 *
 * Vector-table symbol: g_pfnVectors (CMSIS startup_stm32h573xx.s).
 * Static allocation is used → USE_DYNAMIC_MEMORY_ALLOCATION left undefined.
 */

SYSTEM_CLOCK      =   240000000
SYSTICK_CYCLES    =   ((SYSTEM_CLOCK / 100) - 1)

    .section .text
    .balign 4
    .syntax unified
    .eabi_attribute Tag_ABI_align_preserved, 1
    .global  _tx_initialize_low_level
    .thumb_func
.type _tx_initialize_low_level, function
_tx_initialize_low_level:

    CPSID   i

    /* Vector Table Offset Register → g_pfnVectors. */
    MOV     r0, #0xE000E000
    LDR     r1, =g_pfnVectors
    STR     r1, [r0, #0xD08]

    /* Enable the cycle counter. */
    LDR     r0, =0xE0001000
    LDR     r1, [r0]
    ORR     r1, r1, #1
    STR     r1, [r0]

    /* Save the reset-vector stack pointer as the ThreadX system stack. */
    LDR     r0, =_tx_thread_system_stack_ptr
    LDR     r1, =g_pfnVectors
    LDR     r1, [r1]
    STR     r1, [r0]

    /* SysTick → 100 Hz (10 ms tick). */
    MOV     r0, #0xE000E000
    LDR     r1, =SYSTICK_CYCLES
    STR     r1, [r0, #0x14]
    MOV     r1, #0x7
    STR     r1, [r0, #0x10]

    /* System-handler priorities: SVC lowest-but-one, PendSV & SysTick lowest. */
    LDR     r1, =0x00000000
    STR     r1, [r0, #0xD18]
    LDR     r1, =0xFF000000
    STR     r1, [r0, #0xD1C]
    LDR     r1, =0x40FF0000
    STR     r1, [r0, #0xD20]

    BX      lr

/* Default shells for unused ThreadX vector slots. */
    .section .text
    .balign 4
    .syntax unified
    .eabi_attribute Tag_ABI_align_preserved, 1
    .global  __tx_BadHandler
    .thumb_func
.type __tx_BadHandler, function
__tx_BadHandler:
    B       __tx_BadHandler

    .section .text
    .balign 4
    .syntax unified
    .eabi_attribute Tag_ABI_align_preserved, 1
    .global  __tx_IntHandler
    .thumb_func
.type __tx_IntHandler, function
__tx_IntHandler:
    PUSH    {r0,lr}
    POP     {r0,lr}
    BX      lr

    .section .text
    .balign 4
    .syntax unified
    .eabi_attribute Tag_ABI_align_preserved, 1
    .global  SysTick_Handler
    .thumb_func
.type SysTick_Handler, function
SysTick_Handler:
    PUSH    {r0,lr}
    BL      _tx_timer_interrupt
    POP     {r0,lr}
    BX      lr

    .section .text
    .balign 4
    .syntax unified
    .eabi_attribute Tag_ABI_align_preserved, 1
    .global  __tx_NMIHandler
    .thumb_func
.type __tx_NMIHandler, function
__tx_NMIHandler:
    B       __tx_NMIHandler

    .section .text
    .balign 4
    .syntax unified
    .eabi_attribute Tag_ABI_align_preserved, 1
    .global  __tx_DBGHandler
    .thumb_func
.type __tx_DBGHandler, function
__tx_DBGHandler:
    B       __tx_DBGHandler

    .end
