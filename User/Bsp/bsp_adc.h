/**
 * @file bsp_adc.h
 * @brief 双 ADC 同步采样与六路物理量接口。
 */
#ifndef SUPERPOWER_BSP_ADC_H
#define SUPERPOWER_BSP_ADC_H

/** @brief OPAMP 起动等待时间，单位 ms，覆盖 LL_OPAMP_DELAY_STARTUP_US 的 6 us。 */
#define BSP_ADC_OPAMP_SETTLE_MS (1U)
/** @brief 每次 TRG1 触发完成四组同步 Rank，来源为 CubeMX 的 ADC1/2 配置。 */
#define BSP_ADC_RANK_COUNT (4U)
/** @brief 每次 Master 重复事件对应四轮 TRG1 扫描。 */
#define BSP_ADC_SCAN_COUNT (4U)
/** @brief 循环 DMA 长度，单位为 32 位打包字。 */
#define BSP_ADC_DMA_WORD_COUNT (BSP_ADC_SCAN_COUNT * BSP_ADC_RANK_COUNT)

/* ==================== 标称硬件参数 ==================== */

/** @note 旧五路的分压、零点及量程兼容旧校准和保护接口。 */

/** @brief ADC 的 VREF+ 标称电压，单位 V；原理图 V1.3 U4 REF3033，实测值待标定。 */
#define ADC_REFERENCE_V (3.3f)
/** @brief 12 位右对齐 ADC 的满量程计数，不启用硬件过采样。 */
#define ADC_FULL_SCALE_COUNT (4095U)
/** @brief 底盘分压还原倍数，无量纲；R26=13 kΩ、R34=1 kΩ。 */
#define ADC_V_CHASSIS_DIVIDER_RATIO (14.0f)
/** @brief 电池分压还原倍数，无量纲；R38=10 kΩ、R46=1 kΩ。 */
#define ADC_V_BAT_DIVIDER_RATIO (11.0f)
/** @brief 电容分压还原倍数，无量纲；R27=10 kΩ、R71=1 kΩ。 */
#define ADC_V_CAP_DIVIDER_RATIO (11.0f)
/** @brief V1.3 U5/U8/U9 INA240A1 的标称增益，单位 V/V；TI 手册为 20。 */
#define ADC_CURRENT_SENSE_GAIN (20.0f)
/** @brief V1.3 R5/R6/R23 电流采样电阻，单位 Ω。 */
#define ADC_CURRENT_SHUNT_OHM (0.003f)
/** @brief 电池电流通道零电流输出，单位 V；V1.3 U6 参考的一半，独立于 ADC 的 U4 参考。 */
#define ADC_I_BAT_ZERO_V (1.65f)
/** @brief 底盘电流通道零电流输出，单位 V；标称值，后续可独立标定。 */
#define ADC_I_CHASSIS_ZERO_V (1.65f)
/* ==================== 各通道转换参数 ==================== */

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

/** @brief 四轮平均的编译期系数，无量纲。 */
#define BSP_ADC_SCAN_RECIPROCAL (1.0f / BSP_ADC_SCAN_COUNT)
/** @brief 三路电压的分压还原倍数，无量纲；V1.3 模拟采样页为 33 kΩ / 2.2 kΩ。 */
#define BSP_ADC_VOLTAGE_DIVIDER_RATIO (16.0f)
/** @brief B 路零电流输出，单位 V；V1.3 的 U7 REF3033 的一半，实测待标定。 */
#define BSP_ADC_I_B_ZERO_V (1.65f)

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

/** @brief 旧底盘电流接口占位，单位 A；上电为 0，当前采样路径不写入。 */
extern float g_bsp_adc_legacy_chassis_current_a;

/**
 * @brief 启动电压跟随器、校准 ADC1/2 并准备双 ADC 循环 DMA。
 * @pre CubeMX 外设初始化完成，SysTick 正常运行，HRTIM Master 尚未启动，功率输出关闭。
 * @note 仅启动阶段调用一次。HAL 调用失败进入 Error_Handler()。
 */
void BspAdcFeedback_Init(void);

/**
 * @brief 对四轮同步扫描按 Rank 累加并换算，发布六路标称电压及电流。
 * @pre 采样已初始化，由 62.5 kHz Master 重复回调调用。
 * @pre DMA 已写完四轮，CPU 须在下一轮覆盖缓冲区前完成读取。
 * @note 电流正方向为 INA240 的 IN+ 流向 IN-。
 */
void BspAdcFeedback_Process(void);

#endif /* SUPERPOWER_BSP_ADC_H */
