#!/usr/bin/env python3
"""
Sound Distance Measurement using Chirp Sonar
Measures distance to objects by emitting a chirp and detecting the echo
"""

import numpy as np
import pyaudio
import sys
import os
from datetime import datetime
from scipy.io import wavfile
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QFrame, QGroupBox, QTabWidget, QFileDialog
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal
from PyQt6.QtGui import QFont, QPalette, QColor
import pyqtgraph as pg


# ─── Worker thread ────────────────────────────────────────────────────────────

class MeasurementThread(QThread):
    """Run distance measurement in background to keep UI responsive."""

    finished = pyqtSignal(object, object, object, object, object)
    status_update = pyqtSignal(str)

    def __init__(self, meter):
        super().__init__()
        self.meter = meter

    def run(self):
        self.status_update.emit("Playing chirp...")
        distance, time_delay, chirp, audio_data, correlation = self.meter.measure_distance()
        self.finished.emit(distance, time_delay, chirp, audio_data, correlation)


# ─── Measurement logic ────────────────────────────────────────────────────────

class ChirpDistanceMeter:
    # SAMPLE_RATE   = 44100
    # SAMPLE_RATE   = 96000
    SAMPLE_RATE   = 192000
    CHUNK         = 1024
    SPEED_OF_SOUND = 343.0   # m/s at 20 °C
    MIN_DIST      = 0.6      # metres
    MAX_DIST      = 1.4      # metres
    RX_DURATION   = 0.1      # second
    MIN_CORR      = 0.2      # reject weak peaks

    def __init__(self):
        self.audio = pyaudio.PyAudio()

    # ------------------------------------------------------------------
    def generate_chirp(self, duration=0.02, f_start=2000, f_end=4000):
        """Linear frequency-sweep chirp with Hanning envelope."""
        t = np.linspace(0, duration, int(self.SAMPLE_RATE * duration))
        chirp = np.sin(2 * np.pi * (f_start * t + (f_end - f_start) * t ** 2 / (2 * duration)))
        chirp = chirp * np.hanning(len(chirp)) * 0.3
        return chirp.astype(np.float32)

    # ------------------------------------------------------------------
    def detect_echo(self, audio_data):
        """
        Detect echo via FFT-based autocorrelation of the received signal.
        Search is restricted to the round-trip delay window for [MIN_DIST, MAX_DIST].
        """
        audio_norm = audio_data / (np.max(np.abs(audio_data)) + 1e-10)

        n = len(audio_norm)
        fft_sig  = np.fft.rfft(audio_norm, n=2 * n)
        autocorr = np.fft.irfft(fft_sig * np.conj(fft_sig))[:n]
        autocorr /= (np.max(np.abs(autocorr)) + 1e-10)

        # Convert distance range → sample indices (round-trip)
        min_s = int((2 * self.MIN_DIST / self.SPEED_OF_SOUND) * self.SAMPLE_RATE)
        max_s = min(int((2 * self.MAX_DIST / self.SPEED_OF_SOUND) * self.SAMPLE_RATE), n - 1)

        window = autocorr[min_s:max_s]
        if window.size == 0:
            return None, None, autocorr

        peak_idx = int(np.argmax(window)) + min_s

        if autocorr[peak_idx] < self.MIN_CORR:           # reject weak peaks
            return None, None, autocorr

        time_delay = peak_idx / self.SAMPLE_RATE
        distance   = (self.SPEED_OF_SOUND * time_delay) / 2
        return distance, time_delay, autocorr

    # ------------------------------------------------------------------
    def measure_distance(self):
        """Play chirp, record echo, return measurement data."""
        try:
            chirp = self.generate_chirp()

            stream_out = self.audio.open(
                format=pyaudio.paFloat32, channels=1,
                rate=self.SAMPLE_RATE, output=True
            )
            stream_in = self.audio.open(
                format=pyaudio.paFloat32, channels=1,
                rate=self.SAMPLE_RATE, input=True,
                frames_per_buffer=self.CHUNK
            )

            stream_out.write(chirp.tobytes())
            stream_out.stop_stream()
            stream_out.close()

            frames     = []
            num_chunks = int(self.SAMPLE_RATE * self.RX_DURATION / self.CHUNK)
            for _ in range(num_chunks):
                data = stream_in.read(self.CHUNK, exception_on_overflow=False)
                frames.append(np.frombuffer(data, dtype=np.float32))

            stream_in.stop_stream()
            stream_in.close()

            audio_data              = np.concatenate(frames)
            distance, time_delay, correlation = self.detect_echo(audio_data)
            return distance, time_delay, chirp, audio_data, correlation

        except Exception as e:
            return None, str(e), None, None, None

    # ------------------------------------------------------------------
    def cleanup(self):
        self.audio.terminate()


# ─── Main window ──────────────────────────────────────────────────────────────

class DistanceMeterWindow(QMainWindow):

    def __init__(self):
        super().__init__()
        self.meter              = ChirpDistanceMeter()
        self.measurement_thread = None
        self.last_chirp         = None
        self.last_audio         = None
        self.last_distance      = None
        self.last_time_delay    = None
        self._init_ui()

    # ── UI construction ───────────────────────────────────────────────

    def _init_ui(self):
        self.setWindowTitle("Chirp Distance Meter")
        self.setFixedSize(900, 800)

        central = QWidget()
        self.setCentralWidget(central)

        root = QHBoxLayout()
        root.setSpacing(20)
        root.setContentsMargins(20, 20, 20, 20)
        root.addWidget(self._build_left_panel(), stretch=0)
        root.addWidget(self._build_right_panel(), stretch=1)
        central.setLayout(root)

        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor("#f0f0f0"))
        self.setPalette(palette)

    # ------------------------------------------------------------------
    def _build_left_panel(self):
        panel  = QWidget()
        panel.setMaximumWidth(320)
        layout = QVBoxLayout()
        layout.setSpacing(16)

        # Title
        title = QLabel("🔊 Sound Distance Meter")
        title.setFont(QFont("Arial", 18, QFont.Weight.Bold))
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("color: #2c3e50;")
        layout.addWidget(title)

        # Info box
        layout.addWidget(self._make_group(
            "How it works", "#e8f4f8", "#3498db",
            "• Emits chirp (2–4 kHz)\n"
            "• Autocorrelation on received signal\n"
            f"• Range: {self.meter.MIN_DIST} m – {self.meter.MAX_DIST} m\n"
            "• Use hard, flat surfaces"
        ))

        # Measure button
        self.measure_btn = self._make_button(
            "▶  Start Measurement", "#3498db", "#2980b9", "#21618c"
        )
        self.measure_btn.clicked.connect(self.start_measurement)
        layout.addWidget(self.measure_btn)

        # Save button
        self.save_btn = self._make_button(
            "💾  Save Audio", "#27ae60", "#229954", "#1e8449"
        )
        self.save_btn.setEnabled(False)
        self.save_btn.clicked.connect(self.save_audio)
        layout.addWidget(self.save_btn)

        # Status
        self.status_label = QLabel("Ready")
        self.status_label.setFont(QFont("Arial", 11))
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status_label.setStyleSheet("color: #7f8c8d; padding: 6px;")
        layout.addWidget(self.status_label)

        # Distance display
        self.distance_label = QLabel("--")
        self.distance_label.setFont(QFont("Arial", 48, QFont.Weight.Bold))
        self.distance_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._set_result_style(self.distance_label, "#95a5a6")
        layout.addWidget(self.distance_label)

        self.unit_label = QLabel("")
        self.unit_label.setFont(QFont("Arial", 16))
        self.unit_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.unit_label.setStyleSheet("color: #27ae60; padding: 4px;")
        layout.addWidget(self.unit_label)

        self.delay_label = QLabel("")
        self.delay_label.setFont(QFont("Arial", 11))
        self.delay_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.delay_label.setStyleSheet("color: #7f8c8d; padding: 2px;")
        layout.addWidget(self.delay_label)

        # Tips box
        layout.addWidget(self._make_group(
            "Tips", "#fff9e6", "#f39c12",
            "✓ Quiet environment\n"
            "✓ Flat object at 1–2 m\n"
            "✓ Point device toward object\n"
            "✓ Hard surfaces work better"
        ))

        layout.addStretch()
        panel.setLayout(layout)
        return panel

    # ------------------------------------------------------------------
    def _build_right_panel(self):
        panel  = QWidget()
        layout = QVBoxLayout()
        layout.setSpacing(10)

        self.tab_widget = QTabWidget()
        self.tab_widget.setStyleSheet("""
            QTabWidget::pane {
                border: 2px solid #bdc3c7;
                border-radius: 5px;
                background: white;
            }
            QTabBar::tab {
                background: #ecf0f1;
                padding: 8px 16px;
                margin-right: 2px;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
            }
            QTabBar::tab:selected {
                background: #3498db;
                color: white;
            }
        """)

        self.chirp_curve, self.chirp_plot = self._make_plot(
            "Chirp Signal", "Emitted Chirp Signal", "Time (ms)", "Amplitude",
            pg.mkPen("#3498db", width=2)
        )
        self.tab_widget.addTab(self.chirp_plot, "Chirp Signal")

        self.audio_curve, self.audio_plot = self._make_plot(
            "Received Audio", "Received Audio (Echo)", "Time (ms)", "Amplitude",
            pg.mkPen("#27ae60", width=1)
        )
        self.tab_widget.addTab(self.audio_plot, "Received Audio")

        self.corr_curve, self.corr_plot = self._make_plot(
            "Autocorrelation", "Autocorrelation (Echo Detection)", "Time Delay (ms)", "Correlation",
            pg.mkPen("#e74c3c", width=2)
        )
        self.echo_marker = pg.ScatterPlotItem(size=10, brush=pg.mkBrush("#f39c12"))
        self.corr_plot.addItem(self.echo_marker)
        self.tab_widget.addTab(self.corr_plot, "Autocorrelation")

        self.spec_plot  = pg.PlotWidget()
        self.spec_plot.setBackground("w")
        self.spec_plot.setLabel("left", "Frequency (Hz)")
        self.spec_plot.setLabel("bottom", "Time (ms)")
        self.spec_plot.setTitle("Chirp Spectrogram", color="#2c3e50", size="12pt")
        self.spec_image = pg.ImageItem()
        self.spec_plot.addItem(self.spec_image)
        self.tab_widget.addTab(self.spec_plot, "Spectrogram")

        layout.addWidget(self.tab_widget)
        panel.setLayout(layout)
        return panel

    # ── Helpers ───────────────────────────────────────────────────────

    @staticmethod
    def _make_plot(tab_name, title, xlabel, ylabel, pen):
        plot = pg.PlotWidget()
        plot.setBackground("w")
        plot.setLabel("left", ylabel)
        plot.setLabel("bottom", xlabel)
        plot.setTitle(title, color="#2c3e50", size="12pt")
        plot.showGrid(x=True, y=True, alpha=0.3)
        curve = plot.plot(pen=pen)
        return curve, plot

    @staticmethod
    def _make_group(title, bg, border, text):
        group = QGroupBox(title)
        group.setFont(QFont("Arial", 10, QFont.Weight.Bold))
        group.setStyleSheet(f"""
            QGroupBox {{
                background-color: {bg};
                border: 2px solid {border};
                border-radius: 8px;
                margin-top: 10px;
                padding: 10px;
            }}
            QGroupBox::title {{
                color: #2c3e50;
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }}
        """)
        label = QLabel(text)
        label.setFont(QFont("Arial", 9))
        label.setStyleSheet("color: #34495e; background: transparent; border: none;")
        inner = QVBoxLayout()
        inner.addWidget(label)
        group.setLayout(inner)
        return group

    @staticmethod
    def _make_button(text, color, hover, pressed):
        btn = QPushButton(text)
        btn.setFont(QFont("Arial", 12, QFont.Weight.Bold))
        btn.setMinimumHeight(48)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {color};
                color: white;
                border-radius: 8px;
                padding: 10px;
            }}
            QPushButton:hover    {{ background-color: {hover}; }}
            QPushButton:pressed  {{ background-color: {pressed}; }}
            QPushButton:disabled {{ background-color: #95a5a6; color: #ecf0f1; }}
        """)
        return btn

    @staticmethod
    def _set_result_style(label, color):
        label.setStyleSheet(f"""
            background-color: {color};
            color: white;
            border-radius: 8px;
            padding: 20px;
            min-height: 80px;
        """)

    # ── Measurement flow ──────────────────────────────────────────────

    def start_measurement(self):
        if self.measurement_thread and self.measurement_thread.isRunning():
            return

        self.measure_btn.setEnabled(False)
        self.status_label.setText("Playing chirp...")
        self.status_label.setStyleSheet("color: #3498db; padding: 6px;")
        self.distance_label.setText("--")
        self._set_result_style(self.distance_label, "#95a5a6")
        self.unit_label.setText("")
        self.delay_label.setText("")

        self.measurement_thread = MeasurementThread(self.meter)
        self.measurement_thread.status_update.connect(self.update_status)
        self.measurement_thread.finished.connect(self.update_results)
        self.measurement_thread.start()

    def update_status(self, status):
        self.status_label.setText(status)

    def update_results(self, distance, time_delay, chirp, audio_data, correlation):
        self.measure_btn.setEnabled(True)

        self.last_chirp      = chirp
        self.last_audio      = audio_data
        self.last_distance   = distance
        self.last_time_delay = time_delay

        if chirp is not None and audio_data is not None:
            self.save_btn.setEnabled(True)

        self._update_plots(chirp, audio_data, correlation, distance)

        valid = distance is not None and self.meter.MIN_DIST <= distance <= self.meter.MAX_DIST
        if valid:
            self.status_label.setText("✓ Echo detected!")
            self.status_label.setStyleSheet("color: #27ae60; padding: 6px;")
            self.distance_label.setText(f"{distance:.2f}")
            self.unit_label.setText("meters")
            self._set_result_style(self.distance_label, "#2ecc71")
            delay_ms = time_delay * 1000 if time_delay is not None else None
            if delay_ms is not None:
                if delay_ms < 1:
                    self.delay_label.setText(f"⏱ Time delay: {delay_ms * 1000:.1f} μs")
                else:
                    self.delay_label.setText(f"⏱ Time delay: {delay_ms:.2f} ms")
        else:
            self.status_label.setText("No echo detected. Try again with object at 1–2 m.")
            self.status_label.setStyleSheet("color: #e74c3c; padding: 6px;")
            self.distance_label.setText("--")
            self.unit_label.setText("")
            self.delay_label.setText("")
            self._set_result_style(self.distance_label, "#e74c3c")

    # ── Plots ─────────────────────────────────────────────────────────

    def _update_plots(self, chirp, audio_data, correlation, distance):
        sr = self.meter.SAMPLE_RATE

        if chirp is not None:
            t_chirp = np.arange(len(chirp)) / sr * 1000
            self.chirp_curve.setData(t_chirp, chirp)
            try:
                from scipy import signal
                f, t, Sxx = signal.spectrogram(chirp, sr, nperseg=256)
                self.spec_image.setImage(
                    (10 * np.log10(Sxx + 1e-10)).T, autoLevels=True
                )
                self.spec_image.setRect(0, 0, t[-1] * 1000, f[-1])
            except ImportError:
                pass

        if audio_data is not None:
            t_audio = np.arange(len(audio_data)) / sr * 1000
            self.audio_curve.setData(t_audio, audio_data)

        if correlation is not None:
            t_corr = np.arange(len(correlation)) / sr * 1000
            self.corr_curve.setData(t_corr, correlation)

            # Shade the valid 1–2 m window
            for item in list(self.corr_plot.items()):
                if isinstance(item, pg.LinearRegionItem):
                    self.corr_plot.removeItem(item)
            min_ms = (2 * self.meter.MIN_DIST / self.meter.SPEED_OF_SOUND) * 1000
            max_ms = (2 * self.meter.MAX_DIST / self.meter.SPEED_OF_SOUND) * 1000
            region = pg.LinearRegionItem(
                values=[min_ms, max_ms],
                brush=pg.mkBrush(52, 152, 219, 40),
                movable=False
            )
            self.corr_plot.addItem(region)

            # Orange marker on the detected peak
            valid = distance is not None and self.meter.MIN_DIST <= distance <= self.meter.MAX_DIST
            if valid:
                peak_ms  = (2 * distance / self.meter.SPEED_OF_SOUND) * 1000
                peak_idx = int(peak_ms / 1000 * sr)
                if peak_idx < len(correlation):
                    self.echo_marker.setData([peak_ms], [correlation[peak_idx]])
            else:
                self.echo_marker.clear()

    # ── Save audio ────────────────────────────────────────────────────

    def save_audio(self):
        if self.last_chirp is None or self.last_audio is None:
            return

        timestamp    = datetime.now().strftime("%Y%m%d_%H%M%S")
        valid        = (
            self.last_distance is not None
            and self.meter.MIN_DIST <= self.last_distance <= self.meter.MAX_DIST
        )
        dist_str     = f"_{self.last_distance:.2f}m" if valid else ""
        default_name = f"chirp_measurement_{timestamp}{dist_str}"

        file_path, _ = QFileDialog.getSaveFileName(
            self, "Save Audio Recording", default_name,
            "WAV Files (*.wav);;All Files (*)"
        )
        if not file_path:
            return
        if not file_path.endswith(".wav"):
            file_path += ".wav"

        try:
            base = file_path[:-4]

            chirp_path = f"{base}_chirp.wav"
            wavfile.write(chirp_path, self.meter.SAMPLE_RATE,
                          (self.last_chirp * 32767).astype(np.int16))

            audio_path = f"{base}_received.wav"
            wavfile.write(audio_path, self.meter.SAMPLE_RATE,
                          (self.last_audio * 32767).astype(np.int16))

            meta_path = f"{base}_info.txt"
            with open(meta_path, "w") as f:
                f.write("Chirp Distance Measurement\n")
                f.write("=" * 40 + "\n")
                f.write(f"Timestamp    : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Sample Rate  : {self.meter.SAMPLE_RATE} Hz\n")
                f.write(f"Speed of Sound: {self.meter.SPEED_OF_SOUND} m/s\n\n")
                if valid:
                    f.write(f"Distance     : {self.last_distance:.2f} m\n")
                    if self.last_time_delay is not None:
                        td_ms = self.last_time_delay * 1000
                        f.write(f"Time Delay   : {td_ms:.2f} ms\n")
                        f.write(f"Round Trip   : {td_ms * 2:.2f} ms\n")
                    f.write(f"\nFormula      : d = ({self.meter.SPEED_OF_SOUND} × t) / 2\n")
                else:
                    f.write("Distance     : No echo detected\n")
                f.write(f"\nFiles:\n")
                f.write(f"  {os.path.basename(chirp_path)}\n")
                f.write(f"  {os.path.basename(audio_path)}\n")

            self.status_label.setText("✓ Audio saved successfully!")
            self.status_label.setStyleSheet("color: #27ae60; padding: 6px;")

        except Exception as e:
            self.status_label.setText(f"Save error: {e}")
            self.status_label.setStyleSheet("color: #e74c3c; padding: 6px;")

    # ── Cleanup ───────────────────────────────────────────────────────

    def closeEvent(self, event):
        if self.measurement_thread and self.measurement_thread.isRunning():
            self.measurement_thread.wait()
        self.meter.cleanup()
        event.accept()


# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    print("Sound Distance Measurement App")
    print("=" * 50)
    print("Requirements:")
    print("  pip install numpy pyaudio PyQt6 pyqtgraph scipy")
    print()
    print("Make sure your microphone and speakers are enabled!")
    print("=" * 50)

    app    = QApplication(sys.argv)
    window = DistanceMeterWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
