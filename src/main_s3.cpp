/*
 * main_s3.cpp - 糖尿病初筛腕带主控板（ESP32-S3R8N8完整适配版）
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
#include <esp_sleep.h>
#include <driver/ledc.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLE2902.h>

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

// 丙酮检测相关（腕带无此功能，但保留接口）
#define PIN_GAS_HEATER  9   // SnO₂加热PWM引脚
#define PIN_GAS_ADC     10  // AD623输出ADC引脚
#define HEATER_DUTY     180 // 加热占空比（180/255 ≈ 70%）

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

// ==================== BLE初始化 ====================
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
        BLE_CHAR_PROPERTIES
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

// ==================== 丙酮检测初始化（腕带无此功能，保留接口）====================
void initAcetoneSensor() {
    // 配置加热PWM
    ledcSetup(0, 1000, 8); // 通道0，1kHz，8位分辨率
    ledcAttachPin(PIN_GAS_HEATER, 0);
    ledcWrite(0, 0); // 初始关闭加热
    
    // 配置ADC引脚
    analogReadResolution(12); // 12位ADC
    pinMode(PIN_GAS_ADC, INPUT);
    
    Serial.println("[Acetone] 传感器接口初始化完成（腕带无实际传感器）");
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
    char jsonBuffer[BLE_JSON_MAX_LENGTH];
    if (acetone >= 0) {
        snprintf(jsonBuffer, sizeof(jsonBuffer),
                "{\"hr\":%d,\"spo2\":%d,\"acetone\":%.1f,\"note\":\"腕带数据，SNR:%.1fdB\"}",
                hr, spo2, acetone, snr_db);
    } else {
        snprintf(jsonBuffer, sizeof(jsonBuffer),
                "{\"hr\":%d,\"spo2\":%d,\"acetone\":-1,\"note\":\"腕带数据，SNR:%.1fdB\"}",
                hr, spo2, snr_db);
    }
    
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
        
        // 每64个样本计算一次BPM和SpO2
        static uint8_t sample_count = 0;
        sample_count++;
        
        if (sample_count >= 64) {
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
    
    // 显示标题
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("糖尿病初筛腕带");
    
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
    
    display.display();
}

// ==================== 腕带主控初始化 ====================
void wrist_setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n========================================");
    Serial.println("  糖尿病初筛腕带主控板 (ESP32-S3R8N8)");
    Serial.println("========================================\n");
    
    // 初始化系统状态
    system_state_init();
    hr_algorithm_init();
    
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
    
    // 初始化MAX30102（使用现有hr_driver.cpp）
    if (!hr_init()) {
        Serial.println("[ERROR] MAX30102初始化失败，系统停止");
        while (1);
    }
    
    // 初始化丙酮传感器接口
    initAcetoneSensor();
    
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
    
    // 10ms采样定时器（非阻塞）
    if (currentTime - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = currentTime;
        processSample();
    }
    
    // 4秒BLE通知定时器
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
    
    // 低功耗处理：无连接时进入轻睡模式
    if (!deviceConnected && oledPowerOn) {
        // 使用esp_sleep进行精确延时
        esp_sleep_enable_timer_wakeup(SAMPLE_INTERVAL_MS * 1000); // 微秒
        esp_light_sleep_start();
    }
}

// ==================== Arduino主函数 ====================
void setup() {
    wrist_setup();
}

void loop() {
    wrist_loop();
}