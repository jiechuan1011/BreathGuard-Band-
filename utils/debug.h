#ifndef DEBUG_H
#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

// 调试级别枚举
typedef enum {
    DEBUG_NONE = 0,
    DEBUG_ERROR = 1,
    DEBUG_WARNING = 2,
    DEBUG_INFO = 3,
    DEBUG_DEBUG = 4
} DebugLevel;

// 设置全局调试级别
void debug_set_level(DebugLevel level);

// 输出不同级别的调试信息
void debug_print(DebugLevel level, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif // DEBUG_H