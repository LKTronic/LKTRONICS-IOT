/**
 * LKTRONICS-IOT QUICKSTART EXAMPLE
 * Demonstrates standard WiFi connection, reading sensors on V0, and toggling a relay on V1.
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

// ---------------------------------------------------------------------------
// Dashboard incoming command on Virtual Pin V1 (Switch or Button)
// ---------------------------------------------------------------------------
LK_WRITE(V1) {
  int state = param.asInt(); // 1 = ON, 0 = OFF
  digitalWrite(RELAY1_PIN, state);
  Serial.print("Relay V1 turned: ");
  Serial.println(state ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);

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

  // (Optional 1-liner) Set custom firmware version tag
  LKTRONICS_IOT.setFirmware("v1.0.0-prod");
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

    // Send to Virtual Pin V0 (Gauge, Value Display, or Chart)
    LKTRONICS_IOT.virtualWrite(V0, simulatedTemp);

    // Send to Virtual Pin V2
    LKTRONICS_IOT.virtualWrite(V2, analogVal);

    Serial.print("Telemetry Sent -> Temp: ");
    Serial.print(simulatedTemp);
    Serial.print(" | ADC: ");
    Serial.println(analogVal);
  }
}
