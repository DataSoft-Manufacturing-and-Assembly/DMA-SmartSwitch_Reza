#pragma once
#include <Arduino.h>

typedef void (*RfCallback)(unsigned long code, uint8_t bits);

void rfSetup();
void rfTask(void* param);
void rfSetCallback(RfCallback fn);