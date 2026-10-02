#pragma once

/**
 * LKIoT DEFAULT CLOUD BROKER SETTINGS
 * Dedicated LK-TRONICS VPS Mosquitto TLS Broker (Port 8883).
 */
#define LK_DEFAULT_BROKER   "lktvps.vps.webdock.cloud"
#define LK_DEFAULT_PORT     8883
#define LK_DEFAULT_USER     "iot_dashboard"
#define LK_DEFAULT_PASS     "LK#tronics#Iot_mqtt12"

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
