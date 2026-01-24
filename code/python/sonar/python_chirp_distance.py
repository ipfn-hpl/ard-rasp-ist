#!/usr/bin/env python3
"""
Sound Distance Measurement using Chirp Sonar
Measures distance to objects by emitting a chirp and detecting the echo
"""

import numpy as np
import pyaudio
import sys
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QPushButton, QLabel, QFrame, QGroupBox, QTabWidget)
from PyQt6.QtCore import Qt, QThread, pyqtSignal
from PyQt6.QtGui import QFont, QPalette, QColor
import pyqtgraph as pg


class MeasurementThread(QThread):
    """Thread for performing distance measurement without blocking UI"""
    finished = pyqtSignal(object, object, object, object, object)
    status_update = pyqtSignal(str)
    
    def __init__(self, meter):
        super().__init__()
        self.meter = meter
    
    def run(self):
        self.status_update.emit("Playing chirp...")
        distance, time_delay, chirp, audio_data, correlation = self.meter.measure_distance()
        self.finished.emit(distance, time_delay, chirp, audio_data, correlation)


class ChirpDistanceMeter:
    def __init__(self):
        self.SAMPLE_RATE = 44100
        self.CHUNK = 1024
        self.SPEED_OF_SOUND = 343  # m/s at 20°C
        
        self.audio = pyaudio.PyAudio()
        self.is_measuring = False
        
    def generate_chirp(self, duration=0.05, f_start=2000, f_end=4000):
        """Generate a linear chirp signal"""
        t = np.linspace(0, duration, int(self.SAMPLE_RATE * duration))
        chirp = np.sin(2 * np.pi * (f_start * t + (f_end - f_start) * t**2 / (2 * duration)))
        envelope = np.hanning(len(chirp))
        chirp = chirp * envelope * 0.3
        return chirp.astype(np.float32)
    
    def detect_echo(self, audio_data, chirp):
        """Detect echo using cross-correlation"""
        audio_data = audio_data / (np.max(np.abs(audio_data)) + 1e-10)
        chirp_norm = chirp / (np.max(np.abs(chirp)) + 1e-10)
        
        correlation = np.correlate(audio_data, chirp_norm, mode='full')
        correlation = correlation[len(chirp_norm):]
        
        threshold = np.max(correlation) * 0.3
        skip_samples = int(0.02 * self.SAMPLE_RATE)
        
        peaks = []
        for i in range(skip_samples, len(correlation) - 100):
            if correlation[i] > threshold:
                if correlation[i] == np.max(correlation[i-50:i+50]):
                    peaks.append(i)
        
        if peaks:
            echo_index = peaks[0]
            time_delay = echo_index / self.SAMPLE_RATE
            distance = (self.SPEED_OF_SOUND * time_delay) / 2
            return distance, time_delay, correlation
        
        return None, None, correlation
    
    def measure_distance(self):
        """Perform a complete distance measurement"""
        try:
            chirp = self.generate_chirp()
            
            stream_out = self.audio.open(
                format=pyaudio.paFloat32,
                channels=1,
                rate=self.SAMPLE_RATE,
                output=True
            )
            
            stream_in = self.audio.open(
                format=pyaudio.paFloat32,
                channels=1,
                rate=self.SAMPLE_RATE,
                input=True,
                frames_per_buffer=self.CHUNK
            )
            
            stream_out.write(chirp.tobytes())
            stream_out.stop_stream()
            stream_out.close()
            
            frames = []
            num_chunks = int(self.SAMPLE_RATE * 0.5 / self.CHUNK)
            
            for _ in range(num_chunks):
                data = stream_in.read(self.CHUNK, exception_on_overflow=False)
                frames.append(np.frombuffer(data, dtype=np.float32))
            
            stream_in.stop_stream()
            stream_in.close()
            
            audio_data = np.concatenate(frames)
            distance, time_delay, correlation = self.detect_echo(audio_data, chirp)
            
            return distance, time_delay, chirp, audio_data, correlation
            
        except Exception as e:
            return None, str(e), None, None, None
    
    def cleanup(self):
        """Cleanup audio resources"""
        self.audio.terminate()


class DistanceMeterWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.meter = ChirpDistanceMeter()
        self.measurement_thread = None
        self.init_ui()
        
    def init_ui(self):
        self.setWindowTitle("Chirp Distance Meter")
        self.setFixedSize(900, 800)
        
        # Central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Main layout
        main_layout = QHBoxLayout()
        main_layout.setSpacing(20)
        main_layout.setContentsMargins(20, 20, 20, 20)
        
        # Left panel - controls
        left_panel = QWidget()
        left_layout = QVBoxLayout()
        left_layout.setSpacing(20)
        
        # Title
        title = QLabel("🔊 Sound Distance Meter")
        title.setFont(QFont("Arial", 20, QFont.Weight.Bold))
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("color: #2c3e50; margin-bottom: 10px;")
        left_layout.addWidget(title)
        
        # Info group
        info_group = QGroupBox("How it works")
        info_group.setFont(QFont("Arial", 10, QFont.Weight.Bold))
        info_group.setStyleSheet("""
            QGroupBox {
                background-color: #e8f4f8;
                border: 2px solid #3498db;
                border-radius: 8px;
                margin-top: 10px;
                padding: 10px;
            }
            QGroupBox::title {
                color: #2c3e50;
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
        """)
        
        info_layout = QVBoxLayout()
        info_text = QLabel(
            "• Emits chirp (2-4 kHz)\n"
            "• Listens for echo\n"
            "• Range: 0.3m - 3m\n"
            "• Use hard surfaces"
        )
        info_text.setFont(QFont("Arial", 9))
        info_text.setStyleSheet("color: #34495e; background: transparent; border: none;")
        info_layout.addWidget(info_text)
        info_group.setLayout(info_layout)
        left_layout.addWidget(info_group)
        
        # Measure button
        self.measure_btn = QPushButton("▶ Start Measurement")
        self.measure_btn.setFont(QFont("Arial", 12, QFont.Weight.Bold))
        self.measure_btn.setMinimumHeight(50)
        self.measure_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.measure_btn.setStyleSheet("""
            QPushButton {
                background-color: #3498db;
                color: white;
                border-radius: 8px;
                padding: 10px;
            }
            QPushButton:hover {
                background-color: #2980b9;
            }
            QPushButton:pressed {
                background-color: #21618c;
            }
            QPushButton:disabled {
                background-color: #95a5a6;
                color: #ecf0f1;
            }
        """)
        self.measure_btn.clicked.connect(self.start_measurement)
        left_layout.addWidget(self.measure_btn)
        
        # Status label
        self.status_label = QLabel("Ready")
        self.status_label.setFont(QFont("Arial", 11))
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status_label.setStyleSheet("color: #7f8c8d; padding: 8px;")
        left_layout.addWidget(self.status_label)
        
        # Results frame
        results_frame = QFrame()
        results_layout = QVBoxLayout()
        results_layout.setSpacing(8)
        
        self.distance_label = QLabel("--")
        self.distance_label.setFont(QFont("Arial", 48, QFont.Weight.Bold))
        self.distance_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.distance_label.setStyleSheet("""
            background-color: #95a5a6;
            color: white;
            border-radius: 8px;
            padding: 20px;
            min-height: 80px;
        """)
        results_layout.addWidget(self.distance_label)
        
        self.unit_label = QLabel("")
        self.unit_label.setFont(QFont("Arial", 16))
        self.unit_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.unit_label.setStyleSheet("color: #27ae60; padding: 5px;")
        results_layout.addWidget(self.unit_label)
        
        results_frame.setLayout(results_layout)
        left_layout.addWidget(results_frame)
        
        # Tips group
        tips_group = QGroupBox("Tips")
        tips_group.setFont(QFont("Arial", 9, QFont.Weight.Bold))
        tips_group.setStyleSheet("""
            QGroupBox {
                background-color: #fff9e6;
                border: 2px solid #f39c12;
                border-radius: 8px;
                margin-top: 10px;
                padding: 10px;
            }
            QGroupBox::title {
                color: #2c3e50;
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
        """)
        
        tips_layout = QVBoxLayout()
        tips_text = QLabel(
            "✓ Quiet environment\n"
            "✓ Flat object nearby\n"
            "✓ Point toward object\n"
            "✓ Hard surfaces better"
        )
        tips_text.setFont(QFont("Arial", 8))
        tips_text.setStyleSheet("color: #7f8c8d; background: transparent; border: none;")
        tips_layout.addWidget(tips_text)
        tips_group.setLayout(tips_layout)
        left_layout.addWidget(tips_group)
        
        left_layout.addStretch()
        left_panel.setLayout(left_layout)
        left_panel.setMaximumWidth(320)
        
        # Right panel - plots
        right_panel = QWidget()
        right_layout = QVBoxLayout()
        right_layout.setSpacing(10)
        
        # Tab widget for different plots
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
        
        # Chirp signal plot
        self.chirp_plot = pg.PlotWidget()
        self.chirp_plot.setBackground('w')
        self.chirp_plot.setLabel('left', 'Amplitude')
        self.chirp_plot.setLabel('bottom', 'Time (ms)')
        self.chirp_plot.setTitle('Emitted Chirp Signal', color='#2c3e50', size='12pt')
        self.chirp_plot.showGrid(x=True, y=True, alpha=0.3)
        self.chirp_curve = self.chirp_plot.plot(pen=pg.mkPen('#3498db', width=2))
        self.tab_widget.addTab(self.chirp_plot, "Chirp Signal")
        
        # Received audio plot
        self.audio_plot = pg.PlotWidget()
        self.audio_plot.setBackground('w')
        self.audio_plot.setLabel('left', 'Amplitude')
        self.audio_plot.setLabel('bottom', 'Time (ms)')
        self.audio_plot.setTitle('Received Audio (Echo)', color='#2c3e50', size='12pt')
        self.audio_plot.showGrid(x=True, y=True, alpha=0.3)
        self.audio_curve = self.audio_plot.plot(pen=pg.mkPen('#27ae60', width=1))
        self.tab_widget.addTab(self.audio_plot, "Received Audio")
        
        # Correlation plot
        self.corr_plot = pg.PlotWidget()
        self.corr_plot.setBackground('w')
        self.corr_plot.setLabel('left', 'Correlation')
        self.corr_plot.setLabel('bottom', 'Time Delay (ms)')
        self.corr_plot.setTitle('Cross-Correlation (Echo Detection)', color='#2c3e50', size='12pt')
        self.corr_plot.showGrid(x=True, y=True, alpha=0.3)
        self.corr_curve = self.corr_plot.plot(pen=pg.mkPen('#e74c3c', width=2))
        self.echo_marker = pg.ScatterPlotItem(size=10, brush=pg.mkBrush('#f39c12'))
        self.corr_plot.addItem(self.echo_marker)
        self.tab_widget.addTab(self.corr_plot, "Correlation")
        
        # Spectrogram
        self.spec_plot = pg.PlotWidget()
        self.spec_plot.setBackground('w')
        self.spec_plot.setLabel('left', 'Frequency (Hz)')
        self.spec_plot.setLabel('bottom', 'Time (ms)')
        self.spec_plot.setTitle('Chirp Spectrogram', color='#2c3e50', size='12pt')
        self.spec_image = pg.ImageItem()
        self.spec_plot.addItem(self.spec_image)
        self.tab_widget.addTab(self.spec_plot, "Spectrogram")
        
        right_layout.addWidget(self.tab_widget)
        right_panel.setLayout(right_layout)
        
        # Add panels to main layout
        main_layout.addWidget(left_panel)
        main_layout.addWidget(right_panel, stretch=1)
        
        central_widget.setLayout(main_layout)
        
        # Set window background
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor("#f0f0f0"))
        self.setPalette(palette)
    
    def start_measurement(self):
        if self.measurement_thread and self.measurement_thread.isRunning():
            return
        
        self.measure_btn.setEnabled(False)
        self.status_label.setText("Playing chirp...")
        self.status_label.setStyleSheet("color: #3498db; padding: 8px;")
        self.distance_label.setText("--")
        self.distance_label.setStyleSheet("""
            background-color: #95a5a6;
            color: white;
            border-radius: 8px;
            padding: 20px;
            min-height: 80px;
        """)
        self.unit_label.setText("")
        
        self.measurement_thread = MeasurementThread(self.meter)
        self.measurement_thread.finished.connect(self.update_results)
        self.measurement_thread.status_update.connect(self.update_status)
        self.measurement_thread.start()
    
    def update_status(self, status):
        self.status_label.setText(status)
    
    def update_plots(self, chirp, audio_data, correlation, distance):
        """Update all visualization plots"""
        sample_rate = self.meter.SAMPLE_RATE
        
        if chirp is not None:
            # Chirp signal plot
            time_chirp = np.arange(len(chirp)) / sample_rate * 1000  # in ms
            self.chirp_curve.setData(time_chirp, chirp)
            
            # Spectrogram of chirp
            try:
                from scipy import signal
                f, t, Sxx = signal.spectrogram(chirp, sample_rate, nperseg=256)
                self.spec_image.setImage(10 * np.log10(Sxx + 1e-10).T, 
                                        autoLevels=True)
                self.spec_image.setRect(0, 0, t[-1] * 1000, f[-1])
            except ImportError:
                pass
        
        if audio_data is not None:
            # Received audio plot
            time_audio = np.arange(len(audio_data)) / sample_rate * 1000
            self.audio_curve.setData(time_audio, audio_data)
        
        if correlation is not None:
            # Correlation plot
            time_corr = np.arange(len(correlation)) / sample_rate * 1000
            self.corr_curve.setData(time_corr, correlation)
            
            # Mark echo peak if distance detected
            if distance is not None and 0.1 < distance < 10:
                time_delay = (2 * distance) / self.meter.SPEED_OF_SOUND
                peak_index = int(time_delay * sample_rate)
                if peak_index < len(correlation):
                    self.echo_marker.setData([time_delay * 1000], [correlation[peak_index]])
            else:
                self.echo_marker.clear()
    
    def update_results(self, distance, time_delay, chirp, audio_data, correlation):
        self.measure_btn.setEnabled(True)
        
        # Update plots
        self.update_plots(chirp, audio_data, correlation, distance)
        
        if distance is not None and 0.1 < distance < 10:
            self.status_label.setText("✓ Echo detected!")
            self.status_label.setStyleSheet("color: #27ae60; padding: 8px;")
            
            if distance < 1:
                self.distance_label.setText(f"{distance * 100:.1f}")
                self.unit_label.setText("centimeters")
            else:
                self.distance_label.setText(f"{distance:.2f}")
                self.unit_label.setText("meters")
            
            self.distance_label.setStyleSheet("""
                background-color: #2ecc71;
                color: white;
                border-radius: 8px;
                padding: 20px;
                min-height: 80px;
            """)
        else:
            self.status_label.setText("No echo detected. Try again with object nearby.")
            self.status_label.setStyleSheet("color: #e74c3c; padding: 8px;")
            self.distance_label.setText("--")
            self.distance_label.setStyleSheet("""
                background-color: #e74c3c;
                color: white;
                border-radius: 8px;
                padding: 20px;
                min-height: 80px;
            """)
    
    def closeEvent(self, event):
        if self.measurement_thread and self.measurement_thread.isRunning():
            self.measurement_thread.wait()
        self.meter.cleanup()
        event.accept()


def main():
    print("Sound Distance Measurement App with Visualization")
    print("=" * 50)
    print("Requirements:")
    print("  pip install numpy pyaudio PyQt6 pyqtgraph scipy")
    print()
    print("Make sure your microphone and speakers are enabled!")
    print("=" * 50)
    print()
    
    app = QApplication(sys.argv)
    window = DistanceMeterWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
