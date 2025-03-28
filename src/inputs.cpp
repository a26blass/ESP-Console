#include <Arduino.h>
#include "inputs.h"
#include "debug.h"

void inputs_init() {
    pinMode(DBG_BUTTON_PIN, INPUT_PULLUP);

    dbg_write_line("PIN 0 SET TO PULLUP");
}

bool debug_input_check() {
    return digitalRead(DBG_BUTTON_PIN) == LOW;
}