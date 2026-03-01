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


def run_pyinstaller(script_name: str, exe_name: str, add_data: list = None, hidden_imports: list = None, console: bool = True):
    """调用 PyInstaller 打包单个脚本。console=False 为图形界面程序（不显示控制台）。"""
    script_path = SCRIPT_DIR / script_name
    if not script_path.exists():
        print(f"错误: 找不到脚本 {script_path}")
        return False

    # 先确保旧的 exe 不存在（可能被上次构建锁定）
    target_exe = DIST_DIR / f"{exe_name}.exe"
    if target_exe.exists():
        print(f"检测到旧的 {target_exe.name}，尝试删除...")
        for i in range(5):
            try:
                target_exe.unlink()
                print("  已删除旧文件")
                break
            except PermissionError:
                # 说明可能 exe 正在运行或被占用
                print("  无法删除，文件可能正在使用中。请关闭运行中的程序再重试。")
                if i < 4:
                    import time
                    time.sleep(0.2)
                else:
                    print("多次尝试仍无法清除旧文件，打包中止。")
                    return False

    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",
        "--noconsole" if not console else "--console",
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

    # 3. 图形界面（无控制台窗口）
    ok_gui = run_pyinstaller(
        "auto_git_sync_gui.py",
        "AutoGitSync_GUI",
        hidden_imports=["tkinter"],
        console=False,
    )
    if not ok_gui:
        print("图形界面版打包失败")
        sys.exit(1)

    print("\n" + "=" * 60)
    print("打包完成！")
    print(f"输出目录: {DIST_DIR}")
    print("  - AutoGitSync_Local.exe   (本地版/简化版，轮询监控)")
    print("  - AutoGitSync_Full.exe    (完整版，实时监控)")
    print("  - AutoGitSync_GUI.exe     (图形界面，需与上面两个 exe 同目录使用)")
    print("\n使用方式:")
    print("  - 命令行版: 将 AutoGitSync_Local.exe 或 Full.exe 放到仓库根目录双击运行。")
    print("  - 图形版: 将 dist 目录下三个 exe 放在同一文件夹，运行 AutoGitSync_GUI.exe 选择目录后开始同步。")
    print("=" * 60)


if __name__ == "__main__":
    main()
