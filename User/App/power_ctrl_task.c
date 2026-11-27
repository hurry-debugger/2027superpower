/**
 * @file power_ctrl_task.c
 * @brief 超电初始化及 HRTIM 采样、DAC 更新和故障保护调度。
 */
#include "power_ctrl_task.h"

#include "boardinfo.h"
#include "bsp_analog.h"
#include "bsp_can.h"
#include "bsp_dwt.h"
#include "bsp_hrtim.h"
#include "bsp_usart.h"
#include "fdcan.h"
#include "hrtim.h"
#include "power_ctrl.h"
#include "power_ctrl_config.h"
#include "prot_supercap.h"
#include "data_scope.h"

void SuperPower_Init(void)
{
    LL_HRTIM_DisableOutput(HRTIM1, BSP_HRTIM_POWER_OUTPUT_MASK);
    BoardInfo_Init();
    Bsp_DWT_Init();
    BspAnalog_Init();
    PowerCtrl_Init();
    USER_UART_Init();

    if(HAL_FDCAN_Start(&hfdcan2) != HAL_OK)
    {
        Error_Handler();
    }

    if(HAL_HRTIM_SoftwareUpdate(&hhrtim1, HRTIM_TIMERUPDATE_A | HRTIM_TIMERUPDATE_B) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_HRTIM_MASTER_ENABLE_IT(&hhrtim1,
                                 hhrtim1.TimerParam[HRTIM_TIMERINDEX_MASTER].InterruptRequests);
    __HAL_HRTIM_TIMER_ENABLE_IT(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_TIM_IT_REP);
    if(HAL_HRTIM_WaveformCountStart(
           &hhrtim1, HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Master 每 16 us 调度采样与控制，Timer A 每 1 ms 轮询 Fault 并调度低频任务。
 * @param[in] hhrtim 产生事件的 HRTIM 句柄。
 * @param[in] timer_index 产生重复事件的计数器索引。
 * @note HAL 已清除重复事件标志。Master 先发布反馈，再执行控制任务。
 */
void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t timer_index)
{
    if(hhrtim == &hhrtim1)
    {
        if(timer_index == HRTIM_TIMERINDEX_MASTER)
        {
            Bsp_DWT_Stop(0);
            Bsp_DWT_Start(0);
#ifdef CLOSE_LOOP
            Bsp_DWT_Start(1);

            BspAdcFeedback_Process();
            PowerCtrl_Task();

            Bsp_DWT_Stop(1);
#endif
#ifdef OPEN_LOOP
    BspHrtim_SetOpenLoopPwm();
#endif
        }
        else if(timer_index == HRTIM_TIMERINDEX_TIMER_A)
        {
            Bsp_DWT_Stop(2);
            Bsp_DWT_Start(2);
            Bsp_DWT_Start(3);

            BspAnalog_UpdateMcuTemp();
            PowerCtrl_ServiceFault();
            TimerA_1ms_buffer_loop_calculate();
            SupercapCan_Send1ms();
            DataWave(&huart1);

            Bsp_DWT_Stop(3);
        }
    }
}
