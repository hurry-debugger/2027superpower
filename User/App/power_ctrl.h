/** @file power_ctrl.h
 * @brief 超电控制环及功率级运行状态。
 */
#ifndef SUPERPOWER_POWER_CTRL_H
#define SUPERPOWER_POWER_CTRL_H

#include "pid.h"
#include "stm32g4xx_hal.h"

#include <stdint.h>

/** @brief HRTIM1 公共 ISR 中已接入的五条 Fault。 */
#define POWER_CTRL_HRTIM_FAULT_MASK \
    (HRTIM_FLAG_FLT1 | HRTIM_FLAG_FLT2 | HRTIM_FLAG_FLT3 | HRTIM_FLAG_FLT4 | HRTIM_FLAG_FLT5)

/**
 * @brief HRTIM 故障锁存和恢复计时；用户指定类型名为 error_t。
 * @note 位域顺序依赖目标编译器，运行期统一通过 all 读写 HRTIM 原始掩码。
 */
typedef struct
{
    union
    {
        uint32_t all; /**< HRTIM FLT1～FLT5 原始锁存位，零表示无锁存。 */
        struct
        {
            uint32_t chassis_overcurrent : 1; /**< bit 0，FLT1，A 侧过流。 */
            uint32_t battery_overcurrent : 1; /**< bit 1，FLT2，电池入口过流。 */
            uint32_t chassis_overvoltage : 1; /**< bit 2，FLT3，A 侧过压。 */
            uint32_t cap_overcurrent : 1; /**< bit 3，FLT4，B 侧过流。 */
            uint32_t cap_overvoltage : 1; /**< bit 4，FLT5，B 侧过压。 */
            uint32_t reserved : 27; /**< bit 5～31，未使用。 */
        } bits;
    } faults;
    uint32_t fault_retry_start_ms; /**< 首次关断或最近一次恢复尝试的 tick，单位 ms。 */
} error_t;

/** @brief 用户指定状态名；编译测试模式与运行状态彼此独立。 */
typedef enum
{
    PROTECT_MODE = 0,
    CTRL_MODE = 1,
} PowerCtrlState_e;

/**
 * @brief 控制环状态和反馈。
 * @note 功率环和电压环均输出 B 端平均电流，硬件比较器执行电感峰谷值电流控制。
 */
typedef struct
{
    volatile PowerCtrlState_e state;
    float ref_power_fdb_w; /**< 裁判端实测功率 V_BAT*I_BAT，单位 W。 */
    float ref_power_target_w; /**< 裁判端目标功率误差 P_lim-P_ref_fdb-u_E，单位 W，仅 CTRL 更新。 */
    float inducotor_current_target_a; /**< 电感峰值电流目标，单位 A，按 B 端平均电流目标折算。 */
    float inductor_current_upper_limit_a; /**< 按电容电压电流以及占空比计算的 电感峰值电流上界，单位 A。 */
    float inductor_current_lower_limit_a; /**< 按电容电压电流以及占空比计算的 电感峰值电流下界，单位 A。 */
    PID_TypeDef energy_pid; /**< 45 J 能量误差生成修正 u_E，W；每个控制周期计算。 */
    PID_TypeDef power_pid; /**< 输入净功率误差 W，输出 B 端平均电流候选 A。 */
    PID_TypeDef voltage_pid; /**< 电压环输出的 B 端平均充电电流候选，单位 A。 */
    uint8_t voltage_compete_enabled; /**< 电压资格历史，0 禁止、1 允许，预备输出时清零。 */
} PowerCtrl_t;

extern PowerCtrl_t g_power_ctrl;

/**
 * @brief 进入 PROTECT 并初始化能量、功率和电压三环。
 * @pre 输出已关闭，计数器及控制中断尚未启动，启动时调用一次。
 */
void PowerCtrl_Init(void);

/**
 * @brief 更新调制模式，将 B 端平均电流目标折算为电感目标并更新 DAC 峰谷值。
 * @pre 控制环及 HRTIM 已初始化，本周期采样已发布，初始比较值已装载。
 *      由 Master 串行调用，与 Fault 同抢占优先级；控制系数按 16 us 周期定义。
 * @note 电压资格严格高于 25.65 V 进入，低于 24.65 V 退出。
 *       每个有效控制周期计算两环候选，电压环仅在正向充电请求、资格有效且候选
 *       严格更小时胜出。落选环随即复位。跳过控制计算时，DAC 使用 0 A 电感平均目标。
 */
void PowerCtrl_Task(void);

/**
 * @brief 关闭四路功率输出，锁存触发的 HRTIM Fault，并进入保护态。
 * @param[in] fault_mask HRTIM_FLAG_FLT1～HRTIM_FLAG_FLT5 中本次触发的位。
 * @pre 仅由 HRTIM1 Fault 回调调用，与 Master 同抢占优先级。
 */
void PowerCtrl_RecordHrtimFault(uint32_t fault_mask);

/**
 * @brief 每隔 2 s 检查待处理 Fault，满足条件则清除软件锁存。
 * @pre 仅由 1 kHz Timer A 回调调用，优先级低于 Master 和 Fault。
 * @note 清锁存后由 Master 检查输出资格并处理启停。
 */
void PowerCtrl_TryRecoverFault(void);

/**
 * @brief 每隔 1ms 计算能量环输出，若裁判超时则使用固定功率上限。
 * @pre 仅由 1 kHz Timer A 回调调用，优先级低于 Master 和 Fault。
 * @note 裁判超时期间采用固定功率上限，并暂停能量环更新。
 */
void TimerA_1ms_buffer_loop_calculate(void);
/**
 * @brief 读取 HRTIM1 软件锁存位。
 * @return 与 HRTIM ISR 对齐的 FLT1～FLT5 位，零表示当前无软件锁存。
 */
uint32_t PowerCtrl_ReadHrtimFaults(void);

#endif /* SUPERPOWER_POWER_CTRL_H */
