#include "config.h"
#include "led.h"
#include "switches.h"
#include "network.h"
#include "messages.h"
#include "rf.h"

// ---- RF callback: fill in once you know your remote codes ----
static void onRf(unsigned long code, uint8_t bits) {
    LOGF("RF", "unmapped code=%lu bits=%u", code, bits);
    // switch (code) {
    //     case 0x00A1B2C3: setSwitchState(0, !getSwitchState(0), true, "rf"); break;
    // }
    (void)code; (void)bits;
}

// ---- heartbeat task ----
static void mainTask(void* param) {
    LOG("MAIN", "task started");
    unsigned long lastHb = 0;
    for (;;) {
        esp_task_wdt_reset();
        unsigned long now = millis();
        if (now - lastHb >= HB_INTERVAL_MS) { lastHb = now; publishHeartbeat(); }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ---- setup ----
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("========================================");
    Serial.printf ("  DMA SmartSwitch  %s  (%s)\n",
                   FIRMWARE_VERSION, FIRMWARE_RELEASE_DATE);
    Serial.println("========================================");

    // ---- Device ID ----
    prefs.begin("device_data", false);
#if CHANGE_DEVICE_ID
    {
        char id[32];
        snprintf(id, sizeof(id), "%s%s%s%s",
                 WORK_PACKAGE, GW_TYPE, FIRMWARE_UPDATE_DATE, DEVICE_SERIAL);
        prefs.putString("device_id", id);
        LOGF("SETUP", "device_id written: %s", id);
    }
#endif
    {
        String s = prefs.getString("device_id", "UNKNOWN");
        strncpy(g_deviceId, s.c_str(), sizeof(g_deviceId) - 1);
        g_deviceId[sizeof(g_deviceId) - 1] = '\0';
    }
    prefs.end();
    LOGF("SETUP", "device_id = %s", DEVICE_ID);

    // ---- MQTT out queue ----
    mqttOutQueue = xQueueCreate(MQTT_QUEUE_SIZE, sizeof(MqttOutMsg));
    LOGF("SETUP", "MQTT queue: %s", mqttOutQueue ? "OK" : "FAIL");

    // ---- LED ----
    ledSetup();
    ledFlash(L_RED, 100);
    vTaskDelay(pdMS_TO_TICKS(120));
    ledFlash(L_GREEN, 100);
    vTaskDelay(pdMS_TO_TICKS(120));
    ledFlash(L_BLUE, 100);

    // ---- Switches ----
    switchesSetup();

    // ---- MQTT ----
    mqtt.setServer(MQTT_SERVER, 1883);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(MQTT_RX_BUFFER);

    // ---- Watchdog ----
    esp_task_wdt_init(60, true);
    LOG("SETUP", "watchdog enabled");

    // ---- RF ----
    rfSetup();
    rfSetCallback(onRf);

    // ---- Tasks ----
    xTaskCreatePinnedToCore(networkTask,  "Net",  8 * 1024, NULL, 1, &taskNetwork,  0);
    xTaskCreatePinnedToCore(switchesTask, "Sw",   4 * 1024, NULL, 1, &taskSwitches, 1);
    xTaskCreatePinnedToCore(mainTask,     "Main", 4 * 1024, NULL, 1, &taskMain,     1);
#ifdef USE_RF_RECEIVER
    xTaskCreatePinnedToCore(rfTask,       "RF",   4 * 1024, NULL, 1, &taskRf,       1);
#endif

    LOG("SETUP", "boot complete");
    Serial.println("========================================");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}