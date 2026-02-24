# flash_gui.py
# -*- coding: utf-8 -*-
"""
糖尿病初筛系统 - 一键烧录工具 v1.0

依赖: Python 自带 tkinter, 以及 pyserial (pip install pyserial)
保存位置: 工程根目录 (包含 config/config.h)

功能:
 - 选择腕带/检测模块, 自动修改 config/config.h
 - 下拉选择 COM 端口, 可刷新
 - 开始烧录后执行 pio run ... upload 命令
 - 显示实时日志, 进度条和完成提示
"""
import os
import sys
import threading
import subprocess
import shutil
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
from serial.tools import list_ports

# 全局变量
ROLE_WRIST = "DEVICE_ROLE_WRIST"
ROLE_DETECTOR = "DEVICE_ROLE_DETECTOR"

# config.h 文件路径根据运行方式调整：
#  - 脚本模式下使用当前工作目录
#  - 冻结为 exe 时根据 exe 所在位置计算项目根目录
if getattr(sys, 'frozen', False):
    exe_dir = os.path.dirname(os.path.abspath(sys.executable))
    # exe 通常位于 tools\dist，项目根在上上级
    project_root = os.path.abspath(os.path.join(exe_dir, '..', '..'))
else:
    project_root = os.getcwd()

# 注意: 配置文件实际位于项目根的 config 目录中，而不是 src。
CONFIG_PATH = os.path.join(project_root, "config", "config.h")

# 调试输出路径，方便定位错误
print(f"[DEBUG] project_root = {project_root}")
print(f"[DEBUG] CONFIG_PATH = {CONFIG_PATH}")

# 如果文件不存在，也在程序启动时提示
if not os.path.isfile(CONFIG_PATH):
    print(f"[DEBUG] 配置文件不存在: {CONFIG_PATH}")



def modify_config(role_macro: str):
    """
    将 config.h 中的 DEVICE_ROLE_* 宏设置为指定角色。
    保留其他行不变。
    """
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            lines = f.readlines()
    except Exception as e:
        raise IOError(f"无法读取配置文件 {CONFIG_PATH}: {e}")

    out_lines = []
    found = False
    for ln in lines:
        if ln.strip().startswith("#define DEVICE_ROLE_"):
            if role_macro in ln:
                out_lines.append(f"#define {role_macro}\n")
            else:
                # 注释掉其他角色
                out_lines.append(f"//{ln}" if not ln.strip().startswith("//") else ln)
            found = True
        else:
            out_lines.append(ln)
    if not found:
        # 如果未定义则追加
        out_lines.append(f"#define {role_macro}\n")
    try:
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            f.writelines(out_lines)
    except Exception as e:
        raise IOError(f"保存配置文件 {CONFIG_PATH} 失败: {e}")


class FlashGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("糖尿病初筛系统 - 一键烧录工具 v1.0")
        self.geometry("700x500")

        # 角色选择
        self.role_var = tk.StringVar(value=ROLE_WRIST)
        role_frame = ttk.LabelFrame(self, text="设备角色")
        role_frame.pack(fill="x", padx=10, pady=5)
        ttk.Radiobutton(role_frame, text="腕带主控", variable=self.role_var,
                        value=ROLE_WRIST).pack(side="left", padx=10, pady=5)
        ttk.Radiobutton(role_frame, text="检测模块", variable=self.role_var,
                        value=ROLE_DETECTOR).pack(side="left", padx=10, pady=5)

        # COM 选择
        port_frame = ttk.Frame(self)
        port_frame.pack(fill="x", padx=10, pady=5)
        ttk.Label(port_frame, text="串口:").pack(side="left")
        self.port_cb = ttk.Combobox(port_frame, width=20, state="readonly")
        self.port_cb.pack(side="left", padx=5)
        ttk.Button(port_frame, text="刷新", command=self.refresh_ports).pack(side="left")
        self.refresh_ports()

        # 操作按钮: 烧录与擦除
        button_frame = ttk.Frame(self)
        button_frame.pack(fill="x", padx=10, pady=10)
        self.start_btn = ttk.Button(button_frame, text="🚀 开始烧录", command=self.start_flash)
        self.start_btn.pack(side="left", expand=True, fill="x", padx=5)
        self.erase_btn = ttk.Button(button_frame, text="🧹 擦除 FLASH", command=self.start_erase)
        self.erase_btn.pack(side="left", expand=True, fill="x", padx=5)

        # 进度条
        self.progress = ttk.Progressbar(self, mode="indeterminate")
        self.progress.pack(fill="x", padx=10, pady=5)

        # 日志窗口
        self.log_text = scrolledtext.ScrolledText(self, state="disabled", height=20)
        self.log_text.pack(fill="both", expand=True, padx=10, pady=5)

    def log(self, msg: str):
        """在日志窗口追加一行文本"""
        self.log_text.configure(state="normal")
        self.log_text.insert(tk.END, msg + "\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state="disabled")

    def refresh_ports(self):
        """列出可用串口"""
        ports = [p.device for p in list_ports.comports()]
        self.port_cb['values'] = ports
        if ports:
            self.port_cb.current(0)

    def start_flash(self):
        port = self.port_cb.get()
        if not port:
            messagebox.showwarning("提示", "请先选择一个 COM 端口。")
            return

        role = self.role_var.get()
        try:
            modify_config(role)
            self.log(f"已设置角色: {role}")
        except Exception as e:
            messagebox.showerror("错误", str(e))
            return

        # 禁用控件
        self.start_btn.config(state="disabled")
        self.erase_btn.config(state="disabled")
        self.progress.start(10)
        self.log("开始执行烧录命令...")

        t = threading.Thread(target=self.run_pio, args=(port, "upload"), daemon=True)
        t.start()

    def start_erase(self):
        # erase doesn't technically need a COM port, but we use the selected one if present
        port = self.port_cb.get()
        # disable controls
        self.start_btn.config(state="disabled")
        self.erase_btn.config(state="disabled")
        self.progress.start(10)
        self.log("开始擦除 Flash...")

        t = threading.Thread(target=self.run_pio, args=(port, "erase"), daemon=True)
        t.start()

    def run_pio(self, port: str, target: str = "upload"):
        # try to locate the pio executable on PATH
        pio_exe = shutil.which("pio") or shutil.which("platformio")
        cmd = None
        if pio_exe:
            cmd = [pio_exe, "run", "-e", "esp32s3_final", "-t", target]
            if port:
                cmd += ["--upload-port", port]
        else:
            # fallback: run via python -m platformio if python is available
            python_exe = shutil.which("python") or shutil.which("python3")
            if python_exe:
                cmd = [python_exe, "-m", "platformio", "run", "-e", "esp32s3_final",
                       "-t", target]
                if port:
                    cmd += ["--upload-port", port]
            else:
                self.log("未找到 'pio' 命令，也无法定位 Python 解释器，无法进行烧录。")
                self.after(0, self.finish, False)
                return

        try:
            # ensure PlatformIO is run from project root so pio.ini can be located
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, cwd=project_root)
        except Exception as e:
            self.log(f"启动命令失败: {e}")
            self.after(0, self.finish, False)
            return

        # 读取输出
        for line in proc.stdout:
            self.after(0, self.log, line.rstrip())

        proc.wait()
        success = proc.returncode == 0
        self.after(0, self.finish, success, target)

    def finish(self, success: bool, target: str = "upload"):
        """命令结束后的处理"""
        self.progress.stop()
        self.start_btn.config(state="normal")
        self.erase_btn.config(state="normal")
        if success:
            if target == "upload":
                messagebox.showinfo("完成", "烧录完成！")
            else:
                messagebox.showinfo("完成", "擦除完成！")
        else:
            if target == "upload":
                messagebox.showerror("失败", "烧录失败，请查看日志。")
            else:
                messagebox.showerror("失败", "擦除失败，请查看日志。")


if __name__ == "__main__":
    app = FlashGUI()
    app.mainloop()
