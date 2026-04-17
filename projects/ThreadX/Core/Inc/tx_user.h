/* tx_user.h — ThreadX build-time configuration for this project.
 * Only override what we need; everything else uses ThreadX defaults.
 * SysTick in tx_initialize_low_level.S is (SYSTEM_CLOCK/100)-1 → 100 Hz tick.
 * TX_SINGLE_MODE_NON_SECURE comes from CMake -D flag.
 */
#ifndef TX_USER_H
#define TX_USER_H

#define TX_TIMER_TICKS_PER_SECOND     100U

#endif /* TX_USER_H */
