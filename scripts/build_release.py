#!/usr/bin/env python3
"""
生成 Release 发布包：先构建三个 exe，再打包为 zip（含使用说明）。
输出：scripts/release/AutoGitSync_Release_v1.0.zip
"""
import sys
import shutil
import zipfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DIST_DIR = SCRIPT_DIR / "dist"
RELEASE_DIR = SCRIPT_DIR / "release"


def get_version():
    version_file = SCRIPT_DIR / "VERSION"
    if version_file.exists():
        return version_file.read_text(encoding="utf-8").strip().split()[0]
    return "1.0"


def main():
    print("=" * 60)
    print("Auto Git Sync - Release 发布包构建")
    print("=" * 60)

    version = get_version()
    package_name = f"AutoGitSync_Release_v{version}"
    package_dir = RELEASE_DIR / package_name
    zip_path = RELEASE_DIR / f"{package_name}.zip"

    # 1. 调用 build_exe 构建
    print("\n[1/3] 正在构建 exe...")
    import build_exe
    if not build_exe.main():
        sys.exit(1)

    # 2. 准备发布目录
    if RELEASE_DIR.exists():
        shutil.rmtree(RELEASE_DIR, ignore_errors=True)
    package_dir.mkdir(parents=True, exist_ok=True)

    exes = ["AutoGitSync_Local.exe", "AutoGitSync_Full.exe", "AutoGitSync_GUI.exe"]
    for name in exes:
        src = DIST_DIR / name
        if src.exists():
            shutil.copy2(src, package_dir / name)
            print(f"  已复制: {name}")
        else:
            print(f"  警告: 未找到 {name}")

    usage_src = SCRIPT_DIR / "release_usage.txt"
    if usage_src.exists():
        shutil.copy2(usage_src, package_dir / "使用说明.txt")

    # 3. 打 zip 包
    print(f"\n[2/3] 正在打包: {zip_path.name}")
    RELEASE_DIR.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in package_dir.rglob("*"):
            if f.is_file():
                arcname = f.relative_to(package_dir)
                zf.write(f, arcname)

    print(f"\n[3/3] 完成。")
    print("=" * 60)
    print(f"发布包: {zip_path}")
    print(f"解压后运行 AutoGitSync_GUI.exe 即可使用图形界面。")
    print("=" * 60)


if __name__ == "__main__":
    main()
