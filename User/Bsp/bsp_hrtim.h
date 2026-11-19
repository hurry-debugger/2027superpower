/**
 * @file bsp_hrtim.h
 * @brief 双半桥模式切换、PWM 比较值更新及功率输出掩码。
 * @details Timer A 接电容侧，Timer B 接底盘侧；CH1 接下管，CH2 接上管。
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

/* ==================== 调制参数与模式 ==================== */

/** @brief A/B 周期计数，取自本板 CubeMX 配置，与港科 RM2025 上场版一致。 */
#define BSP_HRTIM_PERIOD_COUNT (21760U)

/** @brief 港科上场版模式迟滞门槛，均为无量纲的电容侧与底盘侧电压比。 */
#define BSP_HRTIM_BUCK_TO_BUCKBOOST_RATIO (0.84f)
#define BSP_HRTIM_BUCKBOOST_TO_BUCK_RATIO (0.80f)
#define BSP_HRTIM_BUCKBOOST_TO_BOOSTBUCK_RATIO (1.02f)
#define BSP_HRTIM_BOOST_MODES_TO_BUCK_RATIO (0.82f)
#define BSP_HRTIM_BOOSTBUCK_TO_BUCKBOOST_RATIO (0.98f)
#define BSP_HRTIM_BOOSTBUCK_TO_BOOST_RATIO (1.25f)
#define BSP_HRTIM_BOOST_TO_BOOSTBUCK_RATIO (1.19f)

/** @brief 港科上场版 CMP3/CMP4 相对周期的无量纲系数。 */
#define BSP_HRTIM_REGULATED_CMP4_FRACTION (0.06f)
#define BSP_HRTIM_BUCK_CMP3_FRACTION (0.998f)
#define BSP_HRTIM_BUCKBOOST_CMP3_FRACTION (0.95f)
#define BSP_HRTIM_FIXED_CMP3_FRACTION (0.16f)
#define BSP_HRTIM_FIXED_CMP4_FRACTION (0.24f)
#define BSP_HRTIM_BOOST_CMP3_FRACTION (0.75f)

/** @brief 港科上场版的升压 CMP3 调整门槛及计算基准，单位 V。 */
#define BSP_HRTIM_BOOST_VB_THRESHOLD_V (28.8f * 1.01f)
#define BSP_HRTIM_BOOST_DUTY_LIMIT_V (29.8f)
/** @brief BoostBuck 的底盘侧固定上管占空比，无量纲，取自港科上场版。 */
#define BSP_HRTIM_BOOSTBUCK_CHASSIS_DUTY (0.84f)

typedef enum
{
    BSP_HRTIM_MODE_BUCK,
    BSP_HRTIM_MODE_BUCKBOOST,
    BSP_HRTIM_MODE_BOOSTBUCK,
    BSP_HRTIM_MODE_BOOST
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
 * @brief 计算电压比、切换功率级模式并更新 A/B 的 CMP3 和 CMP4 预装载值。
 * @return 本轮模式的 B 端（电容侧，Timer A）上管占空比估计值，无量纲；模式无效时为零。
 * @pre 仅在 HRTIM1 Master 回调发布本批 ADC 反馈后调用。运行期由此函数独占比较值写入。
 * @pre A/B 电压反馈有效，且 A 端电压大于零。
 * @pre CubeMX 为 A/B 设置 21760 计数周期并打开预装载及复位更新。
 * @note A/B 各自在下次复位装载比较值。功率输出的启停由调用方直接使用 LL 接口管理。
 */
void BspHrtim_ModeStateMachine(void);

#ifdef __cplusplus
}
#endif

#endif /* SUPERPOWER_BSP_HRTIM_H */
