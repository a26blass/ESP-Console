#include <Arduino.h>
#include "debug.h"
#include "game.h"
#include "ili9341.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_log.h"


// Debug struct
debug_t debug = {
    .setup_complete = false,
    .screen_init = false,
};

// Async task handler
TaskHandle_t led_task_handle = NULL; // Handle for LED task

// LED tasks
void blink_LED(void *parameter) {
    while (1) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // Toggle LED
        vTaskDelay(pdMS_TO_TICKS(LED_FLASH_INTERVAL));  // delay 
    }
}

void start_blinking() {
    if (led_task_handle == NULL) {
        xTaskCreate(blink_LED, "LED Task", 1024, NULL, 1, &led_task_handle);
    }
}

void stop_blinking() {
    if (led_task_handle != NULL) {
        vTaskDelete(led_task_handle);  // Kill the task
        led_task_handle = NULL;
        digitalWrite(LED_PIN, LOW);  // Ensure LED is off
    }
}


// Panic Handler
extern "C" void __wrap_esp_panic_handler(void *frame) {
    esp_task_wdt_deinit();
    for (int i = 0; i < 32; i++)
        esp_intr_disable_source(i);
    

    // Cast frame to a CPU context structure
    XtExcFrame *exc_frame = (XtExcFrame *)frame;

    // Print a warning message
    if (debug.screen_init)
        display_panic_message(exc_frame);

    // Print a panic message
    ESP_LOGE("PANIC", "KERNEL PANIC, ABORTING...");

    // Extract and print CPU register values
    ESP_LOGE("PANIC", "PC        (Program Counter)  : 0x%08X", exc_frame->pc);
    ESP_LOGE("PANIC", "LR        (Link Register)    : 0x%08X", exc_frame->a0);
    ESP_LOGE("PANIC", "SP        (Stack Pointer)    : 0x%08X", exc_frame->a1);

    // Print all general-purpose registers (A2–A15)
    for (int i = 2; i <= 15; i++) {
        ESP_LOGE("PANIC", "A%-2d       : 0x%08X", i, *((uint32_t *)exc_frame + i));
    }

    ESP_LOGE("PANIC", "EXCCAUSE  (Exception Cause)  : %d", exc_frame->exccause);
    ESP_LOGE("PANIC", "EXCVADDR  (Faulting Address) : 0x%08X", exc_frame->excvaddr);

    // Read special-purpose registers (Processor State Register and Shift Amount Register)
    uint32_t ps, sar;
    asm volatile("rsr.ps %0" : "=r"(ps));   // Read Processor State Register
    asm volatile("rsr.sar %0" : "=r"(sar)); // Read Shift Amount Register

    ESP_LOGE("PANIC", "PS        (Processor State)  : 0x%08X", ps);
    ESP_LOGE("PANIC", "SAR       (Shift Amount)     : 0x%08X", sar);
        
    
    // Wait for logs to be flushed before restarting

    #ifndef DEBUG
        esp_restart();
    #endif

    
}

const char* chip_model_to_string(esp_chip_model_t chip_model) {
    switch(chip_model) {
        case CHIP_ESP32:
            return "CPU_ESP32";
        case CHIP_ESP32S2:
            return "CPU_ESP32-S2";
        case CHIP_ESP32S3:
            return "CPU_ESP32-S3";
        case CHIP_ESP32C3:
            return "CPU_ESP32-C3";
        case CHIP_ESP32H2:
            return "CPU_ESP32-H2";
        default:
            return "Unknown Chip Model";
    }
}

const char* reset_reason_to_string(esp_reset_reason_t reset_reason) {
    switch (reset_reason) {
        case ESP_RST_UNKNOWN:
            return "UNKNOWN RESET REASON";
        case ESP_RST_POWERON:
            return "POWER-ON RESET";
        case ESP_RST_EXT:
            return "EXTERNAL PIN RESET";
        case ESP_RST_SW:
            return "SOFTWARE RESET";
        case ESP_RST_PANIC:
            return "PANIC RESET";
        case ESP_RST_INT_WDT:
            return "INTERRUPT WATCHDOG RESET";
        case ESP_RST_TASK_WDT:
            return "TASK WATCHDOG RESET";
        case ESP_RST_WDT:
            return "OTHER WATCHDOG RESET";
        case ESP_RST_DEEPSLEEP:
            return "DEEP SLEEP EXIT RESET";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT RESET";
        case ESP_RST_SDIO:
            return "SDIO RESET";
        default:
            return "UNKNOWN RESET REASON";
    }
}

void query_display_status() {
    uint8_t status = get_display_status();
    Serial.print("DISPLAY CHECK... STATUS = 0x");
    Serial.println(status, HEX);

    if (!DISPLAY_OK) {
        Serial.println("DISPLAY CHECK FAIL, ABORTING...");
        esp_system_abort("DISPLAY CHECK FAIL");
    }
}

void dbg_write_line(const char *line) {
    Serial.println(line);
    #ifdef DEBUG
        drawdebugtext(line);
    #endif
}

void debug_delay_ms(int ms) {
    #ifdef DEBUG
        delay(ms);
    #endif
}

void debug_init() {
    // Setup pins / serial
    Serial.begin(9600);

    #ifdef DEBUG
        esp_log_level_set("BOOT", ESP_LOG_INFO);
    #endif

    char msg[40];


    
    if (digitalRead(5) == HIGH) {
        sprintf(msg, "BACKLIGHT ON (D5)...");
        ESP_LOGI("BOOT", msg);
        #ifdef DEBUG
        drawdebugtext(msg);
        #endif
    } else {
        ESP_LOGE("BOOT FAIL", "BACKLIGHT FAILED TO ENABLE, ABORTING...");
        esp_system_abort("BACKLIGHT FAILED TO ENABLE, ABORTING...");
    }
    
    debug.screen_init = true;
    
    esp_chip_info_t c_info;
    esp_chip_info(&c_info);
    sprintf(msg, "CPU MODEL %s (%d CORES)...", chip_model_to_string(c_info.model), c_info.cores);
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    bool ble = (c_info.features & CHIP_FEATURE_BLE) != 0;
    bool blu = (c_info.features & CHIP_FEATURE_BT) != 0;
    bool w_support = (c_info.features & CHIP_FEATURE_IEEE802154) != 0;
    bool wifi_bgn = (c_info.features & CHIP_FEATURE_WIFI_BGN) != 0;
    bool flash = (c_info.features & CHIP_FEATURE_EMB_FLASH) != 0;
    bool psram = (c_info.features & CHIP_FEATURE_EMB_PSRAM) != 0;
    sprintf(msg, "BLU: %d | BLE : %d | IEEE802154 : %d", blu, ble, w_support);
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext("FEATURE FLAGS:");
        drawdebugtext(msg);
    #endif
    sprintf(msg, "W_BGN: %d | FLASH : %d | PSRAM : %d", wifi_bgn, flash, psram);
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    esp_reset_reason_t reset_reason = esp_reset_reason();
    sprintf(msg, "RESET REASON: %s", reset_reason_to_string(reset_reason));
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    if (reset_reason == ESP_RST_BROWNOUT) {
        ESP_LOGE("BOOT FAIL", "BROWNOUT DETECTED, ENTERING DEEP SLEEP...");
        esp_deep_sleep_start();
    }

    if (!Serial) {
        ESP_LOGE("BOOT FAIL", "SERIAL1 NOT INITIALIZED...");
        #ifdef DEBUG
            drawdebugtext("SERIAL1 NOT INITIALIZED...");
        #endif
        esp_system_abort("SERIAL INIT FAIL");
    }

    #ifdef DEBUG
        drawdebugtext("SERIAL1 PORT INITIALIZED...");
    #endif

    sprintf(msg, "FREE HEAP MEMORY: %dB (%dKB)", esp_get_free_heap_size(), esp_get_free_heap_size()/1000);
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
    sprintf(msg, "FREE STACK MEMORY: %dB (%dKB)", stack_remaining, stack_remaining/1000);
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    sprintf(msg, "ESP IDF %s", esp_get_idf_version());
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif  

    drawloadtext();
    
    #ifdef DEBUG
        drawdebugtext("LEVEL LOAD OK...");
    #endif

    #ifdef DEBUG
        drawdebugtext("STARTING LEVEL...");
    #endif

    debug.setup_complete = true;  

    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for logs to be printed
}