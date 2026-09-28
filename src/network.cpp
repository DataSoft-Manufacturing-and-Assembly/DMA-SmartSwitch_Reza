#include "network.h"
#include "led.h"
#include "switches.h"
#include "messages.h"
#include <esp_wifi.h>

static volatile bool s_apRequested = false;

// ============================================================
//  Tiny JSON helpers — no library needed
// ============================================================
static long jsonGetInt(const char* json, const char* key, long fallback = LONG_MIN) {
    char needle[24];
    int n = snprintf(needle, sizeof(needle), "\"%s\":", key);
    if (n <= 0 || (size_t)n >= sizeof(needle)) return fallback;
    const char* p = strstr(json, needle);
    if (!p) return fallback;
    p += strlen(needle);
    while (*p == ' ') p++;
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    return (end == p) ? fallback : v;
}

static bool jsonGetStr(const char* json, const char* key, char* out, size_t outsz) {
    char needle[24];
    int n = snprintf(needle, sizeof(needle), "\"%s\":", key);
    if (n <= 0 || (size_t)n >= sizeof(needle)) return false;
    const char* p = strstr(json, needle);
    if (!p) return false;
    p += strlen(needle);
    while (*p == ' ') p++;
    if (*p != '"') return false;
    p++;
    const char* e = strchr(p, '"');
    if (!e) return false;
    size_t len = e - p;
    if (len >= outsz) len = outsz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

// ============================================================
//  Helpers
// ============================================================
static bool hasWifiCreds() {
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return false;
    return strlen((char*)conf.sta.ssid) > 0;
}

static bool mqttConnectOnce() {
    char cid[32];
    snprintf(cid, sizeof(cid), "dma_ssw_%04X%04X%04X",
             (unsigned)random(0xffff), (unsigned)random(0xffff),
             (unsigned)random(0xffff));
    if (!mqtt.connect(cid, MQTT_USER, MQTT_PASS)) {
        LOGF("MQTT", "connect failed rc=%d", mqtt.state());
        return false;
    }
    char sub[80];
    snprintf(sub, sizeof(sub), "%s/%s", TOPIC_SUB, DEVICE_ID);
    bool ok = mqtt.subscribe(sub);
    LOGF("MQTT", "connected, subscribed to [%s] %s", sub, ok ? "OK" : "FAIL");
    return true;
}

void requestApMode() { s_apRequested = true; }

// ============================================================
//  Outgoing queue
// ============================================================
bool queueMqtt(const char* topic, const char* payload) {
    if (!mqttOutQueue) return false;

    MqttOutMsg m;
    strncpy(m.topic, topic, sizeof(m.topic) - 1);
    m.topic[sizeof(m.topic) - 1] = '\0';

    size_t len = strlen(payload);
    if (len >= sizeof(m.payload)) {
        LOG("MQTT", "!! payload too big");
        return false;
    }
    memcpy(m.payload, payload, len + 1);

    if (xQueueSend(mqttOutQueue, &m, pdMS_TO_TICKS(50)) != pdTRUE) {
        LOG("MQTT", "!! queue full");
        return false;
    }
    return true;
}

// ============================================================
//  Publish (green flash on success, except state ACKs)
// ============================================================
static void publishOne(const MqttOutMsg& m) {
    bool ok = mqtt.publish(m.topic, m.payload);
    if (ok && strcmp(m.topic, TOPIC_PUB) != 0) ledFlash(L_GREEN, 150);
    LOGF("MQTT", "PUB [%s] -> %s", m.topic, ok ? "ok" : "FAIL");
}

// ============================================================
//  Heartbeat
// ============================================================
void publishHeartbeat() {
    if (!mqtt.connected()) { LOG("HB", "skipped (mqtt down)"); return; }
    char buf[MQTT_PAYLOAD_MAX];
    if (msgHeartbeat(buf, sizeof(buf))) queueMqtt(TOPIC_PUB, buf);
}

// ============================================================
//  Commands
// ============================================================
static void startOta() {
    if (taskOta == nullptr)
        xTaskCreatePinnedToCore(otaTask, "Ota", 8 * 1024, NULL, 1, &taskOta, 1);
}

static void handleJson(const char* json) {
    char cmd[16];
    if (jsonGetStr(json, "cmd", cmd, sizeof(cmd))) {
        if (strcmp(cmd, "ping") == 0) {
            char buf[MQTT_PAYLOAD_MAX];
            if (msgPing(buf, sizeof(buf))) queueMqtt(TOPIC_PUB, buf);
            return;
        }
        if (strcmp(cmd, "ota") == 0) { startOta(); return; }
        if (strcmp(cmd, "restart") == 0) {
            char buf[MQTT_PAYLOAD_MAX];
            if (msgAck(buf, sizeof(buf), "mqtt")) queueMqtt(TOPIC_PUB, buf);
            vTaskDelay(pdMS_TO_TICKS(1500));
            ESP.restart();
        }
    }

    bool changed = false;

    long v = jsonGetInt(json, "all");
    if (v != LONG_MIN) {
        bool on = v != 0;
        for (uint8_t i = 0; i < 3; i++)
            if (getSwitchState(i) != on) { setSwitchState(i, on, false, "mqtt"); changed = true; }
    }

    const char* keys[3] = { "light1", "light2", "fan" };
    for (uint8_t i = 0; i < 3; i++) {
        v = jsonGetInt(json, keys[i]);
        if (v != LONG_MIN) {
            bool on = v != 0;
            if (getSwitchState(i) != on) { setSwitchState(i, on, false, "mqtt"); changed = true; }
        }
    }

    v = jsonGetInt(json, "fan_speed");
    if (v != LONG_MIN) {
        uint8_t sp = (uint8_t)v;
        if (sp != getFanSpeed()) { setFanSpeed(sp, false, "mqtt"); changed = true; }
    }

    if (changed) {
        char buf[MQTT_PAYLOAD_MAX];
        if (msgAck(buf, sizeof(buf), "mqtt")) queueMqtt(TOPIC_PUB, buf);
    }
}

static void handleString(const char* msg) {
    if (strcmp(msg, "ping") == 0) {
        char buf[MQTT_PAYLOAD_MAX];
        if (msgPing(buf, sizeof(buf))) queueMqtt(TOPIC_PUB, buf);
        return;
    }
    if (strcmp(msg, "ota") == 0) { startOta(); return; }
    if (strcmp(msg, "restart") == 0) {
        char buf[MQTT_PAYLOAD_MAX];
        if (msgAck(buf, sizeof(buf), "mqtt")) queueMqtt(TOPIC_PUB, buf);
        vTaskDelay(pdMS_TO_TICKS(1500));
        ESP.restart();
    }
    if (strcmp(msg, "light1:1") == 0) { setSwitchState(0, true,  true,  "mqtt"); return; }
    if (strcmp(msg, "light1:0") == 0) { setSwitchState(0, false, true,  "mqtt"); return; }
    if (strcmp(msg, "light2:1") == 0) { setSwitchState(1, true,  true,  "mqtt"); return; }
    if (strcmp(msg, "light2:0") == 0) { setSwitchState(1, false, true,  "mqtt"); return; }
    if (strcmp(msg, "fan:1")    == 0) { setSwitchState(2, true,  true,  "mqtt"); return; }
    if (strcmp(msg, "fan:0")    == 0) { setSwitchState(2, false, true,  "mqtt"); return; }
    if (strcmp(msg, "all:1")    == 0) { setAllSwitches(true,  true, "mqtt"); return; }
    if (strcmp(msg, "all:0")    == 0) { setAllSwitches(false, true, "mqtt"); return; }
    if (strncmp(msg, "fan_speed:", 10) == 0) {
        setFanSpeed((uint8_t)atoi(msg + 10), true, "mqtt");
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    static char buf[MQTT_PAYLOAD_MAX];
    if (length >= sizeof(buf)) length = sizeof(buf) - 1;
    memcpy(buf, payload, length);
    buf[length] = '\0';
    LOGF("MQTT", "recv [%s] %s", topic, buf);

    if (buf[0] == '{') handleJson(buf);
    else               handleString(buf);
}

// ============================================================
//  Network task — simple try/rest loop
// ============================================================
void networkTask(void* param) {
    LOG("NET", "task started");

    WiFi.mode(WIFI_STA);
    delay(200);

    if (!hasWifiCreds()) {
        LOG("NET", "no WiFi creds — AP mode");
        xTaskCreatePinnedToCore(wifiResetTask, "Ap", 8 * 1024, NULL, 1, &taskWifiReset, 1);
        vTaskDelete(nullptr);
    }

    bool          wifiTrying = false;
    unsigned long wifiStart  = 0;
    unsigned long wifiWait   = 0;

    bool          mqttTrying = false;
    unsigned long mqttStart  = 0;
    unsigned long mqttWait   = 0;

    for (;;) {
        esp_task_wdt_reset();
        unsigned long now = millis();

        if (s_apRequested) {
            s_apRequested = false;
            LOG("NET", "AP requested");
            xTaskCreatePinnedToCore(wifiResetTask, "Ap", 8 * 1024, NULL, 1, &taskWifiReset, 1);
            vTaskDelete(nullptr);
        }

        // ---------- WiFi ----------
        if (WiFi.status() != WL_CONNECTED) {
            mqttTrying = false;
            mqttWait   = 0;

            if (wifiTrying) {
                if (now - wifiStart >= WIFI_TRY_MS) {
                    wifiTrying = false;
                    wifiWait   = now + WIFI_REST_MS;
                    LOG("WIFI", "try done — resting");
                } else {
                    ledWifiConnecting();
                }
            } else {
                if (now >= wifiWait) {
                    LOG("WIFI", "attempting");
                    WiFi.begin();
                    wifiTrying = true;
                    wifiStart  = now;
                    ledWifiConnecting();
                } else {
                    ledWifiResting();
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        wifiTrying = false;
        wifiWait   = 0;

        // ---------- MQTT ----------
        if (!mqtt.connected()) {
            if (mqttTrying) {
                if (now - mqttStart >= MQTT_TRY_MS) {
                    mqttTrying = false;
                    mqttWait   = now + MQTT_REST_MS;
                    LOG("MQTT", "try done — resting");
                } else {
                    ledMqttConnecting();
                }
            } else {
                if (now >= mqttWait) {
                    mqttTrying = true;
                    mqttStart  = now;
                    ledMqttConnecting();
                    if (mqttConnectOnce()) ledFlash(L_GREEN, 300);
                } else {
                    ledMqttResting();
                }
            }
        } else {
            mqttTrying = false;
            mqttWait   = 0;
            ledOnline();

            MqttOutMsg out;
            while (xQueueReceive(mqttOutQueue, &out, 0) == pdTRUE) {
                publishOne(out);
            }
        }

        mqtt.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================
//  AP mode
// ============================================================
void wifiResetTask(void* param) {
    LOG("AP", "entering AP mode");
    ledApMode();

    if (taskSwitches) vTaskSuspend(taskSwitches);
    if (taskMain)     vTaskSuspend(taskMain);

    wifiManager.resetSettings();
    wifiManager.setConfigPortalTimeout(180);
    if (!wifiManager.autoConnect("DMA_SmartSwitch_Config")) {
        LOG("AP", "portal timeout — reboot");
        ESP.restart();
    }
    LOG("AP", "creds saved — reboot");
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP.restart();
}

// ============================================================
//  OTA
// ============================================================
void otaTask(void* param) {
    LOG("OTA", "starting");
    HTTPClient http;
    http.begin(OTA_URL);
    int code = http.GET();
    char buf[MQTT_PAYLOAD_MAX];

    if (code == HTTP_CODE_OK) {
        int len = http.getSize();
        if (Update.begin(len)) {
            Update.writeStream(http.getStream());
            if (Update.end() && Update.isFinished()) {
                if (msgOta(buf, sizeof(buf), "success")) queueMqtt(TOPIC_PUB, buf);
                vTaskDelay(pdMS_TO_TICKS(2000));
                http.end(); ESP.restart();
            } else {
                if (msgOta(buf, sizeof(buf), "write_failed")) queueMqtt(TOPIC_PUB, buf);
            }
        } else {
            if (msgOta(buf, sizeof(buf), "begin_failed")) queueMqtt(TOPIC_PUB, buf);
        }
    } else {
        if (msgOta(buf, sizeof(buf), "http_failed")) queueMqtt(TOPIC_PUB, buf);
    }
    http.end();
    vTaskDelay(pdMS_TO_TICKS(2000));
    taskOta = nullptr;
    vTaskDelete(nullptr);
}