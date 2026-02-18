#ifndef SNO2_DRIVER_H
#define SNO2_DRIVER_H

#include <Arduino.h>
// ──────────────────────────────────────────────
// SnO2 驱动参数与宏（从 drivers/sno2_driver.h 同步）
// 引脚配置（根据用户要求）
#ifndef SNO2_HEATER_PIN
#define SNO2_HEATER_PIN      5    // GPIO5 - MOSFET加热控制
#endif
#ifndef SNO2_ADC_PIN
#define SNO2_ADC_PIN         4    // GPIO4 - AD623放大后ADC输入
#endif

// 加热控制参数
#ifndef SNO2_HEAT_DURATION_MS
#define SNO2_HEAT_DURATION_MS   8000    // 加热8秒
#endif
#ifndef SNO2_CYCLE_INTERVAL_MS
#define SNO2_CYCLE_INTERVAL_MS  40000   // 每40秒一次加热周期
#endif
#ifndef SNO2_HEATER_ON
#define SNO2_HEATER_ON          1       // 高电平加热
#endif
#ifndef SNO2_HEATER_OFF
#define SNO2_HEATER_OFF         0       // 低电平关闭
#endif

// ADC配置（ESP32-C3）
#ifndef SNO2_ADC_REF_MV
#define SNO2_ADC_REF_MV         3300    // 3.3V参考电压
#endif
#ifndef SNO2_ADC_RESOLUTION
#define SNO2_ADC_RESOLUTION     4096    // 12位ADC
#endif
#ifndef SNO2_ADC_SAMPLES
#define SNO2_ADC_SAMPLES        16      // 采集样本数（必须是2的幂次，便于移位）
#endif

// 浓度计算参数（定点运算，使用Q格式）
#ifndef SNO2_Q_FRACTION_BITS
#define SNO2_Q_FRACTION_BITS    6
#endif
#ifndef SNO2_Q_SCALE
#define SNO2_Q_SCALE            (1 << SNO2_Q_FRACTION_BITS)  // 64
#endif

// 线性标定参数（ppm = a * voltage_mv + b）
#ifndef SNO2_CALIB_A_Q
#define SNO2_CALIB_A_Q          (int16_t)(0.5 * SNO2_Q_SCALE)   // a=0.5
#endif
#ifndef SNO2_CALIB_B_Q
#define SNO2_CALIB_B_Q          (int16_t)(-100 * SNO2_Q_SCALE)  // b=-100
#endif

// 电压与浓度范围
#ifndef SNO2_VOLTAGE_MIN_MV
#define SNO2_VOLTAGE_MIN_MV     0
#endif
#ifndef SNO2_VOLTAGE_MAX_MV
#define SNO2_VOLTAGE_MAX_MV     3300
#endif
#ifndef SNO2_CONC_MIN_PPM
#define SNO2_CONC_MIN_PPM       0
#endif
#ifndef SNO2_CONC_MAX_PPM
#define SNO2_CONC_MAX_PPM       1000
#endif

// 可选包含项目级别 pin 配置（如需要覆盖宏，可在 config/pin_config.h 中定义）
#include "config/pin_config.h"

// Copy of drivers/sno2_driver.h content (shortened for brevity)
// Full header moved here to lib include so LDF can find it.

typedef enum {
    SNO2_STATE_IDLE = 0,
    SNO2_STATE_HEATING,
    SNO2_STATE_SAMPLING,
    SNO2_STATE_CALCULATING,
    SNO2_STATE_ERROR
} Sno2State;

typedef struct {
    uint16_t voltage_mv;
    uint16_t concentration_ppm;
    uint8_t valid : 1;
    uint8_t heater_on : 1;
    uint8_t reserved : 6;
} Sno2Data;

void sno2_init();
void sno2_update();
Sno2State sno2_get_state();
Sno2Data sno2_get_data();
void sno2_set_calibration(float a, float b);
uint8_t sno2_is_heater_on();
uint32_t sno2_get_heating_remaining();
uint32_t sno2_get_next_sample_time();

#endif // SNO2_DRIVER_H
