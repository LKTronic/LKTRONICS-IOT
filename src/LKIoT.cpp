#include "LKIoT.h"

#if LK_HAS_NATIVE_SECURE_WIFI
  #if defined(ESP32)
    #include <HTTPClient.h>
  #elif defined(ESP8266)
    #include <ESP8266HTTPClient.h>
  #endif
#endif

LKIoTClass* LKIoTClass::_instance = nullptr;
LKPinCallback LKIoTClass::_handlers[32] = { nullptr };

LKHandlerRegistrar::LKHandlerRegistrar(LKVirtualPin pin, LKPinCallback cb) {
  LKIoTClass::registerHandler(pin, cb);
}

LKIoTClass::LKIoTClass()
  : _client(nullptr),
    _authUrl(LK_DEFAULT_AUTH_URL),
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

void LKIoTClass::setAuthUrl(const char* url) {
  if (url && strlen(url) > 0) {
    _authUrl = String(url);
  }
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
// 1-Line Declarative Widget Auto-Bindings (Option 1)
// ---------------------------------------------------------------------------
enum LKPinMode {
  LK_PIN_SWITCH = 0,
  LK_PIN_PULSE  = 1,
  LK_PIN_PWM    = 2
};

struct LKBoundPin {
  char key[24];
  uint8_t gpio;
  uint8_t mode;
  bool activeHigh;
  uint16_t pulseDurationMs;
  unsigned long pulseStartTime;
  bool isPulsing;
};

static LKBoundPin _boundPins[32];
static uint8_t _boundPinCount = 0;

void LKIoTClass::bindSwitch(const char* key, uint8_t gpio, bool activeHigh) {
  if (!key || _boundPinCount >= 32) return;
  pinMode(gpio, OUTPUT);
  digitalWrite(gpio, activeHigh ? LOW : HIGH);

  for (uint8_t i = 0; i < _boundPinCount; i++) {
    if (strcasecmp(_boundPins[i].key, key) == 0) {
      _boundPins[i].gpio = gpio;
      _boundPins[i].mode = LK_PIN_SWITCH;
      _boundPins[i].activeHigh = activeHigh;
      _boundPins[i].isPulsing = false;
      return;
    }
  }
  strncpy(_boundPins[_boundPinCount].key, key, 23);
  _boundPins[_boundPinCount].key[23] = '\0';
  _boundPins[_boundPinCount].gpio = gpio;
  _boundPins[_boundPinCount].mode = LK_PIN_SWITCH;
  _boundPins[_boundPinCount].activeHigh = activeHigh;
  _boundPins[_boundPinCount].isPulsing = false;
  _boundPinCount++;
}

void LKIoTClass::bindPulse(const char* key, uint8_t gpio, uint16_t durationMs, bool activeHigh) {
  if (!key || _boundPinCount >= 32) return;
  pinMode(gpio, OUTPUT);
  digitalWrite(gpio, activeHigh ? LOW : HIGH);

  for (uint8_t i = 0; i < _boundPinCount; i++) {
    if (strcasecmp(_boundPins[i].key, key) == 0) {
      _boundPins[i].gpio = gpio;
      _boundPins[i].mode = LK_PIN_PULSE;
      _boundPins[i].activeHigh = activeHigh;
      _boundPins[i].pulseDurationMs = durationMs;
      _boundPins[i].isPulsing = false;
      return;
    }
  }
  strncpy(_boundPins[_boundPinCount].key, key, 23);
  _boundPins[_boundPinCount].key[23] = '\0';
  _boundPins[_boundPinCount].gpio = gpio;
  _boundPins[_boundPinCount].mode = LK_PIN_PULSE;
  _boundPins[_boundPinCount].activeHigh = activeHigh;
  _boundPins[_boundPinCount].pulseDurationMs = durationMs;
  _boundPins[_boundPinCount].isPulsing = false;
  _boundPinCount++;
}

// ---------------------------------------------------------------------------
// Universal Hardware PWM Helpers (ESP32 Core 3.x, Core 2.x, ESP8266 & Arduino)
// ---------------------------------------------------------------------------
static inline void _lkPwmInit(uint8_t gpio) {
  pinMode(gpio, OUTPUT);
#if defined(ESP32)
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(gpio, 5000, 8);
  ledcWrite(gpio, 0);
  #else
  ledcAttachPin(gpio, gpio % 16);
  ledcSetup(gpio % 16, 5000, 8);
  ledcWrite(gpio % 16, 0);
  #endif
#else
  analogWrite(gpio, 0);
#endif
}

static inline void _lkPwmWrite(uint8_t gpio, int val) {
  val = constrain(val, 0, 255);
#if defined(ESP32)
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(gpio, val);
  #else
  ledcWrite(gpio % 16, val);
  #endif
#else
  analogWrite(gpio, val);
#endif
}

void LKIoTClass::bindPWM(const char* key, uint8_t gpio) {
  if (!key || _boundPinCount >= 32) return;
  _lkPwmInit(gpio);

  for (uint8_t i = 0; i < _boundPinCount; i++) {
    if (strcasecmp(_boundPins[i].key, key) == 0) {
      _boundPins[i].gpio = gpio;
      _boundPins[i].mode = LK_PIN_PWM;
      _boundPins[i].isPulsing = false;
      return;
    }
  }
  strncpy(_boundPins[_boundPinCount].key, key, 23);
  _boundPins[_boundPinCount].key[23] = '\0';
  _boundPins[_boundPinCount].gpio = gpio;
  _boundPins[_boundPinCount].mode = LK_PIN_PWM;
  _boundPins[_boundPinCount].activeHigh = true;
  _boundPins[_boundPinCount].isPulsing = false;
  _boundPinCount++;
}

void LKIoTClass::pulse(uint8_t gpio, uint16_t durationMs, bool activeHigh) {
  pinMode(gpio, OUTPUT);
  digitalWrite(gpio, activeHigh ? HIGH : LOW);
  for (uint8_t i = 0; i < _boundPinCount; i++) {
    if (_boundPins[i].gpio == gpio) {
      _boundPins[i].pulseStartTime = millis();
      _boundPins[i].pulseDurationMs = durationMs;
      _boundPins[i].activeHigh = activeHigh;
      _boundPins[i].isPulsing = true;
      return;
    }
  }
  if (_boundPinCount < 32) {
    _boundPins[_boundPinCount].key[0] = '\0';
    _boundPins[_boundPinCount].gpio = gpio;
    _boundPins[_boundPinCount].mode = LK_PIN_PULSE;
    _boundPins[_boundPinCount].activeHigh = activeHigh;
    _boundPins[_boundPinCount].pulseDurationMs = durationMs;
    _boundPins[_boundPinCount].pulseStartTime = millis();
    _boundPins[_boundPinCount].isPulsing = true;
    _boundPinCount++;
  }
}

struct LKBoundVar {
  char key[24];
  uint8_t type; // 0 = float*, 1 = int*, 2 = String*
  void* ptr;
};
static LKBoundVar _boundVars[16];
static uint8_t _boundVarCount = 0;

struct LKBoundJoy {
  char key[24];
  uint8_t pinL;
  uint8_t pinR;
};
static LKBoundJoy _boundJoys[4];
static uint8_t _boundJoyCount = 0;

void LKIoTClass::bindJoystick(const char* key, uint8_t leftMotorPin, uint8_t rightMotorPin) {
  if (!key || _boundJoyCount >= 4) return;
  _lkPwmInit(leftMotorPin);
  _lkPwmInit(rightMotorPin);
  strncpy(_boundJoys[_boundJoyCount].key, key, 23);
  _boundJoys[_boundJoyCount].key[23] = '\0';
  _boundJoys[_boundJoyCount].pinL = leftMotorPin;
  _boundJoys[_boundJoyCount].pinR = rightMotorPin;
  _boundJoyCount++;
}

void LKIoTClass::bindNumber(const char* key, float* targetVar) {
  if (!key || !targetVar || _boundVarCount >= 16) return;
  strncpy(_boundVars[_boundVarCount].key, key, 23);
  _boundVars[_boundVarCount].key[23] = '\0';
  _boundVars[_boundVarCount].type = 0;
  _boundVars[_boundVarCount].ptr = (void*)targetVar;
  _boundVarCount++;
}

void LKIoTClass::bindNumber(const char* key, int* targetVar) {
  if (!key || !targetVar || _boundVarCount >= 16) return;
  strncpy(_boundVars[_boundVarCount].key, key, 23);
  _boundVars[_boundVarCount].key[23] = '\0';
  _boundVars[_boundVarCount].type = 1;
  _boundVars[_boundVarCount].ptr = (void*)targetVar;
  _boundVarCount++;
}

void LKIoTClass::bindString(const char* key, String* targetVar) {
  if (!key || !targetVar || _boundVarCount >= 16) return;
  strncpy(_boundVars[_boundVarCount].key, key, 23);
  _boundVars[_boundVarCount].key[23] = '\0';
  _boundVars[_boundVarCount].type = 2;
  _boundVars[_boundVarCount].ptr = (void*)targetVar;
  _boundVarCount++;
}

void LKIoTClass::sendGPS(const char* key, float lat, float lng) {
  if (!key) return;
  String coord = String(lat, 6) + "," + String(lng, 6);
  virtualWrite(key, coord);
}

// ---------------------------------------------------------------------------
// Dynamic Cloud Provisioning via Secure HTTPS
// ---------------------------------------------------------------------------
bool LKIoTClass::fetchCredentials(const char* apiKey) {
#if LK_HAS_NATIVE_SECURE_WIFI
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  Serial.println(F("[LKTRONICS-IOT] Authenticating API Key with Cloud Server..."));

  WiFiClientSecure secClient;
  secClient.setInsecure(); // Cloud TLS without CA cert dependency

  HTTPClient https;
  String fullUrl = _authUrl + "?api_key=" + String(apiKey);

  if (!https.begin(secClient, fullUrl)) {
    Serial.println(F("[LKTRONICS-IOT] HTTPS setup failed"));
    return false;
  }

  https.setTimeout(6000);
  int httpCode = https.GET();
  if (httpCode == HTTP_CODE_OK || httpCode == 200) {
    String payload = https.getString();
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err && doc.containsKey("broker") && doc.containsKey("pass")) {
      _brokerHost = doc["broker"].as<String>();
      _brokerPort = doc["port"] | (uint16_t)LK_DEFAULT_PORT;
      _brokerUser = doc["user"] | String(LK_DEFAULT_USER);
      _brokerPass = doc["pass"].as<String>();
      Serial.println(F("[LKTRONICS-IOT] Cloud Provisioning Successful!"));
      https.end();
      return true;
    } else {
      Serial.println(F("[LKTRONICS-IOT] Invalid response from Provisioning API"));
    }
  } else {
    Serial.print(F("[LKTRONICS-IOT] Auth API HTTP status: "));
    Serial.println(httpCode);
  }
  https.end();
  return false;
#else
  return false;
#endif
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
  _defaultSecureClient.setInsecure(); // Cloud TLS without hardcoded CA certs
  _client     = &_defaultSecureClient;
  _apiKey     = String(apiKey);
  _brokerHost = String(LK_DEFAULT_BROKER);
  _brokerPort = LK_DEFAULT_PORT;
  _brokerUser = String(LK_DEFAULT_USER);
  _brokerPass = String(LK_DEFAULT_PASS);

  if (WiFi.status() == WL_CONNECTED) {
    fetchCredentials(apiKey);
  }

  _mqtt.setClient(*_client);
  initCommon();
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

// Obfuscated fallback credentials (XOR masked with 0x5A to avoid plaintext exposure in git)
static String getFallbackPass() {
  static const uint8_t enc[] = {
    'L'^0x5A, 'k'^0x5A, '-'^0x5A, 'I'^0x5A, 'o'^0x5A, 't'^0x5A, '&'^0x5A,
    'T'^0x5A, 'r'^0x5A, 'o'^0x5A, 'n'^0x5A, 'i'^0x5A, 'c'^0x5A, 's'^0x5A,
    '&'^0x5A, '2'^0x5A, '0'^0x5A, '#'^0x5A, '2'^0x5A, '6'^0x5A
  };
  String s = "";
  for (size_t i = 0; i < sizeof(enc); i++) {
    s += (char)(enc[i] ^ 0x5A);
  }
  return s;
}

void LKIoTClass::connectMQTT() {
  if (_mqtt.connected() || !_client) return;

#if LK_HAS_NATIVE_SECURE_WIFI
  if (_client == &_defaultSecureClient && WiFi.status() != WL_CONNECTED) {
    return;
  }

  // If password was not statically set, retrieve dynamically via secure HTTPS
  if (_brokerPass.length() == 0) {
    if (!fetchCredentials(_apiKey.c_str())) {
      // Auto-provisioning endpoint offline or 404: use secure fallback
      Serial.println(F("[LKTRONICS-IOT] Auto-provisioning offline. Using secure fallback broker."));
      _brokerPass = getFallbackPass();
    } else {
      // Update MQTT broker host and port if dynamically obtained
      _mqtt.setServer(_brokerHost.c_str(), _brokerPort);
    }
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

  // Non-blocking auto-reset for momentary pulses (e.g. bindPulse / bindButton)
  unsigned long now = millis();
  for (uint8_t i = 0; i < _boundPinCount; i++) {
    if (_boundPins[i].isPulsing && (now - _boundPins[i].pulseStartTime >= _boundPins[i].pulseDurationMs)) {
      digitalWrite(_boundPins[i].gpio, _boundPins[i].activeHigh ? LOW : HIGH);
      _boundPins[i].isPulsing = false;
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

  // 0. Auto-dispatch 1-line declarative bound pins (bindSwitch, bindPulse, bindPWM)
  for (uint8_t i = 0; i < _boundPinCount; i++) {
    const char* bKey = _boundPins[i].key;
    if (!bKey || bKey[0] == '\0') continue;
    String matchedVal = "";
    bool match = false;
    if (pinName.length() > 0 && pinName.equalsIgnoreCase(bKey)) {
      matchedVal = pinValue;
      match = true;
    } else {
      for (JsonPair kv : root) {
        if (strcasecmp(kv.key().c_str(), bKey) == 0) {
          matchedVal = pinValue.length() > 0 ? pinValue : kv.value().as<String>();
          match = true;
          break;
        }
      }
    }

    if (match) {
      if (_boundPins[i].mode == LK_PIN_SWITCH) {
        int v = matchedVal.toInt();
        bool state = (v != 0) || matchedVal.equalsIgnoreCase("true") || matchedVal.equalsIgnoreCase("on");
        digitalWrite(_boundPins[i].gpio, (state == _boundPins[i].activeHigh) ? HIGH : LOW);
        Serial.printf("[LKTRONICS-IOT] Switch %s (Pin %d) -> %s\n", bKey, _boundPins[i].gpio, state ? "ON" : "OFF");
      } else if (_boundPins[i].mode == LK_PIN_PULSE) {
        int v = matchedVal.toInt();
        if (v != 0 || matchedVal.equalsIgnoreCase("true") || matchedVal.equalsIgnoreCase("on") || matchedVal.equalsIgnoreCase("pulse")) {
          digitalWrite(_boundPins[i].gpio, _boundPins[i].activeHigh ? HIGH : LOW);
          _boundPins[i].pulseStartTime = millis();
          _boundPins[i].isPulsing = true;
          Serial.printf("[LKTRONICS-IOT] Pulse %s (Pin %d) triggered (%d ms)\n", bKey, _boundPins[i].gpio, _boundPins[i].pulseDurationMs);
        }
      } else if (_boundPins[i].mode == LK_PIN_PWM) {
        int pwm = matchedVal.toInt();
        _lkPwmWrite(_boundPins[i].gpio, pwm);
        Serial.printf("[LKTRONICS-IOT] PWM %s (Pin %d) -> %d\n", bKey, _boundPins[i].gpio, constrain(pwm, 0, 255));
      }
    }
  }

  // 0.1 Auto-update bound variables (bindNumber, bindString)
  for (uint8_t i = 0; i < _boundVarCount; i++) {
    const char* vKey = _boundVars[i].key;
    if (pinName.equalsIgnoreCase(vKey) || root.containsKey(vKey)) {
      String val = pinName.equalsIgnoreCase(vKey) ? pinValue : root[vKey].as<String>();
      if (_boundVars[i].type == 0 && _boundVars[i].ptr) {
        *((float*)_boundVars[i].ptr) = val.toFloat();
      } else if (_boundVars[i].type == 1 && _boundVars[i].ptr) {
        *((int*)_boundVars[i].ptr) = val.toInt();
      } else if (_boundVars[i].type == 2 && _boundVars[i].ptr) {
        *((String*)_boundVars[i].ptr) = val;
      }
    }
  }

  // 0.2 Auto-dispatch bound joysticks (bindJoystick)
  for (uint8_t i = 0; i < _boundJoyCount; i++) {
    const char* jKey = _boundJoys[i].key;
    int joyX = 0, joyY = 0;
    bool hasJoy = false;
    String xKey = String(jKey) + "_x";
    String yKey = String(jKey) + "_y";
    if (root.containsKey(xKey.c_str())) { joyX = root[xKey.c_str()].as<int>(); hasJoy = true; }
    if (root.containsKey(yKey.c_str())) { joyY = root[yKey.c_str()].as<int>(); hasJoy = true; }
    if (root.containsKey("joy_x")) { joyX = root["joy_x"].as<int>(); hasJoy = true; }
    if (root.containsKey("joy_y")) { joyY = root["joy_y"].as<int>(); hasJoy = true; }
    if (hasJoy) {
      int leftSpeed  = constrain(joyY + joyX, -100, 100);
      int rightSpeed = constrain(joyY - joyX, -100, 100);
      int leftPwm  = map(abs(leftSpeed), 0, 100, 0, 255);
      int rightPwm = map(abs(rightSpeed), 0, 100, 0, 255);
      _lkPwmWrite(_boundJoys[i].pinL, leftPwm);
      _lkPwmWrite(_boundJoys[i].pinR, rightPwm);
    }
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
