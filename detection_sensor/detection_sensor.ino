/*
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
#include "config/pin_config.h"  // 统一引脚配置

// ==================== 引脚定义（ESP32-C3） ====================
#ifndef PIN_SDA
#define PIN_SDA           4   // I2C SDA（MAX30102）
#endif
#ifndef PIN_SCL
#define PIN_SCL           5   // I2C SCL（MAX30102）
#endif
#ifndef PIN_GAS_HEATER
#define PIN_GAS_HEATER    8   // MQ-138 加热 MOSFET 栅极（高电平开启）
#endif
#ifndef PIN_ACETONE_ADC
#define PIN_ACETONE_ADC   PIN_AD623_OUT  // AD623 输出 → RC 滤波 → ADC（ESP32-C3 ADC1）
#endif

// ==================== BLE 自定义 128bit UUID ====================
#define BLE_DEVICE_NAME       "DiabetesSensor"
#define BLE_SERVICE_UUID      "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
#define BLE_CHAR_HR_UUID      "a1b2c3d4-e5f6-4789-abcd-ef012345678b"
#define BLE_CHAR_SPO2_UUID    "a1b2c3d4-e5f6-4789-abcd-ef012345678c"
#define BLE_CHAR_ACETONE_UUID "a1b2c3d4-e5f6-4789-abcd-ef012345678d"
#define BLE_CHAR_ERROR_UUID   "a1b2c3d4-e5f6-4789-abcd-ef012345678e"

// ==================== MAX30102 寄存器（与 hr_driver 一致） ====================
#define MAX30102_I2C_ADDR     0x57
#define REG_INTR_STATUS_1     0x00
#define REG_FIFO_WR_PTR       0x04
#define REG_OVF_COUNTER       0x05
#define REG_FIFO_RD_PTR       0x06
#define REG_FIFO_DATA         0x07
#define REG_FIFO_CONFIG       0x08
#define REG_MODE_CONFIG       0x09
#define REG_SPO2_CONFIG       0x0A
#define REG_LED1_PA           0x0C   // RED
#define REG_LED2_PA           0x0D   // IR
#define REG_PART_ID           0xFF

#define REG_TEMP_CONFIG       0x21
#define REG_TEMP_INTEGER      0x1F

// ==================== MAX30102 参数 ====================
#define HR_SAMPLE_RATE        100
#define HR_PULSE_WIDTH        411
#define HR_LED_CURRENT        0x24
#define HR_FIFO_AVERAGE       4
#define MAX30102_RETRY        3

// ==================== SnO₂ / 丙酮 校准参数（可实测调整） ====================
#define GAS_HEAT_MS           60000   // 加热时长 60 秒
#define GAS_ADC_REF_MV        3300    // ESP32 3.3V 参考
#define GAS_ADC_BITS          4095    // 12bit
#define GAS_BASELINE_MV       500.0f  // 空气中基线电压 (mV)，需校准
#define GAS_SCALE_PPM_PER_MV  200.0f  // ppm/(mV)，需校准
#define GAS_SAMPLE_N          20      // 采集样本数（用于运动干扰检测）
#define GAS_VARIANCE_THRESHOLD 100.0f // 方差阈值（mV^2），超过则视为运动干扰

// ==================== 分时阶段 ====================
#define PHASE_HR_SPO2_MS      30000   // 阶段1：MAX30102 采集 30 秒
#define PHASE_HEAT_MS         60000   // 阶段2：加热 60 秒
#define PHASE_ACETONE_MS      5000    // 阶段3：读丙酮并稳定约 5 秒
#define SLEEP_DURATION_US     300000000 // 低功耗睡眠时长 300秒（5分钟）

// ==================== 全局对象与变量 ====================
BLEServer* pServer = nullptr;
BLECharacteristic* pCharHR = nullptr;
BLECharacteristic* pCharSpO2 = nullptr;
BLECharacteristic* pCharAcetone = nullptr;
BLECharacteristic* pCharError = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// 分时状态机
enum Phase { PHASE_HR_SPO2, PHASE_HEAT, PHASE_ACETONE, PHASE_SLEEP };
Phase curPhase = PHASE_HR_SPO2;
uint32_t phaseStartMs = 0;

// 当前测量值（0xFF、0xFFFF 表示无效）
uint8_t lastHR = 0;           // 0 或 1–254 有效
uint8_t lastSpO2 = 0;         // 0 或 1–100
float lastAcetonePpm = -1.0;  // 负数表示无效
uint8_t errorCode = 0;        // 错误码：0=无错误，1=MAX30102初始化失败，2=SnO₂ ADC异常

// MAX30102 心率/血氧 简易计算用缓冲
#define HR_BUF_SIZE  64
static int16_t irBuf[HR_BUF_SIZE];
static int16_t redBuf[HR_BUF_SIZE];
static uint8_t hrBufIdx = 0;
static bool hrBufFilled = false;

// 丙酮 ADC 样本缓冲区（用于运动干扰校正）
static uint16_t gasSamples[GAS_SAMPLE_N];
static uint8_t gasSampleIdx = 0;

// ==================== I2C 封装 ====================
static bool i2cWrite(uint8_t reg, uint8_t val) {
  for (uint8_t r = 0; r < MAX30102_RETRY; r++) {
    Wire.beginTransmission(MAX30102_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    if (Wire.endTransmission() == 0) return true;
    delay(2);
  }
  return false;
}

static bool i2cRead(uint8_t reg, uint8_t* buf, uint8_t len) {
  for (uint8_t r = 0; r < MAX30102_RETRY; r++) {
    Wire.beginTransmission(MAX30102_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) { delay(2); continue; }
    Wire.requestFrom(MAX30102_I2C_ADDR, len);
    if (Wire.available() >= (int)len) {
      for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
      return true;
    }
    delay(2);
  }
  return false;
}

// ==================== MAX30102 初始化 ====================
bool max30102_init() {
  Wire.begin(PIN_SDA, PIN_SCL);
  if (!i2cWrite(REG_MODE_CONFIG, 0x40)) return false;  // 软复位
  delay(100);
  uint8_t id;
  if (!i2cRead(REG_PART_ID, &id, 1) || id != 0x15) return false;

  uint8_t fifo = (HR_FIFO_AVERAGE << 5) | (1 << 4);  // rollover
  if (!i2cWrite(REG_FIFO_CONFIG, fifo)) return false;
  if (!i2cWrite(REG_MODE_CONFIG, 0x03)) return false; // SpO2+HR

  uint8_t sr = (HR_SAMPLE_RATE == 100) ? 1 : 2;
  uint8_t pw = 3;  // 411us
  uint8_t spo2 = (0 << 5) | (sr << 2) | pw;
  if (!i2cWrite(REG_SPO2_CONFIG, spo2)) return false;
  if (!i2cWrite(REG_LED1_PA, HR_LED_CURRENT)) return false;
  if (!i2cWrite(REG_LED2_PA, HR_LED_CURRENT)) return false;
  i2cWrite(REG_FIFO_WR_PTR, 0);
  i2cWrite(REG_FIFO_RD_PTR, 0);
  return true;
}

// 读一帧 FIFO（red + ir 各 18bit 右对齐）
bool max30102_read_sample(int32_t* red, int32_t* ir) {
  uint8_t d[6];
  if (!i2cRead(REG_FIFO_DATA, d, 6)) return false;
  *red = ((int32_t)d[0] << 16 | (int32_t)d[1] << 8 | d[2]) >> 6;
  *ir  = ((int32_t)d[3] << 16 | (int32_t)d[4] << 8 | d[5]) >> 6;
  return true;
}

// 简易心率：峰峰间隔 -> BPM（样本间隔 10ms，100Hz）
uint8_t compute_bpm_simple(int16_t* buf, uint8_t n) {
  int32_t sum = 0, sum2 = 0;
  for (uint8_t i = 0; i < n; i++) { sum += buf[i]; sum2 += (int32_t)buf[i]*buf[i]; }
  int32_t mean = sum / (int32_t)n;
  int32_t var = (sum2 / (int32_t)n) - (mean * mean);
  if (var < 0) var = 0;
  int32_t th = mean + (int32_t)(sqrt((double)var) / 2);  // 自适应阈值
  uint8_t peaks[8], np = 0;
  for (uint8_t i = 1; i < n - 1 && np < 8; i++) {
    if (buf[i] > buf[i-1] && buf[i] > buf[i+1] && buf[i] > (int16_t)th)
      peaks[np++] = i;
  }
  if (np < 2) return 0;
  uint16_t tot = 0;
  for (uint8_t i = 1; i < np; i++) tot += (peaks[i] - peaks[i-1]);
  uint16_t avg = tot / (np - 1);  // 样本数/心跳
  if (avg == 0) return 0;
  uint16_t bpm = 6000 / avg;  // 60*100 / avg
  if (bpm < 40 || bpm > 180) return 0;
  return (uint8_t)bpm;
}

// 简易 SpO2：R = (AC_red/DC_red)/(AC_ir/DC_ir)，SpO2 ≈ 110 - 25*R（经验式，仅趋势）
uint8_t compute_spo2_simple(int16_t* red, int16_t* ir, uint8_t n) {
  int32_t dr = 0, di = 0, ar = 0, ai = 0;
  for (uint8_t i = 0; i < n; i++) { dr += red[i]; di += ir[i]; }
  int32_t dcR = dr / (int32_t)n, dcI = di / (int32_t)n;
  for (uint8_t i = 0; i < n; i++) {
    int32_t rr = (int32_t)red[i] - dcR, ii = (int32_t)ir[i] - dcI;
    ar += rr * rr; ai += ii * ii;
  }
  uint32_t acR = (uint32_t)(ar / (int32_t)n), acI = (uint32_t)(ai / (int32_t)n);
  if (acI == 0 || dcI == 0 || dcR == 0) return 0;
  float r = (sqrt(acR) / (float)dcR) / (sqrt(acI) / (float)dcI);
  float s = 110.0f - 25.0f * r;
  if (s < 70.0f) s = 70.0f;
  if (s > 100.0f) s = 100.0f;
  return (uint8_t)(s + 0.5f);
}

// ==================== SnO₂ 丙酮：加热控制 ====================
void gas_heater_on()  { digitalWrite(PIN_GAS_HEATER, HIGH); }
void gas_heater_off() { digitalWrite(PIN_GAS_HEATER, LOW); }

// 读取一次ADC原始值（0-4095）
uint16_t read_adc_raw() {
  return analogRead(PIN_ACETONE_ADC) & 0xFFF;
}

// 将ADC原始值转换为电压（mV）
float adc_raw_to_mv(uint16_t raw) {
  return (raw / (float)GAS_ADC_BITS) * GAS_ADC_REF_MV;
}

// 运动干扰检测：计算样本方差
float compute_variance(uint16_t* samples, uint8_t n) {
  if (n < 2) return 0.0f;
  float sum = 0.0f, sum2 = 0.0f;
  for (uint8_t i = 0; i < n; i++) {
    sum += samples[i];
    sum2 += (float)samples[i] * samples[i];
  }
  float mean = sum / n;
  return (sum2 / n) - (mean * mean);
}

// 中值滤波（冒泡排序取中间值）
uint16_t median_filter(uint16_t* samples, uint8_t n) {
  uint16_t temp[GAS_SAMPLE_N];
  memcpy(temp, samples, n * sizeof(uint16_t));
  
  // 冒泡排序
  for (uint8_t i = 0; i < n - 1; i++) {
    for (uint8_t j = 0; j < n - i - 1; j++) {
      if (temp[j] > temp[j + 1]) {
        uint16_t swap = temp[j];
        temp[j] = temp[j + 1];
        temp[j + 1] = swap;
      }
    }
  }
  return temp[n / 2];
}

// 采集并处理丙酮信号，返回电压（mV）
float measure_acetone_voltage() {
  // 采集20个样本
  for (gasSampleIdx = 0; gasSampleIdx < GAS_SAMPLE_N; gasSampleIdx++) {
    gasSamples[gasSampleIdx] = read_adc_raw();
    delay(5);  // 采样间隔5ms
  }
  
  // 计算方差判断是否有运动干扰
  float variance = compute_variance(gasSamples, GAS_SAMPLE_N);
  float voltage_mv;
  
  if (variance > GAS_VARIANCE_THRESHOLD) {
    // 方差过大，使用中值滤波
    uint16_t median = median_filter(gasSamples, GAS_SAMPLE_N);
    voltage_mv = adc_raw_to_mv(median);
    Serial.printf("[SnO₂] 运动干扰检测，使用中值滤波: %.1f mV (方差: %.1f)\n", voltage_mv, variance);
  } else {
    // 方差正常，使用平均值
    uint32_t sum = 0;
    for (uint8_t i = 0; i < GAS_SAMPLE_N; i++) sum += gasSamples[i];
    uint16_t avg = sum / GAS_SAMPLE_N;
    voltage_mv = adc_raw_to_mv(avg);
    Serial.printf("[SnO₂] 正常采集，使用平均值: %.1f mV (方差: %.1f)\n", voltage_mv, variance);
  }
  
  return voltage_mv;
}

// 丙酮浓度转换：acetone_ppm = (voltage - offset) * sensitivity
float acetone_ppm_from_mv(float v_mv) {
  // 线性标定：ppm = max(0, (V_mV - baseline) * scale / 1000)
  float ppm = (v_mv - GAS_BASELINE_MV) * GAS_SCALE_PPM_PER_MV / 1000.0f;
  return (ppm > 0.0f) ? ppm : 0.0f;
}

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

// ==================== BLE 发送数据 ====================
void send_ble_data() {
  if (!deviceConnected) return;
  
  // 发送心率
  if (pCharHR) {
    uint8_t hrVal = (lastHR == 0) ? 0xFF : lastHR;
    pCharHR->setValue(&hrVal, 1);
    pCharHR->notify();
  }
  
  // 发送血氧
  if (pCharSpO2) {
    uint8_t spo2Val = (lastSpO2 == 0) ? 0xFF : lastSpO2;
    pCharSpO2->setValue(&spo2Val, 1);
    pCharSpO2->notify();
  }
  
  // 发送丙酮浓度（float，4字节）
  if (pCharAcetone) {
    float acetoneVal = (lastAcetonePpm < 0) ? -1.0f : lastAcetonePpm;
    pCharAcetone->setValue((uint8_t*)&acetoneVal, sizeof(float));
    pCharAcetone->notify();
  }
  
  // 发送错误码（如果有）
  if (pCharError && errorCode != 0) {
    pCharError->setValue(&errorCode, 1);
    pCharError->notify();
  }
  
  Serial.printf("[BLE] 数据已发送 - HR:%d SpO2:%d Acetone:%.1f ppm\n", 
                lastHR, lastSpO2, lastAcetonePpm);
}

// ==================== 低功耗睡眠 ====================
void enter_light_sleep(uint64_t sleep_us) {
  Serial.printf("[低功耗] 进入睡眠，时长: %.1f 秒\n", sleep_us / 1000000.0);
  esp_sleep_enable_timer_wakeup(sleep_us);
  esp_light_sleep_start();
  Serial.println("[低功耗] 唤醒");
}

// ==================== 检测模块专用函数声明 ====================
// 这些函数在main.cpp中被调用

void detector_setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[detection_sensor] DiabetesSensor 检测板启动");
  
  // 初始化引脚
  pinMode(PIN_GAS_HEATER, OUTPUT);
  gas_heater_off();
  
  // 初始化MAX30102
  bool max30102_ok = max30102_init();
  if (!max30102_ok) {
    Serial.println("[detection_sensor] MAX30102 初始化失败");
    errorCode = 1;  // 设置错误码
  } else {
    Serial.println("[detection_sensor] MAX30102 初始化成功");
  }
  
  // 初始化BLE
  BLEDevice::init(BLE_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // 创建服务
  BLEService* pSvc = pServer->createService(BLE_SERVICE_UUID);
  
  // 创建特征值：心率
  pCharHR = pSvc->createCharacteristic(
    BLE_CHAR_HR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharHR->addDescriptor(new BLE2902());
  
  // 创建特征值：血氧
  pCharSpO2 = pSvc->createCharacteristic(
    BLE_CHAR_SPO2_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharSpO2->addDescriptor(new BLE2902());
  
  // 创建特征值：丙酮浓度
  pCharAcetone = pSvc->createCharacteristic(
    BLE_CHAR_ACETONE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharAcetone->addDescriptor(new BLE2902());
  
  // 创建特征值：错误码
  pCharError = pSvc->createCharacteristic(
    BLE_CHAR_ERROR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharError->addDescriptor(new BLE2902());
  
  // 启动服务和广播
  pSvc->start();
  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(BLE_SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  pAdv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("[detection_sensor] BLE 广播已启动，设备名: " BLE_DEVICE_NAME);
  
  // 初始化状态
  curPhase = PHASE_HR_SPO2;
  phaseStartMs = millis();
  memset(irBuf, 0, sizeof(irBuf));
  memset(redBuf, 0, sizeof(redBuf));
  hrBufIdx = 0;
  hrBufFilled = false;
  
  // 发送初始错误码（如果有）
  if (errorCode != 0) {
    send_ble_data();
  }
}

// ==================== loop：分时状态机 ====================
void loop() {
  uint32_t now = millis();
  uint32_t elapsed = now - phaseStartMs;
  
  // 处理BLE连接状态变化
  if (!deviceConnected && oldDeviceConnected) {
    delay(300);  // 给蓝牙栈一点时间
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  switch (curPhase) {
    case PHASE_HR_SPO2: {
      if (elapsed >= PHASE_HR_SPO2_MS) {
        // 30秒采集结束，进入加热阶段
        curPhase = PHASE_HEAT;
        phaseStartMs = now;
        gas_heater_on();
        Serial.println("[分时] MAX30102采集完成，开始加热SnO₂");
        
        // 发送最后一次心率血氧数据
        if (hrBufFilled) {
          lastHR = compute_bpm_simple(irBuf, HR_BUF_SIZE);
          lastSpO2 = compute_spo2_simple(redBuf, irBuf, HR_BUF_SIZE);
          send_ble_data();
        }
        break;
      }
      
      // 持续采集MAX30102数据
      int32_t red, ir;
      if (max30102_read_sample(&red, &ir)) {
        irBuf[hrBufIdx] = (int16_t)(ir >> 2);
        redBuf[hrBufIdx] = (int16_t)(red >> 2);
        hrBufIdx = (hrBufIdx + 1) % HR_BUF_SIZE;
        if (hrBufIdx == 0) hrBufFilled = true;
        
        // 每采集满一个缓冲区就计算并发送一次
        if (hrBufFilled) {
          lastHR = compute_bpm_simple(irBuf, HR_BUF_SIZE);
          lastSpO2 = compute_spo2_simple(redBuf, irBuf, HR_BUF_SIZE);
          send_ble_data();
        }
      }
      delay(8);  // 模拟100Hz采样率
      break;
    }
    
    case PHASE_HEAT: {
      if (elapsed >= PHASE_HEAT_MS) {
        // 60秒加热结束，进入丙酮采集阶段
        curPhase = PHASE_ACETONE;
        phaseStartMs = now;
        gas_heater_off();
        Serial.println("[分时] SnO₂加热完成，开始采集丙酮浓度");
      }
      delay(100);  // 加热阶段不需要高频操作
      break;
    }
    
    case PHASE_ACETONE: {
      // 采集丙酮浓度
      float voltage_mv = measure_acetone_voltage();
      lastAcetonePpm = acetone_ppm_from_mv(voltage_mv);
      
      Serial.printf("[分时] 丙酮采集完成: %.1f mV -> %.1f ppm\n", 
                    voltage_mv, lastAcetonePpm);
      
      // 发送丙酮数据
      send_ble_data();
      
      // 等待稳定时间
      if (elapsed >= PHASE_ACETONE_MS) {
        // 进入睡眠阶段
        curPhase = PHASE_SLEEP;
        Serial.println("[分时] 丙酮采集阶段结束，准备进入低功耗睡眠");
      }
      break;
    }
    
    case PHASE_SLEEP: {
      // 发送一轮完整数据
      send_ble_data();
      Serial.println("[分时] 一轮采集完成，进入5分钟低功耗睡眠");
      
      // 重置状态
      hrBufIdx = 0;
      hrBufFilled = false;
      lastHR = 0;
      lastSpO2 = 0;
      lastAcetonePpm = -1.0;
      
      // 进入低功耗睡眠
      enter_light_sleep(SLEEP_DURATION_US);
      
      // 唤醒后重新开始采集
      curPhase = PHASE_HR_SPO2;
      phaseStartMs = millis();
      Serial.println("[分时] 睡眠结束，开始新一轮采集");
      break;
    }
  }
}