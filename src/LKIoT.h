#pragma once

#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "LKIoTConfig.h"

// Detect native secure WiFi capability (ESP32 and ESP8266)
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
  #define LK_HAS_NATIVE_SECURE_WIFI 1
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #define LK_HAS_NATIVE_SECURE_WIFI 1
#else
  #define LK_HAS_NATIVE_SECURE_WIFI 0
#endif

// Define Virtual Pins (V0 - V31)
enum LKVirtualPin {
  V0 = 0,  V1,  V2,  V3,  V4,  V5,  V6,  V7,
  V8,      V9,  V10, V11, V12, V13, V14, V15,
  V16,     V17, V18, V19, V20, V21, V22, V23,
  V24,     V25, V26, V27, V28, V29, V30, V31
};

// Parameter wrapper for LK_WRITE callbacks
class LKParam {
public:
  String val;
  LKParam(const String& v = "") : val(v) {}
  int asInt() const { return val.toInt(); }
  float asFloat() const { return val.toFloat(); }
  double asDouble() const { return val.toDouble(); }
  String asString() const { return val; }
};

typedef void (*LKPinCallback)(const LKParam& param);

// Registration helper for LK_WRITE macros
class LKHandlerRegistrar {
public:
  LKHandlerRegistrar(LKVirtualPin pin, LKPinCallback cb);
};

// Macro for virtual pin incoming command handlers
#define LK_WRITE(pin) \
  void _lk_h_##pin(const LKParam& param); \
  static LKHandlerRegistrar _lk_r_##pin(pin, _lk_h_##pin); \
  void _lk_h_##pin(const LKParam& param)

#define LKIOT_WRITE(pin) LK_WRITE(pin)
#define LKTRONICS_IOT_WRITE(pin) LK_WRITE(pin)

class LKIoTClass {
public:
  LKIoTClass();

  // -------------------------------------------------------------------------
  // 1. Universal begin for ANY board (Arduino Uno, Mega, Nano with WiFi module,
  //    Uno R4, MKR, Ethernet Shield, etc.) by passing your Client object
  // -------------------------------------------------------------------------
  void begin(Client& client, const char* apiKey);
  void begin(Client& client, const char* apiKey, const char* brokerHost, uint16_t brokerPort,
             const char* brokerUser, const char* brokerPass);

#if LK_HAS_NATIVE_SECURE_WIFI
  // -------------------------------------------------------------------------
  // 2. Convenience zero-config begin for ESP32 and ESP8266 (built-in TLS/SSL)
  // -------------------------------------------------------------------------
  void begin(const char* apiKey);
  void begin(const char* apiKey, const char* brokerHost, uint16_t brokerPort,
             const char* brokerUser, const char* brokerPass);
#endif

  // Must be called in Arduino loop()
  void run();

  // Check connection status
  bool connected();

  // Virtual Write overloads for pins (V0 - V31)
  void virtualWrite(LKVirtualPin pin, int value);
  void virtualWrite(LKVirtualPin pin, unsigned int value);
  void virtualWrite(LKVirtualPin pin, long value);
  void virtualWrite(LKVirtualPin pin, unsigned long value);
  void virtualWrite(LKVirtualPin pin, bool value);
  void virtualWrite(LKVirtualPin pin, float value, int decimals = 2);
  void virtualWrite(LKVirtualPin pin, double value, int decimals = 2);
  void virtualWrite(LKVirtualPin pin, const String& value);
  void virtualWrite(LKVirtualPin pin, const char* value);

  // Virtual Write overloads for custom sensor key names (e.g. "temperature", "humidity")
  void virtualWrite(const char* key, int value);
  void virtualWrite(const char* key, unsigned int value);
  void virtualWrite(const char* key, long value);
  void virtualWrite(const char* key, unsigned long value);
  void virtualWrite(const char* key, bool value);
  void virtualWrite(const char* key, float value, int decimals = 2);
  void virtualWrite(const char* key, double value, int decimals = 2);
  void virtualWrite(const char* key, const String& value);
  void virtualWrite(const char* key, const char* value);

  // Attach callback for ANY custom widget key name (e.g. "joy_x", "fan_pwm", "relay1")
  void on(const char* key, LKPinCallback cb);
  void on(LKVirtualPin pin, LKPinCallback cb);

  // Auto-Diagnostics for Device Card, Signal Strength, and Uptime widgets
  void setFirmware(const char* version);
  void setBattery(int percent);
  void setDiagnosticsInterval(uint32_t seconds);
  void enableDiagnostics(bool enable = true);
  void sendDiagnostics();

  // Internal handler registration
  static bool registerHandler(LKVirtualPin pin, LKPinCallback cb);
  static bool registerHandler(const char* key, LKPinCallback cb);

private:
  void initCommon();
  void connectMQTT();
  void flushTelemetry();
  static void onMqttMessage(char* topic, byte* payload, unsigned int length);

  bool _diagnosticsEnabled;
  uint32_t _diagnosticsInterval;
  unsigned long _lastDiagnosticsTime;
  String _firmwareVersion;
  int _batteryPercent;

  Client* _client;
#if LK_HAS_NATIVE_SECURE_WIFI
  WiFiClientSecure _defaultSecureClient;
#endif

  PubSubClient _mqtt;

  String _apiKey;
  String _brokerHost;
  uint16_t _brokerPort;
  String _brokerUser;
  String _brokerPass;

  String _topicData;
  String _topicCmd;
  String _topicCmdAlt;
  String _topicStatus;

  StaticJsonDocument<LK_JSON_DOC_SIZE> _outDoc;
  bool _pendingData;
  unsigned long _lastFlush;
  unsigned long _lastRetry;

  static LKPinCallback _handlers[32];
  static LKIoTClass* _instance;
};

extern LKIoTClass LKIoT;

// Compatibility aliases
typedef LKIoTClass LKTRONICS_IOTClass;
typedef LKIoTClass LKTronicsClass;
#define LKTRONICS_IOT LKIoT
#define LKTronics LKIoT
