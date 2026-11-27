/**
 * @file bsp_analog.h
 * @brief 模拟外设启动、ADC5 芯片温度采样、双 ADC 反馈及双路 DAC 更新接口。
 */
#ifndef SUPERPOWER_BSP_ANALOG_H
#define SUPERPOWER_BSP_ANALOG_H

#include <stdint.h>

/* ==================== OPAMP 与双 ADC / ADC5 采样 ==================== */

/** @brief OPAMP 启动等待时间，单位 ms，覆盖 LL_OPAMP_DELAY_STARTUP_US 的 6 us。 */
#define BSP_ADC_OPAMP_SETTLE_MS (1U)
/** @brief 每次 TRG1 触发完成四组同步 Rank，来源为 CubeMX 的 ADC1/2 配置。 */
#define BSP_ADC_RANK_COUNT (4U)
/** @brief 每次 Master 重复事件对应四轮 TRG1 扫描。 */
#define BSP_ADC_SCAN_COUNT (4U)
/** @brief 循环 DMA 长度，单位为 32 位打包字。 */
#define BSP_ADC_DMA_WORD_COUNT (BSP_ADC_SCAN_COUNT * BSP_ADC_RANK_COUNT)
/** @brief ADC5 循环 DMA 通道数，Rank 1 为 VREFINT，Rank 2 为片内温度传感器。 */
#define BSP_ADC5_CHANNEL_COUNT (2U)
/** @brief ADC5 启动后等待首轮 256× 过采样完成的时长，单位 ms（两通道一轮约 7.87 ms）。 */
#define BSP_ADC5_STARTUP_SETTLE_MS (15U)

/* ==================== 模拟前端参数与反馈换算 ==================== */

/** @brief ADC 的 VREF+ 标称电压，单位 V；原理图 V1.3 U4 REF3033，实测值待标定。 */
#define ADC_REFERENCE_V (3.3f)
/** @brief 芯片温度 4 bit 档位步长，单位 °C/档（底盘读取后乘 10 得温度）。 */
#define BSP_ADC_MCU_TEMP_STEP_C (10.0f)
/** @brief 芯片温度 4 bit 档位最大值，对应 150 °C。 */
#define BSP_ADC_MCU_TEMP_MAX_LEVEL (15U)
/** @brief 12 位右对齐 ADC 的满量程计数，ADC1/2 不启用硬件过采样，ADC5 右移后同为 12 位。 */
#define ADC_FULL_SCALE_COUNT (4095U)
/** @brief V1.3 U5/U8/U9 INA240A1 的标称增益，单位 V/V；TI 手册为 20。 */
#define ADC_CURRENT_SENSE_GAIN (20.0f)
/** @brief V1.3 R5/R6/R23 电流采样电阻，单位 Ω。 */
#define ADC_CURRENT_SHUNT_OHM (0.003f)
/** @brief 电流检测模拟前端总灵敏度，单位 V/A；由 INA240A1 增益与采样电阻推导。 */
#define BSP_ADC_CURRENT_GAIN_V_PER_A (ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM)
/** @brief 三路电压的分压还原倍数，无量纲；V1.3 模拟采样页为 33 kΩ / 2.2 kΩ。 */
#define BSP_ADC_VOLTAGE_DIVIDER_RATIO (16.0f)
/** @brief A 路和电池电流零电流输出，单位 V；V1.3 U6 REF3033 的一半，实测待标定。 */
#define BSP_ADC_I_A_BAT_ZERO_V (1.65f)
/** @brief B 路零电流输出，单位 V；V1.3 U7 REF3033 的一半，实测待标定。 */
#define BSP_ADC_I_B_ZERO_V (1.65f)

/** @brief 四轮电压计数和转换系数，单位 V/计数。 */
#define BSP_ADC_V_PER_SUM_COUNT \
    (ADC_REFERENCE_V * BSP_ADC_VOLTAGE_DIVIDER_RATIO / (BSP_ADC_SCAN_COUNT * ADC_FULL_SCALE_COUNT))
/** @brief 四轮电流计数和转换系数，单位 A/计数。 */
#define BSP_ADC_A_PER_SUM_COUNT \
    (ADC_REFERENCE_V \
     / (BSP_ADC_SCAN_COUNT * ADC_FULL_SCALE_COUNT * BSP_ADC_CURRENT_GAIN_V_PER_A))
/** @brief A 路和电池电流偏置，单位 A；两路共享 V1.3 的 U6 参考源。 */
#define BSP_ADC_I_A_BAT_BIAS_A (-BSP_ADC_I_A_BAT_ZERO_V / BSP_ADC_CURRENT_GAIN_V_PER_A)
/** @brief B 路电流偏置，单位 A；参考源为 V1.3 的 U7。 */
#define BSP_ADC_I_B_BIAS_A (-BSP_ADC_I_B_ZERO_V / BSP_ADC_CURRENT_GAIN_V_PER_A)

/* ==================== ADC 看门狗比较位宽 ==================== */

/** @brief 按 STM32G4 的比较位宽，每个 AWD1 字段码对应 1 个 12 位原码。 */
#define BSP_ADC_AWD1_COUNTS_PER_CODE (1U)
/** @brief 按 STM32G4 的比较位宽，AWD2/3 忽略低 4 位，每个字段码对应 16 个原码。 */
#define BSP_ADC_AWD23_COUNTS_PER_CODE (16U)

/* ==================== DAC 锯齿参数 ==================== */

/** @brief DAC 12 位码数，单位为计数；来源为 STM32G4 参考手册。 */
#define BSP_ANALOG_DAC_CODE_COUNT (4096U)
/**
 * @brief DAC 锯齿步进码，取自《斜坡补偿参数计算》截图选点，12.4 定点为 14.375 计数/步。
 * @note 本板斜坡补偿效果待上板验证。
 */
#define BSP_ANALOG_DAC_STEP_CODE (360U)

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
 * @brief 自校准并启动 OPAMP、ADC5 DMA（芯片温度）、DAC 与 COMP，再校准 ADC1/2、设置看门狗并启动双 ADC DMA。
 * @pre CubeMX 外设初始化完成，SysTick 正常运行，HRTIM Master/A/B 尚未启动，功率输出关闭。
 * @note 仅启动阶段调用一次。HAL 调用失败进入 Error_Handler()。
 */
void BspAnalog_Init(void);

/**
 * @brief 从 ADC5 循环 DMA 缓冲实时刷新 MCU 芯片结温。
 * @pre ADC5 循环 DMA 已启动；由初始化或 1 kHz Timer A 回调调用。
 * @note 采用标称 VREF+ 与 ST 官方 TS_CAL1/TS_CAL2 出厂标定值计算。
 */
void BspAnalog_UpdateMcuTemp(void);

/**
 * @brief 获取当前 MCU 芯片结温，单位 °C。
 * @return ADC5 内部温度传感器经出厂两点校准后的温度值。
 */
float BspAnalog_GetMcuTemp(void);

/**
 * @brief 获取 4 bit 量化后的芯片温度档位（0～15，底盘读取后乘 10 得当前温度 °C）。
 * @return 范围 0～15，低于 0 °C 钳位为 0，不低于 150 °C 钳位为 15。
 */
uint8_t BspAnalog_GetMcuTempLevel(void);

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
