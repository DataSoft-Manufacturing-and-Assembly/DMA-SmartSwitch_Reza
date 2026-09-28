#pragma once
#include <Arduino.h>

enum LedColor : uint8_t {
    L_OFF, L_RED, L_GREEN, L_BLUE, L_YELLOW, L_PINK, L_CYAN, L_WHITE
};

// Persistent state — the LED sits in this when no flash is active.
// `blink=true` toggles the color every onMs/offMs.
void ledBase(LedColor color, bool blink = false,
                uint16_t onMs = 300, uint16_t offMs = 300);

// One-shot flash — temporarily overrides base, then reverts.
void ledFlash(LedColor color, uint16_t durationMs = 200);

// Convenience for common network states
void ledWifiConnecting();    // red blink
void ledWifiResting();       // solid red
void ledMqttConnecting();    // yellow blink
void ledMqttResting();       // solid yellow
void ledOnline();            // off
void ledApMode();            // green blink

void ledSetup();