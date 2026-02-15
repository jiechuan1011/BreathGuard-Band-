GitHub 整体文件替换工具

说明：本工具将把指定本地文件夹中的所有文件（保持相对路径）替换推送到 GitHub 仓库对应路径上。通过 Windows 批处理（`push_replace.bat`）触发 Python 脚本完成操作。

文件列表：
- `push_replace.bat` — Windows 触发脚本
- `push_replace.py` — 推送实现（调用 GitHub Contents API）

使用步骤：
1. 在 Windows 上安装 Python（推荐 3.8+），并确保 `python` 在 PATH 中。
2. 安装依赖：

```powershell
python -m pip install requests
```

3. 在系统环境变量中设置 `GITHUB_TOKEN`（推荐使用带 `repo` 权限的个人访问令牌），或者在运行批处理时以 `--token` 传入。

4. 准备要替换的文件夹（例如 `C:\replace_files`），文件夹内的相对路径将对应到仓库根路径下的相对位置。

5. 运行示例（在命令行或双击 `push_replace.bat`）：

```powershell
push_replace.bat owner repo C:\replace_files main "整体文件替换提交"
```

参数说明（批处理调用顺序）：
- owner: 仓库所有者（用户名或组织）
- repo: 仓库名
- src_folder: 本地待推送文件夹（绝对或相对路径）
- branch: 目标分支（可选，默认 `main`）
- commit_msg: 提交信息（可选）

安全提示：不要把 token 写入脚本文件中；优先使用环境变量。

如需更多定制（只推特定子路径、跳过二进制等），可修改 `push_replace.py`。
