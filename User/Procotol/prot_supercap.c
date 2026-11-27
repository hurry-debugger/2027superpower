/**
 * @file prot_supercap.c
 * @brief 旧 SuperCap2026_7_17 线上格式与 Taurus BSP 的对接。
 * @note 读取采样与 App 控制状态并取得一致快照；不导入旧控制参数或 FD 报文。
 */
#include "prot_supercap.h"

#include "bsp_can.h"
#include "bsp_analog.h"
#include "power_ctrl.h"

#include <float.h>
#include <string.h>
