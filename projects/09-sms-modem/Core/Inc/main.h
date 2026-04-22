/* main.h — 09-sms-modem project header.
 * Pulls in the LowLevel main.h (pin definitions, Error_Handler) and
 * adds project-specific LED definitions.
 */
#ifndef MAIN_H
#define MAIN_H

/* LowLevel main.h provides: stm32h5xx_hal.h, all pin #defines, Error_Handler */
#include "../../LowLevel/Core/Inc/main.h"   /* resolved by include path in CMake */

/* LED_R = PC8, LED_G = PC9 (not in LowLevel IOC — initialised locally) */
#define LED_R_PIN   GPIO_PIN_8
#define LED_G_PIN   GPIO_PIN_9
#define LED_RG_PORT GPIOC

#ifdef __cplusplus
extern "C" {
#endif

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
