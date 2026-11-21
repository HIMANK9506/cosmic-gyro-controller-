#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BleMouse.h>
#include <esp_sleep.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

BleMouse bleMouse("Cosmic - device", "Custom", 100);

// --- Sensitivity Profiles with Rock-Solid Anti-Drift Deadzones ---
struct AimProfile {
  const char* name;
  float sensX;
  float sensY;
  float deadzone;
  float exponent;
};

AimProfile profiles[] = {
  {"SNIPER",   0.7, 0.6, 1.2, 1.10},  // Sniping (Zero Jitter)
  {"TACTICAL", 1.8, 1.6, 1.8, 1.25},  // Balanced (Zero Drift)
  {"FLICK",    3.2, 2.8, 2.2, 1.40}   // Fast 180 Turns
};
int currentProfile = 1; // Default TACTICAL
const int NUM_PROFILES = 3;

// --- BOOT Button (GPIO 0) Multi-Function ---
#define BOOT_BUTTON_PIN 0
unsigned long lastButtonPress = 0;
unsigned long buttonDownTime = 0;
bool isDown = false;
int clickCount = 0;

// --- Gyro Offsets & Calibration ---
float biasX = 0, biasY = 0, biasZ = 0;
float prevRawGx = 0, prevRawGy = 0, prevRawGz = 0;
int stillCount = 0;

// Session & Battery
unsigned long sessionStart = 0;
float batteryPct = 95.0;
bool showCalibratedMessage = false;
unsigned long calMessageTimer = 0;

// Sequenced Safe 16-Bit I2C Read (No byte-swapping)
void readRawGyro(float &gx, float &gy, float &gz) {
  Wire.beginTransmission(0x68);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6);

  if (Wire.available() >= 6) {
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    uint8_t zh = Wire.read();
    uint8_t zl = Wire.read();

    int16_t rawX = (int16_t)((xh << 8) | xl);
    int16_t rawY = (int16_t)((yh << 8) | yl);
    int16_t rawZ = (int16_t)((zh << 8) | zl);

    gx = (float)rawX / 65.5;
    gy = (float)rawY / 65.5;
    gz = (float)rawZ / 65.5;
  }
}

void calibrateZero() {
  float sumX = 0, sumY = 0, sumZ = 0;
  for (int i = 0; i < 60; i++) {
    float gx, gy, gz;
    readRawGyro(gx, gy, gz);
    sumX += gx;
    sumY += gy;
    sumZ += gz;
    delay(5);
  }
  biasX = sumX / 60.0;
  biasY = sumY / 60.0;
  biasZ = sumZ / 60.0;
  showCalibratedMessage = true;
  calMessageTimer = millis();
}

void enterDeepSleep() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 20);
  display.println("POWERING OFF...");
  display.setCursor(14, 35);
  display.println("PRESS BOOT TO WAKE");
  display.display();
  delay(1000);

  display.ssd1306_command(SSD1306_DISPLAYOFF);

  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x40);
  Wire.endTransmission();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(400000);
  Wire.setTimeOut(25);

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, 20);
  display.println("STARTING GYRO...");
  display.setCursor(14, 35);
  display.println("CALIBRATING ZERO");
  display.display();

  // Wake up MPU
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();

  // +/- 500 deg/s Range
  Wire.beginTransmission(0x68);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission();

  // 42Hz DLPF Filter
  Wire.beginTransmission(0x68);
  Wire.write(0x1A);
  Wire.write(0x03);
  Wire.endTransmission();

  calibrateZero();

  bleMouse.begin();
  sessionStart = millis();
}

void drawBattery(int x, int y, int pct) {
  display.drawRect(x, y, 14, 7, SSD1306_WHITE);
  display.fillRect(x + 14, y + 2, 2, 3, SSD1306_WHITE);
  int fill = map(constrain(pct, 0, 100), 0, 100, 0, 10);
  if (fill > 0) {
    display.fillRect(x + 2, y + 2, fill, 3, SSD1306_WHITE);
  }
}

unsigned long lastScreen = 0;
int mouseMoveX = 0;
int mouseMoveY = 0;

void loop() {
  unsigned long now = millis();

  // 1. Button Logic
  bool btn = (digitalRead(BOOT_BUTTON_PIN) == LOW);

  if (btn && !isDown) {
    isDown = true;
    buttonDownTime = now;
  } else if (!btn && isDown) {
    isDown = false;
    unsigned long dur = now - buttonDownTime;
    if (dur >= 1800) {
      enterDeepSleep();
    } else if (dur > 30) {
      clickCount++;
      lastButtonPress = now;
    }
  }

  if (clickCount > 0 && (now - lastButtonPress > 280)) {
    if (clickCount == 1) {
      currentProfile = (currentProfile + 1) % NUM_PROFILES;
    } else if (clickCount >= 2) {
      calibrateZero(); // Double-click recalibrates true zero instantly!
    }
    clickCount = 0;
  }

  // 2. Read Gyro with Sequential Safe Bytes
  float rawGx = 0, rawGy = 0, rawGz = 0;
  readRawGyro(rawGx, rawGy, rawGz);

  // 3. Gentle Drift Zeroing (Only when truly stationary, prevents rubber-banding!)
  float jitter = fabs(rawGx - prevRawGx) + fabs(rawGy - prevRawGy) + fabs(rawGz - prevRawGz);
  prevRawGx = rawGx;
  prevRawGy = rawGy;
  prevRawGz = rawGz;

  if (jitter < 0.35) { // True still condition (on table / resting)
    stillCount++;
    if (stillCount > 60) { // ~600ms of true rest
      biasX += (rawGx - biasX) * 0.005; // Ultra-slow gentle drift tracking
      biasY += (rawGy - biasY) * 0.005;
      biasZ += (rawGz - biasZ) * 0.005;
    }
  } else {
    stillCount = 0;
  }

  // 4. Calculate Aim (with Smooth Deadzone Subtraction)
  AimProfile& prof = profiles[currentProfile];
  float degX = rawGx - biasX; // Pitch (Up/Down)
  float degZ = rawGz - biasZ; // Yaw (Left/Right)

  float inputX = 0;
  float inputY = 0;

  // Solid deadzone with subtraction eliminates idle creep completely
  if (fabs(degZ) > prof.deadzone) {
    float mag = fabs(degZ) - prof.deadzone;
    inputX = (degZ > 0) ? mag : -mag;
  }
  if (fabs(degX) > prof.deadzone) {
    float mag = fabs(degX) - prof.deadzone;
    inputY = (degX > 0) ? mag : -mag;
  }

  mouseMoveX = 0;
  mouseMoveY = 0;

  if (inputX != 0) {
    float sign = (inputX > 0) ? 1.0 : -1.0;
    mouseMoveX = (int)(sign * pow(fabs(inputX), prof.exponent) * prof.sensX * 0.16);
  }
  if (inputY != 0) {
    float sign = (inputY > 0) ? 1.0 : -1.0;
    mouseMoveY = (int)(sign * pow(fabs(inputY), prof.exponent) * prof.sensY * 0.16);
  }

  // Constrain deltas
  mouseMoveX = constrain(mouseMoveX, -127, 127);
  mouseMoveY = constrain(mouseMoveY, -127, 127);

  // 5. Send Bluetooth Mouse Delta
  bool connected = bleMouse.isConnected();
  if (connected && (mouseMoveX != 0 || mouseMoveY != 0)) {
    bleMouse.move((signed char)mouseMoveX, (signed char)mouseMoveY, 0);
  }

  // 6. Battery % (Countdown based on active session time)
  unsigned long minutes = (now - sessionStart) / 60000;
  batteryPct = max(8.0f, 96.0f - (minutes * 0.40f));

  // 7. Render OLED HUD (every 80ms)
  if (now - lastScreen > 80) {
    lastScreen = now;

    display.clearDisplay();

    // Top Header: Profile + Battery %
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("MOD:");
    display.print(prof.name);

    display.setCursor(62, 0);
    display.print("BAT:");
    display.print((int)batteryPct);
    display.print("%");
    drawBattery(112, 0, (int)batteryPct);

    display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);

    // Bluetooth Status
    display.setCursor(0, 13);
    display.print("BLE: ");
    if (connected) {
      display.println("CONNECTED [OK]");
    } else {
      display.println("PAIRING...");
    }

    // Sensitivity Bar
    display.setCursor(0, 24);
    display.print("SENS:");
    int barWidth = (currentProfile + 1) * 15;
    display.drawRect(34, 24, 46, 7, SSD1306_WHITE);
    display.fillRect(34, 24, barWidth, 7, SSD1306_WHITE);

    // Drift / Recalibration Banner
    display.setCursor(0, 36);
    if (showCalibratedMessage) {
      display.println(">>> ZERO LOCKED! <<<");
      if (now - calMessageTimer > 1200) showCalibratedMessage = false;
    } else {
      display.print("DRIFT:");
      if (stillCount > 30) {
        display.println("ZEROED [LOCK]");
      } else {
        display.println("TRACKING");
      }
    }

    // Aim Deltas
    display.setCursor(0, 48);
    display.print("X:"); display.print(mouseMoveX);
    display.setCursor(42, 48);
    display.print("Y:"); display.println(mouseMoveY);

    if (isDown && (now - buttonDownTime > 800)) {
      display.fillRect(0, 46, 80, 16, SSD1306_BLACK);
      display.drawRect(0, 46, 80, 16, SSD1306_WHITE);
      display.setCursor(4, 50);
      display.print("RELEASE: OFF");
    }

    // Crosshair Gauge
    int cx = 105, cy = 40, r = 16;
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    display.drawFastHLine(cx - 3, cy, 7, SSD1306_WHITE);
    display.drawFastVLine(cx, cy - 3, 7, SSD1306_WHITE);

    int dotX = cx + constrain(mouseMoveX, -r + 2, r - 2);
    int dotY = cy + constrain(mouseMoveY, -r + 2, r - 2);
    display.fillCircle(dotX, dotY, 2, SSD1306_WHITE);

    display.display();
  }

  delay(10);
}
