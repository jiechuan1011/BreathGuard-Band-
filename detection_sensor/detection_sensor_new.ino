 * detection_sensor.ino - 糖尿病初筛检测板（ESP32-C3 SuperMini）
 * 分时采集状态机：PHASE_HR_SPO2 → PHASE_HEAT → PHASE_ACETONE → PHASE_SLEEP
 *
 * 功能：分时采集 MAX30102（心率/血氧 30 秒）→ SnO₂ 加热 60 秒 → 采集丙酮浓度 5 秒；
 *       BLE Server 发送 JSON 格式数据，每轮采集后进入 light sleep 5 分钟。
 *
 * 硬件：
 *   - MAX30102（I2C）：心率、血氧
 *   - SnO₂ 气敏传感器：MOSFET 加热 60s，AD623 放大 + RC 低通滤波，ADC 读取
 *   - ESP32-C3 SuperMini：BLE、ADC、GPIO
 *
 * 库：Arduino-ESP32、BLEDevice、Wire、math.h
 * 编译：开发板选 ESP32C3（如 Lolin C3 Mini / ESP32-C3-DevModule），Arduino-ESP32 核心。
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_sleep.h>
#include "algorithm/hr_algorithm.h"
#include "drivers/gas_driver.h"
#include "config/pin_config.h"  // 使用统一的引脚配置

// ==================== 引脚定义（ESP32-C3） ====================
#ifndef PIN_SDA
#define PIN_SDA           4   // I2C SDA（MAX30102）
#endif
#ifndef PIN_SCL
#define PIN_SCL           5   // I2C SCL（MAX30102）
#endif
#ifndef PIN_GAS_HEATER
#define PIN_GAS_HEATER    9   // SnO₂ 加热 MOSFET 栅极（PWM控制）
#endif
#ifndef PIN_AO3400_GATE
#define PIN_AO3400_GATE  11   // AO3400 门控（通过 1kΩ 电阻连接到 MCU）
#endif
#ifndef PIN_ACETONE_ADC
#define PIN_ACETONE_ADC   PIN_AD623_OUT  // AD623 输出 → RC 滤波 → ADC（ESP32-C3 ADC1）
#endif

// ==================== BLE UUID 定义 ====================
#define BLE_DEVICE_NAME       "DiabetesSensor"
#define BLE_SERVICE_UUID      "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
#define BLE_CHAR_DATA_UUID    "a1b2c3d4-e5f6-4789-abcd-ef012345678a"  // JSON数据
#define BLE_CHAR_ERROR_UUID   "a1b2c3d4-e5f6-4789-abcd-ef012345678e"  // 错误码

// ==================== 分时阶段配置 ====================
#define PHASE_HR_SPO2_MS      30000   // 阶段1：MAX30102 采集 30 秒
#define PHASE_HEAT_MS         60000   // 阶段2：加热 60 秒
#define PHASE_ACETONE_MS      5000    // 阶段3：采集丙酮浓度 5 秒
#define SLEEP_DURATION_US     300000000 // 低功耗睡眠时长 300秒（5分钟）

// ==================== 全局对象与变量 ====================
BLEServer* pServer = nullptr;
BLECharacteristic* pCharData = nullptr;
BLECharacteristic* pCharError = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// 分时状态机
enum Phase { 
  PHASE_HR_SPO2,    // 心率血氧采集
  PHASE_HEAT,       // SnO₂加热
  PHASE_ACETONE,    // 丙酮浓度采集
  PHASE_SLEEP       // 低功耗睡眠
};
Phase curPhase = PHASE_HR_SPO2;
uint32_t phaseStartMs = 0;

// 测量数据
uint8_t heartRate = 0;        // 心率（BPM），0表示无效
uint8_t spO2 = 0;             // 血氧饱和度（%），0表示无效
float acetonePpm = 0.0;       // 丙酮浓度（ppm），0表示无效
uint8_t errorCode = 0;        // 错误码：0=无错误，1=MAX30102初始化失败，2=ADC异常，3=加热异常

// 心率采集相关
uint32_t lastHrUpdateMs = 0;
uint8_t hrValidCount = 0;
uint8_t hrBuffer[10];         // 心率缓冲区，用于平均

// 丙酮采集相关
uint32_t lastAcetoneReadMs = 0;

// ==================== BLE 回调 ====================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* p) override {
    deviceConnected = true;
    Serial.println("[BLE] 设备已连接");
  }
  
  void onDisconnect(BLEServer* p) override {
    deviceConnected = false;
    Serial.println("[BLE] 设备已断开");
    // 延迟300ms后重新开始广播
    delay(300);
    pServer->startAdvertising();
    Serial.println("[BLE] 重新开始广播");
  }
};

// ==================== 辅助函数 ====================

// 创建JSON格式数据
String createJsonData() {
  // 构建JSON字符串：{"hr":xx, "spo2":xx, "acetone":xx.x, "note":"仅供参考"}
  String json = "{";
  json += "\"hr\":";
  json += (heartRate == 0) ? "null" : String(heartRate);
  json += ",\"spo2\":";
  json += (spO2 == 0) ? "null" : String(spO2);
  json += ",\"acetone\":";
  json += (acetonePpm <= 0) ? "null" : String(acetonePpm, 1);
  json += ",\"note\":\"仅供参考\"";
  json += "}";
  return json;
}

// 通过BLE发送JSON数据
void sendJsonData() {
  if (!deviceConnected) return;
  
  String jsonData = createJsonData();
  pCharData->setValue(jsonData.c_str());
  pCharData->notify();
  
  Serial.print("[BLE] JSON数据已发送: ");
  Serial.println(jsonData);
}

// 发送错误码
void sendErrorCode() {
  if (!deviceConnected || errorCode == 0) return;
  
  pCharError->setValue(&errorCode, 1);
  pCharError->notify();
  
  Serial.printf("[BLE] 错误码已发送: %d\n", errorCode);
}

// 进入低功耗睡眠
void enterLightSleep() {
  Serial.println("[低功耗] 进入5分钟低功耗睡眠");
  
  // 配置唤醒源：定时器唤醒
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
  
  // 配置唤醒源：GPIO唤醒（呼吸传感器，如果需要）
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_X, HIGH);
  
  // 进入light sleep
  esp_light_sleep_start();
  
  Serial.println("[低功耗] 唤醒，开始新一轮采集");
}

// 计算平均心率
uint8_t calculateAverageHeartRate() {
  if (hrValidCount == 0) return 0;
  
  uint16_t sum = 0;
  for (uint8_t i = 0; i < hrValidCount; i++) {
    sum += hrBuffer[i];
  }
  return sum / hrValidCount;
}

// ==================== 状态机处理函数 ====================

// PHASE_HR_SPO2: 心率血氧采集阶段
void handlePhaseHrSpo2() {
  uint32_t now = millis();
  uint32_t elapsed = now - phaseStartMs;
  
  // 检查阶段是否结束
  if (elapsed >= PHASE_HR_SPO2_MS) {
    // 计算最终平均心率
    heartRate = calculateAverageHeartRate();
    
    // 发送最终数据
    sendJsonData();
    
    // 切换到加热阶段
    curPhase = PHASE_HEAT;
    phaseStartMs = now;
    
    // 初始化气体传感器（开始加热）
    if (!gas_init()) {
      Serial.println("[状态机] 气体传感器初始化失败");
      errorCode = 3;  // 加热异常
      sendErrorCode();
    } else {
      Serial.println("[状态机] 心率血氧采集完成，开始加热SnO₂");
    }
    
    return;
  }
  
  // 每10ms更新心率数据
  if (now - lastHrUpdateMs >= 10) {
    lastHrUpdateMs = now;
    
    // 更新心率算法
    int status = hr_algorithm_update();
    if (status == HR_SUCCESS) {
      // 每2秒计算一次心率
      static uint16_t hrCounter = 0;
      if (++hrCounter >= 200) {  // 10ms × 200 = 2秒
        hrCounter = 0;
        
        int calcStatus;
        uint8_t bpm = hr_calculate_bpm(&calcStatus);
        
        if (calcStatus == HR_SUCCESS && bpm >= 40 && bpm <= 180) {
          // 存储到心率缓冲区
          if (hrValidCount < 10) {
            hrBuffer[hrValidCount++] = bpm;
          } else {
            // 缓冲区已满，滚动更新
            for (uint8_t i = 0; i < 9; i++) {
              hrBuffer[i] = hrBuffer[i + 1];
            }
            hrBuffer[9] = bpm;
          }
          
          // 更新当前心率（使用最新值）
          heartRate = bpm;
          
          // 每10秒发送一次实时数据
          static uint8_t sendCounter = 0;
          if (++sendCounter >= 5) {  // 2秒 × 5 = 10秒
            sendCounter = 0;
            sendJsonData();
          }
        }
      }
    }
  }
}

// PHASE_HEAT: SnO₂加热阶段
void handlePhaseHeat() {
  uint32_t now = millis();
  uint32_t elapsed = now - phaseStartMs;
  
  // 检查阶段是否结束
  if (elapsed >= PHASE_HEAT_MS) {
    // 切换到丙酮采集阶段
    curPhase = PHASE_ACETONE;
    phaseStartMs = now;
    lastAcetoneReadMs = now;
    
    Serial.println("[状态机] SnO₂加热完成，开始采集丙酮浓度");
    return;
  }
  
  // 加热阶段，每10秒打印一次状态
  static uint32_t lastStatusPrint = 0;
  if (now - lastStatusPrint >= 10000) {
    lastStatusPrint = now;
    float remaining = (PHASE_HEAT_MS - elapsed) / 1000.0;
    float dutyCycle = gas_get_heater_duty_cycle();
    bool warmedUp = gas_is_warmed_up();
    
    Serial.printf("[状态机] 加热中... 剩余: %.1f秒, 占空比: %.0f%%, 预热完成: %s\n",
                  remaining, dutyCycle, warmedUp ? "是" : "否");
  }
}

// PHASE_ACETONE: 丙酮浓度采集阶段
void handlePhaseAcetone() {
  uint32_t now = millis();
  uint32_t elapsed = now - phaseStartMs;
  
  // 检查阶段是否结束
  if (elapsed >= PHASE_ACETONE_MS) {
    // 发送最终丙酮数据
    sendJsonData();
    
    // 切换到睡眠阶段
    curPhase = PHASE_SLEEP;
    Serial.println("[状态机] 丙酮采集完成，准备进入低功耗睡眠");
    return;
  }
  
  // 每秒采集一次丙酮浓度
  if (now - lastAcetoneReadMs >= 1000) {
    lastAcetoneReadMs = now;
    
    float voltage_mv, conc_ppm;
    if (gas_read(&voltage_mv, &conc_ppm)) {
      acetonePpm = conc_ppm;
      
      Serial.printf("[状态机] 丙酮采集: %.1f mV -> %.2f ppm\n", voltage_mv, conc_ppm);
      
      // 发送实时数据
      sendJsonData();
    } else {
      Serial.println("[状态机] 丙酮采集失败（可能仍在预热中）");
    }
  }
}

// PHASE_SLEEP: 低功耗睡眠阶段
void handlePhaseSleep() {
  // 发送最终完整数据
  sendJsonData();
  
  // 重置测量数据
  heartRate = 0;
  spO2 = 0;  // 注意：当前未实现血氧计算，实际项目中需要添加
  acetonePpm = 0.0;
  hrValidCount = 0;
  
  // 进入低功耗睡眠
  enterLightSleep();
  
  // 睡眠结束后重新开始采集
  curPhase = PHASE_HR_SPO2;
  phaseStartMs = millis();
  
  // 重新初始化心率算法
  hr_algorithm_init();
  
  Serial.println("[状态机] 睡眠结束，开始新一轮心率血氧采集");
}

// ==================== setup 函数 ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[detection_sensor] 糖尿病初筛检测板启动");
  
  // 初始化I2C
  Wire.begin(PIN_SDA, PIN_SCL);
  
  // 初始化心率算法
  hr_algorithm_init();
  
  // 初始化MAX30102（通过hr_driver）
  // 注意：hr_driver.h中已经包含了MAX30102初始化
  
  // 初始化BLE
  BLEDevice::init(BLE_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // 创建服务
  BLEService* pService = pServer->createService(BLE_SERVICE_UUID);
  
  // 创建数据特征（JSON格式）
  pCharData = pService->createCharacteristic(
    BLE_CHAR_DATA_UUID,
    BLECharacteristic::PROPERTY_READ | 
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  pCharData->addDescriptor(new BLE2902());
  
  // 创建错误码特征
  pCharError = pService->createCharacteristic(
    BLE_CHAR_ERROR_UUID,
    BLECharacteristic::PROPERTY_READ | 
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharError->addDescriptor(new BLE2902());
  
  // 启动服务
  pService->start();
  
  // 配置广播
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  
  // 开始广播
  BLEDevice::startAdvertising();
  
  Serial.println("[BLE] 广播已启动，设备名: " BLE_DEVICE_NAME);
  
  // 初始化状态机
  curPhase = PHASE_HR_SPO2;
  phaseStartMs = millis();
  lastHrUpdateMs = millis();
  
  Serial.println("[状态机] 开始心率血氧采集（30秒）");
}

// ==================== loop 函数 ====================
void loop() {
  // 处理BLE连接状态变化
  if (!deviceConnected && oldDeviceConnected) {
    delay(300);  // 给蓝牙栈一点时间
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  // 根据当前阶段调用相应的处理函数
  switch (curPhase) {
    case PHASE_HR_SPO2:
      handlePhaseHrSpo2();
      break;
      
    case PHASE_HEAT:
      handlePhaseHeat();
      break;
      
    case PHASE_ACETONE:
      handlePhaseAcetone();
      break;
      
    case PHASE_SLEEP:
      handlePhaseSleep();
      break;
  }
  
  // 短延迟，避免占用过多CPU
  delay(10);
}