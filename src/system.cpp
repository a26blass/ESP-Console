#include <Arduino.h>
#include <Preferences.h>
#include "system.h"
#include "esp_system.h"
#include "debug.h"
#include "display.h"

// GLOBALS
bool critical_batt = false;
Preferences prefs; // used to fetch max score, stored in NVRAM

/* Task will be spun up on boot
   Asynchronously triggers battery monitor circuit, 
   enters deep sleep if volts < MIN_VOLTS
*/
void battery_monitor_task(void *pvParameters) {
    while (true) {
        float batteryVoltage = readBatteryVoltage();
        ESP_LOGE("SYSTEM", "Battery Voltage: %.2f V", batteryVoltage);
        
        if (batteryVoltage < MIN_VOLTS && batteryVoltage > NOBATT_VOLTS) {
            #ifndef BATT_VOLT_OVERRIDE
                critical_batt = true;
                ESP_LOGE("SYSTEM", "BATTERY VOLTAGE LOW (%.2f V), ENTERING DEEP SLEEP", batteryVoltage);

                if (get_screen_init()) {
                    draw_lowbatt_symbol();
                    delay(1500);
                }

                gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
                gpio_set_level(GPIO_NUM_5, 0);

                esp_deep_sleep_start();
            #else
                ESP_LOGE("SYSTEM", "BATTERY VOLTAGE LOW (%.2f V), DEEP SLEEP OVERRIDE, CONTINUING...", batteryVoltage);
            #endif 

        } if (batteryVoltage < NOBATT_VOLTS) {
            ESP_LOGE("SYSTEM", "BATTERY VOLTAGE BELOW MINVOLTS (%.2f V), RUNNING ON USB POWER", batteryVoltage); 
        }
        vTaskDelay(BATTERY_CHECK_INTERVAL / portTICK_PERIOD_MS);
    }
}

float readBatteryVoltage() {
    digitalWrite(BATTERY_CONTROL_PIN, LOW);
    delay(50);
    int sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += analogRead(ADC_PIN);
        delay(1);
    }
    int avgReading = sum / NUM_SAMPLES;
    digitalWrite(BATTERY_CONTROL_PIN, HIGH);
    float voltage = (avgReading / (float)ADC_MAX) * V_REF;
    return voltage * ((R1 + R2) / R2);
}

int get_hiscore() {
    int hiscore = prefs.getInt("highscore", 0); // Default to 0
    Serial.println("Fetched Stored High Score: " + String(hiscore));
    return hiscore;
}

void set_hiscore(int hiscore) {
    prefs.putInt("highscore", hiscore);
    Serial.println("New high score saved");
}

void system_init() {
    // Setup pins / serial
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    pinMode(BATTERY_CONTROL_PIN, OUTPUT);
    digitalWrite(BATTERY_CONTROL_PIN, HIGH);

    critical_batt = false;

    char msg[40];

    #ifdef DEBUG
        esp_log_level_set("SYSTEM", ESP_LOG_INFO);
    #endif

    prefs.begin("game", false); // setup prefs, namespace game

    esp_reset_reason_t reset_reason = esp_reset_reason();
    sprintf(msg, "RESET REASON: %s", reset_reason_to_string(reset_reason));
    ESP_LOGI("SYSTEM", msg);

    if (reset_reason == ESP_RST_BROWNOUT) {
        ESP_LOGE("SYSTEM FAIL", "BROWNOUT DETECTED, ENTERING DEEP SLEEP...");
        esp_deep_sleep_start();
    }

    xTaskCreatePinnedToCore(
        battery_monitor_task,
        "BatteryMonitor",
        2048,
        NULL,
        1,
        NULL,
        0
    );

    ESP_LOGI("SYSTEM", "BATTERY MONITOR INIT");

    // Allow battery monitor to run once
    delay(500);
}