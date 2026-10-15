/*
  Step 3: talk to the TMC2209 (NO MOTOR CONNECTED)
  Keeps: OLED (A4/A5), encoder (D2/D3/D4)
  New:   VIO->5V, GND->GND, VM->12V, GND->brick-, brick- -> Uno GND
         100uF across VM/GND, MS1->GND, MS2->GND, EN->D7
         PDN/UART <- D10 direct, and D11 -> 1k -> PDN/UART
  Press the knob: toggles the driver's EN line, watch "EN pin" follow.
  Board: Arduino Uno   Serial Monitor: 115200 baud
*/
#include <Wire.h>
#include <U8g2lib.h>
#include <TMCStepper.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

const uint8_t PIN_ENC_SW = 4;
const uint8_t PIN_EN     = 7;    // active LOW: LOW = driver on
const uint8_t SW_RX      = 10;   // straight to PDN/UART
const uint8_t SW_TX      = 11;   // through 1k to PDN/UART

#define R_SENSE   0.11f
#define DRV_ADDR  0b00
TMC2209Stepper driver(SW_RX, SW_TX, R_SENSE, DRV_ADDR);

bool enOn = false;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, HIGH);          // start disabled
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  oled.begin();

  driver.beginSerial(57600);
  delay(50);
  driver.begin();
  driver.pdn_disable(true);            // PDN pin used as UART
  driver.I_scale_analog(false);        // current set by firmware, not the pot
  driver.rms_current(200);             // low, no motor anyway
  driver.toff(0);                      // outputs off for this step
  Serial.println(F("Step 3: driver UART check"));
}

void loop() {
  // Button toggles EN (debounced by edge + delay)
  static bool lastBtn = false;
  bool btn = digitalRead(PIN_ENC_SW) == LOW;
  if (btn && !lastBtn) {
    enOn = !enOn;
    digitalWrite(PIN_EN, enOn ? LOW : HIGH);
    delay(30);
  }
  lastBtn = btn;

  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 500) return;
  lastPoll = millis();

  uint8_t  conn = driver.test_connection();   // 0 OK, 1 no reply, 2 all zeros
  uint8_t  ver  = driver.version();           // expect 0x21
  uint32_t ioin = driver.IOIN();
  bool enPinSeen = !(ioin & 0x01);            // IOIN bit0 = ENN (1 = disabled)

  Serial.print(F("conn ")); Serial.print(conn);
  Serial.print(F("  version 0x")); Serial.print(ver, HEX);
  Serial.print(F("  IOIN 0x")); Serial.print(ioin, HEX);
  Serial.print(F("  EN pin ")); Serial.println(enPinSeen ? F("ON") : F("off"));

  char l1[22], l2[22], l3[22];
  snprintf(l1, sizeof(l1), "Link: %s (%u)", conn == 0 ? "OK" : "FAIL", conn);
  snprintf(l2, sizeof(l2), "Version: 0x%02X", ver);
  snprintf(l3, sizeof(l3), "EN out %s / seen %s", enOn ? "ON" : "off", enPinSeen ? "ON" : "off");
  oled.firstPage();
  do {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(0, 12, "Step 3: driver UART");
    oled.drawStr(0, 28, l1);
    oled.drawStr(0, 42, l2);
    oled.drawStr(0, 58, l3);
  } while (oled.nextPage());
}
