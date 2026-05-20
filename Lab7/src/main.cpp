/*
 * Lab 7 - Contactless Communication Security System
 *
 * State machine:
 *   WAITING  -> user types 4 digits + '#' on the keypad to set the lock code
 *   LOCKED   -> user enters the 4-digit code on the IR remote, confirm with OK/Power
 *   UNLOCKED -> RFID-RC522 listens for tags, UID is sent over Serial
 *
 * Keypad shortcuts:
 *   '#'  while UNLOCKED -> re-lock with the same code (back to LOCKED)
 *   '*'  in any state   -> full reset back to WAITING (clears the saved code)
 *
 * Required libraries (install via Library Manager):
 *   - Keypad           (Mark Stanley / Alexander Brevig)
 *   - IRremote         (Armin Joachimsmeyer, v3.x)
 *   - MFRC522          (GithubCommunity)
 */
#include <Arduino.h>
#include <Keypad.h>
#define DECODE_NEC
#include <IRremote.hpp>
#include <SPI.h>
#include <MFRC522.h>

// ---------------- Pin map ----------------
// Keypad 4x4 (membrane)
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {A0, A1, A2, A3};   // row pinouts
byte colPins[COLS] = {5, 4, 3, 2};       // column pinouts
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// IR receiver
const uint8_t IR_RECV_PIN = 6;

// LEDs (status indicators)
const uint8_t LED_RED   = 7;   // lit when LOCKED / blink in WAITING
const uint8_t LED_GREEN = 8;   // lit when UNLOCKED / blink in WAITING

// RFID-RC522 (SPI)
const uint8_t RFID_SS_PIN  = 10;
const uint8_t RFID_RST_PIN = 9;
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// ---------------- State ----------------
enum SysState { WAITING, LOCKED, UNLOCKED };
SysState state = WAITING;

const uint8_t CODE_LEN = 4;
char lockCode[CODE_LEN + 1] = "";   // set during WAITING, stays for the session
char inputBuf[CODE_LEN + 1] = "";
uint8_t inputLen = 0;

unsigned long lastBlinkMs = 0;
bool blinkOn = false;

// ---------------- Helpers ----------------
void clearInput() {
  inputLen = 0;
  inputBuf[0] = '\0';
}

void setLeds(bool red, bool green) {
  digitalWrite(LED_RED,   red   ? HIGH : LOW);
  digitalWrite(LED_GREEN, green ? HIGH : LOW);
}

void flashTagSuccess() {
  for (uint8_t i = 0; i < 3; i++) {
    setLeds(true, true);
    delay(80);
    setLeds(false, false);
    delay(80);
  }
  // restore unlocked pattern
  setLeds(false, true);
}

void announceState() {
  Serial.print("STATE:");
  switch (state) {
    case WAITING:  Serial.println("WAITING");  break;
    case LOCKED:   Serial.println("LOCKED");   break;
    case UNLOCKED: Serial.println("UNLOCKED"); break;
  }
}

void goWaiting() {
  state = WAITING;
  clearInput();
  lockCode[0] = '\0';
  announceState();
}

void goLocked() {
  state = LOCKED;
  clearInput();
  setLeds(true, false);
  announceState();
}

void goUnlocked() {
  state = UNLOCKED;
  clearInput();
  setLeds(false, true);
  announceState();
}

// ---------------- IR remote -> digit map ----------------
// Elegoo / generic NEC remote. The IRremote v3 library reports the command byte
// in IrReceiver.decodedIRData.command. Numeric keys 0-9 are below; we only care
// about digits and the OK/confirm key.
char irCommandToChar(uint8_t cmd) {
  switch (cmd) {
    case 0x16: return '0';
    case 0x0C: return '1';
    case 0x18: return '2';
    case 0x5E: return '3';
    case 0x08: return '4';
    case 0x1C: return '5';
    case 0x5A: return '6';
    case 0x42: return '7';
    case 0x52: return '8';
    case 0x4A: return '9';
    case 0x40: return '#';   // OK / confirm
    case 0x44: return '*';   // reset
    default:   return 0;
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  setLeds(false, false);

  // DISABLE_LED_FEEDBACK is critical: the default feedback LED is LED_BUILTIN
  // (pin 13), which is also the SPI SCK pin for the RC522. Toggling it on every
  // IR pulse corrupts the SPI bus.
  IrReceiver.begin(IR_RECV_PIN, DISABLE_LED_FEEDBACK);

  SPI.begin();
  rfid.PCD_Init();

  Serial.println("READY");
  announceState();
}

// ---------------- WAITING handler ----------------
void handleWaiting(char k) {
  if (!k) return;
  if (k >= '0' && k <= '9' && inputLen < CODE_LEN) {
    inputBuf[inputLen++] = k;
    inputBuf[inputLen]   = '\0';
    Serial.print("KEY:"); Serial.println(k);
  } else if (k == '#' && inputLen == CODE_LEN) {
    strcpy(lockCode, inputBuf);
    Serial.print("CODE_SET:"); Serial.println(lockCode);
    goLocked();
  } else if (k == '*') {
    clearInput();
    Serial.println("INPUT_CLEARED");
  }
}

// ---------------- LOCKED handler (IR remote) ----------------
void handleLockedIR(char k) {
  if (!k) return;
  if (k == '*') {
    goWaiting();
    return;
  }
  if (k >= '0' && k <= '9' && inputLen < CODE_LEN) {
    inputBuf[inputLen++] = k;
    inputBuf[inputLen]   = '\0';
    Serial.print("IR:"); Serial.println(k);
  } else if (k == '#' && inputLen == CODE_LEN) {
    if (strcmp(inputBuf, lockCode) == 0) {
      Serial.println("UNLOCK_OK");
      goUnlocked();
    } else {
      Serial.println("UNLOCK_FAIL");
      // quick fail flash
      for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(LED_RED, LOW);  delay(80);
        digitalWrite(LED_RED, HIGH); delay(80);
      }
      clearInput();
    }
  }
}

// ---------------- UNLOCKED handler (RFID) ----------------
void handleUnlockedRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  Serial.print("TAG:");
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  flashTagSuccess();
  rfid.PICC_HaltA();
}

// ---------------- LED patterns ----------------
void updateLeds() {
  if (state == WAITING) {
    // alternate red/green every 300 ms
    if (millis() - lastBlinkMs > 300) {
      lastBlinkMs = millis();
      blinkOn = !blinkOn;
      setLeds(blinkOn, !blinkOn);
    }
  }
  // LOCKED and UNLOCKED LED states are set on transition
}

// ---------------- Main loop ----------------
void loop() {
  updateLeds();

  // Keypad shortcuts that work across states
  char k = keypad.getKey();
  if (k == '*' && state != WAITING) {
    goWaiting();           // full reset, clears saved code
    return;
  }
  if (k == '#' && state == UNLOCKED) {
    Serial.println("RELOCK");
    goLocked();            // re-lock with the same code
    return;
  }

  switch (state) {
    case WAITING:
      handleWaiting(k);
      break;

    case LOCKED:
      // LOCKED accepts IR digits only; keypad is ignored except '*' (handled above)
      if (IrReceiver.decode()) {
        bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
        bool isKnown  = IrReceiver.decodedIRData.protocol != UNKNOWN;
        // Skip repeat frames (held buttons) and ambient-IR noise.
        if (!isRepeat && isKnown) {
          char ch = irCommandToChar((uint8_t)IrReceiver.decodedIRData.command);
          handleLockedIR(ch);
        }
        IrReceiver.resume();
      }
      break;

    case UNLOCKED:
      handleUnlockedRFID();
      break;
  }
}
