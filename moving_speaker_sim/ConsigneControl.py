import tkinter as tk
import ttkbootstrap as ttk
from ttkbootstrap.constants import *


class ConsigneControl(ttk.Frame):
    """
    Composite widget laid out on a single row:
    [ Label | Entry | ---- Scale ---- ] + optional rotation controls
    """

    SCALE_MIN_WIDTH = 200  # fixed minimum width for the scale widget

    def __init__(self, master, label="Consigne", min_val=0.0, max_val=100.0,
                 initial=0.0, step=0.01, with_rotation=False, *args, **kwargs):
        super().__init__(master, *args, **kwargs)

        self.min_val = min_val
        self.max_val = max_val
        self.step = step

        # Shared variable for the current value
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

        # Configure column to guarantee the scale keeps a minimum width
        self.columnconfigure(2, weight=1, minsize=self.SCALE_MIN_WIDTH)

        # Bind entry key events to synchronize typed value with the scale
        self.entry.bind("<KeyRelease>", self._on_entry_typed)

        # --- Option de rotation ---
        self.with_rotation = with_rotation
        if self.with_rotation:
            self.rot_var = tk.IntVar(value=2)  # default = shortest
            # Radio buttons: 0=CW, 1=CCW, 2=Shortest
            self.rb_cw = ttk.Radiobutton(self, text="CW", variable=self.rot_var, value=0)
            self.rb_ccw = ttk.Radiobutton(self, text="CCW", variable=self.rot_var, value=1)
            self.rb_shortest = ttk.Radiobutton(self, text="Shortest", variable=self.rot_var, value=2)
            self.rb_cw.grid(row=0, column=3, padx=5, pady=5)
            self.rb_ccw.grid(row=0, column=4, padx=5, pady=5)
            self.rb_shortest.grid(row=0, column=5, padx=5, pady=5)

    # ---------- Synchronisation ----------
    def _on_scale_moved(self, event=None):
        """Scale moved: snap value to step and update variable."""
        value = round(self.scale.get(), 2)
        value = round(self._snap_to_step(value), 2)
        self.var.set(value)

    def _on_entry_typed(self, event=None):
        """Keyboard entry: validate, clamp and snap to step."""
        try:
            value = float(self.entry.get())
            value = self._snap_to_step(value)
            value = max(min(value, self.max_val), self.min_val)
            self.var.set(value)
        except ValueError:
            pass

    def _snap_to_step(self, value):
        """Force the value to be a multiple of self.step."""
        steps = round(value / self.step)
        return steps * self.step

    # ---------- API externe ----------
    def get(self):
        return round(self.var.get(), 2)

    def set(self, value):
        value = max(min(value, self.max_val), self.min_val)
        self.var.set(value)

    def getDirection(self):
        """Return direction mode: 0=CW, 1=CCW, 2=Shortest (if rotation enabled)."""
        if self.with_rotation:
            return self.rot_var.get()
        return None

    def setDirection(self, val):
        """Set the rotation direction mode."""
        if self.with_rotation and val in (0, 1, 2):
            self.rot_var.set(val)
