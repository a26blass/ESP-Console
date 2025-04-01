// system.h
#ifndef SYSTEM_H

#include <Arduino.h>

// Battery monitoring constants
const int ADC_PIN = 34;
const int BATTERY_CONTROL_PIN = 15;
const float R1 = 5060;
const float R2 = 990;
const float V_REF = 4.4;
const int ADC_MAX = 4095;
const int NUM_SAMPLES = 300;
const float MIN_VOLTS = 3.25;
const int BATTERY_CHECK_INTERVAL = 10000; // 10 seconds
extern bool critical_batt;

void system_init();
void battery_monitor_task(void *pvParameters);
float readBatteryVoltage();

#endif