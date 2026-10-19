/**********************************************************************************
 * LK-TRONICS IOT - MULTI-DEVICE / MULTI-NODE TEST EXAMPLE
 * 
 * Demonstrates connecting multiple hardware microcontrollers (e.g. Device 1 & Device 2)
 * to the same IoT Dashboard project:
 *  - Device 1: Main Gateway Node (e.g. Temperature sensor & Relay switch)
 *  - Device 2: Remote Pump / Battery Node (e.g. Battery fuel gauge, RSSI, status)
 * 
 * Simply change DEVICE_NAME to "Device 1" on Node 1, and "Device 2" on Node 2!
 **********************************************************************************/

#include <WiFi.h>
#include <LKTRONICS-IOT.h>

// WiFi Configuration
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Project API Key (Shared across your project devices)
const char* API_KEY       = "YOUR_PROJECT_API_KEY";

// Configure Device Identity for this physical board:
// Change to "Device 1" for your main node, or "Device 2" for your second node
#define DEVICE_NAME "Device 2"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================");
  Serial.println("  LK-TRONICS MULTI-DEVICE NODE TEST");
  Serial.printf ("  Configured Device Node: %s\n", DEVICE_NAME);
  Serial.println("========================================");

  // 1. Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());

  // 2. Set Multi-Device Node Name & Diagnostics
  LKTRONICS_IOT.setDeviceName(DEVICE_NAME);
  LKTRONICS_IOT.setFirmware("v2.5.0-Node");
  LKTRONICS_IOT.enableDiagnostics(true);
  LKTRONICS_IOT.setDiagnosticsInterval(15); // Transmit diagnostics every 15s

  // 3. Optional: Listen for Relay commands for this node
  LKTRONICS_IOT.on("relay2", [](const LKParam& param) {
    Serial.printf("[%s] Relay 2 state received: %d\n", DEVICE_NAME, param.asInt());
  });

  // 4. Initialize Cloud Connection
  LKTRONICS_IOT.begin(API_KEY);
}

void loop() {
  // Keep connection alive & handle incoming commands
  LKTRONICS_IOT.run();

  // Send periodic telemetry for this device node every 5 seconds
  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 5000) {
    lastSend = millis();

    // Read battery voltage or calculate percentage
    int simulatedBattery = random(85, 98);
    LKTRONICS_IOT.setBattery(simulatedBattery);

    // Send sensor telemetry assigned to Device 2
    float waterFlow = random(120, 240) / 10.0;
    LKTRONICS_IOT.virtualWrite("device2_flow", waterFlow);
    LKTRONICS_IOT.virtualWrite("device2_batt", simulatedBattery);

    Serial.printf("[%s] Telemetry sent: Batt=%d%%, Flow=%.1f L/min, RSSI=%d dBm\n",
                  DEVICE_NAME, simulatedBattery, waterFlow, WiFi.RSSI());
  }
}
