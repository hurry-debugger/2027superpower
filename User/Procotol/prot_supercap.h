/**
 * @file prot_supercap.h
 * @brief 超电向底盘发送的标准经典 CAN 遥测协议，固定 8 字节，小端序。
 */
#ifndef SUPERPOWER_PROT_SUPERCAP_H
#define SUPERPOWER_PROT_SUPERCAP_H

#include <stdint.h>

/** @brief 协议约定的标准帧 ID。 */
#define SUPERCAP_CAN_TX_ID (0x100U)
/** @brief 协议约定的无符号量化位宽，单位 bit。 */
#define SUPERCAP_CAN_DATA_BITS (16U)
/** @brief 协议约定的电压量程下限，单位 V。 */
#define SUPERCAP_CAN_VOLTAGE_MIN_V (-10.0f)
/** @brief 协议约定的电压量程上限，单位 V。 */
#define SUPERCAP_CAN_VOLTAGE_MAX_V (35.0f)
/** @brief 协议约定的电流量程下限，单位 A。 */
#define SUPERCAP_CAN_CURRENT_MIN_A (-25.0f)
/** @brief 协议约定的电流量程上限，单位 A。 */
#define SUPERCAP_CAN_CURRENT_MAX_A (25.0f)
/** @brief 协议约定的功率量程下限，单位 W。 */
#define SUPERCAP_CAN_POWER_MIN_W (-500.0f)
/** @brief 协议约定的功率量程上限，单位 W。 */
#define SUPERCAP_CAN_POWER_MAX_W (500.0f)

/**
 * @brief 底盘侧解码后的超电遥测。
 * @note 故障位顺序与 error_t 一致；位域访问要求目标编译器从低位向高位布局。
 */
typedef struct
{
    union
    {
        uint8_t all; /**< 报文第 0 字节，完整故障字。 */
        struct
        {
            uint8_t chassis_overcurrent : 1; /**< bit 0，底盘侧过流。 */
            uint8_t battery_overcurrent : 1; /**< bit 1，电池入口过流。 */
            uint8_t cap_overcurrent : 1; /**< bit 2，电容侧过流。 */
            uint8_t chassis_overvoltage : 1; /**< bit 3，底盘侧过压。 */
            uint8_t chassis_undervoltage : 1; /**< bit 4，底盘侧欠压。 */
            uint8_t cap_overvoltage : 1; /**< bit 5，电容侧过压。 */
            uint8_t cap_undervoltage : 1; /**< bit 6，电容侧欠压。 */
            uint8_t chassis_output_loss : 1; /**< bit 7，底盘输出许可丢失。 */
        } bits;
    } error_flags;
    float ref_voltage_v; /**< 裁判端反馈电压，单位 V。 */
    float ref_current_a; /**< 裁判端反馈电流，单位 A。 */
    float discharge_power_max_w; /**< 当前可放电功率，单位 W。 */
} SupercapCanRxData_t;

/** @brief 底盘侧最近一次解码的遥测数据。 */
extern SupercapCanRxData_t g_supercap_can_rx;

/**
 * @brief 按当前故障字、反馈及放电电流限制发送一帧遥测。
 * @pre 仅由 Timer A 的 1 ms 回调调用，FDCAN2 已启动。
 * @note 直接读取控制和反馈，不做临界区快照；未入队则舍弃本帧，不重试。
 */
void SupercapCan_Send1ms(void);

/**
 * @brief 底盘侧解码超电遥测，忽略不匹配的 ID 或空指针。
 * @param[in] rx_id 接收到的标准 CAN 帧 ID。
 * @param[in] data 指向 8 字节的遥测帧数据，不保留指针。
 * @pre 调用方已确认帧长为 8 字节；功控板不注册此回调。
 */
void SupercapCan_RxCallback(uint32_t rx_id, const uint8_t *data);

#endif /* SUPERPOWER_PROT_SUPERCAP_H */
