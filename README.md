# 2027 超级电容控制器

面向 STM32G474RET6 的 CubeMX / Keil 工程。自维护代码移植自相邻的
`2026superpower` 当前工作区，包含源工程尚未提交的 PID、控制和参数修改。

## 当前状态

| 项目 | 状态 |
| --- | --- |
| 规范、User、模板及检查工具 | 已复制；原样保留代码、参数、行尾和编码 |
| Keil 编译登记 | 本轮登记七个 User 分组、17 个 C、18 个 H 及七个包含目录 |
| 启动行为 | 外设初始化后仅关闭四路功率输出；不启动 HRTIM 计数器 |
| 采样、通信及控制调度 | 未启动，未调用 `SuperPower_Init()` |
| 新板硬件适配 | 待完成 CubeMX、采样定义、换算、校准及参数核对 |
| 固件编译及上板 | 未执行 |

开／闭环编译选择位于 `Core/Inc/main.h` 的 USER CODE 区，默认闭环；定义该宏不代表
控制链已经启动。User 中保留的初始化函数和 HAL 回调是来源实现，不能据此认定新板功能可用。

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
