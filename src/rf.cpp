#include "rf.h"
#include "config.h"

#ifdef USE_RF_RECEIVER
#include <RCSwitch.h>
#include "led.h"
#include "network.h"
#include "messages.h"

static RCSwitch    s_rf;
static RfCallback  s_cb = nullptr;

// simple 8-slot per-code debounce — no dynamic allocation
struct RfSlot { unsigned long code; unsigned long lastMs; };
static RfSlot        s_deb[8];
static uint8_t       s_debCount = 0;
static unsigned long s_lastGlobal = 0;

void rfSetCallback(RfCallback fn) { s_cb = fn; }

void rfSetup() {
    LOGF("RF", "setup on GPIO %d", RF_PIN);
    s_rf.enableReceive(digitalPinToInterrupt(RF_PIN));
}

void rfTask(void* param) {
    LOG("RF", "task started");
    for (;;) {
        esp_task_wdt_reset();
        if (s_rf.available()) {
            unsigned long code = s_rf.getReceivedValue();
            uint8_t       bits = s_rf.getReceivedBitlength();
            unsigned long now  = millis();

            if (code != 0 && bits >= 24 && (now - s_lastGlobal) > 100) {
                bool allow = true;
                int  slot  = -1;
                for (uint8_t i = 0; i < s_debCount; i++) {
                    if (s_deb[i].code == code) {
                        slot = i;
                        if (now - s_deb[i].lastMs < 2000) allow = false;
                        break;
                    }
                }
                if (allow) {
                    if (slot < 0) {
                        if (s_debCount < 8) slot = s_debCount++;
                        else {
                            slot = 0;
                            for (uint8_t i = 1; i < 8; i++)
                                if (s_deb[i].lastMs < s_deb[slot].lastMs) slot = i;
                        }
                    }
                    s_deb[slot].code   = code;
                    s_deb[slot].lastMs = now;
                    s_lastGlobal       = now;

                    LOGF("RF", "code=%lu bits=%u", code, bits);

                    char buf[MQTT_PAYLOAD_MAX];
                    if (msgRf(buf, sizeof(buf), code, bits)) queueMqtt(TOPIC_PUB, buf);
                    if (s_cb) s_cb(code, bits);
                    ledFlash(L_BLUE, 200);
                }
            }
            s_rf.resetAvailable();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

#else
void rfSetup()                    {}
void rfTask(void*)                {}
void rfSetCallback(RfCallback)    {}
#endif