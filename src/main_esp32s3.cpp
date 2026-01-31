/*
 * main_esp32s3.cpp - 糖尿病初筛腕带主控板（ESP32-S3R8N8完整适配版）
 * 
 * 功能：完整手表功能 + BLE广播数据
 *   - MAX30102心率血氧采集（使用现有hr_driver_s3.cpp）
 *   - SnO₂+AD623丙酮检测（腕带无此功能，数据填-1）
 *   - BLE广播JSON数据到MIT App Inventor APP
 *   - OLED显示（超时熄屏）
 *   - 按键控制（GPIO0：短按唤醒/刷新，长按>2s进入配对模式）
 *   - 低功耗优化（ESP32-S3轻睡模式）
 * 
 * 硬件：ESP32-S3R8N8（双核240MHz，8MB Flash + 8MB PSRAM）
 *   - I2C引脚：GPIO4(SDA)/GPIO5(SCL)
 *   - MAX30102地址：0x57
 *   - OLED地址：0x3C
 *   - 按键引脚：GPIO0（内部上拉，低电平有效）
 *   - SnO₂加热：GPIO9 PWM（占空比180/255）
 *   - AD623读取：GPIO10 ADC
 * 
 * 库依赖：
 *   - Arduino-ESP32 2.0.17+
 *   - Adafruit SSD1306
 *   - NimBLE-Arduino
 *   - SparkFun MAX30105（用于MAX30102驱动）
 * 
 * 编译：开发板选ESP32S3 Dev Module
 * 
 * 免责声明：本设备仅用于糖尿病初筛参考，不提供医疗诊断。
 *           测量结果仅供参考，不能替代专业医疗检查。
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
#define SCREEN_TIMEOUT_MS       30000   // 30秒无操作熄屏
#define SAMPLE_INTERVAL_MS      10      // 10ms采样间隔（100Hz）
#define BLE_NOTIFY_INTERVAL_MS  4000    // 4秒BLE通知间隔
#define BUTTON_DEBOUNCE_MS      50      // 按键消抖时间
#define BUTTON_LONG_PRESS_MS    2000    // 长按时间（2秒）

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
static uint32_t lastButtonCheckTime = 0;
static bool oledPowerOn = true;

// 按键状态变量
static bool buttonPressed = false;
static uint32_t buttonPressStartTime = 0;
static bool buttonLongPressTriggered = false;

// 丙酮检测相关（腕带无此功能，但保留接口）
#define PIN_GAS_HEATER  9   // SnO₂加热PWM引脚
#define PIN_GAS_ADC     10  // AD623输出ADC引脚
#define HEATER_DUTY     180 // 加热占空比（180/255 ≈ 70%）

// ==================== 按键初始化 ====================
void initButton() {
    // GPIO0作为按键输入，内部上拉，低电平有效
    pinMode(0, INPUT_PULLUP);
    Serial.println("[Button] GPIO0按键初始化完成");
}

// ==================== 按键处理 ====================
void handleButton() {
    uint32_t currentTime = millis();
    
    // 按键消抖检查
    if (currentTime - lastButtonCheckTime < BUTTON_DEBOUNCE_MS) {
        return;
    }
    lastButtonCheckTime = currentTime;
    
    // 读取按键状态（低电平有效）
    bool currentButtonState = (digitalRead(0) == LOW);
    
    if (currentButtonState && !buttonPressed) {
        // 按键按下
        buttonPressed = true;
        buttonPressStartTime = currentTime;
        buttonLongPressTriggered = false;
        Serial.println("[Button] 按键按下");
    } 
    else if (!currentButtonState && buttonPressed) {
        // 按键释放
        buttonPressed = false;
        
        // 检查是否为短按（未触发长按）
        if (!buttonLongPressTriggered) {
            // 短按：唤醒/刷新屏幕
            if (!oledPowerOn) {
                setOLEDPower(true);
            }
            lastActivityTime = currentTime; // 重置屏幕超时
            Serial.println("[Button] 短按：唤醒/刷新屏幕");
        }
    }
    
    // 检查长按
    if (buttonPressed && !buttonLongPressTriggered && 
        (currentTime - buttonPressStartTime >= BUTTON_LONG_PRESS_MS)) {
        buttonLongPressTriggered = true;
        
        // 长按：进入BLE配对模式或重启广播
        #ifdef USE_BLE_MODULE
        if (!deviceConnected) {
            NimBLEDevice::startAdvertising();
            Serial.println("[Button] 长按：重新开始BLE广播");
        } else {
            Serial.println("[Button] 长按：已连接，无法重新广播");
        }
        #else
        Serial.println("[Button] 长按：BLE未启用");
        #endif
    }
}

// ==================== OLED电源控制 ====================
void setOLEDPower(bool on) {
    display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    oledPowerOn = on;
    lastActivityTime = millis(); // 重置活动时间
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

// ==================== 电池电压读取 ====================
// 假设GPIO1为分压输入，1:1分压，ADC参考电压3.3V
#define PIN_BATTERY_ADC 1
#define ADC_REF_VOLTAGE 3.3
#define ADC_RESOLUTION 4095  // 12位ADC
#define VOLTAGE_DIVIDER_RATIO 2.0  // 1:1分压，实际电压=ADC读数*2

uint8_t readBatteryPercentage() {
    // 读取ADC值
    int adc_value = analogRead(PIN_BATTERY_ADC);
    
    // 计算实际电压（mV）
    float voltage_mv = (adc_value * ADC_REF_VOLTAGE * 1000.0 / ADC_RESOLUTION) * VOLTAGE_DIVIDER_RATIO;
    
    // 粗略估算电池百分比
    // >4200mV=100%，<3400mV=0%
    if (voltage_mv >= 4200) return 100;
    if (voltage_mv <= 3400) return 0;
    
    // 线性插值
    uint8_t percentage = (uint8_t)((voltage_mv - 3400) * 100 / (4200 - 3400));
    return percentage;
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
    
    // 检查数据有效性
    bool hr_valid = (hr > 0 && hr >= 40 && hr <= 180);
    bool spo2_valid = (spo2 > 0 && spo2 >= 70 && spo2 <= 100);
    bool snr_valid = (snr_x10 >= 200); // SNR >= 20dB
    
    // 生成JSON字符串
    char jsonBuffer[128];
    if (!hr_valid || !spo2_valid || !snr_valid) {
        // 数据无效，发送错误信息
        snprintf(jsonBuffer, sizeof(jsonBuffer),
                "{\"hr\":0,\"spo2\":0,\"acetone\":-1,\"note\":\"采集失败，请检查佩戴\"}");
    } else if (acetone >= 0) {
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
#ifdef USE_BLE_MODULE
void sendBLEData() {
    if (!deviceConnected) {
        return;
    }
    
    String jsonData = generateJSONData();
    pCharacteristic->setValue(jsonData.c_str());
    pCharacteristic->notify();
    
    Serial.printf("[BLE] 发送数据: %s\n", jsonData.c_str());
}
#else
void sendBLEData() {
    // BLE未启用，仅打印数据
    String jsonData = generateJSONData();
    Serial.printf("[模拟BLE] 数据: %s\n", jsonData.c_str());
}
#endif

// ==================== 采样处理 ====================
void processSample() {
    // 读取MAX30102数据
    int32_t red, ir;
    if (hr_read_latest(&red, &ir)) {
        // 更新心率算法
        int hr_status = hr_algorithm_update();
        
        // 每64个样本计算一次BPM和SpO2（约0.64秒）
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
            } else {
                // 如果计算失败，设置无效状态
                system_state_set_hr_spo2(0, 0, 0, 0, status);
                Serial.printf("[HR] 计算失败，状态码: %d\n", status);
            }
        }
    } else {
        // 读取失败，设置无效状态
        static uint8_t read_fail_count = 0;
        read_fail_count++;
        if (read_fail_count >= 10) { // 连续10次读取失败
            system_state_set_hr_spo2(0, 0, 0, 0, HR_READ_FAILED);
            read_fail_count = 0;
            Serial.println("[HR] MAX30102读取失败");
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
    #ifdef USE_BLE_MODULE
    display.print(deviceConnected ? "已连接" : "未连接");
    #else
    display.print("未启用");
    #endif
    
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
    
    // 初始化MAX30102（使用hr_driver_s3.cpp）
    if (!hr_init()) {
        Serial.println("[ERROR] MAX30102初始化失败，系统停止");
        while (1);
    }
    
    // 初始化按键
    initButton();
    
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
    if (currentTime - lastNotifyTime >= BLE_NOTIFY_INTERVAL_MS) {
        lastNotifyTime = currentTime;
        sendBLEData();
    }
    
    // 处理按键
    handleButton();
    
    // 更新OLED显示
    updateDisplay();
    
    // 检查OLED超时熄屏
    if (oledPowerOn && (currentTime - lastActivityTime >= SCREEN_TIMEOUT_MS)) {
        setOLEDPower(false);
    }
    
    // BLE连接管理
    #ifdef USE_BLE_MODULE
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // 给蓝牙栈一个清理时间
        NimBLEDevice::startAdvertising();
        Serial.println("[BLE] 开始广播");
        oldDeviceConnected = deviceConnected;
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
    #endif
    
    // 低功耗处理：使用非阻塞延时
    uint32_t elapsed = millis() - currentTime;
    if (elapsed < SAMPLE_INTERVAL_MS) {
        delay(SAMPLE_INTERVAL_MS - elapsed);
    }
}

// ==================== Arduino主函数 ====================
void setup() {
    wrist_setup();
}

void loop() {
    wrist_loop();
}