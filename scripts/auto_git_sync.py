#!/usr/bin/env python3
"""
自动Git同步脚本
监控指定目录的文件变化，自动执行git add、commit、push操作
支持Windows/Mac/Linux环境
"""

import os
import sys
import time
import subprocess
import logging
import argparse
from datetime import datetime
from pathlib import Path
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

# 被 GUI 或管道调用时强制使用 UTF-8，避免 Windows 下乱码
if hasattr(sys.stdout, "buffer") and (not getattr(sys.stdout, "encoding", None) or sys.stdout.encoding.upper() != "UTF-8"):
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", line_buffering=True)
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace", line_buffering=True)

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('auto_git_sync.log', encoding='utf-8'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

class GitAutoSyncHandler(FileSystemEventHandler):
    """处理文件系统事件，自动执行Git操作"""
    
    def __init__(self, repo_path, commit_message_prefix="Auto-commit"):
        self.repo_path = repo_path
        self.commit_message_prefix = commit_message_prefix
        self.last_commit_time = 0
        self.debounce_seconds = 5  # 防抖时间，避免频繁提交
        self.ignored_extensions = ['.log', '.tmp', '.swp', '.pyc', '__pycache__']
        self.ignored_dirs = ['.git', '.vscode', '.pio', '__pycache__']
        
    def should_ignore(self, path):
        """检查路径是否应该被忽略"""
        path_str = str(path)
        
        # 检查是否在忽略的目录中
        for ignored_dir in self.ignored_dirs:
            if f'/{ignored_dir}/' in path_str.replace('\\', '/') or path_str.endswith(f'/{ignored_dir}'):
                return True
        
        # 检查文件扩展名
        for ext in self.ignored_extensions:
            if path_str.endswith(ext):
                return True
        
        return False
    
    def on_modified(self, event):
        """文件修改事件处理"""
        if not event.is_directory:
            self.handle_file_change(event.src_path)
    
    def on_created(self, event):
        """文件创建事件处理"""
        if not event.is_directory:
            self.handle_file_change(event.src_path)
    
    def on_deleted(self, event):
        """文件删除事件处理"""
        if not event.is_directory:
            self.handle_file_change(event.src_path)
    
    def on_moved(self, event):
        """文件移动/重命名事件处理"""
        if not event.is_directory:
            self.handle_file_change(event.dest_path)
    
    def handle_file_change(self, file_path):
        """处理文件变化"""
        # 检查是否应该忽略此文件
        if self.should_ignore(file_path):
            logger.debug(f"忽略文件变化: {file_path}")
            return
        
        # 防抖处理：避免频繁提交
        current_time = time.time()
        if current_time - self.last_commit_time < self.debounce_seconds:
            logger.debug(f"防抖期内，跳过提交: {file_path}")
            return
        
        logger.info(f"检测到文件变化: {file_path}")
        
        try:
            # 执行Git操作
            self.git_auto_commit(file_path)
            self.last_commit_time = current_time
        except Exception as e:
            logger.error(f"Git操作失败: {e}")
    
    def git_auto_commit(self, changed_file=None):
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
                return
            
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
                return
            
            logger.info("成功执行 git add")
            
            # 3. 创建提交信息
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            commit_message = f"{self.commit_message_prefix}: {timestamp}"
            
            if changed_file:
                file_name = os.path.basename(changed_file)
                commit_message = f"{self.commit_message_prefix}: {file_name} - {timestamp}"
            
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
                else:
                    logger.error(f"git commit 失败: {commit_result.stderr}")
                    os.chdir(original_cwd)
                    return
            
            logger.info(f"成功提交: {commit_message}")
            
            # 5. 推送到远程仓库（明确指定main分支）
            push_result = subprocess.run(
                ['git', 'push', 'origin', 'main'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                encoding='utf-8'
            )
            
            if push_result.returncode != 0:
                logger.error(f"git push 失败: {push_result.stderr}")
                if "Permission denied (publickey)" in push_result.stderr or "Could not read from remote" in push_result.stderr:
                    logger.error("提示: 请检查 SSH 密钥是否已配置并加入 ssh-agent，或改用 HTTPS 远程地址。")
                # 尝试先拉取最新更改（有未提交更改时先暂存再 pull）
                logger.info("尝试先拉取最新更改...")
                stashed = False
                stash_result = subprocess.run(
                    ['git', 'stash', 'push', '-u', '-m', 'auto-sync-before-pull'],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                    universal_newlines=True, encoding='utf-8'
                )
                if stash_result.returncode == 0 and "No local changes" not in stash_result.stderr:
                    stashed = True
                pull_result = subprocess.run(
                    ['git', 'pull', '--rebase'],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                    encoding='utf-8'
                )
                if stashed:
                    subprocess.run(['git', 'stash', 'pop'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   universal_newlines=True, encoding='utf-8')
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
                    else:
                        logger.info("成功推送到远程仓库")
                else:
                    logger.error(f"git pull 失败: {pull_result.stderr}")
            else:
                logger.info("成功推送到远程仓库")
            
            # 恢复原始工作目录
            os.chdir(original_cwd)
            
        except Exception as e:
            logger.error(f"Git操作异常: {e}")
            # 确保恢复原始工作目录
            try:
                os.chdir(original_cwd)
            except:
                pass

def check_dependencies():
    """检查必要的依赖"""
    try:
        import watchdog
        logger.info("watchdog 库已安装")
        return True
    except ImportError:
        logger.error("未安装 watchdog 库")
        logger.info("请使用以下命令安装: pip install watchdog")
        return False

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="自动Git同步脚本（完整版）")
    parser.add_argument("--path", type=str, default=None, help="要监控的 Git 仓库目录（默认：当前目录或 exe 所在目录）")
    args = parser.parse_args()

    print("=" * 60)
    print("自动Git同步脚本")
    print("监控文件变化并自动提交到GitHub")
    print("=" * 60)
    
    # 检查依赖
    if not check_dependencies():
        print("\n请先安装依赖: pip install watchdog")
        sys.exit(1)
    
    if args.path and os.path.isdir(args.path):
        repo_path = os.path.abspath(args.path)
    elif getattr(sys, 'frozen', False):
        repo_path = os.path.dirname(os.path.abspath(sys.executable))
    else:
        repo_path = os.getcwd()
    print(f"\n监控目录: {repo_path}")
    
    # 检查是否是Git仓库
    if not os.path.exists(os.path.join(repo_path, '.git')):
        print(f"错误: {repo_path} 不是Git仓库")
        sys.exit(1)
    
    # 创建事件处理器
    event_handler = GitAutoSyncHandler(repo_path)
    
    # 创建观察者
    observer = Observer()
    observer.schedule(event_handler, repo_path, recursive=True)
    
    print("\n开始监控文件变化...")
    print("按 Ctrl+C 停止监控")
    print("\n日志文件: auto_git_sync.log")
    print("忽略的目录: .git, .vscode, .pio, __pycache__")
    print("忽略的文件扩展名: .log, .tmp, .swp, .pyc")
    print(f"防抖时间: {event_handler.debounce_seconds}秒")
    print("-" * 60)
    
    try:
        observer.start()
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n\n停止监控...")
        observer.stop()
    except Exception as e:
        logger.error(f"监控异常: {e}")
        observer.stop()
    
    observer.join()
    print("监控已停止")

if __name__ == "__main__":
    main()