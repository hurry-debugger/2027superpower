/**
 * @file bsp_analog.c
 * @brief 模拟外设启动、双 ADC 同步采样和 DAC 锯齿复位码更新。
 */
#include "bsp_analog.h"

#include "adc.h"
#include "comp.h"
#include "dac.h"
#include "opamp.h"

/** @brief 循环 DMA 一次填充四轮扫描，每字低半字为 ADC1，高半字为 ADC2。 */
static volatile uint32_t s_adc12_dma[BSP_ADC_SCAN_COUNT][BSP_ADC_RANK_COUNT];

BspAdcFeedback_t adc_fdb;

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

    BspAnalog_UpdateDacFromAdc();
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
        + BSP_ADC_I_A_I_BAT_BIAS_A;
    adc_fdb.B.I =
        (float)(rank_sums[0U] >> ADC_CDR_RDATA_SLV_Pos) * BSP_ADC_A_PER_SUM_COUNT
        + BSP_ADC_I_B_BIAS_A;
    adc_fdb.ref.I =
        (float)(rank_sums[1U] & ADC_CDR_RDATA_MST_Msk) * BSP_ADC_A_PER_SUM_COUNT
        + BSP_ADC_I_A_I_BAT_BIAS_A;
    adc_fdb.A.V =
        (float)(rank_sums[2U] & ADC_CDR_RDATA_MST_Msk) * BSP_ADC_V_PER_SUM_COUNT;
    adc_fdb.B.V =
        (float)(rank_sums[2U] >> ADC_CDR_RDATA_SLV_Pos) * BSP_ADC_V_PER_SUM_COUNT;
    adc_fdb.ref.V =
        (float)(rank_sums[3U] & ADC_CDR_RDATA_MST_Msk) * BSP_ADC_V_PER_SUM_COUNT;
}

void BspAnalog_UpdateDacFromAdc(void)
{
    /* 按本板零点和 INA240 标称参数直接换算，截断小数后写入锯齿复位码。 */
    if(HAL_DACEx_SawtoothWaveGenerate(
           &hdac1, DAC_CHANNEL_2, DAC_SAWTOOTH_POLARITY_INCREMENT,
           (uint32_t)((ADC_I_BAT_ZERO_V
                       + adc_fdb.A.I * (ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM))
                      * (BSP_ANALOG_DAC_CODE_COUNT / ADC_REFERENCE_V)),
           BSP_ANALOG_DAC_STEP_CODE) != HAL_OK
       || HAL_DACEx_SawtoothWaveGenerate(
           &hdac1, DAC_CHANNEL_1, DAC_SAWTOOTH_POLARITY_INCREMENT,
           (uint32_t)((BSP_ADC_I_B_ZERO_V
                       + adc_fdb.B.I * (ADC_CURRENT_SENSE_GAIN * ADC_CURRENT_SHUNT_OHM))
                      * (BSP_ANALOG_DAC_CODE_COUNT / ADC_REFERENCE_V)),
           BSP_ANALOG_DAC_STEP_CODE) != HAL_OK)
    {
        Error_Handler();
    }
}
