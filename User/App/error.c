/**
 * @file error.c
 * @brief 超电故障记录、自动恢复、保护启停和电容电荷积分。
 */
#include "error.h"

#include "bsp_analog.h"
#include "bsp_hrtim.h"
#include "hrtim.h"
#include "power_ctrl.h"
#include "power_ctrl_config.h"

/* ==================== 私有定义 ==================== */

typedef char ErrorGroupSizeCheck[(sizeof(ErrorGroup_t) == sizeof(uint32_t)) ? 1 : -1];

/* 首次采样检查前，电压资格为无效，阻止控制过早开启。 */
volatile ErrorData_t g_error =
{
    .auto_recover.flags.all = ERROR_CAP_VOLTAGE_MASK | ERROR_INPUT_VOLTAGE_MASK
};

static volatile uint32_t s_last_trip_ms; /**< Master/Fault 更新的最近一次锁存时间。 */
static volatile uint32_t s_short_circuit_score; /**< Master 加权，Timer A 衰减及清零。 */
static uint16_t s_power_off_cycles; /**< Timer A 记录连续低压周期，饱和于门槛加一。 */
static uint16_t s_cap_charge_cycles; /**< 当前窗口已累计样本数，零表示开启新窗口。 */

/* ==================== 私有声明 ==================== */

static void Error_LatchTrip(uint32_t error_mask);
static void PowerCtrl_UpdatePowerStage(bool allow_output);
static void PowerCtrl_ResetLoops(void);
static void PowerCtrl_IntegrateCapCharge(void);

/* ==================== 公共实现 ==================== */

ErrorLevel_e Error_GetLevel(void)
{
    ErrorLevel_e level = ERROR_NONE;

    if(g_error.unrecoverable.flags.all != 0U)
        level = ERROR_UNRECOVERABLE;
    else if(g_error.auto_recover.flags.all != 0U)
        level = ERROR_RECOVER_AUTO;

    return level;
}

void Error_CheckFast(void)
{
    uint32_t short_mask = 0U;

    if(adc_fdb.B.V >= POWER_CTRL_CAP_MIN_VOLTAGE_V && adc_fdb.B.V <= POWER_CTRL_OVERVOLTAGE_V)
        g_error.auto_recover.flags.all &= ~ERROR_CAP_VOLTAGE_MASK;
    else
        g_error.auto_recover.flags.all |= ERROR_CAP_VOLTAGE_MASK;

    if(BspHrtim_IsPowerOutputEnabled())
    {
        if(adc_fdb.A.V <= ERROR_SHORT_CIRCUIT_V && -adc_fdb.A.I >= ERROR_SHORT_CIRCUIT_A)
        {
            s_short_circuit_score += ERROR_SHORT_A_WEIGHT;
            if(s_short_circuit_score > ERROR_SHORT_TRIP_SCORE)
                short_mask |= ERROR_SHORT_A_MASK;
        }

        if(adc_fdb.B.V <= ERROR_SHORT_CIRCUIT_V && adc_fdb.B.I >= ERROR_SHORT_CIRCUIT_A)
        {
            s_short_circuit_score += ERROR_SHORT_B_WEIGHT;
            if(s_short_circuit_score > ERROR_SHORT_TRIP_SCORE)
                short_mask |= ERROR_SHORT_B_MASK;
        }

        if(short_mask != 0U)
            Error_LatchTrip(short_mask);
    }
}

void Error_Update1ms(void)
{
    float chassis_voltage_v;
    float bat_voltage_v;
    uint32_t voltage_errors = 0U;
    uint32_t primask = __get_PRIMASK();

    /* 保留同一批反馈，电压比较在临界区外执行。 */
    __disable_irq();
    __DMB();
    chassis_voltage_v = adc_fdb.A.V;
    bat_voltage_v = adc_fdb.ref.V;
    __DMB();
    __set_PRIMASK(primask);

    if(!(chassis_voltage_v >= ERROR_CHASSIS_UVLO_V && chassis_voltage_v <= POWER_CTRL_OVERVOLTAGE_V))
        voltage_errors |= ERROR_CHASSIS_VOLTAGE_MASK;

    if(!(bat_voltage_v >= ERROR_BAT_MIN_V && bat_voltage_v <= ERROR_BAT_MAX_V))
        voltage_errors |= ERROR_BAT_VOLTAGE_MASK;

    /* 连续欠压用于识别下电，当前欠压仍通过实时位阻止输出。 */
    if(chassis_voltage_v < ERROR_CHASSIS_UVLO_V)
    {
        if(s_power_off_cycles <= ERROR_POWER_OFF_LIMIT_CYCLES)
            s_power_off_cycles++;
    }
    else
        s_power_off_cycles = 0U;

    /* 合并最新锁存位，并在同一临界区检查恢复与积分的输出状态。 */
    __disable_irq();
    __DMB();
    g_error.auto_recover.flags.all =
        (g_error.auto_recover.flags.all & ~ERROR_INPUT_VOLTAGE_MASK) | voltage_errors;

    if((g_error.auto_recover.flags.all & ERROR_SHORT_MASK) == 0U && s_short_circuit_score > 0U)
        s_short_circuit_score--;

    if((g_error.auto_recover.flags.all & ERROR_TRIP_MASK) != 0U
       && ((uint32_t)(HAL_GetTick() - s_last_trip_ms) >= ERROR_AUTO_RECOVER_DELAY_MS
           || s_power_off_cycles > ERROR_POWER_OFF_LIMIT_CYCLES)
       && (hhrtim1.Instance->sCommonRegs.ISR & ERROR_HRTIM_FAULT_MASK) == 0U)
    {
        if((g_error.auto_recover.flags.all & ERROR_SHORT_MASK) != 0U)
            s_short_circuit_score = 0U;

        g_error.auto_recover.flags.all &= ~ERROR_TRIP_MASK;
    }

    PowerCtrl_IntegrateCapCharge();
    __DMB();
    __set_PRIMASK(primask);
}

void Error_RecordHrtimFault(uint32_t fault_mask)
{
    Error_LatchTrip(fault_mask);
}

void Error_ApplyProtection(void)
{
    PowerCtrl_UpdatePowerStage(
        Error_GetLevel() == ERROR_NONE
        && (g_power_ctrl.state == CTRL_MODE ? adc_fdb.A.V >= ERROR_CHASSIS_UVLO_V
                                           : adc_fdb.A.V > ERROR_CHASSIS_RESTART_V)
        && g_power_ctrl.generalized_duty > BSP_HRTIM_GENERALIZED_DUTY_MIN
        && g_power_ctrl.generalized_duty < BSP_HRTIM_GENERALIZED_DUTY_MAX
        && (hhrtim1.Instance->sCommonRegs.ISR & ERROR_HRTIM_FAULT_MASK) == 0U);
}

/* ==================== 私有实现 ==================== */

/** @brief 先关断输出，随后锁存所有命中的位并更新自动恢复起点。 */
static void Error_LatchTrip(uint32_t error_mask)
{
    BspHrtim_TripPowerOutput();
    g_error.auto_recover.flags.all |= error_mask;
    s_last_trip_ms = HAL_GetTick();
}

/** @brief 统一切换功率级。恢复时先预置电流环，控制态无需重复开启。 */
static void PowerCtrl_UpdatePowerStage(bool allow_output)
{
    if(!allow_output)
    {
        BspHrtim_DisableOutput();
        g_power_ctrl.state = PROTECT_MODE;
    }
    else if(g_power_ctrl.state == PROTECT_MODE)
    {
        PowerCtrl_ResetLoops();
        g_power_ctrl.current_pid.Output = g_power_ctrl.generalized_duty;
        g_power_ctrl.current_pid.Last_Output = g_power_ctrl.current_pid.Output;
        BspHrtim_EnableOutput();
        g_power_ctrl.state = CTRL_MODE;
    }
}

/** @brief 丢弃故障前的控制历史，保留本周期前馈及功率反馈。 */
static void PowerCtrl_ResetLoops(void)
{
    g_power_ctrl.energy_pid.PID_reset(&g_power_ctrl.energy_pid);
    g_power_ctrl.power_pid.PID_reset(&g_power_ctrl.power_pid);
    g_power_ctrl.voltage_pid.PID_reset(&g_power_ctrl.voltage_pid);
    g_power_ctrl.current_pid.PID_reset(&g_power_ctrl.current_pid);
    g_power_ctrl.voltage_compete_enabled = 0U;
    g_power_ctrl.voltage_loop_active = 0U;
    g_power_ctrl.current_upper_limit_a = 0.0f;
    g_power_ctrl.current_lower_limit_a = 0.0f;
}

/**
 * @brief 用 B 路标称电流积分电荷变化量，单位 C，充电为正。
 * @pre Timer A 临界区内调用，输出状态和 ADC 电流取自本次采样。
 * @note 关断保留显示值并结束窗口，恢复积分时首样本从零累计。
 */
static void PowerCtrl_IntegrateCapCharge(void)
{
    if(BspHrtim_IsPowerOutputEnabled())
    {
        if(s_cap_charge_cycles == 0U)
            g_power_ctrl.cap_charge_c = 0.0f;

        g_power_ctrl.cap_charge_c += adc_fdb.B.I * ERROR_CAP_CHARGE_PERIOD_S;
        s_cap_charge_cycles++;
        if(s_cap_charge_cycles == ERROR_CAP_CHARGE_WINDOW_CYCLES)
            s_cap_charge_cycles = 0U;
    }
    else
        s_cap_charge_cycles = 0U;
}
