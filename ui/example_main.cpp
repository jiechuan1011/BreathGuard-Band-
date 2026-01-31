/*
 * example_main.cpp - 糖尿病筛查手表UI示例程序
 * 
 * 功能：展示完整的UI系统使用方式
 * 硬件：ESP32-S3R8N8 + 1.85英寸AMOLED (390×450)
 * 库依赖：TFT_eSPI
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "ui_config.h"
#include "ui_manager.h"
#include "main_screen.h"
#include "status_bar.h"
#include "metric_card.h"
#include "progress_ring.h"
#include "waveform_view.h"

// ==================== 硬件配置 ====================
TFT_eSPI tft = TFT_eSPI();      // 显示对象
UiManager& ui = UiManager::getInstance(); // UI管理器

// ==================== 引脚定义 ====================
#define PIN_BTN1 0              // 按键1
#define PIN_BTN2 1              // 按键2
#define PIN_BACKLIGHT 2         // 背光控制

// ==================== 全局变量 ====================
uint32_t last_update_time = 0;
uint32_t last_button_check = 0;
bool btn1_pressed = false;
bool btn2_pressed = false;

// ==================== 测试数据 ====================
TestResult test_results[] = {
    {.timestamp = 1706594400, .heart_rate = 75, .blood_oxygen = 98, .acetone_level = 5.2, .trend = TREND_NORMAL, .confidence = 95, .test_type = TEST_TYPE_COMPREHENSIVE},
    {.timestamp = 1706508000, .heart_rate = 72, .blood_oxygen = 97, .acetone_level = 4.8, .trend = TREND_NORMAL, .confidence = 92, .test_type = TEST_TYPE_COMPREHENSIVE},
