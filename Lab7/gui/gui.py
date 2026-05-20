"""
Lab 7 - Flask server for the contactless security system.
Reads serial messages from the Arduino and logs terminal events.
"""
import os
from flask import Flask, render_template, jsonify, request
import serial
import csv
import threading
import time
from collections import deque
from datetime import datetime

# --- Умный поиск папки templates для предотвращения ошибки TemplateNotFound ---
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)

if os.path.exists(os.path.join(current_dir, 'templates')):
    app = Flask(__name__)
elif os.path.exists(os.path.join(parent_dir, 'templates')):
    app = Flask(__name__, template_folder=os.path.join(parent_dir, 'templates'))
else:
    app = Flask(__name__)

# ---------------- Config ----------------
SERIAL_PORT = "COM17"      # Измените на ваш порт (например COM3 на Windows)
BAUD_RATE   = 115200
CSV_FILE    = os.path.join(current_dir, "tags.csv")
CSV_FIELDS  = ["id", "uid", "first_seen", "last_seen", "scan_count"]
MAX_EVENTS  = 50

# ---------------- Shared state ----------------
state_lock   = threading.Lock()
csv_lock     = threading.Lock()
system_state = "DISCONNECTED"
events       = deque(maxlen=MAX_EVENTS )
ser          = None

# ---------------- CSV-based tag store ----------------
def csv_init():
    """Create tags.csv with a header row if it doesn't exist yet."""
    if not os.path.exists(CSV_FILE):
        with open(CSV_FILE, "w", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow(CSV_FIELDS)

def read_tags():
    """Load all rows as a list of dicts. Returns [] if the file is empty."""
    if not os.path.exists(CSV_FILE):
        return []
    with open(CSV_FILE, "r", newline="", encoding="utf-8") as f:
        rows = []
        for r in csv.DictReader(f):
            r["id"] = int(r["id"])
            r["scan_count"] = int(r["scan_count"])
            rows.append(r)
        return rows

def write_tags(rows):
    """Rewrite the whole CSV (header + rows)."""
    with open(CSV_FILE, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        writer.writeheader()
        for r in rows:
            writer.writerow(r)

def record_tag(uid: str):
    """Insert a new tag or increment scan_count on an existing one."""
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with csv_lock:
        rows = read_tags()
        for r in rows:
            if r["uid"] == uid:
                r["last_seen"]  = now
                r["scan_count"] = r["scan_count"] + 1
                write_tags(rows)
                return "UPDATED"
        
        next_id = max((r["id"] for r in rows), default=0) + 1
        rows.append({
            "id":         next_id,
            "uid":        uid,
            "first_seen": now,
            "last_seen":  now,
            "scan_count": 1,
        })
        write_tags(rows)
        return "NEW"

# ---------------- Serial ----------------
def add_event(message: str):
    """Adds a message to the GUI event log."""
    with state_lock:
        events.appendleft({
            "time": datetime.now().strftime("%H:%M:%S"),
            "message": message,
        })

def connect_serial():
    global ser
    while True:
        try:
            msg = f"Trying {SERIAL_PORT} @ {BAUD_RATE}..."
            print(msg)
            add_event(f"Terminal: {msg}")  # Отправляем лог терминала в GUI
            
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            time.sleep(2)
            
            msg_ok = "Serial connected."
            print(msg_ok)
            add_event(f"Terminal: {msg_ok}") # Отправляем лог терминала в GUI
            
            with state_lock:
                global system_state
                system_state = "CONNECTED"
            return
        except Exception as e:
            msg_err = f"Serial connect failed: {e}"
            print(msg_err)
            add_event(f"Terminal Error: {msg_err}") # Отправляем лог терминала в GUI
            with state_lock:
                system_state = "DISCONNECTED"
            time.sleep(2)

def serial_reader():
    global ser, system_state
    connect_serial()
    while True:
        try:
            if ser is None or not ser.is_open:
                connect_serial()
                
            raw = ser.readline().decode("utf-8", errors="ignore").strip()
            if not raw:
                continue
                
            print("RX:", raw) # Вывод в консоль
            
            # Обработка команд Arduino
            if raw.startswith("STATE:"):
                with state_lock:
                    system_state = raw.split(":", 1)[1]
                add_event(f"State -> {system_state}")
            elif raw.startswith("TAG:"):
                uid = raw.split(":", 1)[1].upper()
                action = record_tag(uid)
                add_event(f"Tag {uid} ({action})")
            elif raw.startswith("CODE_SET:"):
                add_event("Lock code set")
            elif raw == "UNLOCK_OK":
                add_event("Unlock successful")
            elif raw == "UNLOCK_FAIL":
                add_event("Unlock failed - wrong code")
            elif raw == "READY":
                add_event("Arduino ready")
            elif raw == "INPUT_CLEARED":
                add_event("Input cleared")
            elif raw == "RELOCK":
                add_event("Re-locked (code retained)")
            else:
                # Если пришла какая-то другая информация (например дебаг), выводим ее в лог GUI
                add_event(f"Terminal RX: {raw}")

        except serial.SerialException as e:
            msg = f"Serial dropped: {e}"
            print(msg)
            add_event(f"Terminal: {msg}")
            with state_lock:
                system_state = "DISCONNECTED"
            time.sleep(1)
            connect_serial()
        except Exception as e:
            msg = f"Reader error: {e}"
            print(msg)
            add_event(f"Terminal: {msg}")
            time.sleep(0.2)

# ---------------- Routes ----------------
@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/status")
def api_status():
    with state_lock:
        return jsonify({
            "state": system_state,
            "events": list(events),
        })

@app.route("/api/tags")
def api_tags():
    rows = read_tags()
    rows.sort(key=lambda r: r["last_seen"], reverse=True)
    return jsonify(rows)

@app.route("/api/tags/clear", methods=["POST"])
def api_tags_clear():
    with csv_lock:
        write_tags([])
    add_event("Database cleared")
    return jsonify({"ok": True})

# ---------------- Main ----------------
if __name__ == "__main__":
    csv_init()
    threading.Thread(target=serial_reader, daemon=True).start()
    app.run(host="0.0.0.0", port=5000, debug=True, use_reloader=False)