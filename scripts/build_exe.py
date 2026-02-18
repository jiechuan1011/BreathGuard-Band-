#!/usr/bin/env python3
"""
将 scripts 下的 Git 同步小工具打包成 Windows exe：
- 完整版：AutoGitSync_Full.exe（含 watchdog，实时监控）
- 本地版：AutoGitSync_Local.exe（简化版，轮询监控，无额外依赖）
"""
import os
import sys
import subprocess
import shutil
from pathlib import Path

# 脚本所在目录（即 scripts/）
SCRIPT_DIR = Path(__file__).resolve().parent
# 项目根目录
PROJECT_ROOT = SCRIPT_DIR.parent
# 输出目录
DIST_DIR = SCRIPT_DIR / "dist"
BUILD_DIR = SCRIPT_DIR / "build"
SPEC_DIR = SCRIPT_DIR / "spec"


def run_pyinstaller(script_name: str, exe_name: str, add_data: list = None, hidden_imports: list = None):
    """调用 PyInstaller 打包单个脚本"""
    script_path = SCRIPT_DIR / script_name
    if not script_path.exists():
        print(f"错误: 找不到脚本 {script_path}")
        return False

    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",           # 单文件 exe
        "--console",           # 控制台程序
        "--name", exe_name,
        "--distpath", str(DIST_DIR),
        "--workpath", str(BUILD_DIR),
        "--specpath", str(SPEC_DIR),
        "--clean",
        "--noconfirm",
        str(script_path),
    ]
    if add_data:
        for item in add_data:
            cmd.extend(["--add-data", item])
    if hidden_imports:
        for imp in hidden_imports:
            cmd.extend(["--hidden-import", imp])

    print(f"\n正在打包: {script_name} -> {exe_name}.exe")
    print(" ".join(cmd))
    ret = subprocess.run(cmd, cwd=str(SCRIPT_DIR))
    return ret.returncode == 0


def main():
    print("=" * 60)
    print("Auto Git Sync 工具 - 打包 exe")
    print("=" * 60)

    # 检查 PyInstaller
    try:
        import PyInstaller
    except ImportError:
        print("请先安装 PyInstaller: pip install pyinstaller")
        print("或: pip install -r scripts/requirements-build.txt")
        sys.exit(1)

    # 清理旧输出
    for d in [DIST_DIR, BUILD_DIR, SPEC_DIR]:
        if d.exists():
            shutil.rmtree(d, ignore_errors=True)
    DIST_DIR.mkdir(parents=True, exist_ok=True)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    SPEC_DIR.mkdir(parents=True, exist_ok=True)

    # 1. 本地版（简化版，无 watchdog）
    ok_local = run_pyinstaller(
        "auto_git_sync_simple.py",
        "AutoGitSync_Local",
    )
    if not ok_local:
        print("本地版打包失败")
        sys.exit(1)

    # 2. 完整版（含 watchdog）
    ok_full = run_pyinstaller(
        "auto_git_sync.py",
        "AutoGitSync_Full",
        hidden_imports=["watchdog", "watchdog.observers", "watchdog.observers.api", "watchdog.events"],
    )
    if not ok_full:
        print("完整版打包失败")
        sys.exit(1)

    print("\n" + "=" * 60)
    print("打包完成！")
    print(f"输出目录: {DIST_DIR}")
    print("  - AutoGitSync_Local.exe   (本地版/简化版，轮询监控)")
    print("  - AutoGitSync_Full.exe    (完整版，实时监控，需 watchdog)")
    print("\n使用方式: 将对应 exe 放到 Git 仓库根目录，双击运行即可。")
    print("=" * 60)


if __name__ == "__main__":
    main()
