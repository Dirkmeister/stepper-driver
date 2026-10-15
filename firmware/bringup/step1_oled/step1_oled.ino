/*
  Step 1: OLED only
  Wiring: OLED GND->GND, VCC->5V, SCL->A5, SDA->A4
  Board:  Arduino Uno
  Serial Monitor: 115200 baud
*/
#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // 1. Look for anything on the I2C bus
  Serial.println(F("Scanning I2C..."));
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  Found device at 0x"));
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println(F("  Nothing found. Check the 4 wires."));

  // 2. Draw on the screen
  oled.begin();
}

void loop() {
  static uint16_t count = 0;
  char buf[20];
  snprintf(buf, sizeof(buf), "Count: %u", count++);

  oled.firstPage();
  do {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(0, 12, "Step 1: OLED works");
    oled.drawStr(0, 30, buf);
    oled.drawFrame(0, 40, 128, 24);
  } while (oled.nextPage());

  delay(500);
}
