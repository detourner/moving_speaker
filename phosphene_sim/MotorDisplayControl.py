import tkinter as tk
import ttkbootstrap as ttk
from ttkbootstrap.constants import *


class MotorDisplayControl(ttk.Frame):
    def __init__(self, parent, title_text="moteur A", initial_value=0.0, *args, **kwargs):
        super().__init__(parent, *args, **kwargs) 

        self.value_var = tk.DoubleVar(value=initial_value)
        self.moving_var = tk.BooleanVar(value=False)  # False = gris, True = rouge
        
        # Label titre
        self.frame = ttk.Labelframe(self, text=title_text, bootstyle="info")   
        self.frame.pack()
        
        # Container pour l'indicateur supérieur droit
        self.top_frame = ttk.Frame(self.frame)
        self.top_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Indicateur numérique en haut à droite
        self.status_label = ttk.Label(self.top_frame, text="0.00", font=("Arial", 10))
        self.status_label.pack(side=tk.RIGHT)
        
        # Rond coloré en dessous de l'indicateur
        self.canvas_status = tk.Canvas(self.frame, width=20, height=20, bg="white", highlightthickness=0)
        self.canvas_status.pack(side=tk.RIGHT, padx=5, pady=5)
        self.status_circle = self.canvas_status.create_oval(2, 2, 18, 18, fill="gray", outline="black")
        
        # Label valeur principale (grande) - centré verticalement
        self.value_label = ttk.Label(self.frame, text=f"{self.value_var.get():.2f}", font=("Arial", 32, "bold"),)
        self.value_label.pack(expand=True, pady=0, padx=50)

    def update_position(self, new_value):
        """Met à jour la valeur position avec 2 décimales"""
        self.value_var.set(new_value)
        self.value_label.config(text=f"{self.value_var.get():06.2f}")

    def update_speed(self, speed_value):
        """Met à jour l'indicateur de vitesse"""
        self.status_label.config(text=f"{speed_value:.2f}")

    def update_moving(self, status_value):
        """Met à jour l'indicateur de statut (rouge si True, gris si False)"""
        self.moving_var.set(status_value)
        color = "red" if status_value else "gray"
        self.canvas_status.itemconfig(self.status_circle, fill=color)
        
