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

/** @brief 软件锁存中 A 端过压明细位，按已发布反馈与 29 V 上界比较。 */
#define POWER_CTRL_FAULT_CHASSIS_OVERVOLTAGE (1U << 5)
/** @brief 软件锁存中 A/ref 欠压明细位，按已发布 A 端反馈与 15 V 下界比较。 */
#define POWER_CTRL_FAULT_CHASSIS_UNDERVOLTAGE (1U << 6)
/** @brief 软件锁存中 B 端过压明细位，按已发布反馈与软件 29 V 上界比较。 */
#define POWER_CTRL_FAULT_CAP_OVERVOLTAGE (1U << 7)
/** @brief 软件锁存中 B 端欠压明细位，按已发布反馈与 2.9 V 下界比较。 */
#define POWER_CTRL_FAULT_CAP_UNDERVOLTAGE (1U << 8)

/**
 * @brief HRTIM 原始故障、电压越界明细及恢复计时；用户指定类型名为 error_t。
 * @note 低五位保留原始 Fault，方向明细不能替代原始锁存；位域布局依赖目标编译器。
 */
typedef struct
{
    union
    {
        uint32_t all; /**< 低五位为 FLT1～FLT5，bit 5～8 为电压明细，零表示无锁存。 */
        struct
        {
            uint32_t chassis_overcurrent : 1; /**< bit 0，FLT1，A 侧过流。 */
            uint32_t battery_overcurrent : 1; /**< bit 1，FLT2，电池入口过流。 */
            uint32_t chassis_voltage_fault : 1; /**< bit 2，FLT3，A/ref 电压越界，方向可能未知。 */
            uint32_t cap_overcurrent : 1; /**< bit 3，FLT4，B 侧过流。 */
            uint32_t cap_voltage_fault : 1; /**< bit 4，FLT5，B 侧电压越界，方向可能未知。 */
            uint32_t chassis_overvoltage : 1; /**< bit 5，A 侧反馈超过上界。 */
            uint32_t chassis_undervoltage : 1; /**< bit 6，A 侧反馈低于共用下界。 */
            uint32_t cap_overvoltage : 1; /**< bit 7，B 侧反馈超过软件上界。 */
            uint32_t cap_undervoltage : 1; /**< bit 8，B 侧反馈低于下界。 */
            uint32_t reserved : 23; /**< bit 9～31，未使用。 */
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
 * @note 两环生成电感电流目标候选，按 B 端电压曲线限幅后直接用于 DAC 峰谷值控制，
 *       不按占空比折算 B 端平均电流。
 */
typedef struct
{
    volatile PowerCtrlState_e state;
    float ref_power_fdb_w; /**< 裁判端实测功率 V_BAT*I_BAT，单位 W。 */
    float ref_power_target_w; /**< 裁判端目标功率误差 P_lim-P_ref_fdb-u_E，单位 W，仅 CTRL 更新。 */
    float inducotor_current_target_a; /**< 有符号电感电流目标，单位 A，充电为正，不含半纹波。 */
    float inductor_current_upper_limit_a; /**< B 端电压曲线给出的电感电流目标上界，单位 A。 */
    float inductor_current_lower_limit_a; /**< B 端电压曲线给出的电感电流目标下界，单位 A。 */
    PID_TypeDef energy_pid; /**< 45 J 能量误差生成修正 u_E，W；每个控制周期计算。 */
    PID_TypeDef power_pid; /**< 输入功率误差，单位 W；输出有符号电感电流候选，单位 A。 */
    PID_TypeDef voltage_pid; /**< 电压环限充候选，范围 0～15 A，不产生放电请求。 */
    uint8_t voltage_compete_enabled; /**< 电压资格历史，0 禁止、1 允许，预备输出时清零。 */
} PowerCtrl_t;

extern PowerCtrl_t g_power_ctrl;

/**
 * @brief 进入 PROTECT 并初始化能量、功率和电压三环。
 * @pre 输出已关闭，计数器及控制中断尚未启动，启动时调用一次。
 */
void PowerCtrl_Init(void);

/**
 * @brief 按 B 端电压限制电感电流目标，更新调制模式和 DAC 峰谷值。
 * @pre 控制环及 HRTIM 已初始化，本周期采样已发布，初始比较值已装载。
 *      由 Master 串行调用，与 Fault 同抢占优先级；控制系数按 16 us 周期定义。
 * @note 电压资格严格高于 25.65 V 进入，低于 24.65 V 退出。
 *       电压环候选先限制为非负，仅在正向充电请求、资格有效且候选严格更小时胜出。
 *       落选环随即复位；无故障锁存并从保护态启用输出时，先写入 0 A DAC 目标。
 */
void PowerCtrl_Task(void);

/**
 * @brief 关闭四路功率输出，锁存触发的 HRTIM Fault，并进入保护态。
 * @param[in] fault_mask HRTIM_FLAG_FLT1～HRTIM_FLAG_FLT5 中本次触发的位。
 * @pre 仅由 HRTIM1 Fault 回调调用，与 Master 同抢占优先级。
 * @note FLT3/5 始终锁存原始电压故障；明细由最近发布的 A/B 电压与软件上下界比较。
 *       反馈不是触发瞬间的快照，未观察到越界时不推测方向，原始故障仍阻止输出。
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
 * @brief 读取原始 HRTIM 故障和电压越界明细的完整软件锁存。
 * @return 低五位与 FLT1～FLT5 对齐，bit 5～8 由 POWER_CTRL_FAULT_* 掩码解释，零表示无锁存。
 */
uint32_t PowerCtrl_ReadFaults(void);

#endif /* SUPERPOWER_POWER_CTRL_H */
