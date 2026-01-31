# 糖尿病筛查手表UI系统

## 概述

这是一个为糖尿病筛查监测系统设计的嵌入式UI系统，专门针对ESP32-S3R8N8微控制器和1.85英寸AMOLED屏幕（390×450像素）优化。系统采用组件化设计，支持医疗级界面显示、实时数据可视化、动画效果和低功耗优化。

## 硬件要求

- **MCU**: ESP32-S3R8N8
- **屏幕**: 1.85英寸AMOLED，390×450像素，16位色彩
- **按键**: 2个物理按键（推荐使用GPIO0和GPIO1）
- **通信**: BLE 5.0（用于数据传输）
- **传感器**: 心率传感器、血氧传感器、丙酮传感器

## 软件架构

```
ui/
├── ui_config.h          # UI配置文件（颜色、尺寸、阈值等）
├── ui_manager.h         # UI管理器（状态机、事件处理）
├── ui_component.h       # UI组件基类
├── status_bar.h         # 状态栏组件
├── metric_card.h        # 数据卡片组件
├── progress_ring.h      # 进度环组件
├── waveform_view.h      # 波形视图组件
├── main_screen.h        # 主界面组件
└── example_main.cpp     # 示例程序
```

## 核心特性

### 1. 医疗级界面设计
- 颜色编码：绿色（正常）、橙色（警告）、红色（危险）
- 趋势指示：上升/下降箭头，趋势分析
- 数据可信度显示
- 紧急警告系统

### 2. 实时数据可视化
- 心电图波形显示
- 呼吸波形显示
- 丙酮浓度趋势图
- 实时数据卡片

### 3. 动画系统
- 60fps流畅动画
- 多种动画类型：滑动、淡入淡出、缩放
- 呼吸引导动画
- 进度动画

### 4. 低功耗优化
- AMOLED黑色背景优化
- 动态亮度调节
- 屏幕超时休眠
- 像素偏移防烧屏

### 5. 交互设计
- 2个物理按键支持
- 手势识别（上滑/下滑）
- 长按/短按识别
- 触觉反馈

## 快速开始

### 1. 安装依赖

确保已安装以下库：
- TFT_eSPI（显示驱动）
- Arduino框架

### 2. 配置TFT_eSPI

在`User_Setup.h`中配置屏幕参数：

```cpp
#define ST7789_DRIVER
#define TFT_WIDTH  390
#define TFT_HEIGHT 450
#define TFT_MISO   -1
#define TFT_MOSI   11
#define TFT_SCLK   12
#define TFT_CS     10
#define TFT_DC     9
#define TFT_RST    8
#define TFT_BL     2
```

### 3. 基本使用示例

```cpp
#include <TFT_eSPI.h>
#include "ui_manager.h"
#include "main_screen.h"

TFT_eSPI tft;
UiManager& ui = UiManager::getInstance();

void setup() {
    // 初始化显示
    tft.init();
    tft.setRotation(1);
    
    // 初始化UI
    ui.init(&tft);
    
    // 创建主界面
    Rect bounds = {0, 0, 390, 450};
    UiComponent* screen = main_screen_create_standard("主界面", &bounds);
    
    // 设置亮度
    ui.setBrightness(80);
}

void loop() {
    uint32_t delta_time = 16; // 60fps
    ui.update(delta_time);
    ui.render();
    delay(16);
}
```

## 组件使用

### 状态栏组件

```cpp
// 创建状态栏
Rect status_bounds = {0, 0, 390, 32};
UiComponent* status_bar = status_bar_create("状态栏", &status_bounds);

// 更新数据
status_bar_update_time(status_bar, 14, 30, 0);
status_bar_update_battery(status_bar, 85, BATTERY_HIGH);
status_bar_update_bluetooth(status_bar, CONNECTION_CONNECTED);
```

### 数据卡片组件

```cpp
// 创建心率卡片
Rect card_bounds = {20, 50, 120, 80};
UiComponent* hr_card = metric_card_create_heart_rate("心率", &card_bounds);

// 更新数据
metric_card_set_value(hr_card, 75);
metric_card_set_status(hr_card, VALUE_NORMAL);
metric_card_set_trend(hr_card, TREND_STABLE);
```

### 进度环组件

```cpp
// 创建测试进度环
Rect ring_bounds = {95, 150, 200, 200};
UiComponent* progress = progress_ring_create_acetone_test("丙酮测试", &ring_bounds);

// 开始测试
progress_ring_start_test(progress, 60000, "丙酮检测");
progress_ring_set_progress(progress, 0.5); // 50%进度
```

### 波形视图组件

```cpp
// 创建心电图视图
Rect wave_bounds = {20, 200, 350, 100};
UiComponent* ecg_view = waveform_view_create_ecg("心电图", &wave_bounds);

// 添加数据点
waveform_view_add_point(ecg_view, 1.2, 0); // 通道0
waveform_view_add_point(ecg_view, 1.5, 0);
waveform_view_add_point(ecg_view, 1.3, 0);
```

## 界面状态管理

系统支持10种界面状态：

```cpp
typedef enum {
    UI_STATE_MAIN,              // 主界面
    UI_STATE_TEST_SELECT,       // 测试选择
    UI_STATE_ACETONE_TESTING,   // 丙酮检测
    UI_STATE_HEART_RATE_TESTING,// 心率血氧检测
    UI_STATE_COMPREHENSIVE_TESTING, // 综合检测
    UI_STATE_RESULT_DISPLAY,    // 结果展示
    UI_STATE_HISTORY,           // 历史记录
    UI_STATE_SETTINGS,          // 设置
    UI_STATE_EMERGENCY_ALERT,   // 紧急警告
    UI_STATE_BREATHING_GUIDE    // 呼吸引导
} UiState;
```

切换界面：

```cpp
// 切换到测试选择界面（带滑动动画）
ui.setState(UI_STATE_TEST_SELECT, ANIMATION_SLIDE_LEFT);

// 切换到结果界面（带淡入动画）
ui.setState(UI_STATE_RESULT_DISPLAY, ANIMATION_FADE_IN);
```

## 事件处理

系统支持多种事件类型：

```cpp
typedef enum {
    EVENT_BUTTON1_PRESS,        // 按键1按下
    EVENT_BUTTON1_RELEASE,      // 按键1释放
    EVENT_BUTTON1_LONG_PRESS,   // 按键1长按
    EVENT_BUTTON2_PRESS,        // 按键2按下
    EVENT_TEST_COMPLETE,        // 测试完成
    EVENT_EMERGENCY_ALERT,      // 紧急警告
    EVENT_DATA_UPDATE,          // 数据更新
    EVENT_SWIPE_UP,             // 上滑手势
    EVENT_SWIPE_DOWN            // 下滑手势
} UiEvent;
```

处理事件：

```cpp
void handleEvent(UiEvent event) {
    switch (event) {
        case EVENT_BUTTON1_PRESS:
            Serial.println("按键1按下");
            break;
        case EVENT_SWIPE_UP:
            ui.setState(UI_STATE_TEST_SELECT, ANIMATION_SLIDE_LEFT);
            break;
        case EVENT_EMERGENCY_ALERT:
            ui.setState(UI_STATE_EMERGENCY_ALERT, ANIMATION_SCALE);
            break;
    }
}
```

## 医疗阈值配置

在`ui_config.h`中配置医疗阈值：

```cpp
// 心率阈值
#define HEART_RATE_NORMAL_MIN 60
#define HEART_RATE_NORMAL_MAX 100
#define HEART_RATE_WARNING_MIN 50
#define HEART_RATE_WARNING_MAX 120

// 血氧阈值
#define SPO2_NORMAL_MIN 95
#define SPO2_WARNING_MIN 90

// 丙酮阈值
#define ACETONE_NORMAL_MAX 10.0    // ppm
#define ACETONE_WARNING_MAX 25.0   // ppm
#define ACETONE_DANGER_MAX 50.0    // ppm
```

## 性能优化

### 内存管理
- UI内存限制：8KB
- 双缓冲渲染
- 动态内存分配控制

### 渲染优化
- 部分区域更新
- 脏矩形渲染
- 60fps帧率控制

### 功耗优化
- AMOLED黑色背景
- 动态亮度（10%-100%）
- 屏幕超时（30秒）
- 像素偏移防烧屏

## 调试功能

启用调试模式：

```cpp
ui.enableDebug(true);
ui.printStats();
```

输出示例：
```
[UI状态] 帧率: 58.5 fps
[UI状态] 内存: 5120 bytes
[UI状态] 渲染时间: 12 ms
[UI状态] 更新时间: 3 ms
[UI状态] 当前界面: 主界面
```

## 集成指南

### 1. 与传感器集成

```cpp
void update_sensor_data() {
    // 读取心率传感器
    uint8_t heart_rate = read_heart_rate_sensor();
    
    // 读取血氧传感器
    uint8_t blood_oxygen = read_blood_oxygen_sensor();
    
    // 读取丙酮传感器
    float acetone = read_acetone_sensor();
    
    // 更新UI
    TestResult result;
    result.heart_rate = heart_rate;
    result.blood_oxygen = blood_oxygen;
    result.acetone_level = acetone;
    result.timestamp = millis() / 1000;
    
    ui.setTestResult(&result);
    ui.handleEvent(EVENT_DATA_UPDATE);
}
```

### 2. 与BLE集成

```cpp
void on_ble_data_received(uint8_t* data, size_t length) {
    // 解析BLE数据
    parse_ble_data(data, length);
    
    // 更新UI
    ui.handleEvent(EVENT_DATA_UPDATE);
}

void send_ui_state_via_ble() {
    UiState current_state = ui.getCurrentState();
    const TestResult* result = ui.getCurrentResult();
    
    // 通过BLE发送UI状态和数据
    send_via_ble(current_state, result);
}
```

### 3. 与电源管理集成

```cpp
void handle_power_management() {
    uint8_t battery_level = read_battery_level();
    
    // 低电量警告
    if (battery_level < 20) {
        ui.handleEvent(EVENT_BATTERY_LOW);
    }
    
    // 屏幕超时
    if (ui.isSleeping()) {
        // 进入低功耗模式
        enter_low_power_mode();
    }
}
```

## 故障排除

### 常见问题

1. **屏幕不显示**
   - 检查TFT_eSPI配置
   - 验证引脚连接
   - 检查背光控制

2. **UI响应慢**
   - 降低帧率（30fps）
   - 减少动画复杂度
   - 启用部分更新

3. **内存不足**
   - 减少同时显示的组件
   - 使用更小的缓冲区
   - 启用内存压缩

4. **动画卡顿**
   - 检查帧率限制
   - 优化渲染代码
   - 使用硬件加速

### 调试建议

1. 启用串口调试输出
2. 使用性能监控功能
3. 逐步增加组件复杂度
4. 测试不同硬件配置

## 扩展开发

### 添加新组件

1. 继承`UiComponent`基类
2. 实现`update()`和`render()`方法
3. 注册到UI管理器
4. 添加事件处理

### 自定义主题

1. 修改`ui_config.h`中的颜色定义
2. 创建新的样式配置
3. 实现主题切换功能

### 多语言支持

1. 创建语言资源文件
2. 实现文本替换系统
3. 添加语言切换功能

## 许可证

本项目采用MIT许可证。详见LICENSE文件。

## 贡献指南

1. Fork项目
2. 创建功能分支
3. 提交更改
4. 创建Pull Request

## 联系方式

如有问题或建议，请通过以下方式联系：
- 项目仓库：https://github.com/your-repo
- 邮箱：contact@example.com
- 文档：https://docs.example.com

---

**版本**: 1.0.0  
**更新日期**: 2026-01-31  
**作者**: 糖尿病筛查系统开发团队