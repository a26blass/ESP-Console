#include <Arduino.h>
#include "display.h"
#include "debug.h"
#include "game.h"
#include "inputs.h"
#include "util.h"
#include "system.h"

void setup() {
    
    pinMode(LED_PIN, OUTPUT); // Setup Debug LED
    start_blinking();

    system_init();    // Initialize the system, can abort boot
    Serial.println("SYS INIT");
    display_init();   // Initialize the display and backlight
    Serial.println("DISP INIT");
    debug_init();     // Initialize debug structs & check status
    Serial.println("DBG INIT");
    inputs_init();    // Initialize inputs
    Serial.println("INPUT INIT");
    debug_delay_ms(); // Delay if debug mode is enabled
    
    stop_blinking();

    start_game();     // Begin the game
}

void loop() {
    if (!critical_batt) {
        // Current frame beginning time
        unsigned long frame_start_time = millis();
    
        // Run one game cycle
        game_cycle();
        
        // Enforce ~60hz refresh rate
        delay_60_hz(frame_start_time);
    }
}