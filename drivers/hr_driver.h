#ifndef HR_DRIVER_H
#define HR_DRIVER_H

#include <Arduino.h>

// ──────────────────────────────────────────────
// 配置参数
#define HR_SAMPLE_RATE          100     // 采样率 Hz (50/100/200/400/800/1600)
#define HR_PULSE_WIDTH          411     // 脉宽 us (69/118/215/411)
#define HR_LED_CURRENT          0x0A    // LED 电流档位 0x00~0xFF (约 0~51mA)
#define MAX30102_I2C_ADDR       0x57    // MAX30102 I2C地址
#define HR_I2C_RETRY_TIMES      3       // I2C重试次数

// ──────────────────────────────────────────────
// 函数声明
bool hr_driver_init();                  // 使用SparkFun库初始化MAX30102
bool hr_read_latest(int32_t* red, int32_t* ir);   // 读取最新一个有效样本
bool hr_available();                    // 是否有新数据可读
void hr_shutdown();                     // 进入低功耗关断模式
void hr_wakeup();                       // 从关断唤醒

// 可选：获取芯片温度（用于校准或调试）
float hr_read_temperature();

#endif
