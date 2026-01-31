/*
 * ui_config.h - 手表端UI配置文件
 * 
 * 硬件规格：
 * - 屏幕：1.85英寸 AMOLED
 * - 分辨率：390×450像素
 * - 色彩深度：16位（565格式）
 * - MCU：ESP32-S3R8N8
 * - 按键：2个物理按键（BTN1/BTN2）
 * 
 * 设计原则：
 * 1. 医疗级界面设计
 * 2. AMOLED优化（深色主题）
 * 3. 60fps流畅动画
 * 4. 低功耗设计
 */

#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <stdint.h>

// ==================== 屏幕配置 ====================
#define SCREEN_WIDTH          390
#define SCREEN_HEIGHT         450
#define SCREEN_CENTER_X       (SCREEN_WIDTH / 2)
#define SCREEN_CENTER_Y       (SCREEN_HEIGHT / 2)

// ==================== 颜色定义（16位565格式）====================
// 基础颜色
#define COLOR_BLACK           0x0000      // 纯黑（AMOLED省电）
#define COLOR_WHITE           0xFFFF      // 白色
#define COLOR_GRAY_LIGHT      0xCE79      // 浅灰
#define COLOR_GRAY_MEDIUM     0x8C71      // 中灰
#define COLOR_GRAY_DARK       0x4A69      // 深灰

// 医疗状态颜色
#define COLOR_NORMAL          0x07E0      // 绿色 - 正常值
#define COLOR_WARNING         0xFDA0      // 橙色 - 警告值
#define COLOR_DANGER          0xF800      // 红色 - 危险值
#define COLOR_INFO            0x04FF      // 蓝色 - 信息
#define COLOR_SUCCESS         0x07E0      // 绿色 - 成功

// 界面元素颜色
#define COLOR_BACKGROUND      COLOR_BLACK // 背景色（纯黑省电）
#define COLOR_TEXT_PRIMARY    COLOR_WHITE // 主要文字
#define COLOR_TEXT_SECONDARY  COLOR_GRAY_LIGHT // 次要文字
#define COLOR_CARD_BG         0x1082      // 卡片背景（深灰）
#define COLOR_BORDER          0x3186      // 边框颜色

// ==================== 字体配置 ====================
#define FONT_SIZE_TITLE       24          // 主标题 24pt
#define FONT_SIZE_BODY        16          // 正文 16pt
#define FONT_SIZE_SMALL       12          // 小字 12pt
#define FONT_SIZE_LARGE       32          // 大数字 32pt

// ==================== 间距和布局 ====================
#define MARGIN_SMALL          4           // 小间距 4px
#define MARGIN_MEDIUM         8           // 中间距 8px
#define MARGIN_LARGE          16          // 大间距 16px
#define MARGIN_XLARGE         24          // 超大间距 24px

#define PADDING_SMALL         4           // 内边距 4px
#define PADDING_MEDIUM        8           // 内边距 8px
#define PADDING_LARGE         12          // 内边距 12px

#define BORDER_RADIUS_SMALL   4           // 小圆角 4px
#define BORDER_RADIUS_MEDIUM  8           // 中圆角 8px
#define BORDER_RADIUS_LARGE   12          // 大圆角 12px

// ==================== 组件尺寸 ====================
#define STATUS_BAR_HEIGHT     32          // 状态栏高度
#define CARD_HEIGHT           80          // 数据卡片高度
#define CARD_WIDTH            120         // 数据卡片宽度
#define BUTTON_HEIGHT         40          // 按钮高度
#define BUTTON_WIDTH          120         // 按钮宽度
#define PROGRESS_RING_SIZE    150         // 环形进度条尺寸
#define WAVEFORM_HEIGHT       100         // 波形视图高度

// ==================== 动画配置 ====================
#define ANIMATION_DURATION_FAST   100     // 快速动画 100ms
#define ANIMATION_DURATION_NORMAL 300     // 正常动画 300ms
#define ANIMATION_DURATION_SLOW   500     // 慢速动画 500ms

#define FRAME_RATE_MAIN       60          // 主界面帧率 60fps
#define FRAME_RATE_TESTING    30          // 检测界面帧率 30fps
#define FRAME_RATE_IDLE       1           // 空闲帧率 1fps

// ==================== 性能配置 ====================
#define UI_MEMORY_LIMIT       8192        // UI内存限制 8KB
#define RESPONSE_TIME_LIMIT   50          // 响应时间限制 50ms
#define STARTUP_TIME_LIMIT    500         // 启动时间限制 500ms

// ==================== 按键配置 ====================
#define BUTTON_DEBOUNCE_MS    20          // 按键消抖时间 20ms
#define BUTTON_LONG_PRESS_MS  1000        // 长按时间 1000ms

// ==================== 医疗阈值 ====================
#define HEART_RATE_NORMAL_MIN 60          // 心率正常最小值
#define HEART_RATE_NORMAL_MAX 100         // 心率正常最大值
#define HEART_RATE_WARNING_MIN 50         // 心率警告最小值
#define HEART_RATE_WARNING_MAX 120        // 心率警告最大值

#define SPO2_NORMAL_MIN       95          // 血氧正常最小值
#define SPO2_WARNING_MIN      90          // 血氧警告最小值

#define ACETONE_NORMAL_MAX    10.0        // 丙酮正常最大值 ppm
#define ACETONE_WARNING_MAX   25.0        // 丙酮警告最大值 ppm
#define ACETONE_DANGER_MAX    50.0        // 丙酮危险最大值 ppm

// ==================== 测试时间配置 ====================
#define TEST_TIME_ACETONE     60          // 丙酮检测时间 60秒
#define TEST_TIME_HEART_RATE  30          // 心率血氧检测时间 30秒
#define TEST_TIME_COMPREHENSIVE 90        // 综合检测时间 90秒

// ==================== 历史记录配置 ====================
#define HISTORY_MAX_RECORDS   50          // 最大历史记录数
#define TREND_DAYS            7           // 趋势分析天数

// ==================== AMOLED优化配置 ====================
#define AMOLED_BRIGHTNESS_MIN 10          // 最小亮度 10%
#define AMOLED_BRIGHTNESS_MAX 100         // 最大亮度 100%
#define AMOLED_BRIGHTNESS_IDLE 10         // 空闲亮度 10%
#define AMOLED_BRIGHTNESS_ACTIVE 80       // 活跃亮度 80%

#define PIXEL_SHIFT_INTERVAL  30000       // 像素偏移间隔 30秒
#define SCREEN_TIMEOUT_MS     30000       // 屏幕超时 30秒

// ==================== 呼吸引导配置 ====================
#define BREATHING_CYCLE_MS    4000        // 呼吸周期 4秒
#define BREATHING_INHALE_MS   2000        // 吸气时间 2秒
#define BREATHING_EXHALE_MS   2000        // 呼气时间 2秒

// ==================== 波形配置 ====================
#define WAVEFORM_SAMPLES      100         // 波形采样点数
#define WAVEFORM_UPDATE_MS    50          // 波形更新间隔 50ms
#define ECG_AMPLITUDE         30          // 心电图振幅

// ==================== 枚举定义 ====================
typedef enum {
    UI_STATE_MAIN = 0,           // 主界面
    UI_STATE_TEST_SELECT,        // 测试选择界面
    UI_STATE_ACETONE_TESTING,    // 丙酮检测界面
    UI_STATE_HEART_RATE_TESTING, // 心率血氧检测界面
    UI_STATE_COMPREHENSIVE_TESTING, // 综合检测界面
    UI_STATE_RESULT_DISPLAY,     // 结果展示界面
    UI_STATE_HISTORY,            // 历史记录界面
    UI_STATE_SETTINGS,           // 设置界面
    UI_STATE_EMERGENCY_ALERT,    // 紧急警告界面
    UI_STATE_BREATHING_GUIDE,    // 呼吸引导界面
    UI_STATE_COUNT               // 状态总数
} UiState;

typedef enum {
    TEST_TYPE_NONE = 0,
    TEST_TYPE_ACETONE,
    TEST_TYPE_HEART_RATE,
    TEST_TYPE_COMPREHENSIVE
} TestType;

typedef enum {
    TREND_NORMAL = 0,    // 正常
    TREND_ATTENTION,     // 注意
    TREND_CONCERN,       // 建议关注
    TREND_EMERGENCY      // 紧急
} TrendLevel;

typedef enum {
    ANIMATION_NONE = 0,
    ANIMATION_SLIDE_LEFT,
    ANIMATION_SLIDE_RIGHT,
    ANIMATION_FADE_IN,
    ANIMATION_FADE_OUT,
    ANIMATION_SCALE
} AnimationType;

// ==================== 数据结构 ====================
typedef struct {
    uint32_t timestamp;          // 时间戳
    uint8_t heart_rate;          // 心率 bpm
    uint8_t blood_oxygen;        // 血氧 %
    float acetone_level;         // 丙酮浓度 ppm
    TrendLevel trend;            // 趋势
    uint8_t confidence;          // 数据可信度 0-100%
    TestType test_type;          // 测试类型
} TestResult;

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} Rect;

typedef struct {
    uint8_t r : 5;
    uint8_t g : 6;
    uint8_t b : 5;
} Color565;

// ==================== 工具函数宏 ====================
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
