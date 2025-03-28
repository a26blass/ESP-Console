#include <Arduino.h>
#include "ili9341.h"
#include "debug.h"
#include "game.h"
#include "inputs.h"
#include "util.h"

void setup() {

    start_blinking();
    debug_init();     // Initialize debug structs & check status
    display_init();   // Initialize the display and backlight
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