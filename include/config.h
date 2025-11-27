#define DEBUG_MODE true
// #define USE_RF_RECEIVER
#define USE_Fast_LED
#define HB_INTERVAL 5*60*1000
// #define DATA_INTERVAL 15*1000
#define CONFIG_TASK_WDT_DEBUG 1

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>  // WiFiManager library
#include <PubSubClient.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include <Preferences.h>

#ifdef USE_RF_RECEIVER
    #include <RCSwitch.h>
    RCSwitch mySwitch = RCSwitch();

    #include <map>
    std::map<unsigned long, unsigned long> lastRFReceivedTimeMap;
    unsigned long lastRFGlobalReceivedTime = 0;  // Global debounce

    #define RF_PIN 15  
#endif

//Device ID Configuration
#define CHANGE_DEICE_ID 0

#if CHANGE_DEICE_ID
    #define WORK_PACKAGE "1225"
    #define GW_TYPE "10"
    #define FIRMWARE_UPDATE_DATE "251015" 
    #define DEVICE_SERIAL "0097"
    //#define DEVICE_ID WORK_PACKAGE GW_TYPE FIRMWARE_UPDATE_DATE DEVICE_SERIAL
#endif

const char* DEVICE_ID;
//=============================================================//

// Switch Pin Definitions
#define SW_PIN1 25  
#define SW_PIN2 26  
#define SW_PIN3 27 
#define SW_PIN4 14
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

const char* mqtt_server = "broker2.dma-bd.com";
const char* mqtt_user = "broker2";
const char* mqtt_password = "Secret!@#$1234";
const char* mqtt_hb_topic = "DMA/SmartSwitch/HB";
const char* mqtt_pub_topic = "DMA/SmartSwitch/PUB";
const char* mqtt_sub_topic = "DMA/SmartSwitch/SUB";
const char* ota_url = "https://raw.githubusercontent.com/DataSoft-Manufacturing-and-Assembly/DMA-SmartSwitch_Reza/main/ota/firmware.bin";
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
TaskHandle_t mainTaskHandle;
TaskHandle_t wifiResetTaskHandle = NULL;
TaskHandle_t otaTaskHandle = NULL;

#define WIFI_RESET_BUTTON_PIN 0
bool wifiResetFlag = false;
