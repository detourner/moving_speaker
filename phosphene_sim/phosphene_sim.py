import tkinter as tk
import ttkbootstrap as ttk
from ttkbootstrap.constants import *
import random
import serial
import threading
import queue
import ConsigneControl as ConsigneControl
import MotorDisplayControl as MotorDisplayControl


class SerialReader:
    """Gère la communication série avec Arduino"""
    def __init__(self, port='COM3', baudrate=115200, data_queue=None):
        self.port = port
        self.baudrate = baudrate
        self.data_queue = data_queue if data_queue else queue.Queue()
        self.running = False
        self.serial_connection = None
        
    def connect(self):
        """Établit la connexion série"""
        try:
            self.serial_connection = serial.Serial(self.port, self.baudrate, timeout=1)
            self.running = True
            return True
        except serial.SerialException as e:
            print(f"Erreur connexion série: {e}")
            return False
    
    def disconnect(self):
        """Ferme la connexion série"""
        self.running = False
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
    
    def write_command(self, command):
        """Envoie une commande à Arduino"""
        if self.serial_connection and self.serial_connection.is_open:
            try:
                self.serial_connection.write((command + '\n').encode('utf-8'))
                return True
            except Exception as e:
                print(f"Erreur envoi commande: {e}")
                return False
        return False
    
    def read_loop(self):
        """Boucle de lecture (à exécuter dans un thread)"""
        while self.running:
            try:
                if self.serial_connection.in_waiting > 0:
                    line = self.serial_connection.readline().decode('utf-8').strip()
                    #print(f"Reçu série: {line}")
                    if line.startswith('P:'):
                        # Format: P:isMoving_a,position_a,isMoving_b,position_b
                        data = line[2:]  # Enlever "P:"
                        parts = data.split(',')
                        if len(parts) == 6:
                            try:
                                motor_data = {
                                    'moving_a': bool(int(parts[0])),
                                    'position_a': float(parts[1]),
                                    'speed_a': float(parts[2]),
                                    'moving_b': bool(int(parts[3])),
                                    'position_b': float(parts[4]),
                                    'speed_b': float(parts[5])
                                }
                                #print(f"Données moteur: {motor_data}")
                                self.data_queue.put(motor_data)

                            except ValueError:
                                print(f"Erreur parsing: {line}")
            except Exception as e:
                print(f"Erreur lecture série: {e}")


class MotorHeadUI:
    def __init__(self, root, serial_port='COM3'):
        self.root = root
        self.root.title("Motor Head Control")
        self.style = ttk.Style("flatly")

        # Valeurs simulées/reçues
        self.position_a = 0
        self.position_b = 0
        self.moving_a = False
        self.moving_b = False
        self.speed_a = 0
        self.speed_b = 0

        # Initialisation liaison série
        self.data_queue = queue.Queue()
        self.serial_reader = SerialReader(port=serial_port, data_queue=self.data_queue, baudrate=115200)
        self.serial_connected = self.serial_reader.connect()
        
        if self.serial_connected:
            # Démarrer le thread de lecture série
            self.serial_thread = threading.Thread(target=self.serial_reader.read_loop, daemon=True)
            self.serial_thread.start()
            print(f"Connexion série établie sur {serial_port}")
        else:
            print(f"Impossible de se connecter à {serial_port}")

        # Frame principale
        main = ttk.Frame(root, padding=20)
        main.pack(fill=BOTH, expand=YES)

        # --- Meters ---------------------------------------------------------
        meters_frame = ttk.Frame(main)
        meters_frame.pack(fill=X, pady=10)

        self.motorADisp = MotorDisplayControl.MotorDisplayControl(meters_frame, title_text="Motor A Position [°]", initial_value=0.0)
        self.motorADisp.pack(side=LEFT, padx=20)
        self.motorBDisp = MotorDisplayControl.MotorDisplayControl(meters_frame, title_text="Motor B Position [°]", initial_value=0.0)
        self.motorBDisp.pack(side=LEFT, padx=20)
        

       

        # --- Sliders pour les consignes ------------------------------------
        sliders_frame = ttk.Labelframe(main, text="Consignes")
        sliders_frame.pack(fill=X, pady=10, padx=10)

        self.motA_target = ConsigneControl.ConsigneControl(sliders_frame, label="mot A Position [°]", min_val=-90, max_val=90, initial=0)
        self.motA_target.pack(fill="x", padx=10, pady=10)

        self.motA_speed = ConsigneControl.ConsigneControl(sliders_frame, label="mot A Vitesse", min_val=0.01, max_val=45, initial=17, step = 0.01)
        self.motA_speed.pack(fill="x", padx=10, pady=10)

        self.motA_accel = ConsigneControl.ConsigneControl(sliders_frame, label="mot A Accell", min_val=1.1, max_val=113, initial=50, step = 0.1)
        self.motA_accel.pack(fill="x", padx=10, pady=10)

        self.motB_target = ConsigneControl.ConsigneControl(sliders_frame, label="mot B Position [°]", min_val=0, max_val=359.99, initial=0, with_rotation=True)
        self.motB_target.pack(fill="x", padx=10, pady=10)

        self.motB_speed = ConsigneControl.ConsigneControl(sliders_frame, label="mot B Vitesse", min_val=0.01, max_val=45, initial=17, step = 0.01)
        self.motB_speed.pack(fill="x", padx=10, pady=10)

        self.motB_accel = ConsigneControl.ConsigneControl(sliders_frame, label="mot A Accell", min_val=1.1, max_val=113, initial=50, step = 0.1)
        self.motB_accel.pack(fill="x", padx=10, pady=10)


       
       

        # --- Boutons de contrôle -------------------------------------------------
        buttons_frame = ttk.Frame(sliders_frame)
        buttons_frame.pack(fill=X, pady=10)

        lbl_target_frame = ttk.Labelframe(buttons_frame, text="Statut des consignes")
        self.lbl_target = ttk.Label(lbl_target_frame, text="Aucune consigne envoyée", foreground="black")
        self.lbl_target.pack(padx=5, pady=5)
        
        ttk.Button(buttons_frame, text="Envoyer consignes", bootstyle=SUCCESS, command=self.send_target).pack(side=LEFT, padx=5)
       
        # Mise à jour régulière (simulation)
        self.update_values()

    def send_target(self):
        """Envoie les consignes vers Arduino"""
        mAt = float(self.motA_target.get())
        mAs = float(self.motA_speed.get())
        mAa = float(self.motA_accel.get())
        mBt = float(self.motB_target.get())
        mBs = float(self.motB_speed.get())
        mBd = int(self.motB_target.getDirection())  # direction de rotation
        mBa = float(self.motB_accel.get())
       
        
        # Format: tilt_target,tilt_speed,pan_target,pan_speed,homing
        command = f"{mAt},{mAs},{mAa},{mBt},{mBs},{mBd},{mBa}"
        print(f"Envoi commande: {command}")
        if self.serial_connected:
            if self.serial_reader.write_command(command):
                self.lbl_target.config(
                    text=f"✓ Consignes envoyées → MotA: {mAt:.2f}°@{mAs:.2f}/{mBa:.2f} | MotB: {mBt:.12f}°@{mBs:.2f} Dir:{mBd} / {mBa:.2f}",
                    foreground="green"
                )
            else:
                self.lbl_target.config(text="✗ Erreur d'envoi", foreground="red")
        else:
            self.lbl_target.config(text="✗ Non connecté à Arduino", foreground="red")
    
   

    def update_values(self):
        """Lecture des données capteur (série ou simulation)"""
        # Lire les données de la queue série
        try:
            while True:
                motor_data = self.data_queue.get_nowait()
                self.moving_a = motor_data['moving_a']
                self.moving_b = motor_data['moving_b']
                self.position_a = motor_data['position_a']
                self.position_b = motor_data['position_b']
                self.speed_a = motor_data['speed_a']
                self.speed_b = motor_data['speed_b']
                print(f"Reçu → motA: {self.position_a}°, motB: {self.position_b}°")
        except queue.Empty:
            pass
        
        # Mise à jour des meters
        self.motorADisp.update_position(self.position_a)
        self.motorBDisp.update_position(self.position_b)
        self.motorADisp.update_speed(self.speed_a)  # Vitesse non implémentée pour l'instant
        self.motorBDisp.update_speed(self.speed_b)  # Vitesse non implémentée pour
        self.motorADisp.update_moving(self.moving_a)
        self.motorBDisp.update_moving(self.moving_b)

        # Reboucle dans 200ms
        self.root.after(200, self.update_values)

if __name__ == "__main__":
    root = ttk.Window(themename="flatly")
    # Adapter le port COM selon votre configuration (COM3, COM4, etc.)
    app = MotorHeadUI(root, serial_port='COM3')
    
    def on_closing():
        if app.serial_connected:
            app.serial_reader.disconnect()
        root.destroy()
    
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()
