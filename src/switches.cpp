#include "switches.h"
#include "config.h"
#include "led.h"
#include "network.h"
#include "messages.h"

#define NUM_SWITCHES 3
#define FAN_MAX 5

static const uint8_t RELAY[NUM_SWITCHES] = { RELAY_LIGHT1_PIN, RELAY_LIGHT2_PIN, RELAY_FAN_PIN };
static const uint8_t TOUCH[NUM_SWITCHES] = { TOUCH_LIGHT1_PIN, TOUCH_LIGHT2_PIN, TOUCH_FAN_PIN };
static const char*   NAME [NUM_SWITCHES] = { "light1", "light2", "fan" };

static bool          s_state[NUM_SWITCHES]     = { false, false, false };
static bool          s_lastTouch[NUM_SWITCHES] = { HIGH,  HIGH,  HIGH  };
static unsigned long s_lastTouchMs[NUM_SWITCHES] = { 0, 0, 0 };
static uint8_t       s_fanSpeed = 0;

// ---- publish ACK ----
static void publishAck(const char* source) {
    char buf[MQTT_PAYLOAD_MAX];
    if (msgAck(buf, sizeof(buf), source)) queueMqtt(TOPIC_PUB, buf);
}

// ---- NVS ----
static void saveSwitch(uint8_t i, bool on) {
    prefs.begin("switches", false);
    char key[8]; snprintf(key, sizeof(key), "sw%u", (unsigned)(i + 1));
    prefs.putBool(key, on);
    prefs.end();
}
static void saveFan() {
    prefs.begin("switches", false);
    prefs.putUChar("fan_speed", s_fanSpeed);
    prefs.end();
}

// ---- getters ----
bool    getSwitchState(uint8_t i) { return (i < NUM_SWITCHES) ? s_state[i] : false; }
uint8_t getFanSpeed()             { return s_fanSpeed; }

// ---- setters ----
void setSwitchState(uint8_t i, bool on, bool publish, const char* source) {
    if (i >= NUM_SWITCHES) return;
    if (s_state[i] == on && publish) return;
    s_state[i] = on;
    digitalWrite(RELAY[i], on ? HIGH : LOW);
    saveSwitch(i, on);
    LOGF("SW", "%s -> %s (from %s)", NAME[i], on ? "ON" : "OFF", source);
    ledFlash(on ? L_GREEN : L_PINK, 250);
    if (publish) publishAck(source);
}

void setAllSwitches(bool on, bool publish, const char* source) {
    for (uint8_t i = 0; i < NUM_SWITCHES; i++) {
        s_state[i] = on;
        digitalWrite(RELAY[i], on ? HIGH : LOW);
        saveSwitch(i, on);
    }
    LOGF("SW", "ALL -> %s (from %s)", on ? "ON" : "OFF", source);
    ledFlash(on ? L_GREEN : L_PINK, 300);
    if (publish) publishAck(source);
}

void setFanSpeed(uint8_t speed, bool publish, const char* source) {
    if (speed > FAN_MAX) speed = FAN_MAX;
    if (speed == s_fanSpeed) return;
    s_fanSpeed = speed;
    saveFan();
    LOGF("FAN", "speed -> %u (from %s)", s_fanSpeed, source);
    ledFlash(L_CYAN, 200);
    if (publish) publishAck(source);
}

// ---- setup ----
void switchesSetup() {
    for (uint8_t i = 0; i < NUM_SWITCHES; i++) {
        pinMode(RELAY[i], OUTPUT);
        digitalWrite(RELAY[i], LOW);
    }
    for (uint8_t i = 0; i < NUM_SWITCHES; i++) pinMode(TOUCH[i], INPUT_PULLUP);
    pinMode(FAN_UP_PIN,   INPUT_PULLUP);
    pinMode(FAN_DOWN_PIN, INPUT_PULLUP);

    prefs.begin("switches", false);
    for (uint8_t i = 0; i < NUM_SWITCHES; i++) {
        char key[8]; snprintf(key, sizeof(key), "sw%u", (unsigned)(i + 1));
        s_state[i] = prefs.getBool(key, false);
        digitalWrite(RELAY[i], s_state[i] ? HIGH : LOW);
    }
    s_fanSpeed = prefs.getUChar("fan_speed", 0);
    prefs.end();

    LOGF("SW", "ready: l1=%d l2=%d fan=%d speed=%u",
         s_state[0], s_state[1], s_state[2], s_fanSpeed);
}

// ---- task ----
void switchesTask(void* param) {
    bool lastUp = HIGH, lastDown = HIGH;
    unsigned long lastUpMs = 0, lastDownMs = 0;

    bool          apHold   = false;
    unsigned long apStart  = 0;

    LOG("SW", "task started");

    for (;;) {
        esp_task_wdt_reset();
        unsigned long now = millis();

        // ---- AP trigger: light1 + light2 both held ----
        bool b1 = digitalRead(TOUCH_LIGHT1_PIN) == LOW;
        bool b2 = digitalRead(TOUCH_LIGHT2_PIN) == LOW;
        if (!b1 && !b2) {
            if (!apHold) { apHold = true; apStart = now; ledFlash(L_WHITE, 100); }
            else if (now - apStart >= AP_HOLD_MS) {
                LOG("AP", "dual-touch -> AP mode");
                requestApMode();
                while (digitalRead(TOUCH_LIGHT1_PIN) == LOW ||
                       digitalRead(TOUCH_LIGHT2_PIN) == LOW)
                    vTaskDelay(pdMS_TO_TICKS(50));
                apHold = false;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        apHold = false;

        // ---- normal toggles ----
        for (uint8_t i = 0; i < NUM_SWITCHES; i++) {
            bool r = digitalRead(TOUCH[i]);
            if (r != s_lastTouch[i]) {
                s_lastTouch[i] = r;
                if (r == LOW && (now - s_lastTouchMs[i]) > DEBOUNCE_MS) {
                    s_lastTouchMs[i] = now;
                    setSwitchState(i, !s_state[i], true, "touch");
                }
            }
        }

        // ---- fan speed up ----
        bool up = digitalRead(FAN_UP_PIN);
        if (up == LOW && lastUp == HIGH && (now - lastUpMs) > DEBOUNCE_MS) {
            lastUpMs = now;
            setFanSpeed(s_fanSpeed + 1, true, "touch");
        }
        lastUp = up;

        // ---- fan speed down ----
        bool dn = digitalRead(FAN_DOWN_PIN);
        if (dn == LOW && lastDown == HIGH && (now - lastDownMs) > DEBOUNCE_MS) {
            lastDownMs = now;
            if (s_fanSpeed > 0) setFanSpeed(s_fanSpeed - 1, true, "touch");
        }
        lastDown = dn;

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}