#include <config.h>

typedef enum
{
  MODE_NORMAL,
  MODE_CALIBRATION
} scaleMode_t;

scaleMode_t currentMode;

typedef struct
{
  float weight;
  long raw;
} scaleData_t;

scaleData_t scaleData;
SemaphoreHandle_t scaleMutex;

// -------------------- Filter Settings --------------------
#define FILTER_SAMPLES 5
#define ZERO_DEADZONE 2.0     // grams
#define PRINT_THRESHOLD 5.0   // grams change required to print

float readFilteredWeight()
{
  float sum = 0;

  for (int i = 0; i < FILTER_SAMPLES; i++)
  {
    sum += scale.get_units(1);
  }

  return sum / FILTER_SAMPLES;
}


//Function Prototypes
void otaTask(void *param);
void wifiResetTask(void *param);
void networkTask(void *param);
void reconnectWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishHeartbeat();
void WifiResetHandle();


void mainTask(void *param);
void serialTask(void *param);
//==================================================================//

//Publish Heartbeat
void publishHeartbeat() {
  if (client.connected()) {
    char hb_data[50];
    snprintf(hb_data, sizeof(hb_data), "%s,wifi_connected", DEVICE_ID);
    client.publish(mqtt_hb_topic, hb_data);
    DEBUG_PRINTLN("Heartbeat sent Successfully");

    #ifdef USE_Fast_LED
      leds[0] = CRGB::Blue;
      FastLED.show();
      vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
      leds[0] = CRGB::Black;
      FastLED.show();
    #endif
  } else {
    DEBUG_PRINTLN("Failed to publish Heartbeat on MQTT");
  }
}
//========================================//

//WiFi Reset Handler
void WifiResetHandle() {
  unsigned long pressStartTime = millis();
  DEBUG_PRINTLN("Button Pressed....");

  #ifdef USE_Fast_LED
    leds[0] = CRGB::Blue;
    FastLED.show();
  #endif

  while (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
    if (millis() - pressStartTime >= 5000) {
      DEBUG_PRINTLN("5 seconds holding time reached, starting WiFiManager...");
      
      if(wifiResetTaskHandle == NULL) {
        xTaskCreatePinnedToCore(wifiResetTask, "WiFi Reset Task", 8*1024, NULL, 1, &wifiResetTaskHandle, 1);
      }
      else{
        Serial.println("WiFi Reset Task already running.");
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  #ifdef USE_Fast_LED
    leds[0] = CRGB::Black;
    FastLED.show();
  #endif
}
//========================================//

// Function to reconnect to WiFi
void reconnectWiFi() {
  // digitalWrite(LED_PIN, HIGH);
  #ifdef USE_Fast_LED
    leds[0] = CRGB::Red;
    FastLED.show();
  #endif

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiAttemptCount > 0) {
      esp_task_wdt_reset();
      DEBUG_PRINTLN("Attempting WiFi connection...");
      WiFi.begin();  // Use saved credentials
      wifiAttemptCount--;
      DEBUG_PRINTLN("Remaining WiFi attempts: " + String(wifiAttemptCount));
      // vTaskDelay(WIFI_ATTEMPT_DELAY / portTICK_PERIOD_MS);
      vTaskDelay(pdMS_TO_TICKS(WIFI_ATTEMPT_DELAY));
    } else if (wifiWaitCount > 0) {
      esp_task_wdt_reset();
      wifiWaitCount--;
      DEBUG_PRINTLN("WiFi wait... retrying in a moment");
      DEBUG_PRINTLN("Remaining WiFi wait time: " + String(wifiWaitCount) + " seconds");
      vTaskDelay(pdMS_TO_TICKS(WIFI_WAIT_DELAY));
    } else {
      esp_task_wdt_reset();
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
//=========================================

// Function to reconnect MQTT
void reconnectMQTT() {
  if (!client.connected()) {
    esp_task_wdt_reset();
    #ifdef USE_Fast_LED
      leds[0] = CRGB::Yellow;
      FastLED.show();
    #endif

    char clientId[24];
    snprintf(clientId, sizeof(clientId), "dma_ssw_%04X%04X%04X", random(0xffff), random(0xffff), random(0xffff));

    if (mqttAttemptCount > 0) {
      DEBUG_PRINTLN("Attempting MQTT connection...");
      if (client.connect(clientId)) {
        DEBUG_PRINTLN("MQTT connected");

        #ifdef USE_Fast_LED
          leds[0] = CRGB::Black;
          FastLED.show();
        #endif
        char topic[48];

        snprintf(topic, sizeof(topic), "%s/%s", mqtt_sub_topic, DEVICE_ID);
        client.subscribe(topic);
      } else {
        DEBUG_PRINTLN("MQTT connection failed");
        mqttAttemptCount--;
        DEBUG_PRINTLN("Remaining MQTT attempts: " + String(mqttAttemptCount));
        vTaskDelay(pdMS_TO_TICKS(MQTT_ATTEMPT_DELAY));
      }
    } else {
      DEBUG_PRINTLN("Max MQTT attempts exceeded, restarting...");
      ESP.restart();
    }
  }
}
//===============================================

//MQTT Callback Function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  #ifdef USE_Fast_LED
    leds[0] = CRGB::Blue;
    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(250)); // Short delay to indicate status
    leds[0] = CRGB::Black;
    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(250));
  #endif

  DEBUG_PRINTLN("Message arrived on topic: " + String(topic));
  DEBUG_PRINTLN("Message content: " + message);

  // Handle Ping Command
  if (message == "ping") {
    DEBUG_PRINTLN("Request for ping");
    char pingData[100]; // Increased size for additional info
    snprintf(pingData, sizeof(pingData), "%s,%s,%s,%d,%d,%s,%s",
      DEVICE_ID, WiFi.SSID().c_str(),
      WiFi.localIP().toString().c_str(), WiFi.RSSI(), HB_INTERVAL,FIRMWARE_VERSION,FIRMWARE_RELEASE_DATE);
    client.publish(mqtt_ack_topic, pingData);

    #ifdef USE_Fast_LED
      leds[0] = CRGB::Green;
      FastLED.show();
      vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
      leds[0] = CRGB::Black;
      FastLED.show();
    #endif

    DEBUG_PRINT("Sent ping response to MQTT: ");
    DEBUG_PRINTLN(pingData);
  }
  //=================================================================//

  if(message == "get_hb") {
    DEBUG_PRINTLN("Request for heartbeat");
    publishHeartbeat(); // Call the function to publish heartbeat
  }

  // Handle Restart Command
  if(message == "restart") {
    DEBUG_PRINTLN("Restart command received via MQTT.");
    char message[64];  
    snprintf(message, sizeof(message), "%s,Device Restarting", DEVICE_ID);  
    client.publish(mqtt_ack_topic, message);

    #ifdef USE_Fast_LED
      leds[0] = CRGB::Red;
      FastLED.show();
      vTaskDelay(pdMS_TO_TICKS(250)); // Short delay to indicate status
      leds[0] = CRGB::Black;
      vTaskDelay(pdMS_TO_TICKS(250));
      leds[0] = CRGB::Red;
      vTaskDelay(pdMS_TO_TICKS(250)); // Short delay to indicate status
      leds[0] = CRGB::Black;
      FastLED.show();
    #endif

    DEBUG_PRINTLN("Restarting now...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP.restart();
  }
  //=================================//

  // Handle OTA Update Command
  if (message == "update_firmware") {
    if (otaTaskHandle == NULL) {
      xTaskCreatePinnedToCore(otaTask, "OTA Task", 8*1024, NULL, 1, &otaTaskHandle, 1);
    } else {
      Serial.println("OTA Task already running.");
    }
  }
  //=================================//
}
//===================================

// Start Network Task
void networkTask(void *param) {
  DEBUG_PRINTLN("Network Task started...!");
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  for (;;) {
    esp_task_wdt_reset();
    // Check WiFi connection
    if (WiFi.status() == WL_CONNECTED) {
      // Check and reconnect MQTT if necessary
      if (!client.connected()) {
        reconnectMQTT();
      }
    } else {
      // Reconnect WiFi if disconnected
      reconnectWiFi();
    }

    // Loop MQTT client for processing incoming messages
    client.loop();

    // Delay for 100ms before next cycle
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
//================================

//Start WiFi reset task
void wifiResetTask(void *param) {
  DEBUG_PRINTLN("WiFi Reset Task started, resetting WiFi settings...");

  for (;;) {
    esp_task_wdt_reset();

    leds[0] = CRGB::Green;
    FastLED.show();

    // Suspend other tasks while configuring WiFi
    vTaskSuspend(networkTaskHandle);
    vTaskSuspend(mainTaskHandle);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Reset WiFi settings
    wm.resetSettings();

    // Set timeout for config portal (e.g., 3 minutes)
    wm.setConfigPortalTimeout(180);  // timeout in seconds

    // Start autoConnect with timeout
    if (!wm.autoConnect("Weight_Scale_Config")) {
      DEBUG_PRINTLN("WiFi config portal timed out!");
      // Handle fallback, e.g., restart or continue offline
      ESP.restart();
    }

    // If connected successfully
    DEBUG_PRINTLN("WiFi connected!");
    // If WiFi is configured successfully
    DEBUG_PRINTLN("Restarting to apply settings...");
    delay(2000);
    ESP.restart();  // Restart ESP to use new WiFi credentials

    wifiResetTaskHandle = NULL;
    vTaskDelete(NULL); // Delete this task
  }
}
//=================================

// Start OTA Task
void otaTask(void *parameter) {
  esp_task_wdt_reset();
  Serial.println("OTA Task started...");
  Serial.println("Starting OTA update...");

  #ifdef USE_Fast_LED
    leds[0] = CRGB::Green;
    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(250)); // Short delay to indicate status
    leds[0] = CRGB::Black;
    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(250)); // Short delay to indicate status
    leds[0] = CRGB::Green;
    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(250)); // Short delay to indicate status
    leds[0] = CRGB::Black;
    FastLED.show();
  #endif

  HTTPClient http;
  http.begin(ota_url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("Content-Length: %d bytes\n", contentLength);
    
    if (Update.begin(contentLength)) {
      Update.writeStream(http.getStream());
      if (Update.end() && Update.isFinished()) {
        Serial.println("OTA update completed. Restarting...");
        char message[64];  
        snprintf(message, sizeof(message), "%s,OTA update successful", DEVICE_ID);  
        client.publish(mqtt_ota_topic, message);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        http.end();
        ESP.restart();
      } else {
        Serial.println("OTA update failed!");
        char message[64];  
        snprintf(message, sizeof(message), "%s,OTA Update Failed!", DEVICE_ID);  
        client.publish(mqtt_ota_topic, message);
      }
    } else {
      Serial.println("OTA begin failed!");
      char message[64];  
      snprintf(message, sizeof(message), "%s,OTA Begin Failed!", DEVICE_ID);  
      client.publish(mqtt_ota_topic, message);
    }
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    char message[64];  
    snprintf(message, sizeof(message), "%s,HTTP Request Failed", DEVICE_ID);  
    client.publish(mqtt_ota_topic, message);
  }

  http.end();
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  
  ESP.restart();

  otaTaskHandle = NULL;  
  vTaskDelete(NULL);
}
//=================================

// Start Main Task
void mainTask(void *param) {
  DEBUG_PRINTLN("Main Task started...!");
  unsigned long lastReceivedTime = 0;  
  unsigned long lastReceivedCode = 0;

  for (;;) {
    esp_task_wdt_reset();
      
    static unsigned long last_hb_send_time = 0;
    unsigned long now = millis();
    
    // **Send Heartbeat Every HB_INTERVAL**
    if (now - last_hb_send_time >= HB_INTERVAL) {
      last_hb_send_time = now;
      //---------------------------------------------//
      publishHeartbeat();
      //---------------------------------------------//
    }
    if (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
      //---------------------------------------------//
      WifiResetHandle();
      //---------------------------------------------//
    }
    //===============================================//
    
    //----------------------------------------------------------//
    vTaskDelay(pdMS_TO_TICKS(100)); // Keep FreeRTOS responsive
  }
}

//==================================================================//

//-----------------------------------------------------------
// Scale Task (Sensor + Filtering)
//-----------------------------------------------------------
void scaleTask(void *param)
{
  Serial.println("Scale Task Started");

  scale.power_down();
  vTaskDelay(pdMS_TO_TICKS(500));
  scale.power_up();

  if (currentMode == MODE_CALIBRATION)
  {
    Serial.println("Calibration Mode");
    Serial.println("Clear the scale. Taring in 5 seconds...");

    vTaskDelay(pdMS_TO_TICKS(5000));

    scale.set_scale();
    scale.tare();

    Serial.println("Tare complete. Place known weight.");
  }
  else
  {
    Serial.println("Normal Mode");

    scale.set_scale(55.0325f);   // Your calibration factor
    scale.tare();
  }

  for (;;)
  {
    if (scale.is_ready())
    {
      float weight = readFilteredWeight();
      long raw = scale.get_value(1);

      // Zero dead-zone
      if (abs(weight) < ZERO_DEADZONE)
        weight = 0;

      xSemaphoreTake(scaleMutex, portMAX_DELAY);

      scaleData.weight = weight;
      scaleData.raw = raw;

      xSemaphoreGive(scaleMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

//-----------------------------------------------------------
// Serial Task
//-----------------------------------------------------------
void serialTask(void *param)
{
  Serial.println("Serial Task Started");

  float lastWeight = 0;
  float printedWeight = -9999;

  uint32_t stableStart = 0;
  bool stable = false;

  const float CHANGE_THRESHOLD = 5.0;   // grams
  const uint32_t STABLE_TIME = 1000;    // ms

  for (;;)
  {
    float weight;

    xSemaphoreTake(scaleMutex, portMAX_DELAY);
    weight = scaleData.weight;
    xSemaphoreGive(scaleMutex);

    if (currentMode == MODE_CALIBRATION)
    {
      Serial.print("Raw Value: ");
      Serial.println(scaleData.raw);
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    // Detect weight change
    if (abs(weight - lastWeight) > CHANGE_THRESHOLD)
    {
      stableStart = millis();
      stable = false;
    }

    // Check if stable
    if (!stable && (millis() - stableStart > STABLE_TIME))
    {
      stable = true;
    }

    // Print only once when stable
    if (stable && abs(weight - printedWeight) > CHANGE_THRESHOLD)
    {
      Serial.print("Weight: ");
      Serial.print(weight, 2);
      Serial.println(" g");

      printedWeight = weight;
    }

    lastWeight = weight;

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);

  preferences.begin("device_data", false);  // Open Preferences (NVS)
  static String device_id; // Static variable to persist scope
  
  #if CHANGE_DEICE_ID
    // Construct new device ID
    device_id = String(WORK_PACKAGE) + GW_TYPE + FIRMWARE_UPDATE_DATE + DEVICE_SERIAL;
    
    // Save device ID to Preferences
    preferences.putString("device_id", device_id);
    Serial.println("Device ID updated in Preferences: " + device_id);
  #else
    // Restore device ID from Preferences
    device_id = preferences.getString("device_id", "UNKNOWN");
    Serial.println("Restored Device ID from Preferences: " + device_id);
  #endif

  DEVICE_ID = device_id.c_str(); // Assign to global pointer

  preferences.end();
  
  DEBUG_PRINT("Device ID: ");
  DEBUG_PRINTLN(DEVICE_ID);
  
  #ifdef USE_Fast_LED
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    leds[0] = CRGB::HotPink;
    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(1000));

    leds[0] = CRGB::Black;
    FastLED.show();
  #endif
  //===============================================//

  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  if (digitalRead(MODE_BUTTON_PIN) == LOW)
  {
    currentMode = MODE_CALIBRATION;
    Serial.println("Calibration Mode Activated");
  }
  else
  {
    currentMode = MODE_NORMAL;
    Serial.println("Normal Mode Activated");
  }

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  scaleMutex = xSemaphoreCreateMutex();

    
  //===============================================//

  pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
  client.setKeepAlive(60);
  Serial.println("✅ MQTT Client Initialized!");

  esp_task_wdt_init(60, true);   // 🛡️ 60s timeout for all registered tasks 
  Serial.println("✅ WDT Initialized!");

  // Start appropriate tasks based on mode
  if(wifiMode){
    xTaskCreatePinnedToCore(networkTask, "Network Task", 8*1024, NULL, 1, &networkTaskHandle, 0);
    Serial.println("✅ Network Task Created!");

    xTaskCreatePinnedToCore(mainTask, "Main Task", 16*1024, NULL, 1, &mainTaskHandle, 1);
    Serial.println("✅ Main Task Created!");
  }
  else{
    // xTaskCreatePinnedToCore(serialTask, "Serial Task", 16*1024, NULL, 1, &serialTaskHandle, 1);
    // Serial.println("✅ Serial Task Created!");
    xTaskCreatePinnedToCore(scaleTask, "ScaleTask", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(serialTask, "SerialTask", 4096, NULL, 1, NULL, 1);
  }
  // xTaskCreatePinnedToCore(wifiResetTask, "WiFi Reset Task", 8*1024, NULL, 1, &wifiResetTaskHandle, 1);
}

void loop(){

}