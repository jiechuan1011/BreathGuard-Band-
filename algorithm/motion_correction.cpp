#include "motion_correction.h"

// ──────────────────────────────────────────────
// Kalman滤波器实现（定点运算）
// ──────────────────────────────────────────────

void kalman_init(KalmanState* state, int16_t initial_value) {
    // 初始化状态估计值（转换为Q8.8格式）
    state->x_est_q8 = initial_value << KALMAN_Q_FRACTION_BITS;
    
    // 初始化估计误差协方差
    state->p_est_q8 = KALMAN_P_INIT_Q8;
    
    // 初始Kalman增益为0
    state->k_gain_q8 = 0;
}

int16_t kalman_update(KalmanState* state, int16_t measurement) {
    // 将测量值转换为Q8.8格式
    int16_t z_q8 = measurement << KALMAN_Q_FRACTION_BITS;
    
    // 1. 预测步骤
    // 状态预测：x_pred = x_est（一维系统，无控制输入）
    int16_t x_pred_q8 = state->x_est_q8;
    
    // 误差协方差预测：p_pred = p_est + Q
    int16_t p_pred_q8 = state->p_est_q8 + KALMAN_Q_Q8;
    
    // 2. 更新步骤
    // Kalman增益：K = p_pred / (p_pred + R)
    // 使用32位中间值避免溢出
    int32_t numerator = (int32_t)p_pred_q8 << KALMAN_Q_FRACTION_BITS;  // 转换为Q16.16
    int32_t denominator = (int32_t)p_pred_q8 + KALMAN_R_Q8;
    
    if (denominator == 0) {
        denominator = 1;  // 防止除零
    }
    
    // 计算Kalman增益（Q8.8格式）
    state->k_gain_q8 = (int16_t)(numerator / denominator);
    
    // 状态更新：x_est = x_pred + K * (z - x_pred)
    int16_t innovation_q8 = z_q8 - x_pred_q8;  // 创新（Q8.8）
    int32_t correction_q16 = (int32_t)state->k_gain_q8 * innovation_q8;  // Q8.8 * Q8.8 = Q16.16
    int16_t correction_q8 = (int16_t)(correction_q16 >> KALMAN_Q_FRACTION_BITS);  // 右移8位得到Q8.8
    
    state->x_est_q8 = x_pred_q8 + correction_q8;
    
    // 误差协方差更新：p_est = (1 - K) * p_pred
    // 使用近似：p_est = p_pred - K * p_pred
    int32_t p_reduction_q16 = (int32_t)state->k_gain_q8 * p_pred_q8;  // Q8.8 * Q8.8 = Q16.16
    int16_t p_reduction_q8 = (int16_t)(p_reduction_q16 >> KALMAN_Q_FRACTION_BITS);  // Q8.8
    
    state->p_est_q8 = p_pred_q8 - p_reduction_q8;
    if (state->p_est_q8 < 0) {
        state->p_est_q8 = 0;  // 确保非负
    }
    
    // 返回滤波后的值（转换为整数）
    return (int16_t)(state->x_est_q8 >> KALMAN_Q_FRACTION_BITS);
}

// ──────────────────────────────────────────────
// TSSD滤波器实现（时间移位差分）
// ──────────────────────────────────────────────

void tssd_init(TssdState* state) {
    // 初始化缓冲区为0
    for (uint8_t i = 0; i < TSSD_WINDOW_SIZE; i++) {
        state->buffer[i] = 0;
    }
    
    state->index = 0;
    state->mean = 0;
    state->std_dev = 0;
}

int16_t tssd_update(TssdState* state, int16_t measurement) {
    // 更新滑动窗口
    state->buffer[state->index] = measurement;
    state->index = (state->index + 1) % TSSD_WINDOW_SIZE;
    
    // 计算窗口均值和标准差
    int32_t sum = 0;
    int32_t sum_sq = 0;
    
    for (uint8_t i = 0; i < TSSD_WINDOW_SIZE; i++) {
        int16_t val = state->buffer[i];
        sum += val;
        sum_sq += square_int16(val);
    }
    
    // 计算均值（整数除法）
    state->mean = (int16_t)(sum / TSSD_WINDOW_SIZE);
    
    // 计算方差：var = (sum_sq / N) - mean^2
    int32_t mean_sq = square_int16(state->mean);
    int32_t variance = (sum_sq / TSSD_WINDOW_SIZE) - mean_sq;
    
    if (variance < 0) {
        variance = 0;
    }
    
    // 近似计算标准差：std_dev ≈ sqrt(variance)
    // 使用快速整数平方根近似
    uint32_t v = (uint32_t)variance;
    uint32_t result = 0;
    uint32_t bit = 1 << 30;  // 最高位
    
    // 找到最高位
    while (bit > v) {
        bit >>= 2;
    }
    
    // 计算平方根
    while (bit != 0) {
        if (v >= result + bit) {
            v -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    
    state->std_dev = (int16_t)result;
    
    // 计算时间移位差分（当前值与均值的差）
    int16_t diff = measurement - state->mean;
    int16_t abs_diff = abs_int16(diff);
    
    // 计算动态阈值
    int16_t threshold = state->std_dev * TSSD_THRESHOLD_FACTOR;
    
    // 如果差分超过阈值，认为是运动伪影
    if (abs_diff > threshold && threshold > 0) {
        // 使用窗口均值替代异常值
        return state->mean;
    } else {
        // 正常值，直接返回
        return measurement;
    }
}

// ──────────────────────────────────────────────
// 辅助函数
// ──────────────────────────────────────────────

// 快速整数绝对值（已在头文件中声明为内联函数）

// 快速整数平方（已在头文件中声明为内联函数）