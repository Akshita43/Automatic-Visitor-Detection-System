# Automatic Visitor Detection System

An ESP32-based smart doorbell that senses a visitor **before** the bell is pressed, shows how close they are with an RGB LED, rings a buzzer when the button is pressed, and sends the owner a **photo on Telegram**.

---

## Features

- Detects a visitor approaching the entrance (HC-SR04 ultrasonic sensor)
- Measures distance and shows the zone on an RGB LED
- Rings a buzzer when the doorbell button is pressed
- Captures the visitor's image with the ESP32-CAM
- Sends the photo and distance alerts to the owner's Telegram

## How it works

```
Visitor -> HC-SR04 -> ESP32 -> RGB LED / Buzzer
                         |
                     (D23 -> IO13)
                         v
                    ESP32-CAM -> Wi-Fi -> Telegram
```

1. The ESP32 sends a pulse on TRIG and reads the ECHO pulse width.
2. Distance is calculated as `d = (t x 0.0343) / 2` cm.
3. The RGB LED shows the zone:

| Zone | Distance | LED | GPIO |
|------|----------|-----|------|
| Far | more than 100 cm | Green | 26 |
| Near | 50-100 cm | Blue | 25 |
| Very near | less than 50 cm | Red | 27 |

(Thresholds are configurable in the firmware.)

4. When the push button is pressed, the buzzer sounds and the ESP32 triggers the ESP32-CAM.
5. The ESP32-CAM takes a photo and sends it to Telegram over Wi-Fi.
6. The system goes back to monitoring.

## Hardware

- ESP32 development board
- HC-SR04 ultrasonic sensor
- ESP32-CAM
- FTDI USB-to-serial adapter (to program the ESP32-CAM)
- RGB LED (common 3V3, active-LOW)
- Push button
- Buzzer
- Resistors: 3 x 220 ohm, 1 k ohm and 2 k ohm (ECHO divider)
- Breadboard, jumper wires, power supply

## Pin connections

| Component / pin | Connected to | Remark |
|---|---|---|
| HC-SR04 VCC, GND | ESP32 5V/VIN, GND | Power |
| HC-SR04 TRIG | GPIO 5 | Trigger pulse |
| HC-SR04 ECHO | GPIO 18 | Via 1 k / 2 k divider to GND |
| RGB LED Blue, Green, Red | GPIO 25, 26, 27 | 220 ohm each |
| RGB LED Common | 3V3 | Active-LOW drive |
| Push button Pin A, Pin B | GPIO 33, GND | Doorbell input |
| Buzzer I/O, VCC, GND | GPIO 32, 3V3, GND | Audio alert |
| ESP32 D23 | ESP32-CAM IO13 | Capture trigger |
| ESP32 GND | ESP32-CAM GND | Common ground |
| FTDI VCC, GND | ESP32-CAM 5V, GND | Power and programming |
| FTDI TX, RX | ESP32-CAM U0R, U0T | Serial upload |
| ESP32-CAM IO0 | GND (temporary) | Upload mode only |

The ECHO pin outputs 5 V, so the 1 k / 2 k divider brings it down to 3.3 V for the ESP32.

See the full wiring diagram in `images/`.

## Setup

1. Wire the circuit as shown above.
2. Create a Telegram bot with [@BotFather](https://t.me/BotFather) and note the **bot token** and your **chat ID**.
3. In the code, replace the placeholders:
   ```cpp
   const char* ssid     = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   String botToken      = "YOUR_BOT_TOKEN";
   String chatId        = "YOUR_CHAT_ID";
   ```
   Never commit your real token or password.
4. Upload the **ESP32 (main)** code to the ESP32 using the Arduino IDE.
5. Upload the **ESP32-CAM** code:
   - Connect the FTDI adapter and put **IO0 to GND**.
   - Press reset and upload.
   - Remove the IO0-GND jumper and press reset to run normally.
6. Power both boards (with a common ground) and open Telegram to see the alerts.

## Repository structure

```
code/      ESP32 and ESP32-CAM sketches
docs/      Project report (PDF) and presentation (PPTX)
images/    Wiring diagram and Telegram output screenshots
README.md
```

## Results

- Distance alerts reach Telegram, e.g. "Someone is very NEAR the door. Distance: 3 cm"
- Each doorbell press sends a photo captioned "Doorbell pressed - visitor at the door"

## Future scope

Face recognition, cloud storage of visitor photos, a mobile app, and additional security sensors.

## Smart video doorbell vs this project

Commercial video doorbells offer live video and two-way talk, but cost more and usually need a cloud plan for recordings. This project adds early distance awareness before the press, at low cost with no subscription and open firmware.

## References

1. AI-Thinker, *ESP32-CAM WiFi and Bluetooth Camera Module*, 2023.
2. Espressif Systems, *ESP32 Series Datasheet*, 2025.
3. Cytron Technologies, *HC-SR04 Ultrasonic Sensor User's Manual*, 2013.
4. Telegram, *Telegram Bot API*, 2026.
