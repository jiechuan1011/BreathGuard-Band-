# 自动Git同步脚本

这是一个跨平台的自动Git同步解决方案，可以监控本地文件变化并自动执行`git add`、`commit`和`push`操作到GitHub远程仓库。

## 功能特性

- ✅ **自动监控**：实时监控文件系统变化
- ✅ **自动提交**：检测到更改后自动执行Git操作
- ✅ **跨平台支持**：Windows、Mac、Linux均可运行
- ✅ **智能防抖**：避免频繁提交（默认5秒间隔）
- ✅ **冲突处理**：自动处理推送冲突，先拉取再推送
- ✅ **日志记录**：详细的操作日志记录
- ✅ **忽略配置**：自动忽略.git、临时文件等不需要监控的目录

## 文件结构

```
scripts/
├── auto_git_sync.py          # 主脚本文件（需要watchdog）
├── auto_git_sync_simple.py   # 简化版脚本（无需额外依赖）
├── start_sync.bat           # Windows启动脚本
├── start_sync.sh            # Mac/Linux启动脚本
├── README.md                # 说明文档
└── requirements.txt         # Python依赖（仅主脚本需要）
```

## 快速开始

## 版本选择

### 版本1：完整版（推荐）
- **优点**：实时监控，响应快速
- **缺点**：需要安装watchdog依赖
- **适用场景**：开发环境，需要实时同步

### 版本2：简化版
- **优点**：无需额外依赖，开箱即用
- **缺点**：轮询方式，有延迟（默认30秒）
- **适用场景**：简单使用，不想安装额外依赖

### 1. 安装依赖（仅完整版需要）

```bash
# 安装Python依赖（仅完整版需要）
pip install watchdog

# 或者使用requirements.txt
pip install -r scripts/requirements.txt
```

### 2. 运行脚本

#### 完整版（需要watchdog）：
```bash
# 在项目根目录运行
python scripts/auto_git_sync.py
```

#### 简化版（无需额外依赖）：
```bash
# 在项目根目录运行
python scripts/auto_git_sync_simple.py
```

#### 使用启动脚本：
- **Windows**: 双击 `scripts/start_sync.bat`
- **Mac/Linux**: `bash scripts/start_sync.sh`

### 3. 开始监控

脚本启动后会自动：
1. 检查当前目录是否为Git仓库
2. 开始监控文件变化
3. 检测到更改后自动执行git add、commit、push
4. 按Ctrl+C停止监控

## 配置选项

### 命令行参数

```bash
# 指定监控目录
python scripts/auto_git_sync.py --path /path/to/repo

# 指定提交信息前缀
python scripts/auto_git_sync.py --prefix "Auto-sync"

# 调整防抖时间（秒）
python scripts/auto_git_sync.py --debounce 10
```

### 脚本内配置

在`auto_git_sync.py`中可以修改以下配置：

```python
# 防抖时间（秒）
self.debounce_seconds = 5

# 忽略的目录
self.ignored_dirs = ['.git', '.vscode', '.pio', '__pycache__']

# 忽略的文件扩展名
self.ignored_extensions = ['.log', '.tmp', '.swp', '.pyc', '__pycache__']

# 提交信息前缀
self.commit_message_prefix = "Auto-commit"
```

## 使用场景

### 1. 开发环境自动备份
在开发过程中自动备份代码到GitHub，避免本地数据丢失。

### 2. 文档自动同步
监控文档目录变化，自动同步到远程仓库。

### 3. 配置自动部署
监控配置文件变化，自动提交并触发CI/CD流程。

## 高级用法

### 作为后台服务运行

#### Windows（使用计划任务）
1. 使用提供的 `scripts/start_sync.bat` 或创建自定义批处理文件
2. 使用Windows计划任务设置为开机启动

**创建计划任务步骤**：
1. 打开"任务计划程序"
2. 创建基本任务
3. 触发器选择"计算机启动时"
4. 操作选择"启动程序"，程序选择 `scripts/start_sync.bat`
5. 设置"不管用户是否登录都要运行"

#### Mac/Linux（使用systemd或launchd）
创建服务文件，示例systemd服务：
```ini
[Unit]
Description=Auto Git Sync Service
After=network.target

[Service]
Type=simple
User=yourusername
WorkingDirectory=/path/to/your/repo
ExecStart=/usr/bin/python3 /path/to/your/repo/scripts/auto_git_sync.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

### 集成到现有工作流

#### 与Git Hooks结合
在`.git/hooks/post-commit`中添加：
```bash
#!/bin/bash
# 提交后自动推送
git push origin $(git rev-parse --abbrev-ref HEAD)
```

#### 与IDE集成
在VSCode的`settings.json`中添加：
```json
{
  "tasks": {
    "version": "2.0.0",
    "tasks": [
      {
        "label": "Start Auto Git Sync",
        "type": "shell",
        "command": "python ${workspaceFolder}/scripts/auto_git_sync.py",
        "problemMatcher": []
      }
    ]
  }
}
```

## 注意事项

### 1. 安全性考虑
- 确保`.gitignore`配置正确，避免提交敏感信息（如密码、密钥）
- 定期检查自动提交的内容
- 建议在私有仓库使用，或确保公开仓库不包含敏感数据

### 2. 性能考虑
- 监控大量文件可能影响性能，建议合理配置忽略规则
- 防抖时间不宜过短，避免频繁提交影响开发体验

### 3. 冲突处理
- 脚本会自动处理简单的推送冲突（先pull再push）
- 复杂的合并冲突需要手动解决
- 建议定期手动执行`git pull`保持本地与远程同步

### 4. 网络要求
- 需要稳定的网络连接才能成功推送
- 网络中断时提交会失败，但会在下次检测到变化时重试

## 故障排除

### 常见问题

#### Q: 脚本报错 "ModuleNotFoundError: No module named 'watchdog'"
A: 需要安装依赖：`pip install watchdog`

#### Q: 自动提交过于频繁
A: 调整`debounce_seconds`参数增加防抖时间

#### Q: 推送失败，提示冲突
A: 脚本会自动尝试先拉取再推送，如果仍然失败需要手动解决冲突

#### Q: 监控不到文件变化
A: 检查文件是否在忽略列表中，或调整监控目录

#### Q: 脚本占用CPU过高
A: 减少监控目录范围，增加忽略规则

### 日志查看
- **完整版**：日志保存在 `auto_git_sync.log` 文件中
- **简化版**：日志保存在 `auto_git_sync_simple.log` 文件中

可以查看详细的操作记录和错误信息。

## 替代方案

### 1. Git Hooks方案
使用Git的post-commit钩子自动推送：
```bash
# .git/hooks/post-commit
#!/bin/bash
git push origin $(git rev-parse --abbrev-ref HEAD)
```

### 2. 定时任务方案
使用cron（Linux/Mac）或计划任务（Windows）定时执行：
```bash
# 每5分钟执行一次
*/5 * * * * cd /path/to/repo && git add . && git commit -m "Auto-commit" && git push
```

### 3. 第三方工具
- **git-watch**: 专门的Git监控工具
- **inotifywait** (Linux): 系统级文件监控
- **fswatch** (Mac): 跨平台文件监控工具

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request改进此脚本。

## 更新日志

### v1.1.0 (2024-01-26)
- 新增简化版脚本，无需安装watchdog依赖
- 添加Windows和Mac/Linux启动脚本
- 改进文档和配置说明

### v1.0.0 (2024-01-26)
- 初始版本发布
- 支持基本文件监控和自动Git操作
- 跨平台支持
- 防抖机制和冲突处理
