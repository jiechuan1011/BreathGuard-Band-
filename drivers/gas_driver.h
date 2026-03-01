#ifndef GAS_DRIVER_H
#define GAS_DRIVER_H

#include <Arduino.h>
#include "../config/pin_config.h"

// ──────────────────────────────────────────────
// 调试开关（由config.h控制）
#ifdef DEBUG_MODE
#define GAS_DEBUG_PRINT(x) Serial.print(x)
#define GAS_DEBUG_PRINTLN(x) Serial.println(x)
#define GAS_DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define GAS_DEBUG_PRINT(x)
#define GAS_DEBUG_PRINTLN(x)
#define GAS_DEBUG_PRINTF(...)
#endif

// ──────────────────────────────────────────────
// 硬件配置参数（可在config/system_config.h中覆盖）
#ifndef GAS_WARMUP_MS
#define GAS_WARMUP_MS            60000   // SnO₂预热时间（ms），非阻塞
#endif

#ifndef GAS_HEATER_TARGET_TEMP
#define GAS_HEATER_TARGET_TEMP   350     // 目标加热温度（℃）
#endif

#ifndef GAS_SAMPLE_COUNT
#define GAS_SAMPLE_COUNT         20      // 采集样本数（用于方差计算）
#endif

#ifndef GAS_SAMPLE_INTERVAL_MS
#define GAS_SAMPLE_INTERVAL_MS   5       // 样本采集间隔（ms）
#endif

#ifndef GAS_VARIANCE_THRESHOLD
#define GAS_VARIANCE_THRESHOLD   100.0f  // 方差阈值（mV²），超过则判定为运动干扰
#endif

#ifndef GAS_ADC_REF_MV
#define GAS_ADC_REF_MV           3300    // ESP32-C3 3.3V参考电压（mV）
#endif

#ifndef GAS_ADC_RESOLUTION
#define GAS_ADC_RESOLUTION       4096    // ESP32 12-bit ADC分辨率（4096）
#endif

#ifndef GAS_LOAD_RESISTANCE
#define GAS_LOAD_RESISTANCE      10000   // 负载电阻RL（Ω），10kΩ
#endif

#ifndef GAS_SUPPLY_VOLTAGE_MV
#define GAS_SUPPLY_VOLTAGE_MV    3300    // 电源电压Vcc（mV），3.3V
#endif

#ifndef GAS_BASELINE_RATIO
#define GAS_BASELINE_RATIO       3.8f    // 空气中基线Rs/R0比率，默认3.8，需校准
#endif

// ──────────────────────────────────────────────
// 浓度转换参数（对数模型：ppm = a * ln(Rs/R0) + b）
#ifndef GAS_CALIB_A
#define GAS_CALIB_A              1.5f    // 对数模型参数a
#endif

#ifndef GAS_CALIB_B
#define GAS_CALIB_B              -2.0f   // 对数模型参数b
#endif

// ──────────────────────────────────────────────
// 加热控制参数
#ifndef GAS_HEATER_PREHEAT_DUTY
#define GAS_HEATER_PREHEAT_DUTY  100     // 预热阶段PWM占空比（%）
#endif

#ifndef GAS_HEATER_MAINTAIN_DUTY
#define GAS_HEATER_MAINTAIN_DUTY 80      // 维持阶段PWM占空比（%）
#endif

// ──────────────────────────────────────────────
// 函数声明
bool gas_init();                                    // 初始化引脚、启动加热、记录预热起点
bool gas_read(float* voltage_mv, float* conc_ppm); // 读取：返回true=有效数据，false=预热中
bool gas_is_warmed_up();                           // 检查预热是否完成
float gas_get_heater_duty_cycle();                 // 获取当前加热PWM占空比（0-100%）
uint32_t gas_get_warmup_remaining();               // 获取剩余预热时间（ms）

#ifdef PIN_AO3400_GATE
// 使用AO3400门控时的辅助接口
void gas_set_ao3400_gate(bool on);                  // 将门控引脚设置为高/低
bool gas_get_ao3400_gate();                         // 读取门控引脚状态
#endif

#endif
