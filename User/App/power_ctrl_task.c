/**
 * @file power_ctrl_task.c
 * @brief 62.5 kHz Master 采样调度及旧整机初始化入口。
 */
#include "power_ctrl_task.h"

#include "boardinfo.h"
#include "bsp_adc.h"
#include "bsp_dwt.h"
#include "bsp_hrtim.h"
#include "bsp_usart.h"
#include "hrtim.h"
#include "power_ctrl.h"
#include "prot_supercap.h"

void SuperPower_Init(void)
{
    BspHrtim_DisableOutput();

    BoardInfo_Init();

    Bsp_DWT_Init();

    /* 计数器启动前准备 ADC DMA 及通信接口。 */
    BspAdcFeedback_Init();
    USER_UART_Init();
    SupercapCan_Init();

    PowerCtrl_Init();

    HAL_HRTIM_WaveformCountStart_IT(&hhrtim1, HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B);
}

/**
 * @brief Master 每四个 PWM 周期发布一次 ADC 原始平均计数。
 * @param[in] hhrtim 产生事件的 HRTIM 句柄。
 * @param[in] timer_index 产生重复事件的计数器索引。
 * @note HAL 已清除重复事件标志，采样在中断上下文执行。
 */
void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t timer_index)
{
    if(hhrtim == &hhrtim1 && timer_index == HRTIM_TIMERINDEX_MASTER)
    {
        BspAdcFeedback_Process();
    }
}
