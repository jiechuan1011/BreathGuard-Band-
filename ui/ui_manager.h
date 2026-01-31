/*
 * ui_manager.h - UI管理器类头文件
 * 
 * 功能：UI状态机、界面管理、动画控制、事件处理
 * 设计原则：
 * 1. 状态机驱动界面切换
 * 2. 统一的绘制和更新机制
 * 3. 动画系统支持
 * 4. 低功耗优化
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "ui_config.h"
#include <TFT_eSPI.h>
#include <functional>

// 前向声明
class UiComponent;
class HistoryManager;

// ==================== 事件类型 ====================
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON1_PRESS,      // 按键1按下
    EVENT_BUTTON1_RELEASE,    // 按键1释放
    EVENT_BUTTON1_LONG_PRESS, // 按键1长按
    EVENT_BUTTON2_PRESS,      // 按键2按下
    EVENT_BUTTON2_RELEASE,    // 按键2释放
    EVENT_BUTTON2_LONG_PRESS, // 按键2长按
    EVENT_TEST_COMPLETE,      // 测试完成
    EVENT_EMERGENCY_ALERT,    // 紧急警告
    EVENT_SCREEN_TIMEOUT,     // 屏幕超时
    EVENT_BATTERY_LOW,        // 低电量
    EVENT_SENSOR_ERROR,       // 传感器错误
    EVENT_DATA_UPDATE,        // 数据更新
    EVENT_ANIMATION_COMPLETE, // 动画完成
    EVENT_SWIPE_UP,           // 上滑手势
    EVENT_SWIPE_DOWN,         // 下滑手势
    EVENT_SWIPE_LEFT,         // 左滑手势
    EVENT_SWIPE_RIGHT         // 右滑手势
} UiEvent;

// ==================== 动画回调函数类型 ====================
typedef std::function<void(float progress)> AnimationCallback;

// ==================== 动画结构 ====================
typedef struct {
    AnimationType type;
    uint32_t start_time;
    uint32_t duration;
    float start_value;
    float end_value;
    AnimationCallback update_callback;
    AnimationCallback complete_callback;
    bool is_active;
} Animation;

// ==================== UI管理器类 ====================
class UiManager {
public:
    // 单例模式访问
    static UiManager& getInstance();
    
    // 禁止拷贝和赋值
    UiManager(const UiManager&) = delete;
    UiManager& operator=(const UiManager&) = delete;
    
    // ==================== 初始化和管理 ====================
    bool init(TFT_eSPI* display);
    void deinit();
    
    // ==================== 状态管理 ====================
    UiState getCurrentState() const;
    bool setState(UiState new_state, AnimationType animation = ANIMATION_NONE);
    bool isStateTransitioning() const;
    
    // ==================== 事件处理 ====================
    void handleEvent(UiEvent event);
    void processEvents();
    
    // ==================== 绘制和更新 ====================
    void update(uint32_t delta_time);
    void render();
    void forceRedraw();
    
    // ==================== 动画系统 ====================
    bool startAnimation(AnimationType type, uint32_t duration,
                       AnimationCallback update_cb = nullptr,
                       AnimationCallback complete_cb = nullptr);
    void stopAnimation();
    bool isAnimating() const;
    
    // ==================== 数据管理 ====================
    void setTestResult(const TestResult* result);
    const TestResult* getCurrentResult() const;
    HistoryManager* getHistoryManager();
    
    // ==================== 显示控制 ====================
    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const;
    void sleep();
    void wakeup();
    bool isSleeping() const;
    
    // ==================== 性能监控 ====================
    float getFrameRate() const;
    uint32_t getRenderTime() const;
    uint32_t getUpdateTime() const;
    uint32_t getMemoryUsage() const;
    
    // ==================== 调试功能 ====================
    void enableDebug(bool enable);
    void printStats();
    
private:
    // 私有构造函数（单例模式）
    UiManager();
    ~UiManager();
    
    // ==================== 内部方法 ====================
    void initComponents();
    void cleanupComponents();
    
    void updateStateMachine(uint32_t delta_time);
    void updateAnimations(uint32_t delta_time);
    void updateScreenTimeout(uint32_t delta_time);
    
    void renderCurrentState();
    void renderStatusBar();
    void renderDebugOverlay();
    
    void handleButtonEvents();
    void handleSensorEvents();
    void handleSystemEvents();
    
    void transitionToState(UiState new_state);
    void onStateEnter(UiState state);
    void onStateExit(UiState state);
    void onStateUpdate(UiState state, uint32_t delta_time);
    
    // ==================== 状态处理函数 ====================
    void handleMainState(UiEvent event);
    void handleTestSelectState(UiEvent event);
    void handleAcetoneTestingState(UiEvent event);
    void handleHeartRateTestingState(UiEvent event);
    void handleComprehensiveTestingState(UiEvent event);
    void handleResultDisplayState(UiEvent event);
    void handleHistoryState(UiEvent event);
    void handleSettingsState(UiEvent event);
    void handleEmergencyAlertState(UiEvent event);
    void handleBreathingGuideState(UiEvent event);
    
    // ==================== 成员变量 ====================
    TFT_eSPI* display_;              // 显示对象
    UiState current_state_;          // 当前状态
    UiState target_state_;           // 目标状态
    UiState previous_state_;         // 前一个状态
    
    Animation current_animation_;    // 当前动画
    uint32_t state_start_time_;      // 状态开始时间
    uint32_t last_update_time_;      // 上次更新时间
    uint32_t last_render_time_;      // 上次渲染时间
    uint32_t screen_timeout_timer_;  // 屏幕超时计时器
    
    TestResult current_result_;      // 当前测试结果
    HistoryManager* history_manager_; // 历史记录管理器
    
    // 性能监控
    uint32_t frame_count_;
    uint32_t frame_time_total_;
    uint32_t update_time_total_;
    uint32_t render_time_total_;
    uint32_t last_stat_time_;
    
    // 显示控制
    uint8_t brightness_;
    bool is_sleeping_;
    bool needs_redraw_;
    bool partial_update_;
    
    // 调试
    bool debug_enabled_;
    uint32_t memory_usage_;
    
    // 组件指针
    UiComponent* status_bar_;
    UiComponent* current_screen_;
    
    // 按键状态
    struct {
        bool btn1_pressed;
        bool btn2_pressed;
        uint32_t btn1_press_time;
        uint32_t btn2_press_time;
        bool btn1_long_press_detected;
        bool btn2_long_press_detected;
    } button_state_;
    
    // 事件队列
    static const int MAX_EVENTS = 16;
    UiEvent event_queue_[MAX_EVENTS];
    uint8_t event_queue_head_;
    uint8_t event_queue_tail_;
    
    // 添加事件到队列
    void queueEvent(UiEvent event);
    UiEvent dequeueEvent();
    bool isEventQueueEmpty() const;
    bool isEventQueueFull() const;
    
    // 动画辅助函数
    float calculateAnimationProgress() const;
    void applyAnimationTransform(float progress);
    
    // 显示优化
    void applyAmoledOptimizations();
    void shiftPixels();
    uint32_t last_pixel_shift_time_;
    
    // 帧率控制
    uint32_t target_frame_time_;
    uint32_t last_frame_time_;
    
    // 医疗特性
    bool emergency_alert_active_;
    uint32_t alert_start_time_;
    
    // 测试状态
    TestType current_test_type_;
    uint32_t test_start_time_;
    uint32_t test_duration_;
    float test_progress_;
    
    // 呼吸引导状态
    uint32_t breathing_start_time_;
    bool is_inhaling_;
    
    // 波形数据
    int16_t waveform_data_[WAVEFORM_SAMPLES];
    uint8_t waveform_index_;
};

// ==================== 全局访问函数 ====================
#ifdef __cplusplus
extern "C" {
#endif

UiManager* ui_manager_get_instance();
bool ui_manager_init(void* display);
void ui_manager_update(uint32_t delta_time);
void ui_manager_render();
void ui_manager_handle_event(uint8_t event);
void ui_manager_set_brightness(uint8_t percent);
void ui_manager_sleep();
void ui_manager_wakeup();

#ifdef __cplusplus
}
#endif

#endif // UI_MANAGER_H