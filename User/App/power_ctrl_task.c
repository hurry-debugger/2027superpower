/**
 * @file power_ctrl_task.c
 * @brief 超电初始化及 HRTIM 采样、DAC 更新和故障保护调度。
 */
#include "power_ctrl_task.h"

#include "boardinfo.h"
#include "bsp_analog.h"
#include "bsp_dwt.h"
#include "bsp_hrtim.h"
#include "bsp_usart.h"
#include "hrtim.h"
#include "power_ctrl.h"
#include "power_ctrl_config.h"
#include "data_scope.h"

void SuperPower_Init(void)
{
    LL_HRTIM_DisableOutput(HRTIM1, BSP_HRTIM_POWER_OUTPUT_MASK);
    BoardInfo_Init();
    Bsp_DWT_Init();
    BspAnalog_Init();
    PowerCtrl_Init();
    USER_UART_Init();

    if(HAL_HRTIM_SoftwareUpdate(&hhrtim1, HRTIM_TIMERUPDATE_A | HRTIM_TIMERUPDATE_B) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_HRTIM_ENABLE_IT(&hhrtim1, hhrtim1.Init.HRTIMInterruptResquests);
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
 * @brief Master 每 16 us 调度采样与控制，Timer A 每 1 ms 调度 Fault 恢复检查。
 * @param[in] hhrtim 产生事件的 HRTIM 句柄。
 * @param[in] timer_index 产生重复事件的计数器索引。
 * @note HAL 已清除重复事件标志。Master 先发布反馈，再执行控制任务。
 */
void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t timer_index)
{
    if(hhrtim == &hhrtim1)
    {
#ifdef  CLOSE_LOOP
        if(timer_index == HRTIM_TIMERINDEX_MASTER)
        {
			Bsp_DWT_Stop(0);
			Bsp_DWT_Start(0);
			
            Bsp_DWT_Start(1);
            BspAdcFeedback_Process();
            PowerCtrl_Task();
            Bsp_DWT_Stop(1);
        }
        else if(timer_index == HRTIM_TIMERINDEX_TIMER_A)
        {
			Bsp_DWT_Stop(2);
			Bsp_DWT_Start(2);
			
            Bsp_DWT_Start(3);
            TimerA_1ms_buffer_loop_calculate();
            PowerCtrl_TryRecoverFault();
			DataWave(&huart1);
            Bsp_DWT_Stop(3);
        }
#endif
#ifdef OPEN_LOOP
	LL_HRTIM_EnableOutput(HRTIM1, BSP_HRTIM_POWER_OUTPUT_MASK);
	BspHrtim_SetOpenLoopPwm();
#endif	
    }
}

/** @brief HRTIM1 FLT1 回调，HAL 清标志后记录 A 侧过流。 */
void HAL_HRTIM_Fault1Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    PowerCtrl_RecordHrtimFault(HRTIM_FLAG_FLT1);
}

/** @brief HRTIM1 FLT2 回调，HAL 清标志后记录电池入口过流。 */
void HAL_HRTIM_Fault2Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    PowerCtrl_RecordHrtimFault(HRTIM_FLAG_FLT2);
}

/** @brief HRTIM1 FLT3 回调，HAL 清标志后记录 A/ref 电压越界及可判定的方向。 */
void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    PowerCtrl_RecordHrtimFault(HRTIM_FLAG_FLT3);
}

/** @brief HRTIM1 FLT4 回调，HAL 清标志后记录 B 侧过流。 */
void HAL_HRTIM_Fault4Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    PowerCtrl_RecordHrtimFault(HRTIM_FLAG_FLT4);
}

/** @brief HRTIM1 FLT5 回调，HAL 清标志后记录 B 侧电压越界及可判定的方向。 */
void HAL_HRTIM_Fault5Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
	Bsp_DWT_Stop(4);
	Bsp_DWT_Start(4);
	Bsp_DWT_Start(5);
    PowerCtrl_RecordHrtimFault(HRTIM_FLAG_FLT5);
	Bsp_DWT_Stop(5);
}
