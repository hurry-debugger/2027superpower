/**
 * @file power_ctrl.c
 * @brief 能量修正、入口功率调节、电压竞争和 DAC 峰谷值目标。
 * @details 正电流充电、负电流放电；逻辑 A 端为底盘侧，对应 Timer B，逻辑 B 端为电容侧，对应 Timer A。
 * @note HRTIM Fault 自动关断四路输出，Timer A 负责故障轮询和输出使能。
 */
#include "power_ctrl.h"

#include "bsp_analog.h"
#include "bsp_hrtim.h"
#include "power_ctrl_config.h"
#include "prot_judge.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_ll_adc.h"

#include <stdbool.h>

/** @brief Timer A 写入故障字和重试时间 */
error_t s_error;

PowerCtrl_t g_power_ctrl;

/* ==================== 私有声明 ==================== */

static void PowerCtrl_CalculateCurrentTarget(void);
static void PowerCtrl_CurrentTargetLimit(void);
static void PowerCtrl_ResetLoops(void);
static bool PowerCtrl_FeedbackInSafeWindow(void);

/* ==================== 公共入口 ==================== */

void PowerCtrl_Init(void)
{
    g_power_ctrl.state = PROTECT_MODE;
    //初次恢复正常的时候开始计算时间
    g_power_ctrl.fault_retry_interval_ms = 100U;
    /* 三个位置式外环首版均使用零增益。离散系数按 Master 每 16 us 调用定义。 */
    PID_Init(&g_power_ctrl.energy_pid, 20, 
                0.0f, 0.0f,
                2.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, NONE);
    PID_Init(&g_power_ctrl.power_pid, POWER_CTRL_INDUCTOR_CURRENT_PEAK_A,
                POWER_CTRL_INDUCTOR_CURRENT_PEAK_A, 0.0f,
                0.0f, 0.0001f, 0.0f, 0.0f, 
                0.0f, 0.0f, 0.0f, NONE);
    PID_Init(&g_power_ctrl.voltage_pid, POWER_CTRL_INDUCTOR_CURRENT_PEAK_A,
                POWER_CTRL_INDUCTOR_CURRENT_PEAK_A, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 0.0f, NONE);
}

void PowerCtrl_Task(void)
{
    switch(g_power_ctrl.state)
    {
        case PROTECT_MODE:
        {
            PowerCtrl_ResetLoops();
            LL_HRTIM_DisableOutput(HRTIM1, BSP_HRTIM_POWER_OUTPUT_MASK);
            if(s_error.faults.all == 0U)
            {
                BspHrtim_ModeStateMachine();
                BspAnalog_UpdateDacTarget(0.0f);
                LL_HRTIM_EnableOutput(HRTIM1, BSP_HRTIM_POWER_OUTPUT_MASK);
                g_power_ctrl.state = CTRL_MODE;
            }
        }    break;

        case CTRL_MODE:
        {
            if(s_error.faults.all != 0U)
                g_power_ctrl.state = PROTECT_MODE;
            else
            {
                PowerCtrl_CalculateCurrentTarget();
                PowerCtrl_CurrentTargetLimit();

                BspHrtim_ModeStateMachine();
                BspAnalog_UpdateDacTarget(g_power_ctrl.inducotor_current_target_a);
            }
        }    break;

        default:
        {
            g_power_ctrl.state = PROTECT_MODE;
        }    break;
    }
}


void PowerCtrl_ServiceFault(void)
{
    //不设置ICR清除ISR,ISR就是长期状态，哪怕只发生一次
    s_error.faults.all = LL_HRTIM_ReadReg(HRTIM1, sCommonRegs.ISR) & POWER_CTRL_HRTIM_FAULT_MASK;

    if(adc_fdb.A.V > BSP_ADC_AWD_V_A_HIGH_V)
        s_error.faults.bits.chassis_overvoltage = 1;
    else if(adc_fdb.A.V < BSP_ADC_AWD_V_A_LOW_V)
        s_error.faults.bits.chassis_undervoltage = 1;
    if(adc_fdb.B.V > BSP_ADC_AWD_V_B_HIGH_V)
        s_error.faults.bits.cap_overvoltage = 1;
    else if(adc_fdb.B.V < BSP_ADC_AWD_V_B_LOW_V)
        s_error.faults.bits.cap_undervoltage = 1;

    //多等待1ms判断是否可以恢复，避免四次均值后的引起捕捉不到某次高峰
    if((s_error.fault_retry_start_ms >= g_power_ctrl.fault_retry_interval_ms + 1) && !s_error.faults.all)
    {
        s_error.faults.all = 0U;
        s_error.fault_retry_start_ms = 0U;
        //指数上升到一定数值就相当于锁存
        g_power_ctrl.fault_retry_interval_ms *= 2;
    }
//                                  初次恢复正常的时候开始计算时间
    if(s_error.faults.all == 0U || !PowerCtrl_FeedbackInSafeWindow() || s_error.fault_retry_start_ms > g_power_ctrl.fault_retry_interval_ms)
        s_error.fault_retry_start_ms = 0;

    if(s_error.faults.all != 0U)
    {
        if(PowerCtrl_FeedbackInSafeWindow())
        {
        //  尝试恢复 后++
            if(s_error.fault_retry_start_ms++ == g_power_ctrl.fault_retry_interval_ms)
            {
                LL_HRTIM_WriteReg(HRTIM1, sCommonRegs.ICR, POWER_CTRL_HRTIM_FAULT_MASK);
            }
        }
    }
//裁判系统丢失不会触发锁存时间翻倍，恢复即立刻清空错误
    if(robot_status.power_management_chassis_output == 0U)
    {
        s_error.faults.bits.chassis_output_loss = 1;
    }
}

void TimerA_1ms_buffer_loop_calculate(void)
{
    if((uint32_t)(HAL_GetTick() - g_judge_rx_update_ms) > POWER_CTRL_JUDGE_TIMEOUT_MS)
    {
        robot_status.chassis_power_limit = POWER_CTRL_JUDGE_OFFLINE_LIMIT_W;
        g_power_ctrl.energy_pid.Output = 0.0f;
    }else
    {
        g_power_ctrl.energy_pid.Output = g_power_ctrl.energy_pid.Kp * (POWER_CTRL_ENERGY_TARGET_J - power_heat_data.buffer_energy);
    }
}

uint32_t PowerCtrl_ReadFaults(void)
{
    return s_error.faults.all;
}

/* ==================== 私有辅助 ==================== */

/**
 * @brief 用本板看门狗的标称电压和电流窗口判定反馈是否允许恢复。
 * @pre Timer A 在屏蔽 Master 抢占时调用，直接读取同一批 adc_fdb，单位 V、A。
 * @return 六路反馈均在含边界的窗口内时为 true，越界或无效值返回 false。
 */
static bool PowerCtrl_FeedbackInSafeWindow(void)
{
    return adc_fdb.A.V >= BSP_ADC_AWD_V_A_LOW_V
        && adc_fdb.A.V <= BSP_ADC_AWD_V_A_HIGH_V
        && adc_fdb.ref.V >= BSP_ADC_AWD_V_A_LOW_V
        && adc_fdb.ref.V <= BSP_ADC_AWD_V_A_HIGH_V
        && adc_fdb.B.V >= BSP_ADC_AWD_V_B_LOW_V
        && adc_fdb.B.V <= BSP_ADC_AWD_V_B_HIGH_V
        && adc_fdb.A.I >= BSP_ADC_AWD_I_A_LOW_A
        && adc_fdb.A.I <= BSP_ADC_AWD_I_A_HIGH_A
        && adc_fdb.ref.I >= BSP_ADC_AWD_I_BAT_LOW_A
        && adc_fdb.ref.I <= BSP_ADC_AWD_I_BAT_HIGH_A
        && adc_fdb.B.I >= BSP_ADC_AWD_I_B_LOW_A
        && adc_fdb.B.I <= BSP_ADC_AWD_I_B_HIGH_A;
}

/** @brief 丢弃故障前的三环历史及限幅记录。 */
static void PowerCtrl_ResetLoops(void)
{
    g_power_ctrl.energy_pid.PID_reset(&g_power_ctrl.energy_pid);
    g_power_ctrl.power_pid.PID_reset(&g_power_ctrl.power_pid);
    g_power_ctrl.voltage_pid.PID_reset(&g_power_ctrl.voltage_pid);
    g_power_ctrl.voltage_compete_enabled = 0U;
    g_power_ctrl.inductor_current_upper_limit_a = 0.0f;
    g_power_ctrl.inductor_current_lower_limit_a = 0.0f;
}

/**
 * @brief 根据入口功率反馈和电容电压选出有符号电感电流目标，单位 A。
 * @pre 仅在 Master 发布本批 ADC 反馈后的 CTRL_MODE 中调用。
 * @note 电压环只限充，负输出钳到零并同步增量式 PID 的输出历史。
 *       每周期计算两环候选并复位落选环，电压竞争资格独立保留。
 */
static void PowerCtrl_CalculateCurrentTarget(void)
{
    g_power_ctrl.ref_power_fdb_w = adc_fdb.ref.V * adc_fdb.ref.I;
	
	g_power_ctrl.ref_power_target_w = robot_status.chassis_power_limit - g_power_ctrl.energy_pid.Output;
	
    PID_Calculate_Increase(&g_power_ctrl.power_pid, g_power_ctrl.ref_power_fdb_w, g_power_ctrl.ref_power_target_w);

    PID_Calculate_Increase(&g_power_ctrl.voltage_pid, adc_fdb.B.V, POWER_CTRL_CAP_TARGET_V);
	
    if(g_power_ctrl.voltage_pid.Output < 0.0f)
    {
        g_power_ctrl.voltage_pid.Output = 0.0f;
        g_power_ctrl.voltage_pid.Last_Output = 0.0f;
    }

    if(g_power_ctrl.voltage_compete_enabled != 0U)
    {
        if(adc_fdb.B.V < POWER_CTRL_VOLTAGE_COMPETE_V)
            g_power_ctrl.voltage_compete_enabled = 0U;
    }
    else if(adc_fdb.B.V > POWER_CTRL_VOLTAGE_COMPETE_ENTER_V)
    {
        g_power_ctrl.voltage_compete_enabled = 1U;
    }

    if(g_power_ctrl.power_pid.Output > 0.0f
       && g_power_ctrl.voltage_compete_enabled != 0U
       && g_power_ctrl.voltage_pid.Output < g_power_ctrl.power_pid.Output)
    {
        g_power_ctrl.inducotor_current_target_a = g_power_ctrl.voltage_pid.Output;
        g_power_ctrl.power_pid.PID_reset(&g_power_ctrl.power_pid);
    }
    else
    {
        g_power_ctrl.inducotor_current_target_a = g_power_ctrl.power_pid.Output;
        g_power_ctrl.voltage_pid.PID_reset(&g_power_ctrl.voltage_pid);
    }
}

static void PowerCtrl_CurrentTargetLimit(void)
{
    if(adc_fdb.B.V >= POWER_CTRL_CAP_LOW_VOLTAGE_V)
    {
        g_power_ctrl.inductor_current_upper_limit_a = POWER_CTRL_INDUCTOR_CURRENT_PEAK_A;
        g_power_ctrl.inductor_current_lower_limit_a = -POWER_CTRL_INDUCTOR_CURRENT_PEAK_A;
    }
    else if(adc_fdb.B.V > POWER_CTRL_CAP_CURRENT_KNEE_V)
    {
        g_power_ctrl.inductor_current_upper_limit_a = POWER_CTRL_CAP_CURRENT_KNEE_A
            + (adc_fdb.B.V - POWER_CTRL_CAP_CURRENT_KNEE_V)
                * ((POWER_CTRL_INDUCTOR_CURRENT_PEAK_A - POWER_CTRL_CAP_CURRENT_KNEE_A)
                    / (POWER_CTRL_CAP_LOW_VOLTAGE_V - POWER_CTRL_CAP_CURRENT_KNEE_V));
        g_power_ctrl.inductor_current_lower_limit_a = -g_power_ctrl.inductor_current_upper_limit_a;
    }
    else
    {
        g_power_ctrl.inductor_current_upper_limit_a = POWER_CTRL_CAP_CURRENT_KNEE_A;
        g_power_ctrl.inductor_current_lower_limit_a = 0.0f;
    }

    OUTPUT_LIMIT(g_power_ctrl.inducotor_current_target_a,
                 g_power_ctrl.inductor_current_upper_limit_a,
                 g_power_ctrl.inductor_current_lower_limit_a);
	//刷新一下增量式的output不然容易超调
	g_power_ctrl.voltage_pid.Output = g_power_ctrl.inducotor_current_target_a;
	g_power_ctrl.power_pid.Output = g_power_ctrl.inducotor_current_target_a;
	
}

