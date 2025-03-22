#include <Arduino.h>
#include "util.h"
#include "esp_system.h"

int getRandomInt(int min, int max) {
    return min + (esp_random() % (max - min + 1));
}

float getRandomFloat(float min, float max) {
    return min + (max - min) * (esp_random() / (float)UINT32_MAX);
}

void delay_60_hz(float cur_f_time) {
    // Enforce ~60hz
    unsigned long f_complete_time = millis() - cur_f_time;
    if (f_complete_time < 16)
        delay(16 - f_complete_time);
}