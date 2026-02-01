#ifndef MOTION_CORRECTION_H
#define MOTION_CORRECTION_H

#include <stdint.h>

// ──────────────────────────────────────────────
// 配置参数（低RAM优化）
// ──────────────────────────────────────────────

// Kalman滤波器参数（定点运算，Q8.8格式）
#define KALMAN_Q_FRACTION_BITS   8
#define KALMAN_Q_SCALE           (1 << KALMAN_Q_FRACTION_BITS)  // 256

// 过程噪声协方差（Q，Q8.8格式）
#define KALMAN_Q_Q8              (int16_t)(0.1 * KALMAN_Q_SCALE)   // 0.1

// 测量噪声协方差（R，Q8.8格式）
#define KALMAN_R_Q8              (int16_t)(1.0 * KALMAN_Q_SCALE)   // 1.0

// 初始估计误差协方差（P，Q8.8格式）
#define KALMAN_P_INIT_Q8         (int16_t)(1.0 * KALMAN_Q_SCALE)   // 1.0

// TSSD参数
#define TSSD_WINDOW_SIZE         5      // 时间移位差分窗口大小
#define TSSD_THRESHOLD_FACTOR    3      // 阈值因子（标准差倍数）

// ──────────────────────────────────────────────
// 数据结构
// ──────────────────────────────────────────────

// Kalman滤波器状态（定点运算）
typedef struct {
    int16_t x_est_q8;      // 状态估计值（Q8.8）
    int16_t p_est_q8;      // 估计误差协方差（Q8.8）
    int16_t k_gain_q8;     // Kalman增益（Q8.8）
} KalmanState;

// TSSD滤波器状态
typedef struct {
    int16_t buffer[TSSD_WINDOW_SIZE];  // 滑动窗口
    uint8_t index;                     // 当前索引
    int16_t mean;                      // 窗口均值
    int16_t std_dev;                   // 窗口标准差
} TssdState;

// ──────────────────────────────────────────────
// 函数声明
// ──────────────────────────────────────────────

// Kalman滤波器初始化
void kalman_init(KalmanState* state, int16_t initial_value);

// Kalman滤波器更新（返回滤波后的值）
int16_t kalman_update(KalmanState* state, int16_t measurement);

// TSSD滤波器初始化
void tssd_init(TssdState* state);

// TSSD滤波器更新（返回去噪后的值）
int16_t tssd_update(TssdState* state, int16_t measurement);

// 快速整数绝对值
static inline int16_t abs_int16(int16_t x) {
    return (x < 0) ? -x : x;
}

// 快速整数平方（16位）
static inline int32_t square_int16(int16_t x) {
    return (int32_t)x * x;
}

#endif // MOTION_CORRECTION_H