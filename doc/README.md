# 工程资料与代码参考

## 本板硬件资料

- [Super-capacitor control_V1.3.pdf](<Super-capacitor control_V1.3.pdf>)：本工程已有原理图。
- [硬件与 CubeMX 待办](../docs/hardware.md)：本板配置、采样与保护路径的核对入口。

## RM2025 功控板代码参考

后续代码内容和实现思路参考香港科技大学 ENTERPRIZE 战队的 RM2025 功控板工程。

- [原文章：RM2025-超级电容与无线充电系统开源](https://bbs.robomaster.com/article/761385?source=4)。
- [Git 仓库：RM2025-PowerControlBoard-WirelessCharging](https://github.com/hkustenterprize/RM2025-PowerControlBoard-WirelessCharging)。
- [功控板上场版代码：PowerControlBoard_Software_FieldVersion](https://github.com/hkustenterprize/RM2025-PowerControlBoard-WirelessCharging/tree/main/PowerControlBoard_Software_FieldVersion)。

用户提供的本地解压目录如下，后续源码查阅以此目录中的工程文件为参考：

```text
E:\RoboMaster\code\hardware\git(DESKTOP-ELF7FKO)\港科开源\RM2025\RM2025-PowerControlBoard-WirelessCharging-main\PowerControlBoard_Software_FieldVersion
```

目录已确认包含 `Core/Src/PowerManager.cpp`、`Core/Src/UserTask.cpp`、
`Core/Inc/Config.hpp` 及配套工程文件；本地解压内容与远端具体提交的一致性未核验。

## 使用约定

- 参考该工程的代码内容与实现思路，命名、排版、中文 Doxygen、变量使用和函数约束继续遵守
  本工程原有[代码规范](../docs/coding_style.md)，不沿用参考工程的代码风格或 C++ 工程组织方式。
- 硬件参数、采样换算、保护阈值、外设配置和执行时序须结合本板确认；参考工程的实现及验证
  记录不能作为本工程已实现、已编译或已上板验证的证据。
- 本次仅登记参考资料，不复制源码、不导入构建、不接入运行；后续移植仍按
  [协作流程](../docs/ai_workflow.md)逐文件审批，参考目录保持只读。
