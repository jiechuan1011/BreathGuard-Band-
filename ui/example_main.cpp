/*
 * example_main.cpp - 糖尿病筛查手表UI示例程序
 * 
 * 功能：展示完整的UI系统使用方式
 * 硬件：ESP32-S3R8N8 + 1.85英寸AMOLED (390×450)
 * 库依赖：TFT_eSPI
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "ui_config.h"
#include "ui_manager.h"
#include "main_screen.h"
#include "status_bar.h"
#include "metric_card.h"
#include "progress_ring.h"
#include "waveform_view.h"

// ==================== 硬件配置 ====================
TFT_eSPI tft = TFT_eSPI();      // 显示对象
UiManager& ui = UiManager::getInstance(); // UI管理器

// ==================== 引脚定义 ====================
#define PIN_BTN1 0              // 按键1
#define PIN_BTN2 1              // 按键2
#define PIN_BACKLIGHT 2         // 背光控制

// ==================== 全局变量 ====================
uint32_t last_update_time = 0;
uint32_t last_button_check = 0;
bool btn1_pressed = false;
bool btn2_pressed = false;

// ==================== 测试数据 ====================
TestResult test_results[] = {
    {.timestamp = 1706594400, .heart_rate = 75, .blood_oxygen = 98, .acetone_level = 5.2, .trend = TREND_NORMAL, .confidence = 95, .test_type = TEST_TYPE_COMPREHENSIVE},
    {.timestamp = 1706508000, .heart_rate = 72, .blood_oxygen = 97, .acetone_level = 4.8, .trend = TREND_NORMAL, .confidence = 92, .test_type = TEST_TYPE_COMPREHENSIVE},
    {.timestamp = 1706421600, .heart_rate = 78, .blood_oxygen = 96, .acetone_level = 6.1, .trend = TREND_ATTENTION, .confidence = 88, .test_type = TEST_TYPE_COMPREHENSIVE},
    {.timestamp = 1706335200, .heart_rate = 80, .blood_oxygen = 95, .acetone_level = 7.3, .trend = TREND_CONCERN, .confidence = 85, .test_type = TEST_TYPE_COMPREHENSIVE},
    {.timestamp = 1706248800, .heart_rate = 76, .blood_oxygen = 98, .acetone_level = 5.5, .trend = TREND_NORMAL, .confidence = 90, .test_type = TEST_TYPE_COMPREHENSIVE}
};

// ==================== 按键处理函数 ====================
void check_buttons() {
    uint32_t current_time = millis();
    
    // 消抖处理
    if (current_time - last_button_check < 20) {
        return;
    }
    last_button_check = current_time;
    
    // 读取按键状态
    bool btn1_current = digitalRead(PIN_BTN1) == LOW;
    bool btn2_current = digitalRead(PIN_BTN2) == LOW;
    
    // 检测按键按下
    if (btn1_current && !btn1_pressed) {
        ui.handleEvent(EVENT_BUTTON1_PRESS);
        btn1_pressed = true;
    }
    
    if (btn2_current && !btn2_pressed) {
        ui.handleEvent(EVENT_BUTTON2_PRESS);
        btn2_pressed = true;
    }
    
    // 检测按键释放
    if (!btn1_current && btn1_pressed) {
        ui.handleEvent(EVENT_BUTTON1_RELEASE);
        btn1_pressed = false;
    }
    
    if (!btn2_current && btn2_pressed) {
        ui.handleEvent(EVENT_BUTTON2_RELEASE);
        btn2_pressed = false;
    }
}

// ==================== 模拟数据更新 ====================
void update_sensor_data() {
    static uint32_t last_sensor_update = 0;
    uint32_t current_time = millis();
    
    // 每100ms更新一次传感器数据
    if (current_time - last_sensor_update < 100) {
        return;
    }
    last_sensor_update = current_time;
    
    // 模拟心率数据 (60-100 bpm)
    static uint8_t heart_rate = 75;
    static int8_t hr_direction = 1;
    heart_rate += hr_direction;
    if (heart_rate > 100 || heart_rate < 60) {
        hr_direction = -hr_direction;
    }
    
    // 模拟血氧数据 (95-99%)
    static uint8_t blood_oxygen = 97;
    static int8_t spo2_direction = 1;
    blood_oxygen += spo2_direction;
    if (blood_oxygen > 99 || blood_oxygen < 95) {
        spo2_direction = -spo2_direction;
    }
    
    // 模拟丙酮数据 (0-15 ppm)
    static float acetone_level = 5.0;
    static float acetone_direction = 0.1;
    acetone_level += acetone_direction;
    if (acetone_level > 15.0 || acetone_level < 0.0) {
        acetone_direction = -acetone_direction;
    }
    
    // 创建测试结果
    TestResult result;
    result.timestamp = current_time / 1000;
    result.heart_rate = heart_rate;
    result.blood_oxygen = blood_oxygen;
    result.acetone_level = acetone_level;
    result.confidence = 90 + random(10);
    
    // 根据数值设置趋势
    if (heart_rate > 90 || acetone_level > 10.0) {
        result.trend = TREND_ATTENTION;
    } else if (heart_rate > 100 || acetone_level > 15.0) {
        result.trend = TREND_CONCERN;
    } else {
        result.trend = TREND_NORMAL;
    }
    
    result.test_type = TEST_TYPE_COMPREHENSIVE;
    
    // 更新UI数据
    ui.setTestResult(&result);
    ui.handleEvent(EVENT_DATA_UPDATE);
}

// ==================== 初始化函数 ====================
void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n========================================");
    Serial.println("  糖尿病筛查手表UI系统 - 示例程序");
    Serial.println("========================================\n");
    
    // 初始化引脚
    pinMode(PIN_BTN1, INPUT_PULLUP);
    pinMode(PIN_BTN2, INPUT_PULLUP);
    pinMode(PIN_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_BACKLIGHT, HIGH); // 开启背光
    
    // 初始化显示
    tft.init();
    tft.setRotation(1); // 横屏模式
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    Serial.println("[显示] TFT_eSPI初始化完成");
    Serial.printf("[显示] 分辨率: %dx%d\n", tft.width(), tft.height());
    
    // 初始化UI管理器
    if (!ui.init(&tft)) {
        Serial.println("[错误] UI管理器初始化失败");
        while (1);
    }
    
    Serial.println("[UI] UI管理器初始化成功");
    
    // 设置亮度
    ui.setBrightness(80);
    
    // 创建主界面
    Rect screen_bounds = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    UiComponent* main_screen = main_screen_create_standard("主界面", &screen_bounds);
    
    if (!main_screen) {
        Serial.println("[错误] 主界面创建失败");
        while (1);
    }
    
    // 设置主界面数据
    MainScreenData screen_data;
    memset(&screen_data, 0, sizeof(screen_data));
    
    // 设置时间
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    screen_data.current_time = *timeinfo;
    
    // 设置位置和天气
    screen_data.location = "北京";
    screen_data.weather = "晴";
    screen_data.temperature = 25.0;
    
    // 设置系统状态
    screen_data.battery_level = 85;
    screen_data.bluetooth_connected = true;
    screen_data.has_notifications = false;
    
    // 设置医疗状态
    screen_data.health_status = "正常";
    screen_data.overall_trend = TREND_NORMAL;
    screen_data.last_test_time = now - 3600; // 1小时前
    
    // 设置导航提示
    screen_data.swipe_up_hint = "上划开始测试";
    screen_data.swipe_down_hint = "下滑查看历史";
    screen_data.button1_hint = "选择";
    screen_data.button2_hint = "返回";
    
    // 设置快速数据
    screen_data.last_heart_rate = 75;
    screen_data.last_blood_oxygen = 98;
    screen_data.last_acetone_level = 5.2;
    
    // 更新主界面
    main_screen_update_all(main_screen, &screen_data);
    
    Serial.println("[UI] 主界面设置完成");
    
    // 启动欢迎动画
    main_screen_start_welcome_animation(main_screen);
    
    last_update_time = millis();
    Serial.println("\n[系统] 初始化完成，开始主循环\n");
}

// ==================== 主循环 ====================
void loop() {
    uint32_t current_time = millis();
    uint32_t delta_time = current_time - last_update_time;
    last_update_time = current_time;
    
    // 检查按键
    check_buttons();
    
    // 更新传感器数据
    update_sensor_data();
    
    // 更新UI
    ui.update(delta_time);
    
    // 渲染UI
    ui.render();
    
    // 控制帧率
    uint32_t frame_time = millis() - current_time;
    if (frame_time < 16) { // 目标60fps
        delay(16 - frame_time);
    }
    
    // 每5秒打印状态
    static uint32_t last_status_time = 0;
    if (current_time - last_status_time > 5000) {
        last_status_time = current_time;
        
        Serial.printf("[状态] 帧率: %.1f fps, 内存: %u bytes\n", 
                     ui.getFrameRate(), ui.getMemoryUsage());
        Serial.printf("[状态] 渲染时间: %u ms, 更新时间: %u ms\n",
                     ui.getRenderTime(), ui.getUpdateTime());
        
        // 打印当前状态
        const char* state_names[] = {
            "主界面", "测试选择", "丙酮检测", "心率检测",
            "综合检测", "结果展示", "历史记录", "设置",
            "紧急警告", "呼吸引导"
        };
        
        UiState current_state = ui.getCurrentState();
        if (current_state < UI_STATE_COUNT) {
            Serial.printf("[状态] 当前界面: %s\n", state_names[current_state]);
        }
    }
}

// ==================== 示例界面切换函数 ====================
void switch_to_test_select() {
    ui.setState(UI_STATE_TEST_SELECT, ANIMATION_SLIDE_LEFT);
    Serial.println("[界面] 切换到测试选择界面");
}

void switch_to_acetone_test() {
    ui.setState(UI_STATE_ACETONE_TESTING, ANIMATION_FADE_IN);
    Serial.println("[界面] 切换到丙酮检测界面");
}

void switch_to_heart_rate_test() {
    ui.setState(UI_STATE_HEART_RATE_TESTING, ANIMATION_FADE_IN);
    Serial.println("[界面] 切换到心率检测界面");
}

void switch_to_results() {
    ui.setState(UI_STATE_RESULT_DISPLAY, ANIMATION_SLIDE_RIGHT);
    Serial.println("[界面] 切换到结果展示界面");
}

// ==================== 测试进度回调 ====================
void on_test_progress(UiComponent* ring, uint8_t event_type, void* user_data) {
    switch (event_type) {
        case 1: // 测试开始
            Serial.println("[测试] 测试开始");
            break;
        case 2: // 测试进度更新
            Serial.printf("[测试] 进度更新: %.1f%%\n", *(float*)user_data * 100);
            break;
        case 3: // 测试完成
            Serial.println("[测试] 测试完成");
            switch_to_results();
            break;
        case 4: // 测试错误
            Serial.println("[测试] 测试错误");
            break;
    }
}

// ==================== 紧急警告处理 ====================
void handle_emergency_alert() {
    Serial.println("[警告] 检测到异常值，切换到紧急警告界面");
    ui.setState(UI_STATE_EMERGENCY_ALERT, ANIMATION_SCALE);
    
    // 闪烁红色警告
    for (int i = 0; i < 5; i++) {
        ui.setBrightness(100);
        delay(200);
        ui.setBrightness(50);
        delay(200);
    }
    ui.setBrightness(80);
}

// ==================== 呼吸引导回调 ====================
void on_breathing_guide(UiComponent* guide, uint8_t event_type, void* user_data) {
    switch (event_type) {
        case 1: // 开始吸气
            Serial.println("[呼吸] 吸气...");
            break;
        case 2: // 开始呼气
            Serial.println("[呼吸] 呼气...");
            break;
        case 3: // 引导完成
            Serial.println("[呼吸] 呼吸引导完成");
            break;
    }
}

// ==================== 历史记录示例 ====================
void display_history() {
    Serial.println("\n[历史] 显示测试历史记录");
    Serial.println("时间戳\t\t心率\t血氧\t丙酮\t趋势");
    Serial.println("------------------------------------------------");
    
    for (int i = 0; i < 5; i++) {
        TestResult* result = &test_results[i];
        struct tm* timeinfo = localtime((time_t*)&result->timestamp);
        
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", timeinfo);
        
        const char* trend_str = "正常";
        if (result->trend == TREND_ATTENTION) trend_str = "注意";
        else if (result->trend == TREND_CONCERN) trend_str = "建议关注";
        
        Serial.printf("%s\t%d\t%d\t%.1f\t%s\n", 
                     time_str, result->heart_rate, result->blood_oxygen,
                     result->acetone_level, trend_str);
    }
    Serial.println();
}

// ==================== 性能测试 ====================
void run_performance_test() {
    Serial.println("\n[性能] 开始性能测试...");
    
    uint32_t start_time = millis();
    uint32_t frame_count = 0;
    
    // 运行10秒性能测试
    while (millis() - start_time < 10000) {
        ui.update(16); // 模拟60fps
        ui.render();
        frame_count++;
        delay(16);
    }
    
    uint32_t total_time = millis() - start_time;
    float avg_fps = (float)frame_count / (total_time / 1000.0);
    
    Serial.printf("[性能] 测试完成: %u 帧, %.1f fps\n", frame_count, avg_fps);
    Serial.printf("[性能] 平均帧时间: %.1f ms\n", (float)total_time / frame_count);
    
    if (avg_fps > 55) {
        Serial.println("[性能] 性能优秀 (>55 fps)");
    } else if (avg_fps > 30) {
        Serial.println("[性能] 性能良好 (30-55 fps)");
    } else {
        Serial.println("[性能] 性能需要优化 (<30 fps)");
    }
}

// ==================== 内存使用报告 ====================
void report_memory_usage() {
    uint32_t ui_memory = ui.getMemoryUsage();
    uint32_t heap_free = ESP.getFreeHeap();
    uint32_t heap_total = ESP.getHeapSize();
    
    Serial.println("\n[内存] 内存使用报告:");
    Serial.printf("  UI内存: %u bytes (限制: %u bytes)\n", ui_memory, UI_MEMORY_LIMIT);
    Serial.printf("  堆内存: %u / %u bytes (%.1f%% 使用率)\n", 
                 heap_total - heap_free, heap_total,
                 (float)(heap_total - heap_free) / heap_total * 100);
    
    if (ui_memory > UI_MEMORY_LIMIT) {
        Serial.println("[警告] UI内存超过限制!");
    }
    
    if (heap_free < 10240) { // 少于10KB
        Serial.println("[警告] 堆内存不足!");
    }
}

// ==================== 系统信息 ====================
void print_system_info() {
    Serial.println("\n[系统] 系统信息:");
    Serial.printf("  MCU: ESP32-S3R8N8\n");
    Serial.printf("  CPU频率: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  Flash大小: %u MB\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("  PSRAM: %s\n", ESP.getPsramSize() ? "可用" : "不可用");
    Serial.printf("  SDK版本: %s\n", ESP.getSdkVersion());
    Serial.printf("  编译时间: %s %s\n", __DATE__, __TIME__);
}