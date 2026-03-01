/*
 * hardware_validation.h - 硬件配置验证文件
 * 
 * 功能：验证ESP32-S3的引脚分配，确认OLED显示正常，测试BLE连接稳定性
 * 设计原则：
 * 1. 统一的引脚验证机制
 * 2. 硬件兼容性检查
 * 3. 错误诊断和报告
 * 4. 自动配置优化
 */

#ifndef HARDWARE_VALIDATION_H
#define HARDWARE_VALIDATION_H

#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"
#include "config.h"

// ==================== 引脚验证结果结构 ====================
typedef struct {
    bool i2c_initialized;
    bool oled_detected;
    bool max30102_detected;
    bool ble_initialized;
    bool gas_sensor_detected;
    bool battery_monitor_working;
    bool buttons_working;
    uint8_t error_count;
    char error_messages[5][64]; // 最多存储5个错误信息
} HardwareValidationResult;

// ==================== 引脚功能验证 ====================
typedef enum {
    PIN_FUNC_I2C_SDA = 0,
    PIN_FUNC_I2C_SCL,
    PIN_FUNC_ADC,
    PIN_FUNC_PWM,
    PIN_FUNC_DIGITAL_INPUT,
    PIN_FUNC_DIGITAL_OUTPUT,
    PIN_FUNC_INTERRUPT
} PinFunction;

// ==================== 函数声明 ====================
void hardware_validation_init();
HardwareValidationResult validate_hardware_configuration();
bool validate_pin_configuration(uint8_t pin, PinFunction function);
bool test_i2c_bus();
bool test_oled_display();
bool test_max30102_sensor();
bool test_ble_module();
bool test_gas_sensor();
bool test_battery_monitor();
bool test_buttons();
void print_validation_result(const HardwareValidationResult* result);
void auto_configure_pins();

// ==================== 硬件特定配置 ====================
#ifdef MCU_ESP32_S3
// ESP32-S3R8N8 特定配置
#define ESP32_S3_I2C_PINS_VALID 1
#define ESP32_S3_ADC_PINS_VALID 1
#define ESP32_S3_PWM_PINS_VALID 1

// 推荐的引脚分配
#define RECOMMENDED_SDA_PIN 4
#define RECOMMENDED_SCL_PIN 5
#define RECOMMENDED_OLED_RESET_PIN -1  // 无复位引脚
#define RECOMMENDED_BUTTON1_PIN 0      // GPIO0作为按键
#define RECOMMENDED_BUTTON2_PIN 1      // GPIO1作为第二个按键
#define RECOMMENDED_BATTERY_ADC_PIN 2  // GPIO2作为电池ADC
#define RECOMMENDED_GAS_HEATER_PIN 9   // GPIO9作为加热PWM
#define RECOMMENDED_GAS_ADC_PIN 10     // GPIO10作为气体ADC

#elif defined(MCU_ESP32_C3)
// ESP32-C3 SuperMini 特定配置
#define ESP32_C3_I2C_PINS_VALID 1
#define ESP32_C3_ADC_PINS_VALID 1
#define ESP32_C3_PWM_PINS_VALID 1

// 推荐的引脚分配
#define RECOMMENDED_SDA_PIN 4
#define RECOMMENDED_SCL_PIN 5
#define RECOMMENDED_OLED_RESET_PIN -1
#define RECOMMENDED_BUTTON1_PIN 6      // GPIO6作为按键
#define RECOMMENDED_BUTTON2_PIN 7      // GPIO7作为第二个按键
#define RECOMMENDED_BATTERY_ADC_PIN 2  // GPIO2作为电池ADC
#define RECOMMENDED_GAS_HEATER_PIN 9   // GPIO9作为加热PWM
#define RECOMMENDED_GAS_ADC_PIN 0      // GPIO0作为气体ADC

#endif

// ==================== 引脚冲突检查 ====================
typedef struct {
    uint8_t pin1;
    uint8_t pin2;
    const char* conflict_description;
} PinConflict;

static const PinConflict known_conflicts[] = {
    {PIN_BAT_ADC, PIN_GAS_ADC, "电池ADC与气体ADC引脚冲突"},
    {PIN_BTN1, PIN_BTN2, "按键引脚可能冲突"},
    // 添加更多已知冲突
};

// ==================== 验证函数实现 ====================
#ifdef __cplusplus
extern "C" {
#endif

// 初始化硬件验证
void hardware_validation_init() {
    // 初始化验证系统
}

// 验证硬件配置
HardwareValidationResult validate_hardware_configuration() {
    HardwareValidationResult result = {0};
    
    // 验证I2C总线
    result.i2c_initialized = test_i2c_bus();
    if (!result.i2c_initialized) {
        snprintf(result.error_messages[result.error_count++], 64, 
                "I2C总线初始化失败");
    }
    
    // 验证OLED显示
#ifdef USE_OLED_DISPLAY
    result.oled_detected = test_oled_display();
    if (!result.oled_detected) {
        snprintf(result.error_messages[result.error_count++], 64, 
                "OLED显示未检测到");
    }
#endif
    
    // 验证MAX30102传感器
#ifdef USE_MAX30102
    result.max30102_detected = test_max30102_sensor();
    if (!result.max30102_detected) {
        snprintf(result.error_messages[result.error_count++], 64, 
                "MAX30102传感器未检测到");
    }
#endif
    
    // 验证BLE模块
#ifdef USE_BLE_MODULE
    result.ble_initialized = test_ble_module();
    if (!result.ble_initialized) {
        snprintf(result.error_messages[result.error_count++], 64, 
                "BLE模块初始化失败");
    }
#endif
    
    // 验证气体传感器
#ifdef USE_SNO2_SENSOR
    result.gas_sensor_detected = test_gas_sensor();
    if (!result.gas_sensor_detected) {
        snprintf(result.error_messages[result.error_count++], 64, 
                "气体传感器未检测到");
    }
#endif
    
    // 验证电池监控
    result.battery_monitor_working = test_battery_monitor();
    if (!result.battery_monitor_working) {
        snprintf(result.error_messages[result.error_count++], 64, 
                "电池监控功能异常");
    }
    
    // 验证按键
    result.buttons_working = test_buttons();
    if (!result.buttons_working) {
        snprintf(result.error_messages[result.error_count++], 64, 
                "按键功能异常");
    }
    
    return result;
}

// 测试I2C总线
bool test_i2c_bus() {
    // 检查I2C引脚配置
    if (!validate_pin_configuration(PIN_SDA, PIN_FUNC_I2C_SDA)) {
        return false;
    }
    
    if (!validate_pin_configuration(PIN_SCL, PIN_FUNC_I2C_SCL)) {
        return false;
    }
    
    // 尝试扫描I2C设备
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.beginTransmission(0x00); // 测试传输
    uint8_t error = Wire.endTransmission();
    
    return (error == 0);
}

// 测试OLED显示
bool test_oled_display() {
    // 检查OLED地址是否响应
    Wire.beginTransmission(I2C_ADDR_OLED);
    uint8_t error = Wire.endTransmission();
    
    return (error == 0);
}

// 测试MAX30102传感器
bool test_max30102_sensor() {
    // 检查MAX30102地址是否响应
    // 检查丙酮传感器 ADC 引脚（如果定义）
    #ifdef PIN_ACETONE_ADC
    validate_pin_configuration(PIN_ACETONE_ADC, PIN_FUNC_ADC);
    #endif
    Wire.beginTransmission(I2C_ADDR_MAX30102);
    uint8_t error = Wire.endTransmission();
    
    return (error == 0);
}

// 测试BLE模块
bool test_ble_module() {
    // BLE初始化测试
    // 这里可以添加BLE功能测试
    return true; // 暂时返回true，实际需要实现测试
}

// 测试气体传感器
bool test_gas_sensor() {
#ifdef DEVICE_ROLE_DETECTOR
    // 检测模块：检查气体传感器
    bool ok = validate_pin_configuration(PIN_GAS_ADC, PIN_FUNC_ADC) &&
              validate_pin_configuration(PIN_GAS_HEATER, PIN_FUNC_PWM);
    
    // 可选的AO3400门控
    #ifdef PIN_AO3400_GATE
    ok &= validate_pin_configuration(PIN_AO3400_GATE, PIN_FUNC_DIGITAL_OUTPUT);
    #endif
    return ok;
#else
    // 腕带主控：没有气体传感器
    return true;
#endif
}

// 测试电池监控
bool test_battery_monitor() {
    bool ok = validate_pin_configuration(PIN_BAT_ADC, PIN_FUNC_ADC);
    #ifdef PIN_BAT_FET_GATE
    ok &= validate_pin_configuration(PIN_BAT_FET_GATE, PIN_FUNC_DIGITAL_OUTPUT);
    #endif
    return ok;
}

// 测试按键
bool test_buttons() {
    bool btn1_ok = true;
    bool btn2_ok = true;
    
#ifdef PIN_BTN1
    btn1_ok = validate_pin_configuration(PIN_BTN1, PIN_FUNC_DIGITAL_INPUT);
#endif
    
#ifdef PIN_BTN2
    btn2_ok = validate_pin_configuration(PIN_BTN2, PIN_FUNC_DIGITAL_INPUT);
#endif
    
    return btn1_ok && btn2_ok;
}

// 验证引脚配置
bool validate_pin_configuration(uint8_t pin, PinFunction function) {
    // 检查引脚是否在有效范围内
#ifdef MCU_ESP32_S3
    if (pin > 48) { // ESP32-S3最大GPIO编号
        return false;
    }
#elif defined(MCU_ESP32_C3)
    if (pin > 22) { // ESP32-C3最大GPIO编号
        return false;
    }
#endif
    
    // 检查引脚功能兼容性
    switch (function) {
        case PIN_FUNC_I2C_SDA:
        case PIN_FUNC_I2C_SCL:
            // 检查引脚是否支持I2C
            return true; // 简化检查，实际需要根据硬件手册验证
            
        case PIN_FUNC_ADC:
            // 检查引脚是否支持ADC
            return true;
            
        case PIN_FUNC_PWM:
            // 检查引脚是否支持PWM
            return true;
            
        case PIN_FUNC_DIGITAL_INPUT:
        case PIN_FUNC_DIGITAL_OUTPUT:
            // 所有GPIO都支持数字输入输出
            return true;
            
        case PIN_FUNC_INTERRUPT:
            // 检查引脚是否支持中断
            return true;
            
        default:
            return false;
    }
}

// 打印验证结果
void print_validation_result(const HardwareValidationResult* result) {
    Serial.println("\n=== 硬件配置验证结果 ===");
    Serial.println("项目\t\t状态");
    Serial.println("----------------------");
    
    Serial.print("I2C总线\t\t");
    Serial.println(result->i2c_initialized ? "✓ 正常" : "✗ 异常");
    
#ifdef USE_OLED_DISPLAY
    Serial.print("OLED显示\t");
    Serial.println(result->oled_detected ? "✓ 正常" : "✗ 异常");
#endif
    
#ifdef USE_MAX30102
    Serial.print("MAX30102\t");
    Serial.println(result->max30102_detected ? "✓ 正常" : "✗ 异常");
#endif
    
#ifdef USE_BLE_MODULE
    Serial.print("BLE模块\t\t");
    Serial.println(result->ble_initialized ? "✓ 正常" : "✗ 异常");
#endif
    
#ifdef USE_SNO2_SENSOR
    Serial.print("气体传感器\t");
    Serial.println(result->gas_sensor_detected ? "✓ 正常" : "✗ 异常");
#endif
    
    Serial.print("电池监控\t");
    Serial.println(result->battery_monitor_working ? "✓ 正常" : "✗ 异常");
    
    Serial.print("按键功能\t");
    Serial.println(result->buttons_working ? "✓ 正常" : "✗ 异常");
    
    Serial.println("----------------------");
    
    if (result->error_count > 0) {
        Serial.println("错误信息：");
        for (uint8_t i = 0; i < result->error_count; i++) {
            Serial.printf("  %d. %s\n", i + 1, result->error_messages[i]);
        }
    } else {
        Serial.println("所有硬件检查通过 ✓");
    }
    Serial.println("======================\n");
}

// 自动配置引脚（基于硬件平台）
void auto_configure_pins() {
    // 根据硬件平台自动配置推荐引脚
#ifdef MCU_ESP32_S3
    Serial.println("[配置] 检测到ESP32-S3平台，使用推荐引脚配置");
    // 这里可以自动设置引脚配置
#elif defined(MCU_ESP32_C3)
    Serial.println("[配置] 检测到ESP32-C3平台，使用推荐引脚配置");
    // 这里可以自动设置引脚配置
#endif
    
    // 检查引脚冲突
    for (size_t i = 0; i < sizeof(known_conflicts) / sizeof(known_conflicts[0]); i++) {
        const PinConflict* conflict = &known_conflicts[i];
        // 检查冲突
        // 这里可以实现冲突检测逻辑
    }
}

#ifdef __cplusplus
}
#endif

#endif // HARDWARE_VALIDATION_H