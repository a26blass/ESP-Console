// system.h

// Uncomment to override low battery shutoff
// #define BATT_VOLT_OVERRIDE

#ifndef SYSTEM_H

#include <Arduino.h>

#define LED_CHANNEL 1
#define LED_RESOLUTION 8 // 0-255
#define LED_FREQ 5000

// Battery monitoring constants
const int ADC_PIN = 34;
const int LED_PIN = 2;
const int BATTERY_CONTROL_PIN = 15;
const float R1 = 5060;
const float R2 = 990;
const float V_REF = 4.4;
const int ADC_MAX = 4095;
const int NUM_SAMPLES = 300;
const float MIN_VOLTS = 3.20;
const float LOW_VOLTS = 3.55;
const float CRITICAL_VOLTS = 3.40;
const float EPSILON = 0.4;
const float NOBATT_VOLTS = 0.1; // any voltage less than this implies that power source is USB-C
const int BATTERY_CHECK_INTERVAL = 10000; // 10 seconds
extern bool critical_batt;
extern float battery_volts;

int get_hiscore();
void set_hiscore(int hiscore);
void battery_monitor_task(void *pvParameters);
float readBatteryVoltage();
void led_brightness(int level);
void system_init();
 
 #endif