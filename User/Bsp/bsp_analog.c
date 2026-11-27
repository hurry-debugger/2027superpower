/**
 * @file bsp_analog.c
 * @brief 模拟外设启动、ADC 看门狗设置、双 ADC 采样和 DAC 锯齿复位码更新。
 */
#include "bsp_analog.h"

#include "adc.h"
#include "comp.h"
#include "dac.h"
#include "opamp.h"
#include "power_ctrl_config.h"
#include "stm32g4xx_ll_adc.h"

/* ==================== 私有定义 ==================== */

/** @brief 循环 DMA 一次填充四轮扫描，每字低半字为 ADC1，高半字为 ADC2。 */
static volatile uint32_t s_adc12_dma[BSP_ADC_SCAN_COUNT][BSP_ADC_RANK_COUNT];

BspAdcFeedback_t adc_fdb;

/* ==================== 私有声明 ==================== */

static void BspAnalog_InitAdcWatchdog(void);
static void BspAnalog_ConfigAwd(ADC_TypeDef *adc, uint32_t awd_instance, float low_raw,
                                float high_raw, uint32_t counts_per_code);
static inline float BspAnalog_CurrentToRaw(float current_a, float zero_v);
static inline float BspAnalog_VoltageToRaw(float voltage_v);
static uint32_t BspAnalog_CurrentToDacCode(float current_a, float zero_v);

/* ==================== 公共实现 ==================== */

void BspAnalog_Init(void)
{
    if(HAL_OPAMP_SelfCalibrate(&hopamp1) != HAL_OK
       || HAL_OPAMP_SelfCalibrate(&hopamp2) != HAL_OK
       || HAL_OPAMP_SelfCalibrate(&hopamp3) != HAL_OK)
    {
        Error_Handler();
    }

    if(HAL_OPAMP_Start(&hopamp1) != HAL_OK
       || HAL_OPAMP_Start(&hopamp2) != HAL_OK
       || HAL_OPAMP_Start(&hopamp3) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_Delay(BSP_ADC_OPAMP_SETTLE_MS);

    BspAnalog_UpdateDacTarget(0.0f);
    if(HAL_DAC_Start(&hdac1, DAC_CHANNEL_2) != HAL_OK
       || HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK
       || HAL_COMP_Start(&hcomp2) != HAL_OK
       || HAL_COMP_Start(&hcomp3) != HAL_OK)
    {
        Error_Handler();
    }

    if(HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK
       || HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }

    BspAnalog_InitAdcWatchdog();

    /* 双模式 HAL 同时使能 ADC1 和 ADC2。地址转换仅用于启动 DMA。 */
    if(HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t *)&s_adc12_dma[0U][0U],
                                    BSP_ADC_DMA_WORD_COUNT) != HAL_OK)
    {
        Error_Handler();
    }

    /* 采样由 Master 调度，DMA IRQ 继续处理传输错误。 */
    __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT | DMA_IT_TC);
}

void BspAdcFeedback_Process(void)
{
    uint32_t rank_sums[BSP_ADC_RANK_COUNT] = {0U};

    /* 每个 12 位半字四轮最多累加到 16380，低半字不会向高半字进位。 */
    for(uint32_t scan = 0U; scan < BSP_ADC_SCAN_COUNT; scan++)
    {
        rank_sums[0U] += s_adc12_dma[scan][0U];
        rank_sums[1U] += s_adc12_dma[scan][1U];
        rank_sums[2U] += s_adc12_dma[scan][2U];
        rank_sums[3U] += s_adc12_dma[scan][3U];
    }

    adc_fdb.A.I =
        (float)(rank_sums[0U] & ADC_CDR_RDATA_MST_Msk) * BSP_ADC_A_PER_SUM_COUNT
        + BSP_ADC_I_A_BAT_BIAS_A;
    adc_fdb.B.I =
        (float)(rank_sums[0U] >> ADC_CDR_RDATA_SLV_Pos) * BSP_ADC_A_PER_SUM_COUNT
        + BSP_ADC_I_B_BIAS_A;
    adc_fdb.ref.I =
        (float)(rank_sums[1U] & ADC_CDR_RDATA_MST_Msk) * BSP_ADC_A_PER_SUM_COUNT
        + BSP_ADC_I_A_BAT_BIAS_A;
    adc_fdb.A.V =
        (float)(rank_sums[2U] & ADC_CDR_RDATA_MST_Msk) * BSP_ADC_V_PER_SUM_COUNT;
    adc_fdb.B.V =
        (float)(rank_sums[2U] >> ADC_CDR_RDATA_SLV_Pos) * BSP_ADC_V_PER_SUM_COUNT;
    adc_fdb.ref.V =
        (float)(rank_sums[3U] & ADC_CDR_RDATA_MST_Msk) * BSP_ADC_V_PER_SUM_COUNT;
}

void BspAnalog_UpdateDacTarget(float inductor_target_a)
{
    (void)HAL_DACEx_SawtoothWaveGenerate(
        &hdac1, DAC_CHANNEL_2, DAC_SAWTOOTH_POLARITY_DECREMENT,
        BspAnalog_CurrentToDacCode(inductor_target_a + BSP_ANALOG_DAC_HALF_RIPPLE_A,
                                   BSP_ADC_I_A_BAT_ZERO_V),
        BSP_ANALOG_DAC_STEP_CODE);
    (void)HAL_DACEx_SawtoothWaveGenerate(
        &hdac1, DAC_CHANNEL_1, DAC_SAWTOOTH_POLARITY_INCREMENT,
        BspAnalog_CurrentToDacCode(inductor_target_a - BSP_ANALOG_DAC_HALF_RIPPLE_A,
                                   BSP_ADC_I_B_ZERO_V),
        BSP_ANALOG_DAC_STEP_CODE);
}

/* ==================== 私有实现 ==================== */

/**
 * @brief 写入五路 ADC 看门狗的 TR1/2/3 上下界，硬件事件经 EEV1～EEV5 送入 HRTIM Fault。
 * @pre CubeMX 已配置监测通道、滤波、中断和 Fault 路由，双 ADC 已校准且尚未开始转换。
 * @note 两侧电流的 3 样本滤波由 CubeMX 配置，LL 写入上下界时保留 TR1 的滤波位。
 */
static void BspAnalog_InitAdcWatchdog(void)
{
    BspAnalog_ConfigAwd(ADC1, LL_ADC_AWD1,
                        BspAnalog_CurrentToRaw(BSP_ADC_AWD_I_A_LOW_A, BSP_ADC_I_A_BAT_ZERO_V),
                        BspAnalog_CurrentToRaw(BSP_ADC_AWD_I_A_HIGH_A, BSP_ADC_I_A_BAT_ZERO_V),
                        BSP_ADC_AWD1_COUNTS_PER_CODE);
    BspAnalog_ConfigAwd(ADC1, LL_ADC_AWD2,
                        BspAnalog_CurrentToRaw(BSP_ADC_AWD_I_BAT_LOW_A, BSP_ADC_I_A_BAT_ZERO_V),
                        BspAnalog_CurrentToRaw(BSP_ADC_AWD_I_BAT_HIGH_A, BSP_ADC_I_A_BAT_ZERO_V),
                        BSP_ADC_AWD23_COUNTS_PER_CODE);
    BspAnalog_ConfigAwd(ADC1, LL_ADC_AWD3,
                        BspAnalog_VoltageToRaw(BSP_ADC_AWD_V_A_LOW_V),
                        BspAnalog_VoltageToRaw(BSP_ADC_AWD_V_A_HIGH_V),
                        BSP_ADC_AWD23_COUNTS_PER_CODE);
    BspAnalog_ConfigAwd(ADC2, LL_ADC_AWD1,
                        BspAnalog_CurrentToRaw(BSP_ADC_AWD_I_B_LOW_A, BSP_ADC_I_B_ZERO_V),
                        BspAnalog_CurrentToRaw(BSP_ADC_AWD_I_B_HIGH_A, BSP_ADC_I_B_ZERO_V),
                        BSP_ADC_AWD1_COUNTS_PER_CODE);
    BspAnalog_ConfigAwd(ADC2, LL_ADC_AWD2,
                        BspAnalog_VoltageToRaw(BSP_ADC_AWD_V_B_LOW_V),
                        BspAnalog_VoltageToRaw(BSP_ADC_AWD_V_B_HIGH_V),
                        BSP_ADC_AWD23_COUNTS_PER_CODE);
}

/**
 * @brief 将理论 12 位原码区间收紧为对应位宽的 AWD 字段码并写入寄存器。
 * @param[in] adc ADC 外设实例。
 * @param[in] awd_instance LL 看门狗编号（AWD1/2/3）。
 * @param[in] low_raw 理论下限原码，范围 0～ADC_FULL_SCALE_COUNT。
 * @param[in] high_raw 理论上限原码，范围 (counts_per_code - 1)～ADC_FULL_SCALE_COUNT。
 * @param[in] counts_per_code 每个字段码对应的 12 位原码数，为 1 或 16。
 * @pre 阈值来自 power_ctrl_config.h 固定配置，位于 ADC 有效量程内且 low_raw <= high_raw。
 * @note 下限向上取整、上限向下取整，使硬件允许区间严格落在理论物理量边界内。
 */
static void BspAnalog_ConfigAwd(ADC_TypeDef *adc, uint32_t awd_instance, float low_raw,
                                float high_raw, uint32_t counts_per_code)
{
    uint32_t low_int = (uint32_t)low_raw;

    if(low_raw > (float)low_int)
        low_int++;

    LL_ADC_ConfigAnalogWDThresholds(adc, awd_instance,
                                    ((uint32_t)high_raw - (counts_per_code - 1U))
                                        / counts_per_code,
                                    (low_int + counts_per_code - 1U) / counts_per_code);
}

/**
 * @brief 按本板 V1.3 标称采样参数，将电流和通道零点换算为理论 12 位原码。
 * @param[in] current_a 目标电流，单位 A。
 * @param[in] zero_v 对应通道的零电流参考电压，单位 V。
 * @return 理论 12 位 ADC 原码，无量纲。
 */
static inline float BspAnalog_CurrentToRaw(float current_a, float zero_v)
{
    return (zero_v + current_a * BSP_ADC_CURRENT_GAIN_V_PER_A)
        * (ADC_FULL_SCALE_COUNT / ADC_REFERENCE_V);
}

/**
 * @brief 按本板 V1.3 分压及 ADC 参考电压，将输入电压换算为理论 12 位原码。
 * @param[in] voltage_v 目标电压，单位 V。
 * @return 理论 12 位 ADC 原码，无量纲。
 */
static inline float BspAnalog_VoltageToRaw(float voltage_v)
{
    return voltage_v * (ADC_FULL_SCALE_COUNT / (ADC_REFERENCE_V * BSP_ADC_VOLTAGE_DIVIDER_RATIO));
}

/**
 * @brief 由电流 A 和通道零点 V 计算 12 位 DAC 复位码。
 * @pre current_a 有限，零点与 VREF+ 使用本板同一套标称换算参数。
 * @note 先限制码值范围，再截断小数，避免写入相邻寄存器字段。
 */
static uint32_t BspAnalog_CurrentToDacCode(float current_a, float zero_v)
{
    float reset_code = (zero_v + current_a * BSP_ADC_CURRENT_GAIN_V_PER_A)
        * (BSP_ANALOG_DAC_CODE_COUNT / ADC_REFERENCE_V);

    if(reset_code < 0.0f)
        reset_code = 0.0f;
    else if(reset_code > (float)(BSP_ANALOG_DAC_CODE_COUNT - 1U))
        reset_code = (float)(BSP_ANALOG_DAC_CODE_COUNT - 1U);

    return (uint32_t)reset_code;
}
