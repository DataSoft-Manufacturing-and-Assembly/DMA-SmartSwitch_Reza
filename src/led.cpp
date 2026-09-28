#include "led.h"
#include "config.h"

// ---- hardware ----
static CRGB s_leds[1];     // ← the ONLY CRGB array in the project

// ---- state ----
static volatile LedColor s_baseColor = L_OFF;
static volatile bool     s_baseBlink = false;
static volatile uint16_t s_baseOn    = 300;
static volatile uint16_t s_baseOff   = 300;

static volatile LedColor s_flashColor  = L_OFF;
static volatile uint16_t s_flashMs     = 0;
static volatile unsigned long s_flashEnd = 0;

static CRGB toCrgb(LedColor c) {
    switch (c) {
        case L_RED:    return CRGB::Red;
        case L_GREEN:  return CRGB::Green;
        case L_BLUE:   return CRGB::Blue;
        case L_YELLOW: return CRGB::Yellow;
        case L_PINK:   return CRGB::HotPink;
        case L_CYAN:   return CRGB::Cyan;
        case L_WHITE:  return CRGB::White;
        case L_OFF:
        default:       return CRGB::Black;
    }
}

// ---------------- public API ----------------
void ledBase(LedColor color, bool blink, uint16_t onMs, uint16_t offMs) {
    s_baseColor = color;
    s_baseBlink = blink;
    s_baseOn    = onMs;
    s_baseOff   = offMs;
}

void ledFlash(LedColor color, uint16_t durationMs) {
    s_flashColor = color;
    s_flashMs    = durationMs;
    s_flashEnd   = millis() + durationMs;
}

void ledWifiConnecting() { ledBase(L_RED,    true,  300, 300); }
void ledWifiResting()    { ledBase(L_RED,    false); }
void ledMqttConnecting() { ledBase(L_YELLOW, true,  300, 300); }
void ledMqttResting()    { ledBase(L_YELLOW, false); }
void ledOnline()         { ledBase(L_OFF,    false); }
void ledApMode()         { ledBase(L_GREEN,  true,  200, 200); }

// ---------------- task ----------------
static void ledTask(void* param) {
    LOG("LED", "task started");

    unsigned long lastToggle = 0;
    bool blinkOn = false;

    for (;;) {
        unsigned long now = millis();

        // 1. Flash first
        if (s_flashMs > 0 && (long)(now - s_flashEnd) < 0) {
            s_leds[0] = toCrgb(s_flashColor);
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        s_flashMs = 0;   // flash finished

        // 2. Base pattern
        if (!s_baseBlink) {
            s_leds[0] = toCrgb(s_baseColor);
        } else {
            uint16_t dur = blinkOn ? s_baseOn : s_baseOff;
            if (now - lastToggle >= dur) {
                lastToggle = now;
                blinkOn = !blinkOn;
            }
            s_leds[0] = blinkOn ? toCrgb(s_baseColor) : CRGB::Black;
        }
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ---------------- setup ----------------
void ledSetup() {
    LOG("LED", "setup");
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(s_leds, 1);
    FastLED.setBrightness(60);
    s_leds[0] = CRGB::Red;
    FastLED.show();

    // 4096 — FastLED.show() needs ~2 KB stack on ESP32
    xTaskCreatePinnedToCore(ledTask, "LedTask", 4096, NULL, 1, NULL, 0);
}