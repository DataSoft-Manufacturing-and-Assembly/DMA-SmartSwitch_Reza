#pragma once
#include <Arduino.h>

void    switchesSetup();
void    switchesTask(void* param);

bool    getSwitchState(uint8_t idx);   // 0=light1  1=light2  2=fan
uint8_t getFanSpeed();                 // 0..5

void    setSwitchState(uint8_t idx, bool on, bool publish, const char* source);
void    setAllSwitches (bool on, bool publish, const char* source);
void    setFanSpeed    (uint8_t speed, bool publish, const char* source);