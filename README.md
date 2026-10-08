# LKTRONICS-IOT Arduino Library

The official IoT library for **LK-TRONICS Dashboard**.  
Supports **ESP32, ESP8266, Arduino Uno / Mega / Nano with WiFi module (ESP-01 / WiFiEsp), Arduino Uno R4 WiFi, Ethernet Shield**, and all Arduino-compatible boards.

Hides all Cloud MQTT, TLS/SSL, PubSubClient, topics, keepAlive, auto-diagnostics (IP, RSSI, Uptime), and JSON serialization behind simple Virtual Pins (`V0` - `V31`), just like Blynk.

---

## 📦 How to Install in Arduino IDE

### Method 1: Add as .ZIP Library (Easiest)
1. In Arduino IDE, click **Sketch** $\rightarrow$ **Include Library** $\rightarrow$ **Add .ZIP Library...**
2. Select `LKTRONICS-IOT.zip` from your project folder.
3. Done!

### Method 2: Manual Copy
1. Copy the `LKTRONICS-IOT` folder into your Arduino libraries folder:
   - **Windows**: `Documents\Arduino\libraries\LKTRONICS-IOT`
   - **Mac/Linux**: `~/Documents/Arduino/libraries/LKTRONICS-IOT`
2. Restart Arduino IDE.

---

## ⚙️ Dependencies Required
Install these from Arduino Library Manager (`Ctrl + Shift + I`):
1. **PubSubClient** by Nick O'Leary
2. **ArduinoJson** by Benoît Blanchon (version 6.x or 7.x)

---

## 🚀 Quick Example

```cpp
#include <WiFi.h>
#include <LKTRONICS-IOT.h>

const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiKey   = "YOUR_PROJECT_API_KEY";

#define RELAY_PIN 2

// Dashboard Switch or Button on V1:
LK_WRITE(V1) {
  int state = param.asInt(); // 1 = ON, 0 = OFF
  digitalWrite(RELAY_PIN, state);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);

  // Normal WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  // One-line Cloud IoT start (Auto-Diagnostics enabled by default!):
  LKTRONICS_IOT.begin(apiKey);
}

void loop() {
  // Automatically sends IP, WiFi RSSI, and Uptime in the background!
  LKTRONICS_IOT.run();

  // Send sensor value to V0:
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 2000) {
    lastSend = millis();
    LKTRONICS_IOT.virtualWrite(V0, 25.5);
  }
}
```

---

## 🎛️ Animated Industrial Widgets Integration

Write to the dynamic animated widgets on your dashboard effortlessly:

```cpp
// 1. Industrial Fan: 1 = Spin animation, 0 = Stop
LKTRONICS_IOT.virtualWrite("v605", 1);

// 2. Mist Sprayer: 1 = Atomized spray animation, 0 = Idle
LKTRONICS_IOT.virtualWrite("v355", 1);

// 3. Liquid Tank: 0.0 to 100.0% with realistic floating wave physics
LKTRONICS_IOT.virtualWrite("v156", 78.5);

// 4. Multi-LED Array: 1 = Single LED, or bitmask (3 = LEDs 1&2, 7 = LEDs 1,2&3)
LKTRONICS_IOT.virtualWrite("v112", 1);
```


---

## 🎛️ Virtual Pins, Dropdowns & Numeric Inputs (v1.0.4+)

Receive dashboard commands, dropdown selections, and numeric setpoints without dedicating physical GPIO pins:

`cpp
// 1. Dropdown Option Selector
LKTRONICS_IOT.on("v680", [](const LKParam& p) {
  String mode = p.asString();
  Serial.printf(">> Mode changed to: %s\n", mode.c_str());
});

// 2. Numeric Input / Setpoint
LKTRONICS_IOT.on("v348", [](const LKParam& p) {
  int setpoint = p.asInt();
  Serial.printf(">> Target setpoint: %d\n", setpoint);
});

// 3. Virtual Switch
LKTRONICS_IOT.on("v104", [](const LKParam& p) {
  int state = p.asInt();
  Serial.printf(">> Virtual Switch: %s\n", state ? "ON" : "OFF");
});
`

### What's New in v1.0.4
- **Deduplication Engine**: Built-in 300ms deduplication filter ignores re-delivered or multi-published command packets.
- **Single Command Subscription**: Prevents duplicate topic subscription processing.
- **Clean Serial Output**: Removed internal library Serial logs on input events so only your sketch callbacks log to Serial.
- **ESP32 Core 3.x Compatibility**: Full support for LEDC PWM on ESP32 Core 3.0+.
