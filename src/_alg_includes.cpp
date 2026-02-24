#include "../algorithm/hr_algorithm.cpp"
#include "../algorithm/motion_correction.cpp"
#include "../algorithm/data_filter.cpp"
#include "../algorithm/risk_assessment.cpp"

// scheduler implementation exists outside src directory; include directly
#include "../system/scheduler.cpp"

// system state needs to be linked as well (contains functions used by scheduler/hr)
#include "../system/system_state.cpp"
