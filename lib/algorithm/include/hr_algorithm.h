// Copied from algorithm/hr_algorithm.h for LDF
#ifndef HR_ALGORITHM_H
#define HR_ALGORITHM_H

#include <Arduino.h>
#include "hr_driver.h"

// configuration defaults (kept from original)
#ifndef HR_BUFFER_SIZE
#define HR_BUFFER_SIZE          128
#endif

#ifndef HR_SAMPLE_INTERVAL_MS
#define HR_SAMPLE_INTERVAL_MS   10
#endif

#ifndef HR_MIN_PEAKS_REQUIRED
#define HR_MIN_PEAKS_REQUIRED   3
#endif

// function declarations
void hr_algorithm_init();
int hr_algorithm_update();
uint8_t hr_calculate_bpm(int* status);
uint8_t hr_calculate_spo2(int* status);
uint8_t hr_get_latest_bpm();
uint8_t hr_get_latest_spo2();
uint8_t hr_get_signal_quality();
uint8_t hr_get_correlation_quality();

#endif // HR_ALGORITHM_H
