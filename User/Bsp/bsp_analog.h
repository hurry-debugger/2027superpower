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

/* ==================== DAC 锯齿 ==================== */

/** @brief DAC 12 位码数，单位为计数；来源为 STM32G4 参考手册。 */
#define BSP_ANALOG_DAC_CODE_COUNT (4096U)
/**
 * @brief DAC 锯齿步进码，取自《斜坡补偿参数计算》截图选点，12.4 定点为 14.375 计数/步。
 * @note 截图的次谐波稳定性结论仅适用于参考参数和所列工况；本板尚未上板验证。
 */
#define BSP_ANALOG_DAC_STEP_CODE (230U)

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
 * @brief 自校准并启动 OPAMP，准备并启动 DAC 和 COMP，再校准 ADC1/2 并启动双 ADC DMA。
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
 * @brief 直接读取 adc_fdb.A.I/B.I（单位 A），更新 DAC1 CH2/CH1 的锯齿复位码。
 * @pre CubeMX 已配置 DAC 通道及触发源，功率输出关闭。
 * @pre 启动时由 BspAnalog_Init() 在 ADC 反馈首次写入前调用，使用零初始化电流；
 *      运行时仅在 Master 回调发布本轮 ADC 反馈后调用。
 * @pre 电流换算结果须满足 HAL 的 0 至 4095 复位码范围，本函数不作限幅或有效性判断。
 * @note 按 4096 计数换算并截断小数，ADC 满量程可能得到 4096，不自动修正。
 * @note A 路为 CH2/COMP2，B 路为 CH1/COMP3；A 路写入失败时跳过 B 路。
 * @note 不重配触发源，不把实测电流作为闭环峰值目标。HAL 调用失败进入 Error_Handler()。
 */
void BspAnalog_UpdateDacFromAdc(void);

#endif /* SUPERPOWER_BSP_ANALOG_H */
