/* stm32h5xx_it.c — 07-dual-blink interrupt service routines
 *
 * Copied verbatim from LowLevel/Core/Src/stm32h5xx_it.c.
 * USER CODE section adds the __errno stub required by nano.specs + -lm.
 *
 * Note: SysTick_Handler is intentionally empty.
 * The HAL tick is driven by TIM4 (stm32h5xx_hal_timebase_tim.c),
 * so TIM4_IRQHandler → HAL_TIM_IRQHandler → HAL_IncTick().
 */

#include "main.h"
#include "stm32h5xx_it.h"

/* External variables --------------------------------------------------------*/
extern DMA_NodeTypeDef Node_GPDMA1_Channel0;
extern DMA_QListTypeDef List_GPDMA1_Channel0;
extern DMA_HandleTypeDef handle_GPDMA1_Channel0;
extern ADC_HandleTypeDef hadc1;
extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;
extern I2C_HandleTypeDef hi2c1;
extern RTC_HandleTypeDef hrtc;
extern DMA_HandleTypeDef handle_GPDMA1_Channel2;
extern DMA_HandleTypeDef handle_GPDMA1_Channel1;
extern DMA_HandleTypeDef handle_GPDMA1_Channel6;
extern DMA_HandleTypeDef handle_GPDMA1_Channel5;
extern DMA_HandleTypeDef handle_GPDMA1_Channel4;
extern DMA_HandleTypeDef handle_GPDMA1_Channel3;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi4;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;
extern DMA_HandleTypeDef handle_GPDMA2_Channel3;
extern DMA_HandleTypeDef handle_GPDMA2_Channel4;
extern DMA_HandleTypeDef handle_GPDMA2_Channel5;
extern DMA_HandleTypeDef handle_GPDMA2_Channel0;
extern DMA_HandleTypeDef handle_GPDMA2_Channel1;
extern DMA_HandleTypeDef handle_GPDMA2_Channel2;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart9;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim4;

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers             */
/******************************************************************************/
void NMI_Handler(void)          { while (1) {} }
void HardFault_Handler(void)    { while (1) {} }
void MemManage_Handler(void)    { while (1) {} }
void BusFault_Handler(void)     { while (1) {} }
void UsageFault_Handler(void)   { while (1) {} }
void SVC_Handler(void)          {}
void DebugMon_Handler(void)     {}
void PendSV_Handler(void)       {}

/* SysTick_Handler is empty: HAL tick comes from TIM4. */
void SysTick_Handler(void)      {}

/******************************************************************************/
/* STM32H5xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/
void RTC_IRQHandler(void)               { HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc); }
void FLASH_IRQHandler(void)             { HAL_FLASH_IRQHandler(); }
void EXTI2_IRQHandler(void)             { HAL_GPIO_EXTI_IRQHandler(MC60_RI_Pin); }
void EXTI5_IRQHandler(void)             { HAL_GPIO_EXTI_IRQHandler(CAN_FD_INTn_Pin); }
void EXTI8_IRQHandler(void)             { HAL_GPIO_EXTI_IRQHandler(IGNITION_DIG_Pin); }
void EXTI13_IRQHandler(void)            { HAL_GPIO_EXTI_IRQHandler(ACC_INT1_Pin); }
void GPDMA1_Channel0_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA1_Channel0); }
void GPDMA1_Channel1_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA1_Channel1); }
void GPDMA1_Channel2_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA1_Channel2); }
void GPDMA1_Channel3_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA1_Channel3); }
void GPDMA1_Channel4_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA1_Channel4); }
void GPDMA1_Channel5_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA1_Channel5); }
void GPDMA1_Channel6_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA1_Channel6); }
void ADC1_IRQHandler(void)              { HAL_ADC_IRQHandler(&hadc1); }
void FDCAN1_IT0_IRQHandler(void)        { HAL_FDCAN_IRQHandler(&hfdcan1); }
void FDCAN1_IT1_IRQHandler(void)        { HAL_FDCAN_IRQHandler(&hfdcan1); }
void TIM4_IRQHandler(void)              { HAL_TIM_IRQHandler(&htim4); }
void TIM6_IRQHandler(void)              { HAL_TIM_IRQHandler(&htim6); }
void TIM7_IRQHandler(void)              { HAL_TIM_IRQHandler(&htim7); }
void I2C1_EV_IRQHandler(void)           { HAL_I2C_EV_IRQHandler(&hi2c1); }
void I2C1_ER_IRQHandler(void)           { HAL_I2C_ER_IRQHandler(&hi2c1); }
void SPI1_IRQHandler(void)              { HAL_SPI_IRQHandler(&hspi1); }
void SPI2_IRQHandler(void)              { HAL_SPI_IRQHandler(&hspi2); }
void USART1_IRQHandler(void)            { HAL_UART_IRQHandler(&huart1); }
void USART2_IRQHandler(void)            { HAL_UART_IRQHandler(&huart2); }
void USART3_IRQHandler(void)            { HAL_UART_IRQHandler(&huart3); }
void UART4_IRQHandler(void)             { HAL_UART_IRQHandler(&huart4); }
void SPI4_IRQHandler(void)              { HAL_SPI_IRQHandler(&hspi4); }
void GPDMA2_Channel0_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA2_Channel0); }
void GPDMA2_Channel1_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA2_Channel1); }
void GPDMA2_Channel2_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA2_Channel2); }
void GPDMA2_Channel3_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA2_Channel3); }
void GPDMA2_Channel4_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA2_Channel4); }
void GPDMA2_Channel5_IRQHandler(void)   { HAL_DMA_IRQHandler(&handle_GPDMA2_Channel5); }
void UART7_IRQHandler(void)             { HAL_UART_IRQHandler(&huart7); }
void UART9_IRQHandler(void)             { HAL_UART_IRQHandler(&huart9); }
void FDCAN2_IT0_IRQHandler(void)        { HAL_FDCAN_IRQHandler(&hfdcan2); }
void FDCAN2_IT1_IRQHandler(void)        { HAL_FDCAN_IRQHandler(&hfdcan2); }

/* USER CODE BEGIN 1 */

/* __errno stub: required by nano.specs when any TU pulls in a libm symbol.
 * This project does not use <math.h>, but the stub is kept as a safety net
 * consistent with the workspace pattern established in projects 03 and 04. */
int *__errno(void)
{
    static int s_errno = 0;
    return &s_errno;
}

/* USER CODE END 1 */
