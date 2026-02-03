#ifndef HR_ALGORITHM_H
#define HR_ALGORITHM_H

#include <Arduino.h>
#include "../drivers/hr_driver.h"   // 路径根据你的工程调整

// ──────────────────────────────────────────────
// 配置参数（低RAM优化版本）
// 原值500占用4000 bytes，改为128占用1024 bytes（仍节省内存）
#define HR_BUFFER_SIZE          128     // ≈1.28秒 @100Hz（提高低心率检测可靠性）
#define HR_SAMPLE_INTERVAL_MS   10      // 与 hr_driver 采样率匹配
#define HR_MIN_PEAKS_REQUIRED   3       // 至少需要几个峰才计算（128样本约4-6个峰）
#define HR_MOVING_AVG_WINDOW    9       // 滑动平均窗口（奇数，噪声抑制）
#define HR_PEAK_THRESHOLD_BASE  0.5     // 自适应阈值基础倍数（信号标准差）
#define HR_MIN_BPM              40      // 合理心率下限
#define HR_MAX_BPM              180     // 合理心率上限
#define HR_SNR_THRESHOLD        20.0    // 最低信噪比（dB），低于此值视为无效（20dB = 200）

// SpO2 计算参数
#define SPO2_MIN_VALUE          70      // 血氧最小值（%）
#define SPO2_MAX_VALUE          100     // 血氧最大值（%）
#define SPO2_CORRELATION_THRESHOLD 0.7  // 红外/红光相关性阈值，低于此值视为运动伪影
#define SPO2_RATIO_MIN          0.4     // R值最小值
#define SPO2_RATIO_MAX          3.4     // R值最大值

// ──────────────────────────────────────────────
// 返回码定义（负值为错误，便于APP处理）
#define HR_SUCCESS              0       // 计算成功
#define HR_SUCCESS_WITH_MOTION  1       // 计算成功但有运动干扰（使用红光通道fallback）
#define HR_BUFFER_NOT_FULL     -1       // 缓冲未满
#define HR_POOR_SIGNAL         -2       // 信号质量差（噪声大/无峰）
#define HR_OUT_OF_RANGE        -3       // BPM 超出合理范围
#define HR_READ_FAILED         -4       // 驱动读取失败

// ──────────────────────────────────────────────
// 函数声明
void hr_algorithm_init();               // 初始化缓冲区
int hr_algorithm_update();              // 每 SAMPLE_INTERVAL_MS 调用：采集 + 缓冲更新，返回状态
uint8_t hr_calculate_bpm(int* status);  // 计算BPM，返回uint8_t（0=无效，40-180=BPM值）；status输出详细码
uint8_t hr_calculate_spo2(int* status); // 计算SpO2，返回uint8_t（0=无效，70-100=SpO2值）；status输出详细码
uint8_t hr_get_latest_bpm();            // 获取最近有效BPM（0=无效，40-180=BPM值）
uint8_t hr_get_latest_spo2();           // 获取最近有效SpO2（0=无效，70-100=SpO2值）

// 可选调试：获取当前信号质量（SNR*10，例如15.3dB返回153）
uint8_t hr_get_signal_quality();

// 运动干扰检测：计算红外/红光信号相关性（0-100，越高表示相关性越好）
uint8_t hr_get_correlation_quality();

#endif