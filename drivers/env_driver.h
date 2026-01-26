#ifndef ENV_DRIVER_H
#define ENV_DRIVER_H

#include <Arduino.h>
#include "../config/pin_config.h"

// ──────────────────────────────────────────────
// 配置开关
#define ENV_USE_MOCK             1       // 1=虚拟（调试用），0=真实传感器
#define ENV_USE_I2C              1       // 1=I2C（DHT20/AHT20），0=模拟/数字

// ──────────────────────────────────────────────
// I2C 配置（DHT20/AHT20）
#define ENV_I2C_ADDR             0x38    // DHT20/AHT20 默认地址

// ──────────────────────────────────────────────
// 模拟引脚（如果使用模拟传感器）
#ifndef ENV_ANALOG_TEMP_PIN
#define ENV_ANALOG_TEMP_PIN      A3      // 温度模拟输入
#endif

#ifndef ENV_ANALOG_RH_PIN
#define ENV_ANALOG_RH_PIN        A6      // 湿度模拟输入
#endif

// ──────────────────────────────────────────────
// 数据结构
typedef struct {
    float temperature_c;    // 温度（℃）
    float humidity_rh;      // 相对湿度（%）
    bool valid;             // 数据是否有效
} EnvData;

// ──────────────────────────────────────────────
// 函数声明
bool env_init();                    // 初始化
bool env_read(EnvData* out);        // 读取环境数据，返回true=有效

#endif
