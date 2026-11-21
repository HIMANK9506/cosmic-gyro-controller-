#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA = GPIO 21, SCL = GPIO 22
  delay(1500);
  Serial.println("\n--- Starting I2C Scan ---");

  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("✅ Found device at address: 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("❌ No I2C devices found! Check wiring (3.3V, GND, SDA, SCL).");
  } else {
    Serial.println("--- Scan Complete ---");
  }
}

void loop() {}
