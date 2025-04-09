#include <Arduino.h>
#include "inputs.h"
#include "debug.h"

// Pin definitions (replace with actual pin numbers)
#define INPUT_UP     13
#define INPUT_DOWN   12
#define INPUT_LEFT   14
#define INPUT_RIGHT  27
#define INPUT_A      26
#define INPUT_B      25
#define INPUT_START  32
#define DBG_BUTTON_PIN 33

// State variables for detecting button presses
bool prevStateA = HIGH;
bool prevStateB = HIGH;
bool prevStateStart = HIGH;

void inputs_init() {
    pinMode(DBG_BUTTON_PIN, INPUT_PULLUP);
    pinMode(INPUT_UP, INPUT_PULLUP);    // Set UP button pin to input with pull-up resistor
    pinMode(INPUT_DOWN, INPUT_PULLUP);  // Set DOWN button pin to input with pull-up resistor
    pinMode(INPUT_LEFT, INPUT_PULLUP);  // Set LEFT button pin to input with pull-up resistor
    pinMode(INPUT_RIGHT, INPUT_PULLUP); // Set RIGHT button pin to input with pull-up resistor
    pinMode(INPUT_A, INPUT_PULLUP);     // Set A button pin to input with pull-up resistor
    pinMode(INPUT_B, INPUT_PULLUP);     // Set B button pin to input with pull-up resistor
    pinMode(INPUT_START, INPUT_PULLUP); // Set START button pin to input with pull-up resistor
}

bool debug_input_check() {
    return digitalRead(DBG_BUTTON_PIN) == LOW;
}

// Function to check if UP button is pressed
bool get_up_pressed() {
    return digitalRead(INPUT_UP) == LOW;
}

// Function to check if DOWN button is pressed
bool get_down_pressed() {
    return digitalRead(INPUT_DOWN) == LOW;
}

// Function to check if LEFT button is pressed
bool get_left_pressed() {
    return digitalRead(INPUT_LEFT) == LOW;
}

// Function to check if RIGHT button is pressed
bool get_right_pressed() {
    return digitalRead(INPUT_RIGHT) == LOW;
}

// Function to check if A button is pressed
bool get_a_pressed() {
    bool currentState = digitalRead(INPUT_A) == LOW;
    if (currentState && prevStateA == HIGH) {
        prevStateA = LOW;
        return true;
    }
    if (!currentState) {
        prevStateA = HIGH;
    }
    return false;
}

// Function to check if B button is pressed
bool get_b_pressed() {
    bool currentState = digitalRead(INPUT_B) == LOW;
    if (currentState && prevStateB == HIGH) {
        prevStateB = LOW;
        return true;
    }
    if (!currentState) {
        prevStateB = HIGH;
    }
    return false;
}

// Function to check if START button is pressed
bool get_start_pressed() {
    bool currentState = digitalRead(INPUT_START) == LOW;
    if (currentState && prevStateStart == HIGH) {
        prevStateStart = LOW;
        return true;
    }
    if (!currentState) {
        prevStateStart = HIGH;
    }
    return false;
}

