#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 24);
  display.println("Init MPU6050...");
  display.display();

  if (!mpu.begin()) {
    display.clearDisplay();
    display.setCursor(10, 24);
    display.println("MPU6050 NOT FOUND!");
    display.display();
    while (1) {
      delay(10);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(500);
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Calculate pitch and roll from accelerometer (in degrees)
  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  float pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

  // Map pitch and roll to screen coordinates for artificial horizon / crosshair
  // Center is (96, 36)
  int centerX = 96;
  int centerY = 36;
  int radius = 18;

  int bubbleX = centerX + constrain((int)(roll * 0.4), -radius + 4, radius - 4);
  int bubbleY = centerY + constrain((int)(pitch * 0.4), -radius + 4, radius - 4);

  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.println("GYRO HUD TEST");
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);

  // Angle Readings on Left
  display.setCursor(4, 18);
  display.print("PITCH: ");
  display.print((int)pitch);
  display.print((char)247);

  display.setCursor(4, 30);
  display.print("ROLL : ");
  display.print((int)roll);
  display.print((char)247);

  display.setCursor(4, 44);
  display.print("G-Z  : ");
  display.print(a.acceleration.z, 1);
  display.print("m/s");

  display.setCursor(4, 55);
  display.print("GYRO : OK");

  // Tilt circle gauge on Right
  display.drawCircle(centerX, centerY, radius, SSD1306_WHITE);
  display.drawFastHLine(centerX - 3, centerY, 7, SSD1306_WHITE);
  display.drawFastVLine(centerX, centerY - 3, 7, SSD1306_WHITE);

  // Moving tilt bubble
  display.fillCircle(bubbleX, bubbleY, 3, SSD1306_WHITE);

  display.display();
  delay(30); // ~33 FPS smooth animation
}
