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
#include "stm32g4xx_hal.h"

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
#define ADC1_IN8_I_BAT_Pin GPIO_PIN_2
#define ADC1_IN8_I_BAT_GPIO_Port GPIOC
#define ADC1_IN9_I_A_Pin GPIO_PIN_3
#define ADC1_IN9_I_A_GPIO_Port GPIOC
#define ADC1_IN3_OPAMP1_OUT_V_BAT_Pin GPIO_PIN_2
#define ADC1_IN3_OPAMP1_OUT_V_BAT_GPIO_Port GPIOA
#define ADC2_IN3_OPAMP2_OUT_V_CAP_Pin GPIO_PIN_6
#define ADC2_IN3_OPAMP2_OUT_V_CAP_GPIO_Port GPIOA
#define ADC2_IN5_I_B_Pin GPIO_PIN_4
#define ADC2_IN5_I_B_GPIO_Port GPIOC
#define ADC1_IN12_OPAMP3_OUT_V_CHA_Pin GPIO_PIN_1
#define ADC1_IN12_OPAMP3_OUT_V_CHA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
/** @brief 二选一编译模式；当前默认闭环，控制启动入口暂未接入。 */
#define POWER_CTRL_CLOSED_LOOP
//#define POWER_CTRL_OPEN_LOOP

#if (defined(POWER_CTRL_CLOSED_LOOP) + defined(POWER_CTRL_OPEN_LOOP)) != 1
#error Select_exactly_one_power_control_mode_in_main_h
#endif

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
