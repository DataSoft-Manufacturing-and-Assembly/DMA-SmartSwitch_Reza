#include "config.h"

const char* MQTT_SERVER = "broker2.dma-bd.com";
const char* MQTT_USER   = "broker2";
const char* MQTT_PASS   = "Secret!@#$1234";
const char* TOPIC_PUB   = "DMA/SmartSwitch/PUB";
const char* TOPIC_SUB   = "DMA/SmartSwitch/SUB";
const char* OTA_URL     =
    "https://raw.githubusercontent.com/DataSoft-Manufacturing-and-Assembly/"
    "DMA-SmartSwitch_Reza/main/ota/firmware.bin";

Preferences  prefs;
WiFiManager  wifiManager;
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

char        g_deviceId[32] = "UNKNOWN";
const char* DEVICE_ID      = g_deviceId;

QueueHandle_t mqttOutQueue = nullptr;

TaskHandle_t taskNetwork   = nullptr;
TaskHandle_t taskMain      = nullptr;
TaskHandle_t taskSwitches  = nullptr;
TaskHandle_t taskWifiReset = nullptr;
TaskHandle_t taskOta       = nullptr;
TaskHandle_t taskRf        = nullptr;