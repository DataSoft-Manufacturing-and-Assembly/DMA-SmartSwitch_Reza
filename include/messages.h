#pragma once
#include <Arduino.h>
#include "config.h"

// All builders write into a caller-supplied buffer.
// Returns true if it fit.
bool msgHeartbeat   (char* out, size_t outsz);
bool msgAck         (char* out, size_t outsz, const char* source);
bool msgPing        (char* out, size_t outsz);
bool msgOta         (char* out, size_t outsz, const char* status);
bool msgRf          (char* out, size_t outsz, unsigned long code, uint8_t bits);