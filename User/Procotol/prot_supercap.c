/**
 * @file prot_supercap.c
 * @brief 超电遥测的经典 CAN 打包与解包。
 */
#include "prot_supercap.h"

#include "bsp_analog.h"
#include "bsp_can.h"
#include "power_ctrl.h"

#include <stddef.h>

SupercapCanRxData_t g_supercap_can_rx;

/* ==================== 私有声明 ==================== */

static uint16_t SupercapCan_FloatToUint(float value, float min_val, float max_val, uint8_t bits);
static float SupercapCan_UintToFloat(uint16_t raw, float min_val, float max_val, uint8_t bits);

/* ==================== 公共实现 ==================== */

void SupercapCan_Send1ms(void)
{
    uint8_t tx_data[8];
    uint16_t raw;

    tx_data[0] = (uint8_t)s_error.faults.all;

    raw = SupercapCan_FloatToUint(adc_fdb.ref.V, SUPERCAP_CAN_VOLTAGE_MIN_V,
                                 SUPERCAP_CAN_VOLTAGE_MAX_V, SUPERCAP_CAN_DATA_BITS);
    tx_data[1] = (uint8_t)raw;
    tx_data[2] = (uint8_t)(raw >> 8U);

    raw = SupercapCan_FloatToUint(adc_fdb.ref.I, SUPERCAP_CAN_CURRENT_MIN_A,
                                 SUPERCAP_CAN_CURRENT_MAX_A, SUPERCAP_CAN_DATA_BITS);
    tx_data[3] = (uint8_t)raw;
    tx_data[4] = (uint8_t)(raw >> 8U);

    raw = SupercapCan_FloatToUint(adc_fdb.B.V * (-g_power_ctrl.inductor_current_lower_limit_a),
                                 SUPERCAP_CAN_POWER_MIN_W, SUPERCAP_CAN_POWER_MAX_W,
                                 SUPERCAP_CAN_DATA_BITS);
    tx_data[5] = (uint8_t)raw;
    tx_data[6] = (uint8_t)(raw >> 8U);
    tx_data[7] = 0U;

    (void)Fdcanx_Transmit(&hfdcan2, SUPERCAP_CAN_TX_ID, tx_data);
}

void SupercapCan_RxCallback(uint32_t rx_id, const uint8_t *data)
{
    if(rx_id == SUPERCAP_CAN_TX_ID && data != NULL)
    {
        g_supercap_can_rx.error_flags.all = data[0];
        g_supercap_can_rx.ref_voltage_v = SupercapCan_UintToFloat(
            (uint16_t)((uint16_t)data[1] | ((uint16_t)data[2] << 8U)), SUPERCAP_CAN_VOLTAGE_MIN_V,
            SUPERCAP_CAN_VOLTAGE_MAX_V, SUPERCAP_CAN_DATA_BITS);
        g_supercap_can_rx.ref_current_a = SupercapCan_UintToFloat(
            (uint16_t)((uint16_t)data[3] | ((uint16_t)data[4] << 8U)), SUPERCAP_CAN_CURRENT_MIN_A,
            SUPERCAP_CAN_CURRENT_MAX_A, SUPERCAP_CAN_DATA_BITS);
        g_supercap_can_rx.discharge_power_max_w = SupercapCan_UintToFloat(
            (uint16_t)((uint16_t)data[5] | ((uint16_t)data[6] << 8U)), SUPERCAP_CAN_POWER_MIN_W,
            SUPERCAP_CAN_POWER_MAX_W, SUPERCAP_CAN_DATA_BITS);
    }
}

/* ==================== 私有实现 ==================== */

/**
 * @brief 将物理值线性量化到无符号整数，四舍五入，越界饱和，NaN 映射到零码。
 * @param[in] value 待量化的物理值，与上下界单位一致。
 * @param[in] min_val 零码对应的物理值。
 * @param[in] max_val 满码对应的物理值。
 * @param[in] bits 量化位数，范围 1～16 bit。
 * @pre max_val 大于 min_val，量程为协议定义的固定常量。
 * @return 范围为 0 到 (2^bits - 1) 的量化码。
 */
static uint16_t SupercapCan_FloatToUint(float value, float min_val, float max_val, uint8_t bits)
{
    uint32_t max_raw = (1UL << bits) - 1UL;

    if(!(value >= min_val))
        value = min_val;
    else if(value > max_val)
        value = max_val;

    return (uint16_t)((value - min_val) / (max_val - min_val) * (float)max_raw + 0.5f);
}

/**
 * @brief 将无符号量化码按对应量程线性还原。
 * @param[in] raw 协议量化码，范围 0 到 (2^bits - 1)。
 * @param[in] min_val 零码对应的物理值。
 * @param[in] max_val 满码对应的物理值。
 * @param[in] bits 量化位数，范围 1～16 bit。
 * @return 还原的物理值，单位与上下界一致。
 */
static float SupercapCan_UintToFloat(uint16_t raw, float min_val, float max_val, uint8_t bits)
{
    uint32_t max_raw = (1UL << bits) - 1UL;

    return (float)raw / (float)max_raw * (max_val - min_val) + min_val;
}
