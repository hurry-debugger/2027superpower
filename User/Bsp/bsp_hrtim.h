/**
 * @file bsp_hrtim.h
 * @brief 双半桥模式切换、PWM 比较值更新及功率输出掩码。
 * @details Master 事件复位两桥计数器，本桥 CMP1 置位 CH1 开下管，
 *          本桥 CMP3 或对侧电流事件复位 CH1。
 *          CH2 由硬件死区单元生成上管信号，CMP4 决定本桥比较器消隐结束时刻。
 */

#ifndef SUPERPOWER_BSP_HRTIM_H
#define SUPERPOWER_BSP_HRTIM_H

#include "stm32g4xx_ll_hrtim.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ==================== 功率输出 ==================== */

/** @brief 本板四路功率输出，来源为当前 HRTIM1 引脚配置及原理图第 3 页。 */
#define BSP_HRTIM_POWER_OUTPUT_MASK \
    (LL_HRTIM_OUTPUT_TA1 | LL_HRTIM_OUTPUT_TA2 | LL_HRTIM_OUTPUT_TB1 | LL_HRTIM_OUTPUT_TB2)

/** @brief V1.3 底盘桥为 Timer B，TB1 接下管，TB2 接上管。 */
#define BSP_HRTIM_CHASSIS_TIMER (LL_HRTIM_TIMER_B)
/** @brief V1.3 电容桥为 Timer A，TA1 接下管，TA2 接上管。 */
#define BSP_HRTIM_CAPACITOR_TIMER (LL_HRTIM_TIMER_A)

/* ==================== 调制参数与模式 ==================== */

/** @brief A/B 周期计数，取自本板 CubeMX 配置，与港科 RM2025 上场版一致。 */
#define BSP_HRTIM_PERIOD_COUNT (21760U)

/** @brief 两桥 CH1 的本地 CMP1 置位位置，单位为 HRTIM 计数，取自本板 CubeMX。 */
#define BSP_HRTIM_PWM_SET_COUNT (100U)
/** @brief 开环沿用同一个 CH1 置位位置，单位为 HRTIM 计数。 */
#define BSP_HRTIM_OPEN_LOOP_SET_COUNT (BSP_HRTIM_PWM_SET_COUNT)
/** @brief 开环 CH1 复位位置，单位为 HRTIM 计数，在置位后保留半周期名义脉宽。 */
#define BSP_HRTIM_OPEN_LOOP_RESET_COUNT \
    (BSP_HRTIM_OPEN_LOOP_SET_COUNT + BSP_HRTIM_PERIOD_COUNT / 2U)

/** @brief 港科上场版模式迟滞门槛，均为无量纲的电容侧与底盘侧电压比。 */
#define BSP_HRTIM_BUCK_TO_BUCKBOOST_RATIO (0.84f)
#define BSP_HRTIM_BUCKBOOST_TO_BUCK_RATIO (0.80f)
#define BSP_HRTIM_BUCKBOOST_TO_BOOSTBUCK_RATIO (1.02f)
#define BSP_HRTIM_BOOST_MODES_TO_BUCK_RATIO (0.82f)
#define BSP_HRTIM_BOOSTBUCK_TO_BUCKBOOST_RATIO (0.98f)
#define BSP_HRTIM_BOOSTBUCK_TO_BOOST_RATIO (1.25f)
#define BSP_HRTIM_BOOST_TO_BOOSTBUCK_RATIO (1.19f)

/**
 * @brief 调流桥仅由 CMP3 兜底时的最小名义上管占空比，单位 %，按死区插入前计。
 * @note Buck 的 5% 由用户设定，独立于 BuckBoost；调整时须核对死区和驱动最小脉宽。
 */
#define BSP_HRTIM_BUCK_MIN_HIGH_PERCENT (5U)
#define BSP_HRTIM_BUCKBOOST_MIN_HIGH_PERCENT (5U) /**< 港科技术报告 V1.1 第 50 页。 */
#define BSP_HRTIM_BOOST_MIN_HIGH_PERCENT (55U)   /**< 报告第 50 页，供 BoostBuck 和 Boost 共用。 */

/** @brief 固定桥和自举刷新桥的名义上管占空比，单位 %。 */
#define BSP_HRTIM_FIXED_HIGH_PERCENT (84U)   /**< 港科上场版的固定占空比。 */
#define BSP_HRTIM_REFRESH_HIGH_PERCENT (95U) /**< 用户给定的旁路占空比。 */

/**
 * @brief 将死区前的名义上管占空比（整数 %）换算为本地 CMP3，单位为 HRTIM 计数。
 * @details 名义上管窗口从 CMP3 到下一周期 CMP1。因此在 CMP1 上加一个周期的
 *          上管占空比补值，并将该补值截断为整数计数。
 */
#define BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(high_percent) \
    (BSP_HRTIM_PWM_SET_COUNT + BSP_HRTIM_PERIOD_COUNT * (100U - (high_percent)) / 100U)

/** @brief 调流比较器的 CMP4，单位为 HRTIM 计数，取自本板 CubeMX 的约 6% 周期。 */
#define BSP_HRTIM_REGULATED_BLANK_END_COUNT (1306U)
/** @brief 固定占空比、自举刷新及开环的全周期比较器消隐值，单位为 HRTIM 计数。 */
#define BSP_HRTIM_FULL_BLANK_END_COUNT (BSP_HRTIM_PERIOD_COUNT)

typedef enum
{
    BSP_HRTIM_MODE_BUCK,      /**< 底盘桥谷值调流，电容桥自举刷新。 */
    BSP_HRTIM_MODE_BUCKBOOST, /**< 底盘桥谷值调流，电容桥固定占空比。 */
    BSP_HRTIM_MODE_BOOSTBUCK, /**< 底盘桥固定占空比，电容桥峰值调流。 */
    BSP_HRTIM_MODE_BOOST      /**< 底盘桥自举刷新，电容桥峰值调流。 */
} BspHrtimMode_e;

/** @brief Master 调度中使用的调制状态。 */
typedef struct
{
    BspHrtimMode_e mode; /**< 当前调制模式，跨周期保留以实现迟滞。 */
    float voltage_ratio; /**< 本轮电容侧与底盘侧电压比 V_B/V_A，无量纲。 */
} BspHrtimState_t;

/* ==================== 应用层环路限幅 ==================== */

/**
 * @brief 广义占空比限幅下限，无单位。
 * @note 来源：港科 RM2024 配套 PowerManager.cpp 的 updatePWM()，本板范围待验证。
 */
#define BSP_HRTIM_GENERALIZED_DUTY_MIN (0.05f)

/** @brief 同源广义占空比限幅上限，无单位；不代表硬件额定范围。 */
#define BSP_HRTIM_GENERALIZED_DUTY_MAX (10.0f)

/** @brief 同源环路占空比限幅上限，无单位；不代表硬件额定范围。 */
#define BSP_HRTIM_LOOP_DUTY_MAX (0.2f)

/* ==================== 公共接口 ==================== */
/**
 * @brief 按电容侧与底盘侧电压比切换模式，并更新两桥的 CMP3、CMP4 预装载值。
 * @pre 仅在 HRTIM1 Master 回调发布本批有效 ADC 反馈后调用，底盘侧电压大于零。
 * @pre Timer B 用 Master Period 复位，Timer A 用 Master CMP2 复位，CH1 由本桥 CMP1 置位。
 * @pre 两桥周期和 CMP1 分别为 21760 和 100 计数，独立门控、预装载和复位更新已启用。
 * @pre TB1 由本桥 CMP3 或 EEV8 复位，TA1 由本桥 CMP3 或 EEV6 复位；
 *      EEV8 和 EEV6 分别由接收定时器的 CMP4 消隐。
 * @note 本函数独占闭环运行期比较值和预装载更新门控，两桥在各自复位边界装载。
 *       功率输出启停由调用方直接使用 LL 接口管理。
 */
void BspHrtim_ModeStateMachine(void);

/**
 * @brief 预装载 A/B 相差 180 度、死区前占空比为 50% 的开环 PWM 比较值。
 * @details 两桥由 Master 错开半周期复位，本地 CMP1 置位、CMP3 复位，CMP4 全周期消隐 EEV6/8。
 * @pre HRTIM1 已按本板配置初始化，四路功率输出关闭，开环独占两桥比较值写入。
 * @note 启动前调用时沿用启动流程的软件更新装载。计数器运行时须保持输出关闭，
 *       直到写入完成后两桥各经历一次本地复位。功率输出使能由调用方管理。
 */
void BspHrtim_SetOpenLoopPwm(void);

#ifdef __cplusplus
}
#endif

#endif /* SUPERPOWER_BSP_HRTIM_H */
