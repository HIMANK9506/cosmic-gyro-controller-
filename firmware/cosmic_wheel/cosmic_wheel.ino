#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BleGamepad.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Bluetooth Gamepad (Seen as Xbox/DirectInput Gamepad in Forza)
BleGamepad bleGamepad("Cosmic Wheel", "Custom", 100);

Preferences prefs;

// --- Steering Profiles (Max Steering Lock Angle) ---
struct SteerProfile {
  const char* name;
  float maxAngle;   // Degrees of tilt for 100% full steering lock
  float deadzone;   // Center deadzone in degrees (eliminates wobble on straights)
  float linearity;  // Steering curve exponent (1.0 = linear, 1.2 = gentle center)
};

SteerProfile profiles[] = {
  {"GT ROAD",   36.0, 1.2, 1.15},  // Balanced standard road racing (Forza Horizon)
  {"F1 RACE",   24.0, 0.8, 1.05},  // Quick, twitchy hypercar / track steering
  {"DRIFT",     50.0, 1.8, 1.25}   // Wide angle for high-angle counter-steering
};
int currentProfile = 0; // Default: GT ROAD
const int NUM_PROFILES = 3;

// --- BOOT Button (GPIO 0) Multi-Function ---
#define BOOT_BUTTON_PIN 0
unsigned long lastButtonPress = 0;
unsigned long buttonDownTime = 0;
bool isDown = false;
int clickCount = 0;

// Steering Direction & Calibration
bool invertSteering = false;
bool showBanner = false;
const char* bannerText = "";
unsigned long bannerTimer = 0;

// --- IMU & Complementary Filter ---
#define MPU_ADDR 0x68
float steerAngleRaw = 0.0;
float zeroOffset = 0.0;
unsigned long lastMicros = 0;

// Session & Battery
unsigned long sessionStart = 0;
float batteryPct = 95.0;

// Draw small battery icon on OLED
void drawBattery(int x, int y, int pct) {
  display.drawRect(x, y, 14, 7, SSD1306_WHITE);
  display.drawFastVLine(x + 14, y + 2, 3, SSD1306_WHITE);
  int fill = map(pct, 0, 100, 0, 10);
  fill = constrain(fill, 0, 10);
  if (fill > 0) {
    display.fillRect(x + 2, y + 2, fill, 3, SSD1306_WHITE);
  }
}

// Deep Sleep Shutdown
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

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BOOT_BUTTON_PIN, 0); // LOW level wake
  esp_deep_sleep_start();
}

void setup() {
  Wire.begin(21, 22);
  Wire.setClock(400000); // 400kHz Fast I2C

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Load saved preferences
  prefs.begin("cosmic_wheel", false);
  invertSteering = prefs.getBool("invert", false);

  // Initialize OLED
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16, 18);
    display.println("COSMIC WHEEL v1.1");
    display.setCursor(18, 34);
    display.println("SIDE-TILT STEER");
    display.setCursor(22, 50);
    display.println("STARTING BLE...");
    display.display();
  }

  // Wake MPU-6500 (clear sleep bit in PWR_MGMT_1)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // Configure Gyro ±500 dps (FS_SEL = 1)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission(true);

  // Configure Accel ±4g (AFS_SEL = 1)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x08);
  Wire.endTransmission(true);

  // Start Bluetooth Gamepad
  BleGamepadConfiguration config;
  config.setAutoReport(false);
  bleGamepad.begin(&config);

  sessionStart = millis();
  lastMicros = micros();
  delay(500);
}

unsigned long lastScreen = 0;
int16_t currentSteerAxis = 0;

void loop() {
  unsigned long now = millis();
  unsigned long nowMicros = micros();
  float dt = (nowMicros - lastMicros) / 1000000.0f;
  lastMicros = nowMicros;
  if (dt <= 0.0f || dt > 0.05f) dt = 0.01f;

  // 1. BOOT Button Logic (Single, Double, Triple Click)
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
      // 1 Click: Cycle Steering Profiles
      currentProfile = (currentProfile + 1) % NUM_PROFILES;
    } else if (clickCount == 2) {
      // 2 Clicks: Set resting zero position
      zeroOffset = steerAngleRaw;
      bannerText = ">>> ZERO LOCKED! <<<";
      showBanner = true;
      bannerTimer = now;
    } else if (clickCount >= 3) {
      // 3 Clicks: Invert Direction (Flip Left <-> Right if ever needed)
      invertSteering = !invertSteering;
      prefs.putBool("invert", invertSteering);
      bannerText = invertSteering ? "DIR: REVERSED" : "DIR: NORMAL";
      showBanner = true;
      bannerTimer = now;
    }
    clickCount = 0;
  }

  // 2. Read 14 Bytes of Motion Data
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (size_t)14, true);

  if (Wire.available() >= 14) {
    uint8_t axh = Wire.read(), axl = Wire.read();
    uint8_t ayh = Wire.read(), ayl = Wire.read();
    uint8_t azh = Wire.read(), azl = Wire.read();
    Wire.read(); Wire.read(); // Skip Temperature
    uint8_t gxh = Wire.read(), gxl = Wire.read();
    uint8_t gyh = Wire.read(), gyl = Wire.read();
    uint8_t gzh = Wire.read(), gzl = Wire.read();

    int16_t rawAx = (int16_t)((axh << 8) | axl);
    int16_t rawAy = (int16_t)((ayh << 8) | ayl);
    int16_t rawAz = (int16_t)((azh << 8) | azl);
    int16_t rawGx = (int16_t)((gxh << 8) | gxl);
    int16_t rawGz = (int16_t)((gzh << 8) | gzl);

    float ax = (float)rawAx / 8192.0f; // ±4g scale
    float ay = (float)rawAy / 8192.0f;
    float az = (float)rawAz / 8192.0f;
    
    // Side-to-Side Steering:
    // With vertical breadboard mount on back, side-to-side tilt moves gravity into AX,
    // and angular rotation rate is measured by rawGx (in-plane roll).
    float gyroSteerRate = (float)rawGx / 65.5f;

    // Pitch-invariant Side-Tilt Angle (Right Side Down = Positive, Left Side Down = Negative)
    float accelSideTilt = atan2(ax, sqrt(ay * ay + az * az)) * (180.0f / M_PI);

    // Complementary Filter: 96% gyro rate integration + 4% gravity tilt
    steerAngleRaw = 0.96f * (steerAngleRaw + gyroSteerRate * dt) + 0.04f * accelSideTilt;
  }

  // 3. Compute Steering Angle & Inversion
  float steerAngle = steerAngleRaw - zeroOffset;
  if (invertSteering) {
    steerAngle = -steerAngle;
  }

  // 4. Process Angle through Profile Dynamics
  SteerProfile& prof = profiles[currentProfile];
  float effectiveAngle = 0.0f;

  if (fabs(steerAngle) > prof.deadzone) {
    float sign = (steerAngle > 0.0f) ? 1.0f : -1.0f;
    float magnitude = fabs(steerAngle) - prof.deadzone;
    float maxMag = prof.maxAngle - prof.deadzone;
    if (maxMag < 5.0f) maxMag = 5.0f;

    float normalized = constrain(magnitude / maxMag, 0.0f, 1.0f);
    float curved = pow(normalized, prof.linearity);

    effectiveAngle = sign * curved;
  }

  // Map to 16-bit signed Gamepad X-Axis (-32767 to +32767)
  // Right tilt down -> Positive X (Turn Right)
  // Left tilt down -> Negative X (Turn Left)
  currentSteerAxis = (int16_t)constrain(effectiveAngle * 32767.0f, -32767.0f, 32767.0f);

  // 5. Send Bluetooth Gamepad Left Stick Report
  bool connected = bleGamepad.isConnected();
  if (connected) {
    bleGamepad.setX(currentSteerAxis);
    bleGamepad.sendReport();
  }

  // 6. Battery % Countdown
  unsigned long minutes = (now - sessionStart) / 60000;
  batteryPct = max(8.0f, 96.0f - (minutes * 0.35f));

  // 7. OLED Telemetry HUD (Updated every 50ms / 20fps)
  if (now - lastScreen > 50) {
    lastScreen = now;
    display.clearDisplay();

    // Top Header: Profile + Battery
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

    // Bluetooth Link Status
    display.setCursor(0, 13);
    display.print("LINK: ");
    if (connected) {
      display.println("FORZA [OK]");
    } else {
      display.println("PAIRING...");
    }

    // Steering Angle Text or Banner
    display.setCursor(0, 24);
    if (showBanner) {
      display.println(bannerText);
      if (now - bannerTimer > 1200) showBanner = false;
    } else {
      display.print("STEER: ");
      if (fabs(steerAngle) < 1.0f) {
        display.println("[ CENTER ]");
      } else if (steerAngle < 0) {
        display.print("<- ");
        display.print(fabs(steerAngle), 1);
        display.println((char)247);
      } else {
        display.print(steerAngle, 1);
        display.print((char)247);
        display.println(" ->");
      }
    }

    // Virtual Steering Gauge (Bar + Center Marker + Needle)
    int gaugeY = 38;
    int gaugeW = 110;
    int gaugeX = (SCREEN_WIDTH - gaugeW) / 2;
    display.drawRect(gaugeX, gaugeY, gaugeW, 10, SSD1306_WHITE);
    // Center dead-straight tick
    display.drawFastVLine(SCREEN_WIDTH / 2, gaugeY - 2, 14, SSD1306_WHITE);

    // Steering Fill / Needle
    int needleX = map(currentSteerAxis, -32767, 32767, gaugeX + 2, gaugeX + gaugeW - 3);
    needleX = constrain(needleX, gaugeX + 2, gaugeX + gaugeW - 3);

    // Draw active steering wedge
    int midX = SCREEN_WIDTH / 2;
    if (needleX < midX) {
      display.fillRect(needleX, gaugeY + 2, midX - needleX, 6, SSD1306_WHITE);
    } else if (needleX > midX) {
      display.fillRect(midX, gaugeY + 2, needleX - midX, 6, SSD1306_WHITE);
    }
    // Highlight needle tip
    display.drawFastVLine(needleX, gaugeY - 1, 12, SSD1306_WHITE);

    // Bottom Status: 16-bit Axis Value & Lock Angle
    display.setCursor(0, 54);
    display.print("AXIS: ");
    display.print(currentSteerAxis);

    display.setCursor(80, 54);
    display.print("LOCK:");
    display.print((int)prof.maxAngle);
    display.print((char)247);

    // Deep Sleep Prompt
    if (isDown && (now - buttonDownTime > 800)) {
      display.fillRect(10, 48, 108, 16, SSD1306_BLACK);
      display.drawRect(10, 48, 108, 16, SSD1306_WHITE);
      display.setCursor(16, 52);
      display.print("RELEASE: OFF");
    }

    display.display();
  }

  delay(6);
}
