# 2027 超级电容控制器

面向 STM32G474RET6 的 CubeMX / Keil 工程。自维护代码移植自相邻的
`2026superpower` 当前工作区，包含源工程尚未提交的 PID、控制和参数修改。

## 当前状态

| 项目 | 状态 |
| --- | --- |
| Keil 编译登记 | 保留原分组及包含路径，BSP 的 C/H 条目更新为 `bsp_analog` |
| 启动行为 | `main` 调用 `SuperPower_Init()`，先关输出，再初始化模拟外设并启动 HRTIM |
| 采样与 DAC 调度 | Master 发布 ADC 反馈，再运行控制并更新 DAC 峰谷值；Timer A 调度能量环和 Fault 恢复检查 |
| 功率输出 | 启动先关输出；无故障锁存时控制任务可使能输出 |
| 新板硬件适配 | 比较器路由和消隐已生成，B 侧谷值事件极性与实际波形仍待验证 |
| 固件编译及上板 | 未执行 |

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

旧工程的采样布局、分压和标定记录仅作来源参考，不作为本板验证证据。
本板控制调用链和已确认的边界见[架构](docs/architecture.md)与[当前交接](docs/handoff.md)。

## 静态检查

在工程根目录执行，只读检查，不生成固件产物：

```powershell
python -X utf8 -B tools/check_project.py
python -X utf8 -B tools/check_project.py --format
```

源文件既有编码和排版问题按直接复制约定保留；检查失败不自动修复。
格式检查依赖已有 `clang-format`，缺失时报告未执行，不自动安装。
