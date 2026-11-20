/**
 * @file bsp_hrtim.c
 * @brief 按端口电压切换四种功率级模式，并通过 LL 更新 HRTIM 比较值。
 * @details 模式迟滞沿用港科 RM2025 上场版。模式按物理半桥分配谷值调流、峰值调流、
 *          固定占空比或自举刷新。按名义上管占空比确定 CMP3，CMP4 是比较器消隐结束边沿。
 */

#include "bsp_hrtim.h"

#include "bsp_analog.h"

/* ==================== 私有定义 ==================== */

/** @brief 保存模式迟滞历史及本轮电压比，由 Master 回调更新。 */
BspHrtimState_t s_hrtim_state = {.mode = BSP_HRTIM_MODE_BUCK};

/* ==================== 私有声明 ==================== */

static inline void BspHrtim_WriteBridgeTiming(uint32_t timer, uint32_t cmp3_count,
                                            uint32_t blank_end_count);

/* ==================== 公共实现 ==================== */

void BspHrtim_ModeStateMachine(void)
{
    s_hrtim_state.voltage_ratio = adc_fdb.B.V / adc_fdb.A.V;

    switch(s_hrtim_state.mode)
    {
        case BSP_HRTIM_MODE_BUCK:
        {
            if(s_hrtim_state.voltage_ratio > BSP_HRTIM_BUCK_TO_BUCKBOOST_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BUCKBOOST;
        }    break;

        case BSP_HRTIM_MODE_BUCKBOOST:
        {
            if(s_hrtim_state.voltage_ratio < BSP_HRTIM_BUCKBOOST_TO_BUCK_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BUCK;
            else if(s_hrtim_state.voltage_ratio > BSP_HRTIM_BUCKBOOST_TO_BOOSTBUCK_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BOOSTBUCK;
        }    break;

        case BSP_HRTIM_MODE_BOOSTBUCK:
        {
            if(s_hrtim_state.voltage_ratio < BSP_HRTIM_BOOST_MODES_TO_BUCK_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BUCK;
            else if(s_hrtim_state.voltage_ratio < BSP_HRTIM_BOOSTBUCK_TO_BUCKBOOST_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BUCKBOOST;
            else if(s_hrtim_state.voltage_ratio > BSP_HRTIM_BOOSTBUCK_TO_BOOST_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BOOST;
        }    break;

        case BSP_HRTIM_MODE_BOOST:
        {
            if(s_hrtim_state.voltage_ratio < BSP_HRTIM_BOOST_MODES_TO_BUCK_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BUCK;
            else if(s_hrtim_state.voltage_ratio < BSP_HRTIM_BOOST_TO_BOOSTBUCK_RATIO)
                s_hrtim_state.mode = BSP_HRTIM_MODE_BOOSTBUCK;
        }    break;

        default:
        {
        }    break;
    }

    /* 避免本地复位跨过比较值写入过程而装载半组 CMP3/CMP4。 */
    LL_HRTIM_SuspendUpdate(HRTIM1, BSP_HRTIM_CHASSIS_TIMER | BSP_HRTIM_CAPACITOR_TIMER);
    switch(s_hrtim_state.mode)
    {
        case BSP_HRTIM_MODE_BUCK:
        {
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CHASSIS_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_BUCK_MIN_HIGH_PERCENT),
                BSP_HRTIM_REGULATED_BLANK_END_COUNT);
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CAPACITOR_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_REFRESH_HIGH_PERCENT),
                BSP_HRTIM_FULL_BLANK_END_COUNT);
        }    break;

        case BSP_HRTIM_MODE_BUCKBOOST:
        {
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CHASSIS_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_BUCKBOOST_MIN_HIGH_PERCENT),
                BSP_HRTIM_REGULATED_BLANK_END_COUNT);
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CAPACITOR_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_FIXED_HIGH_PERCENT),
                BSP_HRTIM_FULL_BLANK_END_COUNT);
        }    break;

        case BSP_HRTIM_MODE_BOOSTBUCK:
        {
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CHASSIS_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_FIXED_HIGH_PERCENT),
                BSP_HRTIM_FULL_BLANK_END_COUNT);
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CAPACITOR_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_BOOST_MIN_HIGH_PERCENT),
                BSP_HRTIM_REGULATED_BLANK_END_COUNT);
        }    break;

        case BSP_HRTIM_MODE_BOOST:
        {
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CHASSIS_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_REFRESH_HIGH_PERCENT),
                BSP_HRTIM_FULL_BLANK_END_COUNT);
            BspHrtim_WriteBridgeTiming(
                BSP_HRTIM_CAPACITOR_TIMER,
                BSP_HRTIM_CMP3_FROM_HIGH_PERCENT(BSP_HRTIM_BOOST_MIN_HIGH_PERCENT),
                BSP_HRTIM_REGULATED_BLANK_END_COUNT);
        }    break;

        default:
        {
        }    break;
    }

    LL_HRTIM_ResumeUpdate(HRTIM1, BSP_HRTIM_CHASSIS_TIMER | BSP_HRTIM_CAPACITOR_TIMER);
}

void BspHrtim_SetOpenLoopPwm(void)
{
    LL_HRTIM_TIM_SetCompare1(HRTIM1, BSP_HRTIM_CAPACITOR_TIMER, BSP_HRTIM_OPEN_LOOP_SET_COUNT);
    BspHrtim_WriteBridgeTiming(BSP_HRTIM_CAPACITOR_TIMER, BSP_HRTIM_OPEN_LOOP_RESET_COUNT,
                               BSP_HRTIM_FULL_BLANK_END_COUNT);

    LL_HRTIM_TIM_SetCompare1(HRTIM1, BSP_HRTIM_CHASSIS_TIMER, BSP_HRTIM_OPEN_LOOP_SET_COUNT);
    BspHrtim_WriteBridgeTiming(BSP_HRTIM_CHASSIS_TIMER, BSP_HRTIM_OPEN_LOOP_RESET_COUNT,
                               BSP_HRTIM_FULL_BLANK_END_COUNT);
}

/* ==================== 私有实现 ==================== */

/**
 * @brief 成对写入一座半桥的预装载值。
 * @param[in] timer 本板底盘桥或电容桥的 HRTIM 定时器。
 * @param[in] cmp3_count 由名义上管占空比换算得到的 CMP3，单位为本地周期计数。
 * @param[in] blank_end_count 比较器消隐结束位置，单位为本地周期计数。
 * @pre 两桥的预装载传输已暂停，或功率输出保持关闭直至新比较值装载。
 */
static inline void BspHrtim_WriteBridgeTiming(uint32_t timer, uint32_t cmp3_count,
                                            uint32_t blank_end_count)
{
    LL_HRTIM_TIM_SetCompare3(HRTIM1, timer, cmp3_count);
    LL_HRTIM_TIM_SetCompare4(HRTIM1, timer, blank_end_count);
}

