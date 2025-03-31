#include <Arduino.h>
#include "ili9341.h"
#include "debug.h"
#include "game.h"
#include "inputs.h"
#include "util.h"
#include "system.h"

void setup() {
    
    pinMode(LED_PIN, OUTPUT); // Setup Debug LED
    start_blinking();

    system_init();    // Initialize the system, can abort boot
    display_init();   // Initialize the display and backlight
    debug_init();     // Initialize debug structs & check status
    inputs_init();    // Initialize inputs
    debug_delay_ms(); // Delay if debug mode is enabled
    
    stop_blinking();

    start_game();     // Begin the game
}

void loop() {
    // Current frame beginning time
    unsigned long frame_start_time = millis();

    // Run one game cycle
    game_cycle();
    
    // Enforce ~60hz refresh rate
    delay_60_hz(frame_start_time);
}