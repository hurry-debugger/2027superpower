/**
 * @file bsp_adc.c
 * @brief 双 ADC 同步采样、循环 DMA 及六路物理量发布。
 */
#include "bsp_adc.h"

#include "adc.h"
#include "opamp.h"

/** @brief 循环 DMA 一次填充四轮扫描，每字低半字为 ADC1，高半字为 ADC2。 */
static volatile uint32_t s_adc12_dma[BSP_ADC_SCAN_COUNT][BSP_ADC_RANK_COUNT];

BspAdcFeedback_t adc_fdb;
float g_bsp_adc_legacy_chassis_current_a;

void BspAdcFeedback_Init(void)
{
    if(HAL_OPAMP_Start(&hopamp1) != HAL_OK
       || HAL_OPAMP_Start(&hopamp2) != HAL_OK
       || HAL_OPAMP_Start(&hopamp3) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_Delay(BSP_ADC_OPAMP_SETTLE_MS);

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
