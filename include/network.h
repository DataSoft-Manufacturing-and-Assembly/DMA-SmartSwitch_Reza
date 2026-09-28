#pragma once
#include <Arduino.h>
#include "config.h"

void networkTask  (void* param);
void wifiResetTask(void* param);
void otaTask      (void* param);

// Any task can call this. The network task publishes it in order.
bool queueMqtt(const char* topic, const char* payload);

// Ask the network task to enter AP mode (used by switches dual-touch)
void requestApMode();

// Called by network task when a command arrives
void mqttCallback(char* topic, byte* payload, unsigned int length);

// Called by main task every HB_INTERVAL_MS
void publishHeartbeat();