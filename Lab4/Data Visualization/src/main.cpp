#include <Arduino.h>

// --- 1. Define Pins ---
const int pinSW = 2;   // Switch connected to Pin 2
const int pinX = A0;   // VRx connected to Analog 0
const int pinY = A1;   // VRy connected to Analog 1

// LED Pins
const int ledUp = 8;
const int ledDown = 7;
const int ledLeft = 10;
const int ledRight = 11;

// --- 2. Define Thresholds & Dead-zone ---
// Center is usually around 512.
// Dead-zone: 400 to 600 (Ignored to prevent jitter)
const int thresholdLow = 400;  
const int thresholdHigh = 600; 

void setup() {
  // Use 115200 for faster sampling speed (Task Requirement)
  Serial.begin(115200); 

  // Setup Joystick Switch
  // INPUT_PULLUP is crucial: it uses the Arduino's internal resistor.
  pinMode(pinSW, INPUT_PULLUP);

  // Setup LEDs
  pinMode(ledUp, OUTPUT);
  pinMode(ledDown, OUTPUT);
  pinMode(ledLeft, OUTPUT);
  pinMode(ledRight, OUTPUT);
}

void loop() {
  // Start timer for evidence of speed
  unsigned long startTime = micros();

  // --- READ INPUTS ---
  int valX = analogRead(pinX);
  int valY = analogRead(pinY);
  int valSW = digitalRead(pinSW); // Reads 0 if pressed, 1 if not

  // --- RESET LEDS ---
  digitalWrite(ledUp, LOW);
  digitalWrite(ledDown, LOW);
  digitalWrite(ledLeft, LOW);
  digitalWrite(ledRight, LOW);

  String direction = "CENTER";

  // --- DIRECTION LOGIC ---
  // Note: Adjust UP/DOWN if your joystick is inverted
  if (valY < thresholdLow) {
    digitalWrite(ledUp, HIGH);
    direction = "UP";
  } 
  else if (valY > thresholdHigh) {
    digitalWrite(ledDown, HIGH);
    direction = "DOWN";
  }

  if (valX < thresholdLow) {
    digitalWrite(ledLeft, HIGH);
    direction = "LEFT";
  } 
  else if (valX > thresholdHigh) {
    digitalWrite(ledRight, HIGH);
    direction = "RIGHT";
  }

  // --- SWITCH LOGIC ---
  // If valSW is LOW (0), the button is pressed
  if (valSW == LOW) {
    direction = direction + " + CLICK";
    // Optional: Flash all LEDs or do a specific action when clicked
    digitalWrite(ledUp, HIGH);
    digitalWrite(ledDown, HIGH);
    digitalWrite(ledLeft, HIGH);
    digitalWrite(ledRight, HIGH);
  }

// --- SERIAL OUTPUT (Format: X, Y, SW, LoopTime) ---
  Serial.print(valX);
  Serial.print(",");
  Serial.print(valY);
  Serial.print(",");
  Serial.print(valSW);
  Serial.print(",");
  
  // Evidence of Sampling Speed
  unsigned long endTime = micros();
  unsigned long loopTime = endTime - startTime;
  Serial.println(loopTime);
  
  // 10ms delay for stability
  delay(10); 
}
