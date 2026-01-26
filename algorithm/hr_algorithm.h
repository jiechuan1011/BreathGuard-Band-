#ifndef HR_ALGORITHM_H
#define HR_ALGORITHM_H

#include <Arduino.h>
#include "../drivers/hr_driver.h"   // 路径根据你的工程调整

// ──────────────────────────────────────────────
// 配置参数（低RAM优化版本）
// 原值500占用4000 bytes，改为64仅占用512 bytes（节省3488 bytes）
#define HR_BUFFER_SIZE          64      // ≈0.64秒 @100Hz（低RAM优化，仍足够峰值检测）
#define HR_SAMPLE_INTERVAL_MS   10      // 与 hr_driver 采样率匹配
#define HR_MIN_PEAKS_REQUIRED   2       // 至少需要几个峰才计算（低RAM优化，64样本约2-3个峰）
#define HR_MOVING_AVG_WINDOW    9       // 滑动平均窗口（奇数，噪声抑制）
#define HR_PEAK_THRESHOLD_BASE  0.5     // 自适应阈值基础倍数（信号标准差）
#define HR_MIN_BPM              40      // 合理心率下限
#define HR_MAX_BPM              180     // 合理心率上限
#define HR_SNR_THRESHOLD        20.0    // 最低信噪比（dB），低于此值视为无效（20dB = 200）

// ──────────────────────────────────────────────
// 返回码定义（负值为错误，便于APP处理）
#define HR_SUCCESS              0       // 计算成功
#define HR_BUFFER_NOT_FULL     -1       // 缓冲未满
#define HR_POOR_SIGNAL         -2       // 信号质量差（噪声大/无峰）
#define HR_OUT_OF_RANGE        -3       // BPM 超出合理范围
#define HR_READ_FAILED         -4       // 驱动读取失败

// ──────────────────────────────────────────────
// 函数声明
void hr_algorithm_init();               // 初始化缓冲区
int hr_algorithm_update();              // 每 SAMPLE_INTERVAL_MS 调用：采集 + 缓冲更新，返回状态
uint8_t hr_calculate_bpm(int* status);  // 计算BPM，返回uint8_t（0=无效，40-180=BPM值）；status输出详细码
uint8_t hr_get_latest_bpm();            // 获取最近有效BPM（0=无效，40-180=BPM值）

// 可选调试：获取当前信号质量（SNR*10，例如15.3dB返回153）
uint8_t hr_get_signal_quality();

#endif