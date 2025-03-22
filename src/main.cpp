#include <Arduino.h>
#include "ili9341.h"
#include "debug.h"
#include "game.h"
#include "util.h"

void setup() {
    display_init();
    debug_init();
    start_game();  
}

void loop() {
    unsigned long frame_start_time = millis();

    game_loop();
    
    // Enforce ~60hz refresh rate
    delay_60_hz(frame_start_time);
}