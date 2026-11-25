/** @file power_ctrl.h
 * @brief 超电控制环及功率级运行状态。
 */
#ifndef SUPERPOWER_POWER_CTRL_H
#define SUPERPOWER_POWER_CTRL_H

#include "pid.h"
#include "stm32g4xx_hal.h"

#include <stdint.h>

/** @brief Timer A 轮询的 HRTIM1 公共 ISR 中的五条 Fault。 */
#define POWER_CTRL_HRTIM_FAULT_MASK \
    (HRTIM_FLAG_FLT1 | HRTIM_FLAG_FLT2 | HRTIM_FLAG_FLT3 | HRTIM_FLAG_FLT4 | HRTIM_FLAG_FLT5)

/** @brief ADC1 的 A 侧电流、电池入口电流和 A 侧电压看门狗标志。 */
#define POWER_CTRL_ADC1_AWD_MASK (ADC_FLAG_AWD1 | ADC_FLAG_AWD2 | ADC_FLAG_AWD3)
/** @brief ADC2 的 B 侧电流和电压看门狗标志。 */
#define POWER_CTRL_ADC2_AWD_MASK (ADC_FLAG_AWD1 | ADC_FLAG_AWD2)
/** @brief 四种调制模式间最多连续切换三次，用于输出关闭时完成模式选择。 */
#define POWER_CTRL_HRTIM_MODE_MAX_TRANSITIONS (3U)

/** @brief 软件锁存中 A 端过压明细位，反馈超过 AWD 标称上界。 */
#define POWER_CTRL_FAULT_CHASSIS_OVERVOLTAGE (1U << 5)
/** @brief 软件锁存中 A 端欠压明细位，反馈低于 AWD 标称下界。 */
#define POWER_CTRL_FAULT_CHASSIS_UNDERVOLTAGE (1U << 6)
/** @brief 软件锁存中 B 端过压明细位，反馈超过 AWD 标称上界。 */
#define POWER_CTRL_FAULT_CAP_OVERVOLTAGE (1U << 7)
/** @brief 软件锁存中 B 端欠压明细位，反馈低于 AWD 标称下界。 */
#define POWER_CTRL_FAULT_CAP_UNDERVOLTAGE (1U << 8)

/**
 * @brief HRTIM 原始故障、电压越界明细及恢复计时；用户指定类型名为 error_t。
 * @note 低五位保留原始 Fault，方向明细不能替代原始锁存；位域布局依赖目标编译器。
 */
typedef struct
{
    union
    {
        volatile uint32_t all; /**< Timer A 发布，低五位为 FLT1～FLT5，bit 5～8 为电压明细。 */
        struct
        {
            uint32_t chassis_overcurrent : 1; /**< bit 0，FLT1，A 侧过流。 */
            uint32_t battery_overcurrent : 1; /**< bit 1，FLT2，电池入口过流。 */
            uint32_t chassis_voltage_fault : 1; /**< bit 2，FLT3，A/ref 电压越界，方向可能未知。 */
            uint32_t cap_overcurrent : 1; /**< bit 3，FLT4，B 侧过流。 */
            uint32_t cap_voltage_fault : 1; /**< bit 4，FLT5，B 侧电压越界，方向可能未知。 */
            uint32_t chassis_overvoltage : 1; /**< bit 5，A 侧反馈超过上界。 */
            uint32_t chassis_undervoltage : 1; /**< bit 6，A 侧反馈低于共用下界。 */
            uint32_t cap_overvoltage : 1; /**< bit 7，B 侧反馈超过 AWD 标称上界。 */
            uint32_t cap_undervoltage : 1; /**< bit 8，B 侧反馈低于下界。 */
            uint32_t reserved : 23; /**< bit 9～31，未使用。 */
        } bits;
    } faults;
    uint32_t fault_retry_start_ms; /**< 首次锁存或最近一次恢复尝试的 tick，单位 ms。 */
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
    uint32_t fault_retry_interval_ms; /**< 软件 Fault 恢复尝试间隔，单位 ms，初始值为250，每发生一次fault恢复后翻倍。 */
} PowerCtrl_t;

extern PowerCtrl_t g_power_ctrl;
extern error_t s_error;

/**
 * @brief 进入 PROTECT 并初始化能量、功率和电压三环。
 * @pre 输出已关闭，计数器及控制中断尚未启动，启动时调用一次。
 */
void PowerCtrl_Init(void);

/**
 * @brief 保护时关断四路输出并复位控制环，运行时按 B 端电压限幅并更新 DAC 目标。
 * @pre 仅由 62.5 kHz Master 回调在发布本轮反馈后调用，抢占优先级高于 Timer A。
 * @note 无故障锁存时，保护态先更新调制并写入 0 A DAC 目标，再进入运行态。
 *       四路输出由 Timer A 在确认条件满足后使能。
 */
void PowerCtrl_Task(void);

/**
 * @brief 轮询 FLT1～FLT5，锁存故障，并在故障期间每隔 1000 ms 检查恢复条件。
 * @pre 仅由 1 kHz Timer A 回调调用，抢占优先级低于 Master，公用 Fault 中断已关闭。
 * @pre 运行期已配置的 AWD 和 FLT 标志只由本函数清除。
 * @note ADC 反馈检查合格时清除硬件标志，下一次轮询确认标志均未再置位才解除锁存。
 *       解除锁存后的轮询由 Master 先进入运行态，再由本函数复核六路反馈和 HRTIM 状态后使能。
 */
void PowerCtrl_ServiceFault(void);

/**
 * @brief 每隔 1ms 计算能量环输出，若裁判超时则使用固定功率上限。
 * @pre 仅由 1 kHz Timer A 回调调用，抢占优先级低于 Master。
 * @note 裁判超时期间采用固定功率上限，并暂停能量环更新。
 */
void TimerA_1ms_buffer_loop_calculate(void);
/**
 * @brief 读取原始 HRTIM 故障和电压越界明细的完整软件锁存。
 * @return 低五位与 FLT1～FLT5 对齐，bit 5～8 由 POWER_CTRL_FAULT_* 掩码解释，零表示无锁存。
 */
uint32_t PowerCtrl_ReadFaults(void);

#endif /* SUPERPOWER_POWER_CTRL_H */
