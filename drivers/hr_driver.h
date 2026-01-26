#ifndef HR_DRIVER_H
#define HR_DRIVER_H

#include <Arduino.h>

// ──────────────────────────────────────────────
// 配置开关与基本参数（建议放在 config/ 目录统一管理，后期可迁移）
#define HR_USE_MOCK             0       // 1 = 使用模拟数据，0 = 真实 MAX30102
#define HR_I2C_RETRY_TIMES      3       // I2C 通信失败重试次数
#define HR_FIFO_READ_TIMEOUT_MS 50      // 读取 FIFO 超时保护（ms）

// ──────────────────────────────────────────────
// 推荐采样配置（可在 init 时修改，影响功耗与精度）
#define HR_SAMPLE_RATE          100     // 采样率 Hz (50/100/200/400/800/1600)
#define HR_PULSE_WIDTH          411     // 脉宽 us (69/118/215/411)
#define HR_LED_CURRENT          0x24    // LED 电流档位 0x00~0xFF (约 0~51mA)
#define HR_FIFO_AVERAGE         4       // 内部平均样本数 (1/2/4/8/16/32)

// ──────────────────────────────────────────────
// 寄存器地址（参考 MAX30102 datasheet）
#define MAX30102_I2C_ADDR       0x57

#define REG_INTR_STATUS_1       0x00
#define REG_INTR_STATUS_2       0x01
#define REG_INTR_ENABLE_1       0x02
#define REG_INTR_ENABLE_2       0x03
#define REG_FIFO_WR_PTR         0x04
#define REG_OVF_COUNTER         0x05
#define REG_FIFO_RD_PTR         0x06
#define REG_FIFO_DATA           0x07
#define REG_FIFO_CONFIG         0x08
#define REG_MODE_CONFIG         0x09
#define REG_SPO2_CONFIG         0x0A
#define REG_LED1_PA             0x0C    // RED
#define REG_LED2_PA             0x0D    // IR
#define REG_MULTI_LED_CTRL1     0x11
#define REG_MULTI_LED_CTRL2     0x12
#define REG_TEMP_INTEGER        0x1F
#define REG_TEMP_FRACTION       0x20
#define REG_TEMP_CONFIG         0x21
#define REG_PART_ID             0xFF    // 应返回 0x15

// ──────────────────────────────────────────────
// 函数声明
bool hr_init();
bool hr_available();                    // 是否有新数据可读（轮询方式）
bool hr_read_sample(int32_t* red, int32_t* ir);   // 读取一个样本
bool hr_read_latest(int32_t* red, int32_t* ir);   // 读取最新一个有效样本（推荐）
void hr_shutdown();                     // 进入低功耗关断模式
void hr_wakeup();                       // 从关断唤醒

// 可选：获取芯片温度（用于校准或调试）
float hr_read_temperature();

#endif