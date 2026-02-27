# 端口检测工具（Port Tester）

用途：帮助查看连接到电脑的检测模块或腕带模块上单片机的工作状况（串口交互、日志查看、发送测试命令）。

快速开始：

1. 在有 Python 的环境中安装依赖：

```powershell
python -m pip install -r tools/port_tester/requirements.txt
```

2. 运行脚本（调试模式）：

```powershell
python tools/port_tester/port_tester.py
```

功能：
- 列出系统串口并连接/断开
- 发送自定义命令或预设 Ping
- 接收并显示串口返回（带时间戳）
- 保存日志到文件

打包为单个 exe（Windows）：

1. 安装 `pyinstaller`：

```powershell
python -m pip install pyinstaller
```

2. 在 `tools/port_tester` 目录运行：

```powershell
.\build_exe.bat
```

生成的可执行文件会在 `dist` 目录下。

注意：不同 MCU 的协议不同，请在发送命令时使用目标模块支持的命令（例如 `ping`、`status` 等）。
