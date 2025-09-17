import tkinter as tk
from tkinter import filedialog, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import time
import threading

SERIAL_PORT = None
BAUDRATE = 115200
ser = None

def connect_serial(port):
    global ser, SERIAL_PORT
    SERIAL_PORT = port
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
        time.sleep(2)  # wait for Arduino reset
    except Exception as e:
        messagebox.showerror("Error", f"Cannot open port {SERIAL_PORT}: {e}")

def send_line(line, log_box):
    if ser and ser.is_open:
        ser.write((line + "\n").encode())
        response = ser.readline().decode(errors="ignore").strip()
        if response:
            log_box.insert(tk.END, ">> " + response + "\n")
            log_box.see(tk.END)
    else:
        messagebox.showwarning("Warning", "No serial connection")

def send_file(log_box):
    filename = filedialog.askopenfilename(filetypes=[("G-code", "*.gcode;*.nc;*.txt"), ("All files", "*.*")])
    if not filename:
        return

    def worker():
        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                send_line(line, log_box)
                time.sleep(0.05)

    threading.Thread(target=worker, daemon=True).start()

def send_zero(log_box):
    send_line("G92 X0 Y0 Z0", log_box)

def main():
    root = tk.Tk()
    root.title("Simple G-code Sender")

    frame = tk.Frame(root)
    frame.pack(padx=10, pady=10)

    tk.Label(frame, text="Select COM Port:").grid(row=0, column=0, padx=5)
    com_var = tk.StringVar()
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if not ports:
        ports = ["COM3"]
    com_var.set(ports[0])
    com_menu = tk.OptionMenu(frame, com_var, *ports)
    com_menu.grid(row=0, column=1, padx=5)

    def connect():
        connect_serial(com_var.get())
        messagebox.showinfo("Info", f"Connected to {com_var.get()}")

    btn_connect = tk.Button(frame, text="Connect", command=connect)
    btn_connect.grid(row=0, column=2, padx=5)

    btn_send = tk.Button(frame, text="Send File", command=lambda: send_file(log_box))
    btn_send.grid(row=1, column=0, padx=5, pady=5)

    btn_zero = tk.Button(frame, text="Set Zero (G92)", command=lambda: send_zero(log_box))
    btn_zero.grid(row=1, column=1, padx=5, pady=5)

    log_box = scrolledtext.ScrolledText(root, width=60, height=20)
    log_box.pack(padx=10, pady=10)

    root.mainloop()

    if ser and ser.is_open:
        ser.close()

if __name__ == "__main__":
    main()