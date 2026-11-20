/**
 * @file power_ctrl.c
 * @brief 能量修正、入口功率调节、电压竞争和 DAC 峰谷值目标。
 * @details 正电流充电、负电流放电；逻辑 A 端为底盘侧，对应 Timer B，逻辑 B 端为电容侧，对应 Timer A。
 * @note HRTIM Fault 负责故障关断，Master 根据锁存和输出资格切换功率级。
 */
#include "power_ctrl.h"

#include "bsp_analog.h"
#include "bsp_hrtim.h"
#include "power_ctrl_config.h"
#include "prot_judge.h"
#include "stm32g4xx_hal.h"

#include <stdbool.h>

/* ==================== 私有声明 ==================== */

static void PowerCtrl_CalculateCurrentTarget(void);
static void PowerCtrl_CurrentTargetLimit(void);
static void PowerCtrl_ResetLoops(void);

error_t s_error; /**< Fault 与 Timer A 共享的故障锁存及重试计时。 */
PowerCtrl_t g_power_ctrl;
/* ==================== 公共入口 ==================== */

void PowerCtrl_Init(void)
{
    g_power_ctrl.state = PROTECT_MODE;

    /* 三个位置式外环首版均使用零增益。离散系数按 Master 每 16 us 调用定义。 */
    PID_Init(&g_power_ctrl.energy_pid, 20, 
                0.0f, 0.0f,
                0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, NONE);
    PID_Init(&g_power_ctrl.power_pid, POWER_CTRL_CAP_CURRENT_MAX_A,
                POWER_CTRL_CAP_CURRENT_MAX_A, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 0.0f, NONE);
    PID_Init(&g_power_ctrl.voltage_pid, POWER_CTRL_CAP_CURRENT_MAX_A,
                POWER_CTRL_CAP_CURRENT_MAX_A, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 0.0f, 0.0f, NONE);
}

void PowerCtrl_Task(void)
{
    // clang-format off
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
//                PowerCtrl_CalculateCurrentTarget();
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
    // clang-format on
}

void PowerCtrl_RecordHrtimFault(uint32_t fault_mask)
{
    LL_HRTIM_DisableOutput(HRTIM1, BSP_HRTIM_POWER_OUTPUT_MASK);
    if(s_error.faults.all == 0U)
        s_error.fault_retry_start_ms = HAL_GetTick();

    if((fault_mask & HRTIM_FLAG_FLT1) != 0U)
        s_error.faults.bits.chassis_overcurrent = 1U;

    if((fault_mask & HRTIM_FLAG_FLT2) != 0U)
        s_error.faults.bits.battery_overcurrent = 1U;

    if((fault_mask & HRTIM_FLAG_FLT3) != 0U)
    {
        s_error.faults.bits.chassis_voltage_fault = 1U;
        if(adc_fdb.A.V > POWER_CTRL_A_REF_MAX_V)
            s_error.faults.bits.chassis_overvoltage = 1U;
        else if(adc_fdb.A.V < POWER_CTRL_A_REF_MIN_V)
            s_error.faults.bits.chassis_undervoltage = 1U;
    }

    if((fault_mask & HRTIM_FLAG_FLT4) != 0U)
        s_error.faults.bits.cap_overcurrent = 1U;

    if((fault_mask & HRTIM_FLAG_FLT5) != 0U)
    {
        s_error.faults.bits.cap_voltage_fault = 1U;
        /* 明细记录最近发布的反馈；原始 Fault 独立锁存，不依赖反馈是否已越界。 */
        if(adc_fdb.B.V > POWER_CTRL_OVERVOLTAGE_V)
            s_error.faults.bits.cap_overvoltage = 1U;
        else if(adc_fdb.B.V < POWER_CTRL_CAP_MIN_VOLTAGE_V)
            s_error.faults.bits.cap_undervoltage = 1U;
    }

    g_power_ctrl.state = PROTECT_MODE;
}

void PowerCtrl_TryRecoverFault(void)
{
    if(s_error.faults.all != 0U)
    {
        const uint32_t primask = __get_PRIMASK();

        /* Fault 可抢占 Timer A。将到期判断、待处理标志检查和清除锁存放在同一临界区。 */
        __disable_irq();
        const uint32_t now_ms = HAL_GetTick();
        if((uint32_t)(now_ms - s_error.fault_retry_start_ms) >= POWER_CTRL_FAULT_RETRY_MS)
        {
            s_error.fault_retry_start_ms = now_ms;
            if((LL_HRTIM_ReadReg(HRTIM1, sCommonRegs.ISR) & POWER_CTRL_HRTIM_FAULT_MASK) == 0U)
                s_error.faults.all = 0U;
        }
        __set_PRIMASK(primask);
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
        g_power_ctrl.energy_pid.Output = g_power_ctrl.energy_pid.Kp * (POWER_CTRL_ENERGY_TARGET_J - g_power_ctrl.energy_pid.Measure);
    }
}

uint32_t PowerCtrl_ReadFaults(void)
{
    return s_error.faults.all;
}

/* ==================== 私有辅助 ==================== */

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
        g_power_ctrl.inductor_current_upper_limit_a = POWER_CTRL_CAP_CURRENT_MAX_A;
        g_power_ctrl.inductor_current_lower_limit_a = -POWER_CTRL_CAP_CURRENT_MAX_A;
    }
    else if(adc_fdb.B.V > POWER_CTRL_CAP_CURRENT_KNEE_V)
    {
        g_power_ctrl.inductor_current_upper_limit_a = POWER_CTRL_CAP_CURRENT_KNEE_A
            + (adc_fdb.B.V - POWER_CTRL_CAP_CURRENT_KNEE_V)
                * ((POWER_CTRL_CAP_CURRENT_MAX_A - POWER_CTRL_CAP_CURRENT_KNEE_A)
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
}

