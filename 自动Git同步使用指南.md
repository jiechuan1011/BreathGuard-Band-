# 自动Git同步使用指南

## 概述

本解决方案提供了自动监控本地文件变化并同步到GitHub的功能。当您在本地修改文件时，系统会自动检测更改并执行 `git add`、`commit`、`push` 操作。

## 解决方案组成

### 1. 完整版脚本 (`scripts/auto_git_sync.py`)
- **特点**：实时监控，响应快速
- **依赖**：需要安装 `watchdog` 库
- **适用**：开发环境，需要实时同步

### 2. 简化版脚本 (`scripts/auto_git_sync_simple.py`)
- **特点**：轮询检查（默认30秒间隔），无需额外依赖
- **依赖**：仅需Python标准库
- **适用**：简单使用，不想安装额外依赖

### 3. 启动脚本
- **Windows**: `scripts/start_sync.bat`（双击运行）
- **Mac/Linux**: `scripts/start_sync.sh`（`bash scripts/start_sync.sh`）

## 快速开始

### 方法一：使用简化版（推荐初学者）
1. 确保已安装Python 3.6+
2. 双击 `scripts/start_sync.bat`（Windows）或运行 `bash scripts/start_sync.sh`（Mac/Linux）
3. 脚本会自动开始监控当前目录

### 方法二：使用完整版（需要实时监控）
1. 安装依赖：`pip install watchdog`
2. 运行：`python scripts/auto_git_sync.py`
3. 脚本会实时监控文件变化

## 配置说明

### 基本配置
- **监控目录**：默认当前Git仓库目录
- **检查间隔**：简化版30秒，完整版实时
- **防抖时间**：5秒（避免频繁提交）

### 忽略规则
自动忽略以下目录和文件：
- `.git/`、`.vscode/`、`.pio/`、`__pycache__/`、`scripts/`
- 扩展名：`.log`、`.tmp`、`.swp`、`.pyc`

### 提交信息
默认格式：`Auto-commit: YYYY-MM-DD HH:MM:SS`
可修改 `commit_message_prefix` 参数自定义前缀

## 跨平台支持

### Windows
- 使用批处理文件 `start_sync.bat`
- 可配置为计划任务开机启动
- 支持PowerShell和CMD

### Mac/Linux
- 使用Shell脚本 `start_sync.sh`
- 可配置为systemd/launchd服务
- 需要执行权限：`chmod +x scripts/start_sync.sh`

## 错误处理机制

### 1. 冲突处理
- 推送失败时自动尝试 `git pull --rebase`
- 拉取成功后再尝试推送
- 复杂冲突需要手动解决

### 2. 网络异常
- 网络中断时提交会失败
- 下次检测到变化时会重试
- 建议保持稳定网络连接

### 3. 权限问题
- 检查Git配置是否正确
- 确保有推送权限到远程仓库
- 检查SSH密钥或HTTPS凭证

## 安全注意事项

### 重要警告
1. **敏感信息**：确保 `.gitignore` 正确配置，避免提交密码、密钥等敏感信息
2. **代码审查**：定期检查自动提交的内容
3. **仓库权限**：建议在私有仓库使用，公开仓库需注意数据安全
4. **备份策略**：自动同步不能替代定期备份

### 推荐配置
```gitignore
# 在 .gitignore 中添加
*.env
*.key
*.pem
*.secret
config/credentials.*
```

## 性能优化

### 减少监控范围
如果目录文件过多，可以：
1. 修改 `ignored_dirs` 添加更多忽略目录
2. 只监控特定子目录
3. 增加检查间隔时间

### 调整参数
- **防抖时间**：增加 `debounce_seconds` 减少提交频率
- **检查间隔**：增加 `check_interval` 降低CPU使用率
- **忽略规则**：添加更多忽略模式减少扫描文件数

## 常见问题解答

### Q1: 脚本不工作怎么办？
1. 检查Python是否安装：`python --version`
2. 检查是否是Git仓库：确认有 `.git` 目录
3. 查看日志文件：`auto_git_sync.log` 或 `auto_git_sync_simple.log`

### Q2: 如何停止脚本？
- 按 `Ctrl+C` 停止监控
- Windows可关闭命令行窗口
- 后台服务需停止相应服务

### Q3: 提交太频繁怎么办？
- 修改 `debounce_seconds` 参数增加防抖时间
- 简化版可修改 `check_interval` 增加检查间隔

### Q4: 如何监控特定目录？
修改脚本中的 `repo_path` 参数指向目标目录，或使用命令行参数。

### Q5: 如何查看操作记录？
查看日志文件：
- 完整版：`auto_git_sync.log`
- 简化版：`auto_git_sync_simple.log`

## 高级用法

### 作为系统服务
#### Windows计划任务
1. 打开"任务计划程序"
2. 创建任务，触发器设为"计算机启动时"
3. 操作选择 `scripts/start_sync.bat`
4. 设置"不管用户是否登录都要运行"

#### Linux systemd服务
创建 `/etc/systemd/system/auto-git-sync.service`：
```ini
[Unit]
Description=Auto Git Sync Service
After=network.target

[Service]
Type=simple
User=yourusername
WorkingDirectory=/path/to/repo
ExecStart=/usr/bin/python3 /path/to/repo/scripts/auto_git_sync_simple.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

### 集成到开发流程
#### Git Hooks组合
在 `.git/hooks/post-commit` 中添加：
```bash
#!/bin/bash
# 提交后自动推送当前分支
git push origin $(git rev-parse --abbrev-ref HEAD)
```

#### IDE集成（VSCode）
在 `.vscode/tasks.json` 中添加：
```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Start Auto Git Sync",
      "type": "shell",
      "command": "python ${workspaceFolder}/scripts/auto_git_sync_simple.py",
      "problemMatcher": []
    }
  ]
}
```

## 替代方案比较

| 方案 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| 完整版脚本 | 实时监控，响应快 | 需要安装watchdog | 开发环境，需要实时同步 |
| 简化版脚本 | 无需额外依赖 | 有延迟（30秒） | 简单使用，不想安装依赖 |
| Git Hooks | 集成到Git流程 | 只触发于Git操作 | 已有Git工作流 |
| 定时任务 | 简单可靠 | 固定间隔，不实时 | 定期备份 |

## 最佳实践

### 1. 测试环境
- 先在测试分支或测试仓库试用
- 确认自动提交的内容符合预期
- 调整配置参数适应工作习惯

### 2. 监控策略
- 重要项目使用完整版实时监控
- 文档项目使用简化版定期检查
- 根据项目大小调整忽略规则

### 3. 备份策略
- 自动同步作为辅助备份手段
- 定期手动创建重要提交点
- 重要更改建议手动提交并添加详细说明

### 4. 团队协作
- 在团队中使用时通知成员
- 避免自动提交与手动提交冲突
- 设置合理的提交信息前缀便于识别

## 故障排除流程

1. **检查基础环境**
   - Python是否安装
   - Git是否配置
   - 网络连接是否正常

2. **查看日志文件**
   - 检查错误信息
   - 查看操作记录
   - 确认监控的文件范围

3. **测试Git操作**
   - 手动执行 `git status`
   - 手动执行 `git push`
   - 检查远程仓库权限

4. **调整配置**
   - 修改检查间隔
   - 调整忽略规则
   - 增加防抖时间

## 更新与维护

### 脚本更新
定期检查脚本更新，获取新功能和修复：
1. 查看GitHub仓库更新
2. 备份当前配置
3. 测试新版本功能

### 配置迁移
升级时注意配置兼容性：
1. 备份当前配置文件
2. 对比新旧版本配置差异
3. 逐步迁移配置参数

## 技术支持

### 获取帮助
1. 查看 `scripts/README.md` 详细文档
2. 检查日志文件中的错误信息
3. 在项目Issue中提出问题

### 贡献改进
欢迎提交改进建议：
1. 报告问题或bug
2. 提交功能建议
3. 贡献代码改进

---

**重要提示**：自动同步工具是辅助工具，不能替代开发者的代码审查和版本管理责任。请合理使用，确保代码质量和安全性。