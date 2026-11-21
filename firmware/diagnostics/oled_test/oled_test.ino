#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Clear buffer
  display.clearDisplay();

  // Draw border
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  display.drawRect(2, 2, SCREEN_WIDTH-4, SCREEN_HEIGHT-4, SSD1306_WHITE);

  // Title
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, 10);
  display.println("GYRO CONTROLLER");

  display.drawLine(10, 22, 118, 22, SSD1306_WHITE);

  // Status
  display.setCursor(12, 28);
  display.println("OLED: ONLINE  [OK]");

  display.setCursor(12, 40);
  display.println("MPU6050: READY");

  display.setCursor(12, 50);
  display.println("UPGRADE ACTIVE");

  display.display();
}

int count = 0;

void loop() {
  // Little animated dot in the corner to show it's active
  display.fillRect(116, 6, 6, 6, (count % 2 == 0) ? SSD1306_WHITE : SSD1306_BLACK);
  display.display();
  count++;
  delay(500);
}
