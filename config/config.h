// config/config.h
// 硬件角色配置文件 - 一份代码烧录两个不同板子
// ==============================================
// 使用说明：
// 1. 选择设备角色：注释/取消注释下面的 #define 行
// 2. 只能选择一种角色，不能同时定义两个角色
// 3. 编译前务必确认选择了正确的角色
// 4. 如需调试输出，可启用 DEBUG_MODE
//
// 硬件角色说明：
// - DEVICE_ROLE_WRIST（腕带主控）：
//   * 主要功能：MAX30102 心率/血氧采集、OLED 显示、运动干扰校正、BLE 广播
//   * 适用硬件：ESP32-C3 SuperMini（最终产品），Arduino Uno（调试）
//   * 传感器：MAX30102（I2C）、MPU6050（可选，运动校正）
//   * 输出：OLED 显示、BLE 广播数据
//
// - DEVICE_ROLE_DETECTOR（独立检测模块）：
//   * 主要功能：SnO₂ 呼出气丙酮检测（含加热控制）、HKG-07 呼吸传感器、AD623+RC 滤波信号采集
//   * 适用硬件：ESP32-C3 SuperMini（最终产品），Arduino Uno（调试）
//   * 传感器：SnO₂气敏传感器（模拟+加热MOSFET）、HKG-07（数字触发）、AD623运放
//   * 输出：模拟信号采集、加热控制、呼吸检测
//
// 项目约束：
// - 总成本 ≤500元，腕带重量 <45g
// - 不输出任何医疗诊断相关内容
// ==============================================

#ifndef CONFIG_H
#define CONFIG_H

#include "version.h"

// ==================== 硬件角色选择 ====================
// 重要：通过 platformio.ini 的 build_flags 定义角色，不允许源代码手动注释切换
// 例如：build_flags = -DDEVICE_ROLE_WRIST 或 -DDEVICE_ROLE_DETECTOR

// ==================== 调试模式开关 ====================
// 通过 build_flags 定义：-DDEBUG_MODE=1
// 生产版本建议关闭

// ==================== 硬件平台配置 ====================
// 通过 build_flags 定义：-DMCU_ESP32_C3, -DMCU_ESP32_S3 或 -DMCU_ARDUINO_UNO

// ==================== 角色相关配置 ====================

#ifdef DEVICE_ROLE_WRIST
// 腕带主控专用配置

// 传感器启用标志
#ifndef USE_MAX30102
#define USE_MAX30102           // MAX30102 心率血氧传感器
#endif
#ifndef USE_OLED_DISPLAY
#define USE_OLED_DISPLAY       // OLED 显示屏
#endif
#ifndef USE_BLE_MODULE
#define USE_BLE_MODULE         // BLE 蓝牙模块
#endif
#ifndef USE_MOTION_CORRECTION
#define USE_MOTION_CORRECTION  // 运动干扰校正（需要MPU6050）
#endif

// 腕带重量限制（单位：克）
#define WRISTBAND_MAX_WEIGHT 45

// 成本限制（单位：元）
#define WRISTBAND_MAX_COST 300  // 腕带部分成本

#endif // DEVICE_ROLE_WRIST

#ifdef DEVICE_ROLE_DETECTOR
// 独立检测模块专用配置

// 传感器启用标志
#ifndef USE_SNO2_SENSOR
#define USE_SNO2_SENSOR        // SnO₂ 气敏传感器
#endif
#ifndef USE_HKG07_SENSOR
#define USE_HKG07_SENSOR       // HKG-07 呼吸传感器
#endif
#ifndef USE_AD623_AMP
#define USE_AD623_AMP          // AD623 运放电路
#endif
#ifndef USE_HEATING_CONTROL
#define USE_HEATING_CONTROL    // 加热控制（MOSFET）
#endif

// 检测模块成本限制（单位：元）
#define DETECTOR_MAX_COST 200  // 检测模块部分成本

// 加热控制参数
#define GAS_HEATER_PWM_PIN 9   // 加热控制PWM引脚
#define GAS_WARMUP_TIME_MS 60000  // 预热时间60秒
#define GAS_HEATING_TEMP 350   // 目标加热温度（℃）

#endif // DEVICE_ROLE_DETECTOR

// ==================== 全局成本控制 ====================
// 总成本限制（腕带 + 检测模块 ≤ 500元）
#define TOTAL_MAX_COST 500

// ==================== 免责声明配置 ====================
// 所有输出必须包含的免责声明
#define DISCLAIMER_STRING "本设备仅用于生理参数趋势监测，不用于医疗诊断，请咨询专业医生"

// ==================== 调试输出配置 ====================
#ifdef DEBUG_MODE
// 调试输出宏定义（带免责声明前缀）
#define DEBUG_PRINT(x) do { Serial.print("[DEBUG] "); Serial.print(x); } while(0)
#define DEBUG_PRINTLN(x) do { Serial.print("[DEBUG] "); Serial.println(x); } while(0)
#define DEBUG_PRINTF(...) do { Serial.print("[DEBUG] "); Serial.printf(__VA_ARGS__); } while(0)

// 信息输出（始终包含免责声明）
#define INFO_PRINT(x) do { Serial.print("[INFO] "); Serial.println(x); } while(0)
#define INFO_PRINTF(...) do { Serial.print("[INFO] "); Serial.printf(__VA_ARGS__); Serial.println(); } while(0)

// 错误输出（始终包含免责声明）
#define ERROR_PRINT(x) do { Serial.print("[ERROR] "); Serial.println(x); Serial.println(DISCLAIMER_STRING); } while(0)
#define ERROR_PRINTF(...) do { Serial.print("[ERROR] "); Serial.printf(__VA_ARGS__); Serial.println(); Serial.println(DISCLAIMER_STRING); } while(0)

#else
// 生产版本：调试输出为空宏
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#define INFO_PRINT(x)
#define INFO_PRINTF(...)
#define ERROR_PRINT(x)
#define ERROR_PRINTF(...)
#endif // DEBUG_MODE

// ==================== 硬件兼容性检查 ====================
// 确保选择了硬件平台（通过 build_flags 定义）
#if !defined(MCU_ARDUINO_UNO) && !defined(MCU_ESP32_C3) && !defined(MCU_ESP32_S3)
#error "请选择硬件平台：在 platformio.ini 的 build_flags 中添加 -DMCU_ARDUINO_UNO、-DMCU_ESP32_C3 或 -DMCU_ESP32_S3"
#endif

// 确保没有同时选择两个硬件平台
#if (defined(MCU_ARDUINO_UNO) && defined(MCU_ESP32_C3)) || \
    (defined(MCU_ARDUINO_UNO) && defined(MCU_ESP32_S3)) || \
    (defined(MCU_ESP32_C3) && defined(MCU_ESP32_S3))
#error "不能同时选择两个硬件平台，请只在 platformio.ini 中选择一个平台定义"
#endif

// ==================== 角色选择检查 ====================
// 确保至少选择了一个角色（通过 build_flags 定义）
#if !defined(DEVICE_ROLE_WRIST) && !defined(DEVICE_ROLE_DETECTOR)
#error "请选择设备角色：在 platformio.ini 的 build_flags 中添加 -DDEVICE_ROLE_WRIST 或 -DDEVICE_ROLE_DETECTOR"
#endif

// 确保没有同时选择两个角色
#if defined(DEVICE_ROLE_WRIST) && defined(DEVICE_ROLE_DETECTOR)
#error "不能同时选择两个设备角色，请只在 platformio.ini 中选择一个角色定义"
#endif

// ==================== 成本与重量约束检查 ====================
// 编译时成本检查
#ifdef DEVICE_ROLE_WRIST
#if WRISTBAND_MAX_COST > 300
#error "腕带成本超过300元限制，请调整设计"
#endif
#if WRISTBAND_MAX_WEIGHT > 45
#error "腕带重量超过45g限制，请调整设计"
#endif
#endif

#ifdef DEVICE_ROLE_DETECTOR
#if DETECTOR_MAX_COST > 200
#error "检测模块成本超过200元限制，请调整设计"
#endif
#endif

#if TOTAL_MAX_COST > 500
#error "总成本超过500元限制，请调整设计"
#endif

// ==================== 功能兼容性检查 ====================
#ifdef DEVICE_ROLE_WRIST
// 腕带主控必须启用OLED和BLE
#ifndef USE_OLED_DISPLAY
#define USE_OLED_DISPLAY
#endif
#ifndef USE_BLE_MODULE
#define USE_BLE_MODULE
#endif
#endif // DEVICE_ROLE_WRIST

#ifdef DEVICE_ROLE_DETECTOR
// 检测模块必须启用气体传感器
#ifndef USE_SNO2_SENSOR
#define USE_SNO2_SENSOR
#endif
#ifndef USE_AD623_AMP
#define USE_AD623_AMP
#endif
#endif // DEVICE_ROLE_DETECTOR

#endif // CONFIG_H#define DEVICE_ROLE_WRIST
//#define DEVICE_ROLE_WRIST
