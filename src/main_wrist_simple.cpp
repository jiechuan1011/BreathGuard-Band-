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
#define BLE_CHARACTERISTIC_UUID "a1b2c3d4-e5f6-4789-abcd-ef012345678a"
#define BLE_DEVICE_NAME         "DiabetesSensor"

// ==================== 系统参数 ====================
#define SCREEN_TIMEOUT_MS       30000   // 30秒无操作熄屏
#define SAMPLE_INTERVAL_MS      50      // 50ms采样间隔
#define BLE_NOTIFY_INTERVAL_MS  4000    // 4秒BLE通知间隔
#define BLE_ADV_INTERVAL_MIN    800     // 500ms广播间隔
#define BLE_ADV_INTERVAL_MAX    1600    // 1000ms广播间隔

// ==================== 全局变量 ====================
// OLED显示对象
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BLE对象
static NimBLEServer* pServer = nullptr;
static NimBLEService* pService = nullptr;
static NimBLECharacteristic* pCharacteristic = nullptr;
static bool deviceConnected = false;
static bool oldDeviceConnected = false;

// 定时器变量
static uint32_t lastSampleTime = 0;
static uint32_t lastNotifyTime = 0;
static uint32_t lastActivityTime = 0;
static bool oledPowerOn = true;

// 电池电压监测
#define PIN_BAT_ADC     2   // 电池电压ADC引脚
#define BAT_ADC_MAX     4095    // 12位ADC最大值
#define BAT_REF_VOLTAGE 3.3     // ADC参考电压
#define BAT_DIVIDER_RATIO 2.0   // 分压比（2:1分压）

// ==================== BLE回调类 ====================
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

// ==================== OLED电源控制 ====================
void setOLEDPower(bool on) {
    display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    oledPowerOn = on;
    Serial.printf("[OLED] %s\n", on ? "开启" : "关闭");
}

// ==================== 电池电压读取 ====================
float getBatteryVoltage() {
    uint32_t adcValue = analogRead(PIN_BAT_ADC);
    float voltage = (adcValue / (float)BAT_ADC_MAX) * BAT_REF_VOLTAGE * BAT_DIVIDER_RATIO;
    return voltage;
}

uint8_t getBatteryPercent() {
    float voltage = getBatteryVoltage();
    #define BAT_FULL_VOLTAGE    4.2     // 满电电压
    #define BAT_EMPTY_VOLTAGE   3.3     // 空电电压
    
    if (voltage >= BAT_FULL_VOLTAGE) return 100;
    if (voltage <= BAT_EMPTY_VOLTAGE) return 0;
    return (uint8_t)(((voltage - BAT_EMPTY_VOLTAGE) / (BAT_FULL_VOLTAGE - BAT_EMPTY_VOLTAGE)) * 100);
}

// ==================== BLE初始化 ====================
void initBLE() {
    Serial.println("[BLE] 初始化NimBLE...");
    
    // 设置设备名称
    NimBLEDevice::init(BLE_DEVICE_NAME);
    
    // 设置功耗等级（最低功耗）
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    
    // 创建BLE服务器
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    // 创建服务
    pService = pServer->createService(BLE_SERVICE_UUID);
    
    // 创建特征值（支持read + notify）
    pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    // 添加描述符（用于启用notify）
    pCharacteristic->addDescriptor(new NimBLE2902());
    
    // 设置初始值
    pCharacteristic->setValue("{\"hr\":0,\"spo2\":0,\"acetone\":-1,\"note\":\"腕带版初始化\"}");
    
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

// ==================== 生成JSON数据 ====================
String generateJSONData() {
    const SystemState* state = system_state_get();
    
    // 获取心率、血氧、SNR
    uint8_t hr = state->hr_bpm;
    uint8_t spo2 = state->spo2_value;
    uint8_t snr_x10 = state->hr_snr_db_x10;
    
    // 计算SNR（dB）
    float snr_db = snr_x10 / 10.0;
    
    // 生成JSON字符串（腕带版，acetone固定为-1）
    char jsonBuffer[128];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
