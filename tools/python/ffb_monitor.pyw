"""
FFB Monitor - DirectInput FFB デバッグモニター
=========================================
RP2040 FFBデバイスからのシリアル出力 ([FFB] フォーマット) を
リアルタイムで可視化するGUIツール。

依存パッケージ:
    pip install pyserial matplotlib

シリアル出力フォーマット:
    [FFB] ID:0x05 Idx: 1 Mag: -9842 Op:0 Gain:255 Active:1 CF:1
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import threading
import queue
import re
import time
import datetime
import collections
import sys

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: pyserial が見つかりません。'pip install pyserial' を実行してください。")
    sys.exit(1)

try:
    import matplotlib
    matplotlib.use("TkAgg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.animation import FuncAnimation
    import matplotlib.style as mstyle
except ImportError:
    print("ERROR: matplotlib が見つかりません。'pip install matplotlib' を実行してください。")
    sys.exit(1)

# ============================================================
# 定数
# ============================================================
APP_TITLE      = "FFB Monitor - DirectInput Debug"
GRAPH_MAXLEN   = 500          # グラフに表示するサンプル数
LOG_MAXLINES   = 2000         # ログ表示の最大行数
UPDATE_INTERVAL_MS = 50       # GUIリフレッシュ間隔 (ms)

BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]
DEFAULT_BAUD = 115200

OP_LABELS = {0: "---", 1: "Start", 2: "Solo", 3: "Stop"}
REPORT_ID_LABELS = {
    0x01: "SetEffect",
    0x02: "SetEnvelope",
    0x03: "SetCondition",
    0x04: "SetPeriodic",
    0x05: "SetConstForce",
    0x06: "SetRamp",
    0x0A: "EffectOp",
    0x0B: "BlockFree",
    0x0C: "DevCtrl",
    0x0D: "DevGain",
}

# ダークパレット
C_BG       = "#1e1e2e"
C_SURFACE  = "#2a2a3e"
C_PANEL    = "#313150"
C_ACCENT   = "#7c6af7"
C_ACCENT2  = "#4fc3f7"
C_GREEN    = "#a8e6a3"
C_RED      = "#f28b82"
C_YELLOW   = "#ffd966"
C_TEXT     = "#cdd6f4"
C_SUBTEXT  = "#7c7fa6"
C_BORDER   = "#44475a"

# ============================================================
# シリアルラインパーサ
# ============================================================
_FFB_RE = re.compile(
    r"\[FFB\]\s+ID:0x([0-9A-Fa-f]+)\s+Idx:\s*(\d+)\s+Mag:\s*(-?\d+)\s+Op:(\d+)\s+Gain:(\d+)\s+Active:(\d+)\s+CF:(\d+)"
)

def parse_ffb_line(line: str):
    """[FFB]行をパースしてdictを返す。非マッチはNone。"""
    m = _FFB_RE.search(line)
    if not m:
        return None
    return {
        "report_id"          : int(m.group(1), 16),
        "effect_block_index" : int(m.group(2)),
        "magnitude"          : int(m.group(3)),
        "operation"          : int(m.group(4)),
        "gain"               : int(m.group(5)),
        "active"             : int(m.group(6)) != 0,
        "cf"                 : int(m.group(7)) != 0,
        "ts"                 : time.monotonic(),
    }

# ============================================================
# シリアル読み取りスレッド
# ============================================================
class SerialReaderThread(threading.Thread):
    def __init__(self, port, baud, data_queue):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.data_queue = data_queue
        self._stop_event = threading.Event()
        self.ser = None

    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
            while not self._stop_event.is_set():
                try:
                    raw = self.ser.readline()
                    if raw:
                        line = raw.decode("utf-8", errors="replace").rstrip()
                        self.data_queue.put(("line", line))
                except serial.SerialException as e:
                    self.data_queue.put(("error", str(e)))
                    break
        except Exception as e:
            self.data_queue.put(("error", str(e)))
        finally:
            if self.ser and self.ser.is_open:
                self.ser.close()
            self.data_queue.put(("disconnected", None))

    def stop(self):
        self._stop_event.set()
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass

# ============================================================
# メインアプリ
# ============================================================
class FFBMonitorApp(tk.Tk):

    def __init__(self):
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("1100x780")
        self.minsize(900, 600)
        self.configure(bg=C_BG)

        # 状態
        self._reader       = None
        self._data_queue   = queue.Queue()
        self._connected    = False
        self._log_lines    = []
        self._log_paused   = False
        self._start_time   = None

        # グラフデータ
        self._times      = collections.deque(maxlen=GRAPH_MAXLEN)
        self._magnitudes = collections.deque(maxlen=GRAPH_MAXLEN)

        self._build_ui()
        self._refresh_ports()
        self._schedule_update()

    # ----------------------------------------------------------
    # UI 構築
    # ----------------------------------------------------------
    def _build_ui(self):
        self._build_style()
        # 上段: 接続バー
        self._build_conn_bar()
        # 中段: グラフ + ステータスパネル
        mid = tk.Frame(self, bg=C_BG)
        mid.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 4))
        mid.columnconfigure(0, weight=3)
        mid.columnconfigure(1, weight=1)
        mid.rowconfigure(0, weight=1)
        self._build_graph(mid)
        self._build_status_panel(mid)
        # 下段: ログ
        self._build_log_area()

    def _build_style(self):
        s = ttk.Style(self)
        s.theme_use("clam")
        s.configure(".",
            background=C_BG, foreground=C_TEXT,
            fieldbackground=C_SURFACE, troughcolor=C_SURFACE,
            bordercolor=C_BORDER, lightcolor=C_BORDER, darkcolor=C_BORDER,
            relief="flat", font=("Segoe UI", 9))
        s.configure("TCombobox",
            selectbackground=C_ACCENT, selectforeground=C_TEXT,
            fieldbackground=C_SURFACE, background=C_SURFACE,
            foreground=C_TEXT, arrowcolor=C_TEXT)
        s.map("TCombobox",
            fieldbackground=[("readonly", C_SURFACE)],
            foreground=[("readonly", C_TEXT)])
        s.configure("TButton",
            background=C_PANEL, foreground=C_TEXT, padding=(10, 5))
        s.map("TButton",
            background=[("active", C_ACCENT)],
            foreground=[("active", "#ffffff")])
        s.configure("Accent.TButton",
            background=C_ACCENT, foreground="#ffffff", padding=(10, 5))
        s.map("Accent.TButton",
            background=[("active", "#9b88ff")],
            foreground=[("active", "#ffffff")])
        s.configure("Danger.TButton",
            background="#c62828", foreground="#ffffff", padding=(10, 5))
        s.map("Danger.TButton",
            background=[("active", "#e53935")])
        s.configure("TLabel", background=C_BG, foreground=C_TEXT)
        s.configure("Sub.TLabel", background=C_BG, foreground=C_SUBTEXT, font=("Segoe UI", 8))
        s.configure("Panel.TLabel", background=C_PANEL, foreground=C_TEXT)
        s.configure("Big.TLabel", background=C_PANEL, foreground=C_TEXT, font=("Consolas", 22, "bold"))
        s.configure("Status.TLabel", background=C_SURFACE, foreground=C_SUBTEXT, font=("Segoe UI", 8))
        s.configure("TFrame", background=C_BG)

    def _build_conn_bar(self):
        bar = tk.Frame(self, bg=C_SURFACE, pady=6)
        bar.pack(fill=tk.X, padx=0, pady=0)

        inner = tk.Frame(bar, bg=C_SURFACE)
        inner.pack(padx=12)

        # ポート選択
        tk.Label(inner, text="Port", bg=C_SURFACE, fg=C_SUBTEXT,
                 font=("Segoe UI", 8)).grid(row=0, column=0, sticky="w")
        self._port_var = tk.StringVar()
        self._port_cb = ttk.Combobox(inner, textvariable=self._port_var,
                                     width=14, state="readonly")
        self._port_cb.grid(row=1, column=0, padx=(0, 6))

        # ボーレート
        tk.Label(inner, text="Baud Rate", bg=C_SURFACE, fg=C_SUBTEXT,
                 font=("Segoe UI", 8)).grid(row=0, column=1, sticky="w")
        self._baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        self._baud_cb = ttk.Combobox(inner, textvariable=self._baud_var,
                                     values=[str(b) for b in BAUD_RATES],
                                     width=10, state="readonly")
        self._baud_cb.grid(row=1, column=1, padx=(0, 6))

        # リフレッシュ
        self._refresh_btn = ttk.Button(inner, text="⟳ Refresh",
                                       command=self._refresh_ports)
        self._refresh_btn.grid(row=1, column=2, padx=(0, 12))

        # 接続/切断
        self._conn_btn = ttk.Button(inner, text="▶  Connect",
                                    command=self._toggle_connect,
                                    style="Accent.TButton")
        self._conn_btn.grid(row=1, column=3, padx=(0, 16))

        # ステータスLED
        self._led_canvas = tk.Canvas(inner, width=14, height=14,
                                     bg=C_SURFACE, highlightthickness=0)
        self._led_canvas.grid(row=1, column=4, padx=(0, 4))
        self._led = self._led_canvas.create_oval(2, 2, 12, 12, fill="#555", outline="")
        self._led_label = tk.Label(inner, text="Disconnected",
                                   bg=C_SURFACE, fg=C_SUBTEXT,
                                   font=("Segoe UI", 9))
        self._led_label.grid(row=1, column=5, padx=(0, 20))

        # パケットカウンタ
        tk.Label(inner, text="Packets", bg=C_SURFACE, fg=C_SUBTEXT,
                 font=("Segoe UI", 8)).grid(row=0, column=6, sticky="w")
        self._pkt_var = tk.StringVar(value="0")
        tk.Label(inner, textvariable=self._pkt_var,
                 bg=C_SURFACE, fg=C_ACCENT2,
                 font=("Consolas", 11, "bold")).grid(row=1, column=6, padx=(0, 20))

        # ログ保存
        self._save_btn = ttk.Button(inner, text="💾 Save Log",
                                    command=self._save_log,
                                    state="disabled")
        self._save_btn.grid(row=1, column=7, padx=(0, 6))

        # クリア
        ttk.Button(inner, text="🗑 Clear",
                   command=self._clear_all).grid(row=1, column=8)

        self._pkt_count = 0

    def _build_graph(self, parent):
        frame = tk.Frame(parent, bg=C_SURFACE, relief="flat", bd=0)
        frame.grid(row=0, column=0, sticky="nsew", padx=(0, 6), pady=4)

        tk.Label(frame, text="Magnitude  (Real-time)",
                 bg=C_SURFACE, fg=C_SUBTEXT,
                 font=("Segoe UI", 8)).pack(anchor="nw", padx=8, pady=(6, 0))

        plt.rcParams.update({
            "figure.facecolor" : C_SURFACE,
            "axes.facecolor"   : C_BG,
            "axes.edgecolor"   : C_BORDER,
            "axes.labelcolor"  : C_SUBTEXT,
            "xtick.color"      : C_SUBTEXT,
            "ytick.color"      : C_SUBTEXT,
            "grid.color"       : C_BORDER,
            "grid.linestyle"   : "--",
            "grid.linewidth"   : 0.5,
            "lines.linewidth"  : 1.5,
        })

        self._fig, self._ax = plt.subplots(figsize=(6, 3.2))
        self._fig.subplots_adjust(left=0.08, right=0.98, top=0.92, bottom=0.14)
        self._ax.set_ylim(-11000, 11000)
        self._ax.set_xlim(0, GRAPH_MAXLEN)
        self._ax.set_ylabel("Magnitude", fontsize=8)
        self._ax.set_xlabel("Samples", fontsize=8)
        self._ax.axhline(0, color=C_BORDER, linewidth=0.8)
        self._ax.axhline(10000,  color="#555577", linewidth=0.6, linestyle=":")
        self._ax.axhline(-10000, color="#555577", linewidth=0.6, linestyle=":")
        self._ax.grid(True)
        self._mag_line, = self._ax.plot([], [], color=C_ACCENT, lw=1.5)
        self._ax.set_title("Waiting for data...", fontsize=8, color=C_SUBTEXT, pad=4)

        canvas = FigureCanvasTkAgg(self._fig, master=frame)
        canvas.draw()
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        self._graph_canvas = canvas

    def _build_status_panel(self, parent):
        frame = tk.Frame(parent, bg=C_PANEL, relief="flat", bd=0)
        frame.grid(row=0, column=1, sticky="nsew", pady=4)

        def section(text):
            tk.Label(frame, text=text, bg=C_PANEL, fg=C_SUBTEXT,
                     font=("Segoe UI", 7, "bold")).pack(anchor="nw", padx=10, pady=(10, 2))

        def value_row(label, var, color=C_TEXT, bigfont=False):
            row = tk.Frame(frame, bg=C_PANEL)
            row.pack(fill=tk.X, padx=10, pady=1)
            tk.Label(row, text=label, bg=C_PANEL, fg=C_SUBTEXT,
                     font=("Segoe UI", 8), width=9, anchor="w").pack(side=tk.LEFT)
            font = ("Consolas", 14, "bold") if bigfont else ("Consolas", 10)
            tk.Label(row, textvariable=var, bg=C_PANEL, fg=color,
                     font=font, anchor="e").pack(side=tk.RIGHT)

        section("■ LAST PACKET")
        self._v_reportid  = tk.StringVar(value="---")
        self._v_rid_name  = tk.StringVar(value="")
        self._v_idx       = tk.StringVar(value="---")
        value_row("Report ID", self._v_reportid, C_ACCENT2)
        row2 = tk.Frame(frame, bg=C_PANEL)
        row2.pack(fill=tk.X, padx=10)
        tk.Label(row2, textvariable=self._v_rid_name, bg=C_PANEL, fg=C_SUBTEXT,
                 font=("Segoe UI", 8)).pack(anchor="e")
        value_row("Block Idx", self._v_idx, C_ACCENT2)

        section("■ MAGNITUDE")
        self._v_magnitude = tk.StringVar(value="---")
        value_row("Value", self._v_magnitude, C_ACCENT, bigfont=True)

        section("■ EFFECT STATE")
        self._v_active    = tk.StringVar(value="---")
        self._v_operation = tk.StringVar(value="---")
        self._v_cf        = tk.StringVar(value="---")
        value_row("Active",    self._v_active)
        value_row("Operation", self._v_operation)
        value_row("CF",        self._v_cf)

        section("■ GAIN")
        self._v_gain      = tk.StringVar(value="---")
        value_row("Device Gain", self._v_gain, C_YELLOW)

        # Active インジケータ
        tk.Label(frame, text="ACTIVE", bg=C_PANEL, fg=C_SUBTEXT,
                 font=("Segoe UI", 7, "bold")).pack(pady=(14, 2))
        self._active_ind = tk.Canvas(frame, width=60, height=60,
                                     bg=C_PANEL, highlightthickness=0)
        self._active_ind.pack()
        self._active_circle = self._active_ind.create_oval(
            5, 5, 55, 55, fill="#333344", outline=C_BORDER, width=2)

        # 最小・最大
        section("■ SESSION STATS")
        self._v_min = tk.StringVar(value="---")
        self._v_max = tk.StringVar(value="---")
        value_row("Min Mag", self._v_min, C_RED)
        value_row("Max Mag", self._v_max, C_GREEN)

        self._session_min =  99999
        self._session_max = -99999

    def _build_log_area(self):
        frame = tk.Frame(self, bg=C_SURFACE)
        frame.pack(fill=tk.X, padx=8, pady=(0, 8))

        hdr = tk.Frame(frame, bg=C_SURFACE)
        hdr.pack(fill=tk.X, padx=6, pady=(4, 0))
        tk.Label(hdr, text="Serial Log", bg=C_SURFACE, fg=C_SUBTEXT,
                 font=("Segoe UI", 8, "bold")).pack(side=tk.LEFT)
        self._pause_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(hdr, text="Pause scroll", variable=self._pause_var,
                        command=self._on_pause_toggle).pack(side=tk.RIGHT)

        self._log_text = tk.Text(
            frame, height=8, bg=C_BG, fg=C_TEXT,
            font=("Consolas", 9), insertbackground=C_TEXT,
            selectbackground=C_ACCENT, state="disabled",
            relief="flat", bd=0, wrap="none")
        self._log_text.pack(fill=tk.X, padx=4, pady=(2, 4))
        self._log_text.tag_config("ffb",    foreground=C_ACCENT2)
        self._log_text.tag_config("err",    foreground=C_RED)
        self._log_text.tag_config("status", foreground=C_YELLOW)

    # ----------------------------------------------------------
    # シリアルポート管理
    # ----------------------------------------------------------
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self._port_cb["values"] = ports
        if ports:
            self._port_var.set(ports[0])
        else:
            self._port_var.set("")

    def _toggle_connect(self):
        if self._connected:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self._port_var.get()
        baud = self._baud_var.get()
        if not port:
            messagebox.showwarning("Port Error", "COMポートを選択してください。")
            return
        self._data_queue = queue.Queue()
        self._reader = SerialReaderThread(port, int(baud), self._data_queue)
        self._reader.start()
        self._connected = True
        self._start_time = time.monotonic()
        self._conn_btn.configure(text="■  Disconnect", style="Danger.TButton")
        self._set_led(True)
        self._led_label.configure(text=f"{port} @ {baud}", fg=C_GREEN)
        self._save_btn.configure(state="normal")
        self._log_append(f"[{self._ts()}] Connected: {port} @ {baud} bps\n", "status")

    def _disconnect(self):
        if self._reader:
            self._reader.stop()
            self._reader = None
        self._connected = False
        self._conn_btn.configure(text="▶  Connect", style="Accent.TButton")
        self._set_led(False)
        self._led_label.configure(text="Disconnected", fg=C_SUBTEXT)
        self._log_append(f"[{self._ts()}] Disconnected\n", "status")

    def _set_led(self, on: bool):
        color = C_GREEN if on else "#555555"
        self._led_canvas.itemconfig(self._led, fill=color)

    # ----------------------------------------------------------
    # 定期更新
    # ----------------------------------------------------------
    def _schedule_update(self):
        self.after(UPDATE_INTERVAL_MS, self._update)

    def _update(self):
        changed = False
        while not self._data_queue.empty():
            kind, payload = self._data_queue.get_nowait()
            if kind == "line":
                self._process_line(payload)
                changed = True
            elif kind == "error":
                self._log_append(f"[ERROR] {payload}\n", "err")
                self._disconnect()
            elif kind == "disconnected":
                self._connected = False
                self._conn_btn.configure(text="▶  Connect", style="Accent.TButton")
                self._set_led(False)
                self._led_label.configure(text="Disconnected", fg=C_SUBTEXT)

        if changed:
            self._update_graph()

        self._schedule_update()

    def _process_line(self, line: str):
        self._log_append(line + "\n", "ffb" if line.startswith("[FFB]") else None)
        parsed = parse_ffb_line(line)
        if parsed is None:
            return

        self._pkt_count += 1
        self._pkt_var.set(str(self._pkt_count))

        mag = parsed["magnitude"]
        self._times.append(self._pkt_count)
        self._magnitudes.append(mag)

        # セッション統計
        if mag < self._session_min:
            self._session_min = mag
            self._v_min.set(str(mag))
        if mag > self._session_max:
            self._session_max = mag
            self._v_max.set(str(mag))

        # ステータスパネル更新
        rid = parsed["report_id"]
        idx = parsed["effect_block_index"]
        self._v_reportid.set(f"0x{rid:02X}")
        self._v_rid_name.set(REPORT_ID_LABELS.get(rid, "Unknown"))
        self._v_idx.set(str(idx))
        self._v_magnitude.set(f"{mag:+7d}")
        act = parsed["active"]
        self._v_active.set("● PLAYING" if act else "○  Idle")
        op = parsed["operation"]
        self._v_operation.set(f"{OP_LABELS.get(op, str(op))} ({op})")
        self._v_cf.set("Yes" if parsed["cf"] else "No")
        self._v_gain.set(f'{parsed["gain"]}  ({parsed["gain"]*100//255}%)')

        # Active インジケータ
        circle_color = C_GREEN if act else "#333344"
        self._active_ind.itemconfig(self._active_circle, fill=circle_color)

    def _update_graph(self):
        if not self._magnitudes:
            return
        xs = list(range(len(self._magnitudes)))
        self._mag_line.set_data(xs, list(self._magnitudes))
        self._ax.set_xlim(0, max(GRAPH_MAXLEN, len(self._magnitudes)))
        last_mag = list(self._magnitudes)[-1]
        self._ax.set_title(
            f"Magnitude: {last_mag:+d}  |  Packets: {self._pkt_count}",
            fontsize=8, color=C_TEXT, pad=4)
        self._graph_canvas.draw_idle()

    # ----------------------------------------------------------
    # ログ操作
    # ----------------------------------------------------------
    def _log_append(self, text: str, tag=None):
        self._log_lines.append((text, tag))
        if len(self._log_lines) > LOG_MAXLINES:
            self._log_lines.pop(0)

        self._log_text.configure(state="normal")
        if tag:
            self._log_text.insert(tk.END, text, tag)
        else:
            self._log_text.insert(tk.END, text)

        # 行数制限
        lines = int(self._log_text.index(tk.END).split(".")[0])
        if lines > LOG_MAXLINES:
            self._log_text.delete("1.0", f"{lines - LOG_MAXLINES}.0")

        if not self._pause_var.get():
            self._log_text.see(tk.END)
        self._log_text.configure(state="disabled")

    def _on_pause_toggle(self):
        pass  # チェックボックスの状態は _pause_var で管理

    def _save_log(self):
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        path = filedialog.asksaveasfilename(
            defaultextension=".txt",
            initialfile=f"ffb_log_{ts}.txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if not path:
            return
        with open(path, "w", encoding="utf-8") as f:
            for text, _ in self._log_lines:
                f.write(text)
        messagebox.showinfo("Saved", f"ログを保存しました:\n{path}")

    def _clear_all(self):
        self._log_lines.clear()
        self._log_text.configure(state="normal")
        self._log_text.delete("1.0", tk.END)
        self._log_text.configure(state="disabled")
        self._times.clear()
        self._magnitudes.clear()
        self._pkt_count = 0
        self._pkt_var.set("0")
        self._session_min =  99999
        self._session_max = -99999
        self._v_min.set("---")
        self._v_max.set("---")
        self._mag_line.set_data([], [])
        self._ax.set_title("Waiting for data...", fontsize=8, color=C_SUBTEXT, pad=4)
        self._graph_canvas.draw_idle()

    # ----------------------------------------------------------
    # ユーティリティ
    # ----------------------------------------------------------
    def _ts(self):
        return datetime.datetime.now().strftime("%H:%M:%S")

    def on_closing(self):
        self._disconnect()
        self.destroy()


# ============================================================
# エントリーポイント
# ============================================================
if __name__ == "__main__":
    app = FFBMonitorApp()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
