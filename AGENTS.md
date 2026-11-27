# 2027 超级电容控制器 AI 协作入口

**本工程统一遵循全局 Skill `stm32-embedded-dev`**（编码规范、变量极简原则、单出口、`.h`/`.c` 组织、CubeMX `USER CODE` 边界、静态核对、Keil 编译闭环及 Git 只读约定均以该技能为准）。
以下仅记录本工程特有的结构、高频控制约束与文档入口：

## 本工程特有边界

- **芯片与工程**：STM32G474RET6，Keil 工程为 `MDK-ARM/2027superpower.uvprojx`，CubeMX 文件为 `2027superpower.ioc`。
- **自维护目录**：`User/`（`Algorithm/`、`App/`、`Bsp/`、`Calibration/`、`Config/`、`Debug/`、`Procotol/`）。
- **高频控制与 HAL/LL 分工（本项目专属）**：
  - 超电控制环运行频率高：初始化沿用 HAL，自维护运行期高频外设操作统一优先使用现成 LL 接口（无对应接口时先说明缺口再定方案），不重复造轮子。
  - 硬件以 [Super-capacitor control_V1.3.pdf](<doc/Super-capacitor control_V1.3.pdf>) 为准，外部参考工程与技术报告见 [doc/README.md](doc/README.md)，参数与引脚须按本板核对。
- **命名与接口例外**：
  - 沿用 `UID.c/.h`、`UID_t`、`UID_*()` 及 `BoardInfo_t g_board_info`（定义于 `boardinfo.c`，启动初始化后运行期只读）。
  - 沿用已约定的 ADC 通道名：`V_CHASSIS`、`V_BAT`、`I_BAT`、`I_CHASSIS`、`V_CAP`（总数 `ADC_CHANNEL_COUNT`）及混合物理量数组 `adc_convert`。
- **项目文档入口**：
  - 开始工作前优先阅读 [当前交接](docs/handoff.md)、[工程概览](README.md) 与 [工程架构](docs/architecture.md)。
  - 涉及硬件/决策/移植验收记录时查阅 [硬件待办](docs/hardware.md)、[长期决策](docs/decisions.md)、[验证记录](docs/testing.md)。
