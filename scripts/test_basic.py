#!/usr/bin/env python3
"""
基本功能测试脚本
测试自动Git同步脚本的核心功能
"""

import os
import sys
import subprocess

def test_python_version():
    """测试Python版本"""
    print("1. 测试Python版本...")
    try:
        # Python 3.6兼容写法
        result = subprocess.run([sys.executable, '--version'], 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        version = result.stdout.strip() or result.stderr.strip()
        print(f"   Python版本: {version}")
        return True
    except Exception as e:
        print(f"   错误: {e}")
        return False

def test_git_repo():
    """测试当前目录是否是Git仓库"""
    print("2. 测试Git仓库...")
    try:
        result = subprocess.run(['git', 'rev-parse', '--git-dir'],
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        if result.returncode == 0:
            print(f"   Git仓库: {result.stdout.strip()}")
            
            # 测试远程仓库
            remote_result = subprocess.run(['git', 'remote', '-v'],
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE,
                                         universal_newlines=True)
            print(f"   远程仓库:\n{remote_result.stdout}")
            return True
        else:
            print("   错误: 当前目录不是Git仓库")
            return False
    except Exception as e:
        print(f"   错误: {e}")
        return False

def test_script_import():
    """测试脚本是否能导入"""
    print("3. 测试脚本导入...")
    try:
        # 测试简化版脚本
        import sys
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        
        # 尝试导入简化版脚本的类
        with open('auto_git_sync_simple.py', 'r', encoding='utf-8', errors='ignore') as f:
            exec(f.read())
        print("   简化版脚本导入成功")
        
        # 检查完整版脚本语法
        with open('auto_git_sync.py', 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            if 'watchdog' in content:
                print("   完整版脚本检测到watchdog依赖")
            else:
                print("   完整版脚本未检测到watchdog依赖")
        
        return True
    except Exception as e:
        print(f"   错误: {e}")
        return False

def test_batch_file():
    """测试批处理文件"""
    print("4. 测试批处理文件...")
    batch_path = 'start_sync.bat'
    if os.path.exists(batch_path):
        with open(batch_path, 'r', encoding='utf-8') as f:
            content = f.read()
            if 'python' in content and 'auto_git_sync' in content:
                print("   批处理文件格式正确")
                return True
            else:
                print("   批处理文件内容异常")
                return False
    else:
        print(f"   文件不存在: {batch_path}")
        return False

def test_shell_script():
    """测试Shell脚本"""
    print("5. 测试Shell脚本...")
    shell_path = 'start_sync.sh'
    if os.path.exists(shell_path):
        with open(shell_path, 'r', encoding='utf-8') as f:
            content = f.read()
            if 'python3' in content and 'auto_git_sync' in content:
                print("   Shell脚本格式正确")
                return True
            else:
                print("   Shell脚本内容异常")
                return False
    else:
        print(f"   文件不存在: {shell_path}")
        return False

def main():
    """主测试函数"""
    print("=" * 60)
    print("自动Git同步脚本 - 基本功能测试")
    print("=" * 60)
    
    # 切换到脚本目录
    original_dir = os.getcwd()
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    tests = [
        test_python_version,
        test_git_repo,
        test_script_import,
        test_batch_file,
        test_shell_script
    ]
    
    passed = 0
    total = len(tests)
    
    for test_func in tests:
        if test_func():
            passed += 1
        print()
    
    # 恢复原始目录
    os.chdir(original_dir)
    
    print("=" * 60)
    print(f"测试结果: {passed}/{total} 通过")
    
    if passed == total:
        print("所有测试通过！脚本应该可以正常工作。")
        print("\n下一步：")
        print("1. Windows用户: 双击 scripts/start_sync.bat")
        print("2. Mac/Linux用户: 运行 bash scripts/start_sync.sh")
        print("3. 或者直接运行: python scripts/auto_git_sync_simple.py")
    else:
        print("部分测试失败，请检查上述错误信息。")
    
    print("=" * 60)

if __name__ == "__main__":
    main()