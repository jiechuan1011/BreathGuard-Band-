#!/usr/bin/env python3
"""
简化版自动Git同步脚本
使用轮询方式检查文件变化，自动执行git add、commit、push操作
支持Windows/Mac/Linux环境，无需安装watchdog
"""

import os
import sys
import time
import subprocess
import logging
import hashlib
from datetime import datetime
from pathlib import Path

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('auto_git_sync_simple.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

class SimpleGitAutoSync:
    """简化版Git自动同步"""
    
    def __init__(self, repo_path, commit_message_prefix="Auto-commit"):
        self.repo_path = repo_path
        self.commit_message_prefix = commit_message_prefix
        self.check_interval = 30  # 检查间隔（秒）
        self.last_check_time = 0
        
        # 忽略的目录和文件
        self.ignored_dirs = ['.git', '.vscode', '.pio', '__pycache__', 'scripts']
        self.ignored_extensions = ['.log', '.tmp', '.swp', '.pyc']
        
        # 存储文件状态
        self.file_states = {}
        
    def should_ignore(self, file_path):
        """检查文件是否应该被忽略"""
        file_path_str = str(file_path)
        
        # 检查是否在忽略的目录中
        for ignored_dir in self.ignored_dirs:
            if f'/{ignored_dir}/' in file_path_str.replace('\\', '/') or file_path_str.endswith(f'/{ignored_dir}'):
                return True
        
        # 检查文件扩展名
        for ext in self.ignored_extensions:
            if file_path_str.endswith(ext):
                return True
        
        return False
    
    def get_file_hash(self, file_path):
        """计算文件哈希值"""
        try:
            with open(file_path, 'rb') as f:
                return hashlib.md5(f.read()).hexdigest()
        except Exception:
            return None
    
    def scan_files(self):
        """扫描目录中的文件"""
        file_hashes = {}
        
        for root, dirs, files in os.walk(self.repo_path):
            # 跳过忽略的目录
            dirs[:] = [d for d in dirs if not self.should_ignore(os.path.join(root, d))]
            
            for file in files:
                file_path = os.path.join(root, file)
                if not self.should_ignore(file_path):
                    file_hash = self.get_file_hash(file_path)
                    if file_hash:
                        file_hashes[file_path] = file_hash
        
        return file_hashes
    
    def check_for_changes(self):
        """检查文件变化"""
        current_hashes = self.scan_files()
        
        if not self.file_states:
            # 第一次扫描，初始化状态
            self.file_states = current_hashes
            return False
        
        # 检查变化
        changed_files = []
        
        # 检查修改或新增的文件
        for file_path, file_hash in current_hashes.items():
            if file_path not in self.file_states or self.file_states[file_path] != file_hash:
                changed_files.append(file_path)
        
        # 检查删除的文件
        for file_path in self.file_states:
            if file_path not in current_hashes:
                changed_files.append(file_path + " (deleted)")
        
        # 更新状态
        self.file_states = current_hashes
        
        return changed_files if changed_files else False
    
    def git_auto_commit(self, changed_files=None):
        """自动执行Git提交和推送"""
        try:
            # 切换到仓库目录
            original_cwd = os.getcwd()
            os.chdir(self.repo_path)
            
            # 1. 检查是否有未提交的更改
            status_result = subprocess.run(
                ['git', 'status', '--porcelain'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                encoding='utf-8'
            )
            
            if not status_result.stdout.strip():
                logger.info("没有未提交的更改")
                os.chdir(original_cwd)
                return False
            
            logger.info(f"检测到未提交的更改:\n{status_result.stdout}")
            
            # 2. 添加所有更改
            add_result = subprocess.run(
                ['git', 'add', '.'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                encoding='utf-8'
            )
            
            if add_result.returncode != 0:
                logger.error(f"git add 失败: {add_result.stderr}")
                os.chdir(original_cwd)
                return False
            
            logger.info("成功执行 git add")
            
            # 3. 创建提交信息
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            commit_message = f"{self.commit_message_prefix}: {timestamp}"
            
            if changed_files and len(changed_files) > 0:
                file_count = len(changed_files)
                commit_message = f"{self.commit_message_prefix}: {file_count}个文件变化 - {timestamp}"
            
            # 4. 提交更改
            commit_result = subprocess.run(
                ['git', 'commit', '-m', commit_message],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                encoding='utf-8'
            )
            
            if commit_result.returncode != 0:
                # 检查是否是因为没有更改（可能已经提交）
                if "nothing to commit" in commit_result.stdout.lower():
                    logger.info("没有需要提交的更改")
                    os.chdir(original_cwd)
                    return False
                else:
                    logger.error(f"git commit 失败: {commit_result.stderr}")
                    os.chdir(original_cwd)
                    return False
            
            logger.info(f"成功提交: {commit_message}")
            
            # 5. 推送到远程仓库
            push_result = subprocess.run(
                ['git', 'push'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                encoding='utf-8'
            )
            
            if push_result.returncode != 0:
                logger.error(f"git push 失败: {push_result.stderr}")
                
                # 尝试先拉取最新更改
                logger.info("尝试先拉取最新更改...")
                pull_result = subprocess.run(
                    ['git', 'pull', '--rebase'],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                    encoding='utf-8'
                )
                
                if pull_result.returncode == 0:
                    logger.info("成功拉取最新更改，重新尝试推送...")
                    push_result = subprocess.run(
                        ['git', 'push'],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        universal_newlines=True,
                        encoding='utf-8'
                    )
                    
                    if push_result.returncode != 0:
                        logger.error(f"重新推送失败: {push_result.stderr}")
                        os.chdir(original_cwd)
                        return False
                    else:
                        logger.info("成功推送到远程仓库")
                        os.chdir(original_cwd)
                        return True
                else:
                    logger.error(f"git pull 失败: {pull_result.stderr}")
                    os.chdir(original_cwd)
                    return False
            else:
                logger.info("成功推送到远程仓库")
                os.chdir(original_cwd)
                return True
            
        except Exception as e:
            logger.error(f"Git操作异常: {e}")
            # 确保恢复原始工作目录
            try:
                os.chdir(original_cwd)
            except:
                pass
            return False
    
    def run(self):
        """运行监控循环"""
        print("=" * 60)
        print("简化版自动Git同步脚本")
        print("使用轮询方式检查文件变化并自动提交到GitHub")
        print("=" * 60)
        
        # 检查是否是Git仓库
        if not os.path.exists(os.path.join(self.repo_path, '.git')):
            print(f"错误: {self.repo_path} 不是Git仓库")
            sys.exit(1)
        
        print(f"\n监控目录: {self.repo_path}")
        print(f"检查间隔: {self.check_interval}秒")
        print("忽略的目录: .git, .vscode, .pio, __pycache__, scripts")
        print("忽略的文件扩展名: .log, .tmp, .swp, .pyc")
        print("\n开始监控文件变化...")
        print("按 Ctrl+C 停止监控")
        print("\n日志文件: auto_git_sync_simple.log")
        print("-" * 60)
        
        try:
            # 初始扫描
            logger.info("执行初始文件扫描...")
            self.file_states = self.scan_files()
            logger.info(f"初始扫描完成，监控 {len(self.file_states)} 个文件")
            
            while True:
                time.sleep(self.check_interval)
                
                logger.debug(f"检查文件变化...")
                changed_files = self.check_for_changes()
                
                if changed_files:
                    logger.info(f"检测到 {len(changed_files)} 个文件变化")
                    for i, file in enumerate(changed_files[:5]):  # 只显示前5个
                        logger.info(f"  变化文件 {i+1}: {file}")
                    if len(changed_files) > 5:
                        logger.info(f"  还有 {len(changed_files) - 5} 个文件变化未显示")
                    
                    # 执行Git操作
                    self.git_auto_commit(changed_files)
                
        except KeyboardInterrupt:
            print("\n\n停止监控...")
            logger.info("监控已停止")
        except Exception as e:
            logger.error(f"监控异常: {e}")
            print(f"\n发生错误: {e}")

def main():
    """主函数"""
    # 获取仓库路径（默认为当前目录）
    repo_path = os.getcwd()
    
    # 创建同步器并运行
    sync = SimpleGitAutoSync(repo_path)
    sync.run()

if __name__ == "__main__":
    main()