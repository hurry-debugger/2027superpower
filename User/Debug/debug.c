/**
 * @file debug.c
 * @brief 适配旧工程 debug 模块，按档位观察本工程 PID 记录。
 */
#include "debug.h"

#include "data_scope.h"
#include "power_ctrl.h"
#include "bsp_analog.h"
#include "bsp_hrtim.h"

volatile DebugWave_e g_debug_wave = DEBUG_WAVE_CURRENT;

void DataWavePkg(void)
{
    // clang-format off
    switch(g_debug_wave)
    {
        case DEBUG_WAVE_CURRENT:
        {
			DataScope_Get_Channel_Data(s_hrtim_state.mode);
            DataScope_Get_Channel_Data(g_power_ctrl.inducotor_current_target_a);
			
            DataScope_Get_Channel_Data(adc_fdb.ref.I);
            DataScope_Get_Channel_Data(adc_fdb.A.I);
            DataScope_Get_Channel_Data(adc_fdb.B.I);
			
            DataScope_Get_Channel_Data(adc_fdb.ref.V);
            DataScope_Get_Channel_Data(adc_fdb.A.V);
            DataScope_Get_Channel_Data(adc_fdb.B.V);
			
            DataScope_Get_Channel_Data(g_power_ctrl.ref_power_fdb_w);
            DataScope_Get_Channel_Data(g_power_ctrl.ref_power_target_w);
			
			DataScope_Get_Channel_Data(s_error.faults.all);
			DataScope_Get_Channel_Data(s_error.fault_retry_start_ms);
            DataScope_Get_Channel_Data(g_power_ctrl.fault_retry_interval_ms);

        }    break;

        case DEBUG_WAVE_CURRENT_VOLTAGE:
        {
            DataScope_Get_Channel_Data(g_power_ctrl.power_pid.Output);
            DataScope_Get_Channel_Data(adc_fdb.A.I);
            DataScope_Get_Channel_Data(adc_fdb.B.I);
            DataScope_Get_Channel_Data(g_power_ctrl.voltage_pid.Target);
            DataScope_Get_Channel_Data(g_power_ctrl.voltage_pid.Measure);
            DataScope_Get_Channel_Data(g_power_ctrl.voltage_pid.Output);
        }    break;

        case DEBUG_WAVE_POWER:
        {
            DataScope_Get_Channel_Data(g_power_ctrl.ref_power_target_w);
            DataScope_Get_Channel_Data(g_power_ctrl.ref_power_fdb_w);
            DataScope_Get_Channel_Data(g_power_ctrl.power_pid.Output);
        }    break;

        case DEBUG_WAVE_ENERGY:
        {
            DataScope_Get_Channel_Data(g_power_ctrl.energy_pid.Target);
            DataScope_Get_Channel_Data(g_power_ctrl.energy_pid.Measure);
            DataScope_Get_Channel_Data(g_power_ctrl.energy_pid.Output);
        }    break;

        case DEBUG_WAVE_OFF:
        default:
        {
            /* 无通道时不启动发送，已经进行的 DMA 不受影响。 */
        }    break;
    }
    // clang-format on
}
