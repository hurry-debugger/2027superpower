/**
 * @file power_ctrl_task.c
 * @brief HRTIM 采样、DAC 更新和故障保护调度及旧整机初始化入口。
 */
#include "power_ctrl_task.h"

#include "boardinfo.h"
#include "bsp_analog.h"
#include "bsp_dwt.h"
#include "bsp_hrtim.h"
#include "bsp_usart.h"
#include "error.h"
#include "hrtim.h"
#include "power_ctrl.h"
#include "prot_supercap.h"

void SuperPower_Init(void)
{
    BspHrtim_DisableOutput();

    BoardInfo_Init();

    Bsp_DWT_Init();

    BspAnalog_Init();
    USER_UART_Init();
    SupercapCan_Init();

    PowerCtrl_Init();

    HAL_HRTIM_WaveformCountStart_IT(&hhrtim1, HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B);
}

/**
 * @brief Master 每 16 us 调度采样与快速保护，Timer A 每 1 ms 调度低频任务。
 * @param[in] hhrtim 产生事件的 HRTIM 句柄。
 * @param[in] timer_index 产生重复事件的计数器索引。
 * @note HAL 已清除重复事件标志。Master 先发布反馈，再检查保护并更新 DAC。
 */
void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t timer_index)
{
    if(hhrtim == &hhrtim1)
    {
        if(timer_index == HRTIM_TIMERINDEX_MASTER)
        {
            BspAdcFeedback_Process();
            Error_CheckFast();
            BspAnalog_UpdateDacFromAdc();
        }
        else if(timer_index == HRTIM_TIMERINDEX_TIMER_A)
            Error_Update1ms();
    }
}

/** @brief HRTIM1 FLT1 回调，HAL 清标志后记录A 侧过流。 */
void HAL_HRTIM_Fault1Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    Error_RecordHrtimFault(HRTIM_FLAG_FLT1);
}

/** @brief HRTIM1 FLT2 回调，HAL 清标志后记录电池入口过流。 */
void HAL_HRTIM_Fault2Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    Error_RecordHrtimFault(HRTIM_FLAG_FLT2);
}

/** @brief HRTIM1 FLT3 回调，HAL 清标志后记录A 侧过压。 */
void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    Error_RecordHrtimFault(HRTIM_FLAG_FLT3);
}

/** @brief HRTIM1 FLT4 回调，HAL 清标志后记录B 侧过流。 */
void HAL_HRTIM_Fault4Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    Error_RecordHrtimFault(HRTIM_FLAG_FLT4);
}

/** @brief HRTIM1 FLT5 回调，HAL 清标志后记录B 侧过压。 */
void HAL_HRTIM_Fault5Callback(HRTIM_HandleTypeDef *hhrtim)
{
    (void)hhrtim;
    Error_RecordHrtimFault(HRTIM_FLAG_FLT5);
}
