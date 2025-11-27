#include <config.h>

//Function Prototypes
void otaTask(void *param);
void wifiResetTask(void *param);
void networkTask(void *param);
void mainTask(void *param);
void reconnectWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishHeartbeat();
void WifiResetHandle();
void RFReceiverHandle();
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

//RF Receiver Handler
#ifdef USE_RF_RECEIVER
  void RFReceiverHandle() {
    unsigned long receivedCode = mySwitch.getReceivedValue();
    int bitLength = mySwitch.getReceivedBitlength(); // Get bit length of the received signal

    // **Ignore signals that do not match the expected bit length (e.g., < 24 bits)**
    if (bitLength < 24) {  
      DEBUG_PRINTLN(String("Ignored RF Signal: ") + String(receivedCode) + " (Bits: " + String(bitLength) + ")");
      mySwitch.resetAvailable();
      continue;;
    }

    // **Short-Term Global Debounce (Ignore if received within 100ms)**
    if (now - lastRFGlobalReceivedTime < 100) {
      mySwitch.resetAvailable();
      continue;
    }

    // **Per-Sensor Debounce (Ignore same sensor within 2 sec)**
    if (lastRFReceivedTimeMap.find(receivedCode) == lastRFReceivedTimeMap.end() || 
        (now - lastRFReceivedTimeMap[receivedCode] > 2000)) {  

      lastRFReceivedTimeMap[receivedCode] = now;  // Update per-sensor time
      lastRFGlobalReceivedTime = now;  // Update global debounce

      // **Debug Output**
      DEBUG_PRINTLN(String("Valid RF Received: ") + String(receivedCode) + " (Bits: " + String(bitLength) + ")");
      
      // **Send Data to MQTT**
      char data[50];
      snprintf(data, sizeof(data), "%s,%lu", DEVICE_ID, receivedCode);
      client.publish(mqtt_pub_topic, data);
      DEBUG_PRINTLN(String("Data Sent to MQTT: ") + String(data));
    }

    mySwitch.resetAvailable();
  }
#endif
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
      if (client.connect(clientId, mqtt_user, mqtt_password)) {
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

    #ifdef USE_Fast_LED
      leds[0] = CRGB::Green;
      FastLED.show();
      vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
      leds[0] = CRGB::Black;
      FastLED.show();
    #endif
    DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data))
  } 
  else if (message == "sw1:0") {
      DEBUG_PRINTLN("Switch-1: Off");
      digitalWrite(SW_PIN1, LOW);
      preferences.putBool("sw1", false); 
      char data[32];
      snprintf(data, sizeof(data), "%s,sw1:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::DeepPink;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }

  if (message == "sw2:1") {
      DEBUG_PRINTLN("Switch-2: On");
      digitalWrite(SW_PIN2, HIGH);
      preferences.putBool("sw2", true);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw2:1", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::Green;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  } 
  else if (message == "sw2:0") {
      DEBUG_PRINTLN("Switch-2: Off");
      digitalWrite(SW_PIN2, LOW);
      preferences.putBool("sw2", false);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw2:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::DeepPink;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }

  if (message == "sw3:1") {
      DEBUG_PRINTLN("Switch-3: On");
      digitalWrite(SW_PIN3, HIGH);
      preferences.putBool("sw3", true);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw3:1", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::Green;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  } 
  else if (message == "sw3:0") {
      DEBUG_PRINTLN("Switch-3: Off");
      digitalWrite(SW_PIN3, LOW);
      preferences.putBool("sw3", false);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw3:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::DeepPink;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }

  if (message == "sw4:1") {
      DEBUG_PRINTLN("Switch-4: On");
      digitalWrite(SW_PIN4, HIGH);
      preferences.putBool("sw4", true);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw4:1", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::Green;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  } 
  else if (message == "sw4:0") {
      DEBUG_PRINTLN("Switch-4: Off");
      digitalWrite(SW_PIN4, LOW);
      preferences.putBool("sw4", false);
      char data[32];
      snprintf(data, sizeof(data), "%s,sw4:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::DeepPink;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }

  //Handle All Switches Together
  if (message == "sw1234:1") {
      DEBUG_PRINTLN("Switch-1234: On");
      digitalWrite(SW_PIN1, HIGH);
      digitalWrite(SW_PIN2, HIGH);
      digitalWrite(SW_PIN3, HIGH);
      digitalWrite(SW_PIN4, HIGH);

      preferences.putBool("sw1", true);
      preferences.putBool("sw2", true);
      preferences.putBool("sw3", true);
      preferences.putBool("sw4", true);

      char data[32];
      snprintf(data, sizeof(data), "%s,sw1234:1", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::Green;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  } 
  else if (message == "sw1234:0") {
      DEBUG_PRINTLN("Switch-1234: Off");
      digitalWrite(SW_PIN1, LOW);
      digitalWrite(SW_PIN2, LOW);
      digitalWrite(SW_PIN3, LOW);
      digitalWrite(SW_PIN4, LOW);

      preferences.putBool("sw1", false);
      preferences.putBool("sw2", false);
      preferences.putBool("sw3", false);
      preferences.putBool("sw4", false);

      char data[32];
      snprintf(data, sizeof(data), "%s,sw1234:0", DEVICE_ID); 
      client.publish(mqtt_pub_topic, data);

      #ifdef USE_Fast_LED
        leds[0] = CRGB::DeepPink;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
        leds[0] = CRGB::Black;
        FastLED.show();
      #endif
      DEBUG_PRINTLN(String("Switch Status Sent to MQTT: ") + String(data));
  }
  preferences.end();  // Close Preferences storage
  
  // Handle Ping Command
  if (message == "ping") {
    DEBUG_PRINTLN("Request for ping");
    char pingData[100]; // Increased size for additional info
    snprintf(pingData, sizeof(pingData), "%s,%s,%s,%d,%d,%s,%s",
      DEVICE_ID, WiFi.SSID().c_str(),
      WiFi.localIP().toString().c_str(), WiFi.RSSI(), HB_INTERVAL,FIRMWARE_VERSION,FIRMWARE_UPDATE_DATE);
    client.publish(mqtt_ack_topic, pingData);

    #ifdef USE_Fast_LED
      leds[0] = CRGB::Blue;
      FastLED.show();
      vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to indicate status
      leds[0] = CRGB::Black;
      FastLED.show();
    #endif

    DEBUG_PRINT("Sent ping response to MQTT: ");
    DEBUG_PRINTLN(pingData);
  }
  //=================================================================//

  // Handle Restart Command
  if(message == "restart") {
    DEBUG_PRINTLN("Restart command received via MQTT.");
    char message[64];  
    snprintf(message, sizeof(message), "%s,Device Restarting", DEVICE_ID);  
    client.publish(mqtt_ack_topic, message);
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
    if (!wm.autoConnect("DMA_MoreChick")) {
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
    
    // **RF Signal Handling with Debounce and Bit Length Check**
    #ifdef USE_RF_RECEIVER
    if (mySwitch.available()) {
      //---------------------------------------------//
      
      RFReceiverHandle();
      
      //---------------------------------------------//
    }
    #endif
    
    //----------------------------------------------------------//
    vTaskDelay(pdMS_TO_TICKS(100)); // Keep FreeRTOS responsive
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

  pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);

  // mySwitch.enableReceive(digitalPinToInterrupt(RF_PIN));

  pinMode(SW_PIN1, OUTPUT);
  pinMode(SW_PIN2, OUTPUT);
  pinMode(SW_PIN3, OUTPUT);
  pinMode(SW_PIN4, OUTPUT);

  digitalWrite(SW_PIN1, LOW);
  digitalWrite(SW_PIN2, LOW);
  digitalWrite(SW_PIN3, LOW);
  digitalWrite(SW_PIN4, LOW);

  preferences.begin("switches", false);  // Open Preferences

  digitalWrite(SW_PIN1, preferences.getBool("sw1", false)); // Default: OFF
  digitalWrite(SW_PIN2, preferences.getBool("sw2", false));
  digitalWrite(SW_PIN3, preferences.getBool("sw3", false));
  digitalWrite(SW_PIN4, preferences.getBool("sw4", false));

  preferences.end();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
  client.setKeepAlive(60);
  Serial.println("✅ MQTT Client Initialized!");

  esp_task_wdt_init(60, true);   // 🛡️ 60s timeout for all registered tasks 
  Serial.println("✅ WDT Initialized!");

  xTaskCreatePinnedToCore(networkTask, "Network Task", 8*1024, NULL, 1, &networkTaskHandle, 0);
  xTaskCreatePinnedToCore(mainTask, "Main Task", 16*1024, NULL, 1, &mainTaskHandle, 1);
  // xTaskCreatePinnedToCore(wifiResetTask, "WiFi Reset Task", 8*1024, NULL, 1, &wifiResetTaskHandle, 1);
}

void loop(){

}