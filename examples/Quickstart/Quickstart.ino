/**
 * LKTRONICS-IOT QUICKSTART EXAMPLE
 * Demonstrates 1-line declarative control binding and 1-line telemetry dispatch.
 * Auto-diagnostics (IP, WiFi RSSI, Uptime) are sent automatically in the background.
 */

#include <WiFi.h>          // Standard WiFi
#include <LKTRONICS-IOT.h> // Official LKTRONICS-IOT Library

// Replace with your Network Credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Replace with your Project API Key from the Dashboard
const char* apiKey   = "YOUR_PROJECT_API_KEY";

#define RELAY1_PIN 2

void setup() {
  Serial.begin(115200);

  // 1. Normal WiFi Connection
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi Connected! IP: ");
  Serial.println(WiFi.localIP());

  // 2. Start LKTRONICS-IOT (Connects to HiveMQ TLS SSL, enables Auto-Diagnostics)
  LKTRONICS_IOT.begin(apiKey);

  // 3. 1-Line Control Auto-Binding (Zero Boilerplate!)
  LKTRONICS_IOT.bindSwitch("relay1", RELAY1_PIN); // Auto-drives pin with dashboard sync
}

void loop() {
  // Keeps connection alive, auto-reconnects, and handles incoming dashboard commands.
  // Also sends IP, WiFi RSSI, and Uptime in the background!
  LKTRONICS_IOT.run();

  // Send sensor readings every 2 seconds
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 2000) {
    lastSend = millis();

    int analogVal = analogRead(34);
    float simulatedTemp = 24.5 + (random(0, 50) / 10.0);

    // 1-Line Telemetry Sends
    LKTRONICS_IOT.send("temperature", simulatedTemp);
    LKTRONICS_IOT.send("sensor_val", analogVal);

    Serial.print("Telemetry Sent -> Temp: ");
    Serial.print(simulatedTemp);
    Serial.print(" | ADC: ");
    Serial.println(analogVal);
  }
}
