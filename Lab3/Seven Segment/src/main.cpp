#include <Wire.h>
#include <SevSeg.h>

SevSeg sevseg;

const byte SQW_PIN    = 2;
const byte BUTTON_PIN = 3;
const byte LED_PIN    = 13;

const byte segPins[]   = {4, 5, 6, 7, 8, 9, 10, 11};
const byte digitPins[] = {A3, A2, A1, A0};

const byte DS1307_ADDR = 0x68;

const byte TARGET = 10;
const unsigned long WIN_MS = 300;
const unsigned long DEBOUNCE_MS = 30;

#define DEBUG_TICK_LED 1
#define RUN_START_SELFTEST 1

volatile unsigned long lastTickMs = 0;
volatile bool tickFlag = false;

enum State { IDLE, RUNNING, RESULT };
State state = IDLE;

byte counterVal = 0;
unsigned long targetTickMs = 0;
bool resultSuccess = false;

bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

void onTick() {
  lastTickMs = millis();
  tickFlag = true;
}

void ds1307EnsureRunning() {
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write((byte)0x00);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDR, (byte)1);
  if (Wire.available()) {
    byte sec = Wire.read();
    if (sec & 0x80) {
      sec &= 0x7F;
      Wire.beginTransmission(DS1307_ADDR);
      Wire.write((byte)0x00);
      Wire.write(sec);
      Wire.endTransmission();
    }
  }
}

void ds1307SetSQW_1Hz() {
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write((byte)0x07);
  Wire.write((byte)0x10);
  Wire.endTransmission();
}

void showCount(byte v) {
  char buf[5] = "    ";
  if (v < 10) {
    buf[1] = char('0' + v);
    sevseg.setChars(buf);
  } else if (v == 10) {
    sevseg.setChars("10  ");
  } else {
    sevseg.setChars("----");
  }
}

void showResult(bool ok) {
  if (ok) sevseg.setChars("GOOD");
  else    sevseg.setChars("FAIL");
}

void resetToIdle() {
  state = IDLE;
  counterVal = 0;
  resultSuccess = false;
  digitalWrite(LED_PIN, LOW);
  showCount(0);
}

void displaySelfTest() {
  const char* patterns[] = {"8   ", " 8  ", "  8 ", "   8"};
  for (int r = 0; r < 2; r++) {
    for (int i = 0; i < 4; i++) {
      sevseg.setChars(patterns[i]);
      unsigned long t0 = millis();
      while (millis() - t0 < 400) {
        sevseg.refreshDisplay();
      }
    }
  }
  sevseg.setChars("----");
  unsigned long t1 = millis();
  while (millis() - t1 < 300) sevseg.refreshDisplay();
}

bool buttonPressedEvent() {
  bool reading = digitalRead(BUTTON_PIN);
  bool pressed = false;

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        pressed = true;
      }
    }
  }
  lastButtonState = reading;
  return pressed;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SQW_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, HIGH);
  delay(150);
  digitalWrite(LED_PIN, LOW);

  Wire.begin();
  ds1307EnsureRunning();
  ds1307SetSQW_1Hz();

  bool resistorsOnSegments = true;
  byte hardwareConfig = COMMON_CATHODE;

  sevseg.begin(hardwareConfig, 4, (byte*)digitPins, (byte*)segPins, resistorsOnSegments);
  sevseg.setBrightness(70);

#if RUN_START_SELFTEST
  displaySelfTest();
#endif

  attachInterrupt(digitalPinToInterrupt(SQW_PIN), onTick, FALLING);
  resetToIdle();
}

void loop() {
  sevseg.refreshDisplay();

  if (tickFlag) {
    noInterrupts();
    tickFlag = false;
    unsigned long tickMsCopy = lastTickMs;
    interrupts();

    if (state == RUNNING) {
#if DEBUG_TICK_LED
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
#endif

      if (counterVal < TARGET) {
        counterVal++;
        showCount(counterVal);

        if (counterVal == TARGET) {
          targetTickMs = tickMsCopy;
        }
      } else {
        resultSuccess = false;
        state = RESULT;
        digitalWrite(LED_PIN, LOW);
        showResult(false);
      }
    }
  }

  if (buttonPressedEvent()) {
    if (state == IDLE) {
      counterVal = 0;
      showCount(0);
      digitalWrite(LED_PIN, LOW);
      state = RUNNING;
    }
    else if (state == RUNNING) {
      unsigned long nowMs = millis();
      unsigned long diff = 9999;
      if (counterVal == TARGET) {
        diff = nowMs - targetTickMs;
      }
      else if (counterVal == TARGET - 1) {
        unsigned long expectedTarget = lastTickMs + 1000;
        if (expectedTarget > nowMs) {
          diff = expectedTarget - nowMs;
        } else {
          diff = nowMs - expectedTarget;
        }
      }

      resultSuccess = (diff <= WIN_MS);
      state = RESULT;

      digitalWrite(LED_PIN, resultSuccess ? HIGH : LOW);
      showResult(resultSuccess);
    }
    else if (state == RESULT) {
      resetToIdle();
    }
  }
}