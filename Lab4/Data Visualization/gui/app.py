import sys
import serial
import pyqtgraph as pg
from PyQt6.QtWidgets import QApplication, QMainWindow, QLabel, QPushButton, QVBoxLayout, QHBoxLayout, QWidget
from PyQt6.QtCore import QTimer, Qt

class JoystickGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lab Task 4: Joystick Data Visualization")
        self.resize(600, 500)
        
        self.serial_port = None
        self.port_name = 'COM30'
        self.baud_rate = 115200
        
        # --- UI Layout ---
        main_layout = QVBoxLayout()
        
        # 1. Top Bar: Start/Stop Button
        self.toggle_btn = QPushButton("Start Test")
        self.toggle_btn.clicked.connect(self.toggle_serial)
        self.toggle_btn.setMinimumHeight(40)
        main_layout.addWidget(self.toggle_btn)
        
        # 2. Status Labels (X, Y, Button, Sample Rate)
        status_layout = QHBoxLayout()
        self.lbl_x = QLabel("X Level: ---")
        self.lbl_y = QLabel("Y Level: ---")
        self.lbl_btn = QLabel("Button: ---")
        self.lbl_rate = QLabel("Sample Rate: --- Hz")
        
        for lbl in [self.lbl_x, self.lbl_y, self.lbl_btn, self.lbl_rate]:
            lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
            lbl.setStyleSheet("font-size: 14px; font-weight: bold;")
            status_layout.addWidget(lbl)
            
        main_layout.addLayout(status_layout)
        
        # 3. Data Visualization: 2D Plot for Joystick Position
        self.plot_widget = pg.PlotWidget(title="Joystick Position Mapping")
        self.plot_widget.setXRange(0, 1023) # Analog read range is 0-1023
        self.plot_widget.setYRange(0, 1023)
        self.plot_widget.showGrid(x=True, y=True, alpha=0.5)
        
        # The "dot" representing the joystick
        self.joystick_dot = self.plot_widget.plot([512], [512], pen=None, symbol='o', symbolSize=20, symbolBrush='r')
        main_layout.addWidget(self.plot_widget)
        
        # Set Main Layout
        container = QWidget()
        container.setLayout(main_layout)
        self.setCentralWidget(container)
        
        # Timer for polling Serial Data
        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)

    def toggle_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            self.timer.stop()
            self.toggle_btn.setText("Start Test")
            self.toggle_btn.setStyleSheet("background-color: none;")
        else:
            try:
                self.serial_port = serial.Serial(self.port_name, self.baud_rate, timeout=0.05)
                self.timer.start(10) # Poll every 10ms
                self.toggle_btn.setText("Stop Test")
                self.toggle_btn.setStyleSheet("background-color: #ffcccc;")
            except Exception as e:
                self.lbl_rate.setText("Error connecting to Port!")

    def read_serial(self):
        if self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    # Read and decode the line
                    line = self.serial_port.readline().decode('utf-8').strip()
                    data = line.split(',')
                    
                    if len(data) == 4:
                        x_val = int(data[0])
                        y_val = int(data[1])
                        sw_val = int(data[2])
                        loop_time_us = int(data[3])
                        
                        # Calculate Sample Rate (Hz) -> 1,000,000 us per second
                        sample_rate = 1000000 / loop_time_us if loop_time_us > 0 else 0
                        
                        # Update Labels
                        self.lbl_x.setText(f"X Level: {x_val}")
                        self.lbl_y.setText(f"Y Level: {y_val}")
                        btn_state = "PRESSED" if sw_val == 0 else "Released"
                        self.lbl_btn.setText(f"Button: {btn_state}")
                        self.lbl_rate.setText(f"Sample Rate: {int(sample_rate)} Hz")
                        
                        # Update Graph Position
                        self.joystick_dot.setData([x_val], [y_val])
                        
            except Exception as e:
                pass # Ignore serial garbage data

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = JoystickGUI()
    window.show()
    sys.exit(app.exec())