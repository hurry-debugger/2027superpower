/**
 * @file power_ctrl_config.h
 * @brief 62.5 kHz 控制参数及与保护共用的电压基准。
 * @note 阈值来自用户确认，实际保护能力待上板验证。
 */
#ifndef SUPERPOWER_POWER_CTRL_CONFIG_H
#define SUPERPOWER_POWER_CTRL_CONFIG_H
// CLOSE_LOOP OPEN_LOOP
#define CLOSE_LOOP

/** @brief 裁判共用接收时间戳的超时门限，单位 ms，沿用原控制方案。 */
#define POWER_CTRL_JUDGE_TIMEOUT_MS (300U)
/** @brief 用户指定的裁判超时底盘功率上限，单位 W。 */
#define POWER_CTRL_JUDGE_OFFLINE_LIMIT_W (35.0f)

/** @brief 电感半纹波偏置，单位 A；首版沿用港科 RM2025 上场版的 1.25 A，待本板实测。 */
#define BSP_ANALOG_DAC_HALF_RIPPLE_A (1.85f)
/** @brief 电感瞬时电流幅值参数，单位 A。 */
#define POWER_CTRL_INDUCTOR_CURRENT_PEAK_A (15.0f- BSP_ANALOG_DAC_HALF_RIPPLE_A)

/* ==================== ADC 看门狗阈值 ==================== */

/**
 * @brief 看门狗物理量阈值，电流单位 A，电压单位 V。
 * @details A/ref 并联，共用 A 侧电压保护范围。B 侧下限沿用 2.9 V，
 *          上限和各路电流阈值沿用港科 RM2025 的设置。
 * @note 电压下限填 0.0f 时，比较字段为 0，调试时可关闭对应欠压触发。
 *       实际触发值待上板校验。
 */
#define BSP_ADC_AWD_I_A_HIGH_A (20.0f)
#define BSP_ADC_AWD_I_A_LOW_A (-20.0f)
#define BSP_ADC_AWD_I_BAT_HIGH_A (8.5f)//chassis口最高输出200W否则超载
#define BSP_ADC_AWD_I_BAT_LOW_A (-8.5f)
#define BSP_ADC_AWD_I_B_LOW_A (-25.5f)
#define BSP_ADC_AWD_I_B_HIGH_A (25.5f)

#define BSP_ADC_AWD_V_A_HIGH_V (29.0f)
#define BSP_ADC_AWD_V_B_HIGH_V (25.0f)

#ifdef CLOSE_LOOP
	#define BSP_ADC_AWD_V_B_LOW_V (3.0f)
	#define BSP_ADC_AWD_V_A_LOW_V (15.0f)
#endif

#ifdef OPEN_LOOP
	#define BSP_ADC_AWD_V_B_LOW_V (0.0f)
	#define BSP_ADC_AWD_V_A_LOW_V (0.0f)
#endif


/**
 * @brief 用户确认的底盘、电容软件电压上界及控制基准，当前为 29 V。
 * @note 修改此最大电压时，须重新计算并同步下列派生宏及相关源码、文档中的电压注释。
 *       比例来自用户要求；宏表达式自动计算，注释中的当前电压值需人工同步。
 */
#define POWER_CAP_MAX_V (24.0f)
/**
 * @brief 用户指定的电容最低电压，最大电压的 10%，单位 V；当前为 2.9 V。
 * @note 严格低于时禁止闭环输出，达到下界后由控制任务继续检查输出资格。
 */
#define POWER_CTRL_CAP_MIN_VOLTAGE_V (POWER_CAP_MAX_V * 0.1f)
/** @brief 电压竞争基础阈值，最大电压的 85%，单位 V；当前为 24.65 V，严格低于时退出竞争。 */
#define POWER_CTRL_VOLTAGE_COMPETE_V (POWER_CAP_MAX_V * 0.85f)
/** @brief 用户指定的竞争入口迟滞电压，单位 V；固定为 1 V，不随最大电压缩放。 */
#define POWER_CTRL_VOLTAGE_COMPETE_HYSTERESIS_V (1.0f)
/** @brief 竞争进入阈值，基础阈值加迟滞电压，单位 V；当前为 25.65 V，严格超过时进入。 */
#define POWER_CTRL_VOLTAGE_COMPETE_ENTER_V \
    (POWER_CTRL_VOLTAGE_COMPETE_V + POWER_CTRL_VOLTAGE_COMPETE_HYSTERESIS_V)
/** @brief 电容充电目标，最大电压的 95%，单位 V；当前为 27.55 V。 */
#define POWER_CTRL_CAP_TARGET_V (POWER_CAP_MAX_V * 0.95f)
/** @brief B 端达到最大充放电电流幅值的电压，单位 V，来源为用户提供的限流曲线。 */
#define POWER_CTRL_CAP_LOW_VOLTAGE_V (10.0f)
/** @brief B 端低压限流曲线的拐点，单位 V，来源为用户提供的限流曲线。 */
#define POWER_CTRL_CAP_CURRENT_KNEE_V (5.0f)
/** @brief 低压段的充电上限及拐点处的放电电流幅值，单位 A，来源为用户提供的限流曲线。 */
#define POWER_CTRL_CAP_CURRENT_KNEE_A (1.0f)

/** @brief 用户确认的缓冲能量目标 J；目标减反馈产生功率修正 u_E，随后从功率目标扣除。 */
#define POWER_CTRL_ENERGY_TARGET_J (45.0f)

/* ==================== 芯片过温保护 ==================== */

/** @brief 芯片过温保护触发阈值，单位 °C；Timer A 中严格超过时置位过温标志。 */
#define POWER_CTRL_MCU_OVERTEMP_C (95.0f)
/** @brief 芯片过温保护恢复阈值（迟滞），单位 °C；回落至不高于此值时解除过温标志。 */
#define POWER_CTRL_MCU_OVERTEMP_RECOVER_C (80.0f)

#endif /* SUPERPOWER_POWER_CTRL_CONFIG_H */
