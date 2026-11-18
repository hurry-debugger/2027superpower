# 2027 超级电容控制器

面向 STM32G474RET6 的 CubeMX / Keil 工程。自维护代码移植自相邻的
`2026superpower` 当前工作区，包含源工程尚未提交的 PID、控制和参数修改。

## 当前状态

| 项目 | 状态 |
| --- | --- |
| Keil 编译登记 | 保留原分组及包含路径，BSP 的 C/H 条目更新为 `bsp_analog` |
| 启动行为 | `main` 关闭四路功率输出后初始化模拟外设并启动 HRTIM Master/A/B 计数器 |
| 采样与 DAC 调度 | Master 先发布 ADC 反馈，再按 A/B 电流更新 DAC1 CH2/CH1 复位码 |
| 旧整机控制与通信 | 未启动，未调用 `SuperPower_Init()` |
| 新板硬件适配 | 保留 CubeMX 配置；模拟波形、实际采样窗口、校准及保护参数待验证 |
| 固件编译及上板 | 未执行 |

开／闭环编译选择位于 `Core/Inc/main.h` 的 USER CODE 区，默认闭环；定义该宏不代表
旧整机控制链已经启动。当前 DAC 使用实测电流直通，不构成峰值电流闭环。

## 工程入口

- [硬件与 CubeMX 待办](docs/hardware.md)：按原理图核对尚未配置或有误的项目。
- [当前交接](docs/handoff.md)：本轮实施、检查结果及下一步入口。
- [架构与运行边界](docs/architecture.md)：区分编译登记和运行接入。
- [代码规范](docs/coding_style.md)、[协作入口](AGENTS.md)、[协作流程](docs/ai_workflow.md)。
- [验证方法与结果](docs/testing.md)、[长期决定](docs/decisions.md)。
- [User 来源说明](User/README.md)：开头为 27 年状态提示，其余保留 26 年原说明。

新板原理图保留在 [doc/Super-capacitor control_V1.3.pdf](<doc/Super-capacitor control_V1.3.pdf>)。
`doc/` 保存已有硬件资料，`docs/` 保存工程文档，不移动或覆盖原理图。

## 本轮边界

CubeMX 由用户配置。本轮不修改 `.ioc`、生成初始化代码、厂商驱动、启动/RTE 或 `.uvoptx`。
Core 只在已有 USER CODE 区域内部接入模式选择、关闭输出和 USART2 BSP 入口。
不自动暂存、提交、推送，不创建分支或工作树。

旧代码的五通道布局、电压分压 14/11/11、板卡 UID、标定记录、PID 和保护阈值均保留为
来源基线。新图纸三路电压分压均为 16 倍，A/B 高侧电流采样与旧反馈含义也有差异；
这些问题解决前不接入旧采样／控制启动流程。

## 静态检查

在工程根目录执行，只读检查，不生成固件产物：

```powershell
python -X utf8 -B tools/check_project.py
python -X utf8 -B tools/check_project.py --format
```

源文件既有编码和排版问题按直接复制约定保留；检查失败不自动修复。
格式检查依赖已有 `clang-format`，缺失时报告未执行，不自动安装。
