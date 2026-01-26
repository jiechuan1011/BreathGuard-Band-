/*
 * 糖尿病初筛监测系统 - 主程序入口
 * 
 * 功能：根据config.h中选择的硬件角色，包含对应的主程序文件
 * 设计：一份代码通过条件编译适配两种硬件角色
 * 
 * 硬件角色：
 * 1. DEVICE_ROLE_WRIST（腕带主控）：
 *    - ESP32-C3 SuperMini + MAX30102 + 0.96" OLED
 *    - 锂电池300mAh + TP4056充电 + 按键
 *    - 功能：心率血氧采集、OLED显示、BLE广播
 * 
 * 2. DEVICE_ROLE_DETECTOR（独立检测模块）：
 *    - ESP32-C3 SuperMini + SnO₂气敏传感器 + HKG-07呼吸传感器
 *    - AD623运放 + RC低通滤波 + MOSFET加热控制
 *    - 锂电池200mAh
 *    - 功能：呼出气丙酮检测、呼吸率计算
 * 
 * 项目约束：
 * - 总成本 ≤500元，腕带重量 <45g
 * - 不输出任何医疗诊断相关内容
 * - 所有输出包含免责声明
 */

#include <Arduino.h>
#include "../config/config.h"
#include "../config/pin_config.h"
#include "../config/system_config.h"

// ==================== 免责声明输出 ====================
// 所有输出必须包含的免责声明
const char DISCLAIMER[] PROGMEM = "本设备仅用于生理参数趋势监测，不用于医疗诊断，请咨询专业医生";

// ==================== 角色选择检查 ====================
// 编译时检查角色选择
#ifndef DEVICE_ROLE_WRIST
#ifndef DEVICE_ROLE_DETECTOR
#error "错误：未选择设备角色。请在config.h中取消注释DEVICE_ROLE_WRIST或DEVICE_ROLE_DETECTOR"
#endif
#endif

// ==================== 函数声明 ====================
// 角色特定的函数声明（在包含的.ino文件中定义）
#ifdef DEVICE_ROLE_WRIST
void wrist_setup();
void wrist_loop();
#elif defined(DEVICE_ROLE_DETECTOR)
void detector_setup();
void detector_loop();
#endif

// ==================== 主程序包含 ====================
// 根据选择的角色包含对应的主程序文件

#ifdef DEVICE_ROLE_WRIST
// 包含腕带主控程序
#include "../main_control/main_control.ino"

#elif defined(DEVICE_ROLE_DETECTOR)
// 包含检测模块程序
#include "../detection_sensor/detection_sensor.ino"

#else
#error "错误：未知的设备角色。请检查config.h配置"
#endif

// ==================== 通用初始化函数 ====================
// 这些函数在两个角色中都需要

void setup() {
    // 初始化串口（仅在调试模式下启用）
#ifdef DEBUG_MODE
    Serial.begin(SERIAL_BAUDRATE);
    while (!Serial && millis() < 3000) {
        ;  // 等待串口就绪（最多3秒）
    }
    
    Serial.println(F("========================================="));
    Serial.println(F("  糖尿病初筛监测系统 - 启动中..."));
    Serial.println(F("========================================="));
    
    // 输出设备信息
#ifdef DEVICE_ROLE_WRIST
    Serial.println(F("设备角色：腕带主控"));
    Serial.println(F("硬件：ESP32-C3 + MAX30102 + OLED"));
    Serial.println(F("电池：300mAh锂电池 + TP4056充电"));
#elif defined(DEVICE_ROLE_DETECTOR)
    Serial.println(F("设备角色：检测模块"));
    Serial.println(F("硬件：ESP32-C3 + SnO₂ + HKG-07 + AD623"));
    Serial.println(F("电池：200mAh锂电池"));
#endif
    
    Serial.println(F("版本：") SOFTWARE_VERSION_STRING);
    Serial.println(F("构建日期：") BUILD_DATE_STRING);
    Serial.println();
    
    // 输出项目约束
    Serial.println(F("项目约束："));
    Serial.println(F("- 总成本 ≤500元"));
#ifdef DEVICE_ROLE_WRIST
    Serial.println(F("- 腕带重量 <45g"));
#endif
    Serial.println();
    
    // 输出免责声明
    Serial.println(F("免责声明："));
    Serial.println(DISCLAIMER);
    Serial.println(F("=========================================\n"));
#endif // DEBUG_MODE
    
    // 调用角色特定的初始化函数
#ifdef DEVICE_ROLE_WRIST
    wrist_setup();
#elif defined(DEVICE_ROLE_DETECTOR)
    detector_setup();
#endif
}

void loop() {
    // 调用角色特定的主循环函数
#ifdef DEVICE_ROLE_WRIST
    wrist_loop();
#elif defined(DEVICE_ROLE_DETECTOR)
    detector_loop();
#endif
    
    // 通用看门狗喂狗（如果启用）
#ifdef WATCHDOG_ENABLED
    // 看门狗复位
    // 具体实现取决于硬件平台
#endif
}