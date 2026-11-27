/**
 * @file prot_supercap.h
 * @brief 超电向底盘发送的标准经典 CAN 遥测协议（单帧 0x100，固定 8 字节，小端序）。
 * @details 64 bit 帧布局：
 *          - Byte 0～1：bit 0～11 为 12 位故障字，bit 12～15 为 4 位芯片温度档位（×10 为 °C）；
 *          - Byte 2：8 位累计故障恢复重启次数；
 *          - Byte 3～4：16 位裁判端电流；
 *          - Byte 5～7：bit 0～13 为 14 位裁判端电压，bit 14～23 为 10 位最大可放电功率（0～1023 W）。
 */
#ifndef SUPERCAP_PROT_SUPERCAP_H
#define SUPERCAP_PROT_SUPERCAP_H

#include <stdint.h>

/* ==================== 宏定义 ==================== */

/** @brief 协议约定的标准帧 ID。 */
#define SUPERCAP_CAN_TX_ID (0x100U)
/** @brief 遥测帧固定长度，单位字节。 */
#define SUPERCAP_CAN_FRAME_LEN (8U)
/** @brief 12 位故障字掩码，位于 Byte 0～1 的 bit 0～11。 */
#define SUPERCAP_CAN_FAULT_MASK (0x0FFFU)
/** @brief 4 位芯片温度档位在 Byte 0～1 中的右移位偏置。 */
#define SUPERCAP_CAN_TEMP_SHIFT (12U)
/** @brief 4 位芯片温度档位掩码（范围 0～15）。 */
#define SUPERCAP_CAN_TEMP_MASK (0x0FU)
/** @brief 温度档位换算比例，单位 °C/档（读取档位乘 10 得当前温度）。 */
#define SUPERCAP_CAN_TEMP_SCALE_C (10U)
/** @brief 电流无符号量化位宽，单位 bit。 */
#define SUPERCAP_CAN_CURRENT_BITS (16U)
/** @brief 电压无符号量化位宽，单位 bit。 */
#define SUPERCAP_CAN_VOLTAGE_BITS (14U)
/** @brief 14 位电压码掩码，位于 Byte 5～7 的 bit 0～13。 */
#define SUPERCAP_CAN_VOLTAGE_MASK (0x3FFFUL)
/** @brief 可放电功率无符号量化位宽，单位 bit。 */
#define SUPERCAP_CAN_POWER_BITS (10U)
/** @brief 10 位功率码在 Byte 5～7 24 位字中的位偏置。 */
#define SUPERCAP_CAN_POWER_SHIFT (14U)
/** @brief 10 位功率码掩码（范围 0～1023）。 */
#define SUPERCAP_CAN_POWER_MASK (0x03FFUL)
/** @brief 协议约定的电压量程下限，单位 V。 */
#define SUPERCAP_CAN_VOLTAGE_MIN_V (-10.0f)
/** @brief 协议约定的电压量程上限，单位 V。 */
#define SUPERCAP_CAN_VOLTAGE_MAX_V (35.0f)
/** @brief 协议约定的电流量程下限，单位 A。 */
#define SUPERCAP_CAN_CURRENT_MIN_A (-25.0f)
/** @brief 协议约定的电流量程上限，单位 A。 */
#define SUPERCAP_CAN_CURRENT_MAX_A (25.0f)
/** @brief 协议约定的可放电功率量程下限，单位 W（仅表示放电能力）。 */
#define SUPERCAP_CAN_POWER_MIN_W (0.0f)
/** @brief 协议约定的可放电功率量程上限，单位 W（10 bit 对应 1 W/LSB）。 */
#define SUPERCAP_CAN_POWER_MAX_W (1023.0f)

/* ==================== 结构体定义 ==================== */

/**
 * @brief 底盘侧解码后的超电遥测。
 * @note 12 位故障位顺序与 error_t 一致；位域访问要求目标编译器从低位向高位布局。
 */
typedef struct
{
    union
    {
        uint16_t all; /**< Byte 0～1 的 bit 0～11，12 位完整故障字。 */
        struct
        {
            uint16_t chassis_overcurrent : 1; /**< bit 0，FLT1，底盘侧过流。 */
            uint16_t battery_overcurrent : 1; /**< bit 1，FLT2，电池入口过流。 */
            uint16_t chassis_voltage_fault : 1; /**< bit 2，FLT3，底盘侧电压异常（硬件直锁）。 */
            uint16_t cap_overcurrent : 1; /**< bit 3，FLT4，电容侧过流。 */
            uint16_t cap_voltage_fault : 1; /**< bit 4，FLT5，电容侧电压异常（硬件直锁）。 */
            uint16_t chassis_overvoltage : 1; /**< bit 5，底盘侧过压明细。 */
            uint16_t chassis_undervoltage : 1; /**< bit 6，底盘侧欠压明细。 */
            uint16_t cap_overvoltage : 1; /**< bit 7，电容侧过压明细。 */
            uint16_t cap_undervoltage : 1; /**< bit 8，电容侧欠压明细。 */
            uint16_t chassis_output_loss : 1; /**< bit 9，底盘输出许可丢失。 */
            uint16_t mcu_overtemp : 1; /**< bit 10，MCU 芯片过温保护。 */
            uint16_t adc_cal_failed : 1; /**< bit 11，上电 600 ms 校准/自检失败（不可擦除）。 */
            uint16_t reserved : 4; /**< bit 12～15，保留为零。 */
        } bits;
    } error_flags;
    uint8_t mcu_temp_c; /**< 芯片温度，单位 °C（由 4 bit 档位乘 10 还原，范围 0～150 °C）。 */
    uint8_t restart_count; /**< Byte 2，超电板故障恢复累计重启次数。 */
    float ref_current_a; /**< 裁判端反馈电流，单位 A。 */
    float ref_voltage_v; /**< 裁判端反馈电压，单位 V。 */
    float discharge_power_max_w; /**< 当前最大可放电功率，单位 W，范围 0～1023 W。 */
} SupercapCanRxData_t;

/* ==================== 外部变量声明 ==================== */

/** @brief 底盘侧最近一次解码的遥测数据。 */
extern SupercapCanRxData_t g_supercap_can_rx;

/* ==================== 全局函数声明 ==================== */

/**
 * @brief 直接读取故障字、芯片温度档位、重启次数与反馈，打包 0x100 遥测并经 FDCAN2 发送。
 * @pre 仅由 Timer A 的 1 ms 回调调用，FDCAN2 已启动；未入队时舍弃本帧，不重试。
 */
void SupercapCan_Send1ms(void);

/**
 * @brief 底盘侧解码 0x100 超电遥测，忽略不匹配的 ID 或空指针。
 * @param[in] rx_id 接收到的标准 CAN 帧 ID。
 * @param[in] data 指向 8 字节的遥测帧数据，不保留指针。
 * @pre 调用方已确认帧长为 8 字节。
 */
void SupercapCan_RxCallback(uint32_t rx_id, const uint8_t *data);

#endif /* SUPERCAP_PROT_SUPERCAP_H */
