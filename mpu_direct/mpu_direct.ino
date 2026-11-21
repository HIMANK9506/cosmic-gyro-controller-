#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Check MPU6050 raw WHO_AM_I register (0x75)
  Wire.beginTransmission(0x68);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 1);
  byte whoami = 0;
  if (Wire.available()) {
    whoami = Wire.read();
  }

  // Wake up MPU6050 (PWR_MGMT_1 register 0x6B = 0)
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0);
  byte err = Wire.endTransmission();

  display.setCursor(0, 0);
  display.println("--- MPU CHECK ---");
  display.print("WHO_AM_I: 0x");
  display.println(whoami, HEX);
  display.print("Wakeup Err: ");
  display.println(err);
  display.display();

  Serial.print("WHO_AM_I: 0x");
  Serial.println(whoami, HEX);
  Serial.print("Wakeup Err: ");
  Serial.println(err);
}

void loop() {
  // Read raw Accel X, Y, Z (registers 0x3B to 0x40)
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6);

  if (Wire.available() >= 6) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();

    float pitch = atan2(-ax, sqrt((float)ay * ay + (float)az * az)) * 180.0 / PI;
    float roll  = atan2(ay, az) * 180.0 / PI;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("--- GYRO DIRECT ---");
    display.setCursor(0, 16);
    display.print("Pitch: "); display.print((int)pitch); display.println(" deg");
    display.setCursor(0, 28);
    display.print("Roll : "); display.print((int)roll); display.println(" deg");
    display.setCursor(0, 42);
    display.print("Raw Z: "); display.println(az);

    // Mini level bubble
    int cx = 100, cy = 35, r = 16;
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    int bx = cx + constrain((int)(roll * 0.3), -r + 3, r - 3);
    int by = cy + constrain((int)(pitch * 0.3), -r + 3, r - 3);
    display.fillCircle(bx, by, 3, SSD1306_WHITE);

    display.display();
  }
  delay(40);
}
