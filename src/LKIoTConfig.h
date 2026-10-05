#pragma once

/**
 * LKTRONICS-IOT CLOUD PROVISIONING & BROKER SETTINGS
 * Default credentials are now securely provisioned over HTTPS using
 * the Project API Key, eliminating exposed passwords from the public library.
 */
#define LK_DEFAULT_AUTH_URL "https://iotdashboardv2.lk-tronics.com/api_device_auth.php"
#define LK_DEFAULT_BROKER   "lktvps.vps.webdock.cloud"
#define LK_DEFAULT_PORT     8883
#define LK_DEFAULT_USER     "iot_dashboard"
// Password is protected: retrieved dynamically over HTTPS or passed in sketch
#define LK_DEFAULT_PASS     ""

#if defined(__AVR__)
  // Arduino Uno / Nano / Mega memory optimizations (2KB RAM)
  #define LK_BUFFER_SIZE      256
  #define LK_JSON_DOC_SIZE    192
  #define LK_KEEPALIVE        30
  #define LK_SOCKET_TIMEOUT   5
  #define LK_FLUSH_INTERVAL   150
#else
  // ESP32 / ESP8266 / ARM / RP2040
  #define LK_BUFFER_SIZE      1024
  #define LK_JSON_DOC_SIZE    768
  #define LK_KEEPALIVE        30
  #define LK_SOCKET_TIMEOUT   10
  #define LK_FLUSH_INTERVAL   100
#endif
