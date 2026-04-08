/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define WIFI_SPI4_SCK_Pin GPIO_PIN_2
#define WIFI_SPI4_SCK_GPIO_Port GPIOE
#define STAT_CHRGR_Pin GPIO_PIN_3
#define STAT_CHRGR_GPIO_Port GPIOE
#define WIFI_SPI4_NSS_Pin GPIO_PIN_4
#define WIFI_SPI4_NSS_GPIO_Port GPIOE
#define WIFI_SPI4_MISO_Pin GPIO_PIN_5
#define WIFI_SPI4_MISO_GPIO_Port GPIOE
#define WIFI_SPI4_MOSI_Pin GPIO_PIN_6
#define WIFI_SPI4_MOSI_GPIO_Port GPIOE
#define ADC_IGNITION_ADC1_INP10_Pin GPIO_PIN_0
#define ADC_IGNITION_ADC1_INP10_GPIO_Port GPIOC
#define ADC_INPUT1_ADC1_INP11_Pin GPIO_PIN_1
#define ADC_INPUT1_ADC1_INP11_GPIO_Port GPIOC
#define ADC_INPUT2_ADC1_INP12_Pin GPIO_PIN_2
#define ADC_INPUT2_ADC1_INP12_GPIO_Port GPIOC
#define ADC_INPUT3_ADC1_INP13_Pin GPIO_PIN_3
#define ADC_INPUT3_ADC1_INP13_GPIO_Port GPIOC
#define RS232_UART4_TX_Pin GPIO_PIN_0
#define RS232_UART4_TX_GPIO_Port GPIOA
#define RS232_UART4_RX_Pin GPIO_PIN_1
#define RS232_UART4_RX_GPIO_Port GPIOA
#define MDM_USART2_TX_Pin GPIO_PIN_2
#define MDM_USART2_TX_GPIO_Port GPIOA
#define MDM_USART2_RX_Pin GPIO_PIN_3
#define MDM_USART2_RX_GPIO_Port GPIOA
#define ADC_PVIN_ADC1_INP5_Pin GPIO_PIN_4
#define ADC_PVIN_ADC1_INP5_GPIO_Port GPIOA
#define ACC_NVM_SPI1_SCK_Pin GPIO_PIN_5
#define ACC_NVM_SPI1_SCK_GPIO_Port GPIOA
#define ACC_NVM_SPI1_MISO_Pin GPIO_PIN_6
#define ACC_NVM_SPI1_MISO_GPIO_Port GPIOA
#define ACC_NVM_SPI1_MOSI_Pin GPIO_PIN_7
#define ACC_NVM_SPI1_MOSI_GPIO_Port GPIOA
#define ADC_ASSEMBLY_ADC1_INP9_Pin GPIO_PIN_0
#define ADC_ASSEMBLY_ADC1_INP9_GPIO_Port GPIOB
#define ADC_VBAT_ADC1_INP18_Pin GPIO_PIN_1
#define ADC_VBAT_ADC1_INP18_GPIO_Port GPIOB
#define SPI_NSS_EEPROM_Pin GPIO_PIN_2
#define SPI_NSS_EEPROM_GPIO_Port GPIOB
#define H_CON_UART7_RX_Pin GPIO_PIN_7
#define H_CON_UART7_RX_GPIO_Port GPIOE
#define H_CON_UART7_TX_Pin GPIO_PIN_8
#define H_CON_UART7_TX_GPIO_Port GPIOE
#define SPI_NSS_ACC_Pin GPIO_PIN_9
#define SPI_NSS_ACC_GPIO_Port GPIOE
#define SPI_NSS_FLASH_Pin GPIO_PIN_10
#define SPI_NSS_FLASH_GPIO_Port GPIOE
#define OUTPUT_PP_Pin GPIO_PIN_11
#define OUTPUT_PP_GPIO_Port GPIOE
#define RS232_SHDNn_Pin GPIO_PIN_12
#define RS232_SHDNn_GPIO_Port GPIOE
#define PSRAM_SPI2_NSS_Pin GPIO_PIN_13
#define PSRAM_SPI2_NSS_GPIO_Port GPIOE
#define IN_DIG_Pin GPIO_PIN_14
#define IN_DIG_GPIO_Port GPIOE
#define RS485_DE_Pin GPIO_PIN_15
#define RS485_DE_GPIO_Port GPIOE
#define GNSS_USART3_TX_Pin GPIO_PIN_10
#define GNSS_USART3_TX_GPIO_Port GPIOB
#define GNSS_USART3_RX_Pin GPIO_PIN_11
#define GNSS_USART3_RX_GPIO_Port GPIOB
#define CAN2_TX_Pin GPIO_PIN_13
#define CAN2_TX_GPIO_Port GPIOB
#define CAN_SPI2_MISO_Pin GPIO_PIN_14
#define CAN_SPI2_MISO_GPIO_Port GPIOB
#define CAN_SPI2_MOSI_Pin GPIO_PIN_15
#define CAN_SPI2_MOSI_GPIO_Port GPIOB
#define OW_PWR_EN_Pin GPIO_PIN_8
#define OW_PWR_EN_GPIO_Port GPIOD
#define CAN2_RX_Pin GPIO_PIN_9
#define CAN2_RX_GPIO_Port GPIOD
#define OUTPUT_OD_Pin GPIO_PIN_10
#define OUTPUT_OD_GPIO_Port GPIOD
#define BLE_STS_Pin GPIO_PIN_11
#define BLE_STS_GPIO_Port GPIOD
#define MC60_PWR_Pin GPIO_PIN_12
#define MC60_PWR_GPIO_Port GPIOD
#define ACC_INT1_Pin GPIO_PIN_13
#define ACC_INT1_GPIO_Port GPIOD
#define ACC_INT1_EXTI_IRQn EXTI13_IRQn
#define WIFI_UART9_RX_Pin GPIO_PIN_14
#define WIFI_UART9_RX_GPIO_Port GPIOD
#define WIFI_UART9_TX_Pin GPIO_PIN_15
#define WIFI_UART9_TX_GPIO_Port GPIOD
#define DALLAS_IN_Pin GPIO_PIN_7
#define DALLAS_IN_GPIO_Port GPIOC
#define LED_R_Pin GPIO_PIN_8
#define LED_R_GPIO_Port GPIOC
#define LED_G_Pin GPIO_PIN_9
#define LED_G_GPIO_Port GPIOC
#define IGNITION_DIG_Pin GPIO_PIN_8
#define IGNITION_DIG_GPIO_Port GPIOA
#define IGNITION_DIG_EXTI_IRQn EXTI8_IRQn
#define RS485_USART1_TX_Pin GPIO_PIN_9
#define RS485_USART1_TX_GPIO_Port GPIOA
#define RS485_USART1_RX_Pin GPIO_PIN_10
#define RS485_USART1_RX_GPIO_Port GPIOA
#define CAN1_RX_Pin GPIO_PIN_11
#define CAN1_RX_GPIO_Port GPIOA
#define CAN_SPI2_SCK_Pin GPIO_PIN_12
#define CAN_SPI2_SCK_GPIO_Port GPIOA
#define CAN1_STBY_Pin GPIO_PIN_15
#define CAN1_STBY_GPIO_Port GPIOA
#define CAN_FD_RST_Pin GPIO_PIN_10
#define CAN_FD_RST_GPIO_Port GPIOC
#define P3V3_SW_EN_Pin GPIO_PIN_11
#define P3V3_SW_EN_GPIO_Port GPIOC
#define CAN2_STBY_Pin GPIO_PIN_12
#define CAN2_STBY_GPIO_Port GPIOC
#define MDM_DTR_Pin GPIO_PIN_0
#define MDM_DTR_GPIO_Port GPIOD
#define CAN1_TX_Pin GPIO_PIN_1
#define CAN1_TX_GPIO_Port GPIOD
#define MC60_RI_Pin GPIO_PIN_2
#define MC60_RI_GPIO_Port GPIOD
#define MC60_RI_EXTI_IRQn EXTI2_IRQn
#define MDM_USART2_CTS_Pin GPIO_PIN_3
#define MDM_USART2_CTS_GPIO_Port GPIOD
#define MDM_USART2_RTS_Pin GPIO_PIN_4
#define MDM_USART2_RTS_GPIO_Port GPIOD
#define WIFI_BLE_PWR_EN_Pin GPIO_PIN_5
#define WIFI_BLE_PWR_EN_GPIO_Port GPIOD
#define WIFI_BLE_RESETN_Pin GPIO_PIN_6
#define WIFI_BLE_RESETN_GPIO_Port GPIOD
#define MC60_PWRKEY_Pin GPIO_PIN_7
#define MC60_PWRKEY_GPIO_Port GPIOD
#define ADC_PVIN_SEL_Pin GPIO_PIN_3
#define ADC_PVIN_SEL_GPIO_Port GPIOB
#define CAN_SPI2_NSS_Pin GPIO_PIN_4
#define CAN_SPI2_NSS_GPIO_Port GPIOB
#define CAN_FD_INTn_Pin GPIO_PIN_5
#define CAN_FD_INTn_GPIO_Port GPIOB
#define CAN_FD_INTn_EXTI_IRQn EXTI5_IRQn
#define MC60_GNSS_PWR_Pin GPIO_PIN_8
#define MC60_GNSS_PWR_GPIO_Port GPIOB
#define CHRGR_CEn_Pin GPIO_PIN_9
#define CHRGR_CEn_GPIO_Port GPIOB
#define RS485_REn_Pin GPIO_PIN_0
#define RS485_REn_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */
// SPI
#define NVM_SPI_HANDLE 											hspi1
#define NVM_SPI_INDEX									1

#define EXTERNAL_CAN_SPI_HANDLE							hspi2
#define EXTERNAL_CAN_SPI_INDEX							2
#define EXTERNAL_CAN_SRAM_SPI_INDEX 					2

#define CHARGER_I2C_INDEX 1

#define WIFII_SPI_INDEX 4
#define GSENSOR_SPI_INDEX 1

#define IG_DIG_EXTI_IRQn EXTI8_IRQn

// ******************************************************************************************************
// ADC
// ******************************************************************************************************

// ADC CHANNELS order by rank
#define ADC_HW_ID_CH 													0
#define ADC_VIN_CH 														1
#define ADC_IGN_CH 														2
#define ADC_IN1_CH 														3
#define ADC_IN2_CH 														4
#define ADC_IN3_CH 														5
#define ADC_VBAT_CH 													6
#define ADC_VREF_INT_CH 											7
#define ADC_MCU_TEMPERATURE_CH 								8
#define NUM_OF_ADC_CH 												9

// ADC by ADC INPUT
#define ADC_VIN_CHANNEL	  ADC_CHANNEL_18
#define ADC_IGN_CHANNEL	  ADC_CHANNEL_10

#define ADC_INPUT1_CHANNEL	  ADC_CHANNEL_11
#define ADC_INPUT2_CHANNEL	  ADC_CHANNEL_12
#define ADC_INPUT3_CHANNEL	  ADC_CHANNEL_13

extern DMA_HandleTypeDef handle_GPDMA1_Channel0;
#define DMA_HANDLE_OF_ADC handle_GPDMA1_Channel0

// ******************************************************************************************************
// TIMERS
// ******************************************************************************************************
#define s_hstTimeBase1usec htim6
#define TIME_BASE_1USEC_TIMER_INDEX						6

#define s_hstOneWire1usec htim7
#define ONE_WIRE_TIME_BASE_1USEC_TIMER_INDEX	7

//#define s_hstVSerial1usec htim12
//#define VSERIAL_TIME_BASE_1USEC_TIMER_INDEX	12
//#define VSERIAL_TIMER_INIT MX_TIM12_Init

#define RED_LED_TIMER_INDEX 3
#define RED_LED_TIMER htim3
#define RED_LED_TIMER_CHANNEL TIM_CHANNEL_3

#define GREEN_LED_TIMER_INDEX 3
#define GREEN_LED_TIMER htim3
#define GREEN_LED_TIMER_CHANNEL TIM_CHANNEL_4


// ******************************************************************************************************
// UART
// ******************************************************************************************************

#define RS232_UART_INDEX							4 // 5 to match SAM. //4
#define RS232_UART (huart4)


#define RS485_1_UART_INDEX						1 // Was 3 Arik 07/02/2023 It changed to 6 for be compatible with other projects
#define RS485_1_UART (huart1)

#define MODEM_UART_INDEX							2
#define MODEM_UART (huart2)

#define GPS_UART (huart3)
#define GPS_UART_INDEX 3
#define GPS_UART_IRQn 								USART3_IRQn

#define TRACE_UART_INDEX 7						//was 7 originally. now 1 to match Connect
#define TRACE_UART 										(huart7)

#define MAIN_WIFI_BLE_UART_INDEX 9
#define MAIN_WIFI_BLE_UART 								(huart9)
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
