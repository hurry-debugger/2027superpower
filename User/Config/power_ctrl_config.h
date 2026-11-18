/**
 * @file power_ctrl_config.h
 * @brief 25 kHz 控制参数及与保护共用的电压基准。
 * @note 阈值来自用户确认，实际保护能力待上板验证；编译模式由 main.h 选择。
 */
#ifndef SUPERPOWER_POWER_CTRL_CONFIG_H
#define SUPERPOWER_POWER_CTRL_CONFIG_H

/** @brief ADC1 每十次扫描产生 TC，四环均每 40 us 计算，离散系数按本调用周期定义。 */
#define POWER_CTRL_PERIOD_US (40U)
/** @brief CubeMX 配置：170 MHz * 32 / 21760，PWM 周期 4 us。 */
#define POWER_CTRL_PWM_PERIOD_US (4U)

/** @brief 用户指定的正常电容平均目标参考上限 A；按电压比换算，不含效率补偿。 */
#define POWER_CTRL_CAP_CURRENT_MAX_A (13.0f)
/** @brief 用户指定的 B 端峰值设计约束 A；仅记录约束，本轮未实现峰值保护。 */
#define POWER_CTRL_CAP_CURRENT_PEAK_A (15.0f)
/** @brief 用户确认的低压充电平均目标上限 A，参考港科 RM2024；本板能力待测。 */
#define POWER_CTRL_CAP_LOW_CHARGE_CURRENT_A (4.0f)
/**
 * @brief 能量修正输出的对称范围参数，单位 W。
 * @note 能量 PID 的 MaxOut 为此值；IntegralLimit 虽登记相同值，NONE 下不启用。
 *       此宏不直接限制 A 端目标功率；目标电流另受电容电压分段边界限制。
 */
#define POWER_CTRL_POWER_MAX_W (450.0f)
/**
 * @brief 用户确认的底盘、电容软件电压上界及控制基准，当前为 29 V。
 * @note 修改此最大电压时，须重新计算并同步下列派生宏及相关源码、文档中的电压注释。
 *       比例来自用户要求；宏表达式自动计算，注释中的当前电压值需人工同步。
 */
#define POWER_CTRL_OVERVOLTAGE_V (29.0f)
/** @brief 用户指定的标称底盘电压 V，仅用于输出占空比上限换算，不是实测反馈。 */
#define POWER_CTRL_CHASSIS_NOMINAL_VOLTAGE_V (24.0f)
/**
 * @brief 闭环及保护预置的广义占空比上限，无量纲；当前 29/24，约 1.20833。
 * @note 用户指定最大电压基准除以标称底盘电压；不改变原始前馈 (0.05,10) 门槛。
 *       此值为软件输出约束，不代表器件额定值或实板能力已经验证。
 */
#define POWER_CTRL_OUTPUT_DUTY_MAX \
    (POWER_CTRL_OVERVOLTAGE_V / POWER_CTRL_CHASSIS_NOMINAL_VOLTAGE_V)
/**
 * @brief 用户指定的电容最低电压，最大电压的 10%，单位 V；当前为 2.9 V。
 * @note 严格低于时禁止闭环输出，达到下界后由 Error 模块继续判定恢复条件。
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
/** @brief 正常平均电流目标分界，最大电压的 40%，单位 V；当前为 11.6 V，不是硬欠压门槛。 */
#define POWER_CTRL_CAP_NORMAL_VOLTAGE_V (POWER_CTRL_OVERVOLTAGE_V * 0.4f)
/** @brief 低压电流目标分界，最大电压的 20%，单位 V；当前为 5.8 V，低于时不主动请求放电。 */
#define POWER_CTRL_CAP_LOW_VOLTAGE_V (POWER_CTRL_OVERVOLTAGE_V * 0.2f)

/** @brief 用户确认的缓冲能量目标 J；目标减反馈产生功率修正 u_E，随后从功率目标扣除。 */
#define POWER_CTRL_ENERGY_TARGET_J (45.0f)

#endif /* SUPERPOWER_POWER_CTRL_CONFIG_H */
