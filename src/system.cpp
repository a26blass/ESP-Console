#include <Arduino.h>
#include "system.h"
#include "esp_system.h"
#include "debug.h"
#include "ili9341.h"

void system_init() {
    // Setup pins / serial
    Serial.begin(115200);

    char msg[40];

    #ifdef DEBUG
        esp_log_level_set("SYSTEM", ESP_LOG_INFO);
    #endif


    esp_reset_reason_t reset_reason = esp_reset_reason();
    sprintf(msg, "RESET REASON: %s", reset_reason_to_string(reset_reason));
    ESP_LOGI("SYSTEM", msg);

    if (reset_reason == ESP_RST_BROWNOUT) {
        ESP_LOGE("SYSTEM FAIL", "BROWNOUT DETECTED, ENTERING DEEP SLEEP...");
        esp_deep_sleep_start();
    }


    delay(200);
    
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);


    if (digitalRead(5) == HIGH) {
        sprintf(msg, "BACKLIGHT ON (D5)...");
        ESP_LOGI("SYSTEM", msg);
    } else {
        ESP_LOGE("SYSTEM FAIL", "BACKLIGHT FAILED TO ENABLE, ABORTING...");
        esp_system_abort("BACKLIGHT FAILED TO ENABLE, ABORTING...");
    }
}