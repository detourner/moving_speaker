import sys
import math
import serial
import threading
import queue
import argparse
import datetime

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout,
    QLabel, QSlider, QPushButton, QGroupBox, QRadioButton, QButtonGroup,
    QCheckBox, QScrollArea, QSpinBox, QSplitter, QTabWidget, QDoubleSpinBox, QSizePolicy, QStatusBar,
)
from PyQt6.QtCore import Qt, QTimer, QPointF, pyqtSignal
from PyQt6.QtGui import QPainter, QPen, QBrush, QColor, QFont, QFontMetrics


# ── Serial reader ─────────────────────────────────────────────────────────────

class SerialReader:
    """Handles serial communication with the Arduino and optional logging."""

    def __init__(self, port="COM4", baudrate=115200, data_queue=None, log_path=None):
        self.port = port
        self.baudrate = baudrate
        self.data_queue = data_queue if data_queue else queue.Queue()
        self.running = False
        self.serial_connection = None
        self.log_path = log_path
        self.log_fh = None
        if self.log_path:
            try:
                self.log_fh = open(self.log_path, "a", encoding="utf-8")
            except Exception as e:
                print(f"Unable to open log file '{self.log_path}': {e}")

    def connect(self):
        try:
            self.serial_connection = serial.Serial(self.port, self.baudrate, timeout=1)
            self.running = True
            return True
        except serial.SerialException as e:
            print(f"Serial connection error: {e}")
            return False

    def disconnect(self):
        self.running = False
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
        if self.log_fh:
            try:
                self.log_fh.close()
            except Exception:
                pass

    def write_command(self, command):
        if self.serial_connection and self.serial_connection.is_open:
            try:
                self.serial_connection.write((command + "\n").encode("utf-8"))
                if self.log_fh:
                    ts = datetime.datetime.now().isoformat(sep=" ", timespec="milliseconds")
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
        while self.running:
            try:
                if self.serial_connection.in_waiting > 0:
                    line = self.serial_connection.readline().decode("utf-8").strip()
                    if not line:
                        continue
                    if self.log_fh:
                        ts = datetime.datetime.now().isoformat(sep=" ", timespec="milliseconds")
                        try:
                            self.log_fh.write(f"{ts} Serial <- {line}\n")
                            self.log_fh.flush()
                        except Exception:
                            pass
                    if line.startswith("P:"):
                        parts = line[2:].split(",")
                        if len(parts) == 12:
                            try:
                                self.data_queue.put({
                                    "moving_a":   bool(int(parts[0])),
                                    "position_a": float(parts[1]),
                                    "speed_a":    float(parts[2]),
                                    "moving_b":   bool(int(parts[3])),
                                    "position_b": float(parts[4]),
                                    "speed_b":    float(parts[5]),
                                    "moving_c":   bool(int(parts[6])),
                                    "position_c": float(parts[7]),
                                    "speed_c":    float(parts[8]),
                                    "moving_d":   bool(int(parts[9])),
                                    "position_d": float(parts[10]),
                                    "speed_d":    float(parts[11]),
                                })
                            except ValueError:
                                print(f"Parsing error: {line}")
                    else:
                        print(f"Serial <- {line}")
            except Exception as e:
                print(f"Serial read error: {e}")


# ── Pan trajectory helper ─────────────────────────────────────────────────────

def compute_target_pan(current_pan_absolute, requested_display_pan, mode):
    """
    Return the absolute pan target for a given display-pan request.

    requested_display_pan : angle in [0, +360]
    mode                  : "short" | "cw" | "ccw"
    """
    base = math.floor(current_pan_absolute / 360) * 360
    candidates = [
        base + requested_display_pan,
        base + requested_display_pan + 360,
        base + requested_display_pan - 360,
    ]
    if mode == "short":
        return min(candidates, key=lambda c: abs(c - current_pan_absolute))
    if mode == "cw":
        valid = [c for c in candidates if c >= current_pan_absolute - 1e-6]
        return min(valid) if valid else candidates[0] + 360
    if mode == "ccw":
        valid = [c for c in candidates if c <= current_pan_absolute + 1e-6]
        return max(valid) if valid else candidates[0] - 360
    return candidates[0]


def _normalize_display_pan(absolute):
    """Map any absolute pan angle to [0, +360)."""
    return absolute % 360


# ── Pan/Tilt interactive widget ───────────────────────────────────────────────

class PanTiltWidget(QWidget):
    """
    Square plot showing:
      * blue dot  — current position (pan = Motor B, tilt = Motor A)
      * red dot   — target position (draggable by mouse)
      * dashed line between them, colour-coded by angular error
    """

    target_changed = pyqtSignal(float, float)   # (display_pan, tilt)

    _MARGIN = 55
    _DOT_R  = 8

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(280, 280)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        self._current_pan  = 180.0
        self._current_tilt = 0.0
        self._target_pan   = 180.0
        self._target_tilt  = 0.0
        self._dragging     = False

    def hasHeightForWidth(self):
        return True

    def heightForWidth(self, w):
        return w

    # ── Public API ────────────────────────────────────────────────────────────

    def set_current(self, pan_abs, tilt):
        self._current_pan  = pan_abs % 360
        self._current_tilt = max(-90.0, min(90.0, tilt))
        self.update()

    def set_target_display(self, display_pan, tilt):
        self._target_pan  = max(0.0, min(360.0, display_pan))
        self._target_tilt = max(-90.0,  min(90.0,  tilt))
        self.update()

    # ── Coordinate helpers ────────────────────────────────────────────────────

    def _to_px(self, pan, tilt):
        m = self._MARGIN
        w = self.width()  - 2 * m
        h = self.height() - 2 * m
        return QPointF(
            m + pan / 360.0 * w,
            m + (90.0 - tilt) / 180.0 * h,
        )

    def _to_deg(self, x, y):
        m = self._MARGIN
        w = self.width()  - 2 * m
        h = self.height() - 2 * m
        pan  = (x - m) / w * 360.0
        tilt = 90.0 - (y - m) / h * 180.0
        return max(0.0, min(360.0, pan)), max(-90.0, min(90.0, tilt))

    # ── Paint ─────────────────────────────────────────────────────────────────

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        m = self._MARGIN
        w = self.width()  - 2 * m
        h = self.height() - 2 * m

        # Backgrounds
        p.fillRect(self.rect(), QColor("#12121f"))
        p.fillRect(m, m, w, h, QColor("#16213e"))

        # Dotted grid
        grid_pen = QPen(QColor("#2a2a5a"), 1, Qt.PenStyle.DotLine)
        p.setPen(grid_pen)
        for pan in range(0, 361, 30):
            x = int(m + pan / 360 * w)
            p.drawLine(x, m, x, m + h)
        for tilt in range(-90, 91, 30):
            y = int(m + (90 - tilt) / 180 * h)
            p.drawLine(m, y, m + w, y)

        # Centre axis at 180° pan (slightly brighter)
        p.setPen(QPen(QColor("#3a3a7a"), 1))
        p.drawLine(int(m + 0.5 * w), m, int(m + 0.5 * w), m + h)
        p.drawLine(m, int(m + 0.5 * h), m + w, int(m + 0.5 * h))

        # Border
        p.setPen(QPen(QColor("#4444aa"), 2))
        p.drawRect(m, m, w, h)

        # Tick labels
        font_tick = QFont("Courier", 8)
        p.setFont(font_tick)
        p.setPen(QColor("#7777aa"))
        fm = QFontMetrics(font_tick)
        for pan in range(0, 361, 60):
            x   = int(m + pan / 360 * w)
            txt = f"{pan}\u00b0"
            p.drawText(x - fm.horizontalAdvance(txt) // 2, m + h + 16, txt)
        for tilt in range(-90, 91, 30):
            y   = int(m + (90 - tilt) / 180 * h)
            txt = f"{tilt:+d}\u00b0"
            p.drawText(2, y + fm.height() // 2 - 2, txt)

        # Axis titles
        font_title = QFont("Courier", 9, QFont.Weight.Bold)
        p.setFont(font_title)
        p.setPen(QColor("#aaaadd"))
        fm_t = QFontMetrics(font_title)
        pan_title = "PAN"
        p.drawText(m + (w - fm_t.horizontalAdvance(pan_title)) // 2, m + h + 34, pan_title)
        p.save()
        p.translate(m - 40, m + h // 2)
        p.rotate(-90)
        tilt_title = "TILT"
        p.drawText(-fm_t.horizontalAdvance(tilt_title) // 2, 0, tilt_title)
        p.restore()

        # Error line (colour-coded)
        curr_pt = self._to_px(self._current_pan, self._current_tilt)
        tgt_pt  = self._to_px(self._target_pan,  self._target_tilt)
        error = math.hypot(
            self._target_pan  - self._current_pan,
            (self._target_tilt - self._current_tilt) * 2,
        )
        if error < 10:
            line_color = QColor("#00cc44")
        elif error < 40:
            line_color = QColor("#ff8800")
        else:
            line_color = QColor("#ff2222")
        p.setPen(QPen(line_color, 2, Qt.PenStyle.DashLine))
        p.drawLine(curr_pt.toPoint(), tgt_pt.toPoint())

        # Current position — blue dot
        p.setPen(QPen(QColor("#0055ff"), 2))
        p.setBrush(QBrush(QColor("#3377ff")))
        p.drawEllipse(curr_pt, self._DOT_R, self._DOT_R)

        # Target position — red dot
        p.setPen(QPen(QColor("#cc0000"), 2))
        p.setBrush(QBrush(QColor("#ff3333")))
        p.drawEllipse(tgt_pt, self._DOT_R, self._DOT_R)

        p.end()

    # ── Mouse ─────────────────────────────────────────────────────────────────

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            tgt = self._to_px(self._target_pan, self._target_tilt)
            pos = event.position()
            if math.hypot(pos.x() - tgt.x(), pos.y() - tgt.y()) <= self._DOT_R + 6:
                self._dragging = True

    def mouseMoveEvent(self, event):
        if self._dragging:
            pan, tilt = self._to_deg(event.position().x(), event.position().y())
            self._target_pan  = pan
            self._target_tilt = tilt
            self.update()
            self.target_changed.emit(pan, tilt)

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self._dragging = False


# ── Reusable sub-widgets ──────────────────────────────────────────────────────

class MotorSlider(QWidget):
    """Label + QDoubleSpinBox + QSlider — replaces ConsigneControl."""

    value_changed = pyqtSignal(float)

    def __init__(self, label, min_val, max_val, initial, step=0.01, parent=None):
        super().__init__(parent)
        self._min  = min_val
        self._max  = max_val
        self._step = step
        self._sync = False

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 2, 0, 2)
        layout.setSpacing(6)

        lbl = QLabel(label)
        lbl.setFixedWidth(90)
        lbl.setStyleSheet("color:#cccccc;font-family:Courier;font-size:11px;")
        layout.addWidget(lbl)

        self._spin = QDoubleSpinBox()
        self._spin.setRange(min_val, max_val)
        self._spin.setSingleStep(step)
        self._spin.setDecimals(2)
        self._spin.setValue(initial)
        self._spin.setFixedWidth(90)
        self._spin.setStyleSheet(
            "QDoubleSpinBox{background:#1e1e2e;color:#eeeeee;"
            "border:1px solid #4444aa;border-radius:3px;"
            "font-family:Courier;font-size:11px;padding:2px;}"
        )
        layout.addWidget(self._spin)

        _SCALE = 1000
        self._slider = QSlider(Qt.Orientation.Horizontal)
        self._slider.setRange(int(min_val * _SCALE), int(max_val * _SCALE))
        self._slider.setValue(int(initial * _SCALE))
        self._slider.setSingleStep(max(1, int(step * _SCALE)))
        self._slider.setStyleSheet(
            "QSlider::groove:horizontal{height:4px;background:#2a2a4e;border-radius:2px;}"
            "QSlider::handle:horizontal{width:12px;height:12px;margin:-4px 0;"
            "background:#5555ff;border-radius:6px;}"
            "QSlider::sub-page:horizontal{background:#4444aa;border-radius:2px;}"
        )
        layout.addWidget(self._slider)

        self._SCALE = _SCALE
        self._spin.valueChanged.connect(self._spin_changed)
        self._slider.valueChanged.connect(self._slider_changed)

    def _spin_changed(self, val):
        if self._sync:
            return
        self._sync = True
        self._slider.setValue(int(val * self._SCALE))
        self._sync = False
        self.value_changed.emit(val)

    def _slider_changed(self, raw):
        if self._sync:
            return
        val = raw / self._SCALE
        self._sync = True
        self._spin.setValue(val)
        self._sync = False
        self.value_changed.emit(val)

    def get(self):
        return self._spin.value()

    def set(self, value):
        self._spin.setValue(max(self._min, min(self._max, value)))


class MotorDisplay(QWidget):
    """Position / speed / moving indicator — replaces MotorDisplayControl."""

    def __init__(self, title, parent=None):
        super().__init__(parent)
        self.setStyleSheet(
            "background:#1a1a2e;border:1px solid #333355;border-radius:6px;"
        )
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(2)

        title_lbl = QLabel(title)
        title_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title_lbl.setStyleSheet(
            "color:#8888cc;font-family:Courier;font-size:10px;"
            "font-weight:bold;border:none;"
        )
        layout.addWidget(title_lbl)

        self._pos_lbl = QLabel("+000.00")
        self._pos_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._pos_lbl.setStyleSheet(
            "color:#eeeeee;font-family:Courier;font-size:26px;"
            "font-weight:bold;border:none;"
        )
        layout.addWidget(self._pos_lbl)

        row = QHBoxLayout()
        self._spd_lbl = QLabel("spd: 0.00")
        self._spd_lbl.setStyleSheet(
            "color:#9999bb;font-family:Courier;font-size:10px;border:none;"
        )
        row.addWidget(self._spd_lbl)

        self._moving_lbl = QLabel("*")
        self._moving_lbl.setAlignment(Qt.AlignmentFlag.AlignRight)
        self._moving_lbl.setStyleSheet("color:#333344;font-size:14px;border:none;")
        row.addWidget(self._moving_lbl)
        layout.addLayout(row)

    def update_position(self, val):
        self._pos_lbl.setText(f"{val:+08.2f}")

    def update_speed(self, val):
        self._spd_lbl.setText(f"spd: {val:.2f}")

    def update_moving(self, moving):
        self._moving_lbl.setStyleSheet(
            "color:#ff3333;font-size:14px;border:none;" if moving
            else "color:#333344;font-size:14px;border:none;"
        )


# ── Dark theme stylesheet ─────────────────────────────────────────────────────

_DARK = """
QMainWindow,QWidget{background:#12121f;color:#ccccee;}
QTabWidget::pane{border:1px solid #333355;background:#12121f;}
QTabBar::tab{background:#1a1a2e;color:#8888cc;padding:6px 18px;
    border:1px solid #333355;border-bottom:none;
    font-family:Courier;font-size:11px;}
QTabBar::tab:selected{background:#252540;color:#ffffff;border-top:2px solid #5555ff;}
QGroupBox{border:1px solid #333355;border-radius:6px;margin-top:10px;
    font-family:Courier;font-size:11px;color:#8888cc;}
QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 4px;}
QPushButton{background:#24245a;color:#aaaaee;border:1px solid #4444aa;
    border-radius:4px;padding:6px 14px;font-family:Courier;font-size:11px;}
QPushButton:hover{background:#343480;color:#ffffff;}
QPushButton:pressed{background:#14143a;}
QPushButton#send_btn{background:#1a401a;color:#88ee88;border-color:#44aa44;font-weight:bold;}
QPushButton#send_btn:hover{background:#286028;}
QLabel{color:#ccccee;}
QScrollArea{border:none;}
QScrollBar:vertical{background:#1a1a2e;width:8px;}
QScrollBar::handle:vertical{background:#3a3a7a;border-radius:4px;}
QRadioButton{color:#ccccee;font-family:Courier;font-size:11px;}
QRadioButton::indicator{width:14px;height:14px;}
QRadioButton::indicator:checked{background:#5555ff;border:2px solid #aaaaff;border-radius:7px;}
QRadioButton::indicator:unchecked{background:#1e1e2e;border:2px solid #4444aa;border-radius:7px;}
QStatusBar{background:#1a1a2e;color:#666688;font-family:Courier;font-size:10px;}
"""


# ── Main window ───────────────────────────────────────────────────────────────

class MotorHeadUI(QMainWindow):

    def __init__(self, serial_port="COM4", log_path=None):
        super().__init__()
        self.setWindowTitle("Moving Speaker Sim V2.0")
        self.resize(1150, 800)

        # Motor state (updated from serial)
        self.position_a = 0.0
        self.position_b = 0.0
        self.position_c = 0.0
        self.position_d = 0.0
        self.speed_a = self.speed_b = self.speed_c = self.speed_d = 0.0
        self.moving_a = self.moving_b = self.moving_c = self.moving_d = False

        # Multi-turn absolute tracking for Motor B (Pan) and D
        self._pan_b_abs  = 0.0
        self._pan_b_prev = 0.0
        self._tgt_b_abs  = 180.0   # matches widget initial _target_pan = 180°
        self._pan_d_abs  = 0.0
        self._pan_d_prev = 0.0
        self._tgt_d_abs  = 180.0   # matches widget initial _target_pan = 180°

        # Serial
        self.data_queue = queue.Queue()
        self.serial_reader = SerialReader(
            port=serial_port, data_queue=self.data_queue,
            baudrate=115200, log_path=log_path,
        )
        self.serial_connected = self.serial_reader.connect()
        if self.serial_connected:
            threading.Thread(target=self.serial_reader.read_loop, daemon=True).start()
            print(f"Serial connected on {serial_port}")
        else:
            print(f"Unable to connect to {serial_port}")

        self._build_ui()

        sb = QStatusBar()
        self.setStatusBar(sb)
        sb.showMessage(
            f"Connected: {serial_port}" if self.serial_connected
            else f"Not connected ({serial_port})"
        )

        self._timer = QTimer()
        self._timer.timeout.connect(self._update_values)
        self._timer.start(50)

    # ── UI builder ────────────────────────────────────────────────────────────

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(8, 8, 8, 8)
        tabs = QTabWidget()
        root.addWidget(tabs)

        pt_tab = QWidget()
        pt_outer = QVBoxLayout(pt_tab)
        pt_outer.setContentsMargins(4, 4, 4, 4)
        pt_outer.setSpacing(4)

        # Top bar: sync checkbox + shared send button
        top_bar = QWidget()
        tbl = QHBoxLayout(top_bar)
        tbl.setContentsMargins(4, 2, 4, 2)
        tbl.setSpacing(10)
        self._sync_cb = QCheckBox("Link Pan/Tilt 2 to Pan/Tilt 1  (mirror mode)")
        self._sync_cb.setStyleSheet(
            "font-family:Courier;font-size:11px;color:#ffcc44;padding:2px;"
        )
        self._sync_cb.toggled.connect(self._on_sync_toggled)
        tbl.addWidget(self._sync_cb)
        tbl.addStretch()

        # Auto-send controls
        self._auto_send_cb = QCheckBox("Continuous send")
        self._auto_send_cb.setStyleSheet(
            "font-family:Courier;font-size:11px;color:#88eeaa;padding:2px;"
        )
        tbl.addWidget(self._auto_send_cb)
        self._auto_interval_spin = QSpinBox()
        self._auto_interval_spin.setRange(1, 5000)
        self._auto_interval_spin.setValue(100)
        self._auto_interval_spin.setSuffix(" ms")
        self._auto_interval_spin.setFixedWidth(90)
        self._auto_interval_spin.setStyleSheet(
            "QSpinBox{background:#1e1e2e;color:#eeeeee;"
            "border:1px solid #4444aa;border-radius:3px;"
            "font-family:Courier;font-size:11px;padding:2px;}"
        )
        tbl.addWidget(self._auto_interval_spin)

        self._auto_send_timer = QTimer()
        self._auto_send_timer.timeout.connect(self._send_all_pantilt)
        self._auto_send_cb.toggled.connect(self._on_auto_send_toggled)
        self._auto_interval_spin.valueChanged.connect(
            lambda v: self._auto_send_timer.setInterval(v)
            if self._auto_send_timer.isActive() else None
        )

        shared_send = QPushButton("Send setpoints (4 motors)")
        shared_send.setObjectName("send_btn")
        shared_send.clicked.connect(self._send_all_pantilt)
        tbl.addWidget(shared_send)
        self._shared_status = QLabel("")
        self._shared_status.setStyleSheet("font-family:Courier;font-size:10px;")
        tbl.addWidget(self._shared_status)
        pt_outer.addWidget(top_bar)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.addWidget(self._make_pantilt_panel(1))
        splitter.addWidget(self._make_pantilt_panel(2))
        splitter.setSizes([1, 1])
        pt_outer.addWidget(splitter)
        tabs.addTab(pt_tab,                  "Pan/Tilt")
        tabs.addTab(self._make_motors_tab(), "Motors")

    # ── Pan/Tilt panel (shared factory for pair 1=A+B and 2=C+D) ──────────────

    def _make_pantilt_panel(self, n):
        tab = QWidget()
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(6, 6, 6, 6)
        outer.setSpacing(6)

        # Top — square interactive plot
        pt_widget = PanTiltWidget()
        pt_widget.target_changed.connect(lambda dp, t, _n=n: self._on_pt_dragged(_n, dp, t))
        outer.addWidget(pt_widget)
        setattr(self, f"_pt_{n}_widget", pt_widget)

        # Reset button just below the graph
        reset_btn = QPushButton("Home  Pan=0\u00b0  Tilt=0\u00b0")
        reset_btn.clicked.connect(lambda _=False, _n=n: self._reset_target(_n))
        reset_btn.setStyleSheet(
            "QPushButton{background:#2a1a4a;color:#cc88ff;border:1px solid #7744aa;"
            "border-radius:4px;padding:3px 10px;font-family:Courier;font-size:10px;}"
            "QPushButton:hover{background:#3a2a5a;color:#ffffff;}"
        )
        outer.addWidget(reset_btn)
        bottom = QWidget()
        bottom.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        bl = QHBoxLayout(bottom)
        bl.setContentsMargins(0, 0, 0, 0)
        bl.setSpacing(6)

        # Column 1: current + target position labels
        pos_col = QWidget()
        pcl = QVBoxLayout(pos_col)
        pcl.setContentsMargins(0, 0, 0, 0)
        pcl.setSpacing(4)

        curr_box = QGroupBox("Current position")
        cl = QVBoxLayout(curr_box)
        lbl_cur_pan     = QLabel("Pan  :    +0.0")
        lbl_cur_tilt    = QLabel("Tilt :    +0.0")
        lbl_cur_pan_abs = QLabel("Pan absolute :    +0")
        for lbl in (lbl_cur_pan, lbl_cur_tilt, lbl_cur_pan_abs):
            lbl.setStyleSheet("font-family:Courier;font-size:11px;color:#7799ff;")
            cl.addWidget(lbl)
        lbl_mov_tilt = QLabel("Tilt : stopped")
        lbl_mov_pan  = QLabel("Pan  : stopped")
        for lbl in (lbl_mov_tilt, lbl_mov_pan):
            lbl.setStyleSheet("font-family:Courier;font-size:10px;color:#444455;")
            cl.addWidget(lbl)
        setattr(self, f"_lbl_{n}_cur_pan",     lbl_cur_pan)
        setattr(self, f"_lbl_{n}_cur_tilt",    lbl_cur_tilt)
        setattr(self, f"_lbl_{n}_cur_pan_abs", lbl_cur_pan_abs)
        setattr(self, f"_lbl_{n}_mov_tilt",    lbl_mov_tilt)
        setattr(self, f"_lbl_{n}_mov_pan",     lbl_mov_pan)
        pcl.addWidget(curr_box)

        tgt_box = QGroupBox("Target position")
        tl = QVBoxLayout(tgt_box)
        lbl_tgt_pan     = QLabel("Pan  :    +0.0")
        lbl_tgt_tilt    = QLabel("Tilt :    +0.0")
        lbl_tgt_pan_abs = QLabel("Pan absolute :    +0")
        for lbl in (lbl_tgt_pan, lbl_tgt_tilt, lbl_tgt_pan_abs):
            lbl.setStyleSheet("font-family:Courier;font-size:11px;color:#ff7777;")
            tl.addWidget(lbl)
        setattr(self, f"_lbl_{n}_tgt_pan",     lbl_tgt_pan)
        setattr(self, f"_lbl_{n}_tgt_tilt",    lbl_tgt_tilt)
        setattr(self, f"_lbl_{n}_tgt_pan_abs", lbl_tgt_pan_abs)
        pcl.addWidget(tgt_box)
        pcl.addStretch()
        bl.addWidget(pos_col)

        # Column 2: trajectory mode
        traj_box = QGroupBox("Trajectory mode")
        trj = QVBoxLayout(traj_box)
        rb_shortest = QRadioButton("Short")
        rb_cw       = QRadioButton("Cw")
        rb_ccw      = QRadioButton("Ccw")
        rb_shortest.setChecked(True)
        traj_grp = QButtonGroup()
        for i, rb in enumerate((rb_shortest, rb_cw, rb_ccw)):
            traj_grp.addButton(rb, i)
            trj.addWidget(rb)
        setattr(self, f"_traj_grp_{n}", traj_grp)

        # Column 3: speed sliders + send
        spd_box = QGroupBox("Speed parameters")
        sl = QVBoxLayout(spd_box)
        pt_tilt_spd = MotorSlider("Tilt speed",   0.01,  23, 17, 0.01)
        pt_tilt_acc = MotorSlider("Tilt accel",   1.1,  113, 50, 0.1)
        pt_pan_spd  = MotorSlider("Pan speed",    0.01,  46, 17, 0.01)
        pt_pan_acc  = MotorSlider("Pan accel",    1.1,  113, 50, 0.1)
        for w in (pt_tilt_spd, pt_tilt_acc, pt_pan_spd, pt_pan_acc):
            sl.addWidget(w)
        setattr(self, f"_pt_{n}_tilt_spd", pt_tilt_spd)
        setattr(self, f"_pt_{n}_tilt_acc", pt_tilt_acc)
        setattr(self, f"_pt_{n}_pan_spd",  pt_pan_spd)
        setattr(self, f"_pt_{n}_pan_acc",  pt_pan_acc)

        # Wrap interactive controls so the group can be disabled in sync mode
        interactive = QWidget()
        il = QHBoxLayout(interactive)
        il.setContentsMargins(0, 0, 0, 0)
        il.setSpacing(6)
        il.addWidget(traj_box)
        il.addWidget(spd_box, stretch=2)
        setattr(self, f"_pt_{n}_interactive", interactive)

        bl.addWidget(interactive, stretch=2)
        outer.addWidget(bottom)
        return tab

    # ── Motors tab ────────────────────────────────────────────────────────────

    def _make_motors_tab(self):
        tab = QWidget()
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(8, 8, 8, 8)
        outer.setSpacing(8)

        # Displays row
        disp_row = QHBoxLayout()
        self._disp_a = MotorDisplay("Motor A  (Tilt_1)")
        self._disp_b = MotorDisplay("Motor B  (Pan_1)")
        self._disp_c = MotorDisplay("Motor C  (Tilt_2)")
        self._disp_d = MotorDisplay("Motor D  (Pan_2)")
        for d in (self._disp_a, self._disp_b, self._disp_c, self._disp_d):
            disp_row.addWidget(d)
        outer.addLayout(disp_row)

        # Scrollable sliders
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        outer.addWidget(scroll)

        inner = QWidget()
        inner.setStyleSheet("background:#12121f;")
        scroll.setWidget(inner)
        il = QVBoxLayout(inner)
        il.setContentsMargins(4, 4, 4, 4)
        il.setSpacing(6)

        # Motor A
        box_a = QGroupBox("Motor A - Tilt_1  [-90 ; +90 deg]")
        la = QVBoxLayout(box_a)
        self._sl_a_pos = MotorSlider("Position [deg]",  -90,  90,  0,   0.01)
        self._sl_a_spd = MotorSlider("Speed",           0.01,  23, 17,   0.01)
        self._sl_a_acc = MotorSlider("Acceleration",    1.1, 113, 50,   0.1)
        for s in (self._sl_a_pos, self._sl_a_spd, self._sl_a_acc):
            la.addWidget(s)
        il.addWidget(box_a)

        # Motor B
        box_b = QGroupBox("Motor B - Pan_1  [0 ; 360 deg]  (multi-turn)")
        lb = QVBoxLayout(box_b)
        self._sl_b_pos = MotorSlider("Position [deg]",   0, 359.99,  0, 0.01)
        self._sl_b_spd = MotorSlider("Speed",           0.01,    46, 17, 0.01)
        self._sl_b_acc = MotorSlider("Acceleration",    1.1,   113, 50, 0.1)
        rb_s_b  = QRadioButton("Short")
        rb_cw_b = QRadioButton("Cw")
        rb_ccw_b= QRadioButton("Ccw")
        rb_s_b.setChecked(True)
        self._traj_grp_b = QButtonGroup()
        rb_row_b = QHBoxLayout()
        for i, rb in enumerate((rb_s_b, rb_cw_b, rb_ccw_b)):
            self._traj_grp_b.addButton(rb, i)
            rb_row_b.addWidget(rb)
        for s in (self._sl_b_pos, self._sl_b_spd, self._sl_b_acc):
            lb.addWidget(s)
        lb.addLayout(rb_row_b)
        il.addWidget(box_b)

        # Motor C
        box_c = QGroupBox("Motor C - Tilt_2  [-90 ; +90 deg]")
        lc = QVBoxLayout(box_c)
        self._sl_c_pos = MotorSlider("Position [deg]",  -90,  90,  0,   0.01)
        self._sl_c_spd = MotorSlider("Speed",           0.01,  23, 17,   0.01)
        self._sl_c_acc = MotorSlider("Acceleration",    1.1, 113, 50,   0.1)
        for s in (self._sl_c_pos, self._sl_c_spd, self._sl_c_acc):
            lc.addWidget(s)
        il.addWidget(box_c)

        # Motor D
        box_d = QGroupBox("Motor D - Pan_2  [0 ; 360 deg]  (multi-turn)")
        ld = QVBoxLayout(box_d)
        self._sl_d_pos = MotorSlider("Position [deg]",   0, 359.99,  0, 0.01)
        self._sl_d_spd = MotorSlider("Speed",           0.01,    46, 17, 0.01)
        self._sl_d_acc = MotorSlider("Acceleration",    1.1,   113, 50, 0.1)
        rb_s_d  = QRadioButton("Short")
        rb_cw_d = QRadioButton("Cw")
        rb_ccw_d= QRadioButton("Ccw")
        rb_s_d.setChecked(True)
        self._traj_grp_d = QButtonGroup()
        rb_row_d = QHBoxLayout()
        for i, rb in enumerate((rb_s_d, rb_cw_d, rb_ccw_d)):
            self._traj_grp_d.addButton(rb, i)
            rb_row_d.addWidget(rb)
        for s in (self._sl_d_pos, self._sl_d_spd, self._sl_d_acc):
            ld.addWidget(s)
        ld.addLayout(rb_row_d)
        il.addWidget(box_d)

        # Send all
        btn_row = QHBoxLayout()
        send_all = QPushButton("Send setpoints (all motors)")
        send_all.setObjectName("send_btn")
        send_all.clicked.connect(self._send_all_motors)
        btn_row.addWidget(send_all)
        self._motors_status = QLabel("")
        self._motors_status.setStyleSheet("font-family:Courier;font-size:10px;")
        btn_row.addWidget(self._motors_status)
        il.addLayout(btn_row)
        il.addStretch()

        return tab

    def _reset_target(self, n):
        mode_map = {0: "short", 1: "cw", 2: "ccw"}
        mode = mode_map.get(getattr(self, f"_traj_grp_{n}").checkedId(), "short")
        pan_abs = self._pan_b_abs if n == 1 else self._pan_d_abs
        new_abs = compute_target_pan(pan_abs, 0.0, mode)
        if n == 1:
            self._tgt_b_abs = new_abs
        else:
            self._tgt_d_abs = new_abs
        getattr(self, f"_pt_{n}_widget").set_target_display(0.0, 0.0)
        self._refresh_pt_labels(n)
        if n == 1 and self._sync_cb.isChecked():
            self._tgt_d_abs = compute_target_pan(self._pan_d_abs, 0.0, mode)
            self._pt_2_widget.set_target_display(0.0, 0.0)
            self._refresh_pt_labels(2)

    # ── Slot: sync toggle ───────────────────────────────────────────────

    def _on_sync_toggled(self, checked):
        self._pt_2_widget.setEnabled(not checked)
        self._pt_2_interactive.setEnabled(not checked)
        if checked:
            # Immediately mirror panel 1 targets to panel 2
            self._tgt_d_abs = self._tgt_b_abs
            self._pt_2_widget.set_target_display(
                _normalize_display_pan(self._tgt_b_abs),
                self._pt_1_widget._target_tilt,
            )
            self._refresh_pt_labels(2)

    def _on_auto_send_toggled(self, checked):
        if checked:
            self._auto_send_timer.start(self._auto_interval_spin.value())
        else:
            self._auto_send_timer.stop()

    # ── Slot: Pan/Tilt target dragged ───────────────────────────────────────

    def _on_pt_dragged(self, n, display_pan, tilt):
        mode_map = {0: "short", 1: "cw", 2: "ccw"}
        traj_grp = getattr(self, f"_traj_grp_{n}")
        mode = mode_map.get(traj_grp.checkedId(), "short")
        pan_abs_attr = "_pan_b_abs" if n == 1 else "_pan_d_abs"
        tgt_abs_attr = "_tgt_b_abs" if n == 1 else "_tgt_d_abs"
        setattr(self, tgt_abs_attr,
                compute_target_pan(getattr(self, pan_abs_attr), display_pan, mode))
        self._refresh_pt_labels(n)
        # Mirror drag to panel 2 when sync is active
        if n == 1 and self._sync_cb.isChecked():
            self._tgt_d_abs = compute_target_pan(self._pan_d_abs, display_pan, mode)
            self._pt_2_widget.set_target_display(display_pan, tilt)
            self._refresh_pt_labels(2)

    # ── Send Pan/Tilt ─────────────────────────────────────────────────────────

    def _send_all_pantilt(self):
        tilt1_tgt = self._pt_1_widget._target_tilt
        tilt1_spd = self._pt_1_tilt_spd.get()
        tilt1_acc = self._pt_1_tilt_acc.get()
        pan1_tgt  = self._tgt_b_abs % 360
        pan1_spd  = self._pt_1_pan_spd.get()
        pan1_acc  = self._pt_1_pan_acc.get()
        pan1_dir  = self._traj_grp_1.checkedId()

        if self._sync_cb.isChecked():
            self._tgt_d_abs = self._tgt_b_abs
            tilt2_tgt, tilt2_spd, tilt2_acc = tilt1_tgt, tilt1_spd, tilt1_acc
            pan2_tgt,  pan2_spd,  pan2_acc   = pan1_tgt,  pan1_spd,  pan1_acc
            pan2_dir  = pan1_dir
        else:
            tilt2_tgt = self._pt_2_widget._target_tilt
            tilt2_spd = self._pt_2_tilt_spd.get()
            tilt2_acc = self._pt_2_tilt_acc.get()
            pan2_tgt  = self._tgt_d_abs % 360
            pan2_spd  = self._pt_2_pan_spd.get()
            pan2_acc  = self._pt_2_pan_acc.get()
            pan2_dir  = self._traj_grp_2.checkedId()

        command = (
            f"{tilt1_tgt:.2f},{tilt1_spd:.2f},{tilt1_acc:.2f},"
            f"{pan1_tgt:.2f},{pan1_spd:.2f},{pan1_dir},{pan1_acc:.2f},"
            f"{tilt2_tgt:.2f},{tilt2_spd:.2f},{tilt2_acc:.2f},"
            f"{pan2_tgt:.2f},{pan2_spd:.2f},{pan2_dir},{pan2_acc:.2f}"
        )
        result = self.serial_reader.write_command(command) if self.serial_connected else None
        self._apply_send_result(
            result, self._shared_status,
            f"T1:{tilt1_tgt:+.1f} P1:{self._tgt_b_abs:+.0f}  "
            f"T2:{tilt2_tgt:+.1f} P2:{self._tgt_d_abs:+.0f}"
        )

    # ── Send all motors ───────────────────────────────────────────────────────

    def _send_all_motors(self):
        mAt = self._sl_a_pos.get()
        mAs = self._sl_a_spd.get()
        mAa = self._sl_a_acc.get()
        mBt = self._sl_b_pos.get()
        mBs = self._sl_b_spd.get()
        mBd = self._traj_grp_b.checkedId()
        mBa = self._sl_b_acc.get()
        mCt = self._sl_c_pos.get()
        mCs = self._sl_c_spd.get()
        mCa = self._sl_c_acc.get()
        mDt = self._sl_d_pos.get()
        mDs = self._sl_d_spd.get()
        mDd = self._traj_grp_d.checkedId()
        mDa = self._sl_d_acc.get()
        command = (
            f"{mAt:.2f},{mAs:.2f},{mAa:.2f},"
            f"{mBt:.2f},{mBs:.2f},{mBd},{mBa:.2f},"
            f"{mCt:.2f},{mCs:.2f},{mCa:.2f},"
            f"{mDt:.2f},{mDs:.2f},{mDd},{mDa:.2f}"
        )
        result = self.serial_reader.write_command(command) if self.serial_connected else None
        self._apply_send_result(result, self._motors_status, "Setpoints sent")

    @staticmethod
    def _apply_send_result(ok, label, ok_msg):
        if ok is None:
            label.setStyleSheet("color:#ee8844;font-family:Courier;font-size:10px;")
            label.setText("Not connected")
        elif ok:
            label.setStyleSheet("color:#88ee88;font-family:Courier;font-size:10px;")
            label.setText(f"OK  {ok_msg}")
        else:
            label.setStyleSheet("color:#ee4444;font-family:Courier;font-size:10px;")
            label.setText("Send error")

    # ── Periodic update (50 ms) ───────────────────────────────────────────────

    def _update_values(self):
        try:
            while True:
                d = self.data_queue.get_nowait()

                new_b   = d["position_b"]
                delta_b = new_b - self._pan_b_prev
                if delta_b >  180: delta_b -= 360
                if delta_b < -180: delta_b += 360
                self._pan_b_abs  += delta_b
                self._pan_b_prev  = new_b

                new_d   = d["position_d"]
                delta_d = new_d - self._pan_d_prev
                if delta_d >  180: delta_d -= 360
                if delta_d < -180: delta_d += 360
                self._pan_d_abs  += delta_d
                self._pan_d_prev  = new_d

                self.position_a = d["position_a"]
                self.position_b = new_b
                self.position_c = d["position_c"]
                self.position_d = new_d
                self.speed_a    = d["speed_a"]
                self.speed_b    = d["speed_b"]
                self.speed_c    = d["speed_c"]
                self.speed_d    = d["speed_d"]
                self.moving_a   = d["moving_a"]
                self.moving_b   = d["moving_b"]
                self.moving_c   = d["moving_c"]
                self.moving_d   = d["moving_d"]
        except queue.Empty:
            pass

        self._disp_a.update_position(self.position_a)
        self._disp_b.update_position(self.position_b)
        self._disp_c.update_position(self.position_c)
        self._disp_d.update_position(self.position_d)
        self._disp_a.update_speed(self.speed_a)
        self._disp_b.update_speed(self.speed_b)
        self._disp_c.update_speed(self.speed_c)
        self._disp_d.update_speed(self.speed_d)
        self._disp_a.update_moving(self.moving_a)
        self._disp_b.update_moving(self.moving_b)
        self._disp_c.update_moving(self.moving_c)
        self._disp_d.update_moving(self.moving_d)

        self._pt_1_widget.set_current(self._pan_b_abs, self.position_a)
        self._pt_1_widget.set_target_display(
            _normalize_display_pan(self._tgt_b_abs),
            self._pt_1_widget._target_tilt,
        )
        self._pt_2_widget.set_current(self._pan_d_abs, self.position_c)
        self._pt_2_widget.set_target_display(
            _normalize_display_pan(self._tgt_d_abs),
            self._pt_2_widget._target_tilt,
        )
        self._refresh_pt_labels(1)
        self._refresh_pt_labels(2)

    def _refresh_pt_labels(self, n):
        pan_abs   = self._pan_b_abs  if n == 1 else self._pan_d_abs
        tgt_abs   = self._tgt_b_abs  if n == 1 else self._tgt_d_abs
        tilt_cur  = self.position_a  if n == 1 else self.position_c
        mov_tilt  = self.moving_a    if n == 1 else self.moving_c
        mov_pan   = self.moving_b    if n == 1 else self.moving_d
        pt_widget = getattr(self, f"_pt_{n}_widget")
        cur_disp  = _normalize_display_pan(pan_abs)
        tgt_disp  = _normalize_display_pan(tgt_abs)
        getattr(self, f"_lbl_{n}_cur_pan"    ).setText(f"Pan  : {cur_disp:6.1f}")
        getattr(self, f"_lbl_{n}_cur_tilt"   ).setText(f"Tilt : {tilt_cur:+7.1f}")
        getattr(self, f"_lbl_{n}_cur_pan_abs").setText(f"Pan absolute : {pan_abs:+.1f}")
        getattr(self, f"_lbl_{n}_tgt_pan"    ).setText(f"Pan  : {tgt_disp:6.1f}")
        getattr(self, f"_lbl_{n}_tgt_tilt"   ).setText(f"Tilt : {pt_widget._target_tilt:+7.1f}")
        getattr(self, f"_lbl_{n}_tgt_pan_abs").setText(f"Pan absolute : {tgt_abs:+.1f}")
        lbl_mt = getattr(self, f"_lbl_{n}_mov_tilt")
        lbl_mp = getattr(self, f"_lbl_{n}_mov_pan")
        lbl_mt.setText("Tilt : ● moving" if mov_tilt else "Tilt : ○ stopped")
        lbl_mt.setStyleSheet(
            "font-family:Courier;font-size:10px;color:#ff6644;" if mov_tilt
            else "font-family:Courier;font-size:10px;color:#444455;"
        )
        lbl_mp.setText("Pan  : ● moving" if mov_pan else "Pan  : ○ stopped")
        lbl_mp.setStyleSheet(
            "font-family:Courier;font-size:10px;color:#ff6644;" if mov_pan
            else "font-family:Courier;font-size:10px;color:#444455;"
        )

    # ── Cleanup ───────────────────────────────────────────────────────────────

    def closeEvent(self, event):
        self._timer.stop()
        self._auto_send_timer.stop()
        if self.serial_connected:
            self.serial_reader.disconnect()
        event.accept()


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Moving Speaker Sim - Pan/Tilt controller")
    parser.add_argument("-p", "--port", dest="serial_port", default="COM4")
    parser.add_argument("-l", "--log",  dest="log",         default=None)
    args = parser.parse_args()

    app = QApplication(sys.argv)
    app.setStyleSheet(_DARK)
    window = MotorHeadUI(serial_port=args.serial_port, log_path=args.log)
    window.show()
    sys.exit(app.exec())
