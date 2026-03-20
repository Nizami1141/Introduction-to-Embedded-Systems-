import sys #python interaction 
import time
import serial

from PyQt6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QLabel, QFrame, QProgressBar
)
from PyQt6.QtCore import QTimer, Qt
from PyQt6.QtGui import QFont

# --- БЕЗОПАСНОЕ ПОДКЛЮЧЕНИЕ К COM-ПОРТУ ---
try:
    ser = serial.Serial("COM31", 115200, timeout=1)
    time.sleep(2) #pause for 2 seconds, because arduino may reboot
except serial.SerialException as e:
    print(f"\n[ОШИБКА] Не удалось открыть порт COM31: {e}")
    print("Убедитесь, что порт не занят другой программой (например, Serial Monitor в Arduino IDE).")
    print("Закройте фоновые процессы Python и попробуйте снова.\n")
    sys.exit(1) # Завершаем программу, если порт недоступен

class SerialGUI(QWidget):
    def __init__(self): # self refers to our gui window
        super().__init__()#creates the basic widget behavior

        self.setWindowTitle("Lab 5 Sound Monitor")
        self.resize(500, 420)

        layout = QVBoxLayout()
        layout.setSpacing(15)
        layout.setContentsMargins(20, 20, 20, 20)

        title = QLabel("Sound Sensor Monitor")
        title.setFont(QFont("Arial", 18, QFont.Weight.Bold))
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title) #display the title from the QLabel

        self.level_label = QLabel("Level: -") # without self, level_label would be a temporary local variable
        self.threshold_label = QLabel("Threshold: -") # using self to use it outside of init
        self.events_label = QLabel("Events: -")
        self.db_label = QLabel("dB: -")

        for lbl in [
            self.level_label,
            self.threshold_label,
            self.events_label,
            self.db_label
        ]:
            lbl.setFont(QFont("Arial", 13))
            layout.addWidget(lbl) #addWidget => function, layout is a variable name created in QVBoxLayout

        self.bar_title = QLabel("Live Sound Level")
        self.bar_title.setFont(QFont("Arial", 13, QFont.Weight.Bold))
        layout.addWidget(self.bar_title)

        self.level_bar = QProgressBar()
        self.level_bar.setMinimum(0)
        self.level_bar.setMaximum(10)
        self.level_bar.setValue(0)
        self.level_bar.setFormat("%v") #displays the numeric value
        self.level_bar.setMinimumHeight(28)
        layout.addWidget(self.level_bar)

        self.status_box = QLabel("QUIET")
        self.status_box.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status_box.setFont(QFont("Arial", 20, QFont.Weight.Bold))
        self.status_box.setFrameShape(QFrame.Shape.Box)
        self.status_box.setMinimumHeight(90)
        self.status_box.setStyleSheet(
            "background-color: lightgreen; color: black; border: 2px solid black;"
        )
        layout.addWidget(self.status_box)

        self.setLayout(layout) #attaches vertical layout to our gui

        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(100) #every 1ooms timer emit timeout and read_serial runs

    def read_serial(self):
        if ser.in_waiting > 0: #ser is python's connection to arduino (com17, 115200....)
            line = ser.readline().decode(errors="ignore").strip() #strip removes \n, decode bytes into a string

            if line:
                parts = line.split(",")

                if len(parts) == 5: # 5 fields (level, db, treshold, above, events)
                    level = parts[0].strip()
                    db = parts[1].strip()
                    threshold = parts[2].strip()
                    above = parts[3].strip()
                    events = parts[4].strip()

                    self.level_label.setText(level) #settext to change the text displayed by QLabel
                    self.threshold_label.setText(threshold)
                    self.events_label.setText(events)
                    self.db_label.setText(db)

                    try:
                        level_value = int(level.split("=")[1]) #"Level", "8", 1=> take the second part
                        threshold_value = int(threshold.split("=")[1])

                        if level_value < 0:
                            level_value = 0
                        if level_value > 10:
                            level_value = 10

                        self.level_bar.setValue(level_value)
                        if level_value >= threshold_value:
                            self.level_bar.setStyleSheet("""
                                QProgressBar {
                                    border: 2px solid gray;
                                    border-radius: 5px;
                                    text-align: center;
                                }
                                QProgressBar::chunk {
                                    background-color: red;
                                }
                            """)
                        else:
                            self.level_bar.setStyleSheet("""
                                QProgressBar {
                                    border: 2px solid gray;
                                    border-radius: 5px;
                                    text-align: center;
                                }
                                QProgressBar::chunk {
                                    background-color: lightgreen;
                                }
                            """)
                    except:
                        pass

                if "Above=1" in above:
                    self.status_box.setText("LOUD")
                    self.status_box.setStyleSheet(
                        "background-color: red; color: white; border: 2px solid black;"
                    )
                else:
                    self.status_box.setText("QUIET")
                    self.status_box.setStyleSheet(
                        "background-color: lightgreen; color: black; border: 2px solid black;"
                    )

    def closeEvent(self, event):
        self.timer.stop()
        if 'ser' in globals() and ser.is_open: # Добавлена проверка на существование ser
            ser.close()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv) # standard way to create app
    window = SerialGUI()
    window.show()
    sys.exit(app.exec())