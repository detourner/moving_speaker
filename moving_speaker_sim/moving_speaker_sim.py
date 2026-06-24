import tkinter as tk
import ttkbootstrap as ttk
from ttkbootstrap.constants import *
import random
import serial
import threading
import queue
import argparse
import datetime
import ConsigneControl as ConsigneControl
import MotorDisplayControl as MotorDisplayControl


class SerialReader:
    """Handles serial communication with the Arduino and optional logging."""
    def __init__(self, port='COM4', baudrate=115200, data_queue=None, log_path=None):
        self.port = port
        self.baudrate = baudrate
        self.data_queue = data_queue if data_queue else queue.Queue()
        self.running = False
        self.serial_connection = None
        self.log_path = log_path
        self.log_fh = None
        if self.log_path:
            try:
                # open in append mode
                self.log_fh = open(self.log_path, 'a', encoding='utf-8')
            except Exception as e:
                print(f"Unable to open log file '{self.log_path}': {e}")
        
    def connect(self):
        """Establish the serial connection."""
        try:
            self.serial_connection = serial.Serial(self.port, self.baudrate, timeout=1)
            self.running = True
            return True
        except serial.SerialException as e:
            print(f"Serial connection error: {e}")
            return False
    
    def disconnect(self):
        """Close the serial connection."""
        self.running = False
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
        if self.log_fh:
            try:
                self.log_fh.close()
            except Exception:
                pass
    
    def write_command(self, command):
        """Send a command line to the Arduino over serial."""
        if self.serial_connection and self.serial_connection.is_open:
            try:
                self.serial_connection.write((command + '\n').encode('utf-8'))
                # log outgoing frame if logging enabled
                if self.log_fh:
                    ts = datetime.datetime.now().isoformat(sep=' ', timespec='milliseconds')
                    try:
                        self.log_fh.write(f"{ts} Serial -> {command}\n")
                        self.log_fh.flush()
                    except Exception:
                        pass
                    print(f"Serial -> {command}")
                return True
            except Exception as e:
                print(f"Error sending command: {e}")
                return False
        return False
    
    def read_loop(self):
        """Read loop (intended to run in a background thread)."""
        while self.running:
            try:
                if self.serial_connection.in_waiting > 0:
                    line = self.serial_connection.readline().decode('utf-8').strip()
                    if not line:
                        continue

                    # log incoming frame if logging enabled
                    if self.log_fh:
                        ts = datetime.datetime.now().isoformat(sep=' ', timespec='milliseconds')
                        try:
                            self.log_fh.write(f"{ts} Serial <- {line}\n")
                            self.log_fh.flush()
                        except Exception:
                            pass

                    # If it's a P: status frame, parse and queue it for the UI
                    if line.startswith('P:'):
                        data = line[2:]
                        parts = data.split(',')
                        if len(parts) == 12:
                            try:
                                motor_data = {
                                    'moving_a': bool(int(parts[0])),
                                    'position_a': float(parts[1]),
                                    'speed_a': float(parts[2]),

                                    'moving_b': bool(int(parts[3])),
                                    'position_b': float(parts[4]),
                                    'speed_b': float(parts[5]),

                                    'moving_c': bool(int(parts[6])),  
                                    'position_c': float(parts[7]),
                                    'speed_c': float(parts[8]),

                                    'moving_d': bool(int(parts[9])),
                                    'position_d': float(parts[10]),
                                    'speed_d': float(parts[11])
                                }
                                self.data_queue.put(motor_data)
                            except ValueError:
                                print(f"Parsing error: {line}")
                    else:
                        # For any other frame (S:, I:, E: or custom), print it to the console
                        print(f"Serial <- {line}")
            except Exception as e:
                print(f"Serial read error: {e}")


class MotorHeadUI:
    def __init__(self, root, serial_port='COM4', log_path=None):
        self.root = root
        self.root.title("Moving Speaker Sim V1.0")
        self.style = ttk.Style("flatly")

        # Simulated / received values
        self.position_a = 0
        self.position_b = 0
        self.position_c = 0
        self.position_d = 0
        self.moving_a = False
        self.moving_b = False
        self.moving_c = False
        self.moving_d = False
        self.speed_a = 0
        self.speed_b = 0
        self.speed_c = 0
        self.speed_d = 0

        # Serial initialization
        self.data_queue = queue.Queue()
        self.serial_reader = SerialReader(port=serial_port, data_queue=self.data_queue, baudrate=115200, log_path=log_path)
        self.serial_connected = self.serial_reader.connect()
        
        if self.serial_connected:
            # Start the serial reading thread
            self.serial_thread = threading.Thread(target=self.serial_reader.read_loop, daemon=True)
            self.serial_thread.start()
            print(f"Serial connection established on {serial_port}")
        else:
            print(f"Unable to connect to {serial_port}")

        # Main frame
        main = ttk.Frame(root, padding=20)
        main.pack(fill=BOTH, expand=YES)

        # --- Meters ---------------------------------------------------------
        meters_frame = ttk.Frame(main)
        meters_frame.pack(fill=X, pady=10)

        self.motorADisp = MotorDisplayControl.MotorDisplayControl(meters_frame, title_text="Motor A Position [°]", initial_value=0.0)
        self.motorADisp.pack(side=LEFT, padx=20)
        self.motorBDisp = MotorDisplayControl.MotorDisplayControl(meters_frame, title_text="Motor B Position [°]", initial_value=0.0)
        self.motorBDisp.pack(side=LEFT, padx=20)
        self.motorCDisp = MotorDisplayControl.MotorDisplayControl(meters_frame, title_text="Motor C Position [°]", initial_value=0.0)
        self.motorCDisp.pack(side=LEFT, padx=20)
        self.motorDDisp = MotorDisplayControl.MotorDisplayControl(meters_frame, title_text="Motor D Position [°]", initial_value=0.0)
        self.motorDDisp.pack(side=LEFT, padx=20)
        

       

        # --- Sliders for command inputs ------------------------------------
        sliders_frame = ttk.Labelframe(main, text="Consignes")
        sliders_frame.pack(fill=X, pady=10, padx=10)

        self.motA_target = ConsigneControl.ConsigneControl(sliders_frame, label="mot A Position [°]", min_val=-90, max_val=90, initial=0, step = 0.01)
        self.motA_target.pack(fill="x", padx=10, pady=10)

        self.motA_speed = ConsigneControl.ConsigneControl(sliders_frame, label="mot A Vitesse", min_val=0.01, max_val=23, initial=17, step = 0.01)
        self.motA_speed.pack(fill="x", padx=10, pady=10)

        self.motA_accel = ConsigneControl.ConsigneControl(sliders_frame, label="mot A Accell", min_val=1.1, max_val=113, initial=50, step = 0.1)
        self.motA_accel.pack(fill="x", padx=10, pady=10)

        self.motB_target = ConsigneControl.ConsigneControl(sliders_frame, label="mot B Position [°]", min_val=0, max_val=359.99, initial=0, with_rotation=True, step=0.01)
        self.motB_target.pack(fill="x", padx=10, pady=10)

        self.motB_speed = ConsigneControl.ConsigneControl(sliders_frame, label="mot B Vitesse", min_val=0.01, max_val=23, initial=17, step = 0.01)
        self.motB_speed.pack(fill="x", padx=10, pady=10)

        self.motB_accel = ConsigneControl.ConsigneControl(sliders_frame, label="mot B Accel", min_val=1.1, max_val=113, initial=50, step = 0.1)
        self.motB_accel.pack(fill="x", padx=10, pady=10)

        self.motC_target = ConsigneControl.ConsigneControl(sliders_frame, label="mot C Position [°]", min_val=-90, max_val=90, initial=0, step = 0.01)
        self.motC_target.pack(fill="x", padx=10, pady=10)

        self.motC_speed = ConsigneControl.ConsigneControl(sliders_frame, label="mot C Vitesse", min_val=0.01, max_val=23, initial=17, step = 0.01)
        self.motC_speed.pack(fill="x", padx=10, pady=10)

        self.motC_accel = ConsigneControl.ConsigneControl(sliders_frame, label="mot C Accell", min_val=1.1, max_val=113, initial=50, step = 0.1)
        self.motC_accel.pack(fill="x", padx=10, pady=10)

        self.motD_target = ConsigneControl.ConsigneControl(sliders_frame, label="mot D Position [°]", min_val=0, max_val=359.99, initial=0, with_rotation=True, step=0.01)
        self.motD_target.pack(fill="x", padx=10, pady=10)

        self.motD_speed = ConsigneControl.ConsigneControl(sliders_frame, label="mot D Vitesse", min_val=0.01, max_val=23, initial=17, step = 0.01)
        self.motD_speed.pack(fill="x", padx=10, pady=10)

        self.motD_accel = ConsigneControl.ConsigneControl(sliders_frame, label="mot D Accel", min_val=1.1, max_val=113, initial=50, step = 0.1)
        self.motD_accel.pack(fill="x", padx=10, pady=10)


       
       

        # --- Control buttons -------------------------------------------------
        buttons_frame = ttk.Frame(sliders_frame)
        buttons_frame.pack(fill=X, pady=10)

        lbl_target_frame = ttk.Labelframe(buttons_frame, text="Statut des consignes")
        self.lbl_target = ttk.Label(lbl_target_frame, text="Aucune consigne envoyée", foreground="black")
        self.lbl_target.pack(padx=5, pady=5)
        
        ttk.Button(buttons_frame, text="Envoyer consignes", bootstyle=SUCCESS, command=self.send_target).pack(side=LEFT, padx=5)
       
        # Periodic update (simulation or reading from serial)
        self.update_values()

    def send_target(self):
        """Send the command set to the Arduino."""
        mAt = float(self.motA_target.get())
        mAs = float(self.motA_speed.get())
        mAa = float(self.motA_accel.get())
        mBt = float(self.motB_target.get())
        mBs = float(self.motB_speed.get())
        mBd = int(self.motB_target.getDirection())  # direction de rotation
        mBa = float(self.motB_accel.get())

        mCt = float(self.motC_target.get())
        mCs = float(self.motC_speed.get())
        mCa = float(self.motC_accel.get())

        mDt = float(self.motD_target.get())
        mDs = float(self.motD_speed.get())
        mDd = int(self.motD_target.getDirection())  # direction de rotation
        mDa = float(self.motD_accel.get())

        # Format: motA_target,motA_speed,motA_accel,motB_target,motB_speed,motB_dir,motB_accel,
        #         motC_target,motC_speed,motC_accel,motD_target,motD_speed,motD_dir,motD_accel
        command = f"{mAt},{mAs},{mAa},{mBt},{mBs},{mBd},{mBa},{mCt},{mCs},{mCa},{mDt},{mDs},{mDd},{mDa}"
        if self.serial_connected:
            if self.serial_reader.write_command(command):
                # Use correct accel variables and reasonable decimal precision for display
                self.lbl_target.config(
                    text=f"✓ Consignes envoyées → MotA: {mAt:.2f}° @{mAs:.2f}/{mAa:.2f} | MotB: {mBt:.2f}° @{mBs:.2f} Dir:{mBd} / {mBa:.2f} | MotC: {mCt:.2f}° @{mCs:.2f}/{mCa:.2f} | MotD: {mDt:.2f}° @{mDs:.2f} Dir:{mDd} / {mDa:.2f}",
                    foreground="green"
                )
            else:
                self.lbl_target.config(text="✗ Erreur d'envoi", foreground="red")
        else:
            self.lbl_target.config(text="✗ Non connecté à Arduino", foreground="red")
    
   

    def update_values(self):
        """Read sensor data (from serial or simulation) and update the UI."""
        # Read data from the serial queue
        try:
            while True:
                motor_data = self.data_queue.get_nowait()
                self.moving_a = motor_data['moving_a']
                self.moving_b = motor_data['moving_b']
                self.moving_c = motor_data['moving_c']
                self.moving_d = motor_data['moving_d']
                self.position_a = motor_data['position_a']
                self.position_b = motor_data['position_b']
                self.position_c = motor_data['position_c']
                self.position_d = motor_data['position_d']
                self.speed_a = motor_data['speed_a']
                self.speed_b = motor_data['speed_b']
                self.speed_c = motor_data['speed_c']
                self.speed_d = motor_data['speed_d']

        except queue.Empty:
            pass
        
        # Update meter widgets
        self.motorADisp.update_position(self.position_a)
        self.motorBDisp.update_position(self.position_b)
        self.motorCDisp.update_position(self.position_c)
        self.motorDDisp.update_position(self.position_d)
        self.motorADisp.update_speed(self.speed_a)  
        self.motorBDisp.update_speed(self.speed_b) 
        self.motorCDisp.update_speed(self.speed_c) 
        self.motorDDisp.update_speed(self.speed_d) 
        self.motorADisp.update_moving(self.moving_a)
        self.motorBDisp.update_moving(self.moving_b)
        self.motorCDisp.update_moving(self.moving_c)
        self.motorDDisp.update_moving(self.moving_d)

        # update every ans 50ms
        self.root.after(50, self.update_values)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Motor Head UI (serial port optional)")
    parser.add_argument("-p", "--port", dest="serial_port", default="COM4",
                        help="Serial port to connect to (default: COM4)")
    parser.add_argument("-l", "--log", dest="log", default=None,
                        help="Optional path to a log file. If provided, all sent/received frames are appended to this file.")
    args = parser.parse_args()

    root = ttk.Window(themename="flatly")
    # Use provided serial port or default to COM4
    app = MotorHeadUI(root, serial_port=args.serial_port, log_path=args.log)
    
    def on_closing():
        if app.serial_connected:
            app.serial_reader.disconnect()
        root.destroy()
    
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()
