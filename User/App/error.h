/**
 * @file error.h
 * @brief 超电故障分级、恢复及功率级保护入口。
 */
#ifndef SUPERPOWER_ERROR_H
#define SUPERPOWER_ERROR_H

#include "bsp_analog.h"
#include "stm32g4xx_hal.h"

#include <stdint.h>

/* ==================== 保护参数与错误掩码 ==================== */

/** @brief 底盘关断与恢复阈值，单位 V，来源为港科 RM2025 上场版。 */
#define ERROR_CHASSIS_UVLO_V (18.0f)
#define ERROR_CHASSIS_RESTART_V (20.0f)

/** @brief 电池入口有效范围，单位 V。下界沿用旧控制，量程上界按本板分压还原。 */
#define ERROR_BAT_MIN_V (15.0f)
#define ERROR_BAT_MAX_V (ADC_REFERENCE_V * BSP_ADC_VOLTAGE_DIVIDER_RATIO)

/** @brief 港科 RM2025 软件短路判定的电压、电流门槛，单位分别为 V、A。 */
#define ERROR_SHORT_CIRCUIT_V (5.0f)
#define ERROR_SHORT_CIRCUIT_A (5.0f)

/** @brief 港科 RM2025 软件短路积分权重与触发门槛，无量纲。 */
#define ERROR_SHORT_A_WEIGHT (600U)
#define ERROR_SHORT_B_WEIGHT (300U)
#define ERROR_SHORT_TRIP_SCORE (1700U)

/** @brief 港科 RM2025 可恢复故障清除周期，单位 ms，来源为 WARNING_PERIOD。 */
#define ERROR_AUTO_RECOVER_DELAY_MS (5000U)
/** @brief 港科 RM2025 持续下电门槛，单位为 1 kHz 周期数。 */
#define ERROR_POWER_OFF_LIMIT_CYCLES (2000U)

/** @brief Timer A 以 1 kHz 积分，步长为秒，窗口为港科 RM2025 约 1 秒的采样数。 */
#define ERROR_CAP_CHARGE_PERIOD_S (0.001f)
#define ERROR_CAP_CHARGE_WINDOW_CYCLES (1000U)

/** @brief 硬件低五位与 ErrorGroup_t 的前五个具名位一一对应。 */
#define ERROR_HRTIM_FAULT_MASK \
    (HRTIM_FLAG_FLT1 | HRTIM_FLAG_FLT2 | HRTIM_FLAG_FLT3 | HRTIM_FLAG_FLT4 | HRTIM_FLAG_FLT5)
#define ERROR_SHORT_A_MASK (1UL << 5U)
#define ERROR_SHORT_B_MASK (1UL << 6U)
#define ERROR_CAP_VOLTAGE_MASK (1UL << 7U)
#define ERROR_CHASSIS_VOLTAGE_MASK (1UL << 8U)
#define ERROR_BAT_VOLTAGE_MASK (1UL << 9U)
#define ERROR_SHORT_MASK (ERROR_SHORT_A_MASK | ERROR_SHORT_B_MASK)
#define ERROR_TRIP_MASK (ERROR_HRTIM_FAULT_MASK | ERROR_SHORT_MASK)
#define ERROR_INPUT_VOLTAGE_MASK (ERROR_CHASSIS_VOLTAGE_MASK | ERROR_BAT_VOLTAGE_MASK)

/** @brief 错误等级按阻断优先级排列，无错误时允许检查开启条件。 */
typedef enum
{
    ERROR_NONE = 0,
    ERROR_RECOVER_AUTO,
    ERROR_UNRECOVERABLE
} ErrorLevel_e;

/**
 * @brief 单个等级的错误字，具名位供调试，all 用于或入及清除。
 * @note 目标为 ARMCC 小端，低五位对应 HRTIM ISR 的 FLT1～5。
 *       此布局只供固件内部使用，CAN 由协议模块显式映射。
 */
typedef struct
{
    union
    {
        uint32_t all;
        struct
        {
            uint32_t a_overcurrent : 1;     /**< FLT1，A 侧电流。 */
            uint32_t input_overcurrent : 1; /**< FLT2，电池入口电流。 */
            uint32_t a_overvoltage : 1;     /**< FLT3，A 侧电压。 */
            uint32_t b_overcurrent : 1;     /**< FLT4，B 侧电流。 */
            uint32_t b_overvoltage : 1;     /**< FLT5，B 侧电压。 */
            uint32_t short_circuit_a : 1;  /**< A 侧软件短路锁存。 */
            uint32_t short_circuit_b : 1;  /**< B 侧软件短路锁存。 */
            uint32_t cap_voltage : 1;      /**< 电容电压越出有效范围。 */
            uint32_t chassis_voltage : 1;  /**< 底盘电压越出有效范围。 */
            uint32_t bat_voltage : 1;      /**< 电池电压越出有效范围。 */
            uint32_t power_stage : 1;      /**< 功率级不可恢复错误，预留。 */
            uint32_t cap_array : 1;        /**< 电容组不可恢复错误，预留。 */
            uint32_t reserved : 20;
        } bits;
    } flags;
} ErrorGroup_t;

/** @brief 两个等级独立记录，自动恢复只清本组符合条件的位。 */
typedef struct
{
    ErrorGroup_t auto_recover;
    ErrorGroup_t unrecoverable;
} ErrorData_t;

/** @brief 由 Error 模块写入，调试器和其他模块读取。 */
extern volatile ErrorData_t g_error;

/** @brief 从两个错误字返回最高等级。 */
ErrorLevel_e Error_GetLevel(void);

/**
 * @brief 刷新电容电压有效性，并根据本周期 A/B 电压、电流检测软件短路。
 * @pre 仅由 62.5 kHz Master 回调在发布 ADC 反馈后调用，与 Fault 同抢占优先级。
 */
void Error_CheckFast(void);

/**
 * @brief 更新入口电压错误、短路积分、自动恢复和电容电荷积分。
 * @pre 仅由 1 kHz Timer A 回调调用，优先级低于 Master 和 Fault。
 */
void Error_Update1ms(void);

/**
 * @brief 关断功率级并锁存 HRTIM Fault，硬件标志由 HAL 维护。
 * @param[in] fault_mask HAL 回调对应的 HRTIM_FLAG_FLT1～HRTIM_FLAG_FLT5 位。
 * @pre 仅由 HRTIM1 的 Fault 回调调用。
 */
void Error_RecordHrtimFault(uint32_t fault_mask);

/**
 * @brief 根据错误字、欠压迟滞和原始前馈决定功率级启停。
 * @pre PowerCtrl 已初始化，本周期反馈、快速保护和原始前馈已更新。
 *      由控制上下文串行调用，与 Master、Fault 同抢占优先级。
 */
void Error_ApplyProtection(void);

#endif /* SUPERPOWER_ERROR_H */
