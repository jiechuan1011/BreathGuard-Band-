/*
 * main_control.ino - 糖尿病初筛腕带主控板（ESP32-C3 SuperMini）
 * 
 * 功能：完整手表功能 + BLE接收检测板数据
 *   - NTP时钟同步（阿里云服务器）
 *   - 大字体时间 + 日期星期（中文）
 *   - 三种界面模式：时钟/测量/倒计时
 *   - 30秒无操作OLED自动熄屏
 *   - 低功耗模式（每秒唤醒刷新时间）
 *   - 电池电量分级显示
 * 
 * 硬件：
 *   - 0.96寸 OLED SSD1306（I2C，0x3C地址）
 *   - GPIO6：按键1（短按切换界面/长按启动10秒倒计时）
 *   - GPIO7：按键2（返回时钟界面）
 *   - GPIO2：电池电压ADC（需2:1分压电路）
 *   - BLE Central接收"DiabetesSensor"的notify数据
 * 
 * 库依赖：
 *   - Arduino-ESP32
 *   - Adafruit SSD1306
 *   - Adafruit GFX
 * 
 * 编译：开发板选ESP32C3 Dev Module
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sleep.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>

// ==================== WiFi / NTP 配置（请修改为您的WiFi信息）====================
#define WIFI_SSID           "your_ssid"        // 修改为您的WiFi名称
#define WIFI_PASS           "your_password"    // 修改为您的WiFi密码
#define NTP_SERVER          "ntp.aliyun.com"   // 阿里云NTP服务器
#define GMT_OFFSET_SEC      (8 * 3600)         // 中国时区 UTC+8
#define DAYLIGHT_OFFSET_SEC 0                  // 夏令时偏移（中国无夏令时）
#define NTP_SYNC_INTERVAL   3600000            // NTP重新同步间隔（毫秒，默认1小时）

// ==================== 引脚定义（ESP32-C3）====================
#define PIN_SDA         4   // I2C数据线（OLED）
#define PIN_SCL         5   // I2C时钟线（OLED）
#define PIN_BTN1        6   // 按键1：切换/倒计时（上拉输入，按下为LOW）
#define PIN_BTN2        7   // 按键2：返回（上拉输入，按下为LOW）
#define PIN_BAT_ADC     2   // 电池电压ADC输入（需2:1分压）

// ==================== BLE配置（UUID需与detection_sensor.ino一致）====================
#define BLE_DEVICE_NAME     "DiabetesSensor"
#define BLE_SERVICE_UUID    "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
#define BLE_CHAR_UUID       "a1b2c3d4-e5f6-4789-abcd-ef012345678a"

// ==================== OLED显示配置 ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
#define OLED_RESET      -1  // 无复位引脚

// ==================== 系统参数 ====================
#define SCREEN_TIMEOUT_MS   30000   // 30秒无操作熄屏
#define COUNTDOWN_SECONDS   10      // 倒计时默认秒数
#define DEBOUNCE_MS         50      // 按键消抖时间
#define LONG_PRESS_MS       1000    // 长按触发时间（1秒）

// ==================== 电池电压参数 ====================
#define BAT_ADC_MAX         4095    // 12位ADC最大值
#define BAT_REF_VOLTAGE     3.3     // ADC参考电压（V）
#define BAT_DIVIDER_RATIO   2.0     // 分压比（2:1分压）
#define BAT_FULL_VOLTAGE    4.2     // 满电电压
#define BAT_EMPTY_VOLTAGE   3.3     // 空电电压

// ==================== 健康数据阈值（仅用于趋势提示，非医疗诊断）====================
#define HR_HIGH_THRESHOLD       100  // 心率偏高阈值
#define HR_LOW_THRESHOLD        50   // 心率偏低阈值
#define SPO2_LOW_THRESHOLD      95   // 血氧偏低阈值
#define ACETONE_HIGH_THRESHOLD  25.0 // 丙酮偏高阈值（ppm）

// ==================== 免责声明文本 ====================
#define DISCLAIMER_TEXT         "仅供参考，建议咨询医生"

// ==================== 界面模式枚举 ====================
enum DisplayMode {
  MODE_CLOCK,       // 时钟模式（默认）
  MODE_MEASUREMENT, // 测量数据显示模式
  MODE_TIMER        // 倒计时模式
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

// 倒计时状态
uint8_t countdownRemaining = 0;
uint32_t countdownStartTime = 0;
bool countdownRunning = false;

// BLE状态
BLEClient* pBLEClient = nullptr;
BLERemoteCharacteristic* pRemoteChar = nullptr;
bool bleConnected = false;

// 接收到的健康数据（0xFF表示无效）
uint8_t dataHeartRate = 0;
uint8_t dataSpO2 = 0;
float dataAcetonePPM = -1.0;  // 负数表示无效

// 按键状态
uint8_t btn1LastState = HIGH;
uint8_t btn2LastState = HIGH;
uint32_t btn1PressTime = 0;
bool btn1LongPressed = false;

// 星期中文字符
const char* weekdayChinese[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

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

// 获取电池电量百分比（0-100）
uint8_t getBatteryPercent() {
  float voltage = getBatteryVoltage();
  if (voltage >= BAT_FULL_VOLTAGE) return 100;
  if (voltage <= BAT_EMPTY_VOLTAGE) return 0;
  return (uint8_t)(((voltage - BAT_EMPTY_VOLTAGE) / (BAT_FULL_VOLTAGE - BAT_EMPTY_VOLTAGE)) * 100);
}

// 绘制电池图标（位置x, y, 电量百分比）
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

// ==================== WiFi连接与NTP同步 ====================
void connectWiFi() {
  Serial.println("[WiFi] 开始连接...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WiFi] 连接成功");
    Serial.print("[WiFi] IP地址: ");
    Serial.println(WiFi.localIP());
    
    // 同步NTP时间
    syncNTPTime();
  } else {
    wifiConnected = false;
    Serial.println("\n[WiFi] 连接失败");
  }
}

void syncNTPTime() {
  if (!wifiConnected) return;
  
  Serial.println("[NTP] 开始时间同步...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  // 等待时间同步（最多等待5秒）
  struct tm timeinfo;
  uint8_t attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    attempts++;
  }
  
  if (getLocalTime(&timeinfo)) {
    ntpSynced = true;
    lastNtpSyncTime = millis();
    Serial.println("[NTP] 时间同步成功");
    Serial.printf("[NTP] 当前时间: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    ntpSynced = false;
    Serial.println("[NTP] 时间同步失败");
  }
}

// ==================== BLE回调：接收notify数据 ====================
static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* data, size_t length) {
  if (length < 4) return;
  
  // 数据格式：[HR, SpO2, Acetone_Hi, Acetone_Lo, 0, 0]
  dataHeartRate = (data[0] == 0xFF) ? 0 : data[0];
  dataSpO2 = (data[1] == 0xFF) ? 0 : data[1];
  
  uint16_t acetoneRaw = ((uint16_t)data[2] << 8) | data[3];
  dataAcetonePPM = (acetoneRaw == 0xFFFF) ? -1.0 : (acetoneRaw * 0.1);
  
  Serial.printf("[BLE] 收到数据 - HR:%d SpO2:%d Acetone:%.1f\n", 
                dataHeartRate, dataSpO2, dataAcetonePPM);
}

// ==================== BLE连接管理 ====================
bool connectToBLEServer(BLEAdvertisedDevice& device) {
  Serial.println("[BLE] 尝试连接到设备...");
  
  if (pBLEClient == nullptr) {
    pBLEClient = BLEDevice::createClient();
  }
  
  if (!pBLEClient->connect(device)) {
    Serial.println("[BLE] 连接失败");
    return false;
  }
  
  Serial.println("[BLE] 已连接到设备");
  
  // 获取服务
  BLERemoteService* pService = pBLEClient->getService(BLE_SERVICE_UUID);
  if (pService == nullptr) {
    Serial.println("[BLE] 未找到服务");
    pBLEClient->disconnect();
    return false;
  }
  
  // 获取特征
  pRemoteChar = pService->getCharacteristic(BLE_CHAR_UUID);
  if (pRemoteChar == nullptr) {
    Serial.println("[BLE] 未找到特征");
    pBLEClient->disconnect();
    return false;
  }
  
  // 订阅notify
  if (pRemoteChar->canNotify()) {
    pRemoteChar->registerForNotify(notifyCallback);
    Serial.println("[BLE] 已订阅notify");
  }
  
  bleConnected = true;
  return true;
}

void disconnectBLE() {
  if (pBLEClient != nullptr) {
    pBLEClient->disconnect();
  }
  bleConnected = false;
  pRemoteChar = nullptr;
}

bool scanAndConnectBLE() {
  Serial.println("[BLE] 开始扫描设备...");
  
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  
  BLEScanResults results = pScan->start(8, false);  // 扫描8秒
  
  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice device = results.getDevice(i);
    if (String(device.getName().c_str()) == BLE_DEVICE_NAME) {
      Serial.printf("[BLE] 找到目标设备: %s\n", device.getName().c_str());
      return connectToBLEServer(device);
    }
  }
  
  Serial.println("[BLE] 未找到目标设备");
  return false;
}

// ==================== 健康数据趋势判断 ====================
bool hasHighAlert() {
  if (dataHeartRate > 0 && dataHeartRate < 0xFF && dataHeartRate > HR_HIGH_THRESHOLD) return true;
  if (dataAcetonePPM >= 0 && dataAcetonePPM > ACETONE_HIGH_THRESHOLD) return true;
  return false;
}

bool hasLowAlert() {
  if (dataHeartRate > 0 && dataHeartRate < 0xFF && dataHeartRate < HR_LOW_THRESHOLD) return true;
  if (dataSpO2 > 0 && dataSpO2 < 0xFF && dataSpO2 < SPO2_LOW_THRESHOLD) return true;
  return false;
}

// ==================== 界面绘制：时钟模式 ====================
void drawClockMode() {
  struct tm timeinfo;
  
  if (!ntpSynced || !getLocalTime(&timeinfo)) {
    // 时间未同步，显示提示
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.print("时间未同步");
    display.setCursor(10, 32);
    display.print("请检查WiFi");
    
    // 显示免责声明
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.print(DISCLAIMER_TEXT);
    return;
  }
  
  // 大字体显示时间（HH:MM:SS）
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
  
  // 显示星期（中文）
  display.setCursor(46, 40);
  display.print(weekdayChinese[timeinfo.tm_wday]);
  
  // 右下角显示电池图标和电量
  uint8_t batteryPercent = getBatteryPercent();
  drawBatteryIcon(104, 0, batteryPercent);
  display.setCursor(90, 2);
  display.printf("%d%%", batteryPercent);
  
  // 显示免责声明（底部）
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(DISCLAIMER_TEXT);
}

// ==================== 界面绘制：测量模式 ====================
void drawMeasurementMode() {
  display.setTextSize(1);
  
  // 第一行：心率和血氧
  display.setCursor(0, 0);
  display.print("心率:");
  if (dataHeartRate > 0 && dataHeartRate < 0xFF) {
    display.printf("%d bpm", dataHeartRate);
  } else {
    display.print("--");
  }
  
  display.setCursor(0, 12);
  display.print("血氧:");
  if (dataSpO2 > 0 && dataSpO2 < 0xFF) {
    display.printf("%d%%", dataSpO2);
  } else {
    display.print("--");
  }
  
  // 第二行：丙酮浓度
  display.setCursor(0, 24);
  display.print("丙酮:");
  if (dataAcetonePPM >= 0) {
    display.printf("%.1f ppm", dataAcetonePPM);
  } else {
    display.print("--");
  }
  
  // BLE连接状态
  display.setCursor(0, 36);
  display.print("BLE:");
  display.print(bleConnected ? "已连接" : "未连接");
  
  // 健康趋势提示（非诊断）
  if (hasHighAlert()) {
    display.setCursor(0, 48);
    display.print("数据偏高 建议咨询医生");
  } else if (hasLowAlert()) {
    display.setCursor(0, 48);
    display.print("数据偏低 建议咨询医生");
  } else {
    // 正常状态下显示免责声明
    display.setCursor(0, 48);
    display.print(DISCLAIMER_TEXT);
  }
}

// ==================== 界面绘制：倒计时模式 ====================
void drawTimerMode() {
  display.setTextSize(2);
  display.setCursor(10, 16);
  display.print("开始测量");
  
  display.setTextSize(3);
  display.setCursor(52, 36);
  display.print(countdownRemaining);
}

// ==================== 按键处理 ====================
void handleButtons() {
  uint32_t now = millis();
  uint8_t btn1State = digitalRead(PIN_BTN1);
  uint8_t btn2State = digitalRead(PIN_BTN2);
  
  // 按键1处理（切换/倒计时）
  if (btn1State == LOW && btn1LastState == HIGH) {
    // 按键1按下
    btn1PressTime = now;
    btn1LongPressed = false;
    lastActivityTime = now;
    
    // 唤醒OLED
    if (!oledPowerOn) {
      setOLEDPower(true);
    }
  }
  
  if (btn1State == LOW && btn1LastState == LOW) {
    // 按键1持续按下
    if (!btn1LongPressed && (now - btn1PressTime >= LONG_PRESS_MS)) {
      // 长按触发：启动倒计时
      btn1LongPressed = true;
      if (currentMode != MODE_TIMER) {
        currentMode = MODE_TIMER;
        countdownRemaining = COUNTDOWN_SECONDS;
        countdownStartTime = now;
        countdownRunning = true;
        Serial.println("[按键] 长按：启动倒计时");
      }
    }
  }
  
  if (btn1State == HIGH && btn1LastState == LOW) {
    // 按键1释放
    if (!btn1LongPressed && (now - btn1PressTime >= DEBOUNCE_MS)) {
      // 短按触发：切换界面
      if (currentMode == MODE_CLOCK) {
        currentMode = MODE_MEASUREMENT;
        Serial.println("[按键] 短按：切换到测量模式");
      } else if (currentMode == MODE_MEASUREMENT) {
        currentMode = MODE_CLOCK;
        Serial.println("[按键] 短按：切换到时钟模式");
      }
    }
  }
  
  btn1LastState = btn1State;
  
  // 按键2处理（返回时钟）
  if (btn2State == LOW && btn2LastState == HIGH) {
    // 按键2按下
    lastActivityTime = now;
    
    // 唤醒OLED
    if (!oledPowerOn) {
      setOLEDPower(true);
    } else {
      // 返回时钟模式
      if (currentMode != MODE_CLOCK) {
        currentMode = MODE_CLOCK;
        countdownRunning = false;
        Serial.println("[按键] 按键2：返回时钟模式");
      }
    }
  }
  
  btn2LastState = btn2State;
}

// ==================== 倒计时处理 ====================
void handleCountdown() {
  if (!countdownRunning) return;
  
  uint32_t now = millis();
  uint32_t elapsed = (now - countdownStartTime) / 1000;
  
  if (elapsed >= COUNTDOWN_SECONDS) {
    // 倒计时结束
    countdownRunning = false;
    currentMode = MODE_MEASUREMENT;
    Serial.println("[倒计时] 结束，切换到测量模式");
    
    // 尝试连接BLE（如果未连接）
    if (!bleConnected) {
      scanAndConnectBLE();
    }
  } else {
    // 更新剩余时间
    countdownRemaining = COUNTDOWN_SECONDS - elapsed;
  }
}

// ==================== 腕带主控专用函数声明 ====================
// 这些函数在main.cpp中被调用

void wrist_setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========================================");
  Serial.println("  糖尿病初筛腕带主控板");
  Serial.println("  Diabetes Screening Main Control");
  Serial.println("========================================\n");
  
  // 初始化按键引脚
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  Serial.println("[Init] 按键初始化完成");
  
  // 初始化I2C和OLED
  Wire.begin(PIN_SDA, PIN_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[Init] OLED初始化失败！");
    while (1);  // 停止运行
  }
  Serial.println("[Init] OLED初始化完成");
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 28);
  display.print("   正在启动...");
  display.display();
  
  // 连接WiFi和同步NTP
  connectWiFi();
  
  // 初始化BLE
  BLEDevice::init("");
  Serial.println("[Init] BLE初始化完成");
  
  // 初始化完成
  lastActivityTime = millis();
  display.clearDisplay();
  display.setCursor(0, 28);
  display.print(wifiConnected ? "   初始化完成" : "  WiFi未连接");
  display.display();
  delay(1000);
  
  Serial.println("[Init] 系统启动完成\n");
}

// ==================== Loop主循环 ====================
void loop() {
  uint32_t now = millis();
  
  // 处理按键
  handleButtons();
  
  // 处理倒计时
  handleCountdown();
  
  // 检查OLED超时熄屏
  if (oledPowerOn && (now - lastActivityTime >= SCREEN_TIMEOUT_MS)) {
    setOLEDPower(false);
    Serial.println("[OLED] 超时熄屏");
  }
  
  // 定期同步NTP时间
  if (wifiConnected && ntpSynced && (now - lastNtpSyncTime >= NTP_SYNC_INTERVAL)) {
    syncNTPTime();
  }
  
  // BLE连接管理
  if (pBLEClient != nullptr && !pBLEClient->isConnected()) {
    disconnectBLE();
    Serial.println("[BLE] 连接断开");
  }
  
  // 在测量模式下自动尝试连接BLE
  if (!bleConnected && currentMode == MODE_MEASUREMENT) {
    static uint32_t lastScanTime = 0;
    if (now - lastScanTime >= 10000) {  // 每10秒尝试一次
      lastScanTime = now;
      scanAndConnectBLE();
    }
  }
  
  // 刷新OLED显示
  if (oledPowerOn) {
    display.clearDisplay();
    
    switch (currentMode) {
      case MODE_CLOCK:
        drawClockMode();
        break;
      case MODE_MEASUREMENT:
        drawMeasurementMode();
        break;
      case MODE_TIMER:
        drawTimerMode();
        break;
    }
    
    display.display();
  }
  
  // 低功耗策略：时钟模式且OLED开启时，使用light sleep每秒刷新
  if (currentMode == MODE_CLOCK && oledPowerOn && !countdownRunning && !bleConnected) {
    esp_sleep_enable_timer_wakeup(1000000);  // 1秒后唤醒
    esp_sleep_enable_ext1_wakeup((1ULL << PIN_BTN1) | (1ULL << PIN_BTN2), ESP_EXT1_WAKEUP_ANY_LOW);
    esp_light_sleep_start();
  } else {
    delay(100);  // 其他模式下正常延迟
  }
}
