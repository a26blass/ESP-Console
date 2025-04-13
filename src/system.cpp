#include <Arduino.h>
#include <Preferences.h>
#include "system.h"
#include "esp_system.h"
#include "debug.h"
#include "display.h"

// GLOBALS
// battery
bool critical_batt = false;
float battery_volts = 0.0;

// prefs
Preferences prefs; // used to fetch max score, stored in NVRAM

// LED
int current_brightness_level = 2; // 0 = off, 1 = low, 2 = full
TaskHandle_t led_blink_task_handle = NULL;
int blink_delay_ms = 500;

// LED brightness values
const int BRIGHTNESS_VALUES[] = {0, 20, 64, 255};

// Helper to update LED based on brightness level
void update_led_pwm() {
    ledcWrite(LED_CHANNEL, BRIGHTNESS_VALUES[current_brightness_level]);
}

// Called to change LED brightness
void led_brightness(int level) {
    if (level < 0 || level > 3) return;
    current_brightness_level = level;
    update_led_pwm();
}

// Blinking task
void led_blink_task(void *pvParameters) {
    while (true) {
        ledcWrite(LED_CHANNEL, 0); // Off
        vTaskDelay(blink_delay_ms / portTICK_PERIOD_MS);
        update_led_pwm(); // On
        vTaskDelay(blink_delay_ms / portTICK_PERIOD_MS);
    }
}

// Start blinking
void start_blinking(int delay_ms) {
    blink_delay_ms = delay_ms;
    if (led_blink_task_handle == NULL) {
        xTaskCreatePinnedToCore(
            led_blink_task,
            "LEDBlink",
            1024,
            NULL,
            1,
            &led_blink_task_handle,
            1
        );
    }
}

// Stop blinking
void stop_blinking() {
    if (led_blink_task_handle != NULL) {
        vTaskDelete(led_blink_task_handle);
        led_blink_task_handle = NULL;
        update_led_pwm(); // ensure LED is on
    }
}

/* Task will be spun up on boot
   Asynchronously triggers battery monitor circuit, 
   enters deep sleep if volts < MIN_VOLTS
*/
void battery_monitor_task(void *pvParameters) {
    static bool lowbatt_indicate = false;
    while (true) {
        float batteryVoltage = readBatteryVoltage();
        battery_volts = batteryVoltage;
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
                stop_blinking();
                led_brightness(0);

                esp_deep_sleep_start();
            #else
                ESP_LOGE("SYSTEM", "BATTERY VOLTAGE LOW (%.2f V), DEEP SLEEP OVERRIDE, CONTINUING...", batteryVoltage);
            #endif 

        } else if (batteryVoltage < NOBATT_VOLTS) {
            stop_blinking();
            ESP_LOGE("SYSTEM", "BATTERY VOLTAGE BELOW MINVOLTS (%.2f V), RUNNING ON USB POWER", batteryVoltage); 
        } else if (batteryVoltage < CRITICAL_VOLTS) {
            set_brightness(30); // Screen brightness to MIN functional
            current_brightness_level = 1;

            if(led_blink_task_handle == NULL) {
                start_blinking(blink_delay_ms);
            }
        } else if (batteryVoltage < LOW_VOLTS && !lowbatt_indicate) {
            start_blinking(blink_delay_ms);
            lowbatt_indicate = true;
        } else if (batteryVoltage >= LOW_VOLTS + EPSILON && lowbatt_indicate) {
            stop_blinking();
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

    // LED PWM setup
    ledcSetup(LED_CHANNEL, LED_FREQ, LED_RESOLUTION);
    ledcAttachPin(LED_PIN, LED_CHANNEL);
    led_brightness(2); // full brightness on boot


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