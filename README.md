# Cosmic Gyro Controller

An open-source hardware upgrade that turns any generic 2.4GHz wireless game controller into a zero-drift, high-precision motion-aiming controller using an **ESP32**, **MPU-6500 / MPU-6050 6-Axis IMU**, and a **0.96" I2C SSD1306 OLED Display**.

---

## 🎮 How It Works

* **Dual-Channel Input Architecture:**
  * Your generic wireless controller handles normal thumbstick and button input over its existing 2.4GHz USB dongle.
  * The ESP32 companion unit mounts directly to the back of the controller, acting as a **Bluetooth Low Energy (BLE) HID Mouse** (`Cosmic - device`) providing high-polling-rate gyro aim.
* **Plug & Play:** Works seamlessly with PC games that accept simultaneous Gamepad + Mouse input (Steam, Krunker.io, Venge.io, Halo, Cyberpunk 2077, etc.).

---

## ⚡ Features

* **Anti-Drift Motion Engine:**
  * **Sequenced 16-bit I2C reads:** Solves byte-order swap bugs present in generic MPU libraries.
  * **Deadband Thresholds:** Eliminates hand tremor without sacrificing micro-aim precision.
  * **Exponential Power Curve:** Delivers micro-movement precision at low angular speeds and fast flick-turn speeds at high angular velocities.
* **On-Board OLED Telemetry HUD (SSD1306 128x64):**
  * Live crosshair position gauge.
  * Active profile display (`SNIPER`, `TACTICAL`, `FLICK`).
  * Sensitivity visual bar graph.
  * Connection indicator (`CONNECTED [OK]` vs `PAIRING...`).
  * Battery monitoring and session timer.
  * Zero-calibration confirmation banner (`>>> ZERO LOCKED! <<<`).
* **Multi-Function BOOT Button (GPIO 0):**
  * **Single Click:** Cycles through sensitivity profiles (`TACTICAL` ➔ `FLICK` ➔ `SNIPER`).
  * **Double Click:** Instant gyro re-zeroing (calibrates resting bias in 100ms).
  * **Long Press (1.8s):** Deep sleep shutdown (turns off screen, radios, and enters ultra-low microamp power mode). Single click to wake.

---

## 🔌 Hardware Wiring & Pinout

All I2C modules share the ESP32 hardware I2C bus:

| Module Pin | ESP32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **MPU-6500 VCC** | **3V3** | Power | Regulated 3.3V |
| **MPU-6500 GND** | **GND** | Ground | Common Ground |
| **MPU-6500 SDA** | **GPIO 21** | I2C Data | Shared with OLED |
| **MPU-6500 SCL** | **GPIO 22** | I2C Clock | Shared with OLED |
| **OLED VCC** | **3V3** | Power | Regulated 3.3V |
| **OLED GND** | **GND** | Ground | Common Ground |
| **OLED SDA** | **GPIO 21** | I2C Data | Shared with MPU |
| **OLED SCL** | **GPIO 22** | I2C Clock | Shared with MPU |
| **Battery + (3.7V - 4.2V)** | **VIN** | Power Input | Tapped from LiPo / power board |
| **Battery - (GND)** | **GND** | Ground | Shared ground |

---

## 🛠️ Software & Flashing Guide

### Prerequisites
Install the following libraries in Arduino IDE (`Sketch -> Include Library -> Manage Libraries...`):
1. **Adafruit SSD1306**
2. **Adafruit GFX Library**
3. **ESP32-BLE-Mouse** (by T-Party)

### Flashing Steps
1. Connect your ESP32 board to your PC via USB.
2. Select your board: `Tools -> Board -> ESP32 Arduino -> ESP32 Dev Module`.
3. Select your Port (e.g. `COM6`).
4. Open `firmware/cosmic_gyro/cosmic_gyro.ino`.
5. Click **Upload**.

---

## 📱 Connecting to Your Devices

### PC (Windows 10 / 11)
1. Go to **Settings ➔ Bluetooth & devices**.
2. Click **Add device ➔ Bluetooth**.
3. Select **`Cosmic - device`**.
4. The OLED screen will immediately show `BLE: CONNECTED [OK]`.

### Android (Mobile)
1. Turn on **Location / GPS** (required by Android to scan for BLE devices).
2. Go to **Settings ➔ Bluetooth ➔ Pair new device**.
3. Tap **`Cosmic - device`** and confirm the pairing request.
4. Ensure the **Input Device** toggle is enabled under device settings.

### iPhone / iPad (iOS)
1. Open **Settings ➔ Accessibility ➔ Touch ➔ AssistiveTouch** and turn it **ON**.
2. Tap **Devices ➔ Bluetooth Devices...**.
3. Select **`Cosmic - device`** to pair.

---

## 📁 Repository Structure

```text
├── firmware/
│   ├── cosmic_gyro/          # Main production firmware
│   │   └── cosmic_gyro.ino
│   └── diagnostics/          # Diagnostic verification sketches
│       ├── i2c_scanner/      # Scans I2C bus for 0x3C and 0x68
│       ├── mpu_direct/       # Direct register verification for MPU-6500 / 6050
│       └── oled_test/        # SSD1306 display diagnostic test
├── assets/
│   └── images/               # Circuit diagrams and build photos
├── hardware/                 # Schematics and bill of materials
├── .gitignore
├── LICENSE
└── README.md
```

---

## 📄 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
