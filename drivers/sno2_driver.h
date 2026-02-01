#ifndef SNO2_DRIVER_H
#define SNO2_DRIVER_H

#include <Arduino.h>
#include "../config/pin_config.h"

// ──────────────────────────────────────────────
// 低RAM优化配置（定点运算，无浮点）
// ──────────────────────────────────────────────

// 引脚配置（根据用户要求）
#define SNO2_HEATER_PIN      5    // GPIO5 - MOSFET加热控制
#define SNO2_ADC_PIN         4    // GPIO4 - AD623放大后ADC输入

// 加热控制参数
#define SNO2_HEAT_DURATION_MS   8000    // 加热8秒
#define SNO2_CYCLE_INTERVAL_MS  40000   // 每40秒一次加热周期
#define SNO2_HEATER_ON          1       // 高电平加热
#define SNO2_HEATER_OFF         0       // 低电平关闭

// ADC配置（ESP32-C3）
#define SNO2_ADC_REF_MV         3300    // 3.3V参考电压
#define SNO2_ADC_RESOLUTION     4096    // 12位ADC
#define SNO2_ADC_SAMPLES        16      // 采集样本数（必须是2的幂次，便于移位）

// 浓度计算参数（定点运算，使用Q格式）
// Q10.6格式：10位整数，6位小数（范围0-1023.984375）
#define SNO2_Q_FRACTION_BITS    6
#define SNO2_Q_SCALE            (1 << SNO2_Q_FRACTION_BITS)  // 64

// 线性标定参数（ppm = a * voltage_mv + b）
// 使用Q10.6格式存储
#define SNO2_CALIB_A_Q          (int16_t)(0.5 * SNO2_Q_SCALE)   // a=0.5
#define SNO2_CALIB_B_Q          (int16_t)(-100 * SNO2_Q_SCALE)  // b=-100

// 电压范围（mV）
#define SNO2_VOLTAGE_MIN_MV     0
#define SNO2_VOLTAGE_MAX_MV     3300

// 浓度范围（ppm）
#define SNO2_CONC_MIN_PPM       0
#define SNO2_CONC_MAX_PPM       1000

// ──────────────────────────────────────────────
// 状态定义
// ──────────────────────────────────────────────
typedef enum {
    SNO2_STATE_IDLE = 0,        // 空闲状态
    SNO2_STATE_HEATING,         // 加热中
    SNO2_STATE_SAMPLING,        // 采样中
    SNO2_STATE_CALCULATING,     // 计算中
    SNO2_STATE_ERROR            // 错误状态
} Sno2State;

// ──────────────────────────────────────────────
// 数据结构（低RAM优化）
// ──────────────────────────────────────────────
typedef struct {
    uint16_t voltage_mv;        // 电压值（mV）
    uint16_t concentration_ppm; // 浓度值（ppm）
    uint8_t valid : 1;          // 数据有效标志
    uint8_t heater_on : 1;      // 加热器状态
    uint8_t reserved : 6;       // 保留位
} Sno2Data;

// ──────────────────────────────────────────────
// 函数声明
// ──────────────────────────────────────────────

// 初始化SnO₂传感器
void sno2_init();

// 非阻塞状态机更新（每10ms调用）
void sno2_update();

// 获取当前状态
Sno2State sno2_get_state();

// 获取最新数据
Sno2Data sno2_get_data();

// 设置线性标定参数（a,b为浮点数，内部转换为Q格式）
void sno2_set_calibration(float a, float b);

// 获取加热器状态
uint8_t sno2_is_heater_on();

// 获取剩余加热时间（ms）
uint32_t sno2_get_heating_remaining();

// 获取下一个采样时间（ms）
uint32_t sno2_get_next_sample_time();

#endif // SNO2_DRIVER_H