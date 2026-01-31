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
