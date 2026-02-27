import threading
import queue
import sys
import time
import os

import serial
import serial.tools.list_ports
import PySimpleGUI as sg


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


def make_window():
    # 兼容不同版本/打包时可能缺少的接口
    if hasattr(sg, 'theme'):
        try:
            sg.theme('DarkBlue3')
        except Exception:
            pass
    elif hasattr(sg, 'ChangeLookAndFeel'):
        try:
            sg.ChangeLookAndFeel('Dark')
        except Exception:
            pass

    port_list = list_serial_ports()

    layout = [
        [sg.Text('串口：'), sg.Combo(port_list, key='-PORT-', size=(20, 1)), sg.Button('刷新', key='-REFRESH-'),
         sg.Text('波特率：'), sg.Input('115200', key='-BAUD-', size=(8, 1)), sg.Button('连接', key='-CONNECT-')],
        [sg.Frame('动作', [[sg.Button('发送 Ping', key='-PING-'), sg.Input('ping', key='-PING_CMD-', size=(20,1)),
                            sg.Button('发送', key='-SEND-'), sg.Input('', key='-SEND_CMD-', size=(28,1)),
                            sg.Button('保存日志', key='-SAVE-')]])],
        [sg.Frame('状态', [[sg.Text('连接状态：', size=(10,1)), sg.Text('未连接', key='-STATUS-', size=(30,1))]]),
         sg.Frame('最后响应', [[sg.Text('', key='-LAST-', size=(60,1))]])],
        [sg.Multiline('', key='-LOG-', size=(100, 20), autoscroll=True, disabled=False)],
        [sg.Button('退出')]
    ]

    return sg.Window('端口检测工具 — Diabetes Screening', layout, finalize=True)


def main():
    window = make_window()
    ser = None
    reader = None
    out_q = queue.Queue()
    stop_event = threading.Event()

    def connect(port, baud):
        nonlocal ser, reader, stop_event
        try:
            ser = serial.Serial(port=port, baudrate=int(baud), timeout=0.1)
        except Exception as e:
            window['-STATUS-'].update(f'打开串口失败: {e}')
            return False
        stop_event.clear()
        reader = SerialReader(ser, out_q, stop_event)
        reader.start()
        window['-STATUS-'].update(f'已连接: {port} @ {baud}')
        return True

    def disconnect():
        nonlocal ser, reader, stop_event
        if reader and stop_event:
            stop_event.set()
        if ser:
            try:
                ser.close()
            except Exception:
                pass
        ser = None
        reader = None
        window['-STATUS-'].update('未连接')

    while True:
        event, values = window.read(timeout=100)
        if event == sg.WIN_CLOSED or event == '退出':
            disconnect()
            break

        if event == '-REFRESH-':
            window['-PORT-'].update(values=list_serial_ports())

        if event == '-CONNECT-':
            if ser and ser.is_open:
                disconnect()
                window['-CONNECT-'].update('连接')
            else:
                port = values['-PORT-']
                baud = values['-BAUD-']
                if not port:
                    window['-STATUS-'].update('请选择串口')
                else:
                    ok = connect(port, baud)
                    if ok:
                        window['-CONNECT-'].update('断开')

        if event == '-PING-':
            if not ser or not ser.is_open:
                window['-STATUS-'].update('未连接，无法发送')
            else:
                cmd = values.get('-PING_CMD-', 'ping')
                try:
                    ser.write((cmd + '\n').encode())
                    window['-LOG-'].print(f'SENT: {cmd}')
                except Exception as e:
                    window['-LOG-'].print(f'SEND ERROR: {e}')

        if event == '-SEND-':
            if not ser or not ser.is_open:
                window['-STATUS-'].update('未连接，无法发送')
            else:
                cmd = values.get('-SEND_CMD-', '')
                try:
                    ser.write((cmd + '\n').encode())
                    window['-LOG-'].print(f'SENT: {cmd}')
                except Exception as e:
                    window['-LOG-'].print(f'SEND ERROR: {e}')

        if event == '-SAVE-':
            log_text = window['-LOG-'].get()
            fn = sg.popup_get_file('保存日志为', save_as=True, default_extension='txt', file_types=(('Text','*.txt'),))
            if fn:
                try:
                    with open(fn, 'w', encoding='utf-8') as f:
                        f.write(log_text)
                    sg.popup('已保存', f'{fn}')
                except Exception as e:
                    sg.popup('保存失败', str(e))

        # 处理来自串口的消息
        try:
            while True:
                timestamp, text = out_q.get_nowait()
                window['-LOG-'].print(f'[{timestamp}] {text}')
                window['-LAST-'].update(f'{text}')
        except queue.Empty:
            pass

    window.close()


if __name__ == '__main__':
    main()
