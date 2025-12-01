import tkinter as tk
import ttkbootstrap as ttk
from ttkbootstrap.constants import *


class ConsigneControl(ttk.Frame):
    """
    Widget composite aligné sur une seule ligne :
    [ Label | Entry | ---- Scale ---- ] + option rotation
    """

    SCALE_MIN_WIDTH = 200  # largeur fixe du scale

    def __init__(self, master, label="Consigne", min_val=0.0, max_val=100.0,
                 initial=0.0, step=0.01, with_rotation=False, *args, **kwargs):
        super().__init__(master, *args, **kwargs)

        self.min_val = min_val
        self.max_val = max_val
        self.step = step

        # Variable partagée pour valeur
        self.var = tk.DoubleVar(value=initial)

        # Label
        self.lbl = ttk.Label(self, text=label, width=12)
        self.lbl.grid(row=0, column=0, padx=5, pady=5, sticky=W)

        # Entry
        self.entry = ttk.Entry(self, textvariable=self.var, width=10)
        self.entry.grid(row=0, column=1, padx=5, pady=5, sticky=W)

        # Scale
        self.scale = ttk.Scale(
            self,
            from_=self.min_val,
            to=self.max_val,
            orient=HORIZONTAL,
            variable=self.var,
            command=self._on_scale_moved
        )
        self.scale.grid(row=0, column=2, padx=5, pady=5, sticky=EW)

        # Column configure pour garantir largeur constante du Scale
        self.columnconfigure(2, weight=1, minsize=self.SCALE_MIN_WIDTH)

        # Bind pour synchroniser l’entrée clavier
        self.entry.bind("<KeyRelease>", self._on_entry_typed)

        # --- Option de rotation ---
        self.with_rotation = with_rotation
        if self.with_rotation:
            self.rot_var = tk.IntVar(value=2)  # par défaut = plus court
            self.rb_cw = ttk.Radiobutton(self, text="Shortest", variable=self.rot_var, value=0)
            self.rb_ccw = ttk.Radiobutton(self, text="CC", variable=self.rot_var, value=1)
            self.rb_shortest = ttk.Radiobutton(self, text="CCW", variable=self.rot_var, value=2)
            self.rb_cw.grid(row=0, column=3, padx=5, pady=5)
            self.rb_ccw.grid(row=0, column=4, padx=5, pady=5)
            self.rb_shortest.grid(row=0, column=5, padx=5, pady=5)

    # ---------- Synchronisation ----------
    def _on_scale_moved(self, event=None):
        """Déplacement du scale."""
        value = round(self.scale.get(), 2)
        value = round(self._snap_to_step(value), 2)
        self.var.set(value)

    def _on_entry_typed(self, event=None):
        """Saisie au clavier."""
        try:
            value = float(self.entry.get())
            value = self._snap_to_step(value)
            value = max(min(value, self.max_val), self.min_val)
            self.var.set(value)
        except ValueError:
            pass

    def _snap_to_step(self, value):
        """Force la valeur à être un multiple de self.step"""
        steps = round(value / self.step)
        return steps * self.step

    # ---------- API externe ----------
    def get(self):
        return round(self.var.get(), 2)

    def set(self, value):
        value = max(min(value, self.max_val), self.min_val)
        self.var.set(value)

    def getDirection(self):
        """Retourne 0=CW, 1=CCW, 2=Shortest (si rotation activée)"""
        if self.with_rotation:
            return self.rot_var.get()
        return None

    def setDirection(self, val):
        """Définir le sens de rotation"""
        if self.with_rotation and val in (0, 1, 2):
            self.rot_var.set(val)
