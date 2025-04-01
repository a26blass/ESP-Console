#include <Arduino.h>
#include "inputs.h"
#include "debug.h"

void inputs_init() {
    pinMode(DBG_BUTTON_PIN, INPUT_PULLUP);
}

bool debug_input_check() {
    return digitalRead(DBG_BUTTON_PIN) == LOW;
}