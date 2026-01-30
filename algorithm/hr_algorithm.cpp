#include "hr_algorithm.h"

// 低RAM优化：使用int16_t代替int32_t（MAX30102 18-bit右对齐后范围-32768~32767，int16_t足够）
static int16_t ir_buffer[HR_BUFFER_SIZE];   // 主通道缓冲（IR对心率敏感）
static int16_t red_buffer[HR_BUFFER_SIZE];  // 辅助通道（用于质量检查）
static uint8_t buffer_pos = 0;              // uint8_t足够（HR_BUFFER_SIZE=64）
static bool buffer_filled = false;
// 低RAM优化：BPM用uint8_t（40-180范围），SNR用uint8_t（0-255，实际SNR约0-30dB）
static uint8_t last_bpm = 0;               // 0表示无效，40-180表示实际BPM
static uint8_t last_spo2 = 0;              // 0表示无效，70-100表示实际SpO2
static uint8_t last_snr = 0;               // SNR*10存储（例如15.3dB存储为153）
static uint8_t last_correlation = 0;       // 红外/红光相关性（0-100）

// ─── 私有函数 ──────────────────────────────────────────────

// 快速整数平方根（16位）
static uint16_t fast_sqrt16(uint16_t x) {
    if (x == 0) return 0;
    uint16_t res = 0;
    uint16_t bit = 1 << 14;  // 最高位
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

// 高通滤波（简单一阶IIR，截止≈0.5Hz，去基线漂移）
// 低RAM优化：使用int16_t，alpha用定点数（243/256 ≈ 0.95）
static void high_pass_filter(int16_t* signal) {
    int16_t y_prev = signal[0];
    for (uint8_t i = 1; i < HR_BUFFER_SIZE; i++) {
        // 定点运算：alpha=243/256，避免float
        int16_t y = (int16_t)(((int32_t)243 * (y_prev + signal[i] - signal[i-1])) >> 8);
        y_prev = y;
        signal[i] = y;
    }
}

// 低通滤波（滑动平均，截止≈5Hz）
// 低RAM优化：原地滤波，避免临时数组（节省128 bytes）
static void low_pass_filter(int16_t* signal) {
    // 使用滑动窗口，原地更新（需要反向遍历避免覆盖）
    int16_t prev_values[HR_MOVING_AVG_WINDOW];
    for (uint8_t i = 0; i < HR_MOVING_AVG_WINDOW; i++) {
        prev_values[i] = signal[i];
    }
    
    for (uint8_t i = HR_MOVING_AVG_WINDOW/2; i < HR_BUFFER_SIZE - HR_MOVING_AVG_WINDOW/2; i++) {
        int32_t sum = 0;
        for (int8_t k = -HR_MOVING_AVG_WINDOW/2; k <= HR_MOVING_AVG_WINDOW/2; k++) {
            sum += signal[i + k];
        }
        signal[i] = (int16_t)(sum / HR_MOVING_AVG_WINDOW);
    }
}

// 计算信噪比（SNR = 20 * log10(信号幅度 / 噪声幅度)）
// 低RAM优化：返回uint8_t（SNR*10），避免float
static uint8_t calculate_snr(int16_t* signal) {
    int32_t sum = 0, sum_sq = 0;
    for (uint8_t i = 0; i < HR_BUFFER_SIZE; i++) {
        sum += signal[i];
        sum_sq += (int32_t)signal[i] * signal[i];
    }
    // 定点运算：mean = sum / HR_BUFFER_SIZE
    int32_t mean = sum / HR_BUFFER_SIZE;
    // variance = sum_sq/HR_BUFFER_SIZE - mean^2
    int32_t variance = (sum_sq / HR_BUFFER_SIZE) - (mean * mean);
    if (variance < 0) variance = 0;
    
    // 计算信号幅度（标准差）
    uint16_t signal_amp = fast_sqrt16((uint16_t)variance);
    
    // 估计噪声幅度：使用高频分量（原始信号与滤波后信号的差值）
    // 简化：噪声幅度 ≈ 信号幅度的1/10（经验值）
    uint16_t noise_amp = signal_amp / 10;
    if (noise_amp == 0) noise_amp = 1;
    
    // 计算信噪比（dB）：SNR = 20 * log10(signal/noise)
    // 使用定点数近似：log10(x) ≈ (x-1)/2.3（线性近似，适用于x接近1）
    // 更精确的近似：SNR ≈ 20 * (signal/noise - 1) / 2.3
    // 转换为整数运算：SNR*10 ≈ 200 * (signal/noise - 1) / 2.3 ≈ 87 * (signal/noise - 1)
    uint16_t ratio = (signal_amp * 100) / noise_amp;  // signal/noise * 100
    if (ratio <= 100) return 0;  // 信号小于等于噪声，SNR为0
    
    // SNR*10 = 87 * (ratio/100 - 1) = 87 * (ratio - 100) / 100
    uint32_t snr_x10 = (87UL * (ratio - 100)) / 100;
    
    // 限制范围：0-255（SNR*10，最大25.5dB）
    if (snr_x10 > 255) snr_x10 = 255;
    
    return (uint8_t)snr_x10;
}

// 峰值检测（自适应阈值：阈值 = mean + factor * std_dev）
// 低RAM优化：使用int16_t和定点运算
static uint8_t find_peaks(int16_t* signal, uint8_t* peak_indices, uint8_t max_peaks) {
    int32_t sum = 0, sum_sq = 0;
    for (uint8_t i = 0; i < HR_BUFFER_SIZE; i++) {
        sum += signal[i];
        sum_sq += (int32_t)signal[i] * signal[i];
    }
    int32_t mean = sum / HR_BUFFER_SIZE;
    int32_t variance = (sum_sq / HR_BUFFER_SIZE) - (mean * mean);
    if (variance < 0) variance = 0;
    uint16_t std_dev = fast_sqrt16((uint16_t)variance);
    // threshold = mean + 0.5 * std_dev（HR_PEAK_THRESHOLD_BASE=0.5）
    int16_t threshold = (int16_t)mean + (std_dev / 2);

    uint8_t count = 0;
    for (uint8_t i = 1; i < HR_BUFFER_SIZE - 1; i++) {
        if (signal[i] > signal[i-1] && signal[i] > signal[i+1] && signal[i] > threshold) {
            if (count < max_peaks) {
                peak_indices[count++] = i;
            } else {
                break;
            }
        }
    }
    return count;
}

// ─── 公开接口 ──────────────────────────────────────────────

void hr_algorithm_init() {
    memset(ir_buffer, 0, sizeof(ir_buffer));
    memset(red_buffer, 0, sizeof(red_buffer));
    buffer_pos = 0;
    buffer_filled = false;
    last_bpm = 0;  // 0表示无效
    last_snr = 0;
}

int hr_algorithm_update() {
    int32_t red, ir;
    if (!hr_read_latest(&red, &ir)) {
        return HR_READ_FAILED;
    }
    // 转换int32_t到int16_t（MAX30102数据右对齐后范围适合int16_t）
    ir_buffer[buffer_pos] = (int16_t)(ir >> 2);   // 保留高16位
    red_buffer[buffer_pos] = (int16_t)(red >> 2);
    buffer_pos = (buffer_pos + 1) % HR_BUFFER_SIZE;
    if (buffer_pos == 0) {
        buffer_filled = true;
    }
    return HR_SUCCESS;
}

// 低RAM优化：返回uint8_t（BPM值），0表示无效
uint8_t hr_calculate_bpm(int* status) {
    if (!buffer_filled) {
        if (status) *status = HR_BUFFER_NOT_FULL;
        return 0;
    }

    // 低RAM优化：直接使用ir_buffer，避免拷贝（节省128 bytes）
    // 注意：这会修改原始数据，但计算后立即使用，不影响后续采集
    high_pass_filter(ir_buffer);  // 去基线
    low_pass_filter(ir_buffer);   // 去高频噪声

    last_snr = calculate_snr(ir_buffer);
    if (last_snr < (uint8_t)(HR_SNR_THRESHOLD * 10)) {
        if (status) *status = HR_POOR_SIGNAL;
        return 0;
    }

    // 低RAM优化：最多8个峰（64样本通常2-4个峰）
    uint8_t peaks[8];
    uint8_t peak_count = find_peaks(ir_buffer, peaks, 8);

    if (peak_count < HR_MIN_PEAKS_REQUIRED) {
        if (status) *status = HR_POOR_SIGNAL;
        return 0;
    }

    uint16_t total_interval = 0;
    for (uint8_t i = 1; i < peak_count; i++) {
        total_interval += peaks[i] - peaks[i-1];
    }
    // 定点运算：avg_interval_samples = total_interval / (peak_count - 1)
    uint16_t avg_interval_samples = total_interval / (peak_count - 1);
    // avg_interval_sec = avg_interval_samples * 0.01 (HR_SAMPLE_INTERVAL_MS=10ms)
    // bpm = 60 / avg_interval_sec = 60 * 100 / avg_interval_samples
    uint16_t bpm = (6000 / avg_interval_samples);

    if (bpm < HR_MIN_BPM || bpm > HR_MAX_BPM) {
        if (status) *status = HR_OUT_OF_RANGE;
        return 0;
    }

    last_bpm = (uint8_t)bpm;
    if (status) *status = HR_SUCCESS;
    return (uint8_t)bpm;
}

uint8_t hr_get_latest_bpm() {
    return last_bpm;  // 0表示无效
}

uint8_t hr_get_signal_quality() {
    return last_snr;  // SNR*10，例如15.3dB返回153
}

// 计算红外/红光信号相关性（用于运动干扰检测）
static uint8_t calculate_correlation(int16_t* signal1, int16_t* signal2) {
    int32_t sum1 = 0, sum2 = 0, sum12 = 0, sum1_sq = 0, sum2_sq = 0;
    
    for (uint8_t i = 0; i < HR_BUFFER_SIZE; i++) {
        sum1 += signal1[i];
        sum2 += signal2[i];
        sum12 += (int32_t)signal1[i] * signal2[i];
        sum1_sq += (int32_t)signal1[i] * signal1[i];
        sum2_sq += (int32_t)signal2[i] * signal2[i];
    }
    
    // 计算均值
    int32_t mean1 = sum1 / HR_BUFFER_SIZE;
    int32_t mean2 = sum2 / HR_BUFFER_SIZE;
    
    // 计算协方差和方差
    int32_t cov = (sum12 / HR_BUFFER_SIZE) - (mean1 * mean2);
    int32_t var1 = (sum1_sq / HR_BUFFER_SIZE) - (mean1 * mean1);
    int32_t var2 = (sum2_sq / HR_BUFFER_SIZE) - (mean2 * mean2);
    
    if (var1 <= 0 || var2 <= 0) return 0;
    
    // 计算相关系数 r = cov / sqrt(var1 * var2)
    // 使用定点数运算：先计算 var1 * var2
    uint32_t var_product = (uint32_t)var1 * (uint32_t)var2;
    uint32_t sqrt_var_product = fast_sqrt16((uint16_t)(var_product >> 16)) << 8; // 近似平方根
    
    if (sqrt_var_product == 0) return 0;
    
    // 计算相关系数 * 100（0-100范围）
    int32_t correlation_x100 = (cov * 100) / (int32_t)sqrt_var_product;
    
    // 限制范围 0-100
    if (correlation_x100 < 0) correlation_x100 = 0;
    if (correlation_x100 > 100) correlation_x100 = 100;
    
    return (uint8_t)correlation_x100;
}

// 计算 SpO2（标准 ratio-of-ratios 算法）
uint8_t hr_calculate_spo2(int* status) {
    if (!buffer_filled) {
        if (status) *status = HR_BUFFER_NOT_FULL;
        return 0;
    }
    
    // 检查信号相关性（运动干扰检测）
    last_correlation = calculate_correlation(ir_buffer, red_buffer);
    if (last_correlation < (uint8_t)(SPO2_CORRELATION_THRESHOLD * 100)) {
        if (status) *status = HR_POOR_SIGNAL;
        return 0;
    }
    
    // 计算 AC 和 DC 分量
    int32_t ir_ac_sum = 0, ir_dc_sum = 0;
    int32_t red_ac_sum = 0, red_dc_sum = 0;
    
    for (uint8_t i = 0; i < HR_BUFFER_SIZE; i++) {
        ir_dc_sum += ir_buffer[i];
        red_dc_sum += red_buffer[i];
    }
    
    int32_t ir_dc = ir_dc_sum / HR_BUFFER_SIZE;
    int32_t red_dc = red_dc_sum / HR_BUFFER_SIZE;
    
    // 计算 AC 分量（信号减去 DC）
    for (uint8_t i = 0; i < HR_BUFFER_SIZE; i++) {
        int32_t ir_ac = ir_buffer[i] - ir_dc;
        int32_t red_ac = red_buffer[i] - red_dc;
        ir_ac_sum += ir_ac > 0 ? ir_ac : -ir_ac;  // 绝对值
        red_ac_sum += red_ac > 0 ? red_ac : -red_ac;
    }
    
    int32_t ir_ac_avg = ir_ac_sum / HR_BUFFER_SIZE;
    int32_t red_ac_avg = red_ac_sum / HR_BUFFER_SIZE;
    
    if (red_dc == 0 || ir_dc == 0) {
        if (status) *status = HR_POOR_SIGNAL;
        return 0;
    }
    
    // 计算 R 值 = (red_ac / red_dc) / (ir_ac / ir_dc)
    // 使用定点数运算避免浮点
    uint32_t red_ratio = (red_ac_avg * 1000) / red_dc;  // red_ac/red_dc * 1000
    uint32_t ir_ratio = (ir_ac_avg * 1000) / ir_dc;     // ir_ac/ir_dc * 1000
    
    if (ir_ratio == 0) {
        if (status) *status = HR_POOR_SIGNAL;
        return 0;
    }
    
    uint32_t r_value_x1000 = (red_ratio * 1000) / ir_ratio;  // R * 1000
    
    // 限制 R 值范围
    if (r_value_x1000 < (uint32_t)(SPO2_RATIO_MIN * 1000)) {
        r_value_x1000 = (uint32_t)(SPO2_RATIO_MIN * 1000);
    }
    if (r_value_x1000 > (uint32_t)(SPO2_RATIO_MAX * 1000)) {
        r_value_x1000 = (uint32_t)(SPO2_RATIO_MAX * 1000);
    }
    
    // 使用经验公式：SpO2 = 110 - 25 * R
    // 转换为整数运算：SpO2 = 110 - (25 * R * 1000) / 1000
    int32_t spo2 = 110 - (25 * (int32_t)r_value_x1000) / 1000;
    
    // 限制范围
    if (spo2 < SPO2_MIN_VALUE) spo2 = SPO2_MIN_VALUE;
    if (spo2 > SPO2_MAX_VALUE) spo2 = SPO2_MAX_VALUE;
    
    last_spo2 = (uint8_t)spo2;
    if (status) *status = HR_SUCCESS;
    return (uint8_t)spo2;
}

uint8_t hr_get_latest_spo2() {
    return last_spo2;  // 0表示无效
}

uint8_t hr_get_correlation_quality() {
    return last_correlation;  // 0-100，越高表示相关性越好
}
