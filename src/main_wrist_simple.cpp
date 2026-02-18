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
#include "hr_driver.h"

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
#define SAMPLE_INTERVAL_MS      10      // 10ms采样间隔，匹配hr_algorithm.h中的HR_SAMPLE_INTERVAL_MS
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
            "{\"hr\":%d,\"spo2\":%d,\"acetone\":-1,\"note\":\"腕带版，SNR:%.1fdB\"}",
            hr, spo2, snr_db);
    
    return String(jsonBuffer);
}

// ==================== 发送BLE数据 ====================
void sendBLEData() {
    if (!deviceConnected) {
        return;
    }
    
    String jsonData = generateJSONData();
    pCharacteristic->setValue(jsonData.c_str());
    pCharacteristic->notify();
    
    Serial.printf("[BLE] 发送数据: %s\n", jsonData.c_str());
}

// ==================== 采样处理 ====================
void processSample() {
    // 读取MAX30102数据
    int32_t red, ir;
    if (hr_read_latest(&red, &ir)) {
        // 更新心率算法
        int hr_status = hr_algorithm_update();
        
        // 每128个样本计算一次BPM和SpO2（10ms间隔，128个样本≈1.28秒），匹配hr_algorithm.h中的HR_BUFFER_SIZE
        static uint8_t sample_count = 0;
        sample_count++;
        
        if (sample_count >= 128) {
            sample_count = 0;
            
            // 计算心率
            int status;
            uint8_t bpm = hr_calculate_bpm(&status);
            
            if (status == HR_SUCCESS && bpm > 0) {
                // 计算血氧
                uint8_t spo2 = hr_calculate_spo2(&status);
                uint8_t snr_x10 = hr_get_signal_quality();
                uint8_t correlation = hr_get_correlation_quality();
                
                // 更新系统状态
                system_state_set_hr_spo2(bpm, spo2, snr_x10, correlation, status);
                
                Serial.printf("[HR] BPM:%d SpO2:%d SNR:%.1fdB Corr:%d%%\n",
                            bpm, spo2, snr_x10/10.0, correlation);
            }
        }
    }
}

// ==================== OLED显示 ====================
void updateDisplay() {
    if (!oledPowerOn) return;
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    const SystemState* state = system_state_get();
    uint8_t batteryPercent = getBatteryPercent();
    
    // 显示标题和电池
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("糖尿病初筛腕带");
    
    // 电池百分比
    display.setCursor(90, 0);
    display.printf("电池:%d%%", batteryPercent);
    
    // 显示心率
    display.setCursor(0, 12);
    display.print("心率:");
    if (state->hr_bpm > 0) {
        display.printf("%d bpm", state->hr_bpm);
    } else {
        display.print("--");
    }
    
    // 显示血氧
    display.setCursor(0, 24);
    display.print("血氧:");
    if (state->spo2_value > 0) {
        display.printf("%d%%", state->spo2_value);
    } else {
        display.print("--");
    }
    
    // 显示SNR
    display.setCursor(0, 36);
    display.print("信号质量:");
    if (state->hr_snr_db_x10 > 0) {
        display.printf("%.1f dB", state->hr_snr_db_x10 / 10.0);
    } else {
        display.print("--");
    }
    
    // 显示BLE状态
    display.setCursor(0, 48);
    display.print("BLE:");
    display.print(deviceConnected ? "已连接" : "未连接");
    
    // 显示版本信息
    display.setCursor(70, 56);
    display.print("腕带版");
    
    display.display();
}

// ==================== 腕带主控初始化 ====================
void wrist_setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n========================================");
    Serial.println("  糖尿病初筛腕带简化版 (ESP32-S3R8N8)");
    Serial.println("  硬件：MAX30102 + OLED + BLE");
    Serial.println("  版本：腕带版（无丙酮检测）");
    Serial.println("========================================\n");
    
    // 初始化系统状态
    system_state_init();
    hr_algorithm_init();
    
    // 初始化电池ADC（ESP32-S3）
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // 设置11dB衰减，测量范围0-3.3V
    pinMode(PIN_BAT_ADC, INPUT);
    
    // 初始化OLED
    Wire.begin(PIN_SDA, PIN_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[OLED] 初始化失败！");
        while (1);
    }
    Serial.println("[OLED] 初始化成功");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.print("   正在启动...");
    display.display();
    
    // 初始化MAX30102（带重试机制）
    const uint8_t max_retries = 3;
    bool max30102_ok = false;
    for (uint8_t retry = 0; retry < max_retries; retry++) {
        if (hr_driver_init()) {
            max30102_ok = true;
            break;
        }
        Serial.printf("[ERROR] MAX30102初始化失败，重试 %d/%d\n", retry + 1, max_retries);
        delay(1000);
    }
    if (!max30102_ok) {
        Serial.println("[ERROR] MAX30102初始化彻底失败，系统继续运行但心率功能不可用");
        // 不停止系统，允许其他功能（如BLE、显示）继续工作
    }
    
    // 初始化BLE
    initBLE();
    
    // 初始化完成
    lastActivityTime = millis();
    display.clearDisplay();
    display.setCursor(0, 28);
    display.print("   初始化完成");
    display.display();
    delay(1000);
    
    Serial.println("[Init] 系统启动完成\n");
}

// ==================== 主循环 ====================
void wrist_loop() {
    uint32_t currentTime = millis();
    
    // 10ms采样定时器（非阻塞），匹配hr_algorithm.h中的HR_SAMPLE_INTERVAL_MS
    if (currentTime - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = currentTime;
        processSample();
    }
    
    // 4秒BLE通知定时器（仅在连接时发送）
    if (deviceConnected && (currentTime - lastNotifyTime >= BLE_NOTIFY_INTERVAL_MS)) {
        lastNotifyTime = currentTime;
        sendBLEData();
    }
    
    // 更新OLED显示
    updateDisplay();
    
    // 检查OLED超时熄屏
    if (oledPowerOn && (currentTime - lastActivityTime >= SCREEN_TIMEOUT_MS)) {
        setOLEDPower(false);
    }
    
    // BLE连接管理
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // 给蓝牙栈一个清理时间
        NimBLEDevice::startAdvertising();
        Serial.println("[BLE] 开始广播");
        oldDeviceConnected = deviceConnected;
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
    
    // 低功耗处理：无连接且OLED关闭时进入轻睡眠
    if (!deviceConnected && !oledPowerOn) {
        // 设置唤醒定时器（100ms后唤醒），降低唤醒频率以节省功耗
        esp_sleep_enable_timer_wakeup(100 * 1000); // 100ms，微秒
        esp_light_sleep_start();
        // 睡眠唤醒后，更新时间戳以避免立即再次睡眠
        lastSampleTime = millis();
    } else {
        // 正常模式，使用非阻塞延时
        uint32_t elapsed = millis() - currentTime;
        if (elapsed < SAMPLE_INTERVAL_MS) {
            delay(SAMPLE_INTERVAL_MS - elapsed);
        }
    }
}

// ==================== Arduino主函数 ====================
void setup() {
    wrist_setup();
}

void loop() {
    wrist_loop();
}