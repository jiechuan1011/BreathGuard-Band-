# 糖尿病初筛腕带 - BLE通信实现说明

## 概述

本文档详细说明了为糖尿病初筛腕带（ESP32-S3R8N8）实现的BLE通信功能。腕带作为BLE Peripheral（服务器），定时发送心率、血氧等健康数据给MIT App Inventor APP。

## 硬件更新

- **原硬件**：ESP32-C3 SuperMini
- **新硬件**：ESP32-S3R8N8（双核240MHz，8MB Flash + 8MB Octal PSRAM）
- **优势**：更强的处理能力，更大的内存，更好的BLE性能

## 文件结构修改

### 新增文件

1. **`config/ble_config.h`** - BLE配置文件
   - 定义BLE UUID、设备名、通信参数
   - 支持NimBLE和默认BLE库
   - 低功耗优化配置

2. **`main_control/main_control_new.ino`** - 新的腕带主控程序
   - 作为BLE Peripheral发送数据
   - 使用NimBLE库（更省电）
   - 定时打包JSON数据并通过notify发送

3. **`scripts/test_ble.py`** - Python测试脚本
   - 模拟MIT App Inventor APP接收数据
   - 验证BLE通信功能

### 修改文件

1. **`platformio.ini`** - 新增ESP32-S3环境
   - 添加`wrist_s3`环境配置
   - 添加NimBLE库依赖（`h2zero/NimBLE-Arduino@^1.4.1`）
   - 定义`USE_NIMBLE`编译标志

2. **`config/config.h`** - 添加ESP32-S3支持
   - 更新硬件平台检查逻辑
   - 添加`MCU_ESP32_S3`定义

## BLE配置详情

### UUID配置（与APP文档一致）

- **服务UUID**: `a1b2c3d4-e5f6-4789-abcd-ef0123456789`
- **特征值UUID**: `a1b2c3d4-e5f6-4789-abcd-ef012345678a`
- **设备名称**: `DiabetesSensor`

### 数据格式

JSON格式数据示例：
```json
{
  "hr": 75,
  "spo2": 98,
  "acetone": -99,
  "note": "腕带数据，SNR:15.3dB"
}
```

- `hr`: 心率（bpm）
- `spo2`: 血氧饱和度（%）
- `acetone`: 丙酮浓度（ppm，腕带无此数据，固定为-99）
- `note`: 备注信息，包含SNR质量

### 通信参数

- **广播间隔**: 500ms
- **数据发送间隔**: 4000ms（4秒）
- **功耗优化**: ESP_PWR_LVL_P3（低功耗模式）
- **连接策略**: 只在有客户端连接时才发送数据

## 代码实现要点

### 1. BLE初始化（`ble_init()`函数）

```cpp
void ble_init() {
    // 初始化NimBLE
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    
    // 创建服务器、服务、特征值
    pServer = NimBLEDevice::createServer();
    pService = pServer->createService(BLE_SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    // 开始广播
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setInterval(BLE_ADVERTISING_INTERVAL_MS);
    pAdvertising->start();
}
```

### 2. 数据打包和发送（`wrist_loop()`函数）

```cpp
void wrist_loop() {
    // ... 其他处理 ...
    
    // 定时发送数据（只在连接时发送）
    if (deviceConnected && (now - lastNotifyTime >= BLE_NOTIFY_INTERVAL)) {
        lastNotifyTime = now;
        
        // 获取系统状态
        const SystemState* state = system_state_get();
        
        // 打包JSON数据
        String json_data = ble_pack_json_data(
            state->hr_bpm,
            system_state_get_spo2(),
            ACETONE_INVALID_VALUE,  // 腕带无丙酮数据
            "腕带数据"
        );
        
        // 发送数据
        ble_send_data(json_data);
    }
    
    // ... 其他处理 ...
}
```

### 3. 低功耗优化

- **NimBLE库**: 比默认BLE库更省电，RAM占用更低
- **连接时发送**: 只在有BLE客户端连接时才发送数据
- **OLED超时熄屏**: 30秒无操作自动关闭OLED
- **Light Sleep**: 时钟模式且OLED关闭时进入轻睡眠

## 编译和烧录步骤

### 环境要求

1. **PlatformIO Core**: 6.1.13+
2. **Arduino-ESP32**: 2.0.17+
3. **Python依赖**（测试用）:
   ```bash
   pip install bleak asyncio
   ```

### 编译命令

```bash
# 编译ESP32-S3版本（使用NimBLE）
pio run -e wrist_s3

# 编译ESP32-C3版本（使用默认BLE）
pio run -e wrist

# 清理编译文件
pio run -t clean
```

### 烧录命令

```bash
# 烧录到ESP32-S3
pio run -e wrist_s3 -t upload

# 烧录到ESP32-C3
pio run -e wrist -t upload
```

## 测试验证

### 1. 串口输出验证

烧录后打开串口监视器（115200波特率），应看到以下输出：
```
========================================
  糖尿病初筛腕带主控板 (ESP32-S3)
  BLE Peripheral 模式
========================================

[Init] 按键初始化完成
[Init] OLED初始化完成
[BLE] 初始化NimBLE...
[BLE] NimBLE初始化完成，开始广播
[Init] 系统启动完成
```

### 2. BLE通信测试

使用Python测试脚本验证BLE通信：

```bash
cd scripts
python test_ble.py
```

预期输出：
```
==================================================
糖尿病初筛腕带 - BLE通信测试
==================================================
🔍 正在扫描BLE设备...
✅ 找到目标设备: DiabetesSensor (XX:XX:XX:XX:XX:XX)

🔗 正在连接设备: DiabetesSensor (XX:XX:XX:XX:XX:XX)
✅ 连接成功
📋 发现 X 个服务
✅ 找到目标服务: a1b2c3d4-e5f6-4789-abcd-ef0123456789
✅ 找到目标特征值: a1b2c3d4-e5f6-4789-abcd-ef012345678a
🔔 订阅通知...
✅ 已订阅通知，等待数据...

📡 收到BLE数据:
   心率: 75 bpm
   血氧: 98%
   丙酮: -99 ppm
   备注: 腕带数据，SNR:15.3dB
```

### 3. MIT App Inventor APP测试

1. 在手机上安装MIT App Inventor APP
2. 搜索并连接"DiabetesSensor"设备
3. 订阅特征值通知
4. 验证接收到的JSON数据格式

## 故障排除

### 常见问题

1. **编译错误：找不到NimBLE库**
   ```bash
   pio pkg install --library "h2zero/NimBLE-Arduino@^1.4.1"
   ```

2. **BLE设备不可见**
   - 检查设备是否上电
   - 确认BLE已启用（代码中`USE_BLE_MODULE`已定义）
   - 检查设备名称是否正确

3. **连接不稳定**
   - 调整广播间隔（`BLE_ADVERTISING_INTERVAL_MS`）
   - 检查电源稳定性
   - 确保在有效通信范围内（<10米）

4. **数据格式错误**
   - 验证JSON格式是否正确
   - 检查数据打包函数`ble_pack_json_data()`
   - 确认字符串编码为UTF-8

### 调试技巧

1. **启用调试模式**
   在`platformio.ini`中添加：
   ```ini
   build_flags = ${env.build_flags} -DDEBUG_MODE=1
   ```

2. **查看详细BLE日志**
   ```cpp
   // 在代码中添加
   Serial.printf("[BLE] 连接状态: %s\n", deviceConnected ? "已连接" : "未连接");
   Serial.printf("[BLE] 发送数据长度: %d\n", json_data.length());
   ```

## 性能优化建议

### 功耗优化

1. **进一步降低广播功率**
   ```cpp
   NimBLEDevice::setPower(ESP_PWR_LVL_P6);  // 更低的发射功率
   ```

2. **增加发送间隔**
   ```cpp
   #define BLE_NOTIFY_INTERVAL 8000  // 8秒间隔
   ```

3. **深度睡眠模式**
   - 长时间无连接时进入深度睡眠
   - 通过RTC定时器或外部中断唤醒

### 内存优化

1. **使用静态缓冲区**
   ```cpp
   static char json_buffer[BLE_JSON_BUFFER_SIZE];  // 避免动态分配
   ```

2. **优化JSON格式**
   - 使用更短的键名
   - 减少不必要的字段

### 可靠性优化

1. **数据校验**
   - 添加CRC校验
   - 实现重传机制

2. **连接管理**
   - 实现自动重连
   - 连接超时处理

## 项目约束遵守

### 成本控制（≤500元）

- ESP32-S3R8N8: ~50元
- MAX30102传感器: ~30元
- OLED显示屏: ~15元
- 其他组件: ~50元
- **总计**: ~145元（远低于500元限制）

### 重量控制（<45g）

- ESP32-S3R8N8: 5g
- 传感器模块: 10g
- 电池: 15g
- 外壳和结构: 10g
- **总计**: ~40g（满足<45g要求）

### 医疗免责声明

所有输出数据都包含免责声明：
```
本设备仅用于生理参数趋势监测，不用于医疗诊断，请咨询专业医生
```

## 后续开发建议

1. **数据加密**
   - 实现BLE数据加密传输
   - 添加设备认证机制

2. **多设备支持**
   - 支持同时连接多个APP
   - 实现设备角色切换

3. **OTA升级**
   - 通过BLE实现固件无线升级
   - 添加版本检查和回滚机制

4. **数据存储**
   - 本地存储历史数据
   - 实现数据导出功能

## 联系和支持

如有问题，请参考：
- 项目文档：`项目结构总结.md`
- BLE配置：`config/ble_config.h`
- 测试脚本：`scripts/test_ble.py`
- 主控代码：`main_control/main_control_new.ino`

---
*最后更新: 2026年1月30日*
*版本: 1.0.0*