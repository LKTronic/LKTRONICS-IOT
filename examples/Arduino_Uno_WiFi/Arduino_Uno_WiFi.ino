/**
 * LKTRONICS-IOT EXAMPLE: ARDUINO UNO / MEGA / NANO + WiFi MODULE (ESP-01)
 * Connects Arduino Uno to LK-TRONICS Dashboard via SoftwareSerial & WiFiEsp.
 */

#include <SoftwareSerial.h>
#include <WiFiEsp.h>
#include <LKTRONICS-IOT.h>

// ESP-01 Pins: RX to Uno Pin 3 (via resistor divider), TX to Uno Pin 2
SoftwareSerial espSerial(2, 3);
WiFiEspClient espClient;

const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiKey   = "YOUR_PROJECT_API_KEY";

#define RELAY1_PIN 7

// -------------------------------------------------------------
// Dashboard Switch on V1 toggles relay
// -------------------------------------------------------------
LK_WRITE(V1) {
  int state = param.asInt(); // 1 or 0
  digitalWrite(RELAY1_PIN, state);
  Serial.print(F("Relay V1: "));
  Serial.println(state ? F("ON") : F("OFF"));
}

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  pinMode(RELAY1_PIN, OUTPUT);

  // Initialize WiFi module
  WiFi.init(&espSerial);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("WiFi module not detected!"));
    while (true);
  }

  // Connect to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("Connecting to WiFi... "));
    WiFi.begin(ssid, password);
    delay(2000);
  }
  Serial.println(F("\nWiFi Connected!"));

  // Start LKTRONICS-IOT by passing the Client object & your API key:
  LKTRONICS_IOT.begin(espClient, apiKey);
}

void loop() {
  // Keeps connection alive and handles incoming dashboard commands
  LKTRONICS_IOT.run();

  // Send sensor reading every 3 seconds
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 3000) {
    lastSend = millis();

    int analogVal = analogRead(A0);

    // Write directly to Dashboard Virtual Pin V0:
    LKTRONICS_IOT.virtualWrite(V0, analogVal);

    Serial.print(F("Telemetry sent -> A0: "));
    Serial.println(analogVal);
  }
}
