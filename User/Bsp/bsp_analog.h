/**
 * @file bsp_analog.h
 * @brief 模拟外设启动、双 ADC 反馈及双路 DAC 更新接口。
 */
#ifndef SUPERPOWER_BSP_ANALOG_H
#define SUPERPOWER_BSP_ANALOG_H

/* ==================== OPAMP 启动 ==================== */

/** @brief OPAMP 起动等待时间，单位 ms，覆盖 LL_OPAMP_DELAY_STARTUP_US 的 6 us。 */
#define BSP_ADC_OPAMP_SETTLE_MS (1U)

/* ==================== 双 ADC 采样 ==================== */

/** @brief 每次 TRG1 触发完成四组同步 Rank，来源为 CubeMX 的 ADC1/2 配置。 */
#define BSP_ADC_RANK_COUNT (4U)
/** @brief 每次 Master 重复事件对应四轮 TRG1 扫描。 */
#define BSP_ADC_SCAN_COUNT (4U)
/** @brief 循环 DMA 长度，单位为 32 位打包字。 */
#define BSP_ADC_DMA_WORD_COUNT (BSP_ADC_SCAN_COUNT * BSP_ADC_RANK_COUNT)
/** @brief 四轮平均的编译期系数，无量纲。 */
#define BSP_ADC_SCAN_RECIPROCAL (1.0f / BSP_ADC_SCAN_COUNT)

/* ==================== 当前反馈换算 ==================== */

/** @brief ADC 的 VREF+ 标称电压，单位 V；原理图 V1.3 U4 REF3033，实测值待标定。 */
#define ADC_REFERENCE_V (3.3f)
/** @brief 12 位右对齐 ADC 的满量程计数，不启用硬件过采样。 */
#define ADC_FULL_SCALE_COUNT (4095U)
/** @brief V1.3 U5/U8/U9 INA240A1 的标称增益，单位 V/V；TI 手册为 20。 */
#define ADC_CURRENT_SENSE_GAIN (20.0f)
/** @brief V1.3 R5/R6/R23 电流采样电阻，单位 Ω。 */
#define ADC_CURRENT_SHUNT_OHM (0.003f)
/** @brief 电池电流通道零电流输出，单位 V；V1.3 U6 参考的一半，独立于 ADC 的 U4 参考。 */
#define ADC_I_BAT_ZERO_V (1.65f)
/** @brief B 路零电流输出，单位 V；V1.3 的 U7 REF3033 的一半，实测待标定。 */
#define BSP_ADC_I_B_ZERO_V (1.65f)
/** @brief 三路电压的分压还原倍数，无量纲；V1.3 模拟采样页为 33 kΩ / 2.2 kΩ。 */
#define BSP_ADC_VOLTAGE_DIVIDER_RATIO (16.0f)

/** @brief 四轮电压计数和转换系数，单位 V/计数。 */
#define BSP_ADC_V_PER_SUM_COUNT \
    (BSP_ADC_SCAN_RECIPROCAL * ADC_REFERENCE_V * BSP_ADC_VOLTAGE_DIVIDER_RATIO / ADC_FULL_SCALE_COUNT)
/** @brief 四轮电流计数和转换系数，单位 A/计数。 */ 
#define BSP_ADC_A_PER_SUM_COUNT \
    (BSP_ADC_SCAN_RECIPROCAL * ADC_REFERENCE_V / \
     (ADC_FULL_SCALE_COUNT * ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM))
/** @brief A 路和电池电流偏置，单位 A；两路共享 V1.3 的 U6 参考源。 */
#define BSP_ADC_I_A_I_BAT_BIAS_A (-ADC_I_BAT_ZERO_V / (ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM))
/** @brief B 路电流偏置，单位 A；参考源为 V1.3 的 U7。 */
#define BSP_ADC_I_B_BIAS_A (-BSP_ADC_I_B_ZERO_V / (ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM))

/* ==================== ADC 看门狗阈值 ==================== */

/**
 * @brief 本板 V1.3 标称参数及已确认保护范围换算后的 ADC 看门狗阈值字段。
 * @details 电流理论码 = (零点 + 电流 × 20 V/V × 0.003 Ω) × 4095 / 3.3 V，
 *          A 侧和电池入口取 U6 的 1.65 V 零点，B 侧取 U7 的 1.65 V 零点。
 *          电压理论码 = (输入电压 / 16) × 4095 / 3.3 V。
 *          A/ref 并联，A 侧看门狗监测共用的 15～29 V 范围；B 侧下限沿用 2.9 V，
 *          上限沿用港科 RM2025 的 30.5 V。电流阈值同样沿用该参考设定。
 * @note AWD1 比较 12 位原始码，下界向上取整，上界向下取整。
 *       AWD2/3 比较高 8 位，字段值 k 对应原始码 16k 到 16k + 15。
 *       下界取满足 16k ≥ 理论下界的最小 k，上界取满足 16k + 15 ≤ 理论上界的最大 k。
 *       实际触发值待上板校验。
 */
#define BSP_ADC_AWD_I_A_LOW_CODE (559U)    /**< -20.0 A，理论码 558.41，向上取整。 */
#define BSP_ADC_AWD_I_A_HIGH_CODE (3536U)  /**< +20.0 A，理论码 3536.59，向下取整。 */
#define BSP_ADC_AWD_I_BAT_LOW_CODE (98U)   /**< -6.5 A，理论码 1563.55，整组起码 1568。 */
#define BSP_ADC_AWD_I_BAT_HIGH_CODE (157U) /**< +6.5 A，理论码 2531.45，整组末码 2527。 */
#define BSP_ADC_AWD_V_A_LOW_CODE (73U)     /**< 15.0 V，理论码 1163.35，整组起码 1168。 */
#define BSP_ADC_AWD_V_A_HIGH_CODE (139U)   /**< 29.0 V，理论码 2249.15，整组末码 2239。 */
#define BSP_ADC_AWD_I_B_LOW_CODE (149U)    /**< -25.5 A，理论码 148.91，向上取整。 */
#define BSP_ADC_AWD_I_B_HIGH_CODE (3946U)  /**< +25.5 A，理论码 3946.09，向下取整。 */
#define BSP_ADC_AWD_V_B_LOW_CODE (15U)     /**< 2.9 V，理论码 224.91，整组起码 240。 */
#define BSP_ADC_AWD_V_B_HIGH_CODE (146U)   /**< 30.5 V，理论码 2365.48，整组末码 2351。 */

/* ==================== DAC 锯齿 ==================== */

/** @brief DAC 12 位码数，单位为计数；来源为 STM32G4 参考手册。 */
#define BSP_ANALOG_DAC_CODE_COUNT (4096U)
/**
 * @brief DAC 锯齿步进码，取自《斜坡补偿参数计算》截图选点，12.4 定点为 14.375 计数/步。
 * @note 本板斜坡补偿效果待上板验证。
 */
#define BSP_ANALOG_DAC_STEP_CODE (230U)
/** @brief 电感半纹波偏置，单位 A；首版沿用港科 RM2025 上场版的 1.25 A，待本板实测。 */
#define BSP_ANALOG_DAC_HALF_RIPPLE_A (1.25f)

/* ==================== 旧五路兼容参数 ==================== */

/** @note 旧五路的分压、零点及量程兼容旧校准和保护接口。 */

/** @brief 底盘分压还原倍数，无量纲；R26=13 kΩ、R34=1 kΩ。 */
#define ADC_V_CHASSIS_DIVIDER_RATIO (14.0f)
/** @brief 电池分压还原倍数，无量纲；R38=10 kΩ、R46=1 kΩ。 */
#define ADC_V_BAT_DIVIDER_RATIO (11.0f)
/** @brief 电容分压还原倍数，无量纲；R27=10 kΩ、R71=1 kΩ。 */
#define ADC_V_CAP_DIVIDER_RATIO (11.0f)
/** @brief 底盘电流通道零电流输出，单位 V；标称值，后续可独立标定。 */
#define ADC_I_CHASSIS_ZERO_V (1.65f)

/** @brief 底盘电压转换系数，单位 V/计数。 */
#define ADC_V_CHASSIS_RATIO \
    (ADC_REFERENCE_V * ADC_V_CHASSIS_DIVIDER_RATIO / ADC_FULL_SCALE_COUNT)
/** @brief 电池电压转换系数，单位 V/计数。 */
#define ADC_V_BAT_RATIO (ADC_REFERENCE_V * ADC_V_BAT_DIVIDER_RATIO / ADC_FULL_SCALE_COUNT)
/** @brief 电容电压转换系数，单位 V/计数。 */
#define ADC_V_CAP_RATIO (ADC_REFERENCE_V * ADC_V_CAP_DIVIDER_RATIO / ADC_FULL_SCALE_COUNT)
/** @brief 电池电流转换系数，单位 A/计数。 */
#define ADC_I_BAT_RATIO \
    (ADC_REFERENCE_V / ADC_FULL_SCALE_COUNT / (ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM))
/** @brief 底盘电流转换系数，单位 A/计数。 */
#define ADC_I_CHASSIS_RATIO \
    (ADC_REFERENCE_V / ADC_FULL_SCALE_COUNT / (ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM))

/** @brief 底盘电压零点，单位计数；标称无偏置。 */
#define ADC_V_CHASSIS_OFFSET_COUNT (0.0f)
/** @brief 电池电压零点，单位计数；标称无偏置。 */
#define ADC_V_BAT_OFFSET_COUNT (0.0f)
/** @brief 电容电压零点，单位计数；标称无偏置。 */
#define ADC_V_CAP_OFFSET_COUNT (0.0f)
/** @brief 电池电流零点，单位计数；标称 2047.5，不提前取整。 */
#define ADC_I_BAT_OFFSET_COUNT (ADC_I_BAT_ZERO_V * ADC_FULL_SCALE_COUNT / ADC_REFERENCE_V)
/** @brief 底盘电流零点，单位计数；允许与电池通道分别标定。 */
#define ADC_I_CHASSIS_OFFSET_COUNT (ADC_I_CHASSIS_ZERO_V * ADC_FULL_SCALE_COUNT / ADC_REFERENCE_V)

/** @brief 底盘电压 ADC 理论量程上限，单位 V；由分压比和 ADC 满量程推导。 */
#define ADC_V_CHASSIS_MAX_V ((ADC_FULL_SCALE_COUNT - ADC_V_CHASSIS_OFFSET_COUNT) * ADC_V_CHASSIS_RATIO)
/** @brief 电池电压 ADC 理论量程上限，单位 V；由分压比和 ADC 满量程推导。 */
#define ADC_V_BAT_MAX_V ((ADC_FULL_SCALE_COUNT - ADC_V_BAT_OFFSET_COUNT) * ADC_V_BAT_RATIO)
/** @brief 电容电压 ADC 理论量程上限，单位 V；由分压比和 ADC 满量程推导。 */
#define ADC_V_CAP_MAX_V ((ADC_FULL_SCALE_COUNT - ADC_V_CAP_OFFSET_COUNT) * ADC_V_CAP_RATIO)
/** @brief 电池电流 ADC 理论量程下限，单位 A；由零点、采样电阻和放大倍数推导。 */
#define ADC_I_BAT_MIN_A (-ADC_I_BAT_OFFSET_COUNT * ADC_I_BAT_RATIO)
/** @brief 电池电流 ADC 理论量程上限，单位 A；由零点、采样电阻和放大倍数推导。 */
#define ADC_I_BAT_MAX_A ((ADC_FULL_SCALE_COUNT - ADC_I_BAT_OFFSET_COUNT) * ADC_I_BAT_RATIO)
/** @brief 底盘电流 ADC 理论量程下限，单位 A；由零点、采样电阻和放大倍数推导。 */
#define ADC_I_CHASSIS_MIN_A (-ADC_I_CHASSIS_OFFSET_COUNT * ADC_I_CHASSIS_RATIO)
/** @brief 底盘电流 ADC 理论量程上限，单位 A；由零点、采样电阻和放大倍数推导。 */
#define ADC_I_CHASSIS_MAX_A \
    ((ADC_FULL_SCALE_COUNT - ADC_I_CHASSIS_OFFSET_COUNT) * ADC_I_CHASSIS_RATIO)

/* ==================== 类型与接口 ==================== */

/** @brief 校准记录及旧调用方使用的五路索引，枚举值与记录行序对应。 */
typedef enum
{
    V_CHASSIS = 0,     /**< 底盘电压 V，ADC1 Rank 3 / IN12 / PB1。 */
    V_BAT,             /**< 电池电压 V，ADC1 Rank 4 / IN3 / PA2。 */
    I_BAT,             /**< 电池电流 A，ADC1 Rank 2 / IN8 / PC2。 */
    I_CHASSIS,         /**< 旧底盘电流 A 校准索引。 */
    V_CAP,             /**< 电容电压 V，ADC2 Rank 3 / IN3 / PA6。 */
    ADC_CHANNEL_COUNT  /**< 旧校准记录通道数。 */
} AdcChannel_e;

/** @brief 单端口的电压和电流反馈。 */
typedef struct
{
    float V; /**< 电压，单位 V。 */
    float I; /**< 电流，单位 A。 */
} BspAdcPort_t;

typedef struct
{
    BspAdcPort_t ref; /**< 电池入口，V_BAT 和 I_BAT。 */
    BspAdcPort_t A;   /**< 底盘侧，V_CHASSIS 和 I_A。 */
    BspAdcPort_t B;   /**< 电容侧，V_CAP 和 I_B。 */
} BspAdcFeedback_t;

/** @brief Master 重复回调写入六路物理量；其他上下文需要整组数据时须取得一致快照。 */
extern BspAdcFeedback_t adc_fdb;

/**
 * @brief 自校准并启动 OPAMP，准备并启动 DAC 和 COMP，再校准 ADC1/2、设置看门狗并启动双 ADC DMA。
 * @pre CubeMX 外设初始化完成，SysTick 正常运行，HRTIM Master/A/B 尚未启动，功率输出关闭。
 * @note 仅启动阶段调用一次。HAL 调用失败进入 Error_Handler()。
 */
void BspAnalog_Init(void);

/**
 * @brief 对四轮同步扫描按 Rank 累加并换算，发布六路标称电压及电流。
 * @pre 采样已初始化，由 62.5 kHz Master 重复回调调用。
 * @pre DMA 已写完四轮，CPU 须在下一轮覆盖缓冲区前完成读取。
 * @note 电流正方向为 INA240 的 IN+ 流向 IN-。
 */
void BspAdcFeedback_Process(void);

/**
 * @brief 更新 DAC1 CH2 峰值和 CH1 谷值的锯齿复位码。
 * @param[in] inductor_target_a 有限的有符号电感电流目标，单位 A，充电为正。
 * @pre CubeMX 已配置 DAC1 两路的 HRTIM 复位与步进触发。
 * @pre 启动时在 DAC 使能前调用，运行时仅由 Master 调用。
 * @note CH2 加半纹波偏置并递增，CH1 减半纹波偏置并递减。两路使用相同的步进码，
 *       各自在 HRTIM 复位触发时装载复位码。
 */
void BspAnalog_UpdateDacTarget(float inductor_target_a);

#endif /* SUPERPOWER_BSP_ANALOG_H */
