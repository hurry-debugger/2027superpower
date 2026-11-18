/** @file power_ctrl.h
 * @brief 超电控制环及功率级运行状态。
 */
#ifndef SUPERPOWER_POWER_CTRL_H
#define SUPERPOWER_POWER_CTRL_H

#include "pid.h"
#include <stdint.h>

/** @brief 用户指定状态名；编译测试模式与运行状态彼此独立。 */
typedef enum
{
    PROTECT_MODE = 0,
    CTRL_MODE = 1
} PowerCtrlState_e;

/**
 * @brief 控制环状态、反馈和电荷积分记录。
 * @note A_power_fdb 与 total_power_fbd 分别保存 P_A 和 P_in，单位 W。
 *       竞争限幅后的电流目标直接观察 power_pid.Output。
 *       电压环和电流环使用完整的增量式占空比，四环 Improve 均为 NONE。
 */
typedef struct
{
    volatile PowerCtrlState_e state;
    volatile float cap_charge_c; /**< 1 kHz 积分的窗口电荷变化量，单位 C，充电为正。 */
    float A_power_fdb; /**< A 端 V_CHASSIS*I_CHASSIS，单位 W；正充电、负放电。 */
    float total_power_fbd; /**< A 端功率与底盘用电功率之和的综合反馈，单位 W；由 V_BAT*I_BAT 计算。 */
    float power_target_w; /**< A 端目标功率 P_lim-P_in+P_A-u_E，单位 W；仅 CTRL 更新。 */
    float current_upper_limit_a; /**< A 端正向目标上界 A；B 端充电参考上限乘原始前馈。 */
    float current_lower_limit_a; /**< A 端负向目标下界 A；低于最大电压的 20%（当前 5.8 V）为零，禁止主动请求放电。 */
    float generalized_duty; /**< 先存原始前馈供门控及换算，再存 [0.05,29/24] 内的控制或保护预置值。 */
    PID_TypeDef energy_pid; /**< 45 J 能量误差生成修正 u_E，W；每个控制周期计算。 */
    PID_TypeDef power_pid; /**< A 端目标与反馈之差等价于入口功率误差；输出目标电流 A。 */
    PID_TypeDef voltage_pid; /**< 电压增量式内环，Output 为完整广义占空比；正目标且资格允许时参与竞争。 */
    PID_TypeDef current_pid; /**< A 端电流增量式内环，Output 为完整广义占空比；接管时预置，保留误差历史。 */
    uint8_t voltage_compete_enabled; /**< 电压资格历史：0 禁止、1 允许；运行期仅 ADC 更新，启动全局零初始化、恢复复位清零。 */
    uint8_t voltage_loop_active; /**< 上周期实际控制权：0 电流环、1 电压环；仅 ADC 读写，恢复清零，用于识别接管边沿。 */
} PowerCtrl_t;

extern PowerCtrl_t g_power_ctrl;

/**
 * @brief 进入 PROTECT 并初始化四个控制环。
 * @pre 输出已关闭，计数器及控制中断尚未启动，启动时调用一次。
 */
void PowerCtrl_Init(void);

/**
 * @brief 更新前馈和功率反馈，经 Error 模块保护门控后计算占空比。
 * @pre 控制环及 HRTIM 已初始化，采样、快速保护已更新，比较值已装载。
 *      串行调用，与 Master、Fault 同抢占优先级；控制系数按 40 us 周期定义。
 * @note 电压资格严格高于 25.65 V 进入，低于 24.65 V 退出。
 *       正目标且电压候选更小时采用电压环，其余采用电流环。
 *       接管时用实测前馈和本周期 ITerm 预置输出，限幅并对齐 Last_Output。
 */
void PowerCtrl_Task(void);

#endif /* SUPERPOWER_POWER_CTRL_H */
