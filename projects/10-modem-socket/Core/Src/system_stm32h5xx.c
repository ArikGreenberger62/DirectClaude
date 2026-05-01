/**
  ******************************************************************************
  * @file    system_stm32h5xx.c
  * @author  MCD Application Team
  * @brief   CMSIS Cortex-M33 Device Peripheral Access Layer System Source File
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  * Project-local copy for 10-modem-socket.
  *
  * Modification vs LowLevel original:
  *   Removed the FLASH->OPSR / FLASH->OPTCR OPTSTART block from SystemInit().
  *   That block re-launches an interrupted option-byte operation when CODE_OP = 6
  *   or 7, which triggers a system reset and an infinite reset loop when the
  *   programmer (STM32_Programmer_CLI) leaves dirty CODE_OP bits in OPSR after
  *   flashing (observed when it misidentifies the device).  Removing the block
  *   is safe for normal development: option-byte recovery only matters after a
  *   power-loss during a deliberate option-byte programming sequence, which does
  *   not apply here.
  ******************************************************************************
  */

#include "stm32h5xx.h"

#if !defined  (HSE_VALUE)
  #define HSE_VALUE    (25000000UL)
#endif
#if !defined  (CSI_VALUE)
  #define CSI_VALUE    (4000000UL)
#endif
#if !defined  (HSI_VALUE)
  #define HSI_VALUE    (64000000UL)
#endif

#if !defined(VECT_TAB_OFFSET)
#define VECT_TAB_OFFSET  0x00U
#endif

uint32_t SystemCoreClock = 64000000U;

const uint8_t  AHBPrescTable[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t  APBPrescTable[8]  = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

void SystemInit(void)
{
  /* FPU settings */
  #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
   SCB->CPACR |= ((3UL << 20U)|(3UL << 22U));
  #endif

  /* Reset the RCC clock configuration to the default reset state */
  RCC->CR = (RCC->CR & RCC_CR_HSIDIV_Msk) | RCC_CR_HSION;
  RCC->CFGR1 = 0U;
  RCC->CFGR2 = 0U;

#if defined(RCC_CR_PLL3ON)
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSECSSON | RCC_CR_HSEBYP | RCC_CR_HSEEXT | RCC_CR_HSIKERON | \
               RCC_CR_CSION | RCC_CR_CSIKERON |RCC_CR_HSI48ON | RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
#else
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSECSSON | RCC_CR_HSEBYP | RCC_CR_HSEEXT | RCC_CR_HSIKERON | \
               RCC_CR_CSION | RCC_CR_CSIKERON |RCC_CR_HSI48ON | RCC_CR_PLL1ON | RCC_CR_PLL2ON);
#endif

  RCC->PLL1CFGR = 0U;
  RCC->PLL2CFGR = 0U;
#if defined(RCC_CR_PLL3ON)
  RCC->PLL3CFGR = 0U;
#endif

  RCC->PLL1DIVR  = 0x01010280U;
  RCC->PLL1FRACR = 0x00000000U;
  RCC->PLL2DIVR  = 0x01010280U;
  RCC->PLL2FRACR = 0x00000000U;
#if defined(RCC_CR_PLL3ON)
  RCC->PLL3DIVR  = 0x01010280U;
  RCC->PLL3FRACR = 0x00000000U;
#endif

  RCC->CR &= ~(RCC_CR_HSEBYP);
  RCC->CIER = 0U;

  /* Vector Table location */
  #ifdef VECT_TAB_SRAM
    SCB->VTOR = SRAM1_BASE | VECT_TAB_OFFSET;
  #else
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
  #endif

  /*
   * NOTE: The original ST code here checks FLASH->OPSR for an interrupted
   * option-byte operation (CODE_OP == 6 or 7) and re-launches it via
   * FLASH->OPTCR |= FLASH_OPTCR_OPTSTART.  OPTSTART causes a system reset,
   * which creates an infinite reset loop when dirty CODE_OP bits are left by
   * a programmer that misidentifies the device.  That block is intentionally
   * omitted here.
   */
}

void SystemCoreClockUpdate(void)
{
  uint32_t pllp, pllsource, pllm, pllfracen, hsivalue, tmp;
  float_t fracn1, pllvco;

  switch (RCC->CFGR1 & RCC_CFGR1_SWS)
  {
  case 0x00UL:
    SystemCoreClock = (uint32_t) (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV)>> 3));
    break;

  case 0x08UL:
    SystemCoreClock = CSI_VALUE;
    break;

  case 0x10UL:
    SystemCoreClock = HSE_VALUE;
    break;

  case 0x18UL:
    pllsource = (RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1SRC);
    pllm      = ((RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1M) >> RCC_PLL1CFGR_PLL1M_Pos);
    pllfracen = ((RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1FRACEN) >> RCC_PLL1CFGR_PLL1FRACEN_Pos);
    fracn1    = (float_t)(uint32_t)(pllfracen * ((RCC->PLL1FRACR & RCC_PLL1FRACR_PLL1FRACN) >> RCC_PLL1FRACR_PLL1FRACN_Pos));

    switch (pllsource)
    {
    case 0x01UL:
      hsivalue = (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV) >> 3));
      pllvco   = ((float_t)hsivalue / (float_t)pllm) * ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_PLL1N) +
                 (fracn1 / (float_t)0x2000) + (float_t)1);
      break;
    case 0x02UL:
      pllvco = ((float_t)CSI_VALUE / (float_t)pllm) * ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_PLL1N) +
               (fracn1 / (float_t)0x2000) + (float_t)1);
      break;
    case 0x03UL:
      pllvco = ((float_t)HSE_VALUE / (float_t)pllm) * ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_PLL1N) +
               (fracn1 / (float_t)0x2000) + (float_t)1);
      break;
    default:
      pllvco = (float_t)0U;
      break;
    }

    pllp = (((RCC->PLL1DIVR & RCC_PLL1DIVR_PLL1P) >> RCC_PLL1DIVR_PLL1P_Pos) + 1U);
    SystemCoreClock = (uint32_t)(float_t)(pllvco / (float_t)pllp);
    break;

  default:
    SystemCoreClock = HSI_VALUE;
    break;
  }

  tmp = AHBPrescTable[((RCC->CFGR2 & RCC_CFGR2_HPRE) >> RCC_CFGR2_HPRE_Pos)];
  SystemCoreClock >>= tmp;
}
