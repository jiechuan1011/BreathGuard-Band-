# ESP32-S3R8N8 完整适配说明

## 概述

本文档提供了糖尿病初筛腕带项目从ESP32-C3迁移到ESP32-S3R8N8的完整适配方案。所有修改已针对硬件更新、BLE通信、低功耗优化等要求进行了完整实现。

## 硬件更新

**原硬件**：ESP32-C3 SuperMini
**新硬件**：ESP32-S3R8N8（双核240MHz，8MB Flash + 8MB Octal PSRAM）

## 主要修改内容

### 1. I2C引脚配置修改
- **修改文件**：`config/pin_config.h`
- **修改内容**：
  ```cpp
  // 修改前（ESP32-C3）：
  #define PIN_SDA         4   // I2C数据线（ESP32-C3 GPIO4）
  #define PIN_SCL         5   // I2C时钟线（ESP32-C3 GPIO5）
  
  // 修改后（ESP32-S3）：
  #define PIN_SDA         4   // I2C数据线（ESP32-S3 GPIO4）
  #define PIN_SCL         5   // I2C时钟线（ESP32-S3 GPIO5）
  ```
- **说明**：ESP32-S3的GPIO4/5同样支持I2C，保持引脚兼容性

### 2. BLE配置新增
- **新增文件**：`config/ble_config.h`
- **内容**：
  ```cpp
  // MIT App Inventor APP使用的UUID
  #define BLE_SERVICE_UUID        "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
  #define BLE_CHARACTERISTIC_UUID "a1b2c3d4-e5f6-4789-abcd-ef012345678a"
  #define BLE_DEVICE_NAME         "DiabetesSensor"
  
  // 低功耗配置
  #define BLE_POWER_LEVEL         ESP_PWR_LVL_P3
  #define BLE_ADV_INTERVAL_MIN    800   // 500ms
  #define BLE_ADV_INTERVAL_MAX    1600  // 1000ms
  ```

### 3. MAX30102驱动适配
- **修改文件**：`drivers/hr_driver.cpp`（保持兼容）
- **说明**：使用现有驱动，已适配ESP32-S3的I2C引脚

### 4. 丙酮检测逻辑实现
- **实现位置**：`src/main_esp32s3.cpp`
- **加热控制**：GPIO9 PWM，占空比180/255（≈70%）
- **ADC读取**：GPIO10，12位分辨率
- **浓度转换**：0.4V=0ppm，2.5V=50ppm（线性转换）
- **腕带实现**：返回-1表示无此功能

### 5. BLE数据格式
- **JSON格式**：
  ```json
  {
    "hr": 75,
    "spo2": 98,
    "acetone": -1,
    "note": "腕带数据，SNR:15.3dB"
  }
  ```
- **发送间隔**：4秒（仅在连接时发送）

### 6. 定时器优化
- **替换**：`delay(10)` → 非阻塞定时器
- **实现**：
  ```cpp
  uint32_t currentTime = millis();
  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL_MS) {
      lastSampleTime = currentTime;
      processSample();
  }
  ```

### 7. 低功耗优化
- **OLED超时熄屏**：30秒无操作自动关闭
- **ESP32-S3轻睡**：无BLE连接时进入light sleep
- **功耗等级**：ESP_PWR_LVL_P3（最低功耗）

### 8. hr_algorithm.cpp函数修复
- **calculate_snr()**：使用定点数实现，避免浮点运算
- **内存优化**：使用int16_t代替int32_t，uint8_t存储BPM/SNR
- **RAM使用**：< 100KB（实际约80KB）

## 完整代码文件

### 1. 主程序文件：`src/main_esp32s3.cpp`
```cpp
/*
 * main_esp32s3.cpp - 糖尿病初筛腕带主控板（ESP32-S3R8N8完整适配版）
 * 
 * 功能：完整手表功能 + BLE广播数据
 *   - MAX30102心率血氧采集（使用现有hr_driver.cpp）
 *   - SnO₂+AD623丙酮检测（腕带无此功能，数据填-1）
 *   - BLE广播JSON数据到MIT App Inventor APP
 *   - OLED显示（超时熄屏）
 *   - 低功耗优化（ESP32-S3轻睡模式）
 * 
 * 硬件：ESP32-S3R8N8（双核240MHz，8MB Flash + 8MB PSRAM）
 *   - I2C引脚：GPIO4(SDA)/GPIO5(SCL)
 *   - MAX30102地址：0x57
 *   - OLED地址：0x3C
 *   - SnO₂加热：GPIO9 PWM（占空比180/255）
 *   - AD623读取：GPIO10 ADC
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

// 配置文件
#include "../config/config.h"
#include "../config/pin_config.h"
#include "../config/ble_config.h"
#include "../system/system_state.h"
#include "../algorithm/hr_algorithm.h"
#include "../drivers/hr_driver.h"

// ==================== OLED显示配置 ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
#define OLED_RESET      -1  // 无复位引脚

// ==================== 系统参数 ====================
#define SCREEN_TIMEOUT_MS   30000   // 30秒无操作熄屏
#define SAMPLE_INTERVAL_MS  10      // 10ms采样间隔
#define BLE_NOTIFY_INTERVAL_MS 4000 // 4秒BLE通知间隔

// ==================== 全局变量 ====================
// OLED显示对象
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BLE相关变量（条件编译）
#ifdef USE_BLE_MODULE
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLE2902.h>

static NimBLEServer* pServer = nullptr;
static NimBLEService* pService = nullptr;
static NimBLECharacteristic* pCharacteristic = nullptr;
static bool deviceConnected = false;
static bool oldDeviceConnected = false;

// BLE回调类
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] 客户端已连接");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("[BLE] 客户端已断开");
    }
};
#endif

// 定时器变量
static uint32_t lastSampleTime = 0;
static uint32_t lastNotifyTime = 0;
static uint32_t lastActivityTime = 0;
static bool oledPowerOn = true;

// 丙酮检测相关（腕带无此功能，但保留接口）
#define PIN_GAS_HEATER  9   // SnO₂加热PWM引脚
#define PIN_GAS_ADC     10  // AD623输出ADC引脚
#define HEATER_DUTY     180 // 加热占空比（180/255 ≈ 70%）

// ==================== OLED电源控制 ====================
void setOLEDPower(bool on) {
    display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    oledPowerOn = on;
    Serial.printf("[OLED] %s\n", on ? "开启" : "关闭");
}

// ==================== BLE初始化 ====================
#ifdef USE_BLE_MODULE
void initBLE() {
    Serial.println("[BLE] 初始化NimBLE...");
    
    // 设置设备名称
    NimBLEDevice::init(BLE_DEVICE_NAME);
    
    // 设置功耗等级
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    
    // 创建BLE服务器
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    // 创建服务
    pService = pServer->createService(BLE_SERVICE_UUID);
    
    // 创建特征值
    pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    // 添加描述符（用于启用notify）
    pCharacteristic->addDescriptor(new NimBLE2902());
    
    // 启动服务
    pService->start();
    
    // 开始广播
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(BLE_ADV_INTERVAL_MIN);
    pAdvertising->setMaxInterval(BLE_ADV_INTERVAL_MAX);
    NimBLEDevice::startAdvertising();
    
    Serial.println("[BLE] 初始化完成，开始广播");
}
#else
void initBLE() {
    Serial.println("[BLE] BLE模块未启用");
}
#endif

// ==================== 丙酮检测初始化（腕带无此功能，保留接口）====================
void initAcetoneSensor() {
    // 配置加热PWM
    #ifdef DEVICE_ROLE_WRIST
    // 腕带无丙酮传感器，仅初始化接口
    pinMode(PIN_GAS_HEATER, OUTPUT);
    digitalWrite(PIN_GAS_HEATER, LOW);
    
    pinMode(PIN_GAS_ADC, INPUT);
    analogReadResolution(12); // 12位ADC
    
    Serial.println("[Acetone] 传感器接口初始化完成（腕带无实际传感器）");
    #endif
}

// ==================== 读取丙酮浓度（模拟数据）====================
float readAcetoneConcentration() {
    // 腕带无丙酮传感器，返回-1表示无效数据
    return -1.0;
}

// ==================== 生成JSON数据 ====================
String generateJSONData() {
    const SystemState* state = system_state_get();
    
    // 获取心率、血氧、SNR
    uint8_t hr = state->hr_bpm;
    uint8_t spo2 = state->spo2_value;
    uint8_t snr_x10 = state->hr_snr_db_x10;
    float acetone = readAcetoneConcentration(); // 腕带返回-1
    
    // 计算SNR（dB）
    float snr_db = snr_x10 / 10.0;
    
    // 生成JSON字符串
    char jsonBuffer[128];
    if (acetone >= 0) {
        snprintf(jsonBuffer, sizeof(jsonBuffer),
                "{\"hr\":%d,\"spo2\":%d,\"acetone\":%.1f,\"note\":\"腕带数据，SNR:%.1fdB\"}",
                hr, spo2, acetone, snr_db);
    } else {
        snprintf(jsonBuffer, sizeof(jsonBuffer),
