// config/pin_config.h
// 引脚配置文件 - 根据硬件角色动态配置引脚
// ==============================================
// 使用说明：
// 1. 根据config.h中选择的角色，自动启用相应的引脚定义
// 2. 使用#ifdef DEVICE_ROLE_*进行条件编译
// 3. 确保引脚不冲突，特别是中断引脚
// ==============================================

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "config.h"

// ==================== 通用引脚定义 ====================
// 这些引脚在两个角色中都可能使用

// I2C总线引脚（MAX30102、OLED、环境传感器）
#define PIN_SDA         4   // I2C数据线（ESP32-C3 GPIO4）
#define PIN_SCL         5   // I2C时钟线（ESP32-C3 GPIO5）

// 电源管理引脚
#define PIN_BAT_ADC     2   // 电池电压ADC（ESP32-C3 GPIO2）
#define PIN_CHARGE_STAT 6   // 充电状态检测（可选）

// 用户交互引脚
#define PIN_BTN1        7   // 按键1（切换/确认）
#define PIN_BTN2        8   // 按键2（返回/取消）

// ==================== 腕带主控专用引脚 ====================
#ifdef DEVICE_ROLE_WRIST

// MAX30102心率血氧传感器
#define PIN_MAX30102_INT 3   // 中断引脚（ESP32-C3 GPIO3）

// OLED显示屏（0.96" SSD1306）
#define PIN_OLED_RESET   9   // 复位引脚（可选，通常接VCC）
#define PIN_OLED_DC      10  // 数据/命令选择（I2C模式不需要）

// 运动传感器（MPU6050，可选）
#define PIN_MPU6050_INT  11  // 中断引脚（可选）

// 震动电机反馈
#define PIN_VIBRATE      12  // 震动电机控制

// BLE模块（ESP32-C3内置，无需额外引脚）

#endif // DEVICE_ROLE_WRIST

// ==================== 检测模块专用引脚 ====================
#ifdef DEVICE_ROLE_DETECTOR

// SnO₂气敏传感器电路
#define PIN_GAS_ADC     A0  // 气体传感器ADC输入（ESP32-C3 GPIO0）
#define PIN_GAS_HEATER  9   // 加热控制PWM（ESP32-C3 GPIO9）

// HKG-07呼吸传感器
#define PIN_RESP_TRIG   2   // 呼吸触发信号（ESP32-C3 GPIO2，注意与BAT_ADC冲突）
// 注意：PIN_RESP_TRIG与PIN_BAT_ADC冲突，需要根据实际硬件调整

// AD623运放电路
#define PIN_AD623_REF   A1  // 参考电压设置（ESP32-C3 GPIO1）
#define PIN_AD623_OUT   A2  // 运放输出（ESP32-C3 GPIO2，注意冲突）

// 状态指示灯
#define PIN_LED_STATUS  13  // 状态指示灯（ESP32-C3 GPIO13）

// 加热电流监测
#define PIN_HEATER_CURR A3  // 加热电流监测ADC（ESP32-C3 GPIO3）

#endif // DEVICE_ROLE_DETECTOR

// ==================== 引脚冲突检查 ====================
// 编译时检查引脚冲突

#ifdef DEVICE_ROLE_DETECTOR
// 检查检测模块引脚冲突
#if PIN_RESP_TRIG == PIN_BAT_ADC
#warning "警告：呼吸传感器触发引脚与电池ADC引脚冲突，建议调整"
#endif

#if PIN_AD623_OUT == PIN_BAT_ADC
#warning "警告：AD623输出引脚与电池ADC引脚冲突，建议调整"
#endif
#endif // DEVICE_ROLE_DETECTOR

// ==================== I2C地址定义 ====================
// 传感器I2C地址（与引脚无关）

#define I2C_ADDR_MAX30102 0x57    // MAX30102默认地址
#define I2C_ADDR_OLED     0x3C    // SSD1306 OLED默认地址
#define I2C_ADDR_MPU6050  0x68    // MPU6050默认地址
#define I2C_ADDR_AHT20    0x38    // AHT20温湿度传感器

// ==================== ADC参考电压 ====================
// ESP32-C3 ADC参考电压
#define ADC_REF_VOLTAGE   3.3     // 3.3V参考电压
#define ADC_RESOLUTION    4096    // 12位ADC分辨率

#endif // PIN_CONFIG_H
