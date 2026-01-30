/*
 * main_wrist_simple.cpp - 糖尿病初筛腕带简化版（ESP32-S3R8N8）
 * 
 * 功能：腕带版本，仅包含心率血氧采集 + BLE + OLED
 *   - MAX30102心率血氧采集
 *   - BLE广播JSON数据到MIT App Inventor APP
 *   - OLED显示（超时熄屏）
 *   - 低功耗优化（轻睡眠）
 * 
 * 硬件：ESP32-S3R8N8 + MAX30102 + 0.96" OLED SSD1306 + 锂电池
 *   - I2C引脚：GPIO4(SDA)/GPIO5(SCL)
 *   - MAX30102地址：0x57
 *   - OLED地址：0x3C
 * 
 * 注意：腕带版本无SnO₂气敏传感器、无AD623、无加热控制
 * 
 * 库依赖：
 *   - Arduino-ESP32 2.0.17+
 *   - Adafruit SSD1306
 *   - NimBLE-Arduino
 * 
 * 编译：开发板选ESP32S3 Dev Module
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLE2902.h>

// 配置文件
#include "../config/config.h"
#include "../config/pin_config.h"
#include "../system/system_state.h"
#include "../algorithm/hr_algorithm.h"
#include "../drivers/hr_driver.h"

// ==================== OLED显示配置 ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
#define OLED_RESET      -1  // 无复位引脚

// ==================== BLE UUID配置 ====================
// MIT App Inventor APP使用的UUID
#define BLE_SERVICE_UUID        "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
