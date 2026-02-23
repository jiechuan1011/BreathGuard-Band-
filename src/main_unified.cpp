/*
 * main_unified.cpp - 糖尿病筛查监测系统统一主入口
 * 
 * 功能：统一的主程序入口，通过条件编译支持多种硬件配置
 * 设计原则：
 * 1. 单一入口点，通过宏定义选择硬件平台和角色
 * 2. 模块化设计，职责分离
 * 3. 功耗优化，支持深度睡眠
 * 4. 错误恢复机制
 * 5. 代码质量保证
 * 
 * 支持的硬件配置：
 * 1. ESP32-C3 SuperMini (腕带主控)
 * 2. ESP32-S3R8N8 (腕带主控增强版)
 * 3. ESP32-C3 SuperMini (检测模块)
 * 
 * 编译选项（通过platformio.ini的build_flags设置）：
 * - DEVICE_ROLE_WRIST: 腕带主控角色
 * - DEVICE_ROLE_DETECTOR: 检测模块角色
 * - MCU_ESP32_C3: ESP32-C3平台
 * - MCU_ESP32_S3: ESP32-S3平台
 * - USE_BLE_MODULE: 启用BLE功能
 * - USE_OLED_DISPLAY: 启用OLED显示
 * - USE_MAX30102: 启用心率血氧传感器
 * - USE_SNO2_SENSOR: 启用丙酮传感器
 * - DEBUG_MODE: 调试模式
 */

#include <Arduino.h>
#include "../config/config.h"
#include "../config/pin_config.h"
#include "../config/system_config.h"
#include "../config/ble_config.h"
#include "../system/system_state.h"
// I2C (Wire) used when display or MAX30102 enabled
#if defined(USE_OLED_DISPLAY) || defined(USE_MAX30102)
#include <Wire.h>
#endif
// Heart-rate algorithm API
#include "../algorithm/hr_algorithm.h"

// ==================== 免责声明 ====================
const char DISCLAIMER[] PROGMEM = "本设备仅用于生理参数趋势监测，不用于医疗诊断，请咨询专业医生";

// ==================== 模块初始化函数声明 ====================
void init_system_state();
void init_hardware();
void init_sensors();
void init_communication();
void init_power_management();

// ==================== 主循环函数声明 ====================
void process_sensors();
void update_display();
void handle_communication();
void handle_user_input();
void manage_power();

// ==================== 丙酮数据处理函数 ====================
float read_acetone_concentration();
bool is_acetone_sensor_available();

// ==================== 错误处理函数 ====================
void handle_sensor_errors();
void handle_communication_errors();
void handle_power_errors();

// ==================== 功耗管理函数 ====================
void enter_deep_sleep(uint32_t sleep_time_ms);
void enter_light_sleep();
bool should_enter_sleep();

// ==================== 系统状态变量 ====================
static struct {
    bool initialized;
    bool sensors_ready;
    bool communication_ready;
    bool display_on;
    uint32_t last_activity_time;
    uint8_t error_count;
    uint8_t battery_level;
    bool low_power_mode;
} system_status = {0};

// ==================== 初始化函数 ====================
void setup() {
    // 初始化串口（仅在调试模式）
#ifdef DEBUG_MODE
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n========================================");
    Serial.println("  糖尿病筛查监测系统 - 统一主入口");
    Serial.println("========================================\n");
    
    // 输出系统信息
    Serial.println("系统信息：");
#ifdef DEVICE_ROLE_WRIST
    Serial.println("设备角色：腕带主控");
#ifdef MCU_ESP32_C3
    Serial.println("硬件平台：ESP32-C3 SuperMini");
#elif defined(MCU_ESP32_S3)
    Serial.println("硬件平台：ESP32-S3R8N8");
#endif
#elif defined(DEVICE_ROLE_DETECTOR)
    Serial.println("设备角色：检测模块");
    Serial.println("硬件平台：ESP32-C3 SuperMini");
#endif
    
    Serial.println(String("版本：") + SOFTWARE_VERSION_STRING);
    Serial.println(String("构建日期：") + BUILD_DATE_STRING);
    Serial.println();
    Serial.println("免责声明：");
    Serial.println(DISCLAIMER);
    Serial.println("========================================\n");
#endif

    // 初始化系统状态
    init_system_state();
    
    // 初始化硬件
    init_hardware();
    
    // 初始化传感器
    init_sensors();
    
    // 初始化通信模块
    init_communication();
    
    // 初始化功耗管理
    init_power_management();
    
    // 标记系统已初始化
    system_status.initialized = true;
    system_status.last_activity_time = millis();
    
#ifdef DEBUG_MODE
    Serial.println("[系统] 初始化完成");
#endif
}

// ==================== 主循环函数 ====================
void loop() {
    uint32_t current_time = millis();
    
    // 检查系统状态
    if (!system_status.initialized) {
#ifdef DEBUG_MODE
        Serial.println("[错误] 系统未初始化，尝试重新初始化");
#endif
        setup();
        return;
    }
    
    // 处理传感器数据
    process_sensors();
    
    // 处理用户输入
    handle_user_input();
    
    // 更新显示
    update_display();
    
    // 处理通信
    handle_communication();
    
    // 错误处理
    handle_sensor_errors();
    handle_communication_errors();
    handle_power_errors();
    
    // 功耗管理
    manage_power();
    
    // 非阻塞延时
    static uint32_t last_loop_time = 0;
    uint32_t loop_time = millis() - current_time;
    uint32_t target_interval = 10; // 100Hz主循环
    
    if (loop_time < target_interval) {
        delay(target_interval - loop_time);
    }
}

// ==================== 系统状态初始化 ====================
void init_system_state() {
    system_state_init();
    system_status.initialized = false;
    system_status.sensors_ready = false;
    system_status.communication_ready = false;
    system_status.display_on = true;
    system_status.last_activity_time = 0;
    system_status.error_count = 0;
    system_status.battery_level = 100;
    system_status.low_power_mode = false;
}

// ==================== 硬件初始化 ====================
void init_hardware() {
#ifdef DEBUG_MODE
    Serial.println("[硬件] 初始化硬件...");
#endif

    // 初始化I2C总线
#if defined(USE_OLED_DISPLAY) || defined(USE_MAX30102)
    Wire.begin(PIN_SDA, PIN_SCL);
#ifdef DEBUG_MODE
    Serial.println("[硬件] I2C总线初始化完成");
#endif
#endif

    // 初始化按键
#ifdef PIN_BTN1
    pinMode(PIN_BTN1, INPUT_PULLUP);
#endif
#ifdef PIN_BTN2
    pinMode(PIN_BTN2, INPUT_PULLUP);
#endif

#ifdef DEBUG_MODE
    Serial.println("[硬件] 硬件初始化完成");
#endif
}

// ==================== 传感器初始化 ====================
void init_sensors() {
#ifdef DEBUG_MODE
    Serial.println("[传感器] 初始化传感器...");
#endif

    // 初始化心率血氧传感器
#ifdef USE_MAX30102
    if (hr_init()) {
        hr_algorithm_init();
        system_status.sensors_ready = true;
#ifdef DEBUG_MODE
        Serial.println("[传感器] MAX30102初始化成功");
#endif
    } else {
        system_status.error_count++;
#ifdef DEBUG_MODE
        Serial.println("[错误] MAX30102初始化失败");
#endif
    }
#endif

    // 初始化丙酮传感器
#ifdef USE_SNO2_SENSOR
    if (gas_init()) {
#ifdef DEBUG_MODE
        Serial.println("[传感器] 丙酮传感器初始化成功");
#endif
    } else {
        system_status.error_count++;
#ifdef DEBUG_MODE
        Serial.println("[错误] 丙酮传感器初始化失败");
#endif
    }
#endif

#ifdef DEBUG_MODE
    Serial.println("[传感器] 传感器初始化完成");
#endif
}

// ==================== 通信模块初始化 ====================
void init_communication() {
#ifdef DEBUG_MODE
    Serial.println("[通信] 初始化通信模块...");
#endif

    // 初始化BLE
#ifdef USE_BLE_MODULE
#ifdef MCU_ESP32_S3
    // ESP32-S3使用NimBLE
    initBLE();
#else
    // ESP32-C3使用标准BLE
    initBLE();
#endif
    system_status.communication_ready = true;
#ifdef DEBUG_MODE
    Serial.println("[通信] BLE初始化完成");
#endif
#endif

#ifdef DEBUG_MODE
    Serial.println("[通信] 通信模块初始化完成");
#endif
}

// ==================== 功耗管理初始化 ====================
void init_power_management() {
#ifdef DEBUG_MODE
    Serial.println("[功耗] 初始化功耗管理...");
#endif

    // 配置功耗优化参数
    // 这里可以设置CPU频率、外设功耗等
    
#ifdef DEBUG_MODE
    Serial.println("[功耗] 功耗管理初始化完成");
#endif
}

// ==================== 丙酮浓度读取函数 ====================
float read_acetone_concentration() {
#ifdef USE_SNO2_SENSOR
    float voltage_mv, conc_ppm;
    if (gas_read(&voltage_mv, &conc_ppm)) {
        return conc_ppm;
    }
    return 0.0; // 传感器未就绪时返回0.0
#else
    // 腕带主控没有丙酮传感器，返回0.0表示无效数据
    return 0.0;
#endif
}

bool is_acetone_sensor_available() {
#ifdef USE_SNO2_SENSOR
    return gas_is_warmed_up();
#else
    return false;
#endif
}

// ==================== 传感器数据处理 ====================
void process_sensors() {
    static uint32_t last_sample_time = 0;
    uint32_t current_time = millis();
    
    // 10ms采样间隔
    if (current_time - last_sample_time < 10) {
        return;
    }
    last_sample_time = current_time;
    
    // 处理心率血氧数据
#ifdef USE_MAX30102
    int32_t red, ir;
    if (hr_read_latest(&red, &ir)) {
        hr_algorithm_update();
        
        // 每64个样本（约0.64秒）计算一次
        static uint8_t sample_count = 0;
        sample_count++;
        
        if (sample_count >= 64) {
            sample_count = 0;
            
            int status;
            uint8_t bpm = hr_calculate_bpm(&status);
            
            if (status == HR_SUCCESS && bpm > 0) {
                uint8_t spo2 = hr_calculate_spo2(&status);
                uint8_t snr_x10 = hr_get_signal_quality();
                uint8_t correlation = hr_get_correlation_quality();
                
                system_state_set_hr_spo2(bpm, spo2, snr_x10, correlation, status);
                
#ifdef DEBUG_MODE
                Serial.printf("[传感器] HR:%d SpO2:%d SNR:%.1fdB\n", 
                            bpm, spo2, snr_x10/10.0);
#endif
            }
        }
    }
#endif
}

// ==================== 显示更新 ====================
void update_display() {
#ifdef USE_OLED_DISPLAY
    static uint32_t last_display_update = 0;
    uint32_t current_time = millis();
    
    // 限制显示更新频率（2Hz）
    if (current_time - last_display_update < 500) {
        return;
    }
    last_display_update = current_time;
    
    if (!system_status.display_on) {
        return;
    }
    
    // 这里调用OLED显示函数
    // update_display_content();
#endif
}

// ==================== 通信处理 ====================
void handle_communication() {
#ifdef USE_BLE_MODULE
    static uint32_t last_ble_update = 0;
    uint32_t current_time = millis();
    
    // 4秒更新间隔
    if (current_time - last_ble_update < 4000) {
        return;
    }
    last_ble_update = current_time;
    
    // 生成JSON数据
    const SystemState* state = system_state_get();
    float acetone = read_acetone_concentration();
    
    char json_buffer[128];
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"hr\":%d,\"spo2\":%d,\"acetone\":%.1f,\"snr\":%.1f,\"battery\":%d}",
             state->hr_bpm, state->spo2_value, acetone,
             state->hr_snr_db_x10 / 10.0, system_status.battery_level);
    
    // 发送BLE数据
    // send_ble_data(json_buffer);
    
#ifdef DEBUG_MODE
    Serial.printf("[通信] BLE数据: %s\n", json_buffer);
#endif
#endif
}

// ==================== 用户输入处理 ====================
void handle_user_input() {
    // 处理按键输入
    // 这里实现按键消抖和事件处理
}

// ==================== 错误处理 ====================
void handle_sensor_errors() {
    // 传感器错误恢复逻辑
}

void handle_communication_errors() {
    // 通信错误恢复逻辑
}

void handle_power_errors() {
    // 电源错误处理逻辑
}

// ==================== 功耗管理 ====================
void manage_power() {
    uint32_t current_time = millis();
    
    // 检查是否需要进入睡眠
    if (should_enter_sleep()) {
        // 30秒无活动进入深度睡眠
        if (current_time - system_status.last_activity_time > 30000) {
            enter_deep_sleep(60000); // 睡眠60秒
        }
    }
    
    // 检查电池电量
    if (system_status.battery_level < 20) {
        system_status.low_power_mode = true;
        // 进入低功耗模式
    }
}

bool should_enter_sleep() {
    // 检查是否满足睡眠条件
    // 1. 没有BLE连接
    // 2. 没有用户活动
    // 3. 电池电量充足
    return true;
}

void enter_deep_sleep(uint32_t sleep_time_ms) {
#ifdef DEBUG_MODE
    Serial.printf("[功耗] 进入深度睡眠 %lu ms\n", sleep_time_ms);
#endif
    
    // 保存系统状态
    // 关闭外设
    // 配置唤醒源
    
    // ESP32深度睡眠
#ifdef ESP32
    esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000);
    esp_deep_sleep_start();
#endif
}

void enter_light_sleep() {
    // 轻睡眠模式
#ifdef ESP32
    esp_sleep_enable_timer_wakeup(10000); // 10秒
    esp_light_sleep_start();
#endif
}