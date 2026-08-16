#include <credentials.h>
//=============================================================//

bool wifiMode = false; // false for normal mode, true for calibration mode

#define DEBUG_MODE true
#define USE_Fast_LED

#if defined(USE_Fast_LED)
    #include <FastLED.h>
#endif

#define HB_INTERVAL 5*60*1000
// #define DATA_INTERVAL 15*1000
#define WIFI_RESET_BUTTON_PIN 0


#define CONFIG_TASK_WDT_DEBUG 1
//=============================================================//

#define FIRMWARE_VERSION "WiFi-1.1.0"
#define FIRMWARE_RELEASE_DATE "08-Mar-2026"
//=============================================================//

// Include necessary libraries
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>  // WiFiManager library
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <HX711.h>

//Device ID Configuration
#define CHANGE_DEICE_ID 0

#if CHANGE_DEICE_ID
    #define WORK_PACKAGE "1285"
    #define GW_TYPE "01"
    #define FIRMWARE_UPDATE_DATE "260308" 
    #define DEVICE_SERIAL "0000"
#endif

const char* DEVICE_ID;
//=============================================================//


// Serial Print Section
#define DEBUG_PRINT(x)  if (DEBUG_MODE) { Serial.print(x); }
#define DEBUG_PRINTLN(x) if (DEBUG_MODE) { Serial.println(x); }
//=============================================================//

// WiFi and MQTT reconnection time config
#define WIFI_ATTEMPT_COUNT 60
#define WIFI_ATTEMPT_DELAY 1000
#define WIFI_WAIT_COUNT 60
#define WIFI_WAIT_DELAY 1000
#define MAX_WIFI_ATTEMPTS 2
#define MQTT_ATTEMPT_COUNT 12
#define MQTT_ATTEMPT_DELAY 5000
//=============================================================//

int wifiAttemptCount = WIFI_ATTEMPT_COUNT;
int wifiWaitCount = WIFI_WAIT_COUNT;
int maxWifiAttempts = MAX_WIFI_ATTEMPTS;
int mqttAttemptCount = MQTT_ATTEMPT_COUNT;
//=============================================================//

// FastLED Configuration
#ifdef USE_Fast_LED
    #define DATA_PIN 4
    #define NUM_LEDS 1
    CRGB leds[NUM_LEDS];
#endif
//=============================================================//

//Making Instances
Preferences preferences;
WiFiManager wm;
WiFiClient espClient;
PubSubClient client(espClient);

TaskHandle_t networkTaskHandle;
TaskHandle_t wifiResetTaskHandle = NULL;
TaskHandle_t otaTaskHandle = NULL;

bool wifiResetFlag = false;

TaskHandle_t mainTaskHandle;
TaskHandle_t serialTaskHandle;
TaskHandle_t scaleTaskHandle;

//=============================================================//

// HX711 Configuration
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 17;
const int MODE_BUTTON_PIN = 0;

HX711 scale;
bool isCalibrationMode = false;
