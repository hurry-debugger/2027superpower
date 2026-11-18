/**
 * @file adc_calibration_record.h
 * @brief 超电 ID、手填记录、启动拟合及运行期物理量换算。
 * @note 本文件只定义类型、宏及声明；记录、系数缓存及函数实现在对应 .c 中。
 */

#ifndef SUPERPOWER_ADC_CALIBRATION_RECORD_H
#define SUPERPOWER_ADC_CALIBRATION_RECORD_H

#include "UID.h"
#include "bsp_adc.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief 每路固定的配对校准点数；统一调整后须同步填写每一行。 */
#define ADC_CAL_POINT_COUNT (5U)
/** @brief 每通道两行：ADC 计数、实测物理量。 */
#define ADC_CAL_FIELD_COUNT (2U)
/** @brief 通道对应的 ADC 原始平均计数行，无单位；允许小数。 */
#define ADC_CAL_RAW_ROW(channel) (2U * (channel))
/** @brief 通道对应的实测值行；电压为 V，电流为带符号的 A。 */
#define ADC_CAL_REAL_ROW(channel) (2U * (channel) + 1U)

#if ADC_CAL_POINT_COUNT < 2U
#error ADC_CAL_POINT_COUNT_must_be_at_least_two
#endif

/** @brief 记录下标即超电 ID；0 为未识别保留槽位，有效板卡从 1 开始。 */
typedef enum
{
    SUPERCAP_ID_UNKNOWN = 0, /**< 未识别；对应保留槽位，不参与匹配或校准。 */
    SUPERCAP_ID_01 = 1,      /**< 第一个有效记录槽位，当前 UID 和采集点待填写。 */
    SUPERCAP_ID_02 = 2,     
    SUPERCAP_COUNT          /**< 数组槽位数，含 0 号保留槽位；新增 ID 放在它之前。 */
} SupercapId_e;

/**
 * @brief 每块超电的完整 UID 与原始校准记录，严格保留两个成员。
 * @note 同列的 ADC 计数和实测值配对；每路必须填满固定点数。
 *       缺失位置显式填 NAN，不得省略元素让 C 自动补零；合法零值不能表示缺失。
 */
typedef struct
{
    UID_t uid; /**< 全零为未登记槽位，不参与 UID 匹配。 */
    float calibration_data[ADC_CHANNEL_COUNT * ADC_CAL_FIELD_COUNT][ADC_CAL_POINT_COUNT];
    /**< 偶数行为 ADC 平均计数 [0,4095]，奇数行为对应 V/A；通过行宏索引。 */
} AdcCalibrationRecord_t;

/** @brief 在 .c 中按超电 ID 指定下标初始化；固件运行期间只读。 */
extern const AdcCalibrationRecord_t g_adc_calibration_records[SUPERCAP_COUNT];

/** @brief 全部通道位的掩码，用于初始化未校准通道集合，不代表采样新鲜度。 */
#define ADC_CAL_ALL_CHANNELS_MASK ((1U << ADC_CHANNEL_COUNT) - 1U)

/* ==================== 系数及通道错误类型 ==================== */

/** @brief 统一线性换算的系数；物理量 = ADC 计数 * gain + bias。 */
typedef struct
{
    float gain; /**< 电压通道为 V/计数，电流通道为 A/计数。 */
    float bias; /**< 电压通道为 V，电流通道为 A，允许负值。 */
} AdcCalibrationCoeff_t;

/**
 * @brief 每路校准输入和拟合错误；各成员 1 表示错误有效，0 表示无该错误。
 * @note 首个错误即停止本通道拟合，仅通过成员访问，不依赖位域布局。
 *       缺失和非有限输入由计数范围或实测值范围错误统一报告，不另存同义字段。
 */
typedef struct
{
    uint32_t count_range : 1; /**< 原始计数越界：不在 [0,4095]，包括 NaN 或无穷值。 */
    uint32_t zero_variance : 1; /**< 计数零方差：全部原始计数相同，无法拟合增益。 */
    uint32_t gain_nonpositive : 1; /**< 增益非正：拟合方向错误或窄化为 float 后下溢为零。 */
    uint32_t numeric_range : 1; /**< 数值溢出：系数或全量程换算相关值不可由 float 表示。 */
    uint32_t value_range : 1; /**< 物理量越界：实测值非法或校准区间拟合端点幅值超限。 */
    uint32_t reserved : 27; /**< 保留，保持为零；错误结构仍为一个 32 位字。 */
} AdcCalibrationChannelError_t;

/* ==================== 公共接口 ==================== */

/**
 * @brief 校验固定采集点，以中心化最小二乘拟合指定记录的单个通道。
 * @param[in] supercap_id 唯一匹配的有效记录下标，不允许 UNKNOWN。
 * @param[in] channel 有效五路通道之一。
 * @return 本通道错误结构体，所有错误成员为 0 表示成功。
 * @pre 仅在启动主线程、功率输出关闭且换算消费者尚未运行时，每通道调用一次。
 * @note 成功后更新本模块的通道系数；失败保持原有缓存，首次启动时为理论值。
 *       输入范围和拟合端点由 ADC 模拟链理论量程约束，区间外仅检查 float 可表示性。
 */
AdcCalibrationChannelError_t AdcCalibration_FitChannel(SupercapId_e supercap_id,
                                                      AdcChannel_e channel);

/**
 * @brief 使用显式增益和物理量偏置换算单个采样。
 * @param[in] raw_count 平均后的 ADC 计数，范围为 0～ADC_FULL_SCALE_COUNT。
 * @param[in] gain 增益，单位为 V/计数或 A/计数。
 * @param[in] bias 物理量偏置，单位为 V 或 A；不是原接口的计数零点。
 * @return raw_count * gain + bias，保留正负号。
 * @pre 两个系数均有限，运算中间值及结果可由 float 表示。
 * @note 不限幅、不重复扣零点、不检查参数；所有实现在 .c 中。
 */
float AdcCalibration_ConvertValue(uint16_t raw_count, float gain, float bias);

/**
 * @brief 按五路缓存系数换算平均计数，写入调用方提供的物理量数组。
 * @param[in] adc_raw 平均计数数组，各元素范围为 0～ADC_FULL_SCALE_COUNT。
 * @param[out] adc_convert 输出物理量数组；各索引的单位见 AdcChannel_e。
 * @return 传入的 adc_convert 地址；生命周期由调用方管理，下次写入前结果保持有效。
 * @pre 两个指针均非空，各有 ADC_CHANNEL_COUNT 个元素，输入输出内存不重叠。
 *      调用期间输入保持稳定、输出由本调用独占，系数缓存不允许同时初始化。
 * @note 未拟合或失败通道使用默认增益和偏置，可参与闭环；返回地址不表示已校准。
 *       板卡信息按通道保留未校准提示及原因，其他控制保护仍生效。
 *       正常换算不匹配 UID、不拟合、不平均、不执行低通。
 *       电流正方向为采样电阻 INA240 的 IN+ 侧流向 IN- 侧，实际端口方向待上板验证。
 *       不保证采样同步性，不访问 DMA；可在主线程或中断使用，耗时须在目标板测量。
 */
float *AdcCalibration_Convert(const uint16_t adc_raw[ADC_CHANNEL_COUNT],
                              float adc_convert[ADC_CHANNEL_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* SUPERPOWER_ADC_CALIBRATION_RECORD_H */
