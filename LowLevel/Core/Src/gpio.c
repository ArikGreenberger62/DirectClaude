/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
     PC14-OSC32_IN(OSC32_IN)   ------> RCC_OSC32_IN
     PC15-OSC32_OUT(OSC32_OUT)   ------> RCC_OSC32_OUT
     PH0-OSC_IN(PH0)   ------> RCC_OSC_IN
     PH1-OSC_OUT(PH1)   ------> RCC_OSC_OUT
     PA13(JTMS/SWDIO)   ------> DEBUG_JTMS-SWDIO
     PA14(JTCK/SWCLK)   ------> DEBUG_JTCK-SWCLK
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SPI_NSS_EEPROM_Pin|ADC_PVIN_SEL_Pin|CAN_SPI2_NSS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, SPI_NSS_ACC_Pin|SPI_NSS_FLASH_Pin|PSRAM_SPI2_NSS_Pin|RS485_REn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, OUTPUT_PP_Pin|RS232_SHDNn_Pin|RS485_DE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, OW_PWR_EN_Pin|OUTPUT_OD_Pin|MC60_PWR_Pin|WIFI_BLE_PWR_EN_Pin
                          |WIFI_BLE_RESETN_Pin|MC60_PWRKEY_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, DALLAS_IN_Pin|CAN_FD_RST_Pin|P3V3_SW_EN_Pin|CAN2_STBY_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CAN1_STBY_GPIO_Port, CAN1_STBY_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, MC60_GNSS_PWR_Pin|CHRGR_CEn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : STAT_CHRGR_Pin IN_DIG_Pin */
  GPIO_InitStruct.Pin = STAT_CHRGR_Pin|IN_DIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI_NSS_EEPROM_Pin ADC_PVIN_SEL_Pin CAN_SPI2_NSS_Pin MC60_GNSS_PWR_Pin */
  GPIO_InitStruct.Pin = SPI_NSS_EEPROM_Pin|ADC_PVIN_SEL_Pin|CAN_SPI2_NSS_Pin|MC60_GNSS_PWR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI_NSS_ACC_Pin SPI_NSS_FLASH_Pin OUTPUT_PP_Pin RS232_SHDNn_Pin
                           PSRAM_SPI2_NSS_Pin RS485_DE_Pin RS485_REn_Pin */
  GPIO_InitStruct.Pin = SPI_NSS_ACC_Pin|SPI_NSS_FLASH_Pin|OUTPUT_PP_Pin|RS232_SHDNn_Pin
                          |PSRAM_SPI2_NSS_Pin|RS485_DE_Pin|RS485_REn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : OW_PWR_EN_Pin OUTPUT_OD_Pin MC60_PWR_Pin WIFI_BLE_PWR_EN_Pin
                           MC60_PWRKEY_Pin */
  GPIO_InitStruct.Pin = OW_PWR_EN_Pin|OUTPUT_OD_Pin|MC60_PWR_Pin|WIFI_BLE_PWR_EN_Pin
                          |MC60_PWRKEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : BLE_STS_Pin MDM_DTR_Pin */
  GPIO_InitStruct.Pin = BLE_STS_Pin|MDM_DTR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : ACC_INT1_Pin */
  GPIO_InitStruct.Pin = ACC_INT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(ACC_INT1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DALLAS_IN_Pin */
  GPIO_InitStruct.Pin = DALLAS_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DALLAS_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : IGNITION_DIG_Pin */
  GPIO_InitStruct.Pin = IGNITION_DIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(IGNITION_DIG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CAN1_STBY_Pin */
  GPIO_InitStruct.Pin = CAN1_STBY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CAN1_STBY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : CAN_FD_RST_Pin P3V3_SW_EN_Pin CAN2_STBY_Pin */
  GPIO_InitStruct.Pin = CAN_FD_RST_Pin|P3V3_SW_EN_Pin|CAN2_STBY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : MC60_RI_Pin */
  GPIO_InitStruct.Pin = MC60_RI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MC60_RI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : WIFI_BLE_RESETN_Pin */
  GPIO_InitStruct.Pin = WIFI_BLE_RESETN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(WIFI_BLE_RESETN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CAN_FD_INTn_Pin */
  GPIO_InitStruct.Pin = CAN_FD_INTn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CAN_FD_INTn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CHRGR_CEn_Pin */
  GPIO_InitStruct.Pin = CHRGR_CEn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CHRGR_CEn_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI5_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(EXTI5_IRQn);

  HAL_NVIC_SetPriority(EXTI8_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(EXTI8_IRQn);

  HAL_NVIC_SetPriority(EXTI13_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(EXTI13_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
