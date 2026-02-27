import threading
import queue
import time
import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from tkinter.scrolledtext import ScrolledText


def list_serial_ports():
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]


class SerialReader(threading.Thread):
    def __init__(self, ser, out_queue, stop_event):
        super().__init__(daemon=True)
        self.ser = ser
        self.out_queue = out_queue
        self.stop_event = stop_event

    def run(self):
        try:
            while not self.stop_event.is_set():
                if self.ser.in_waiting:
                    try:
                        data = self.ser.readline()
                    except Exception:
                        data = self.ser.read(self.ser.in_waiting)
                    if not data:
                        continue
                    try:
                        text = data.decode(errors='replace').strip('\r\n')
                    except Exception:
                        text = repr(data)
                    timestamp = time.strftime('%Y-%m-%d %H:%M:%S')
                    self.out_queue.put((timestamp, text))
                else:
                    time.sleep(0.05)
        except Exception as e:
            self.out_queue.put((time.strftime('%Y-%m-%d %H:%M:%S'), f'READER ERROR: {e}'))


class App:
    def __init__(self, root):
        self.root = root
        root.title('端口检测工具 — Diabetes Screening')

        frm = ttk.Frame(root, padding=8)
        frm.grid(sticky='nsew')

        ttk.Label(frm, text='串口：').grid(row=0, column=0, sticky='w')
        self.port_cb = ttk.Combobox(frm, values=list_serial_ports(), width=20)
        self.port_cb.grid(row=0, column=1, sticky='w')
        ttk.Button(frm, text='刷新', command=self.refresh_ports).grid(row=0, column=2, sticky='w')
        ttk.Label(frm, text='波特率：').grid(row=0, column=3, sticky='w')
        self.baud_var = tk.StringVar(value='115200')
        ttk.Entry(frm, textvariable=self.baud_var, width=8).grid(row=0, column=4, sticky='w')
        self.connect_btn = ttk.Button(frm, text='连接', command=self.toggle_connect)
        self.connect_btn.grid(row=0, column=5, sticky='w')

        # Actions
        ttk.Button(frm, text='发送 Ping', command=self.send_ping).grid(row=1, column=0, sticky='w')
        self.ping_cmd = tk.StringVar(value='ping')
        ttk.Entry(frm, textvariable=self.ping_cmd, width=20).grid(row=1, column=1, sticky='w')
        ttk.Button(frm, text='发送', command=self.send_cmd).grid(row=1, column=2, sticky='w')
        self.send_cmd_var = tk.StringVar()
        ttk.Entry(frm, textvariable=self.send_cmd_var, width=28).grid(row=1, column=3, columnspan=2, sticky='w')
        ttk.Button(frm, text='保存日志', command=self.save_log).grid(row=1, column=5, sticky='w')

        ttk.Label(frm, text='连接状态：').grid(row=2, column=0, sticky='w')
        self.status_lbl = ttk.Label(frm, text='未连接')
        self.status_lbl.grid(row=2, column=1, sticky='w')
        ttk.Label(frm, text='最后响应：').grid(row=2, column=2, sticky='w')
        self.last_lbl = ttk.Label(frm, text='')
        self.last_lbl.grid(row=2, column=3, columnspan=3, sticky='w')

        self.log = ScrolledText(frm, width=100, height=20)
        self.log.grid(row=3, column=0, columnspan=6, pady=8)

        # Serial
        self.ser = None
        self.reader = None
        self.out_q = queue.Queue()
        self.stop_event = threading.Event()

        root.protocol('WM_DELETE_WINDOW', self.on_close)
        self._poll()

    def refresh_ports(self):
        self.port_cb['values'] = list_serial_ports()

    def toggle_connect(self):
        if self.ser and self.ser.is_open:
            self.disconnect()
            self.connect_btn.config(text='连接')
        else:
            port = self.port_cb.get()
            baud = self.baud_var.get()
            if not port:
                messagebox.showwarning('提示', '请选择串口')
                return
            try:
                self.ser = serial.Serial(port=port, baudrate=int(baud), timeout=0.1)
            except Exception as e:
                messagebox.showerror('错误', f'打开串口失败: {e}')
                return
            self.stop_event.clear()
            self.reader = SerialReader(self.ser, self.out_q, self.stop_event)
            self.reader.start()
            self.status_lbl.config(text=f'已连接: {port} @ {baud}')
            self.connect_btn.config(text='断开')

    def disconnect(self):
        if self.reader and self.stop_event:
            self.stop_event.set()
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None
        self.reader = None
        self.status_lbl.config(text='未连接')

    def send_ping(self):
        if not self.ser or not getattr(self.ser, 'is_open', False):
            messagebox.showwarning('提示', '未连接，无法发送')
            return
        cmd = self.ping_cmd.get()
        try:
            self.ser.write((cmd + '\n').encode())
            self.log.insert('end', f'SENT: {cmd}\n')
            self.log.see('end')
        except Exception as e:
            self.log.insert('end', f'SEND ERROR: {e}\n')

    def send_cmd(self):
        if not self.ser or not getattr(self.ser, 'is_open', False):
            messagebox.showwarning('提示', '未连接，无法发送')
            return
        cmd = self.send_cmd_var.get()
        try:
            self.ser.write((cmd + '\n').encode())
            self.log.insert('end', f'SENT: {cmd}\n')
            self.log.see('end')
        except Exception as e:
            self.log.insert('end', f'SEND ERROR: {e}\n')

    def save_log(self):
        fn = filedialog.asksaveasfilename(defaultextension='.txt', filetypes=[('Text','*.txt')])
        if fn:
            try:
                with open(fn, 'w', encoding='utf-8') as f:
                    f.write(self.log.get('1.0', 'end'))
                messagebox.showinfo('已保存', fn)
            except Exception as e:
                messagebox.showerror('保存失败', str(e))

    def _poll(self):
        try:
            while True:
                timestamp, text = self.out_q.get_nowait()
                self.log.insert('end', f'[{timestamp}] {text}\n')
                self.log.see('end')
                self.last_lbl.config(text=text)
        except queue.Empty:
            pass
        self.root.after(100, self._poll)

    def on_close(self):
        self.disconnect()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = App(root)
    root.mainloop()


if __name__ == '__main__':
    main()
