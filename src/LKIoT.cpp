#include "LKIoT.h"

LKIoTClass* LKIoTClass::_instance = nullptr;
LKPinCallback LKIoTClass::_handlers[32] = { nullptr };

LKHandlerRegistrar::LKHandlerRegistrar(LKVirtualPin pin, LKPinCallback cb) {
  LKIoTClass::registerHandler(pin, cb);
}

LKIoTClass::LKIoTClass()
  : _client(nullptr),
    _brokerHost(LK_DEFAULT_BROKER),
    _brokerPort(LK_DEFAULT_PORT),
    _brokerUser(LK_DEFAULT_USER),
    _brokerPass(LK_DEFAULT_PASS),
    _diagnosticsEnabled(true),
    _diagnosticsInterval(30000),
    _lastDiagnosticsTime(0),
    _firmwareVersion("v1.0.0"),
    _batteryPercent(-1),
    _pendingData(false),
    _lastFlush(0),
    _lastRetry(0) {
  _instance = this;
}

struct LKNamedHandler {
  char key[24];
  LKPinCallback cb;
};

static LKNamedHandler _namedHandlers[32];
static uint8_t _namedHandlerCount = 0;

bool LKIoTClass::registerHandler(LKVirtualPin pin, LKPinCallback cb) {
  if ((int)pin >= 0 && (int)pin < 32) {
    _handlers[(int)pin] = cb;
    return true;
  }
  return false;
}

bool LKIoTClass::registerHandler(const char* key, LKPinCallback cb) {
  if (!key || _namedHandlerCount >= 32) return false;
  for (uint8_t i = 0; i < _namedHandlerCount; i++) {
    if (strcasecmp(_namedHandlers[i].key, key) == 0) {
      _namedHandlers[i].cb = cb;
      return true;
    }
  }
  strncpy(_namedHandlers[_namedHandlerCount].key, key, 23);
  _namedHandlers[_namedHandlerCount].key[23] = '\0';
  _namedHandlers[_namedHandlerCount].cb = cb;
  _namedHandlerCount++;
  return true;
}

void LKIoTClass::on(const char* key, LKPinCallback cb) {
  registerHandler(key, cb);
}

void LKIoTClass::on(LKVirtualPin pin, LKPinCallback cb) {
  registerHandler(pin, cb);
}

// ---------------------------------------------------------------------------
// Universal Begin (Arduino Uno with WiFi, Mega, Ethernet, Uno R4, etc.)
// ---------------------------------------------------------------------------
void LKIoTClass::begin(Client& client, const char* apiKey) {
  begin(client, apiKey, LK_DEFAULT_BROKER, LK_DEFAULT_PORT, LK_DEFAULT_USER, LK_DEFAULT_PASS);
}

void LKIoTClass::begin(Client& client, const char* apiKey, const char* brokerHost, uint16_t brokerPort,
                       const char* brokerUser, const char* brokerPass) {
  _client     = &client;
  _apiKey     = String(apiKey);
  _brokerHost = String(brokerHost);
  _brokerPort = brokerPort;
  _brokerUser = String(brokerUser);
  _brokerPass = String(brokerPass);

  _mqtt.setClient(*_client);
  initCommon();
}

#if LK_HAS_NATIVE_SECURE_WIFI
// ---------------------------------------------------------------------------
// ESP32 and ESP8266 Built-in Native Secure WiFi Begin
// ---------------------------------------------------------------------------
void LKIoTClass::begin(const char* apiKey) {
  begin(apiKey, LK_DEFAULT_BROKER, LK_DEFAULT_PORT, LK_DEFAULT_USER, LK_DEFAULT_PASS);
}

void LKIoTClass::begin(const char* apiKey, const char* brokerHost, uint16_t brokerPort,
                       const char* brokerUser, const char* brokerPass) {
  _defaultSecureClient.setInsecure(); // Cloud TLS without hardcoded CA certs
  _client     = &_defaultSecureClient;
  _apiKey     = String(apiKey);
  _brokerHost = String(brokerHost);
  _brokerPort = brokerPort;
  _brokerUser = String(brokerUser);
  _brokerPass = String(brokerPass);

  _mqtt.setClient(*_client);
  initCommon();
}
#endif

void LKIoTClass::initCommon() {
  // Generate topics
  _topicData    = "iot/" + _apiKey + "/devices/esp32/data";
  _topicCmd     = "iot/" + _apiKey + "/devices/esp32/cmd";
  _topicCmdAlt  = "iot/" + _apiKey + "/cmd";
  _topicStatus  = "iot/" + _apiKey + "/devices/esp32/status";

  _mqtt.setServer(_brokerHost.c_str(), _brokerPort);
  _mqtt.setCallback(LKIoTClass::onMqttMessage);
  _mqtt.setBufferSize(LK_BUFFER_SIZE);
  _mqtt.setKeepAlive(LK_KEEPALIVE);
  _mqtt.setSocketTimeout(LK_SOCKET_TIMEOUT);

  Serial.println(F("\n======================================"));
  Serial.println(F("     LKTRONICS-IOT Library v1.0       "));
  Serial.println(F("======================================"));

  connectMQTT();
}

void LKIoTClass::connectMQTT() {
  if (_mqtt.connected() || !_client) return;

#if LK_HAS_NATIVE_SECURE_WIFI
  if (_client == &_defaultSecureClient && WiFi.status() != WL_CONNECTED) {
    return;
  }
#endif

  // Generate unique Client ID
  String clientId = "LK_" + _apiKey.substring(0, 8) + "_" + String(random(0xFFFF), HEX);
  Serial.print(F("[LKTRONICS-IOT] Connecting to Cloud Broker ("));
  Serial.print(clientId);
  Serial.println(F(")..."));

  // Last Will and Testament (LWT) QoS 1 retained
  const char* lwtPayload = "{\"state\":\"offline\"}";
  if (_mqtt.connect(clientId.c_str(), _brokerUser.c_str(), _brokerPass.c_str(),
                    _topicStatus.c_str(), 1, true, lwtPayload)) {
    Serial.println(F("[LKTRONICS-IOT] Connected to Broker!"));

    // Broadcast online status
    _mqtt.publish(_topicStatus.c_str(), "{\"state\":\"online\"}", true);

    // Subscribe to both command topics with QoS 1 for reliable delivery
    _mqtt.subscribe(_topicCmd.c_str(), 1);
    _mqtt.subscribe(_topicCmdAlt.c_str(), 1);
    Serial.println(F("[LKTRONICS-IOT] Subscribed (QoS1) to dashboard commands"));
  } else {
    Serial.print(F("[LKTRONICS-IOT] Broker connection failed, rc="));
    Serial.print(_mqtt.state());
    Serial.println(F(" (will retry in 3s)"));
  }
}

void LKIoTClass::run() {
  if (!_client) return;

#if LK_HAS_NATIVE_SECURE_WIFI
  if (_client == &_defaultSecureClient && WiFi.status() != WL_CONNECTED) {
    return;
  }
#endif

  if (!_mqtt.connected()) {
    if (millis() - _lastRetry > 3000) {
      _lastRetry = millis();
      connectMQTT();
    }
  } else {
    _mqtt.loop();
  }

  // Automatic batch flush
  if (_pendingData && (millis() - _lastFlush > LK_FLUSH_INTERVAL)) {
    flushTelemetry();
  }

  // Automatic diagnostics heartbeat (IP, WiFi RSSI, Uptime)
  if (_diagnosticsEnabled && _mqtt.connected()) {
    if (millis() - _lastDiagnosticsTime >= _diagnosticsInterval || _lastDiagnosticsTime == 0) {
      sendDiagnostics();
    }
  }
}

bool LKIoTClass::connected() {
  return _mqtt.connected();
}

void LKIoTClass::setFirmware(const char* version) {
  if (version) _firmwareVersion = String(version);
}

void LKIoTClass::setBattery(int percent) {
  _batteryPercent = constrain(percent, 0, 100);
}

void LKIoTClass::setDiagnosticsInterval(uint32_t seconds) {
  if (seconds < 5) seconds = 5;
  _diagnosticsInterval = seconds * 1000UL;
}

void LKIoTClass::enableDiagnostics(bool enable) {
  _diagnosticsEnabled = enable;
}

void LKIoTClass::sendDiagnostics() {
  if (!_mqtt.connected()) return;

  StaticJsonDocument<256> diagDoc;

#if LK_HAS_NATIVE_SECURE_WIFI
  if (WiFi.status() == WL_CONNECTED) {
    diagDoc["ip"] = WiFi.localIP().toString();
    diagDoc["rssi"] = WiFi.RSSI();
  }
#endif

  diagDoc["uptime"] = (unsigned long)(millis() / 1000UL);
  diagDoc["fw"] = _firmwareVersion;

  if (_batteryPercent >= 0) {
    diagDoc["battery"] = _batteryPercent;
  }

  String payload;
  serializeJson(diagDoc, payload);
  _mqtt.publish(_topicData.c_str(), payload.c_str());

  _lastDiagnosticsTime = millis();
}

// Virtual Writes for Pins (V0 - V31)
void LKIoTClass::virtualWrite(LKVirtualPin pin, int value) {
  _outDoc["v" + String((int)pin)] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, unsigned int value) {
  _outDoc["v" + String((int)pin)] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, long value) {
  _outDoc["v" + String((int)pin)] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, unsigned long value) {
  _outDoc["v" + String((int)pin)] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, bool value) {
  _outDoc["v" + String((int)pin)] = value ? 1 : 0;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, float value, int decimals) {
  _outDoc["v" + String((int)pin)] = serialized(String(value, decimals));
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, double value, int decimals) {
  _outDoc["v" + String((int)pin)] = serialized(String(value, decimals));
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, const String& value) {
  _outDoc["v" + String((int)pin)] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(LKVirtualPin pin, const char* value) {
  _outDoc["v" + String((int)pin)] = value;
  _pendingData = true;
}

// Virtual Writes for Named Keys
void LKIoTClass::virtualWrite(const char* key, int value) {
  _outDoc[key] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, unsigned int value) {
  _outDoc[key] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, long value) {
  _outDoc[key] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, unsigned long value) {
  _outDoc[key] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, bool value) {
  _outDoc[key] = value ? 1 : 0;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, float value, int decimals) {
  _outDoc[key] = serialized(String(value, decimals));
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, double value, int decimals) {
  _outDoc[key] = serialized(String(value, decimals));
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, const String& value) {
  _outDoc[key] = value;
  _pendingData = true;
}

void LKIoTClass::virtualWrite(const char* key, const char* value) {
  _outDoc[key] = value;
  _pendingData = true;
}

void LKIoTClass::flushTelemetry() {
  if (!_mqtt.connected() || !_pendingData) return;

  String payload;
  serializeJson(_outDoc, payload);
  _mqtt.publish(_topicData.c_str(), payload.c_str());

  _outDoc.clear();
  _pendingData = false;
  _lastFlush = millis();
}

void LKIoTClass::onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  // Skip self LWT echoes
  if (String(topic).endsWith("/status")) return;

  StaticJsonDocument<LK_JSON_DOC_SIZE> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) return;

  JsonObject root = doc.as<JsonObject>();

  // Extract explicit pin and value if provided by dashboard
  String pinName = "";
  String pinValue = "";
  if (root.containsKey("pin")) {
    pinName = root["pin"].as<String>();
  }
  if (root.containsKey("value")) {
    pinValue = root["value"].as<String>();
  }

  // 1. Dispatch custom named handlers (e.g. "joy_x", "fan_pwm", "relay1")
  for (uint8_t i = 0; i < _namedHandlerCount; i++) {
    const char* hKey = _namedHandlers[i].key;
    if (pinName.length() > 0 && pinName.equalsIgnoreCase(hKey)) {
      _namedHandlers[i].cb(LKParam(pinValue));
    } else {
      for (JsonPair kv : root) {
        if (strcasecmp(kv.key().c_str(), hKey) == 0) {
          _namedHandlers[i].cb(LKParam(pinValue.length() > 0 ? pinValue : kv.value().as<String>()));
          break;
        }
      }
    }
  }

  // 2. Dispatch V0-V31 and RELAY1-RELAY32 virtual pin handlers
  for (JsonPair kv : root) {
    String key = kv.key().c_str();
    key.toLowerCase();

    int pinIndex = -1;
    if (key.startsWith("v")) {
      pinIndex = key.substring(1).toInt();
    } else if (key.startsWith("relay")) {
      pinIndex = key.substring(5).toInt();
    }

    if (pinIndex >= 0 && pinIndex < 32 && _handlers[pinIndex]) {
      _handlers[pinIndex](LKParam(pinValue.length() > 0 ? pinValue : kv.value().as<String>()));
    }
  }
}

LKIoTClass LKIoT;
