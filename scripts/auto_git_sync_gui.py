#!/usr/bin/env python3
"""
Auto Git Sync - 图形界面
选择仓库目录，启动/停止自动同步到 GitHub，并显示运行日志。
使用 Python 内置 tkinter，无需额外依赖（本地版）；完整版需安装 watchdog。
"""

import os
import sys
import subprocess
import threading
import queue
from pathlib import Path

try:
    import tkinter as tk
    from tkinter import ttk, filedialog, scrolledtext, messagebox
except ImportError:
    print("需要 tkinter。请使用已安装 tkinter 的 Python（Windows/macOS 通常已自带）。")
    sys.exit(1)

# 脚本/程序路径：打包成 exe 时调用同目录下的两个 sync exe，否则调用 .py
if getattr(sys, "frozen", False):
    SCRIPT_DIR = Path(sys.executable).parent
    PATH_SIMPLE = SCRIPT_DIR / "AutoGitSync_Local.exe"
    PATH_FULL = SCRIPT_DIR / "AutoGitSync_Full.exe"
else:
    SCRIPT_DIR = Path(__file__).resolve().parent
    PATH_SIMPLE = SCRIPT_DIR / "auto_git_sync_simple.py"
    PATH_FULL = SCRIPT_DIR / "auto_git_sync.py"


class SyncGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Auto Git Sync - 自动同步到 GitHub")
        self.root.minsize(520, 420)
        self.root.geometry("640x500")

        self.repo_path = tk.StringVar()
        self.sync_mode = tk.StringVar(value="local")  # local | full
        self.process = None
        self.output_queue = queue.Queue()
        self.after_id = None

        self._build_ui()
        self._start_output_reader()

    def _build_ui(self):
        main = ttk.Frame(self.root, padding=10)
        main.pack(fill=tk.BOTH, expand=True)

        # 仓库路径
        path_frame = ttk.LabelFrame(main, text="Git 仓库目录", padding=5)
        path_frame.pack(fill=tk.X, pady=(0, 5))
        ttk.Entry(path_frame, textvariable=self.repo_path, width=60).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        ttk.Button(path_frame, text="浏览...", command=self._browse).pack(side=tk.RIGHT)

        # 模式选择
        mode_frame = ttk.LabelFrame(main, text="同步模式", padding=5)
        mode_frame.pack(fill=tk.X, pady=(0, 5))
        ttk.Radiobutton(mode_frame, text="本地版（轮询，约 30 秒检查一次，无需额外依赖）", variable=self.sync_mode, value="local").pack(anchor=tk.W)
        ttk.Radiobutton(mode_frame, text="完整版（实时监控，需已安装 watchdog）", variable=self.sync_mode, value="full").pack(anchor=tk.W)

        # 启停与清空
        btn_frame = ttk.Frame(main)
        btn_frame.pack(fill=tk.X, pady=(0, 5))
        self.btn_start = ttk.Button(btn_frame, text="开始同步", command=self._start_sync)
        self.btn_start.pack(side=tk.LEFT, padx=(0, 5))
        self.btn_stop = ttk.Button(btn_frame, text="停止同步", command=self._stop_sync, state=tk.DISABLED)
        self.btn_stop.pack(side=tk.LEFT, padx=(0, 5))
        self.btn_manual = ttk.Button(btn_frame, text="手动提交", command=self._manual_commit)
        self.btn_manual.pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(btn_frame, text="清空日志", command=self._clear_log).pack(side=tk.LEFT)

        # 日志区域
        log_frame = ttk.LabelFrame(main, text="运行日志", padding=5)
        log_frame.pack(fill=tk.BOTH, expand=True)
        self.log_text = scrolledtext.ScrolledText(log_frame, height=15, wrap=tk.WORD, state=tk.DISABLED, font=("Consolas", 9))
        self.log_text.pack(fill=tk.BOTH, expand=True)

    def _browse(self):
        path = filedialog.askdirectory(title="选择 Git 仓库目录")
        if path:
            self.repo_path.set(path)

    def _log(self, msg):
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, msg)
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def _clear_log(self):
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.delete(1.0, tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def _start_output_reader(self):
        def check_queue():
            try:
                while True:
                    line = self.output_queue.get_nowait()
                    self._log(line)
            except queue.Empty:
                pass
            self.after_id = self.root.after(100, check_queue)

        self.after_id = self.root.after(100, check_queue)

    def _start_sync(self):
        path = self.repo_path.get().strip()
        if not path or not os.path.isdir(path):
            messagebox.showwarning("请选择目录", "请先选择有效的 Git 仓库目录。")
            return
        if not os.path.exists(os.path.join(path, ".git")):
            messagebox.showerror("不是 Git 仓库", f"该目录不是 Git 仓库：\n{path}")
            return

        script = PATH_FULL if self.sync_mode.get() == "full" else PATH_SIMPLE
        if not script.exists():
            messagebox.showerror("错误", f"找不到同步脚本：\n{script}")
            return

        self._log(f"\n>>> 启动同步：{path}（{'完整版' if self.sync_mode.get() == 'full' else '本地版'}）\n")
        self.btn_start.configure(state=tk.DISABLED)
        self.btn_stop.configure(state=tk.NORMAL)

        is_exe = str(script).lower().endswith(".exe")
        cmd = [str(script), "--path", path] if is_exe else [sys.executable, str(script), "--path", path]
        creationflags = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0

        def run():
            try:
                self.process = subprocess.Popen(
                    cmd,
                    cwd=path,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    creationflags=creationflags,
                )
                for line in iter(self.process.stdout.readline, ""):
                    self.output_queue.put(line)
                self.process.wait()
            except Exception as e:
                self.output_queue.put(f"错误: {e}\n")
            finally:
                self.output_queue.put("\n>>> 同步已停止\n")
                self.root.after(0, self._on_sync_stopped)

        self.thread = threading.Thread(target=run, daemon=True)
        self.thread.start()

    def _on_sync_stopped(self):
        self.process = None
        self.btn_start.configure(state=tk.NORMAL)
        self.btn_stop.configure(state=tk.DISABLED)

    def _stop_sync(self):
        if self.process and self.process.poll() is None:
            self.process.terminate()
            self._log(">>> 正在停止...\n")

    def run(self):
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.mainloop()

    def _on_close(self):
        if self.after_id:
            self.root.after_cancel(self.after_id)
        if self.process and self.process.poll() is None:
            if messagebox.askokcancel("退出", "同步正在运行，确定要退出并停止同步吗？"):
                self.process.terminate()
        self.root.destroy()

    # ---------- manual commit functionality ----------
    def _manual_commit(self):
        path = self.repo_path.get().strip()
        if not path or not os.path.isdir(path):
            messagebox.showwarning("请选择目录", "请先选择有效的 Git 仓库目录。")
            return
        if not os.path.exists(os.path.join(path, ".git")):
            messagebox.showerror("不是 Git 仓库", f"该目录不是 Git 仓库：\n{path}")
            return

        # run commit in background thread
        self._log("\n>>> 手动提交: 开始\n")
        threading.Thread(target=self._run_manual_commit, args=(path,), daemon=True).start()

    def _run_manual_commit(self, repo_path):
        """在指定仓库执行 git add/commit/push 并将输出写入日志。"""
        def log(msg):
            self.output_queue.put(msg)

        try:
            orig = os.getcwd()
            os.chdir(repo_path)

            # 状态检查
            status = subprocess.run(['git', 'status', '--porcelain'],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, encoding='utf-8')
            if not status.stdout.strip():
                log("没有未提交的更改\n")
                os.chdir(orig)
                return
            log(f"检测到未提交的更改:\n{status.stdout}\n")

            # git add .
            add = subprocess.run(['git', 'add', '.'],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True, encoding='utf-8')
            if add.returncode != 0:
                log(f"git add 失败: {add.stderr}\n")
                os.chdir(orig)
                return
            log("git add 成功\n")

            # commit
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            commit_msg = f"Manual commit: {timestamp}"
            commit = subprocess.run(['git', 'commit', '-m', commit_msg],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    text=True, encoding='utf-8')
            if commit.returncode != 0:
                if "nothing to commit" in commit.stdout.lower():
                    log("没有需要提交的更改\n")
                else:
                    log(f"git commit 失败: {commit.stderr}\n")
                os.chdir(orig)
                return
            log(f"提交成功: {commit_msg}\n")

            # push
            push = subprocess.run(['git', 'push', 'origin', 'main'],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  text=True, encoding='utf-8')
            if push.returncode != 0:
                log(f"git push 失败: {push.stderr}\n")
            else:
                log("推送成功\n")

            os.chdir(orig)
        except Exception as e:
            log(f"手动提交异常: {e}\n")
            try:
                os.chdir(orig)
            except:
                pass


def main():
    app = SyncGUI()
    app.run()


if __name__ == "__main__":
    main()
