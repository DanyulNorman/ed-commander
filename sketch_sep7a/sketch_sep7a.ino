#include <Joystick.h>
#include <Encoder.h>

// -----------------------------
// CONFIG
// -----------------------------
#define TEST_595 false  // true = all LEDs on for testing, false = normal operation

// -----------------------------
// MATRIX SETUP (5x5, 24 usable)
// -----------------------------
const byte rowPins[5] = {A3, A2, A1, A0, 15};
const byte colPins[5] = {14, 16, 10, 9, 8};
bool matrixState[5][5];

// -----------------------------
// SHIFT REGISTER SETUP (2x 74HC165)
// -----------------------------
#define SHIFT_CP   2
#define SHIFT_Q7   3
#define SHIFT_PL   4
const int numShiftRegs = 2;
const int numShiftPins = numShiftRegs * 8; // 16 inputs total
bool shiftState[numShiftPins];

// -----------------------------
// DIRECT ENCODER SETUP
// -----------------------------
#define ENC_A 0   // D0
#define ENC_B 1   // D1
#define ENC_CW_BTN 38
#define ENC_CCW_BTN 39

Encoder myEnc(ENC_A, ENC_B);
long lastEncPos = 0;

// -----------------------------
// 595 LED DRIVER SETUP
// -----------------------------
#define LED_DATA   5   // DS
#define LED_LATCH  6   // STCP
#define LED_CLOCK  7   // SHCP

void setup595() {
  pinMode(LED_DATA, OUTPUT);
  pinMode(LED_LATCH, OUTPUT);
  pinMode(LED_CLOCK, OUTPUT);
  // Make sure OE = LOW, SRCLR = HIGH physically!
}

// Write a byte to the 595
void write595(byte data) {
  digitalWrite(LED_LATCH, LOW);
  shiftOut(LED_DATA, LED_CLOCK, MSBFIRST, data);
  digitalWrite(LED_LATCH, HIGH);
}

// -----------------------------
// TEST FUNCTION: all outputs HIGH
// -----------------------------
void test595() {
  write595(B11111111); // all 8 outputs HIGH
}

// -----------------------------
// JOYSTICK SETUP
// -----------------------------
Joystick_ Joystick(
  JOYSTICK_DEFAULT_REPORT_ID,
  JOYSTICK_TYPE_GAMEPAD,
  40, // 24 matrix + 14 shift buttons + 2 encoder buttons
  0,
  false,false,false,false,false,false,
  false,false,false,false
);

// -----------------------------
// LED STATE & BLINK HANDLING
// -----------------------------
byte ledState = 0;                  // current LED bits

enum LedMode {OFF, ON, BLINK};
LedMode ledMode[8] = {OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF};

bool blinkOn[8] = {false};          // current on/off state of blinking LEDs
unsigned long lastBlink = 0;
const unsigned long BLINK_INTERVAL = 500; // ms



void updateLeds() {
  write595(ledState);
}

void setLedOn(int idx) {
  ledMode[idx] = ON;
  ledState |= (1 << idx);
  blinkOn[idx] = false;
  updateLeds();
}

void setLedOff(int idx) {
  ledMode[idx] = OFF;
  ledState &= ~(1 << idx);
  blinkOn[idx] = false;
  updateLeds();
}

void startBlink(int idx) {
  ledMode[idx] = BLINK;
  blinkOn[idx] = false; // start off
}


// -----------------------------
void setup() {
  // Matrix
  for (byte r=0;r<5;r++){
    pinMode(rowPins[r],OUTPUT);
    digitalWrite(rowPins[r],HIGH);
  }
  for (byte c=0;c<5;c++){
    pinMode(colPins[c],INPUT_PULLUP);
  }

  // Shift registers
  pinMode(SHIFT_CP, OUTPUT);
  pinMode(SHIFT_PL, OUTPUT);
  pinMode(SHIFT_Q7, INPUT);

  // Encoder pins
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  setup595();       // initialize 595 pins
  bootSequence();   // power-on LED blink sequence

  // Initialize all LEDs to OFF
  for (int i = 0; i < 8; i++) {
    setLedOff(i);
  }
  // Joystick
  Joystick.begin();

  // Serial
  Serial.begin(115200);
}

// -----------------------------
void readMatrix(){
  for(byte r=0;r<5;r++){
    digitalWrite(rowPins[r],LOW);
    delayMicroseconds(5);
    for(byte c=0;c<5;c++){
      matrixState[r][c] = !digitalRead(colPins[c]); // pressed = LOW
    }
    digitalWrite(rowPins[r],HIGH);
  }
}

// -----------------------------
void readShiftRegs(){
  digitalWrite(SHIFT_PL, LOW);
  delayMicroseconds(5);
  digitalWrite(SHIFT_PL, HIGH);

  for(int i=0;i<numShiftPins;i++){
    shiftState[i] = digitalRead(SHIFT_Q7); // 0 = idle, 1 = pressed
    digitalWrite(SHIFT_CP,HIGH);
    delayMicroseconds(2);
    digitalWrite(SHIFT_CP,LOW);
  }
}

// -----------------------------
void handleEncoder() {
  long encPos = myEnc.read() / 4; // adjust divisor to get desired sensitivity
  if(encPos != lastEncPos){
    if(encPos > lastEncPos){
      // CW
      Joystick.setButton(ENC_CW_BTN, 1);
      delay(5);
      Joystick.setButton(ENC_CW_BTN, 0);
    } else {
      // CCW
      Joystick.setButton(ENC_CCW_BTN, 1);
      delay(5);
      Joystick.setButton(ENC_CCW_BTN, 0);
    }
    lastEncPos = encPos;
  }
}

// -----------------------------
void bootSequence() {
  // Blink all LEDs 3 times
  for(int i=0; i<3; i++){
    write595(B11111111); // all LEDs on
    delay(300);          // on 300ms
    write595(B00000000); // all LEDs off
    delay(300);          // off 300ms
  }
  write595(B00000000); // make sure all off
}

// -----------------------------
void loop(){
  // Read inputs
  readMatrix();
  readShiftRegs();
  handleEncoder();

  // Map matrix buttons 0–23
  int btnIndex = 0;
  for(byte r=0;r<5;r++){
    for(byte c=0;c<5;c++){
      if(!(r==4 && c==4)){
        Joystick.setButton(btnIndex, matrixState[r][c]);
        btnIndex++;
      }
    }
  }

  // Map shift buttons 24–37
  int shiftBtnIndex = 24;
  for(int i=0;i<numShiftPins;i++){
    Joystick.setButton(shiftBtnIndex, shiftState[i]);
    shiftBtnIndex++;
  }

  // --- Serial LED handling ---
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("ON:")) {
      int idx = line.substring(3).toInt();
      setLedOn(idx);
    } else if (line.startsWith("OFF:")) {
      int idx = line.substring(4).toInt();
      setLedOff(idx);
    } else if (line.startsWith("BLINK:")) {
      int idx = line.substring(6).toInt();
      startBlink(idx);
    }
  }

// --- Blink handler ---
if (millis() - lastBlink >= BLINK_INTERVAL) {
  lastBlink = millis();

  for (int i = 0; i < 8; i++) {
    if (ledMode[i] == BLINK) {
      blinkOn[i] = !blinkOn[i];
      if (blinkOn[i])
        ledState |= (1 << i);
      else
        ledState &= ~(1 << i);
    }
  }
  updateLeds();
}


  // --- Fallback test mode ---
  if (TEST_595) test595();

  delay(5);
}
