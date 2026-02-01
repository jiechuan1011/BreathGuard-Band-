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
