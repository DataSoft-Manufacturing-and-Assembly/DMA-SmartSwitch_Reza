#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// ============================================================
//  DEBUG
// ============================================================
#define DEBUG_MODE 1
#if DEBUG_MODE
  #define LOG(tag, msg) do { Serial.print("["); Serial.print(tag); \
                              Serial.print("] "); Serial.println(msg); } while (0)
  #define LOGF(tag, fmt, ...) do { Serial.print("["); Serial.print(tag); \
                                    Serial.printf("] " fmt "\n", ##__VA_ARGS__); } while (0)
#else
  #define LOG(tag, msg) do {} while (0)
  #define LOGF(tag, fmt, ...) do {} while (0)
#endif

// ============================================================
//  Feature flags
// ============================================================
#define USE_RF_RECEIVER

// ============================================================
//  Firmware info
// ============================================================
#define FIRMWARE_VERSION       "WiFi-3.4.0"
#define FIRMWARE_RELEASE_DATE  "28-Nov-2025"
#define HARDWARE_VERSION       "3.26.1"

// ============================================================
//  Device ID
//  Set CHANGE_DEVICE_ID to 1 for one flash, then back to 0
// ============================================================
#define CHANGE_DEVICE_ID 0
#if CHANGE_DEVICE_ID
  #define WORK_PACKAGE         "1225"
  #define GW_TYPE              "10"
  #define FIRMWARE_UPDATE_DATE "251015"
  #define DEVICE_SERIAL        "0031"
#endif

// ============================================================
//  Pins
// ============================================================
#define LED_PIN            4       // data pin for the status LED
#define RELAY_LIGHT1_PIN   25
#define RELAY_LIGHT2_PIN   26
#define RELAY_FAN_PIN      27
#define TOUCH_LIGHT1_PIN   21
#define TOUCH_LIGHT2_PIN   22
#define TOUCH_FAN_PIN      23
#define FAN_UP_PIN         32
#define FAN_DOWN_PIN       33
#define RF_PIN             32

// ============================================================
//  Timing
// ============================================================
#define HB_INTERVAL_MS     (5UL * 60UL * 1000UL)   // heartbeat every 5 min
#define DEBOUNCE_MS        300                     // touch debounce
#define AP_HOLD_MS         5000                    // both light1+light2 held
#define WIFI_TRY_MS        10000                   // WiFi try window
#define WIFI_REST_MS       20000                   // WiFi rest window
#define MQTT_TRY_MS        5000                    // MQTT try window
#define MQTT_REST_MS       20000                   // MQTT rest window

// ============================================================
//  MQTT queue
// ============================================================
#define MQTT_QUEUE_SIZE   8
#define MQTT_TOPIC_MAX    64
#define MQTT_PAYLOAD_MAX  384
#define MQTT_RX_BUFFER    640

struct MqttOutMsg {
    char topic[MQTT_TOPIC_MAX];
    char payload[MQTT_PAYLOAD_MAX];
};

// ============================================================
//  Global objects (defined in config.cpp)
// ============================================================
extern const char* MQTT_SERVER;
extern const char* MQTT_USER;
extern const char* MQTT_PASS;
extern const char* TOPIC_PUB;      // device → server (all messages)
extern const char* TOPIC_SUB;      // server → device  (subscribe at SUB/<device_id>)
extern const char* OTA_URL;

extern Preferences  prefs;
extern WiFiManager  wifiManager;
extern WiFiClient   wifiClient;
extern PubSubClient mqtt;

extern char        g_deviceId[32];
extern const char* DEVICE_ID;

extern QueueHandle_t mqttOutQueue;

// ============================================================
//  Task handles
// ============================================================
extern TaskHandle_t taskNetwork;
extern TaskHandle_t taskMain;
extern TaskHandle_t taskSwitches;
extern TaskHandle_t taskWifiReset;
extern TaskHandle_t taskOta;
extern TaskHandle_t taskRf;