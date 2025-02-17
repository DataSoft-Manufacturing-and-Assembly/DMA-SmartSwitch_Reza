#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>  // WiFiManager library
#include <PubSubClient.h>
// #include <FastLED.h>

#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

#include <Preferences.h>

Preferences preferences; // Create a Preferences object

#define RF_PIN 25  
#define SW_PIN1 18  
#define SW_PIN2 19  
#define SW_PIN3 22  
#define SW_PIN4 23

// Configuration Section
#define Fast_LED false
#define DEBUG_MODE true
#define DEBUG_PRINT(x)  if (DEBUG_MODE) { Serial.print(x); }
#define DEBUG_PRINTLN(x) if (DEBUG_MODE) { Serial.println(x); }

#define WORK_PACKAGE "1225"
#define GW_TYPE "00"
#define FIRMWARE_UPDATE_DATE "250212" 
#define DEVICE_SERIAL "0001"
#define DEVICE_ID WORK_PACKAGE GW_TYPE FIRMWARE_UPDATE_DATE DEVICE_SERIAL

#define HB_INTERVAL 30*1000
#define DATA_INTERVAL 15*1000

// WiFi and MQTT reconnection time config
#define WIFI_ATTEMPT_COUNT 30
#define WIFI_ATTEMPT_DELAY 1000
#define WIFI_WAIT_COUNT 60
#define WIFI_WAIT_DELAY 1000
#define MAX_WIFI_ATTEMPTS 2
#define MQTT_ATTEMPT_COUNT 10
#define MQTT_ATTEMPT_DELAY 5000

int wifiAttemptCount = WIFI_ATTEMPT_COUNT;
int wifiWaitCount = WIFI_WAIT_COUNT;
int maxWifiAttempts = MAX_WIFI_ATTEMPTS;
int mqttAttemptCount = MQTT_ATTEMPT_COUNT;

const char* mqtt_server = "broker2.dma-bd.com";
const char* mqtt_user = "broker2";
const char* mqtt_password = "Secret!@#$1234";
const char* mqtt_hb_topic = "DMA/SmartSwitch/HB";
const char* mqtt_pub_topic = "DMA/SmartSwitch/PUB";
const char* mqtt_sub_topic = "DMA/SmartSwitch/SUB";

#if Fast_LED
#define DATA_PIN 4
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
#endif

WiFiManager wm;
WiFiClient espClient;
PubSubClient client(espClient);

TaskHandle_t networkTaskHandle;
TaskHandle_t mainTaskHandle;
TaskHandle_t wifiResetTaskHandle;

#define WIFI_RESET_BUTTON_PIN 0
bool wifiResetFlag = false;

// Function to reconnect to WiFi
void reconnectWiFi() {
  // digitalWrite(LED_PIN, HIGH);
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiAttemptCount > 0) {
      DEBUG_PRINTLN("Attempting WiFi connection...");
      WiFi.begin();  // Use saved credentials
      wifiAttemptCount--;
      DEBUG_PRINTLN("Remaining WiFi attempts: " + String(wifiAttemptCount));
      // vTaskDelay(WIFI_ATTEMPT_DELAY / portTICK_PERIOD_MS);
      vTaskDelay(pdMS_TO_TICKS(WIFI_ATTEMPT_DELAY));
    } else if (wifiWaitCount > 0) {
      wifiWaitCount--;
      DEBUG_PRINTLN("WiFi wait... retrying in a moment");
      DEBUG_PRINTLN("Remaining WiFi wait time: " + String(wifiWaitCount) + " seconds");
      vTaskDelay(pdMS_TO_TICKS(WIFI_WAIT_DELAY));
    } else {
      wifiAttemptCount = WIFI_ATTEMPT_COUNT;
      wifiWaitCount = WIFI_WAIT_COUNT;
      maxWifiAttempts--;
      if (maxWifiAttempts <= 0) {
        DEBUG_PRINTLN("Max WiFi attempt cycles exceeded, restarting...");
        ESP.restart();
      }
    }
  }
}


void reconnectMQTT() {
  if (!client.connected()) {
    #if Fast_LED
    leds[0] = CRGB::Yellow;
    FastLED.show();
    #endif

    char clientId[24];
    snprintf(clientId, sizeof(clientId), "dma_ssw_%04X%04X%04X", random(0xffff), random(0xffff), random(0xffff));

    if (mqttAttemptCount > 0) {
      DEBUG_PRINTLN("Attempting MQTT connection...");
      if (client.connect(clientId, mqtt_user, mqtt_password)) {
        DEBUG_PRINTLN("MQTT connected");

        #if Fast_LED
        leds[0] = CRGB::Black;
        FastLED.show();
        #endif
        char topic[48];

        snprintf(topic, sizeof(topic), "%s/%s", mqtt_sub_topic, DEVICE_ID);
        client.subscribe(topic);
      } else {
        DEBUG_PRINTLN("MQTT connection failed");
        mqttAttemptCount--;
        vTaskDelay(pdMS_TO_TICKS(MQTT_ATTEMPT_DELAY));
      }
    } else {
      DEBUG_PRINTLN("Max MQTT attempts exceeded, restarting...");
      ESP.restart();
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  DEBUG_PRINTLN("Message arrived on topic: " + String(topic));
  DEBUG_PRINTLN("Message content: " + message);

  preferences.begin("switches", false);  // Open Preferences storage

  if (message == "sw1:1") {
    DEBUG_PRINTLN("Switch-1: On");
    digitalWrite(SW_PIN1, HIGH);
    preferences.putBool("sw1", true);  // Save state
    char data[32];
    snprintf(data, sizeof(data), "%s,sw1:1", DEVICE_ID); 
    client.publish(mqtt_pub_topic, data);
    DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data))
  } 
  else if (message == "sw1:0") {
      DEBUG_PRINTLN("Switch-1: Off");
      digitalWrite(SW_PIN1, LOW);
      preferences.putBool("sw1", false); 
      char data[32];
      snprintf(data, sizeof(data), "%s,sw1:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }

  if (message == "sw2:1") {
      DEBUG_PRINTLN("Switch-2: On");
      digitalWrite(SW_PIN2, HIGH);
      preferences.putBool("sw2", true);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw2:1", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  } 
  else if (message == "sw2:0") {
      DEBUG_PRINTLN("Switch-2: Off");
      digitalWrite(SW_PIN2, LOW);
      preferences.putBool("sw2", false);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw2:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }

  if (message == "sw3:1") {
      DEBUG_PRINTLN("Switch-3: On");
      digitalWrite(SW_PIN3, HIGH);
      preferences.putBool("sw3", true);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw3:1", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  } 
  else if (message == "sw3:0") {
      DEBUG_PRINTLN("Switch-3: Off");
      digitalWrite(SW_PIN3, LOW);
      preferences.putBool("sw3", false);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw3:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }

  if (message == "sw4:1") {
      DEBUG_PRINTLN("Switch-4: On");
      digitalWrite(SW_PIN4, HIGH);
      preferences.putBool("sw4", true);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw4:1", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  } 
  else if (message == "sw4:0") {
      DEBUG_PRINTLN("Switch-4: Off");
      digitalWrite(SW_PIN4, LOW);
      preferences.putBool("sw4", false);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw4:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }


  preferences.end();  // Close Preferences storage
}


void networkTask(void *param) {
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        reconnectMQTT();
      }
    } else {
      reconnectWiFi();
    }
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void wifiResetTask(void *param) {
  for (;;) {
    if (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
      unsigned long pressStartTime = millis();
      DEBUG_PRINTLN("Button Pressed....");

      #if Fast_LED
      leds[0] = CRGB::Blue;
      FastLED.show();
      #endif

      while (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
        if (millis() - pressStartTime >= 5000) {
          DEBUG_PRINTLN("5 seconds holding time reached, starting WiFiManager...");
          vTaskSuspend(networkTaskHandle);
          vTaskSuspend(mainTaskHandle);
          wm.resetSettings();
          wm.autoConnect("DMA_Smart_Switch");
          ESP.restart();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    } else {
      #if Fast_LED
      leds[0] = CRGB::Black;
      FastLED.show();
      #endif
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/*
void mainTask(void *param) {
  for (;;) {
    static unsigned long last_hb_send_time = 0;
    if (millis() - last_hb_send_time >= HB_INTERVAL) {
      last_hb_send_time = millis();
      if (client.connected()) {
        char hb_data[50];
        snprintf(hb_data, sizeof(hb_data), "%s,wifi_connected", DEVICE_ID);
        client.publish(mqtt_hb_topic, hb_data);
        DEBUG_PRINTLN("Heartbeat sent Successfully");
        #if Fast_LED
        leds[0] = CRGB::Blue;
        FastLED.show();
        #endif
        vTaskDelay(pdMS_TO_TICKS(1000));
        #if Fast_LED
        leds[0] = CRGB::Black;
        FastLED.show();
        #endif
      } else {
        DEBUG_PRINTLN("Failed to publish Heartbeat on MQTT");
      }
    }

    
    static unsigned long last_data_send_time = 0;
    if (millis() - last_data_send_time >= DATA_INTERVAL) {
      last_data_send_time = millis();
      #if Fast_LED
      leds[0] = CRGB::Green;
      FastLED.show();
      #endif
      vTaskDelay(pdMS_TO_TICKS(1000));
      #if Fast_LED
      leds[0] = CRGB::Black;
      FastLED.show();
      #endif
    }
    

    if (mySwitch.available()) {
      unsigned long receivedCode = mySwitch.getReceivedValue();
      Serial.print("Received RF Code: ");
      Serial.println(receivedCode);
  
      char data[32];
      snprintf(data, sizeof(data), "%s,%lu", DEVICE_ID, receivedCode); 
  
      client.publish(mqtt_pub_topic, data);
      mySwitch.resetAvailable();
  }
  

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

*/

void mainTask(void *param) {
  unsigned long lastReceivedTime = 0;  
  unsigned long lastReceivedCode = 0;

  for (;;) {
    static unsigned long last_hb_send_time = 0;
    unsigned long now = millis();

    // **Send Heartbeat Every HB_INTERVAL**
    if (now - last_hb_send_time >= HB_INTERVAL) {
      last_hb_send_time = now;
      if (client.connected()) {
        char hb_data[50];
        snprintf(hb_data, sizeof(hb_data), "%s,wifi_connected", DEVICE_ID);
        client.publish(mqtt_hb_topic, hb_data);
        DEBUG_PRINTLN("Heartbeat sent Successfully");

        #if Fast_LED
        leds[0] = CRGB::Blue;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
        #endif
      } else {
        DEBUG_PRINTLN("Failed to publish Heartbeat on MQTT");
      }
    }

    // **Handle RF Signal Reception with Debounce**
    if (mySwitch.available()) {
      unsigned long receivedCode = mySwitch.getReceivedValue();

      // **Debounce Check: Ignore Repeats Within 500ms**
      if (receivedCode != lastReceivedCode || now - lastReceivedTime > 2000) {  
        lastReceivedTime = now;
        lastReceivedCode = receivedCode;
    
        // Correct Debug Print with Macro
        DEBUG_PRINTLN(String("RF Received: ") + String(receivedCode));
    
        char data[50];
        snprintf(data, sizeof(data), "%s,%lu", DEVICE_ID, receivedCode); 
        client.publish(mqtt_pub_topic, data);
    
        // Correct Debug Print for MQTT Data
        DEBUG_PRINTLN(String("Data Sent to MQTT: ") + String(data));
      }
    
      mySwitch.resetAvailable();
    }

    vTaskDelay(1); // Keep FreeRTOS responsive
  }
}

void setup() {
  Serial.begin(115200);
  DEBUG_PRINT("Device ID: ");
  DEBUG_PRINTLN(DEVICE_ID);
  
  #if Fast_LED
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  leds[0] = CRGB::Black;
  FastLED.show();
  #endif

  pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);

  mySwitch.enableReceive(digitalPinToInterrupt(RF_PIN));

  pinMode(SW_PIN1, OUTPUT);
  pinMode(SW_PIN2, OUTPUT);
  pinMode(SW_PIN3, OUTPUT);
  pinMode(SW_PIN4, OUTPUT);

  digitalWrite(SW_PIN1, LOW);
  digitalWrite(SW_PIN2, LOW);
  digitalWrite(SW_PIN3, LOW);
  digitalWrite(SW_PIN4, LOW);

  preferences.begin("switches", true);  // Open Preferences in read-only mode

  digitalWrite(SW_PIN1, preferences.getBool("sw1", false)); // Default: OFF
  digitalWrite(SW_PIN2, preferences.getBool("sw2", false));
  digitalWrite(SW_PIN3, preferences.getBool("sw3", false));
  digitalWrite(SW_PIN4, preferences.getBool("sw4", false));

  preferences.end();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  xTaskCreatePinnedToCore(networkTask, "Network Task", 8*1024, NULL, 1, &networkTaskHandle, 0);
  xTaskCreatePinnedToCore(mainTask, "Main Task", 16*1024, NULL, 1, &mainTaskHandle, 1);
  xTaskCreatePinnedToCore(wifiResetTask, "WiFi Reset Task", 8*1024, NULL, 1, &wifiResetTaskHandle, 1);
}

void loop(){

}