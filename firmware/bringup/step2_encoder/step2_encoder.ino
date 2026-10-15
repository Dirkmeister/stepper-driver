/*
  Step 2: OLED + encoder
  Wiring (new): Encoder A->D2, C->GND, B->D3, switch->D4 and GND
  Board: Arduino Uno   Serial Monitor: 115200 baud
*/
#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

const uint8_t PIN_ENC_A  = 2;
const uint8_t PIN_ENC_B  = 3;
const uint8_t PIN_ENC_SW = 4;

// Quadrature decoder: every valid A/B change adds +1 or -1, bounce cancels out
volatile int16_t encRaw = 0;
volatile uint8_t encPrev = 0;
const int8_t QEM[16] = {0,-1, 1, 0,  1, 0, 0,-1,  -1, 0, 0, 1,  0, 1,-1, 0};

void encISR() {
  uint8_t s = (PIND >> 2) & 0x03;   // bit0 = A (D2), bit1 = B (D3)
  encRaw += QEM[(encPrev << 2) | s];
  encPrev = s;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  encPrev = (PIND >> 2) & 0x03;
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encISR, CHANGE);
  oled.begin();
  Serial.println(F("Step 2: turn the knob, press it"));
}

void loop() {
  noInterrupts(); int16_t raw = encRaw; interrupts();
  int16_t clicks = raw / 4;
  bool pressed = digitalRead(PIN_ENC_SW) == LOW;

  static int16_t lastRaw = 0;
  static bool lastPressed = false;
  if (raw != lastRaw || pressed != lastPressed) {
    Serial.print(F("raw ")); Serial.print(raw);
    Serial.print(F("  clicks ")); Serial.print(clicks);
    Serial.print(F("  button ")); Serial.println(pressed ? F("DOWN") : F("up"));
    lastRaw = raw; lastPressed = pressed;
  }

  char l1[20], l2[20];
  snprintf(l1, sizeof(l1), "Clicks: %d", clicks);
  snprintf(l2, sizeof(l2), "Raw: %d", raw);
  oled.firstPage();
  do {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(0, 12, "Step 2: encoder");
    oled.drawStr(0, 30, l1);
    oled.drawStr(0, 44, l2);
    oled.drawStr(0, 60, pressed ? "Button: DOWN" : "Button: up");
  } while (oled.nextPage());
}
