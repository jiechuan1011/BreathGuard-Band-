#include "scheduler.h"
#include "system_state.h"
#include <Arduino.h>
#include "../algorithm/hr_algorithm.h"
#include "gas_driver.h"
#include "env_driver.h"

// ──────────────────────────────────────────────
// 调度周期配置
#define HR_CALC_INTERVAL_MS      2000    // 心率计算周期（2秒）
#define GAS_POLL_INTERVAL_MS     1000    // 气体轮询周期（1秒）
#define ENV_POLL_INTERVAL_MS     2000    // 环境轮询周期（2秒）

void scheduler_init() {
    // 初始化心率算法
    hr_algorithm_init();

#ifdef DEVICE_ROLE_DETECTOR
    // 仅检测模块初始化气体传感器
    gas_init();

    // 初始化环境传感器
    env_init();
#endif

    // 初始化系统状态
    system_state_init();
}

void scheduler_run() {
    unsigned long now = millis();

    // ─── 心率采样（高频，10ms） ──────────────────────────────
    static unsigned long last_hr_update = 0;
    if (now - last_hr_update >= HR_SAMPLE_INTERVAL_MS) {
        int update_status = hr_algorithm_update();
        last_hr_update = now;
        // 更新失败可记录日志，但不阻塞
    }

    // ─── 心率计算（中频，2秒） ──────────────────────────────
    static unsigned long last_hr_calc = 0;
    if (now - last_hr_calc >= HR_CALC_INTERVAL_MS) {
        int calc_status;
        uint8_t bpm = hr_calculate_bpm(&calc_status);
        uint8_t snr_x10 = hr_get_signal_quality();
        
#ifdef DEVICE_ROLE_WRIST
        // 腕带模式：同时计算 SpO2
        uint8_t spo2 = hr_calculate_spo2(&calc_status);
        uint8_t correlation = hr_get_correlation_quality();
        system_state_set_hr_spo2(bpm, spo2, snr_x10, correlation, (int8_t)calc_status);
#else
        // 检测模块模式：只设置心率
        system_state_set_hr(bpm, snr_x10, (int8_t)calc_status);
#endif
        
        last_hr_calc = now;
    }

#ifdef DEVICE_ROLE_DETECTOR
    // ─── 气体传感器轮询（低频，1秒） ──────────────────────────────
    static unsigned long last_gas_poll = 0;
    if (now - last_gas_poll >= GAS_POLL_INTERVAL_MS) {
        float voltage_mv, conc_ppm;
        if (gas_read(&voltage_mv, &conc_ppm)) {
            // 只有有效数据才更新状态（转换为uint16_t）
            uint16_t v_mv = (uint16_t)(voltage_mv + 0.5f);  // 四舍五入
            uint16_t c_ppm_x10 = (uint16_t)(conc_ppm * 10.0f + 0.5f);  // ppm*10存储
            system_state_set_gas(v_mv, c_ppm_x10, true);
        } else {
            // 预热中或无呼吸触发，标记无效但不更新数值
            system_state_set_gas(0, 0, false);
        }
        last_gas_poll = now;
    }

    // ─── 环境传感器轮询（低频，2秒） ──────────────────────────────
    static unsigned long last_env_poll = 0;
    if (now - last_env_poll >= ENV_POLL_INTERVAL_MS) {
        EnvData env;
        if (env_read(&env) && env.valid) {
            // 转换为int8_t和uint8_t
            int8_t temp = (int8_t)(env.temperature_c + 0.5f);
            uint8_t rh = (uint8_t)(env.humidity_rh + 0.5f);
            system_state_set_env(temp, rh, true);
        } else {
            system_state_set_env(0, 0, false);
        }
        last_env_poll = now;
    }
#endif
}
