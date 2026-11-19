/**
 * @file power_ctrl_task.h
 * @brief 超电初始化入口声明。
 */
#ifndef SUPERPOWER_POWER_CTRL_TASK_H
#define SUPERPOWER_POWER_CTRL_TASK_H

/**
 * @brief 关闭功率输出，初始化板卡、DWT、模拟链路、控制环及裁判接收，再启动 HRTIM。
 * @pre CubeMX 外设初始化完成，SysTick 正常运行，Master/A/B 计数器尚未启动。
 * @note 仅启动阶段调用一次，功率输出保持关闭。HAL 调用失败进入 Error_Handler()。
 */
void SuperPower_Init(void);

#endif /* SUPERPOWER_POWER_CTRL_TASK_H */
