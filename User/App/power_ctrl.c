/**
 * @file power_ctrl.c
 * @brief 能量修正、入口功率调节、电压竞争和电流环。
 * @details 正电流充电、负电流放电；逻辑 A 端为 chassis，本板对应 Timer B。
 * @note Error 模块处理故障和功率级启停。
 */
#include "power_ctrl.h"

#include "bsp_analog.h"
#include "bsp_hrtim.h"
#include "error.h"
#include "power_ctrl_config.h"

/* ==================== 私有声明 ==================== */

static void PowerCtrl_DutyCalcu(void);
static void PowerCtrl_CascadePidCalcu(void);
static void PowerCtrl_CalculateCurrentLimits(void);

PowerCtrl_t g_power_ctrl;
/* ==================== 公共入口 ==================== */

void PowerCtrl_Init(void)
{
    g_power_ctrl.state = PROTECT_MODE;

    /**
     * @brief 系数按每次调用定义；能量、功率环位置式，电压、电流环增量式。
     * @details Kp 单位依次为 W/J、A/W、1/V、1/A；Ki、Kd 按逐次积分/差分定义，
     *          不自动乘除 dt。内环 Output 为完整广义占空比，应用层限于 [0.05,29/24]。
     *          四环 Improve 均为 NONE，不启用滤波、位置式条件积分或输出跟踪。
     *          内环 IntegralLimit 参数保留，增量式 Iout 为本次 Ki*Err，不使用该限值。
     */
    /* 实例在启动时由全局零初始化提供初值，再按原库格式逐环设置参数。 */
    PID_Init(&g_power_ctrl.energy_pid, POWER_CTRL_POWER_MAX_W, POWER_CTRL_POWER_MAX_W, 0.0f,
             0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f, 0.0f, NONE);
    PID_Init(&g_power_ctrl.power_pid, POWER_CTRL_CAP_CURRENT_MAX_A,
             POWER_CTRL_CAP_CURRENT_MAX_A, 0.0f,
             0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f, 0.0f,
             NONE);
    PID_Init(&g_power_ctrl.voltage_pid, POWER_CTRL_OUTPUT_DUTY_MAX,
             BSP_HRTIM_LOOP_DUTY_MAX, 0.0f,
             0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f, 0.0f,
             NONE);
    PID_Init(&g_power_ctrl.current_pid, POWER_CTRL_OUTPUT_DUTY_MAX,
             0.09f, 0.0f,
             0.02f, 0.0008f, 0.0f,
             0.0f, 0.0f, 0.0f, 0.0f,
             NONE);
}

void PowerCtrl_Task(void)
{
    g_power_ctrl.generalized_duty = adc_fdb.B.V / adc_fdb.A.V;
    g_power_ctrl.A_power_fdb = adc_fdb.A.V * adc_fdb.A.I;
    Error_ApplyProtection();
    PowerCtrl_DutyCalcu();
}

/* ==================== 占空比计算与输出 ==================== */

/**
 * @brief 保护时限幅预置，CTRL 时采用内环完整占空比更新调制。
 * @pre Error_ApplyProtection 已执行本周期保护和前馈门控。
 * @note 保护预置同样限制在 [0.05,29/24]；NaN 保留给底层跳过寄存器写入。
 */
static void PowerCtrl_DutyCalcu(void)
{
    // clang-format off
    switch(g_power_ctrl.state)
    {
        case PROTECT_MODE:
        {
            if(g_power_ctrl.generalized_duty > POWER_CTRL_OUTPUT_DUTY_MAX)
                g_power_ctrl.generalized_duty = POWER_CTRL_OUTPUT_DUTY_MAX;
            else if(g_power_ctrl.generalized_duty < BSP_HRTIM_GENERALIZED_DUTY_MIN)
                g_power_ctrl.generalized_duty = BSP_HRTIM_GENERALIZED_DUTY_MIN;
            BspHrtim_SetGeneralizedDuty(g_power_ctrl.generalized_duty);
        }    break;

        case CTRL_MODE:
        {
            PowerCtrl_CascadePidCalcu();
            BspHrtim_SetGeneralizedDuty(g_power_ctrl.generalized_duty);
        }    break;

        default:
        {
            BspHrtim_DisableOutput();
            g_power_ctrl.state = PROTECT_MODE;
        }    break;
    }
    // clang-format on
}

/**
 * @brief 计算低压目标限流及两个增量式内环，再按资格和候选占空比竞争。
 * @details B 端目标限值乘原始 Vcap/Vchassis 得到 A 端边界，分段规则保持。
 *          两环每周期各计算一次，未采用的环也推进历史，Output 为累计完整占空比。
 * @pre 本周期保护及前馈门控已执行，generalized_duty 为原始前馈，A_power_fdb 为 P_A。
 * @note 仅正目标电流允许电压竞争；能量环和功率环保持注释，不启用输出跟踪。
 *       刚接管的环改用前馈加本次 ITerm 并限幅，不清误差历史、不重复计算或竞争。
 *       13 A 仅约束目标；电压覆盖期间不能保证 B 端实际平均或峰值电流。
 */
static void PowerCtrl_CascadePidCalcu(void)
{
    PowerCtrl_CalculateCurrentLimits();

//    /* e_E = 45 J - E，输出 u_E 的单位为 W，每 40 us 按最新可读能量重复计算。
//     * 正向增益下能量不足产生正修正，后续减去它以减少充电或增加补电。
//     * NONE 不限制积分；零误差时库输出 0，但不清除 Iout。 */
//    PID_Calculate(&g_power_ctrl.energy_pid, power_heat_data.buffer_energy,
//                  POWER_CTRL_ENERGY_TARGET_J);

//     /* A 端功率与底盘用电功率之和的综合反馈，单位 W；由入口 V_BAT*I_BAT 计算。
//     * 入口采样未包含 R18 前板载供电自耗，不等同于裁判实测功率。 */
//    g_power_ctrl.total_power_fbd = adc_fdb.ref.V
//        * adc_fdb.ref.I;
//        
//    /* P_A,target = P_lim - P_in + P_A - u_E；加回 P_A 是移除已计入入口的超电支路，
//     * 再计算允许的 A 端功率。以同一 P_A 为反馈，误差化简为 P_lim - P_in - u_E。
//     * P_in 来自 V_BAT*I_BAT，不包含 R18 前板载供电自耗，计量偏差仍待实测。 */
//    g_power_ctrl.power_target_w = (float)robot_status.chassis_power_limit
//        - g_power_ctrl.total_power_fbd + g_power_ctrl.A_power_fdb
//        - g_power_ctrl.energy_pid.Output;

//    /* 功率 PID 由 W 误差生成 A 端目标电流 A，再受低压不对称边界约束。
//     * 正上界不小于负下界绝对值，用其设置库内对称输出范围；NONE 只限输出。 */
//    g_power_ctrl.power_pid.MaxOut = g_power_ctrl.current_upper_limit_a;
//    PID_CalculateConstrained(&g_power_ctrl.power_pid,
//        g_power_ctrl.A_power_fdb, g_power_ctrl.power_target_w,
//        g_power_ctrl.current_lower_limit_a, g_power_ctrl.current_upper_limit_a);
	
    /**
     * @brief 增量式两内环各推进一次历史；电压目标为最大电压的 95%（当前 27.55 V）。
     * @note 候选先限于 [0.05,29/24] 并对齐 Last_Output，再进行本周期竞争。
     *       不再使用前馈上的 ±0.2 修正边界，前馈仅在进入 CTRL 或控制权切换时预置。
     */
    PID_Calculate_Increase(&g_power_ctrl.voltage_pid,
        adc_fdb.B.V, POWER_CTRL_CAP_TARGET_V);
    OUTPUT_LIMIT(g_power_ctrl.voltage_pid.Output,
                 POWER_CTRL_OUTPUT_DUTY_MAX, BSP_HRTIM_GENERALIZED_DUTY_MIN);
    g_power_ctrl.voltage_pid.Last_Output = g_power_ctrl.voltage_pid.Output;

    PID_Calculate_Increase(&g_power_ctrl.current_pid,
        adc_fdb.A.I, g_power_ctrl.power_pid.Output);
    OUTPUT_LIMIT(g_power_ctrl.current_pid.Output,
                 POWER_CTRL_OUTPUT_DUTY_MAX, BSP_HRTIM_GENERALIZED_DUTY_MIN);
    g_power_ctrl.current_pid.Last_Output = g_power_ctrl.current_pid.Output;

    /**
     * @brief 每个 CTRL 周期独立更新电压资格，不受充放电请求或环间胜负影响。
     * @note 严格高于 25.65 V 进入，严格低于 24.65 V 退出；区间及等于边界时保持。
     *       该历史仅由 ADC 更新，不表示本周期采用电压环。
     */
    if(g_power_ctrl.voltage_compete_enabled != 0U)
    {
        if(adc_fdb.B.V < POWER_CTRL_VOLTAGE_COMPETE_V)
            g_power_ctrl.voltage_compete_enabled = 0U;
    }
    else if(adc_fdb.B.V > POWER_CTRL_VOLTAGE_COMPETE_ENTER_V)
    {
        g_power_ctrl.voltage_compete_enabled = 1U;
    }

    /**
     * @brief 正目标电流、资格允许且电压候选更小时采用电压环，相等时采用电流环。
     * @note 只在控制权变化时用原始前馈加已算出的 ITerm 重置接管环的累计基值。
     *       竞争只判定一次，预置后的实际输出可能不同于比较候选，不保证切换无扰。
     */
    if(g_power_ctrl.power_pid.Output > 0.0f
       && g_power_ctrl.voltage_pid.Output < g_power_ctrl.current_pid.Output
       && g_power_ctrl.voltage_compete_enabled != 0U)
    {
        if(g_power_ctrl.voltage_loop_active == 0U)
        {
            g_power_ctrl.voltage_pid.Output = g_power_ctrl.generalized_duty
                + g_power_ctrl.voltage_pid.ITerm;
            OUTPUT_LIMIT(g_power_ctrl.voltage_pid.Output,
                         POWER_CTRL_OUTPUT_DUTY_MAX, BSP_HRTIM_GENERALIZED_DUTY_MIN);
            g_power_ctrl.voltage_pid.Last_Output = g_power_ctrl.voltage_pid.Output;
        }
        g_power_ctrl.voltage_loop_active = 1U;
        g_power_ctrl.generalized_duty = g_power_ctrl.voltage_pid.Output;
    }
    else
    {
        if(g_power_ctrl.voltage_loop_active != 0U)
        {
            g_power_ctrl.current_pid.Output = g_power_ctrl.generalized_duty
                + g_power_ctrl.current_pid.ITerm;
            OUTPUT_LIMIT(g_power_ctrl.current_pid.Output,
                         POWER_CTRL_OUTPUT_DUTY_MAX, BSP_HRTIM_GENERALIZED_DUTY_MIN);
            g_power_ctrl.current_pid.Last_Output = g_power_ctrl.current_pid.Output;
        }
        g_power_ctrl.voltage_loop_active = 0U;
        g_power_ctrl.generalized_duty = g_power_ctrl.current_pid.Output;
    }

    OUTPUT_LIMIT(g_power_ctrl.generalized_duty,
                 POWER_CTRL_OUTPUT_DUTY_MAX, BSP_HRTIM_GENERALIZED_DUTY_MIN);
}

/* ==================== 私有辅助 ==================== */

/**
 * @brief 计算 A 端目标电流上下限，单位 A。
 * @details 按电容电压分段计算 B 端充放电限值，再乘原始前馈换算至 A 端。
 * @pre 仅控制上下文串行调用，generalized_duty 为本周期原始 Vcap/Vchassis。
 * @note 直接更新既有上下限成员；目标限幅不等同于实际电流或峰值保护。
 */
static void PowerCtrl_CalculateCurrentLimits(void)
{
    /* 低压时限制从电容提取功率，仍允许较小充电目标；5.8～11.6 V 连续恢复双向能力。
     * 两个成员先作 B 端安培工作值，再原位换算为 A 端边界，避免另存同义副本。 */
    if(adc_fdb.B.V < POWER_CTRL_CAP_LOW_VOLTAGE_V)
    {
        g_power_ctrl.current_upper_limit_a = POWER_CTRL_CAP_LOW_CHARGE_CURRENT_A;
        g_power_ctrl.current_lower_limit_a = 0.0f;
    }
    else if(adc_fdb.B.V < POWER_CTRL_CAP_NORMAL_VOLTAGE_V)
    {
        g_power_ctrl.current_upper_limit_a = POWER_CTRL_CAP_LOW_CHARGE_CURRENT_A
            + (POWER_CTRL_CAP_CURRENT_MAX_A - POWER_CTRL_CAP_LOW_CHARGE_CURRENT_A)
                * (adc_fdb.B.V - POWER_CTRL_CAP_LOW_VOLTAGE_V)
                / (POWER_CTRL_CAP_NORMAL_VOLTAGE_V - POWER_CTRL_CAP_LOW_VOLTAGE_V);
        g_power_ctrl.current_lower_limit_a = -POWER_CTRL_CAP_CURRENT_MAX_A
            * (adc_fdb.B.V - POWER_CTRL_CAP_LOW_VOLTAGE_V)
            / (POWER_CTRL_CAP_NORMAL_VOLTAGE_V - POWER_CTRL_CAP_LOW_VOLTAGE_V);
    }
    else
    {
        g_power_ctrl.current_upper_limit_a = POWER_CTRL_CAP_CURRENT_MAX_A;
        g_power_ctrl.current_lower_limit_a = -POWER_CTRL_CAP_CURRENT_MAX_A;
    }
    /* 理想功率平衡 V_A*I_A = V_B*I_B，故 A 端限值为 B 端参考乘 V_B/V_A。
     * 这里不估算效率，也不把目标限幅等同于实际平均电流或峰值保护。 */
    g_power_ctrl.current_upper_limit_a *= g_power_ctrl.generalized_duty;
    g_power_ctrl.current_lower_limit_a *= g_power_ctrl.generalized_duty;
}
