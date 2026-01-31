# 家用糖尿病初筛腕带项目 - BLE JSON示例和MIT App Inventor方案

## 1. 完整BLE JSON示例（含运动场景）

### 正常场景示例：
```json
{
  "hr": 72,
  "spo2": 98,
  "acetone": -1,
  "batt": 85,
  "note": "SNR:24.5dB Corr:89%"
}
```

### 运动干扰场景示例：
```json
{
  "hr": 68,
  "spo2": 96,
  "acetone": -1,
  "batt": 83,
  "note": "SNR:17.2dB Corr:58% 运动干扰"
}
```

### 数据无效场景示例：
```json
{
  "hr": 0,
  "spo2": 0,
  "acetone": -1,
  "batt": 79,
  "note": "采集失败，请检查佩戴"
}
```

### 低电量场景示例：
```json
{
  "hr": 75,
  "spo2": 97,
  "acetone": -1,
  "batt": 15,
  "note": "SNR:22.1dB Corr:92%"
}
```

## 2. MIT App Inventor关键Block逻辑方案

### 2.1 蓝牙LE连接模块

**组件设置：**
- BluetoothLE组件：用于BLE通信
- Canvas组件：用于绘制PPG波形（宽度320，高度200）
- Label组件：显示心率、血氧、电池、SNR、相关性
- Button组件：连接/断开按钮
- ListPicker组件：选择设备
- Timer组件：定时更新数据（1秒间隔）

**初始化Block：**
```
当 Screen1.Initialize 时
   设置 BluetoothLE1.Enabled 为 true
   设置 Timer1.Interval 为 1000 (1秒)
   设置 Timer1.Enabled 为 false
```

### 2.2 BLE连接和数据处理

**连接设备Block：**
```
当 ListPicker1.AfterPicking 时
   设置 BluetoothLE1.DeviceAddress 为 ListPicker1.Selection
   调用 BluetoothLE1.Connect
```

**接收Notify数据Block：**
```
