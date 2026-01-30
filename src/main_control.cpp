/*
 * main_control.cpp - 糖尿病初筛腕带主控板（ESP32-C3 SuperMini）
 * 
 * 功能：腕带主控完整功能
 *   - MAX30102 心率血氧采集
 *   - OLED 显示实时心率/血氧/电池
 *   - BLE Server 广播数据（匹配 MIT App Inventor APP UUID）
 *   - 分时采集调度（心率高频采样 + 低频计算）
 *   - 运动干扰校正
 *   - 按键控制
 * 
 * 硬件：
 *   - ESP32-C3 SuperMini
 *   - MAX30102 心率血氧传感器（I2C）
 *   - 0.96" OLED SSD1306（I2C，0x3C地址）
 *   - 300mAh 锂电池 + TP4056充电
 *   - 按键：GPIO6（切换/重置），GPIO7（开关OLED）
 * 
 * 项目约束：
 *   - 总成本 ≤ 500元，腕带重量 < 45g
 *   - 仅输出趋势参考数据，不输出任何医疗诊断结论
 *   - 所有输出包含免责声明
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ESP32AnalogRead.h>
#include "../config/config.h"
#include "../config/pin_config.h"
#include "../algorithm/hr_algorithm.h"
#include "../drivers/hr_driver.h"
#include "../system/scheduler.h"

// ==================== 引脚定义（ESP32-C3 SuperMini）====================
#define PIN_SDA         4   // I2C数据线（OLED + MAX30102）
#define PIN_SCL         5   // I2C时钟线（OLED + MAX30102）
#define PIN_BTN1        6   // 按键1：切换显示/重置测量
#define PIN_BTN2        7   // 按键2：开关OLED
#define PIN_BAT_ADC     2   // 电池电压ADC输入（需2:1分压）

// ==================== OLED显示配置 ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
#define OLED_RESET      -1  // 无复位引脚

// ==================== BLE配置（匹配 MIT App Inventor APP）====================
#define BLE_DEVICE_NAME     "DiabetesWristBand"
#define BLE_SERVICE_UUID    "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
#define BLE_CHAR_UUID       "a1b2c3d4-e5f6-4789-abcd-ef012345678a"

// ==================== 系统参数 ====================
#define SCREEN_TIMEOUT_MS   30000   // 30秒无操作熄屏
#define DEBOUNCE_MS         50      // 按键消抖时间
#define BLE_UPDATE_INTERVAL_MS 1000 // BLE数据更新间隔（1秒）
#define BATTERY_UPDATE_INTERVAL_MS 5000 // 电池电压更新间隔（5秒）

// ==================== 电池电压参数 ====================
#define BAT_ADC_MAX         4095    // 12位ADC最大值
#define BAT_REF_VOLTAGE     3.3     // ADC参考电压（V）
#define BAT_DIVIDER_RATIO   2.0     // 分压比（2:1分压）
#define BAT_FULL_VOLTAGE    4.2     // 满电电压
#define BAT_EMPTY_VOLTAGE   3.3     // 空电电压

// ==================== 全局变量 ====================
// OLED显示对象
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BLE对象
BLEServer* pServer = nullptr;
BLEService* pService = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool bleConnected = false;
bool bleAdvertising = false;

// 系统状态
bool oledPowerOn = true;
uint32_t lastActivityTime = 0;
uint32_t lastBleUpdateTime = 0;
uint32_t lastBatteryUpdateTime = 0;

// 健康数据
uint8_t currentHeartRate = 0;
uint8_t currentSpO2 = 0;
uint8_t signalQuality = 0;  // SNR*10
float batteryVoltage = 0.0;
uint8_t batteryPercent = 0;

// 按键状态
uint8_t btn1LastState = HIGH;
uint8_t btn2LastState = HIGH;
uint32_t btn1PressTime = 0;

// ==================== BLE回调类 ====================
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleConnected = true;
        DEBUG_PRINTLN("[BLE] 设备已连接");
    }

    void onDisconnect(BLEServer* pServer) {
        bleConnected = false;
        bleAdvertising = false;
        DEBUG_PRINTLN("[BLE] 设备已断开");
        // 重新开始广播
        pServer->getAdvertising()->start();
        bleAdvertising = true;
        DEBUG_PRINTLN("[BLE] 重新开始广播");
    }
};

// ==================== OLED电源控制 ====================
void setOLEDPower(bool on) {
    if (on) {
        display.ssd1306_command(SSD1306_DISPLAYON);
        oledPowerOn = true;
        lastActivityTime = millis();
    } else {
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        oledPowerOn = false;
    }
}

// ==================== 电池电量读取 ====================
void updateBatteryVoltage() {
    uint32_t adcValue = analogRead(PIN_BAT_ADC);
    batteryVoltage = (adcValue / (float)BAT_ADC_MAX) * BAT_REF_VOLTAGE * BAT_DIVIDER_RATIO;
    
    // 计算电量百分比
    if (batteryVoltage >= BAT_FULL_VOLTAGE) {
        batteryPercent = 100;
    } else if (batteryVoltage <= BAT_EMPTY_VOLTAGE) {
        batteryPercent = 0;
    } else {
        batteryPercent = (uint8_t)(((batteryVoltage - BAT_EMPTY_VOLTAGE) / 
                                  (BAT_FULL_VOLTAGE - BAT_EMPTY_VOLTAGE)) * 100);
    }
}

// ==================== 绘制电池图标 ====================
void drawBatteryIcon(int x, int y, uint8_t percent) {
    // 电池外框
    display.drawRect(x, y, 18, 9, SSD1306_WHITE);
    display.drawRect(x + 18, y + 3, 2, 3, SSD1306_WHITE);
    
    // 电量条（根据百分比填充）
    int fillWidth = (percent * 14) / 100;
    if (fillWidth > 0) {
        display.fillRect(x + 2, y + 2, fillWidth, 5, SSD1306_WHITE);
    }
}

// ==================== BLE初始化 ====================
void initBLE() {
    // 创建BLE设备
    BLEDevice::init(BLE_DEVICE_NAME);
    
    // 创建BLE服务器
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    // 创建BLE服务
    pService = pServer->createService(BLE_SERVICE_UUID);
    
    // 创建BLE特征值（支持读、写、通知）
    pCharacteristic = pService->createCharacteristic(
        BLE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    
    // 添加描述符
    pCharacteristic->addDescriptor(new BLE2902());
    
    // 启动服务
    pService->start();
    
    // 开始广播
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // 有助于iPhone连接
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    bleAdvertising = true;
    
    DEBUG_PRINTLN("[BLE] 初始化完成，开始广播");
}

// ==================== 发送BLE数据 ====================
void sendBLEData() {
    if (!bleConnected) return;
    
    // 组装JSON数据
    char jsonBuffer[128];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"hr\":%d,\"spo2\":%d,\"acetone\":0.0,\"note\":\"腕带模式，仅供参考\"}",
             currentHeartRate, currentSpO2);
    
    // 发送数据
    pCharacteristic->setValue(jsonBuffer);
    pCharacteristic->notify();
    
    DEBUG_PRINTF("[BLE] 发送数据: %s\n", jsonBuffer);
}

// ==================== 更新健康数据 ====================
void updateHealthData() {
    // 从心率算法获取最新数据
    int status;
    uint8_t bpm = hr_calculate_bpm(&status);
    uint8_t spo2 = hr_calculate_spo2(&status);
    uint8_t snr = hr_get_signal_quality();
    
    if (status == HR_SUCCESS) {
        currentHeartRate = bpm;
        currentSpO2 = spo2;
        signalQuality = snr;
    } else {
        // 如果计算失败，保持上次有效值
        DEBUG_PRINTF("[健康数据] 计算失败，状态码: %d\n", status);
    }
}

// ==================== 绘制主界面 ====================
void drawMainDisplay() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // 标题行
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("糖尿病初筛腕带");
    
    // BLE连接状态
    display.setCursor(90, 0);
    display.print(bleConnected ? "BLE已连" : "BLE未连");
    
    // 心率显示（大字体）
    display.setTextSize(2);
    display.setCursor(0, 12);
    display.print("HR:");
    if (currentHeartRate > 0) {
        display.printf("%3d", currentHeartRate);
    } else {
        display.print(" --");
    }
    display.print(" bpm");
    
    // 血氧显示（大字体）
    display.setCursor(0, 32);
    display.print("SpO2:");
    if (currentSpO2 > 0) {
        display.printf("%3d", currentSpO2);
    } else {
        display.print(" --");
    }
    display.print(" %");
    
    // 信号质量
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.print("信号:");
    if (signalQuality > 0) {
        display.printf("%d.%d dB", signalQuality / 10, signalQuality % 10);
    } else {
        display.print(" --");
    }
    
    // 电池图标和电量
    drawBatteryIcon(104, 0, batteryPercent);
    display.setCursor(90, 10);
    display.printf("%d%%", batteryPercent);
    
    // 免责声明（底部）
    display.setCursor(0, 62);
    display.print(DISCLAIMER_STRING);
    
    display.display();
}

// ==================== 按键处理 ====================
void handleButtons() {
    uint32_t now = millis();
    uint8_t btn1State = digitalRead(PIN_BTN1);
    uint8_t btn2State = digitalRead(PIN_BTN2);
    
    // 按键1处理（切换显示/重置测量）
    if (btn1State == LOW && btn1LastState == HIGH) {
        // 按键1按下
        btn1PressTime = now;
        lastActivityTime = now;
        
        // 唤醒OLED
        if (!oledPowerOn) {
            setOLEDPower(true);
        } else {
            // 短按：重置测量
            DEBUG_PRINTLN("[按键] 短按：重置测量");
            // 可以在这里重置心率算法缓冲区
        }
    }
    
    btn1LastState = btn1State;
    
    // 按键2处理（开关OLED）
    if (btn2State == LOW && btn2LastState == HIGH) {
        // 按键2按下
        lastActivityTime = now;
        
        // 切换OLED电源
        setOLEDPower(!oledPowerOn);
        DEBUG_PRINTF("[按键] 开关OLED: %s\n", oledPowerOn ? "开" : "关");
    }
    
    btn2LastState = btn2State;
}

// ==================== 腕带主控初始化 ====================
void wrist_setup() {
    // 初始化串口
    Serial.begin(115200);
    delay(500);
    
    DEBUG_PRINTLN("\n\n========================================");
    DEBUG_PRINTLN("  糖尿病初筛腕带主控板");
    DEBUG_PRINTLN("  Diabetes Screening Wrist Band");
    DEBUG_PRINTLN("========================================\n");
    
    // 初始化按键引脚
    pinMode(PIN_BTN1, INPUT_PULLUP);
    pinMode(PIN_BTN2, INPUT_PULLUP);
    DEBUG_PRINTLN("[Init] 按键初始化完成");
    
    // 初始化I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    
    // 初始化OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        DEBUG_PRINTLN("[Init] OLED初始化失败！");
        while (1);  // 停止运行
    }
    DEBUG_PRINTLN("[Init] OLED初始化完成");
    
    // 显示启动画面
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println("  糖尿病初筛腕带");
    display.setCursor(0, 35);
    display.println("    正在启动...");
    display.display();
    delay(1000);
    
    // 初始化心率传感器
    if (!hr_init()) {
        DEBUG_PRINTLN("[Init] MAX30102初始化失败！");
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("  MAX30102初始化");
        display.setCursor(0, 35);
        display.println("      失败！");
        display.display();
        delay(2000);
    } else {
        DEBUG_PRINTLN("[Init] MAX30102初始化完成");
    }
    
    // 初始化心率算法
    hr_algorithm_init();
    DEBUG_PRINTLN("[Init] 心率算法初始化完成");
    
    // 初始化调度器
    scheduler_init();
    DEBUG_PRINTLN("[Init] 调度器初始化完成");
    
    // 初始化BLE
    initBLE();
    
    // 初始化电池ADC
    pinMode(PIN_BAT_ADC, INPUT);
    updateBatteryVoltage();
    
    // 初始化完成
    lastActivityTime = millis();
    display.clearDisplay();
    display.setCursor(0, 28);
    display.println("   初始化完成");
    display.display();
    delay(1000);
    
    DEBUG_PRINTLN("[Init] 系统启动完成\n");
}

// ==================== 腕带主循环 ====================
void wrist_loop() {
    uint32_t now = millis();
    
    // 处理按键
    handleButtons();
    
    // 运行调度器（处理心率采样和计算）
    scheduler_run();
    
    // 更新健康数据
    static uint32_t lastHealthUpdate = 0;
    if (now - lastHealthUpdate >= 2000) {  // 每2秒更新一次
        updateHealthData();
        lastHealthUpdate = now;
    }
    
    // 更新电池电压
    if (now - lastBatteryUpdateTime >= BATTERY_UPDATE_INTERVAL_MS) {
        updateBatteryVoltage();
        lastBatteryUpdateTime = now;
    }
    
    // 发送BLE数据
    if (now - lastBleUpdateTime >= BLE_UPDATE_INTERVAL_MS) {
        sendBLEData();
        lastBleUpdateTime = now;
    }
    
    // 检查OLED超时熄屏
    if (oledPowerOn && (now - lastActivityTime >= SCREEN_TIMEOUT_MS)) {
        setOLEDPower(false);
        DEBUG_PRINTLN("[OLED] 超时熄屏");
    }
    
    // 刷新OLED显示
    if (oledPowerOn) {
        drawMainDisplay();
    }
    
    // 低功耗延迟
    delay(10);  // 10ms延迟，对应100Hz采样率
}