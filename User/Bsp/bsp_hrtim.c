/**
 * @file bsp_hrtim.c
 * @brief 按端口电压切换四种功率级模式，并通过 LL 更新 HRTIM 比较值。
 * @details 模式切换和比较值取自港科 RM2025 上场版 PowerManager.cpp 的 modeStateMachine()。
 *          本板底盘侧为 Timer B，电容侧为 Timer A，按物理侧对应参考实现。
 */

#include "bsp_hrtim.h"

#include "bsp_analog.h"
#include "hrtim.h"

/* ==================== 私有定义 ==================== */

/** @brief 保存迟滞模式和本轮电压比，由 Master 回调中的状态机更新。 */
static BspHrtimState_t s_hrtim_state = {.mode = BSP_HRTIM_MODE_BUCK};

/* ==================== 公共实现 ==================== */

void BspHrtim_ModeStateMachine(void)
{
    s_hrtim_state.voltage_ratio = adc_fdb.B.V / adc_fdb.A.V;
    // clang-format off
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

    /* 按切换后的模式写入预装载值，A/B 各自在复位时装载。 */
    switch(s_hrtim_state.mode)
    {
        case BSP_HRTIM_MODE_BUCK:
        {
            LL_HRTIM_TIM_SetCompare4(HRTIM1, LL_HRTIM_TIMER_B,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_REGULATED_CMP4_FRACTION);
            LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_B,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_BUCK_CMP3_FRACTION);
            LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_A, 0U);
        }    break;

        case BSP_HRTIM_MODE_BUCKBOOST:
        {
            LL_HRTIM_TIM_SetCompare4(HRTIM1, LL_HRTIM_TIMER_B,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_REGULATED_CMP4_FRACTION);
            LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_B,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_BUCKBOOST_CMP3_FRACTION);
            LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_A,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_FIXED_CMP3_FRACTION);
            LL_HRTIM_TIM_SetCompare4(HRTIM1, LL_HRTIM_TIMER_A,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_FIXED_CMP4_FRACTION);
        }    break;

        case BSP_HRTIM_MODE_BOOSTBUCK:
        {
            LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_B,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_FIXED_CMP3_FRACTION);
            LL_HRTIM_TIM_SetCompare4(HRTIM1, LL_HRTIM_TIMER_B,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_FIXED_CMP4_FRACTION);
            LL_HRTIM_TIM_SetCompare4(HRTIM1, LL_HRTIM_TIMER_A,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_REGULATED_CMP4_FRACTION);

            if(adc_fdb.B.V < BSP_HRTIM_BOOST_VB_THRESHOLD_V)
            {
                LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_A,
                                         BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_BOOST_CMP3_FRACTION);
            }
            else
            {
                LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_A,
                                         BSP_HRTIM_PERIOD_COUNT *
                                         (1.0f - (adc_fdb.A.V * BSP_HRTIM_BOOSTBUCK_CHASSIS_DUTY) /
                                          BSP_HRTIM_BOOST_DUTY_LIMIT_V));
            }
            /* 由电感伏秒平衡 D_A * V_A = D_B * V_B 估算 B 端占空比。 */
        }    break;

        case BSP_HRTIM_MODE_BOOST:
        {
            LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_B, 0U);
            LL_HRTIM_TIM_SetCompare4(HRTIM1, LL_HRTIM_TIMER_A,
                                     BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_REGULATED_CMP4_FRACTION);

            if(adc_fdb.B.V < BSP_HRTIM_BOOST_VB_THRESHOLD_V)
            {
                LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_A,
                                         BSP_HRTIM_PERIOD_COUNT * BSP_HRTIM_BOOST_CMP3_FRACTION);
            }
            else
            {
                LL_HRTIM_TIM_SetCompare3(HRTIM1, LL_HRTIM_TIMER_A,
                                         BSP_HRTIM_PERIOD_COUNT *
                                         (1.0f - adc_fdb.A.V / BSP_HRTIM_BOOST_DUTY_LIMIT_V));
            }
        }    break;

        default:
        {
        }    break;
    }
}

