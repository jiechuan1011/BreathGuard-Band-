/*
 * main_control_new.ino - 糖尿病初筛腕带主控板（ESP32-S3R8N8）
 * 
 * 功能：腕带作为BLE Peripheral（服务器）发送数据
 *   - 使用NimBLE库（更省电、RAM占用低）
 *   - 定时从system_state_get()获取心率、血氧数据
 *   - 打包成JSON格式通过BLE notify发送
 *   - OLED显示当前状态
 *   - 低功耗优化
 * 
 * 硬件：
 *   - ESP32-S3R8N8（双核240MHz，8MB Flash + 8MB PSRAM）
 *   - 0.96寸 OLED SSD1306（I2C，0x3C地址）
 *   - MAX30102 心率血氧传感器
 *   - GPIO21/22 用于I2C（默认ESP32-S3引脚）
 * 
 * BLE配置：
 *   - 设备名："DiabetesSensor"
 *   - 服务UUID："a1b2c3d4-e5f6-4789-abcd-ef0123456789"
 *   - 特征值UUID："a1b2c3d4-e5f6-4789-abcd-ef012345678a"（支持read + notify）
 * 
 * 项目约束：
 * - 总成本 ≤500元，腕带重量 <45g
 * - 不输出任何医疗诊断相关内容
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sleep.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 使用NimBLE库（更省电）
#define USE_NIMBLE
#include "../config/ble_config.h"

// 系统状态
#include "../system/system_state.h"

// ==================== OLED显示配置 ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
#define OLED_RESET      -1  // 无复位引脚

// ==================== 引脚定义（ESP32-S3）====================
#define PIN_SDA         21   // I2C数据线（OLED和传感器）
#define PIN_SCL         22   // I2C时钟线（OLED和传感器）
#define PIN_BTN1        6    // 按键1：切换界面/功能
#define PIN_BTN2        7    // 按键2：返回/确认
#define PIN_BAT_ADC     2    // 电池电压ADC输入

// ==================== 系统参数 ====================
#define SCREEN_TIMEOUT_MS   30000   // 30秒无操作熄屏
#define DEBOUNCE_MS         50      // 按键消抖时间
#define BLE_NOTIFY_INTERVAL 4000    // BLE数据发送间隔（毫秒）

// ==================== 电池电压参数 ====================
#define BAT_ADC_MAX         4095    // 12位ADC最大值
#define BAT_REF_VOLTAGE     3.3     // ADC参考电压（V）
#define BAT_DIVIDER_RATIO   2.0     // 分压比（2:1分压）
#define BAT_FULL_VOLTAGE    4.2     // 满电电压
#define BAT_EMPTY_VOLTAGE   3.3     // 空电电压

// ==================== 免责声明文本 ====================
#define DISCLAIMER_TEXT     "仅供参考，建议咨询医生"

// ==================== 界面模式枚举 ====================
enum DisplayMode {
  MODE_CLOCK,       // 时钟模式（默认）
  MODE_MEASUREMENT, // 测量数据显示模式
  MODE_BLE_STATUS   // BLE状态显示模式
};

// ==================== 全局变量 ====================
// OLED显示对象
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 界面状态
DisplayMode currentMode = MODE_CLOCK;
bool oledPowerOn = true;
uint32_t lastActivityTime = 0;

// 时间同步状态
bool wifiConnected = false;
bool ntpSynced = false;
uint32_t lastNtpSyncTime = 0;

// BLE状态（在ble_config.h中声明，这里定义）
BLE_SERVER_TYPE pServer = nullptr;
BLE_SERVICE_TYPE pService = nullptr;
BLE_CHARACTERISTIC_TYPE pCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// 数据发送定时器
uint32_t lastNotifyTime = 0;

// 按键状态
uint8_t btn1LastState = HIGH;
uint8_t btn2LastState = HIGH;

// 星期中文字符
const char* weekdayChinese[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

// ==================== BLE回调类 ====================
#ifdef USE_NIMBLE
// NimBLE连接状态回调
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
        Serial.println("[BLE] 客户端已连接");
        deviceConnected = true;
    };

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
        Serial.println("[BLE] 客户端已断开连接");
        deviceConnected = false;
    }
};
#else
// 默认BLE连接状态回调
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("[BLE] 客户端已连接");
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        Serial.println("[BLE] 客户端已断开连接");
        deviceConnected = false;
    }
};
#endif

// ==================== OLED电源控制 ====================
void setOLEDPower(bool on) {
  display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
  oledPowerOn = on;
}

// ==================== 电池电量读取与显示 ====================
float getBatteryVoltage() {
  uint32_t adcValue = analogRead(PIN_BAT_ADC);
  float voltage = (adcValue / (float)BAT_ADC_MAX) * BAT_REF_VOLTAGE * BAT_DIVIDER_RATIO;
  return voltage;
}

uint8_t getBatteryPercent() {
  float voltage = getBatteryVoltage();
  if (voltage >= BAT_FULL_VOLTAGE) return 100;
  if (voltage <= BAT_EMPTY_VOLTAGE) return 0;
  return (uint8_t)(((voltage - BAT_EMPTY_VOLTAGE) / (BAT_FULL_VOLTAGE - BAT_EMPTY_VOLTAGE)) * 100);
}

void drawBatteryIcon(int x, int y, uint8_t percent) {
  // 电池外框
  display.drawRect(x, y, 18, 9, SSD1306_WHITE);
  display.drawRect(x + 18, y + 3, 2, 3, SSD1306_WHITE);
  
  // 电量条
  int fillWidth = (percent * 14) / 100;
  if (fillWidth > 0) {
    display.fillRect(x + 2, y + 2, fillWidth, 5, SSD1306_WHITE);
  }
}

// ==================== BLE初始化 ====================
void ble_init() {
    Serial.println("[BLE] 初始化NimBLE...");
    
#ifdef USE_NIMBLE
    // 初始化NimBLE
    NimBLEDevice::init(BLE_DEVICE_NAME);
    
    // 设置发射功率（低功耗）
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    
    // 创建服务器
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    // 创建服务
    pService = pServer->createService(BLE_SERVICE_UUID);
    
    // 创建特征值（支持read和notify）
    pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    // 设置初始值
    pCharacteristic->setValue("Initializing...");
    
    // 启动服务
    pService->start();
    
    // 开始广播
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // 有助于iPhone连接
    pAdvertising->setMaxPreferred(0x12);
    
    // 设置广播间隔（500ms）
    pAdvertising->setInterval(BLE_ADVERTISING_INTERVAL_MS);
    pAdvertising->start();
    
    Serial.println("[BLE] NimBLE初始化完成，开始广播");
#else
    // 使用默认BLE库
    BLEDevice::init(BLE_DEVICE_NAME);
    
    // 设置发射功率
    BLEDevice::setPower(ESP_PWR_LVL_P3);
    
    // 创建服务器
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    // 创建服务
    pService = pServer->createService(BLE_SERVICE_UUID);
    
    // 创建特征值
    pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    
    // 设置初始值
    pCharacteristic->setValue("Initializing...");
    
    // 启动服务
    pService->start();
    
    // 开始广播
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    
    // 设置广播间隔
    pAdvertising->setInterval(BLE_ADVERTISING_INTERVAL_MS);
    pAdvertising->start();
    
    Serial.println("[BLE] 默认BLE初始化完成，开始广播");
#endif
}

// ==================== 数据打包函数 ====================
String ble_pack_json_data(uint8_t hr_bpm, uint8_t spo2, float acetone, const char* note) {
    char json_buffer[BLE_JSON_BUFFER_SIZE];
    
    // 计算SNR（从系统状态获取）
    const SystemState* state = system_state_get();
    float snr_db = state->hr_snr_db_x10 / 10.0;
    
    // 生成JSON字符串
    snprintf(json_buffer, sizeof(json_buffer),
        "{\"hr\":%d,\"spo2\":%d,\"acetone\":%.1f,\"note\":\"%s,SNR:%.1fdB\"}",
        hr_bpm,
        spo2,
        acetone,
        note,
        snr_db
    );
    
    return String(json_buffer);
}

// ==================== 数据发送函数 ====================
void ble_send_data(const String& json_data) {
    if (!deviceConnected) {
        return;  // 没有连接，不发送数据
    }
    
    if (json_data.length() == 0) {
        return;  // 空数据不发送
    }
    
    // 发送数据
#ifdef USE_NIMBLE
    pCharacteristic->setValue(json_data.c_str());
    pCharacteristic->notify();
#else
    pCharacteristic->setValue(json_data.c_str());
    pCharacteristic->notify();
#endif
    
    Serial.print("[BLE] 发送数据: ");
    Serial.println(json_data);
}

// ==================== 界面绘制：时钟模式 ====================
void drawClockMode() {
    struct tm timeinfo;
    
    if (!ntpSynced || !getLocalTime(&timeinfo)) {
        // 时间未同步
        display.setTextSize(1);
        display.setCursor(10, 20);
        display.print("时间未同步");
        display.setCursor(10, 32);
        display.print("BLE广播中");
    } else {
        // 大字体显示时间
        char timeStr[10];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        display.setTextSize(2);
        display.setCursor(16, 8);
        display.print(timeStr);
        
        // 小字体显示日期
        char dateStr[12];
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", 
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        display.setTextSize(1);
        display.setCursor(22, 28);
        display.print(dateStr);
        
        // 显示星期
        display.setCursor(46, 40);
        display.print(weekdayChinese[timeinfo.tm_wday]);
    }
    
    // 电池图标
    uint8_t batteryPercent = getBatteryPercent();
    drawBatteryIcon(104, 0, batteryPercent);
    display.setCursor(90, 2);
    display.printf("%d%%", batteryPercent);
    
    // 免责声明
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.print(DISCLAIMER_TEXT);
}

// ==================== 界面绘制：测量模式 ====================
void drawMeasurementMode() {
    display.setTextSize(1);
    
    // 获取系统状态
    const SystemState* state = system_state_get();
    
    // 第一行：心率和血氧
    display.setCursor(0, 0);
    display.print("心率:");
    if (state->hr_bpm > 0) {
        display.printf("%d bpm", state->hr_bpm);
    } else {
        display.print("--");
    }
    
    display.setCursor(0, 12);
    display.print("血氧:");
#ifdef DEVICE_ROLE_WRIST
    uint8_t spo2 = system_state_get_spo2();
    if (spo2 > 0) {
        display.printf("%d%%", spo2);
    } else {
        display.print("--");
    }
#else
    display.print("--");
#endif
    
    // 第二行：SNR质量
    display.setCursor(0, 24);
    display.print("SNR:");
    float snr_db = state->hr_snr_db_x10 / 10.0;
    display.printf("%.1f dB", snr_db);
    
    // BLE连接状态
    display.setCursor(0, 36);
    display.print("BLE:");
    if (deviceConnected) {
        display.print("已连接");
    } else {
        display.print("广播中");
    }
    
    // 数据发送状态
    display.setCursor(0, 48);
    display.print("数据:");
    if (deviceConnected) {
        display.print("发送中");
    } else {
        display.print("待连接");
    }
}

// ==================== 界面绘制：BLE状态模式 ====================
void drawBLEStatusMode() {
    display.setTextSize(1);
    
    display.setCursor(0, 0);
    display.print("=== BLE状态 ===");
    
    display.setCursor(0, 12);
    display.print("设备名:");
    display.print(BLE_DEVICE_NAME);
    
    display.setCursor(0, 24);
    display.print("服务UUID:");
    display.setCursor(0, 36);
    display.print(BLE_SERVICE_UUID);
    
    display.setCursor(0, 48);
    display.print("状态:");
    if (deviceConnected) {
        display.print("已连接");
    } else {
        display.print("广播中");
    }
}

// ==================== 按键处理 ====================
void handleButtons() {
    uint32_t now = millis();
    uint8_t btn1State = digitalRead(PIN_BTN1);
    uint8_t btn2State = digitalRead(PIN_BTN2);
    
    // 按键1：切换界面模式
    if (btn1State == LOW && btn1LastState == HIGH) {
        delay(DEBOUNCE_MS);
        if (digitalRead(PIN_BTN1) == LOW) {
            lastActivityTime = now;
            
            // 唤醒OLED
            if (!oledPowerOn) {
                setOLEDPower(true);
            } else {
                // 切换模式
                currentMode = (DisplayMode)((currentMode + 1) % 3);
                Serial.printf("[按键] 切换到模式: %d\n", currentMode);
            }
        }
    }
    
    btn1LastState = btn1State;
    
    // 按键2：返回时钟模式
    if (btn2State == LOW && btn2LastState == HIGH) {
        delay(DEBOUNCE_MS);
        if (digitalRead(PIN_BTN2) == LOW) {
            lastActivityTime = now;
            
            // 唤醒OLED
            if (!oledPowerOn) {
                setOLEDPower(true);
            } else {
                // 返回时钟模式
                if (currentMode != MODE_CLOCK) {
                    currentMode = MODE_CLOCK;
                    Serial.println("[按键] 返回时钟模式");
                }
            }
        }
    }
    
    btn2LastState = btn2State;
}

// ==================== 腕带主控专用函数 ====================
void wrist_setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n========================================");
    Serial.println("  糖尿病初筛腕带主控板 (ESP32-S3)");
    Serial.println(" 