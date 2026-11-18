/**
 * @file power_ctrl_task.h
 * @brief 旧整机初始化入口声明。
 */
#ifndef SUPERPOWER_POWER_CTRL_TASK_H
#define SUPERPOWER_POWER_CTRL_TASK_H

/**
 * @brief 初始化板卡信息、采样、通信和控制参数，启动 Master/A/B 计数器。
 * @pre CubeMX 外设初始化完成，计数器尚未启动，输出关闭。
 * @note 当前 main 不调用此入口。Master/A/B 仅启动计数，Master 回调发布采样后更新 DAC。
 */
void SuperPower_Init(void);

#endif /* SUPERPOWER_POWER_CTRL_TASK_H */
