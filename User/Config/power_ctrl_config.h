/**
 * @file power_ctrl_config.h
 * @brief 62.5 kHz 控制参数及与保护共用的电压基准。
 * @note 阈值来自用户确认，实际保护能力待上板验证。
 */
#ifndef SUPERPOWER_POWER_CTRL_CONFIG_H
#define SUPERPOWER_POWER_CTRL_CONFIG_H
// CLOSE_LOOP OPEN_LOOP
#define CLOSE_LOOP

/** @brief Master 每 4 个 PWM 周期重复中断，离散系数按本调用周期定义。 */
#define POWER_CTRL_PERIOD_US (16U)
/** @brief CubeMX 配置：170 MHz * 32 / 21760，PWM 周期 4 us。 */
#define POWER_CTRL_PWM_PERIOD_US (4U)

/** @brief 裁判共用接收时间戳的超时门限，单位 ms，沿用原控制方案。 */
#define POWER_CTRL_JUDGE_TIMEOUT_MS (300U)
/** @brief 用户指定的裁判超时底盘功率上限，单位 W。 */
#define POWER_CTRL_JUDGE_OFFLINE_LIMIT_W (35.0f)

/** @brief 软件 Fault 恢复尝试间隔，单位 ms，按本板恢复要求设为 1000 ms。 */
#define POWER_CTRL_FAULT_RETRY_MS (500U)

/** @brief 用户确认 A 端与 ref 并联，共用电压下限，单位 V。 */
#define POWER_CTRL_A_REF_MIN_V (15.0f)
/** @brief A/ref 共用电压上限，单位 V，引用用户确认的 29 V 基准。 */
#define POWER_CTRL_A_REF_MAX_V (POWER_CTRL_OVERVOLTAGE_V)

/** @brief 电池入口沿用 A/ref 共用范围，单位 V，不另设独立电压阈值。 */
#define POWER_CTRL_BAT_MIN_V (POWER_CTRL_A_REF_MIN_V)
#define POWER_CTRL_BAT_MAX_V (POWER_CTRL_A_REF_MAX_V)

/** @brief 保留的原电感瞬时电流幅值参数，单位 A；当前控制不引用。 */
#define POWER_CTRL_INDUCTOR_CURRENT_PEAK_A (15.0f)
/** @brief 取用户提供的 CM01 曲线幅值限制电感电流目标，单位 A，不含 DAC 半纹波偏置。 */
#define POWER_CTRL_CAP_CURRENT_MAX_A (15.0f)
/**
 * @brief 用户确认的底盘、电容软件电压上界及控制基准，当前为 29 V。
 * @note 修改此最大电压时，须重新计算并同步下列派生宏及相关源码、文档中的电压注释。
 *       比例来自用户要求；宏表达式自动计算，注释中的当前电压值需人工同步。
 */
#define POWER_CTRL_OVERVOLTAGE_V (29.0f)
/**
 * @brief 用户指定的电容最低电压，最大电压的 10%，单位 V；当前为 2.9 V。
 * @note 严格低于时禁止闭环输出，达到下界后由控制任务继续检查输出资格。
 */
#define POWER_CTRL_CAP_MIN_VOLTAGE_V (POWER_CTRL_OVERVOLTAGE_V * 0.1f)
/** @brief 电压竞争基础阈值，最大电压的 85%，单位 V；当前为 24.65 V，严格低于时退出竞争。 */
#define POWER_CTRL_VOLTAGE_COMPETE_V (POWER_CTRL_OVERVOLTAGE_V * 0.85f)
/** @brief 用户指定的竞争入口迟滞电压，单位 V；固定为 1 V，不随最大电压缩放。 */
#define POWER_CTRL_VOLTAGE_COMPETE_HYSTERESIS_V (1.0f)
/** @brief 竞争进入阈值，基础阈值加迟滞电压，单位 V；当前为 25.65 V，严格超过时进入。 */
#define POWER_CTRL_VOLTAGE_COMPETE_ENTER_V \
    (POWER_CTRL_VOLTAGE_COMPETE_V + POWER_CTRL_VOLTAGE_COMPETE_HYSTERESIS_V)
/** @brief 电容充电目标，最大电压的 95%，单位 V；当前为 27.55 V。 */
#define POWER_CTRL_CAP_TARGET_V (POWER_CTRL_OVERVOLTAGE_V * 0.95f)
/** @brief B 端达到最大充放电电流幅值的电压，单位 V，来源为用户提供的限流曲线。 */
#define POWER_CTRL_CAP_LOW_VOLTAGE_V (10.0f)
/** @brief B 端低压限流曲线的拐点，单位 V，来源为用户提供的限流曲线。 */
#define POWER_CTRL_CAP_CURRENT_KNEE_V (5.0f)
/** @brief 低压段的充电上限及拐点处的放电电流幅值，单位 A，来源为用户提供的限流曲线。 */
#define POWER_CTRL_CAP_CURRENT_KNEE_A (1.0f)

/** @brief 用户确认的缓冲能量目标 J；目标减反馈产生功率修正 u_E，随后从功率目标扣除。 */
#define POWER_CTRL_ENERGY_TARGET_J (45.0f)

#endif /* SUPERPOWER_POWER_CTRL_CONFIG_H */
