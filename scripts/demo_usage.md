# 自动Git同步脚本演示

## 演示步骤

### 1. 启动监控
```bash
# 方法1: 使用简化版脚本（无需额外依赖）
python scripts/auto_git_sync_simple.py

# 方法2: 使用完整版脚本（需要watchdog）
pip install watchdog
python scripts/auto_git_sync.py

# 方法3: 使用启动脚本
# Windows: 双击 scripts/start_sync.bat
# Mac/Linux: bash scripts/start_sync.sh
```

### 2. 监控输出示例
```
============================================================
自动Git同步脚本
监控文件变化并自动提交到GitHub
============================================================

监控目录: E:\Diabetes_Screening_System
检查间隔: 30秒
忽略的目录: .git, .vscode, .pio, __pycache__, scripts
忽略的文件扩展名: .log, .tmp, .swp, .pyc

开始监控文件变化...
按 Ctrl+C 停止监控

日志文件: auto_git_sync_simple.log
------------------------------------------------------------
```

### 3. 创建测试文件
```bash
# 在另一个终端中创建测试文件
echo "测试自动同步功能" > test_demo.txt
```

### 4. 观察自动提交
```
2024-01-26 16:58:30,123 - __main__ - INFO - 检测到 1 个文件变化
2024-01-26 16:58:30,456 - __main__ - INFO - 检测到未提交的更改:
 M test_demo.txt
2024-01-26 16:58:30,789 - __main__ - INFO - 成功执行 git add
2024-01-26 16:58:31,123 - __main__ - INFO - 成功提交: Auto-commit: test_demo.txt - 2024-01-26 16:58:31
2024-01-26 16:58:31,456 - __main__ - INFO - 成功推送到远程仓库
```

### 5. 修改文件
```bash
# 修改测试文件
echo "第二次修改" >> test_demo.txt
```

### 6. 观察第二次提交
```
2024-01-26 16:59:00,123 - __main__ - INFO - 检测到 1 个文件变化
2024-01-26 16:59:00,456 - __main__ - INFO - 检测到未提交的更改:
 M test_demo.txt
2024-01-26 16:59:00,789 - __main__ - INFO - 成功执行 git add
2024-01-26 16:59:01,123 - __main__ - INFO - 成功提交: Auto-commit: test_demo.txt - 2024-01-26 16:59:01
2024-01-26 16:59:01,456 - __main__ - INFO - 成功推送到远程仓库
```

### 7. 停止监控
按 `Ctrl+C` 停止脚本

## 实际应用场景

### 场景1: 开发环境自动备份
```bash
# 在开发项目根目录启动
cd /path/to/your/project
python scripts/auto_git_sync_simple.py

# 现在可以安心开发，所有更改会自动备份到GitHub
```

### 场景2: 文档项目自动同步
```bash
# 监控文档目录
cd /path/to/docs
python scripts/auto_git_sync_simple.py

# 编辑文档时自动保存到GitHub
```

### 场景3: 配置自动部署
```bash
# 监控配置文件目录
cd /path/to/configs
python scripts/auto_git_sync_simple.py

# 配置文件变化自动触发CI/CD
```

## 验证同步结果

### 1. 查看Git历史
```bash
git log --oneline -5
# 输出示例:
# abc1234 Auto-commit: config.yaml - 2024-01-26 17:00:00
# def5678 Auto-commit: main.py - 2024-01-26 16:59:30
# ghi9012 Auto-commit: 2个文件变化 - 2024-01-26 16:59:00
```

### 2. 查看远程仓库
```bash
git remote -v
# 确认远程仓库地址

git log origin/main --oneline -3
# 查看远程仓库的最新提交
```

### 3. 查看日志文件
```bash
# 查看详细操作记录
tail -f auto_git_sync_simple.log
```

## 故障排除演示

### 1. 网络中断情况
```
2024-01-26 17:01:30,123 - __main__ - ERROR - git push 失败: fatal: Could not read from remote repository.
2024-01-26 17:01:30,456 - __main__ - INFO - 尝试先拉取最新更改...
2024-01-26 17:01:30,789 - __main__ - ERROR - git pull 失败: fatal: unable to access 'https://github.com/...': Failed to connect to github.com port 443: Timed out
# 网络恢复后，下次检测到变化时会重试
```

### 2. 冲突处理
```
2024-01-26 17:02:30,123 - __main__ - ERROR - git push 失败: ! [rejected] main -> main (fetch first)
2024-01-26 17:02:30,456 - __main__ - INFO - 尝试先拉取最新更改...
2024-01-26 17:02:31,123 - __main__ - INFO - 成功拉取最新更改，重新尝试推送...
2024-01-26 17:02:31,456 - __main__ - INFO - 成功推送到远程仓库
```

## 性能优化演示

### 1. 调整检查间隔
```python
# 在 auto_git_sync_simple.py 中修改
self.check_interval = 60  # 从30秒改为60秒
```

### 2. 添加忽略规则
```python
# 在 auto_git_sync_simple.py 中修改
self.ignored_dirs = ['.git', '.vscode', '.pio', '__pycache__', 'scripts', 'node_modules', 'dist']
self.ignored_extensions = ['.log', '.tmp', '.swp', '.pyc', '.class', '.jar']
```

### 3. 调整防抖时间
```python
# 在 auto_git_sync.py 中修改
self.debounce_seconds = 10  # 从5秒改为10秒
```

## 安全注意事项演示

### 1. 检查.gitignore配置
```bash
# 确保敏感文件被忽略
cat .gitignore | grep -E "(\.env|\.key|\.pem|password|secret)"
```

### 2. 测试自动提交内容
```bash
# 先在小范围测试
mkdir test_repo
cd test_repo
git init
# 配置脚本并测试
```

### 3. 监控日志审查
```bash
# 定期检查自动提交的内容
grep -A5 -B5 "成功提交" auto_git_sync_simple.log
```

---

**提示**: 在实际使用前，建议先在测试分支或测试仓库中进行充分测试，确保自动同步行为符合预期。