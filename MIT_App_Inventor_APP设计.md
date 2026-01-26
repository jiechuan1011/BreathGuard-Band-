# MIT App Inventor Android APP 设计文档
## 糖尿病初筛监测系统 - 数据接收与报告生成APP

### 一、APP功能概述
- **BLE连接**：连接设备名"DiabetesSensor"
- **数据接收**：接收JSON格式数据 `{"hr":xx, "spo2":xx, "acetone":xx.x, "note":"..."}`
- **数据显示**：实时显示心率、血氧、丙酮数值
- **报告生成**：生成用户风险初筛报告
- **语音朗读**：TextToSpeech朗读报告内容
- **数据存储**：保存最近5次测量到TinyDB

### 二、界面设计（Screen布局）

#### 1. 主界面（Screen1）
```
┌─────────────────────────────────────┐
│ 糖尿病初筛监测APP                   │
├─────────────────────────────────────┤
│ [连接状态：未连接]                  │
│ [连接按钮]                          │
├─────────────────────────────────────┤
│ 实时数据：                          │
│ 心率：-- bpm                        │
│ 血氧：-- %                          │
│ 丙酮：-- ppm                        │
├─────────────────────────────────────┤
│ [生成报告按钮]                      │
├─────────────────────────────────────┤
│ 报告内容：                          │
│ [多行文本框/标签]                   │
├─────────────────────────────────────┤
│ [语音朗读按钮]                      │
│ [清除历史按钮]                      │
└─────────────────────────────────────┘
```

### 三、组件清单

#### 非可视组件
1. **BluetoothLE** (扩展组件)
   - 名称：BluetoothLE1
   - 功能：BLE连接和数据接收

2. **TextToSpeech** (扩展组件)
   - 名称：TextToSpeech1
   - 功能：语音朗读报告

3. **TinyDB** (扩展组件)
   - 名称：TinyDB1
   - 功能：本地数据存储

4. **Clock** (内置组件)
   - 名称：Clock1
   - 功能：定时刷新和数据处理

#### 可视组件
1. **Label** (标签)
   - 名称：Label_Title (标题)
   - 名称：Label_Status (连接状态)
   - 名称：Label_HR (心率标签)
   - 名称：Label_SpO2 (血氧标签)
   - 名称：Label_Acetone (丙酮标签)
   - 名称：Label_ReportTitle (报告标题)

2. **Button** (按钮)
   - 名称：Button_Connect (连接按钮)
   - 名称：Button_GenerateReport (生成报告按钮)
   - 名称：Button_Speak (语音朗读按钮)
   - 名称：Button_ClearHistory (清除历史按钮)

3. **TextBox** 或 **Label** (多行显示)
   - 名称：TextBox_Report (报告内容显示)
   - 属性：MultiLine=true, Height=150

4. **HorizontalArrangement** (水平布局)
   - 用于按钮分组

### 四、全局变量
```
- connected (布尔值)：BLE连接状态
- deviceAddress (文本)：设备MAC地址
- currentData (字典)：当前接收的数据
- historyData (列表)：历史数据列表（最多5条）
```

### 五、主要功能块设计（伪代码）

#### 1. 初始化
```
当 Screen1 初始化时：
   设置 connected = false
   设置 currentData = 创建空字典
   设置 historyData = 从TinyDB1加载历史数据
   如果 historyData 为空，则设置 historyData = 创建空列表
   更新界面状态
```

#### 2. BLE连接功能
```
当 Button_Connect 被点击时：
   如果 connected = false：
       调用 BluetoothLE1.Connect("DiabetesSensor")
       设置 Button_Connect.Text = "正在连接..."
   否则：
       调用 BluetoothLE1.Disconnect()
       设置 connected = false
       更新界面状态
```

#### 3. BLE事件处理
```
当 BluetoothLE1.Connected 时：
   设置 connected = true
   设置 Button_Connect.Text = "断开连接"
   设置 Label_Status.Text = "已连接"
   设置 Label_Status.TextColor = 绿色
   
   订阅特征值通知：
   调用 BluetoothLE1.SubscribeToCharacteristic(
       serviceUUID = "a1b2c3d4-e5f6-4789-abcd-ef0123456789",
       characteristicUUID = "a1b2c3d4-e5f6-4789-abcd-ef012345678a"
   )
```

```
当 BluetoothLE1.Disconnected 时：
   设置 connected = false
   设置 Button_Connect.Text = "连接设备"
   设置 Label_Status.Text = "未连接"
   设置 Label_Status.TextColor = 红色
```

```
当 BluetoothLE1.CharacteristicValueChanged 时（收到数据）：
   获取 characteristicValue (字节列表)
   
   将字节列表转换为文本：
   jsonText = 字节列表转文本(characteristicValue, "UTF-8")
   
   解析JSON：
   currentData = 解析JSON(jsonText)
   
   更新界面显示：
   设置 Label_HR.Text = "心率：" + 获取(currentData, "hr", "--") + " bpm"
   设置 Label_SpO2.Text = "血氧：" + 获取(currentData, "spo2", "--") + " %"
   设置 Label_Acetone.Text = "丙酮：" + 获取(currentData, "acetone", "--") + " ppm"
   
   保存到历史数据：
   添加 currentData 到 historyData 开头
   如果 historyData长度 > 5：
       移除 historyData最后一项
   调用 TinyDB1.StoreValue("history", historyData)
```

#### 4. 报告生成功能
```
当 Button_GenerateReport 被点击时：
   如果 currentData 为空：
       显示通知："请先连接设备并接收数据"
       返回
   
   创建报告文本：
   report = ""
   report = report + "=== 糖尿病初筛监测报告 ===\n"
   report = report + "生成时间：" + 当前时间() + "\n\n"
   
   report = report + "【测量数据】\n"
   report = report + "心率：" + 获取(currentData, "hr", "--") + " bpm\n"
   report = report + "血氧：" + 获取(currentData, "spo2", "--") + " %\n"
   report = report + "丙酮：" + 获取(currentData, "acetone", "--") + " ppm\n\n"
   
   report = report + "【趋势分析】\n"
   趋势判断结果 = ""
   
   心率值 = 转换为数字(获取(currentData, "hr", "0"))
   丙酮值 = 转换为数字(获取(currentData, "acetone", "0"))
   
   如果 心率值 > 100 或 丙酮值 > 25：
       趋势判断结果 = "部分数值偏高，建议关注"
   否则 如果 心率值 < 50：
       趋势判断结果 = "心率偏低，建议关注"
   否则：
       趋势判断结果 = "数值在正常范围内"
   
   report = report + 趋势判断结果 + "\n\n"
   
   report = report + "【历史记录】\n"
   对于 i 从 1 到 historyData长度：
       数据项 = 获取(historyData, i)
       report = report + i + ". 心率：" + 获取(数据项, "hr", "--") + 
                " 丙酮：" + 获取(数据项, "acetone", "--") + "\n"
   
   report = report + "\n"
   report = report + "【重要声明】\n"
   report = report + "本报告仅供个人健康趋势参考，\n"
   report = report + "不构成任何医疗诊断，\n"
   report = report + "请务必咨询专业医生。"
   
   设置 TextBox_Report.Text = report
   设置 TextBox_Report.TextColor = 黑色
   
   ' 将声明部分设置为红色
   ' 注意：MIT App Inventor中可能需要使用HTML格式或特殊处理
```

#### 5. 语音朗读功能
```
当 Button_Speak 被点击时：
   如果 TextBox_Report.Text 不为空：
       调用 TextToSpeech1.Speak(TextBox_Report.Text)
   否则：
       显示通知："请先生成报告"
```

#### 6. 清除历史数据
```
当 Button_ClearHistory 被点击时：
   设置 historyData = 创建空列表
   调用 TinyDB1.ClearTag("history")
   显示通知："历史数据已清除"
   更新报告显示
```

#### 7. 辅助函数
```
函数 获取(字典, 键, 默认值)：
   如果 字典包含键(键)：
       返回 字典[键]
   否则：
       返回 默认值
```

### 六、JSON数据处理示例

#### 接收的JSON格式：
```json
{
  "hr": 75,
  "spo2": 98,
  "acetone": 12.5,
  "note": "仅供参考"
}
```

#### 异常值处理：
- `hr`: null 或 0xFF 表示无效 → 显示"--"
- `spo2`: null 或 0xFF 表示无效 → 显示"--"
- `acetone`: null 或 负数表示无效 → 显示"--"

### 七、趋势判断逻辑

```javascript
// 趋势判断伪代码
function analyzeTrend(data) {
    let hr = data.hr;
    let acetone = data.acetone;
    let warnings = [];
    
    if (hr !== null && hr !== 0xFF) {
        if (hr > 100) warnings.push("心率偏高");
        if (hr < 50) warnings.push("心率偏低");
    }
    
    if (acetone !== null && acetone > 0) {
        if (acetone > 25) warnings.push("丙酮浓度偏高");
    }
    
    if (warnings.length > 0) {
        return "注意：" + warnings.join("，") + "，建议咨询医生";
    } else {
        return "数值在正常范围内";
    }
}
```

### 八、TinyDB数据结构

#### 存储格式：
```json
{
  "history": [
    {
      "timestamp": "2026-01-26 14:30:00",
      "hr": 75,
      "spo2": 98,
      "acetone": 12.5
    },
    {
      "timestamp": "2026-01-26 14:25:00",
      "hr": 72,
      "spo2": 97,
      "acetone": 10.8
    }
  ]
}
```

#### 存储限制：
- 最多保存5条记录
- 新的记录添加到开头
- 超过5条时删除最旧的记录

### 九、界面状态管理

#### 连接状态：
- **未连接**：按钮显示"连接设备"，状态标签红色
- **连接中**：按钮显示"正在连接..."，状态标签黄色
- **已连接**：按钮显示"断开连接"，状态标签绿色

#### 数据状态：
- **无数据**：显示"--"
- **有效数据**：显示数值
- **无效数据**：显示"--"

### 十、扩展组件配置

#### 1. BluetoothLE扩展
- 从MIT App Inventor扩展库添加
- 权限：需要BLUETOOTH和BLUETOOTH_ADMIN权限
- 配置：在AI Companion中测试时需要开启手机蓝牙

#### 2. TextToSpeech扩展
- 从MIT App Inventor扩展库添加
- 语言：设置为中文（zh-CN）
- 速率：1.0（正常语速）

#### 3. TinyDB扩展
- 从MIT App Inventor扩展库添加
- 自动保存应用数据

### 十一、测试流程

1. **BLE连接测试**：
   - 确保"DiabetesSensor"设备正在广播
   - 点击"连接设备"按钮
   - 检查连接状态是否变为"已连接"

2. **数据接收测试**：
   - 连接成功后，等待数据接收
   - 检查心率、血氧、丙酮数值是否更新

3. **报告生成测试**：
   - 点击"生成报告"按钮
   - 检查报告内容是否完整
   - 检查趋势判断是否正确

4. **语音朗读测试**：
   - 点击"语音朗读"按钮
   - 检查是否正常朗读报告内容

5. **数据存储测试**：
   - 多次接收数据
   - 检查历史记录是否正确保存
   - 测试清除历史功能

### 十二、注意事项

1. **BLE兼容性**：
   - 需要Android 5.0以上版本
   - 需要开启手机蓝牙和位置权限（Android 6.0+）

2. **JSON解析**：
   - MIT App Inventor内置JSON解析功能
   - 需要处理异常JSON格式

3. **UI更新**：
   - 在主线程中更新UI组件
   - 使用Clock组件定时刷新

4. **错误处理**：
   - BLE连接失败时显示提示
   - 数据解析失败时使用默认值
   - 网络异常时降级处理

5. **免责声明**：
   - 必须清晰显示免责声明
   - 使用红色字体突出显示
   - 语音朗读时包含免责声明

### 十三、完整块示例（关键部分）

```
// 连接按钮点击事件
Button_Connect.Click:
    if not connected
        BluetoothLE1.Connect("DiabetesSensor")
        set Button_Connect.Text to "正在连接..."
    else
        BluetoothLE1.Disconnect()
        set connected to false
        updateUIStatus()

// 数据接收事件
BluetoothLE1.CharacteristicValueChanged:
    set jsonText to convert bytes to string(value, "UTF-8")
    set currentData to parse jsonText
    updateDataDisplay(currentData)
    saveToHistory(currentData)

// 生成报告
Button_GenerateReport.Click:
    if currentData is empty
        show alert "请先连接设备并接收数据"
    else
        set report to generateReport(currentData, historyData)
        set TextBox_Report.Text to report

// 语音朗读
Button_Speak.Click:
    if TextBox_Report.Text ≠ ""
        TextToSpeech1.Speak(TextBox_Report.Text)
    else
        show alert "请先生成报告"
```

### 十四、部署建议

1. **测试环境**：
   - 使用MIT AI Companion进行实时测试
   - 连接真实的"DiabetesSensor"设备

2. **打包发布**：
   - 生成APK文件
   - 在Google Play或第三方应用市场发布
   - 添加应用描述和隐私政策

3. **用户指导**：
   - 提供简单的使用说明
   - 说明BLE连接步骤
   - 强调免责声明的重要性

### 十五、扩展功能建议（可选）

1. **数据图表**：使用WebView组件显示历史数据图表
2. **数据导出**：将报告保存为文本文件或分享
3. **多语言支持**：添加英文界面
4. **云端同步**：将数据同步到云端服务器
5. **报警提醒**：设置阈值，超过时发出通知

---

**重要提示**：本APP设计仅供个人健康趋势参考，不构成医疗诊断。用户应咨询专业医生获取医疗建议。